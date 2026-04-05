#!/usr/bin/env python3
# gen_dump.py <sim_build_dir> <toplevel>
# writes cocotb_iverilog_dump.v into sim_build_dir
import sys, os

sim_build = sys.argv[1]
toplevel  = sys.argv[2]
fst_path  = os.path.join(sim_build, toplevel + ".fst")

os.makedirs(sim_build, exist_ok=True)

out = os.path.join(sim_build, "cocotb_iverilog_dump.v")
with open(out, "w") as f:
    f.write("module cocotb_iverilog_dump();\n")
    f.write("initial begin\n")
    f.write(f'  $dumpfile("{fst_path}");\n')
    f.write(f"  $dumpvars(0, {toplevel});\n")
    f.write("end\n")
    f.write("endmodule\n")
