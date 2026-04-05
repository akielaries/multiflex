import math
import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, ClockCycles


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


async def push_byte(dut, val):
    dut.tx_byte.value = val
    dut.tx_wr.value   = 1
    await RisingEdge(dut.clk)
    dut.tx_wr.value = 0
    await RisingEdge(dut.clk)


async def collect_symbols(dut, num_symbols, timeout_cycles=500):
    """collect num_symbols valid (sync=1) rising edges from mfx_clk"""
    symbols = []
    cycles  = 0
    while len(symbols) < num_symbols and cycles < timeout_cycles:
        await RisingEdge(dut.mfx_clk)
        cycles += 1
        if dut.mfx_sync.value == 1:
            symbols.append(int(dut.mfx_tx.value))
    return symbols


def symbols_to_bits(symbols, lanes, count=None):
    """unpack symbols into a bit list, MSB first, highest lane = MSB"""
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


def decode_bytes(symbols, lanes, num_bytes):
    """decode num_bytes from a flat symbol list, accounting for padding"""
    syms_per_byte = math.ceil(8 / lanes)
    result = []
    for i in range(num_bytes):
        byte_syms = symbols[i * syms_per_byte : (i + 1) * syms_per_byte]
        result.append(bits_to_byte(symbols_to_bits(byte_syms, lanes, count=8)))
    return result


def start_clock(dut):
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())
