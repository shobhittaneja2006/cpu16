`timescale 1ns/1ps

module regs_tb;

    logic clk;
    logic reset;
    logic [2:0] rA_addr;
    logic [2:0] rB_addr;
    logic [2:0] rC_addr;
    logic [15:0] rA_data;
    logic [15:0] rB_data;
    logic [15:0] rC_data;
    logic write_enabled;
    logic [15:0] write_data;
    logic [2:0] write_addr;



    regs dut (
    .clk(clk),
    .reset(reset),
    .rA_addr(rA_addr),
    .rB_addr(rB_addr),
    .rC_addr(rC_addr),
    .rA_data(rA_data),
    .rB_data(rB_data),
    .rC_data(rC_data),
    .write_enabled(write_enabled),
    .write_data(write_data),
    .write_addr(write_addr)
);

    always #5 clk = ~clk;




endmodule