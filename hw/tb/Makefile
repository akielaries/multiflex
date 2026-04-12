SIM            ?= icarus
TOPLEVEL        = multiflex_tx
TOPLEVEL_LANG   = verilog
VERILOG_SOURCES = $(PWD)/../multiflex_tx.v
COMPILE_ARGS    = -P multiflex_tx.NUM_LANES=3

COCOTB_MAKEFILES = $(shell cocotb-config --makefiles)/Makefile.sim

LANE_TESTS     = test_1lane test_2lane test_3lane
OTHER_TESTS    = test_timing
LOOPBACK_TESTS = test_loopback
DPSSRAM_TESTS  = test_dpssram
MULTIFLEX_TESTS = test_multiflex
TESTS       = $(LANE_TESTS) $(OTHER_TESTS) $(LOOPBACK_TESTS) $(DPSSRAM_TESTS) $(MULTIFLEX_TESTS)

.PHONY: all $(TESTS) clean

all: $(TESTS)

# cocotb's Makefile.icarus uses += to append to VERILOG_SOURCES and COMPILE_ARGS
# when WAVES=1 (adding the dump module and -s cocotb_iverilog_dump).  Those +=
# assignments are defeated if the variables are command-line overrides, and
# cause double-appending if they come from the environment.  Avoid both problems
# by pre-generating the dump module and passing WAVES=0 so the += blocks never
# run; everything is explicit in the command-line values below.
$(LANE_TESTS):
	python3 $(PWD)/gen_dump.py sim_build/$@ $(TOPLEVEL)
	$(MAKE) -f $(COCOTB_MAKEFILES) \
		SIM=$(SIM) \
		TOPLEVEL=$(TOPLEVEL) \
		TOPLEVEL_LANG=$(TOPLEVEL_LANG) \
		VERILOG_SOURCES="$(VERILOG_SOURCES) sim_build/$@/cocotb_iverilog_dump.v" \
		COMPILE_ARGS="$(COMPILE_ARGS) -s cocotb_iverilog_dump" \
		FST="-fst" \
		WAVES=0 \
		SIM_BUILD=sim_build/$@ \
		PYTHONPATH=$(PWD) \
		COCOTB_TEST_MODULES=lane_tests.$@

$(OTHER_TESTS):
	python3 $(PWD)/gen_dump.py sim_build/$@ $(TOPLEVEL)
	$(MAKE) -f $(COCOTB_MAKEFILES) \
		SIM=$(SIM) \
		TOPLEVEL=$(TOPLEVEL) \
		TOPLEVEL_LANG=$(TOPLEVEL_LANG) \
		VERILOG_SOURCES="$(VERILOG_SOURCES) sim_build/$@/cocotb_iverilog_dump.v" \
		COMPILE_ARGS="$(COMPILE_ARGS) -s cocotb_iverilog_dump" \
		FST="-fst" \
		WAVES=0 \
		SIM_BUILD=sim_build/$@ \
		COCOTB_TEST_MODULES=$@

LOOPBACK_SOURCES = $(PWD)/../multiflex_tx.v $(PWD)/../multiflex_rx.v $(PWD)/tb_loopback.v
DPSSRAM_SOURCES  = $(PWD)/../cheby_dpssram.v $(PWD)/sdpb_model.v

$(LOOPBACK_TESTS):
	python3 $(PWD)/gen_dump.py sim_build/$@ multiflex_loopback
	$(MAKE) -f $(COCOTB_MAKEFILES) \
		SIM=$(SIM) \
		TOPLEVEL=multiflex_loopback \
		TOPLEVEL_LANG=verilog \
		VERILOG_SOURCES="$(LOOPBACK_SOURCES) sim_build/$@/cocotb_iverilog_dump.v" \
		COMPILE_ARGS="-P multiflex_loopback.NUM_LANES=3 -s cocotb_iverilog_dump" \
		FST="-fst" \
		WAVES=0 \
		SIM_BUILD=sim_build/$@ \
		PYTHONPATH=$(PWD) \
		COCOTB_TEST_MODULES=$@

$(DPSSRAM_TESTS):
	python3 $(PWD)/gen_dump.py sim_build/$@ cheby_dpssram
	$(MAKE) -f $(COCOTB_MAKEFILES) \
		SIM=$(SIM) \
		TOPLEVEL=cheby_dpssram \
		TOPLEVEL_LANG=verilog \
		VERILOG_SOURCES="$(DPSSRAM_SOURCES) sim_build/$@/cocotb_iverilog_dump.v" \
		COMPILE_ARGS="-s cocotb_iverilog_dump" \
		FST="-fst" \
		WAVES=0 \
		SIM_BUILD=sim_build/$@ \
		PYTHONPATH=$(PWD) \
		COCOTB_TEST_MODULES=$@

MULTIFLEX_SOURCES = $(PWD)/../multiflex.v \
                    $(PWD)/../multiflex_tx.v \
                    $(PWD)/../multiflex_rx.v \
                    $(PWD)/../multiflex_regs.v \
                    $(PWD)/../cheby_dpssram.v \
                    $(PWD)/sdpb_model.v \
                    $(PWD)/tb_multiflex.v

$(MULTIFLEX_TESTS):
	python3 $(PWD)/gen_dump.py sim_build/$@ tb_multiflex
	$(MAKE) -f $(COCOTB_MAKEFILES) \
		SIM=$(SIM) \
		TOPLEVEL=tb_multiflex \
		TOPLEVEL_LANG=verilog \
		VERILOG_SOURCES="$(MULTIFLEX_SOURCES) sim_build/$@/cocotb_iverilog_dump.v" \
		COMPILE_ARGS="-P tb_multiflex.NUM_LANES=3 -s cocotb_iverilog_dump" \
		FST="-fst" \
		WAVES=0 \
		SIM_BUILD=sim_build/$@ \
		PYTHONPATH=$(PWD) \
		COCOTB_TEST_MODULES=$@

clean:
	rm -rf sim_build __pycache__ results.xml
