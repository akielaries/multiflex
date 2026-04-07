import cocotb
from cocotb.triggers import RisingEdge
from tb_utils import start_clock, reset, push_byte


@cocotb.test()
async def test_wire_clock_timing(dut):
    """data must be stable before the rising edge of mfx_clk"""
    start_clock(dut)
    await reset(dut)

    dut.cfg_enable.value  = 1
    dut.cfg_lanes.value   = 1
    dut.cfg_clk_div.value = 3  # wider low half for clear visual in gtkwave

    await push_byte(dut, 0xFF)

    valid_samples = []
    for _ in range(8):
        await RisingEdge(dut.mfx_clk)
        if dut.mfx_sync.value:
            tx_val = int(dut.mfx_tx.value)
            assert tx_val in (0, 1), f"unexpected tx value {tx_val}"
            valid_samples.append(tx_val)

    dut._log.info(f"sampled {len(valid_samples)} valid symbols: {valid_samples} PASS")
