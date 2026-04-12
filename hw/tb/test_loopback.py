"""
test_loopback.py -- cocotb tests for the tx+rx loopback DUT.

toplevel: multiflex_loopback (see tb_loopback.v)
- multiflex_tx and multiflex_rx share the same clk
- mfx_clk_fabric / mfx_sync_fabric from tx feed directly to rx mfx_clk_in / mfx_sync_in
- mfx_tx[0] feeds mfx_rx_in[0]

covers:
  - rx_locked asserts on first rising edge with sync=1
  - single byte received correctly
  - multi-byte burst received correctly
  - rx_locked stays high across back-to-back bytes
"""

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, ClockCycles, with_timeout
from cocotb.utils import get_sim_time


def start_clock(dut):
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())


async def reset(dut):
    dut.rstn.value        = 0
    dut.cfg_enable.value  = 0
    dut.cfg_lanes.value   = 1
    dut.cfg_clk_div.value = 1
    dut.tx_byte.value     = 0
    dut.tx_wr.value       = 0
    await ClockCycles(dut.clk, 4)
    dut.rstn.value = 1
    await ClockCycles(dut.clk, 2)


async def push_byte(dut, val, timeout_cycles=2000):
    """push one byte, waiting for space in the FIFO first"""
    for _ in range(timeout_cycles):
        if int(dut.tx_full.value) == 0:
            break
        await RisingEdge(dut.clk)
    dut.tx_byte.value = val
    dut.tx_wr.value   = 1
    await RisingEdge(dut.clk)
    dut.tx_wr.value = 0
    await RisingEdge(dut.clk)


async def collect_rx(dut, count, timeout_cycles=2000):
    """collect `count` rx_valid pulses; return list of rx_byte values"""
    received = []
    for _ in range(count):
        for _ in range(timeout_cycles):
            await RisingEdge(dut.clk)
            if int(dut.rx_valid.value) == 1:
                received.append(int(dut.rx_byte.value))
                break
        else:
            break
    return received


async def push_bytes(dut, data):
    """push a list of bytes with FIFO flow control"""
    for val in data:
        await push_byte(dut, val)


# ---------------------------------------------------------------------------
# test 1: rx_locked asserts after first byte
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_rx_locked(dut):
    """rx_locked must go high after the first byte is sent"""
    start_clock(dut)
    await reset(dut)

    assert int(dut.rx_locked.value) == 0, "rx_locked should be 0 before enable"

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 1
    dut.cfg_clk_div.value = 1

    await push_byte(dut, 0xA5)

    # wait up to 200 cycles for rx_locked
    locked = False
    for _ in range(200):
        await RisingEdge(dut.clk)
        if int(dut.rx_locked.value) == 1:
            locked = True
            break

    assert locked, "rx_locked never went high"
    dut._log.info(f"rx_locked asserted at {get_sim_time('ns'):.0f}ns PASS")


# ---------------------------------------------------------------------------
# test 2: single byte received correctly, 1 lane
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_single_byte_1lane(dut):
    """1-lane loopback: transmit 0xA5, receive 0xA5"""
    start_clock(dut)
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 1
    dut.cfg_clk_div.value = 1

    await push_byte(dut, 0xA5)

    received = await collect_rx(dut, 1)
    assert len(received) == 1, f"expected 1 byte, got {len(received)}"
    assert received[0] == 0xA5, f"expected 0xA5 got 0x{received[0]:02X}"
    dut._log.info(f"1-lane rx=0x{received[0]:02X} expected=0xA5 PASS")


# ---------------------------------------------------------------------------
# test 3: single byte received correctly, 3 lanes
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_single_byte_3lane(dut):
    """3-lane loopback: transmit 0xA5, receive 0xA5"""
    start_clock(dut)
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 3
    dut.cfg_clk_div.value = 1

    await push_byte(dut, 0xA5)

    received = await collect_rx(dut, 1)
    assert len(received) == 1, f"expected 1 byte, got {len(received)}"
    assert received[0] == 0xA5, f"expected 0xA5 got 0x{received[0]:02X}"
    dut._log.info(f"3-lane rx=0x{received[0]:02X} expected=0xA5 PASS")


# ---------------------------------------------------------------------------
# test 4: multi-byte burst, 1 lane
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_multibyte_1lane(dut):
    """1-lane loopback: send 0x00..0x3F (64 bytes), receive all correctly"""
    start_clock(dut)
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 1
    dut.cfg_clk_div.value = 1

    num_bytes = 64
    cocotb.start_soon(push_bytes(dut, range(num_bytes)))

    received = await collect_rx(dut, num_bytes, timeout_cycles=500)
    assert len(received) == num_bytes, f"expected {num_bytes} bytes, got {len(received)}"

    errors = [(i, received[i], i) for i in range(num_bytes) if received[i] != i]
    assert not errors, f"byte mismatches: {errors[:8]}"
    dut._log.info(f"1-lane burst: {num_bytes} bytes correct PASS")


# ---------------------------------------------------------------------------
# test 5: multi-byte burst, 3 lanes
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_multibyte_3lane(dut):
    """3-lane loopback: send 0x00..0x3F (64 bytes), receive all correctly"""
    start_clock(dut)
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 3
    dut.cfg_clk_div.value = 1

    num_bytes = 64
    cocotb.start_soon(push_bytes(dut, range(num_bytes)))

    received = await collect_rx(dut, num_bytes, timeout_cycles=500)
    assert len(received) == num_bytes, f"expected {num_bytes} bytes, got {len(received)}"

    errors = [(i, received[i], i) for i in range(num_bytes) if received[i] != i]
    assert not errors, f"byte mismatches: {errors[:8]}"
    dut._log.info(f"3-lane burst: {num_bytes} bytes correct PASS")


# ---------------------------------------------------------------------------
# test 6: rx_sync_lost stays clear across a clean burst
# ---------------------------------------------------------------------------
@cocotb.test()
async def test_no_sync_lost(dut):
    """no rx_sync_lost should fire during a clean back-to-back burst"""
    start_clock(dut)
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 1
    dut.cfg_clk_div.value = 1

    for i in range(8):
        await push_byte(dut, i)

    received = await collect_rx(dut, 8, timeout_cycles=3000)
    assert len(received) == 8, f"expected 8 bytes, got {len(received)}"
    assert int(dut.rx_sync_lost.value) == 0, "rx_sync_lost asserted unexpectedly"
    dut._log.info("no rx_sync_lost across 8-byte burst PASS")
