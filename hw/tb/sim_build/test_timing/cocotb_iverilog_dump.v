module cocotb_iverilog_dump();
initial begin
  $dumpfile("sim_build/test_timing/multiflex_tx.fst");
  $dumpvars(0, multiflex_tx);
end
endmodule
