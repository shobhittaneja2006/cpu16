`timescale 1ns/1ps

module dmem_tb;

    logic clk = 0;
    logic dmem_write_enabled;
    logic [8:0] dmem_addr;
    logic [15:0] write_dmem_data;
    logic [15:0] read_dmem_data;

    dmem dut (
        .clk(clk),
        .dmem_write_enabled(dmem_write_enabled),
        .dmem_addr(dmem_addr),
        .write_dmem_data(write_dmem_data),
        .read_dmem_data(read_dmem_data)
    );


    always #5 clk = ~clk;

    // initial begin 
    //     dut.data_memory[2] = 16'd1;

    //     #1;
    //     dmem_write_enabled = 1;
    //     dmem_addr = 9'b000000000;
    //     write_dmem_data = 16'd1;
    //     #5

    //     dmem_write_enabled = 0;
    //     dmem_addr = 9'b000000001;
    //     write_dmem_data = 16'd1;

    //     #5;
    //     dmem_write_enabled = 0;
    //     dmem_addr = 9'b000000010;
    //     write_dmem_data = 16'd1;
    //     #10;

    //     $finish;
    // end

    initial begin
    dmem_write_enabled = 1'b0;
    dmem_addr = 9'd0;
    write_dmem_data = 16'd0;

    // Check startup initialisation.
    #1;
    if (read_dmem_data !== 16'd0)
        $fatal(1, "Memory did not initialise to zero");

    // Prepare a write between rising edges.
    @(negedge clk);
    dmem_addr = 9'd0;
    write_dmem_data = 16'd42;
    dmem_write_enabled = 1'b1;

    // Presenting data must not write it immediately.
    #1;
    if (read_dmem_data !== 16'd0)
        $fatal(1, "Memory changed before the rising edge");

    // The rising edge performs the write.
    @(posedge clk);
    #1;
    if (read_dmem_data !== 16'd42)
        $fatal(1, "Write failed: expected 42, got %0d",
               read_dmem_data);

    // Try overwriting address 0 with writing disabled.
    @(negedge clk);
    dmem_write_enabled = 1'b0;
    write_dmem_data = 16'd99;

    @(posedge clk);
    #1;
    if (read_dmem_data !== 16'd42)
        $fatal(1, "Disabled write changed memory");

    // Write a different value to address 2.
    @(negedge clk);
    dmem_addr = 9'd2;
    write_dmem_data = 16'd7;
    dmem_write_enabled = 1'b1;

    @(posedge clk);
    #1;
    if (read_dmem_data !== 16'd7)
        $fatal(1, "Write to address 2 failed");

    // Disable writes and select address 0.
    @(negedge clk);
    dmem_write_enabled = 1'b0;
    dmem_addr = 9'd0;

    #1;
    if (read_dmem_data !== 16'd42)
        $fatal(1, "Address 0 did not retain 42");

    // Select address 2 without waiting for a clock.
    dmem_addr = 9'd2;
    #1;
    if (read_dmem_data !== 16'd7)
        $fatal(1, "Address 2 did not retain 7");

    // An untouched entry should still be zero.
    dmem_addr = 9'd1;
    #1;
    if (read_dmem_data !== 16'd0)
        $fatal(1, "Untouched address 1 was not zero");

    $display("All data-memory tests passed");
    $finish;
end



endmodule