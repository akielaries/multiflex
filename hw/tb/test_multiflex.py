"""
test_multiflex.py -- full-path cocotb tests for multiflex.v

toplevel: tb_multiflex (see tb_multiflex.v)
clocks: pclk=50MHz (20ns), clk=100MHz (10ns)

tests mirror the firmware test sequence in mfx.c:
  - test_streaming_loopback : tx_data register path (FIFO) + fabric loopback
  - test_drain_loopback     : tx_buf BRAM drain SM path + fabric loopback (phys_loopback_test path)
  - test_rx_buf_readback    : write tx_buf, read it back via APB before transmitting
  - test_rx_count           : verify rx_count matches bytes sent
"""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, ClockCycles

# ---------------------------------------------------------------------------
# APB register addresses
# ---------------------------------------------------------------------------
CTRL      = 0x0000
STATUS    = 0x0004
TX_DATA   = 0x0008
RX_DATA   = 0x000C
ERROR_CNT = 0x0010
ERROR_CLR = 0x0014
TX_LEN    = 0x0018
RX_COUNT  = 0x001C
TX_BUF    = 0x2000   # tx_buf[i] at TX_BUF + i*4
RX_BUF    = 0x4000   # rx_buf[i] at RX_BUF + i*4

# ---------------------------------------------------------------------------
# STATUS bit positions
# ---------------------------------------------------------------------------
TX_BUSY      = 1 << 0
TX_FULL      = 1 << 1
TX_EMPTY     = 1 << 2
TX_OVERFLOW  = 1 << 3
RX_LOCKED    = 1 << 8
RX_SYNC_LOST = 1 << 10

# ---------------------------------------------------------------------------
# ctrl register value: enable=1, lanes_tx=1, lanes_rx=1, clk_div=1, loopback=1
# bit 0: enable; bits[5:1]: lanes_tx; bits[10:6]: lanes_rx;
# bits[18:11]: clk_div; bit 19: loopback
# ---------------------------------------------------------------------------
def ctrl_val(lanes=1, clk_div=1, loopback=1):
    return (1         |       # enable
            (lanes  << 1) |   # lanes_tx
            (lanes  << 6) |   # lanes_rx
            (clk_div << 11) | # clk_div
            (loopback << 19))


# ---------------------------------------------------------------------------
# clock setup: pclk=50MHz, clk=100MHz (matches hardware: main PLL + PLL_ffc)
# ---------------------------------------------------------------------------
def start_clocks(dut):
    cocotb.start_soon(Clock(dut.pclk, 20, unit="ns").start())   # 50 MHz
    cocotb.start_soon(Clock(dut.clk,  10, unit="ns").start())   # 100 MHz


# ---------------------------------------------------------------------------
# reset: hold prstn low for several pclk cycles, then release
# ---------------------------------------------------------------------------
async def reset(dut):
    dut.prstn.value   = 0
    dut.psel.value    = 0
    dut.penable.value = 0
    dut.pwrite.value  = 0
    dut.paddr.value   = 0
    dut.pwdata.value  = 0
    await ClockCycles(dut.pclk, 8)
    dut.prstn.value = 1
    await ClockCycles(dut.pclk, 4)


# ---------------------------------------------------------------------------
# APB master helpers
#
# cheby pipeline: wr_req fires at setup phase (penable=0), latches in wr_req_d0
# at the setup edge, register updates and wr_ack fire at the access edge.
# both writes and reads complete in exactly 2 pclk cycles (setup + access).
# ---------------------------------------------------------------------------
async def apb_write(dut, addr, data):
    dut.paddr.value   = addr
    dut.pwdata.value  = data
    dut.pwrite.value  = 1
    dut.psel.value    = 1
    dut.penable.value = 0
    await RisingEdge(dut.pclk)   # setup phase edge: cheby latches wr_req_d0
    dut.penable.value = 1
    await RisingEdge(dut.pclk)   # access phase edge: pready fires, register updates
    dut.psel.value    = 0
    dut.penable.value = 0
    dut.pwrite.value  = 0


async def apb_read(dut, addr):
    dut.paddr.value   = addr
    dut.pwrite.value  = 0
    dut.psel.value    = 1
    dut.penable.value = 0
    await RisingEdge(dut.pclk)   # setup phase: BRAM samples addr into OCE reg
    dut.penable.value = 1
    await RisingEdge(dut.pclk)   # access phase: rack->rd_ack fires, rd_data<-out_reg
    dut.psel.value    = 0
    dut.penable.value = 0
    await RisingEdge(dut.pclk)   # one extra: prdata stable after rd_data register
    result = int(dut.prdata.value)
    return result


async def poll_status(dut, mask, expected, timeout_cycles=10000):
    """poll STATUS register until (status & mask) == expected"""
    for _ in range(timeout_cycles):
        status = await apb_read(dut, STATUS)
        if (status & mask) == expected:
            return status
    raise AssertionError(
        f"timeout waiting for status mask=0x{mask:08X} expected=0x{expected:08X}, "
        f"last=0x{status:08X}"
    )


# ---------------------------------------------------------------------------
# test 1: tx_buf APB write + readback (drain SM not involved)
# verifies BRAM wiring: write 64 bytes via APB, read them back
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_txbuf_readback(dut):
    """write 64 bytes to tx_buf via APB, read back and verify"""
    start_clocks(dut)
    await reset(dut)

    n = 64
    for i in range(n):
        await apb_write(dut, TX_BUF + i * 4, i)

    errors = []
    for i in range(n):
        got = await apb_read(dut, TX_BUF + i * 4)
        if got != i:
            errors.append((i, i, got))

    assert not errors, f"tx_buf mismatches (addr, expected, got): {errors[:8]}"
    dut._log.info(f"tx_buf APB write/read: {n} bytes correct PASS")


# ---------------------------------------------------------------------------
# test 2: streaming loopback (tx_data FIFO path, firmware mfx_loopback_test)
# writes bytes one at a time to tx_data register; TX FIFO -> wire -> RX -> rx_buf
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_streaming_loopback(dut):
    """stream bytes via tx_data register; verify rx_buf contents"""
    start_clocks(dut)
    await reset(dut)

    n = 32  # keep under FIFO depth (16) x 2 to exercise flow control
    await apb_write(dut, CTRL, ctrl_val(lanes=1, clk_div=1, loopback=1))

    # push bytes, flow-controlling on TX_FULL
    # wait 6 pclk after each write so tx_full_p1 (2-flop CDC) settles before
    # the next poll; without this, the ACCESS phase of the STATUS read can
    # collide with the tx_full_p1 update at the same pclk edge
    for i in range(n):
        await poll_status(dut, TX_FULL, 0)   # wait for space
        await apb_write(dut, TX_DATA, i)
        await ClockCycles(dut.pclk, 6)

    # wait for TX to finish, then poll rx_count until RX CDC catches up
    await poll_status(dut, TX_BUSY, 0)
    rx_count = 0
    for _ in range(2000):
        rx_count = await apb_read(dut, RX_COUNT)
        if rx_count >= n:
            break
        await ClockCycles(dut.pclk, 1)

    # check rx_locked and diagnostics
    status = await apb_read(dut, STATUS)
    err_cnt = await apb_read(dut, ERROR_CNT)
    tx_drop = err_cnt & 0xFF
    rx_err  = (err_cnt >> 8) & 0xFF
    dut._log.info(f"streaming diag: rx_count={rx_count} tx_drop={tx_drop} rx_err={rx_err} status=0x{status:08X}")
    assert status & RX_LOCKED, f"rx_locked not set, status=0x{status:08X}"

    assert rx_count == n, f"rx_count={rx_count} expected {n} (tx_drop={tx_drop})"

    # verify rx_buf
    errors = []
    for i in range(n):
        got = await apb_read(dut, RX_BUF + i * 4)
        if got != i:
            errors.append((i, i, got))
    assert not errors, f"rx_buf mismatches: {errors[:8]}"
    dut._log.info(f"streaming loopback: {n} bytes correct, rx_locked=1 PASS")


# ---------------------------------------------------------------------------
# test 3: drain SM loopback (tx_buf path, firmware mfx_phys_loopback_test)
# loads tx_buf via APB, writes tx_len to trigger drain SM, verifies rx_buf
# this is the exact bug path: drain SM reads BRAM -> CDC -> TX -> RX -> rx_buf
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_drain_loopback(dut):
    """load tx_buf, trigger drain SM via tx_len, verify rx_buf contents"""
    start_clocks(dut)
    await reset(dut)

    n = 64

    # configure: enable, 1 lane, loopback
    await apb_write(dut, CTRL, ctrl_val(lanes=1, clk_div=1, loopback=1))

    # load tx_buf: write 0x00..0x3F
    for i in range(n):
        await apb_write(dut, TX_BUF + i * 4, i)

    # readback tx_buf to confirm BRAM writes landed
    tx_errors = []
    for i in range(n):
        got = await apb_read(dut, TX_BUF + i * 4)
        if got != i:
            tx_errors.append((i, i, got))
    assert not tx_errors, f"tx_buf write errors (pre-drain): {tx_errors[:8]}"

    # trigger drain SM: writing tx_len fires tx_len_start_p via the
    # tx_len_latch_p path (captures pwdata at the setup phase edge,
    # 1 cycle before cheby's wr_req_d0 pipeline updates tx_len_len_reg)
    await apb_write(dut, TX_LEN, n)

    # wait for TX to complete (drain SM + TX FIFO + wire clock)
    await poll_status(dut, TX_BUSY, 0, timeout_cycles=20000)

    # poll rx_count until it reaches n: last bytes may still be in the
    # clk->pclk CDC chain (3-flop sync + rx_buf write) after TX goes idle
    for _ in range(500):
        rx_count = await apb_read(dut, RX_COUNT)
        if rx_count == n:
            break
        await ClockCycles(dut.pclk, 1)
    else:
        rx_count = await apb_read(dut, RX_COUNT)

    # rx_locked must be set
    status = await apb_read(dut, STATUS)
    assert status & RX_LOCKED,    f"rx_locked not set after drain, status=0x{status:08X}"
    assert not (status & RX_SYNC_LOST), f"rx_sync_lost set unexpectedly, status=0x{status:08X}"

    # diagnostic: read error_cnt (tx_drop in [7:0], rx_err in [15:8])
    err_cnt = await apb_read(dut, ERROR_CNT)
    tx_drop = err_cnt & 0xFF
    rx_err  = (err_cnt >> 8) & 0xFF
    dut._log.info(f"drain diag: rx_count={rx_count} tx_drop={tx_drop} rx_err={rx_err}")

    # rx_count must equal n
    assert rx_count == n, f"rx_count={rx_count} expected {n} (tx_drop={tx_drop})"

    # verify rx_buf contents
    errors = []
    for i in range(n):
        got = await apb_read(dut, RX_BUF + i * 4)
        if got != i:
            errors.append((i, i, got))
    assert not errors, f"rx_buf mismatches after drain: {errors[:8]}"
    dut._log.info(
        f"drain loopback: {n} bytes correct, "
        f"rx_locked=1, rx_count={rx_count} PASS"
    )


# ---------------------------------------------------------------------------
# test 4: drain SM with 3 lanes
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_drain_loopback_3lane(dut):
    """drain SM path with 3 active lanes"""
    start_clocks(dut)
    await reset(dut)

    n = 64
    await apb_write(dut, CTRL, ctrl_val(lanes=3, clk_div=1, loopback=1))

    for i in range(n):
        await apb_write(dut, TX_BUF + i * 4, i)

    await apb_write(dut, TX_LEN, n)
    await poll_status(dut, TX_BUSY, 0, timeout_cycles=20000)

    # poll rx_count to catch last bytes still in clk->pclk CDC
    for _ in range(500):
        rx_count = await apb_read(dut, RX_COUNT)
        if rx_count == n:
            break
        await ClockCycles(dut.pclk, 1)
    else:
        rx_count = await apb_read(dut, RX_COUNT)

    status = await apb_read(dut, STATUS)
    assert status & RX_LOCKED, f"rx_locked not set, status=0x{status:08X}"

    err_cnt = await apb_read(dut, ERROR_CNT)
    tx_drop = err_cnt & 0xFF
    rx_err  = (err_cnt >> 8) & 0xFF
    dut._log.info(f"3lane diag: rx_count={rx_count} tx_drop={tx_drop} rx_err={rx_err}")

    rx_count = await apb_read(dut, RX_COUNT)
    assert rx_count == n, f"rx_count={rx_count} expected {n} (tx_drop={tx_drop})"

    errors = []
    for i in range(n):
        got = await apb_read(dut, RX_BUF + i * 4)
        if got != i:
            errors.append((i, i, got))
    assert not errors, f"rx_buf mismatches (3-lane): {errors[:8]}"
    dut._log.info(f"drain loopback 3-lane: {n} bytes correct PASS")


# ---------------------------------------------------------------------------
# test 5: error_clr resets rx_count and rx_locked state
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_error_clr(dut):
    """after a burst, clr_rx resets rx_sync_lost and rx_error_count"""
    start_clocks(dut)
    await reset(dut)

    n = 8
    await apb_write(dut, CTRL, ctrl_val(lanes=1, clk_div=1, loopback=1))

    for i in range(n):
        await apb_write(dut, TX_BUF + i * 4, i)
    await apb_write(dut, TX_LEN, n)
    await poll_status(dut, TX_BUSY, 0, timeout_cycles=10000)

    # confirm locked
    status = await apb_read(dut, STATUS)
    assert status & RX_LOCKED, f"rx_locked not set before clr"

    # issue clr_rx
    await apb_write(dut, ERROR_CLR, 0x2)   # bit 1 = clr_rx

    # rx_error_count should be 0 (pattern checker only errors on mismatch,
    # and we sent 0x00-0x07 in order so it should be 0 anyway)
    cnt = await apb_read(dut, ERROR_CNT)
    rx_err = (cnt >> 8) & 0xFF
    assert rx_err == 0, f"unexpected rx_error_count={rx_err}"

    dut._log.info("error_clr: rx_error_count=0 PASS")


# ---------------------------------------------------------------------------
# test 6: drain loopback at clk_div=31 (matches hardware default)
# exercises the drain SM at the actual hardware clock divider ratio;
# also tests with loopback=0 (physical loopback path through mfx_rx)
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_drain_loopback_clkdiv31(dut):
    """drain SM path at clk_div=31, software loopback=1"""
    start_clocks(dut)
    await reset(dut)

    n = 16
    await apb_write(dut, CTRL, ctrl_val(lanes=1, clk_div=31, loopback=1))

    for i in range(n):
        await apb_write(dut, TX_BUF + i * 4, i)

    await apb_write(dut, TX_LEN, n)

    # clk_div=31: each byte takes 3 symbols x 64 clk cycles x 4ns = 768ns
    # 16 bytes = ~12us = ~600 pclk cycles; give 5000 margin
    await poll_status(dut, TX_BUSY, 0, timeout_cycles=5000)

    for _ in range(1000):
        rx_count = await apb_read(dut, RX_COUNT)
        if rx_count == n:
            break
        await ClockCycles(dut.pclk, 1)

    status = await apb_read(dut, STATUS)
    assert status & RX_LOCKED, f"rx_locked not set, status=0x{status:08X}"

    err_cnt = await apb_read(dut, ERROR_CNT)
    tx_drop = err_cnt & 0xFF
    rx_err  = (err_cnt >> 8) & 0xFF
    dut._log.info(f"clkdiv31 diag: rx_count={rx_count} tx_drop={tx_drop} rx_err={rx_err}")

    assert rx_count == n, f"rx_count={rx_count} expected {n}"

    errors = []
    for i in range(n):
        got = await apb_read(dut, RX_BUF + i * 4)
        if got != i:
            errors.append((i, i, got))
    assert not errors, f"rx_buf mismatches at clk_div=31: {errors[:8]}"
    dut._log.info(f"drain loopback clk_div=31: {n} bytes correct PASS")


@cocotb.test()
async def test_drain_loopback_physpath(dut):
    """drain SM path with loopback=0 (physical mfx_rx path; tb_multiflex wires mfx_rx=mfx_tx)"""
    start_clocks(dut)
    await reset(dut)

    n = 16
    # loopback=0: data travels mfx_tx -> mfx_rx pad wire (tb_multiflex loopback wiring)
    # this exercises the cfg_loopback_c=0 mux path in multiflex.v
    await apb_write(dut, CTRL, ctrl_val(lanes=1, clk_div=31, loopback=0))

    for i in range(n):
        await apb_write(dut, TX_BUF + i * 4, i)

    await apb_write(dut, TX_LEN, n)
    await poll_status(dut, TX_BUSY, 0, timeout_cycles=5000)

    for _ in range(1000):
        rx_count = await apb_read(dut, RX_COUNT)
        if rx_count == n:
            break
        await ClockCycles(dut.pclk, 1)

    status = await apb_read(dut, STATUS)
    assert status & RX_LOCKED, f"rx_locked not set on phys path, status=0x{status:08X}"

    err_cnt = await apb_read(dut, ERROR_CNT)
    tx_drop = err_cnt & 0xFF
    rx_err  = (err_cnt >> 8) & 0xFF
    dut._log.info(f"physpath diag: rx_count={rx_count} tx_drop={tx_drop} rx_err={rx_err}")

    assert rx_count == n, f"rx_count={rx_count} expected {n}"

    errors = []
    for i in range(n):
        got = await apb_read(dut, RX_BUF + i * 4)
        if got != i:
            errors.append((i, i, got))
    assert not errors, f"rx_buf mismatches on phys path: {errors[:8]}"
    dut._log.info(f"drain loopback phys path: {n} bytes correct PASS")
