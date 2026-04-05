import cocotb
from tb_utils import start_clock, reset, push_byte, collect_symbols, symbols_to_bits, bits_to_byte, decode_bytes


@cocotb.test()
async def test_3lane_single_byte(dut):
    """3-lane: transmit 0xA5, verify 3 symbols (last one padded)"""
    start_clock(dut)
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 3
    dut.cfg_clk_div.value = 1

    await push_byte(dut, 0xA5)

    symbols = await collect_symbols(dut, 3)
    assert len(symbols) == 3, f"expected 3 symbols, got {len(symbols)}"

    bits = symbols_to_bits(symbols, 3, count=8)
    got  = bits_to_byte(bits)
    assert got == 0xA5, f"expected 0xA5 got 0x{got:02X}"
    dut._log.info("single byte OK")


@cocotb.test()
async def test_3lane_backtoback(dut):
    """3-lane: push 3 bytes back-to-back, SYNC stays high throughout"""
    start_clock(dut)
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 3
    dut.cfg_clk_div.value = 1

    test_bytes = [0xDE, 0xAD, 0xBE]
    for b in test_bytes:
        await push_byte(dut, b)

    symbols = await collect_symbols(dut, 9, timeout_cycles=1000)
    assert len(symbols) == 9, f"expected 9 symbols got {len(symbols)}"

    for i, (expected, got) in enumerate(zip(test_bytes, decode_bytes(symbols, 3, 3))):
        assert got == expected, f"byte {i}: expected 0x{expected:02X} got 0x{got:02X}"

    dut._log.info("back-to-back OK")
