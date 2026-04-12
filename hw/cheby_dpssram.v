`timescale 1ns/1ns
// cheby_dpssram.v -- dual-port synchronous SRAM for cheby-generated register maps
//
// port A: APB side (read/write by firmware via cheby address decoder)
// port B: hardware side (read by TX engine, written by RX engine)
//
// uses Gowin GW5A SDPB primitive directly; avoids DPB inference which
// always produces WRITE_MODE=2'b10 (read-first), unsupported on GW5AST.
// SDPB has separate read/write ports with no write-mode restriction.
//
// write-source mux: either port A or port B may be the writer.
// for the two use cases cheby instantiates, only one direction is active
// per instance (tx_buf: A writes, B reads; rx_buf: B writes, A reads),
// so synthesis eliminates the mux entirely.
//
// constraints: g_data_width=8, g_size=2048, g_addr_width=11, g_dual_clock=0.
// other combinations are not supported by this Gowin-specific implementation.

module cheby_dpssram #(
  parameter g_data_width = 8,
  parameter g_size       = 2048,
  parameter g_addr_width = 11,
  parameter g_dual_clock = 1'b0,
  parameter g_use_bwsel  = 1'b1
) (
  input  wire                          clk_a_i,
  input  wire                          clk_b_i,

  // port A: APB side
  input  wire [g_addr_width-1:0]       addr_a_i,
  input  wire [(g_data_width+7)/8-1:0] bwsel_a_i,
  input  wire [g_data_width-1:0]       data_a_i,
  output wire [g_data_width-1:0]       data_a_o,
  input  wire                          rd_a_i,
  input  wire                          wr_a_i,

  // port B: hardware side
  input  wire [g_addr_width-1:0]       addr_b_i,
  input  wire [(g_data_width+7)/8-1:0] bwsel_b_i,
  input  wire [g_data_width-1:0]       data_b_i,
  output wire [g_data_width-1:0]       data_b_o,
  input  wire                          rd_b_i,
  input  wire                          wr_b_i
);

  // write-source mux: port A or port B may be the writer
  wire [g_addr_width-1:0] wr_addr = wr_a_i ? addr_a_i : addr_b_i;
  wire [g_data_width-1:0] wr_data = wr_a_i ? data_a_i : data_b_i;
  wire                    we      = wr_a_i | wr_b_i;

  // read-source mux: port B read takes priority over port A
  wire [g_addr_width-1:0] rd_addr = rd_b_i ? addr_b_i : addr_a_i;
  wire                    re      = rd_a_i | rd_b_i;

  // SDPB: 2048x8, write on port A (ADA/CLKA/CEA/DI), read on port B (ADB/CLKB/CEB/DO)
  // READ_MODE=0: normal (registered) output; OCE=1 keeps output register enabled.
  //   read latency = 1 clock cycle.  callers must account for this (drain SM uses
  //   DRAIN_READ state to absorb it; cheby APB read path uses tx_buf_data_rack).
  // BIT_WIDTH_0=8, BIT_WIDTH_1=8: 8-bit data width on both ports
  // ADA/ADB format for BIT_WIDTH=8: {addr[10:0], 3'b000} = 14 bits
  // CLKA (write clock) = clk_b_i; CLKB (read clock) = clk_a_i
  //   both tx_buf and rx_buf use pclk for all ports: received bytes cross from
  //   the fast clk domain via toggle-pulse CDC before reaching the BRAM write port
  wire [31:0] sdpb_do;

  SDPB #(
    .READ_MODE  (1'b0),
    .BIT_WIDTH_0(8),
    .BIT_WIDTH_1(8),
    .BLK_SEL_0  (3'b000),
    .BLK_SEL_1  (3'b000),
    .RESET_MODE ("SYNC")
  ) u_sdpb (
    .DI     ({24'b0, wr_data[7:0]}),
    .DO     (sdpb_do),
    .ADA    ({wr_addr[10:0], 3'b000}),
    .ADB    ({rd_addr[10:0], 3'b000}),
    .CLKA   (clk_b_i),  // write clock: caller's hw-side clock (pclk for tx_buf, fast clk for rx_buf)
    .CLKB   (clk_a_i),  // read clock: caller's apb-side clock
    .CEA    (we),
    .CEB    (re),
    .OCE    (1'b1),
    .RESET  (1'b0),
    .BLKSELA(3'b000),
    .BLKSELB(3'b000)
  );

  // both outputs driven from the same SDPB read port; the caller uses only
  // the output corresponding to the port that issued the read request
  assign data_a_o = sdpb_do[7:0];
  assign data_b_o = sdpb_do[7:0];

endmodule
