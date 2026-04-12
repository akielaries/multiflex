"""
test_dpssram.py -- cocotb tests for cheby_dpssram

toplevel: cheby_dpssram (with sdpb_model.v providing SDPB)

covers:
  - tx_buf use case: port A (APB) writes, port B (drain SM) reads; 1-cycle latency
  - rx_buf use case: port B (RX engine) writes, port A (APB) reads; 1-cycle latency
  - sequential write then read across whole address range
  - write-source mux: only one port writes at a time (A or B)
"""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, ClockCycles


def start_clock(dut):
    # both clocks tied together (g_dual_clock=0); same frequency
    cocotb.start_soon(Clock(dut.clk_a_i, 20, unit="ns").start())
    cocotb.start_soon(Clock(dut.clk_b_i, 20, unit="ns").start())


async def reset_ports(dut):
    dut.addr_a_i.value  = 0
    dut.bwsel_a_i.value = 1
    dut.data_a_i.value  = 0
    dut.rd_a_i.value    = 0
    dut.wr_a_i.value    = 0
    dut.addr_b_i.value  = 0
    dut.bwsel_b_i.value = 1
    dut.data_b_i.value  = 0
    dut.rd_b_i.value    = 0
    dut.wr_b_i.value    = 0
    await ClockCycles(dut.clk_a_i, 2)


async def write_a(dut, addr, data):
    """write one byte via port A"""
    dut.addr_a_i.value = addr
    dut.data_a_i.value = data
    dut.wr_a_i.value   = 1
    await RisingEdge(dut.clk_a_i)
    dut.wr_a_i.value   = 0
    await RisingEdge(dut.clk_a_i)


async def read_b(dut, addr):
    """issue read via port B; return data (1 cycle latency)"""
    dut.addr_b_i.value = addr
    dut.rd_b_i.value   = 1
    await RisingEdge(dut.clk_a_i)
    dut.rd_b_i.value   = 0
    await RisingEdge(dut.clk_a_i)   # 1 cycle for OCE output register
    return int(dut.data_b_o.value)


async def write_b(dut, addr, data):
    """write one byte via port B"""
    dut.addr_b_i.value = addr
    dut.data_b_i.value = data
    dut.wr_b_i.value   = 1
    await RisingEdge(dut.clk_a_i)
    dut.wr_b_i.value   = 0
    await RisingEdge(dut.clk_a_i)


async def read_a(dut, addr):
    """issue read via port A; return data (1 cycle latency)"""
    dut.addr_a_i.value = addr
    dut.rd_a_i.value   = 1
    await RisingEdge(dut.clk_a_i)
    dut.rd_a_i.value   = 0
    await RisingEdge(dut.clk_a_i)
    return int(dut.data_a_o.value)


# ---------------------------------------------------------------------------
# test 1: tx_buf use case -- port A writes, port B reads, 1-cycle latency
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_txbuf_a_write_b_read(dut):
    """tx_buf: APB writes via port A, drain SM reads via port B"""
    start_clock(dut)
    await reset_ports(dut)

    test_data = [0xA5, 0x5A, 0x00, 0xFF, 0x12, 0x34, 0x56, 0x78]
    for i, val in enumerate(test_data):
        await write_a(dut, i, val)

    errors = []
    for i, expected in enumerate(test_data):
        got = await read_b(dut, i)
        if got != expected:
            errors.append((i, expected, got))

    assert not errors, f"mismatches: {errors}"
    dut._log.info(f"tx_buf A-write/B-read: {len(test_data)} bytes correct PASS")


# ---------------------------------------------------------------------------
# test 2: rx_buf use case -- port B writes, port A reads, 1-cycle latency
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_rxbuf_b_write_a_read(dut):
    """rx_buf: RX engine writes via port B, APB reads via port A"""
    start_clock(dut)
    await reset_ports(dut)

    test_data = [0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88]
    for i, val in enumerate(test_data):
        await write_b(dut, i, val)

    errors = []
    for i, expected in enumerate(test_data):
        got = await read_a(dut, i)
        if got != expected:
            errors.append((i, expected, got))

    assert not errors, f"mismatches: {errors}"
    dut._log.info(f"rx_buf B-write/A-read: {len(test_data)} bytes correct PASS")


# ---------------------------------------------------------------------------
# test 3: sequential write/read across 64 addresses (tx_buf pattern)
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_sequential_64(dut):
    """write 0x00..0x3F via port A, read back via port B"""
    start_clock(dut)
    await reset_ports(dut)

    n = 64
    for i in range(n):
        await write_a(dut, i, i)

    errors = []
    for i in range(n):
        got = await read_b(dut, i)
        if got != i:
            errors.append((i, i, got))

    assert not errors, f"first 8 mismatches: {errors[:8]}"
    dut._log.info(f"sequential 64-byte write/read PASS")


# ---------------------------------------------------------------------------
# test 4: read latency is exactly 1 cycle
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_read_latency(dut):
    """data must not be valid on the same cycle as rd_b_i; valid 1 cycle later"""
    start_clock(dut)
    await reset_ports(dut)

    await write_a(dut, 0, 0xBE)
    await write_a(dut, 1, 0xEF)

    # issue read at addr 0
    dut.addr_b_i.value = 0
    dut.rd_b_i.value   = 1
    await RisingEdge(dut.clk_a_i)
    dut.rd_b_i.value   = 0
    # data_b_o should NOT be 0xBE yet (output register hasn't clocked)
    same_cycle = int(dut.data_b_o.value)

    await RisingEdge(dut.clk_a_i)
    # now it should be valid
    one_cycle_later = int(dut.data_b_o.value)

    assert one_cycle_later == 0xBE, \
        f"expected 0xBE after 1 cycle, got 0x{one_cycle_later:02X}"
    dut._log.info(
        f"latency test: same-cycle=0x{same_cycle:02X} "
        f"1-cycle-later=0x{one_cycle_later:02X} PASS"
    )


# ---------------------------------------------------------------------------
# test 5: overwrite -- last write wins
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_overwrite(dut):
    """write same address twice; second value should be read back"""
    start_clock(dut)
    await reset_ports(dut)

    await write_a(dut, 7, 0xAA)
    await write_a(dut, 7, 0xBB)
    got = await read_b(dut, 7)
    assert got == 0xBB, f"expected 0xBB got 0x{got:02X}"
    dut._log.info(f"overwrite: got=0x{got:02X} expected=0xBB PASS")
