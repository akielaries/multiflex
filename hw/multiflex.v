`timescale 1ns/1ns
// multiflex.v -- APB wrapper: multiflex_regs + multiflex_tx + multiflex_rx
//
// clock domains:
//   pclk: APB peripheral clock (50 MHz from Cortex-M1 APB1PCLK)
//   clk:  TX/RX engine clock (200 MHz from PLL)
//
// CDC crossings:
//   pclk->clk: cfg_enable/lanes_tx/lanes_rx/clk_div/loopback  2-flop sync
//              tx_wr + tx_byte                                  toggle pulse sync
//              clr_tx, clr_rx                                   toggle pulse sync
//   clk->pclk: tx_full/empty/busy                              2-flop sync per bit
//              tx_overflow, tx_drop_count                       2-flop sync
//              rx_locked, rx_sync_lost, rx_error_count          2-flop sync
//              rx_valid + rx_byte                               toggle pulse sync
//
// reset: prstn asynchronously asserts clk-domain reset;
//        deassert re-synchronized to clk edges via reset synchronizer
//
// BRAM clocking: both tx_buf and rx_buf are clocked entirely from pclk (cheby
//   default, no clk_hw_i).  the pclk-domain drain SM reads tx_buf and feeds
//   bytes to the TX FIFO via toggle-pulse CDC.  received bytes arrive via
//   toggle-pulse CDC (clk->pclk) and are written into rx_buf at pclk.

module multiflex #(
  parameter NUM_LANES = 3
) (
  input  wire        pclk,
  input  wire        prstn,
  input  wire        clk,

  // APB slave
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
  output wire                  mfx_sync,

  // wire inputs
  input  wire [NUM_LANES-1:0]  mfx_rx
);

  // --------------------------------------------------------------------------
  // clk-domain reset synchronizer
  // --------------------------------------------------------------------------
  reg [1:0] rstn_sync;
  always @(posedge clk or negedge prstn) begin
    if (!prstn) rstn_sync <= 2'b00;
    else        rstn_sync <= {rstn_sync[0], 1'b1};
  end
  wire rstn = rstn_sync[1];

  // --------------------------------------------------------------------------
  // pclk domain: APB write detection
  // offsets: tx_data=0x08 (word 2), error_clr=0x14 (word 5), tx_len=0x18 (word 6)
  // --------------------------------------------------------------------------
  wire tx_data_setup   = psel && pwrite && !penable && (paddr[14:2] == 13'd2);
  wire error_clr_setup = psel && pwrite && !penable && (paddr[14:2] == 13'd5);
  wire tx_len_setup    = psel && pwrite && !penable && (paddr[14:2] == 13'd6);

  reg        tx_wr_p;
  reg [7:0]  tx_byte_p;
  reg        clr_tx_p;
  reg        clr_rx_p;
  reg        tx_len_start_p;
  reg [10:0] tx_len_latch_p;  // pwdata captured at setup phase; valid 1 cycle before cheby pipeline

  always @(posedge pclk) begin
    if (!prstn) begin
      tx_wr_p        <= 1'b0;
      tx_byte_p      <= 8'd0;
      clr_tx_p       <= 1'b0;
      clr_rx_p       <= 1'b0;
      tx_len_start_p <= 1'b0;
      tx_len_latch_p <= 11'd0;
    end else begin
      tx_wr_p  <= tx_data_setup;
      if (tx_data_setup) tx_byte_p <= pwdata[7:0];
      clr_tx_p       <= error_clr_setup && pwdata[0];
      clr_rx_p       <= error_clr_setup && pwdata[1];
      tx_len_start_p <= tx_len_setup && (pwdata[10:0] != 11'd0);
      if (tx_len_setup) tx_len_latch_p <= pwdata[10:0];
    end
  end

  // --------------------------------------------------------------------------
  // CDC pclk->clk: config signals (2-flop sync)
  // --------------------------------------------------------------------------
  wire        cfg_enable_p;
  wire [4:0]  cfg_lanes_tx_p;
  wire [4:0]  cfg_lanes_rx_p;
  wire [7:0]  cfg_clk_div_p;
  wire        cfg_loopback_p;
  wire [10:0] tx_len_p;

  reg         cfg_enable_c0,    cfg_enable_c;
  reg  [4:0]  cfg_lanes_tx_c0,  cfg_lanes_tx_c;
  reg  [4:0]  cfg_lanes_rx_c0,  cfg_lanes_rx_c;
  reg  [7:0]  cfg_clk_div_c0,   cfg_clk_div_c;
  reg         cfg_loopback_c0,  cfg_loopback_c;

  always @(posedge clk) begin
    if (!rstn) begin
      cfg_enable_c0   <= 1'b0;  cfg_enable_c   <= 1'b0;
      cfg_lanes_tx_c0 <= 5'd0;  cfg_lanes_tx_c <= 5'd0;
      cfg_lanes_rx_c0 <= 5'd0;  cfg_lanes_rx_c <= 5'd0;
      cfg_clk_div_c0  <= 8'd0;  cfg_clk_div_c  <= 8'd0;
      cfg_loopback_c0 <= 1'b0;  cfg_loopback_c <= 1'b0;
    end else begin
      cfg_enable_c0   <= cfg_enable_p;    cfg_enable_c   <= cfg_enable_c0;
      cfg_lanes_tx_c0 <= cfg_lanes_tx_p;  cfg_lanes_tx_c <= cfg_lanes_tx_c0;
      cfg_lanes_rx_c0 <= cfg_lanes_rx_p;  cfg_lanes_rx_c <= cfg_lanes_rx_c0;
      cfg_clk_div_c0  <= cfg_clk_div_p;   cfg_clk_div_c  <= cfg_clk_div_c0;
      cfg_loopback_c0 <= cfg_loopback_p;  cfg_loopback_c <= cfg_loopback_c0;
    end
  end

  // --------------------------------------------------------------------------
  // CDC pclk->clk: clr_tx pulse synchronizer
  // --------------------------------------------------------------------------
  reg tog_clr_tx;
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
  // CDC pclk->clk: clr_rx pulse synchronizer
  // --------------------------------------------------------------------------
  reg tog_clr_rx;
  always @(posedge pclk) begin
    if (!prstn) tog_clr_rx <= 1'b0;
    else if (clr_rx_p) tog_clr_rx <= ~tog_clr_rx;
  end

  reg [2:0] sync_clr_rx;
  always @(posedge clk) begin
    if (!rstn) sync_clr_rx <= 3'b0;
    else       sync_clr_rx <= {sync_clr_rx[1:0], tog_clr_rx};
  end
  wire clr_rx_c = sync_clr_rx[2] ^ sync_clr_rx[1];

  // --------------------------------------------------------------------------
  // clk domain: TX live status wires (driven by multiflex_tx)
  // forward-declare wires used before their driving logic appears below
  // --------------------------------------------------------------------------
  wire tx_full_c, tx_empty_c, tx_busy_c;
  wire tx_wr_c;          // assigned after the pclk->clk CDC sync chain below
  wire [7:0] rx_byte_c;  // driven by multiflex_rx instantiation below
  wire       rx_valid_c;
  wire       rx_locked_c;
  wire       rx_sync_lost_c;

  // --------------------------------------------------------------------------
  // clk domain: tx_overflow + tx_drop_count
  // --------------------------------------------------------------------------
  reg       tx_overflow_c;
  reg [7:0] tx_drop_count_c;

  always @(posedge clk) begin
    if (!rstn || clr_tx_c) tx_overflow_c <= 1'b0;
    else if (tx_wr_c && tx_full_c) tx_overflow_c <= 1'b1;
  end

  always @(posedge clk) begin
    if (!rstn || clr_tx_c) tx_drop_count_c <= 8'd0;
    else if (tx_wr_c && tx_full_c && tx_drop_count_c != 8'hFF) begin
      tx_drop_count_c <= tx_drop_count_c + 8'd1;
    end
  end

  // --------------------------------------------------------------------------
  // CDC clk->pclk: TX status (2-flop sync per bit)
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
  // pclk domain: tx_buf drain state machine
  // reads from tx_buf (port B, pclk) and feeds TX FIFO via toggle-pulse CDC.
  // fires on tx_len_start_p; uses tx_len_latch_p (pwdata captured at setup phase,
  // 1 cycle before cheby pipeline) so the count is available immediately.
  // uses tx_full_p1 for backpressure (2-flop sync of tx_full_c).
  // BRAM latency: 1 pclk cycle (combinatorial array + OCE output register).
  // --------------------------------------------------------------------------
  // drain SM states:
  //   IDLE  -> READ (issue BRAM read) -> WRITE (fire byte, tx_full check)
  //   WRITE -> WAIT1 -> WAIT2 -> WAIT3 -> WAIT4 (issue BRAM read) -> READ -> WRITE
  //
  // the 4 wait states (WAIT1..WAIT4) ensure 6 pclk cycles elapse between
  // successive drain_wr_p pulses.  pclk (50 MHz) and clk (100 MHz) are
  // asynchronous.  worst-case round-trip: toggle-pulse crosses clk domain
  // (up to 1 clk period = 2 pclk), tx_full_c asserts, 2-flop sync to pclk
  // adds 2 more pclk, plus 1 pclk setup margin = 5 pclk total.
  // checking at cycle N+6 guarantees tx_full_p1 reflects the FIFO state
  // AFTER our previous write, preventing overflow.
  localparam DRAIN_IDLE      = 3'd0;
  localparam DRAIN_READ      = 3'd1;
  localparam DRAIN_WRITE     = 3'd2;
  localparam DRAIN_WAIT1     = 3'd3;
  localparam DRAIN_WAIT2     = 3'd4;
  localparam DRAIN_WAIT3     = 3'd5;
  localparam DRAIN_WAIT4     = 3'd6;
  localparam DRAIN_LAST_WAIT = 3'd7;

  reg [2:0]  drain_state_p;
  reg [10:0] drain_ptr_p;
  reg [10:0] drain_remaining_p;
  reg        drain_wr_p;
  reg [7:0]  drain_byte_p;
  reg [10:0] tx_buf_adr_p;
  reg        tx_buf_rd_p;
  wire [7:0] tx_buf_dat_p;

  always @(posedge pclk) begin
    drain_wr_p  <= 1'b0;
    tx_buf_rd_p <= 1'b0;

    if (!prstn || !cfg_enable_p) begin
      drain_state_p     <= DRAIN_IDLE;
      drain_ptr_p       <= 11'd0;
      drain_remaining_p <= 11'd0;
    end else begin
      case (drain_state_p)
        DRAIN_IDLE: begin
          if (tx_len_start_p) begin
            drain_ptr_p       <= 11'd0;
            drain_remaining_p <= tx_len_latch_p;  // latched from pwdata at setup phase
            tx_buf_adr_p      <= 11'd0;
            tx_buf_rd_p       <= 1'b1;
            drain_state_p     <= DRAIN_READ;
          end
        end

        DRAIN_READ: begin
          // BRAM read issued last cycle; OCE output valid after this edge
          drain_state_p <= DRAIN_WRITE;
        end

        DRAIN_WRITE: begin
          if (!tx_full_p1) begin
            drain_wr_p        <= 1'b1;
            drain_byte_p      <= tx_buf_dat_p;
            drain_remaining_p <= drain_remaining_p - 11'd1;
            drain_ptr_p       <= drain_ptr_p + 11'd1;
            // always go through DRAIN_WAIT1 -- critical for correct CDC:
            // drain_wr_p is an NBA, so tog_tx_wr reads drain_wr_p from the
            // PREVIOUS cycle.  drain_wr_p=1 set here is only VISIBLE to
            // tog_tx_wr in DRAIN_WAIT1 (the cycle after).  going directly
            // to DRAIN_IDLE on the last byte would make drain_state_p=IDLE
            // at that next cycle, causing tx_wr_combined_p to use tx_wr_p=0
            // instead of drain_wr_p=1, silently dropping the last byte.
            drain_state_p <= DRAIN_WAIT1;
          end
          // tx_full: stay in DRAIN_WRITE; BRAM output remains valid
        end

        DRAIN_WAIT1: begin
          // tog_tx_wr fires here (seeing drain_wr_p=1 from the previous cycle).
          // check remaining to decide whether we are done or need more bytes.
          if (drain_remaining_p == 11'd0) begin
            // last byte: stay non-IDLE for one more cycle (DRAIN_LAST_WAIT) so
            // tx_byte_combined_p = drain_byte_p remains stable while the toggle
            // propagates through the 3-flop clk sync and tx_byte_c captures the
            // correct value before tx_wr_c fires
            drain_state_p <= DRAIN_LAST_WAIT;
          end else begin
            drain_state_p <= DRAIN_WAIT2;
          end
        end

        DRAIN_WAIT2: begin
          drain_state_p <= DRAIN_WAIT3;
        end

        DRAIN_WAIT3: begin
          drain_state_p <= DRAIN_WAIT4;
        end

        DRAIN_WAIT4: begin
          // issue BRAM read for next byte (drain_ptr_p already incremented)
          tx_buf_adr_p  <= drain_ptr_p;
          tx_buf_rd_p   <= 1'b1;
          drain_state_p <= DRAIN_READ;
        end

        DRAIN_LAST_WAIT: begin
          drain_state_p <= DRAIN_IDLE;
        end

        default: drain_state_p <= DRAIN_IDLE;
      endcase
    end
  end

  // drain SM takes priority over streaming writes; both are single-cycle pulses
  wire        tx_wr_combined_p   = (drain_state_p != DRAIN_IDLE) ? drain_wr_p   : tx_wr_p;
  wire [7:0]  tx_byte_combined_p = (drain_state_p != DRAIN_IDLE) ? drain_byte_p : tx_byte_p;

  // tx_busy exposed to status register includes drain SM active
  wire tx_busy_status_p = tx_busy_p1 || (drain_state_p != DRAIN_IDLE);

  // --------------------------------------------------------------------------
  // CDC pclk->clk: combined tx_wr pulse synchronizer
  // --------------------------------------------------------------------------
  reg tog_tx_wr;
  always @(posedge pclk) begin
    if (!prstn) tog_tx_wr <= 1'b0;
    else if (tx_wr_combined_p) tog_tx_wr <= ~tog_tx_wr;
  end

  reg [2:0] sync_tx_wr;
  always @(posedge clk) begin
    if (!rstn) sync_tx_wr <= 3'b0;
    else       sync_tx_wr <= {sync_tx_wr[1:0], tog_tx_wr};
  end
  assign tx_wr_c = sync_tx_wr[2] ^ sync_tx_wr[1];

  // --------------------------------------------------------------------------
  // CDC pclk->clk: tx_byte (2-flop sync; stable well before tx_wr_c fires)
  // --------------------------------------------------------------------------
  reg [7:0] tx_byte_c0, tx_byte_c;
  always @(posedge clk) begin
    tx_byte_c0 <= tx_byte_combined_p;
    tx_byte_c  <= tx_byte_c0;
  end

  // clk domain: multiflex_rx outputs (declared in forward-decl block above)

  // --------------------------------------------------------------------------
  // clk domain: pattern checker (incrementing counter 0x00..0xFF)
  // --------------------------------------------------------------------------
  reg [7:0] rx_expected_c;
  reg [7:0] rx_error_count_c;

  always @(posedge clk) begin
    if (!rstn || clr_rx_c) begin
      rx_expected_c    <= 8'd0;
      rx_error_count_c <= 8'd0;
    end else if (rx_valid_c) begin
      if (rx_byte_c != rx_expected_c && rx_error_count_c != 8'hFF) begin
        rx_error_count_c <= rx_error_count_c + 8'd1;
      end
      rx_expected_c <= rx_expected_c + 8'd1;
    end
  end

  // --------------------------------------------------------------------------
  // CDC clk->pclk: RX status (2-flop sync per bit)
  // --------------------------------------------------------------------------
  reg        rx_locked_p0,    rx_locked_p1;
  reg        rx_sync_lost_p0, rx_sync_lost_p1;
  reg [7:0]  rx_error_p0,     rx_error_p1;

  always @(posedge pclk) begin
    if (!prstn) begin
      {rx_locked_p0,    rx_locked_p1}    <= 2'b00;
      {rx_sync_lost_p0, rx_sync_lost_p1} <= 2'b00;
      rx_error_p0 <= 8'd0; rx_error_p1 <= 8'd0;
    end else begin
      rx_locked_p0    <= rx_locked_c;      rx_locked_p1    <= rx_locked_p0;
      rx_sync_lost_p0 <= rx_sync_lost_c;   rx_sync_lost_p1 <= rx_sync_lost_p0;
      rx_error_p0     <= rx_error_count_c; rx_error_p1     <= rx_error_p0;
    end
  end

  // --------------------------------------------------------------------------
  // CDC clk->pclk: rx_valid + rx_byte (toggle pulse sync)
  // rx_byte_hold_c is captured at clk when rx_valid fires; byte data is then
  // re-sampled twice at pclk before use to cross the clock domain safely.
  // --------------------------------------------------------------------------
  reg [7:0] rx_byte_hold_c;
  reg       tog_rx_valid_c;

  always @(posedge clk) begin
    if (!rstn) begin
      rx_byte_hold_c <= 8'd0;
      tog_rx_valid_c <= 1'b0;
    end else if (rx_valid_c) begin
      rx_byte_hold_c <= rx_byte_c;
      tog_rx_valid_c <= ~tog_rx_valid_c;
    end
  end

  reg [2:0] sync_rx_valid_p;
  always @(posedge pclk) begin
    if (!prstn) sync_rx_valid_p <= 3'b0;
    else        sync_rx_valid_p <= {sync_rx_valid_p[1:0], tog_rx_valid_c};
  end
  wire rx_wr_p = sync_rx_valid_p[2] ^ sync_rx_valid_p[1];

  reg [7:0] rx_byte_p0, rx_byte_p;
  always @(posedge pclk) begin
    rx_byte_p0 <= rx_byte_hold_c;
    rx_byte_p  <= rx_byte_p0;
  end

  // --------------------------------------------------------------------------
  // pclk domain: rx_buf write pointer + byte count
  // --------------------------------------------------------------------------
  reg [10:0] rx_buf_wptr_p;
  reg [10:0] rx_count_p;

  wire rx_buf_we_p = rx_wr_p && cfg_enable_p && (rx_count_p != 11'h7FF);

  always @(posedge pclk) begin
    if (!prstn || clr_rx_p || !cfg_enable_p) begin
      rx_buf_wptr_p <= 11'd0;
      rx_count_p    <= 11'd0;
    end else if (rx_wr_p && rx_count_p != 11'h7FF) begin
      rx_buf_wptr_p <= rx_buf_wptr_p + 11'd1;
      rx_count_p    <= rx_count_p    + 11'd1;
    end
  end

  // --------------------------------------------------------------------------
  // pclk domain: multiflex_regs instantiation
  // --------------------------------------------------------------------------
  multiflex_regs regs (
    .pclk    (pclk),
    .presetn (prstn),
    .paddr   (paddr[14:2]),
    .psel    (psel),
    .pwrite  (pwrite),
    .penable (penable),
    .pready  (pready),
    .pwdata  (pwdata),
    .pstrb   (4'hF),
    .prdata  (prdata),
    .pslverr (pslverr),

    .ctrl_enable_o    (cfg_enable_p),
    .ctrl_lanes_tx_o  (cfg_lanes_tx_p),
    .ctrl_lanes_rx_o  (cfg_lanes_rx_p),
    .ctrl_clk_div_o   (cfg_clk_div_p),
    .ctrl_loopback_o  (cfg_loopback_p),

    .status_tx_busy_i        (tx_busy_status_p),
    .status_tx_full_i        (tx_full_p1),
    .status_tx_empty_i       (tx_empty_p1),
    .status_tx_overflow_i    (tx_overflow_p1),
    .status_rx_locked_i      (rx_locked_p1),
    .status_rx_clk_timeout_i (1'b0),
    .status_rx_sync_lost_i   (rx_sync_lost_p1),
    .status_rx_framing_err_i (1'b0),

    .tx_data_data_o (),

    .rx_data_data_i  (8'd0),
    .rx_data_valid_i (1'b0),

    .error_cnt_tx_drop_count_i  (tx_drop_p1),
    .error_cnt_rx_error_count_i (rx_error_p1),

    .error_clr_clr_tx_o (),
    .error_clr_clr_rx_o (),

    .tx_len_len_o     (tx_len_p),
    .rx_count_count_i (rx_count_p),

    // tx_buf port B: drain SM reads at pclk
    .tx_buf_adr_i      (tx_buf_adr_p),
    .tx_buf_data_rd_i  (tx_buf_rd_p),
    .tx_buf_data_dat_o (tx_buf_dat_p),

    // rx_buf port B: pclk-domain CDC path writes received bytes
    .rx_buf_adr_i      (rx_buf_wptr_p),
    .rx_buf_data_we_i  (rx_buf_we_p),
    .rx_buf_data_dat_i (rx_byte_p)
  );

  // --------------------------------------------------------------------------
  // clk domain: multiflex_tx instantiation
  // --------------------------------------------------------------------------
  wire mfx_clk_fabric;
  wire mfx_sync_fabric;

  multiflex_tx #(.NUM_LANES(NUM_LANES)) tx (
    .clk            (clk),
    .rstn           (rstn),
    .cfg_enable     (cfg_enable_c),
    .cfg_lanes      (cfg_lanes_tx_c),
    .cfg_clk_div    (cfg_clk_div_c),
    .tx_byte        (tx_byte_c),
    .tx_wr          (tx_wr_c),
    .tx_full        (tx_full_c),
    .tx_empty       (tx_empty_c),
    .tx_busy        (tx_busy_c),
    .mfx_clk        (mfx_clk),
    .mfx_tx         (mfx_tx),
    .mfx_sync       (mfx_sync),
    .mfx_clk_fabric (mfx_clk_fabric),
    .mfx_sync_fabric(mfx_sync_fabric)
  );

  // --------------------------------------------------------------------------
  // clk domain: loopback mux + multiflex_rx instantiation
  // clock and sync always from TX engine; data switches between fabric and pins.
  // mfx_clk_fabric/mfx_sync_fabric are FF outputs inside multiflex_tx that are
  // driven by the same flip-flop source as mfx_clk/mfx_sync but are NOT
  // connected to output pads, so Gowin cannot promote them to the global clock
  // network.  using these avoids the edge-detect breakage that occurs when
  // mfx_clk (output pad, name contains "clk") is promoted to a clock-only net.
  // --------------------------------------------------------------------------
  wire [NUM_LANES-1:0] rx_data_in = cfg_loopback_c ? mfx_tx : mfx_rx;

  multiflex_rx #(.NUM_LANES(NUM_LANES)) rx (
    .clk         (clk),
    .rstn        (rstn),
    .cfg_enable  (cfg_enable_c),
    .cfg_lanes   (cfg_lanes_rx_c),
    .clr_rx      (clr_rx_c),
    .mfx_clk_in  (mfx_clk_fabric),
    .mfx_rx_in   (rx_data_in),
    .mfx_sync_in (mfx_sync_fabric),
    .rx_byte     (rx_byte_c),
    .rx_valid    (rx_valid_c),
    .rx_locked   (rx_locked_c),
    .rx_sync_lost(rx_sync_lost_c)
  );

endmodule
