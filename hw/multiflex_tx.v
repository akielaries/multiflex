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

  // wire outputs
  output reg                   mfx_clk,
  output reg  [NUM_LANES-1:0]  mfx_tx,
  output reg                   mfx_sync
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

  wire tx_busy_w = active || !fifo_empty_w;

  // drain: set on the last data fall_tick (last bit of last byte, fifo empty)
  // keeps the clock alive through one more full cycle so the receiver can
  // sample the last rising edge with sync=1, then see a clean sync=0 on the
  // trailing falling edge before the clock gates off
  reg drain;
  wire clk_run = tx_busy_w || drain;

  wire fall_tick = (div_cnt == 0) && (phase == 1'b0) && cfg_enable && clk_run;

  always @(posedge clk) begin
    if (!rstn || !cfg_enable || !clk_run) begin
      div_cnt <= 0;
      phase   <= 0;
      mfx_clk <= 0;
    end else begin
      if (div_cnt == 0) begin
        div_cnt <= cfg_clk_div;
        phase   <= ~phase;
        mfx_clk <= phase; // end of low half (phase was 1) -> clk goes high
                          // end of high half (phase was 0) -> clk goes low
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
  reg       active;

  assign tx_busy = active || !fifo_empty_w || drain;

  wire [7:0] fifo_head = fifo[rd_ptr[FBITS-1:0]];

  integer k;

  always @(posedge clk) begin
    if (!rstn || !cfg_enable) begin
      rd_ptr   <= 0;
      sr       <= 0;
      rem      <= 0;
      active   <= 0;
      drain    <= 0;
      mfx_tx   <= 0;
      mfx_sync <= 0;
    end else if (fall_tick) begin
      if (!active) begin
        // drain fall_tick or idle fall_tick: always clear sync/tx
        // if fifo has data, also load it (back-to-back after a drain burst)
        mfx_tx   <= 0;
        mfx_sync <= 0;
        drain    <= 0;
        if (!fifo_empty_w) begin
          sr     <= fifo_head;
          rem    <= 4'd8;
          active <= 1;
          rd_ptr <= rd_ptr + 1;
        end
      end else begin
        // SEND: drive current symbol from sr
        mfx_sync <= 1;
        for (k = 0; k < NUM_LANES; k = k + 1) begin
          // lane k gets sr[7-(lanes-1-k)] when that bit position is valid
          // (lanes-1-k) is the offset from the top: 0 for the MSB lane
          if ((k < lanes) && ((lanes - 1 - k) < rem)) begin
            mfx_tx[k] <= sr[7 - (lanes - 1 - k)];
          end else begin
            mfx_tx[k] <= 1'b0;
          end
        end

        if (rem <= lanes) begin
          // last symbol of this byte
          if (!fifo_empty_w) begin
            // back-to-back: load next byte; first symbol on next fall_tick
            sr     <= fifo_head;
            rem    <= 4'd8;
            rd_ptr <= rd_ptr + 1;
            // active stays 1, mfx_sync stays 1 for this symbol
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
      // drain holds sync=1 so the receiver can still sample the last rising edge
      if (!active && fifo_empty_w && !drain) begin
        mfx_tx   <= 0;
        mfx_sync <= 0;
      end
    end
  end

endmodule
