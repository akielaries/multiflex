`timescale 1ns/1ns
// multiflex.v -- APB wrapper: multiflex_regs (pclk domain) + multiflex_tx (clk domain)
//
// clock domains:
//   pclk: APB peripheral clock (50 MHz from Cortex-M1 APB1PCLK)
//   clk:  TX engine clock (100 MHz from PLL)
//
// CDC crossings:
//   pclk->clk: cfg_enable/lanes/clk_div  2-flop sync (slow config, stable before enable)
//              tx_wr + tx_byte            toggle pulse sync; byte stable before pulse arrives
//              clr_tx                     toggle pulse sync
//   clk->pclk: tx_full/empty/busy        2-flop sync per bit
//              tx_overflow                2-flop sync
//              tx_drop_count              2-flop per-bit sync (saturating, off-by-one ok)
//
// reset: prstn (pclk domain, active-low) asynchronously asserts clk-domain reset;
//        deassert is re-synchronized to clk edges via reset synchronizer

module multiflex #(
  parameter NUM_LANES = 3
) (
  input  wire        pclk,    // APB clock
  input  wire        prstn,   // APB reset (active-low)
  input  wire        clk,     // TX engine clock (from PLL)

  // APB slave (full 32-bit address; cheby uses paddr[4:2] internally)
  input  wire [31:0] paddr,
  input  wire        psel,
  input  wire        penable,
  input  wire        pwrite,
  input  wire [31:0] pwdata,
  output wire [31:0] prdata,
  output wire        pready,
  output wire        pslverr,

  // wire outputs
  output wire                  mfx_clk,
  output wire [NUM_LANES-1:0]  mfx_tx,
  output wire                  mfx_sync
);

  // --------------------------------------------------------------------------
  // clk-domain reset synchronizer
  // async assert follows prstn immediately; deassert is synchronous to clk
  // --------------------------------------------------------------------------
  reg [1:0] rstn_sync;
  always @(posedge clk or negedge prstn) begin
    if (!prstn) rstn_sync <= 2'b00;
    else        rstn_sync <= {rstn_sync[0], 1'b1};
  end
  wire rstn = rstn_sync[1];

  // --------------------------------------------------------------------------
  // pclk domain: APB write detection
  // tx_data (offset 0x08, paddr[4:2]==010) registered one cycle to match
  // cheby's wr_req_d0 pipeline; error_clr (0x14, paddr[4:2]==101) same
  // --------------------------------------------------------------------------
  wire tx_data_setup   = psel && pwrite && !penable && (paddr[4:2] == 3'b010);
  wire error_clr_setup = psel && pwrite && !penable && (paddr[4:2] == 3'b101);

  reg       tx_wr_p;
  reg [7:0] tx_byte_p;
  reg       clr_tx_p;

  always @(posedge pclk) begin
    if (!prstn) begin
      tx_wr_p   <= 1'b0;
      tx_byte_p <= 8'd0;
      clr_tx_p  <= 1'b0;
    end else begin
      tx_wr_p  <= tx_data_setup;
      if (tx_data_setup) tx_byte_p <= pwdata[7:0];
      clr_tx_p <= error_clr_setup && pwdata[0];
    end
  end

  // --------------------------------------------------------------------------
  // CDC pclk->clk: tx_wr pulse synchronizer
  // toggle FF in pclk domain; 3-flop chain + edge detect in clk domain
  // --------------------------------------------------------------------------
  reg       tog_tx_wr;
  always @(posedge pclk) begin
    if (!prstn) tog_tx_wr <= 1'b0;
    else if (tx_wr_p) tog_tx_wr <= ~tog_tx_wr;
  end

  reg [2:0] sync_tx_wr;
  always @(posedge clk) begin
    if (!rstn) sync_tx_wr <= 3'b0;
    else       sync_tx_wr <= {sync_tx_wr[1:0], tog_tx_wr};
  end
  wire tx_wr_c = sync_tx_wr[2] ^ sync_tx_wr[1];

  // --------------------------------------------------------------------------
  // CDC pclk->clk: clr_tx pulse synchronizer
  // --------------------------------------------------------------------------
  reg       tog_clr_tx;
  always @(posedge pclk) begin
    if (!prstn) tog_clr_tx <= 1'b0;
    else if (clr_tx_p) tog_clr_tx <= ~tog_clr_tx;
  end

  reg [2:0] sync_clr_tx;
  always @(posedge clk) begin
    if (!rstn) sync_clr_tx <= 3'b0;
    else       sync_clr_tx <= {sync_clr_tx[1:0], tog_clr_tx};
  end
  wire clr_tx_c = sync_clr_tx[2] ^ sync_clr_tx[1];

  // --------------------------------------------------------------------------
  // CDC pclk->clk: tx_byte (2-flop sync)
  // tx_byte_p is captured one pclk cycle before tog_tx_wr toggles, so it is
  // stable for 3+ clk cycles before tx_wr_c fires -- safe to 2-flop sync
  // --------------------------------------------------------------------------
  reg [7:0] tx_byte_c0, tx_byte_c;
  always @(posedge clk) begin
    tx_byte_c0 <= tx_byte_p;
    tx_byte_c  <= tx_byte_c0;
  end

  // --------------------------------------------------------------------------
  // CDC pclk->clk: config signals (2-flop sync, slow-changing)
  // firmware protocol: write lanes/clk_div first, then write enable=1;
  // the 2-cycle sync latency is invisible since config only changes at init
  // --------------------------------------------------------------------------
  wire        cfg_enable_p;
  wire [4:0]  cfg_lanes_p;
  wire [7:0]  cfg_clk_div_p;

  reg         cfg_enable_c0,  cfg_enable_c;
  reg  [4:0]  cfg_lanes_c0,   cfg_lanes_c;
  reg  [7:0]  cfg_clk_div_c0, cfg_clk_div_c;

  always @(posedge clk) begin
    if (!rstn) begin
      cfg_enable_c0  <= 1'b0; cfg_enable_c  <= 1'b0;
      cfg_lanes_c0   <= 5'd0; cfg_lanes_c   <= 5'd0;
      cfg_clk_div_c0 <= 8'd0; cfg_clk_div_c <= 8'd0;
    end else begin
      cfg_enable_c0  <= cfg_enable_p;  cfg_enable_c  <= cfg_enable_c0;
      cfg_lanes_c0   <= cfg_lanes_p;   cfg_lanes_c   <= cfg_lanes_c0;
      cfg_clk_div_c0 <= cfg_clk_div_p; cfg_clk_div_c <= cfg_clk_div_c0;
    end
  end

  // --------------------------------------------------------------------------
  // clk domain: TX live status from multiflex_tx
  // --------------------------------------------------------------------------
  wire tx_full_c, tx_empty_c, tx_busy_c;

  // --------------------------------------------------------------------------
  // clk domain: tx_overflow sticky bit and tx_drop_count saturating counter
  // --------------------------------------------------------------------------
  reg       tx_overflow_c;
  reg [7:0] tx_drop_count_c;

  always @(posedge clk) begin
    if (!rstn || clr_tx_c) tx_overflow_c <= 1'b0;
    else if (tx_wr_c && tx_full_c) tx_overflow_c <= 1'b1;
  end

  always @(posedge clk) begin
    if (!rstn || clr_tx_c) tx_drop_count_c <= 8'd0;
    else if (tx_wr_c && tx_full_c && tx_drop_count_c != 8'hFF)
      tx_drop_count_c <= tx_drop_count_c + 8'd1;
  end

  // --------------------------------------------------------------------------
  // CDC clk->pclk: status signals (2-flop sync per bit)
  // --------------------------------------------------------------------------
  reg       tx_full_p0,      tx_full_p1;
  reg       tx_empty_p0,     tx_empty_p1;
  reg       tx_busy_p0,      tx_busy_p1;
  reg       tx_overflow_p0,  tx_overflow_p1;
  reg [7:0] tx_drop_p0,      tx_drop_p1;

  always @(posedge pclk) begin
    if (!prstn) begin
      {tx_full_p0,     tx_full_p1}     <= 2'b00;
      {tx_empty_p0,    tx_empty_p1}    <= 2'b11;
      {tx_busy_p0,     tx_busy_p1}     <= 2'b00;
      {tx_overflow_p0, tx_overflow_p1} <= 2'b00;
      tx_drop_p0 <= 8'd0; tx_drop_p1  <= 8'd0;
    end else begin
      tx_full_p0     <= tx_full_c;       tx_full_p1     <= tx_full_p0;
      tx_empty_p0    <= tx_empty_c;      tx_empty_p1    <= tx_empty_p0;
      tx_busy_p0     <= tx_busy_c;       tx_busy_p1     <= tx_busy_p0;
      tx_overflow_p0 <= tx_overflow_c;   tx_overflow_p1 <= tx_overflow_p0;
      tx_drop_p0     <= tx_drop_count_c; tx_drop_p1     <= tx_drop_p0;
    end
  end

  // --------------------------------------------------------------------------
  // pclk domain: multiflex_regs instantiation
  // --------------------------------------------------------------------------
  multiflex_regs regs (
    .pclk    (pclk),
    .presetn (prstn),
    .paddr   (paddr[4:2]),
    .psel    (psel),
    .pwrite  (pwrite),
    .penable (penable),
    .pready  (pready),
    .pwdata  (pwdata),
    .pstrb   (4'hF),
    .prdata  (prdata),
    .pslverr (pslverr),

    .ctrl_enable_o   (cfg_enable_p),
    .ctrl_lanes_o    (cfg_lanes_p),
    .ctrl_clk_div_o  (cfg_clk_div_p),
    .ctrl_loopback_o (),

    .status_tx_busy_i        (tx_busy_p1),
    .status_tx_full_i        (tx_full_p1),
    .status_tx_empty_i       (tx_empty_p1),
    .status_tx_overflow_i    (tx_overflow_p1),
    .status_rx_locked_i      (1'b0),
    .status_rx_clk_timeout_i (1'b0),
    .status_rx_sync_lost_i   (1'b0),
    .status_rx_framing_err_i (1'b0),
    .status_rx_overflow_i    (1'b0),

    .tx_data_data_o (),

    .rx_data_data_i  (8'd0),
    .rx_data_valid_i (1'b0),

    .error_cnt_tx_drop_count_i  (tx_drop_p1),
    .error_cnt_rx_error_count_i (8'd0),

    .error_clr_clr_tx_o (),
    .error_clr_clr_rx_o ()
  );

  // --------------------------------------------------------------------------
  // clk domain: multiflex_tx instantiation
  // --------------------------------------------------------------------------
  multiflex_tx #(.NUM_LANES(NUM_LANES)) tx (
    .clk         (clk),
    .rstn        (rstn),
    .cfg_enable  (cfg_enable_c),
    .cfg_lanes   (cfg_lanes_c),
    .cfg_clk_div (cfg_clk_div_c),
    .tx_byte     (tx_byte_c),
    .tx_wr       (tx_wr_c),
    .tx_full     (tx_full_c),
    .tx_empty    (tx_empty_c),
    .tx_busy     (tx_busy_c),
    .mfx_clk     (mfx_clk),
    .mfx_tx      (mfx_tx),
    .mfx_sync    (mfx_sync)
  );

endmodule
