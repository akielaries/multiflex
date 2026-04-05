import cocotb
from tb_utils import start_clock, reset, push_byte, collect_symbols, symbols_to_bits, bits_to_byte


@cocotb.test()
async def test_1lane_single_byte(dut):
    """1-lane: transmit 0xA5, verify 8 symbols MSB-first"""
    start_clock(dut)
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 1
    dut.cfg_clk_div.value = 1

    await push_byte(dut, 0xA5)

    symbols = await collect_symbols(dut, 8)
    assert len(symbols) == 8, f"expected 8 symbols, got {len(symbols)}"

    bits = symbols_to_bits(symbols, 1)
    got  = bits_to_byte(bits)
    assert got == 0xA5, f"expected 0xA5 got 0x{got:02X}, bits={bits}"
    dut._log.info("OK")
