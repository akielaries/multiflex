import cocotb
from tb_utils import start_clock, reset, push_byte, collect_symbols, symbols_to_bits, bits_to_byte


@cocotb.test()
async def test_2lane_single_byte(dut):
    """2-lane: transmit 0xA5, verify 4 symbols"""
    start_clock(dut)
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 2
    dut.cfg_clk_div.value = 1

    await push_byte(dut, 0xA5)

    symbols = await collect_symbols(dut, 4)
    assert len(symbols) == 4, f"expected 4 symbols, got {len(symbols)}"

    bits = symbols_to_bits(symbols, 2, count=8)
    got  = bits_to_byte(bits)
    assert got == 0xA5, f"expected 0xA5 got 0x{got:02X}"
    dut._log.info(f"symbols={len(symbols)} bits={bits} got=0x{got:02X} expected=0xA5 PASS")
