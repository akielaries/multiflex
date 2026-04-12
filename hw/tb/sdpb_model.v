`timescale 1ns/1ns
// sdpb_model.v -- behavioral model of Gowin SDPB primitive for simulation
//
// models the subset used by cheby_dpssram:
//   READ_MODE=0 (combinatorial array) + OCE=1 (output register on CLKB)
//   -> 1 cycle read latency: issue read (CEB=1) on cycle N, DO valid on cycle N+1
//   BIT_WIDTH_0=8, BIT_WIDTH_1=8 -> 2048 entries x 8 bits
//   ADA/ADB are {addr[10:0], 3'b000} (14-bit bus, lower 3 bits ignored)

module SDPB #(
  parameter READ_MODE  = 1'b0,
  parameter BIT_WIDTH_0 = 8,
  parameter BIT_WIDTH_1 = 8,
  parameter BLK_SEL_0  = 3'b000,
  parameter BLK_SEL_1  = 3'b000,
  parameter RESET_MODE = "SYNC"
) (
  input  wire [31:0] DI,
  output wire [31:0] DO,
  input  wire [13:0] ADA,
  input  wire [13:0] ADB,
  input  wire        CLKA,
  input  wire        CLKB,
  input  wire        CEA,
  input  wire        CEB,
  input  wire        OCE,
  input  wire        RESET,
  input  wire [2:0]  BLKSELA,
  input  wire [2:0]  BLKSELB
);

  reg [7:0] mem [0:2047];
  reg [7:0] out_reg;

  integer init_i;
  initial begin
    out_reg = 8'h00;
    for (init_i = 0; init_i < 2048; init_i = init_i + 1)
      mem[init_i] = 8'h00;
  end

  // write port: CLKA, CEA
  always @(posedge CLKA) begin
    if (CEA) begin
      mem[ADA[13:3]] <= DI[7:0];
    end
  end

  // read port: CLKB, CEB, with OCE output register (1 cycle latency)
  always @(posedge CLKB) begin
    if (CEB) begin
      out_reg <= mem[ADB[13:3]];
    end
  end

  assign DO = {24'b0, out_reg};

endmodule
