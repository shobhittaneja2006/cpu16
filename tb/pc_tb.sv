`timescale 1ns/1ps

module pc_tb;

    logic clk = 0;
    logic reset;
    logic [15:0] pc_in;
    logic enable;
    logic branch;
    logic jal;
    logic [15:0] jump_addr;
    logic [15:0] pc_out;

    pc dut (
        .clk(clk),
        .reset(reset),
        .pc_in(pc_in),
        .enable(enable),
        .branch(branch),
        .jal(jal),
        .jump_addr(jump_addr),
        .pc_out(pc_out)
    );

    // Clock period: 10 ns.
    always #5 clk = ~clk;

    // Wait for an update, then check after the register has settled.
    task automatic check_pc(
        input logic [15:0] expected,
        input string test_name
    );
        @(posedge clk);
        #1;

        if (pc_out !== expected)
            $fatal(1, "%s: expected %h, got %h",
                   test_name, expected, pc_out);

        $display("PASS: %s | PC=%h", test_name, pc_out);
    endtask

    initial begin
        reset    = 1;
        pc_in    = 16'h0000; // Unused by your current PC implementation.
        enable   = 0;
        branch   = 0;
        jal      = 0;
        jump_addr = 16'h0000;

        check_pc(16'h0000, "Reset while disabled");

        @(negedge clk);
        reset = 0;
        enable = 1;

        check_pc(16'h0001, "First increment");
        check_pc(16'h0002, "Second increment");

        @(negedge clk);
        enable = 0;

        check_pc(16'h0002, "Hold while disabled");
        check_pc(16'h0002, "Hold for another cycle");

        @(negedge clk);
        enable = 1;
        branch = 1;
        jump_addr = 16'h0010;

        // Changing inputs must not immediately change the stored PC.
        #1;
        if (pc_out !== 16'h0002)
            $fatal(1, "PC changed before the rising edge");

        check_pc(16'h0010, "Taken branch");

        @(negedge clk);
        branch = 0;

        check_pc(16'h0011, "Increment after branch");

        @(negedge clk);
        jal = 1;
        jump_addr = 16'h0020;

        check_pc(16'h0020, "JAL target");

        @(negedge clk);
        jal = 0;
        enable = 0;
        branch = 1;
        jump_addr = 16'h0030;

        check_pc(16'h0020, "Disabled PC ignores branch");

        @(negedge clk);
        branch = 0;
        jal = 1;

        check_pc(16'h0020, "Disabled PC ignores JAL");

        // Reset must wait for the rising edge and override other controls.
        @(negedge clk);
        reset = 1;

        #1;
        if (pc_out !== 16'h0020)
            $fatal(1, "Reset acted before the rising edge");

        check_pc(16'h0000, "Reset overrides disabled JAL");

        // Load the maximum address, then check 16-bit wraparound.
        @(negedge clk);
        reset = 0;
        enable = 1;
        jump_addr = 16'hffff;

        check_pc(16'hffff, "Load maximum address");

        @(negedge clk);
        jal = 0;

        check_pc(16'h0000, "Increment wraps to zero");

        $display("All PC tests passed");
        $finish;
    end

endmodule