#!/bin/sh
# run from multiflex/hw/
# generates verilog and c header from multiflex_regs.yaml

set -e

cheby --print-memmap --input multiflex_regs.yaml
cheby --hdl verilog --gen-hdl multiflex_regs.v --input multiflex_regs.yaml
cheby --gen-c ../fw/multiflex_regs.h --input multiflex_regs.yaml

echo "done"
