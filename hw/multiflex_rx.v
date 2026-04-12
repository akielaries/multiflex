`timescale 1ns/1ns
// multiflex_rx.v -- multiflex protocol RX engine
//
// samples mfx_rx_in on rising edges of mfx_clk_in when mfx_sync_in=1
// bit packing mirrors multiflex_tx: highest-numbered active lane = MSB
// one-cycle latency between last symbol and rx_valid/rx_byte output

module multiflex_rx #(
  parameter NUM_LANES = 3
) (
  input  wire                  clk,
  input  wire                  rstn,

  input  wire                  cfg_enable,
  input  wire [4:0]            cfg_lanes,
  input  wire                  clr_rx,

  // wire inputs (from loopback mux or external pins)
  input  wire                  mfx_clk_in,
  input  wire [NUM_LANES-1:0]  mfx_rx_in,
  input  wire                  mfx_sync_in,

  // received byte (valid for one cycle, one cycle after last symbol)
  output reg  [7:0]            rx_byte,
  output reg                   rx_valid,

  // status
  output reg                   rx_locked,    // sync seen at least once
  output reg                   rx_sync_lost  // sync deasserted mid-byte; sticky, cleared by clr_rx
);

  // -------------------------------------------------------------------------
  // active lane count (clamped to [1, NUM_LANES])
  // -------------------------------------------------------------------------
  wire [4:0] lanes = (cfg_lanes == 0)       ? 5'd1 :
                     (cfg_lanes > NUM_LANES) ? NUM_LANES[4:0] :
                     cfg_lanes;

  // -------------------------------------------------------------------------
  // rising edge detect on mfx_clk_in
  // mfx_clk_in is a fabric register in the same clock domain (loopback),
  // so edge detection with a one-cycle delay register is clean and cycle-exact
  // -------------------------------------------------------------------------
  reg mfx_clk_prev;
  always @(posedge clk) begin
    if (!rstn) mfx_clk_prev <= 1'b0;
    else       mfx_clk_prev <= mfx_clk_in;
  end
  wire rise_tick = mfx_clk_in && !mfx_clk_prev;

  // -------------------------------------------------------------------------
  // RX state
  // rx_sr accumulates bits as they arrive; rx_collected counts bits so far
  // rx_done fires the cycle rx_sr is complete (output appears the next cycle)
  // -------------------------------------------------------------------------
  reg [7:0] rx_sr;
  reg [3:0] rx_collected; // bits collected into rx_sr for current byte (0..7)
  reg       rx_done;      // rx_sr holds a complete byte; output it next cycle

  integer k;
  integer bit_pos;

  always @(posedge clk) begin
    rx_valid <= 1'b0;

    if (!rstn || !cfg_enable) begin
      rx_sr        <= 8'd0;
      rx_collected <= 4'd0;
      rx_done      <= 1'b0;
      rx_locked    <= 1'b0;
      rx_sync_lost <= 1'b0;
    end else begin
      if (clr_rx) rx_sync_lost <= 1'b0;

      // one cycle after rx_done: rx_sr has settled, output the byte
      if (rx_done) begin
        rx_byte  <= rx_sr;
        rx_valid <= 1'b1;
      end
      rx_done <= 1'b0;

      if (rise_tick) begin
        if (!mfx_sync_in) begin
          // sync deasserted -- flag error if we were mid-byte
          if (rx_locked && rx_collected != 4'd0) begin
            rx_sync_lost <= 1'b1;
          end
          rx_locked    <= 1'b0;
          rx_collected <= 4'd0;
        end else begin
          rx_locked <= 1'b1;

          // collect bits into rx_sr
          // lane k carries byte bit at position: 7 - rx_collected - (lanes-1-k)
          // valid when (lanes-1-k) < (8 - rx_collected)
          for (k = 0; k < NUM_LANES; k = k + 1) begin
            if ((k < lanes) && ((lanes - 1 - k) < (8 - rx_collected))) begin
              bit_pos = 7 - rx_collected - (lanes - 1 - k);
              rx_sr[bit_pos] <= mfx_rx_in[k];
            end
          end

          if (rx_collected + lanes >= 8) begin
            // byte complete after this symbol
            rx_done      <= 1'b1;
            rx_collected <= 4'd0;
          end else begin
            rx_collected <= rx_collected + lanes[3:0];
          end
        end
      end
    end
  end

endmodule
