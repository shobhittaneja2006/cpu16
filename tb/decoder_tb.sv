`timescale 1ns/1ps

module decoder_tb;

    logic [15:0] instruction;
    logic write_enabled;
    logic [3:0] alu_op;
    logic [15:0] alu_imm; 
    logic alu_src_imm; // tells wether to use the imm or the ALU input B 
    logic [2:0] rA_addr;
    logic [2:0] rB_addr;
    logic [2:0] rC_addr;

    decoder dut (
    .instruction(instruction),
    .write_enabled(write_enabled),
    .alu_op(alu_op),
    .alu_imm(alu_imm),
    .alu_src_imm(alu_src_imm),
    .rA_addr(rA_addr),
    .rB_addr(rB_addr),
    .rC_addr(rC_addr)
);







endmodule