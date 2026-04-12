`timescale 1ns/1ns
// tb_multiflex.v -- wrapper for multiflex full-path simulation
//
// exposes the APB interface directly to cocotb.
// mfx_rx is tied to mfx_tx for physical loopback; software loopback (ctrl.loopback=1)
// uses the internal cfg_loopback_c mux which selects mfx_tx in the fabric path.

module tb_multiflex #(
  parameter NUM_LANES = 3
) (
  input  wire        pclk,
  input  wire        prstn,
  input  wire        clk,

  // APB
  input  wire [31:0] paddr,
  input  wire        psel,
  input  wire        penable,
  input  wire        pwrite,
  input  wire [31:0] pwdata,
  output wire [31:0] prdata,
  output wire        pready,
  output wire        pslverr
);

  // PAD_DELAY_NS: models output-pad + wire + input-pad round-trip delay.
  // 0 = software-loopback-equivalent (no skew).
  // set to 1 or 2 clk cycles worth of ns to stress physical-loopback timing.
  parameter PAD_DELAY_NS = 0;

  wire [NUM_LANES-1:0] mfx_tx;
  wire                 mfx_clk_pad;
  wire                 mfx_sync_pad;

  // model pad round-trip delay on the physical loopback path
  wire [NUM_LANES-1:0] mfx_rx_delayed;
  generate
    if (PAD_DELAY_NS == 0) begin
      assign mfx_rx_delayed = mfx_tx;
    end else begin
      assign #(PAD_DELAY_NS) mfx_rx_delayed = mfx_tx;
    end
  endgenerate

  multiflex #(.NUM_LANES(NUM_LANES)) dut (
    .pclk    (pclk),
    .prstn   (prstn),
    .clk     (clk),
    .paddr   (paddr),
    .psel    (psel),
    .penable (penable),
    .pwrite  (pwrite),
    .pwdata  (pwdata),
    .prdata  (prdata),
    .pready  (pready),
    .pslverr (pslverr),
    .mfx_clk (mfx_clk_pad),
    .mfx_tx  (mfx_tx),
    .mfx_sync(mfx_sync_pad),
    .mfx_rx  (mfx_rx_delayed)   // physical loopback wiring (with pad delay model)
  );

endmodule
