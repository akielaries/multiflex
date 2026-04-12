`timescale 1ns/1ns
// multiflex_tx.v -- multiflex protocol TX engine
// no APB dependency; parameterized by NUM_LANES
//
// wire protocol:
//   data driven on falling edge of mfx_clk, sampled on rising edge
//   mfx_sync=1 marks a valid symbol clock
//   bits packed MSB-first; highest-numbered active lane carries the MSB
//   last partial symbol: upper lanes carry remaining bits, lower lanes = 0

module multiflex_tx #(
  parameter NUM_LANES = 3
) (
  input  wire                  clk,
  input  wire                  rstn,

  // runtime config (from register block)
  input  wire                  cfg_enable,
  input  wire [4:0]            cfg_lanes,   // 1..NUM_LANES active lanes
  input  wire [7:0]            cfg_clk_div, // wire clk half-period in fabric cycles minus 1

  // TX push interface
  input  wire [7:0]            tx_byte,
  input  wire                  tx_wr,
  output wire                  tx_full,
  output wire                  tx_empty,

  // status
  output wire                  tx_busy,

  // wire outputs (pipeline-registered: driven through a one-cycle output stage
  // so that all pad-facing flip-flops are the last register before the pad,
  // eliminating long combinational paths on the TX output)
  output reg                   mfx_clk,
  output reg  [NUM_LANES-1:0]  mfx_tx,
  output reg                   mfx_sync,

  // fabric-side copies: driven by the pre-pipeline (_r) registers, which are
  // pure fabric FFs.  NOT connected to output pads so synthesis will not
  // promote them to the global clock network.  use these for RX loopback and
  // edge-detect feedback.
  //
  // syn_preserve prevents synthesis from merging these with the pad-facing
  // output FFs (mfx_clk/mfx_sync), which would force the RX module to read
  // from a FF placed near the pad, causing long routing across the chip.
  (* syn_preserve = "true" *) output reg                   mfx_clk_fabric,
  (* syn_preserve = "true" *) output reg  [NUM_LANES-1:0]  mfx_tx_fabric,
  (* syn_preserve = "true" *) output reg                   mfx_sync_fabric
);

  // -------------------------------------------------------------------------
  // FIFO -- 16-entry single-clock
  // -------------------------------------------------------------------------
  localparam FDEPTH = 16;
  localparam FBITS  = 4;

  reg [7:0]     fifo [0:FDEPTH-1];
  reg [FBITS:0] wr_ptr;
  reg [FBITS:0] rd_ptr;

  wire fifo_full_w  = (wr_ptr[FBITS] != rd_ptr[FBITS]) &&
                      (wr_ptr[FBITS-1:0] == rd_ptr[FBITS-1:0]);
  wire fifo_empty_w = (wr_ptr == rd_ptr);

  assign tx_full  = fifo_full_w;
  assign tx_empty = fifo_empty_w;

  always @(posedge clk) begin
    if (!rstn || !cfg_enable) begin
      wr_ptr <= 0;
    end else if (tx_wr && !fifo_full_w) begin
      fifo[wr_ptr[FBITS-1:0]] <= tx_byte;
      wr_ptr <= wr_ptr + 1;
    end
  end

  // -------------------------------------------------------------------------
  // wire clock divider
  // -------------------------------------------------------------------------
  // mfx_clk is generated with a configurable half-period.
  // data is updated on the fabric posedge that ends the mfx_clk HIGH half
  // (i.e., mfx_clk is about to go LOW), giving the receiver a full low half
  // plus setup time before the next rising sample edge.
  //
  // clock only runs when the FIFO has data or the state machine is active;
  // holds low otherwise so the clock is absent between transmissions

  reg [7:0] div_cnt;
  reg       phase; // 0 = high half in progress, 1 = low half in progress

  // forward declarations needed before wire assignments that reference them
  reg       active;
  reg       drain;

  wire tx_busy_w = active || !fifo_empty_w;

  // drain: set on the last data fall_tick (last bit of last byte, fifo empty)
  // keeps the clock alive through one more full cycle so the receiver can
  // sample the last rising edge with sync=1, then see a clean sync=0 on the
  // trailing falling edge before the clock gates off
  wire clk_run = tx_busy_w || drain;

  wire fall_tick = (div_cnt == 0) && (phase == 1'b0) && cfg_enable && clk_run;

  // pre-pipeline internal registers: state machine and divider write these;
  // also driven out as fabric-side copies so the placer can put them near
  // the RX module rather than near the output pads
  reg                  mfx_clk_r;
  reg [NUM_LANES-1:0]  mfx_tx_r;
  reg                  mfx_sync_r;
  reg                  mfx_clk_fabric_r;
  reg                  mfx_sync_fabric_r;

  // fabric outputs: driven directly from _r registers so the P&R tool can
  // place them near the RX module.  they are one cycle ahead of the pad-facing
  // outputs; since RX clock/data/sync all shift by the same one cycle the
  // relative phase is preserved and the protocol is unaffected.
  always @(posedge clk) begin
    if (!rstn || !cfg_enable) begin
      mfx_clk_fabric  <= 1'b0;
      mfx_tx_fabric   <= {NUM_LANES{1'b0}};
      mfx_sync_fabric <= 1'b0;
    end else begin
      mfx_clk_fabric  <= mfx_clk_fabric_r;
      mfx_tx_fabric   <= mfx_tx_r;
      mfx_sync_fabric <= mfx_sync_fabric_r;
    end
  end

  // output pipeline register: pad-facing FFs, no reset.
  // neither rstn nor cfg_enable are needed here: the pre-pipeline _r registers
  // are reset by both and will drive 0s into this stage, which propagates to
  // the pads one cycle later.  removing the reset eliminates the long routing
  // path from the APB/reset-sync region to these pad-adjacent FFs.
  always @(posedge clk) begin
    mfx_clk  <= mfx_clk_r;
    mfx_tx   <= mfx_tx_r;
    mfx_sync <= mfx_sync_r;
  end

  always @(posedge clk) begin
    if (!rstn || !cfg_enable || !clk_run) begin
      div_cnt          <= 0;
      phase            <= 0;
      mfx_clk_r        <= 0;
      mfx_clk_fabric_r <= 0;
    end else begin
      if (div_cnt == 0) begin
        div_cnt          <= cfg_clk_div;
        phase            <= ~phase;
        mfx_clk_r        <= phase;
        mfx_clk_fabric_r <= phase;
      end else begin
        div_cnt <= div_cnt - 1;
      end
    end
  end

  // -------------------------------------------------------------------------
  // active lane count (clamped to [1, NUM_LANES])
  // -------------------------------------------------------------------------
  wire [4:0] lanes = (cfg_lanes == 0)       ? 5'd1 :
                     (cfg_lanes > NUM_LANES) ? NUM_LANES[4:0] :
                     cfg_lanes;

  // -------------------------------------------------------------------------
  // TX state machine
  // -------------------------------------------------------------------------
  // States:
  //   IDLE  (!active): wait for a byte in the FIFO
  //   LOAD  (one fall_tick): pop FIFO head into sr, transition to SEND
  //   SEND  (active): on each fall_tick drive one symbol from sr then shift
  //
  // sr holds the current byte, MSB-aligned.
  // rem counts bits remaining (8 down to 1..lanes).
  // Each fall_tick in SEND: drive top `lanes` bits (with zero-pad when
  // rem < lanes), shift sr left by lanes, decrement rem.
  // When rem reaches 0 after the shift, check FIFO for next byte.

  reg [7:0] sr;
  reg [3:0] rem;   // 0 = idle/load, 1..8 = bits left in current byte

  assign tx_busy = active || !fifo_empty_w || drain;

  wire [7:0] fifo_head = fifo[rd_ptr[FBITS-1:0]];

  integer k;

  always @(posedge clk) begin
    if (!rstn || !cfg_enable) begin
      rd_ptr           <= 0;
      sr               <= 0;
      rem              <= 0;
      active           <= 0;
      drain            <= 0;
      mfx_tx_r         <= 0;
      mfx_sync_r       <= 0;
      mfx_sync_fabric_r <= 0;
    end else if (fall_tick) begin
      if (!active) begin
        // drain fall_tick or idle fall_tick: always clear sync/tx
        // if fifo has data, also load it (back-to-back after a drain burst)
        mfx_tx_r          <= 0;
        mfx_sync_r        <= 0;
        mfx_sync_fabric_r <= 0;
        drain             <= 0;
        if (!fifo_empty_w) begin
          sr     <= fifo_head;
          rem    <= 4'd8;
          active <= 1;
          rd_ptr <= rd_ptr + 1;
        end
      end else begin
        // SEND: drive current symbol from sr
        mfx_sync_r        <= 1;
        mfx_sync_fabric_r <= 1;
        for (k = 0; k < NUM_LANES; k = k + 1) begin
          // lane k gets sr[7-(lanes-1-k)] when that bit position is valid
          // (lanes-1-k) is the offset from the top: 0 for the MSB lane
          if ((k < lanes) && ((lanes - 1 - k) < rem)) begin
            mfx_tx_r[k] <= sr[7 - (lanes - 1 - k)];
          end else begin
            mfx_tx_r[k] <= 1'b0;
          end
        end

        if (rem <= lanes) begin
          // last symbol of this byte
          if (!fifo_empty_w) begin
            // back-to-back: load next byte; first symbol on next fall_tick
            sr     <= fifo_head;
            rem    <= 4'd8;
            rd_ptr <= rd_ptr + 1;
            // active stays 1, mfx_sync_r stays 1 for this symbol
            // next fall_tick will drive the first symbol of the new byte
          end else begin
            // no more data; arm drain so the clock runs one more cycle
            // -- the receiver samples this symbol's rising edge (sync=1),
            //    then sees a clean sync=0 falling edge before clock stops
            active <= 0;
            rem    <= 0;
            drain  <= 1;
          end
        end else begin
          sr  <= sr << lanes;
          rem <= rem - lanes[3:0];
        end
      end
    end else begin
      // no fall_tick: clear outputs when truly idle (not during drain)
      // drain holds sync_r=1 so the receiver can still sample the last rising edge
      if (!active && fifo_empty_w && !drain) begin
        mfx_tx_r          <= 0;
        mfx_sync_r        <= 0;
        mfx_sync_fabric_r <= 0;
      end
    end
  end

endmodule
