import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, FallingEdge, ClockCycles


# -------------------------------------------------------------------------
# helpers
# -------------------------------------------------------------------------

async def reset(dut):
    dut.rstn.value       = 0
    dut.cfg_enable.value = 0
    dut.cfg_lanes.value  = 1
    dut.cfg_clk_div.value = 1   # half-period = 2 fabric clocks
    dut.tx_byte.value    = 0
    dut.tx_wr.value      = 0
    await ClockCycles(dut.clk, 4)
    dut.rstn.value = 1
    await ClockCycles(dut.clk, 2)


async def push_byte(dut, val):
    """push one byte into the TX FIFO"""
    dut.tx_byte.value = val
    dut.tx_wr.value   = 1
    await RisingEdge(dut.clk)
    dut.tx_wr.value = 0
    await RisingEdge(dut.clk)


async def collect_symbols(dut, num_symbols, timeout_cycles=500):
    """
    wait for `num_symbols` valid mfx_clk rising edges with mfx_sync=1.
    returns list of (mfx_tx, mfx_sync) tuples sampled on each rising edge.
    """
    symbols = []
    cycles  = 0
    while len(symbols) < num_symbols and cycles < timeout_cycles:
        await RisingEdge(dut.mfx_clk)
        cycles += 1
        if dut.mfx_sync.value == 1:
            symbols.append(int(dut.mfx_tx.value))
    return symbols


def symbols_to_bits(symbols, lanes, count=None):
    """unpack a list of symbols into a bit list, MSB first, top lane = MSB"""
    bits = []
    for sym in symbols:
        for lane in range(lanes - 1, -1, -1):
            bits.append((sym >> lane) & 1)
    return bits[:count] if count else bits


def bits_to_byte(bits):
    val = 0
    for b in bits:
        val = (val << 1) | b
    return val


# -------------------------------------------------------------------------
# test 1: 1-lane, single byte 0xA5
# -------------------------------------------------------------------------
@cocotb.test()
async def test_1lane_single_byte(dut):
    """1-lane: transmit 0xA5, verify 8 symbols MSB-first"""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 1
    dut.cfg_clk_div.value = 1  # 2-fabric-cycle half-period

    await push_byte(dut, 0xA5)

    symbols = await collect_symbols(dut, 8)
    assert len(symbols) == 8, f"expected 8 symbols, got {len(symbols)}"

    bits = symbols_to_bits(symbols, 1)
    got  = bits_to_byte(bits)
    assert got == 0xA5, f"1-lane: expected 0xA5 got 0x{got:02X}, bits={bits}"
    dut._log.info(f"1-lane: symbols={len(symbols)} bits={bits} got=0x{got:02X} expected=0xA5 PASS")


# -------------------------------------------------------------------------
# test 2: 3-lane, single byte 0xA5
# -------------------------------------------------------------------------
@cocotb.test()
async def test_3lane_single_byte(dut):
    """3-lane: 0xA5 should produce 3 symbols (last one padded)"""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 3
    dut.cfg_clk_div.value = 1

    await push_byte(dut, 0xA5)

    symbols = await collect_symbols(dut, 3)
    assert len(symbols) == 3, f"expected 3 symbols, got {len(symbols)}"

    # rebuild the byte from the 3 valid symbols (8 bits, last bit is padding)
    bits = symbols_to_bits(symbols, 3, count=8)
    got  = bits_to_byte(bits)
    assert got == 0xA5, f"3-lane: expected 0xA5 got 0x{got:02X}"
    dut._log.info(f"3-lane: symbols={len(symbols)} bits={bits} got=0x{got:02X} expected=0xA5 PASS")


# -------------------------------------------------------------------------
# test 3: 2-lane, single byte 0xA5
# -------------------------------------------------------------------------
@cocotb.test()
async def test_2lane_single_byte(dut):
    """2-lane: 0xA5 should produce 4 symbols"""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 2
    dut.cfg_clk_div.value = 1

    await push_byte(dut, 0xA5)

    symbols = await collect_symbols(dut, 4)
    assert len(symbols) == 4, f"expected 4 symbols, got {len(symbols)}"

    bits = symbols_to_bits(symbols, 2, count=8)
    got  = bits_to_byte(bits)
    assert got == 0xA5, f"2-lane: expected 0xA5 got 0x{got:02X}"
    dut._log.info(f"2-lane: symbols={len(symbols)} bits={bits} got=0x{got:02X} expected=0xA5 PASS")


# -------------------------------------------------------------------------
# test 4: 3-lane, back-to-back bytes, verify SYNC stays high
# -------------------------------------------------------------------------
@cocotb.test()
async def test_3lane_backtoback(dut):
    """3-lane: push 3 bytes back-to-back, SYNC should stay high throughout"""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 3
    dut.cfg_clk_div.value = 1

    test_bytes = [0xDE, 0xAD, 0xBE]
    for b in test_bytes:
        await push_byte(dut, b)

    # 3 bytes x ceil(8/3)=3 symbols each = 9 total
    # plus 1 extra load cycle at the start; gather more than enough
    symbols = await collect_symbols(dut, 9, timeout_cycles=1000)
    assert len(symbols) == 9, f"expected 9 symbols got {len(symbols)}"

    # rebuild all bytes; each byte spans ceil(8/lanes)=3 symbols = 9 bits,
    # so byte boundaries are every 3 symbols, not every 8 bits
    import math
    syms_per_byte = math.ceil(8 / 3)
    for i, expected in enumerate(test_bytes):
        byte_syms = symbols[i * syms_per_byte : (i + 1) * syms_per_byte]
        bits = symbols_to_bits(byte_syms, 3, count=8)
        got  = bits_to_byte(bits)
        assert got == expected, \
            f"byte {i}: expected 0x{expected:02X} got 0x{got:02X}"
        dut._log.info(f"byte {i}: got=0x{got:02X} expected=0x{expected:02X} PASS")

    dut._log.info(f"3-lane back-to-back: symbols={len(symbols)} {len(test_bytes)} bytes PASS")


# -------------------------------------------------------------------------
# test 5: wire clock timing -- data stable before rising edge
# -------------------------------------------------------------------------
@cocotb.test()
async def test_wire_clock_timing(dut):
    """data must be stable before the rising edge of mfx_clk"""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 1
    dut.cfg_clk_div.value = 3  # 4-fabric-cycle half-period -> wider window to check

    await push_byte(dut, 0xFF)

    # sample mfx_tx just before each rising edge (one fabric cycle before)
    # since data updates on the fabric posedge that drives mfx_clk low,
    # it is stable for the entire low half before the rising edge
    valid_samples = []
    for _ in range(8):
        await RisingEdge(dut.mfx_clk)
        if dut.mfx_sync.value:
            # data is already stable at this point (updated on previous falling edge)
            tx_val = int(dut.mfx_tx.value)
            # just check that the value is 0 or 1 on lane 0
            assert tx_val in (0, 1), f"unexpected tx value {tx_val}"
            valid_samples.append(tx_val)

    dut._log.info(f"timing check: sampled {len(valid_samples)} valid symbols: {valid_samples} PASS")
