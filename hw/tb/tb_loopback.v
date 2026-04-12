`timescale 1ns/1ns
// tb_loopback.v -- simulation wrapper wiring multiflex_tx directly to multiflex_rx
//
// mfx_clk_fabric and mfx_sync_fabric (fabric-only FF copies from tx) feed the
// rx engine, matching the real hardware path in multiflex.v.
// mfx_tx[0] feeds mfx_rx_in[0] (loopback).

module multiflex_loopback #(
  parameter NUM_LANES = 3
) (
  input  wire        clk,
  input  wire        rstn,

  // shared config
  input  wire        cfg_enable,
  input  wire [4:0]  cfg_lanes,
  input  wire [7:0]  cfg_clk_div,

  // tx write port
  input  wire [7:0]  tx_byte,
  input  wire        tx_wr,
  output wire        tx_full,
  output wire        tx_empty,
  output wire        tx_busy,

  // rx output
  output wire [7:0]  rx_byte,
  output wire        rx_valid,
  output wire        rx_locked,
  output wire        rx_sync_lost
);

  wire [NUM_LANES-1:0] mfx_tx;
  wire                 mfx_clk_pad;    // pad-bound (not used for rx)
  wire                 mfx_sync_pad;   // pad-bound (not used for rx)
  wire                 mfx_clk_fabric;
  wire                 mfx_sync_fabric;

  multiflex_tx #(.NUM_LANES(NUM_LANES)) tx (
    .clk            (clk),
    .rstn           (rstn),
    .cfg_enable     (cfg_enable),
    .cfg_lanes      (cfg_lanes),
    .cfg_clk_div    (cfg_clk_div),
    .tx_byte        (tx_byte),
    .tx_wr          (tx_wr),
    .tx_full        (tx_full),
    .tx_empty       (tx_empty),
    .tx_busy        (tx_busy),
    .mfx_clk        (mfx_clk_pad),
    .mfx_tx         (mfx_tx),
    .mfx_sync       (mfx_sync_pad),
    .mfx_clk_fabric (mfx_clk_fabric),
    .mfx_sync_fabric(mfx_sync_fabric)
  );

  multiflex_rx #(.NUM_LANES(NUM_LANES)) rx (
    .clk         (clk),
    .rstn        (rstn),
    .cfg_enable  (cfg_enable),
    .cfg_lanes   (cfg_lanes),
    .clr_rx      (1'b0),
    .mfx_clk_in  (mfx_clk_fabric),
    .mfx_rx_in   (mfx_tx),
    .mfx_sync_in (mfx_sync_fabric),
    .rx_byte     (rx_byte),
    .rx_valid    (rx_valid),
    .rx_locked   (rx_locked),
    .rx_sync_lost(rx_sync_lost)
  );

endmodule
