`timescale 1ns/1ps

module imem_tb; 
    logic [8:0] imem_addr;
    logic [15:0] imem_data;

    imem dut (
        .imem_addr(imem_addr),
        .imem_data(imem_data)
    );

initial begin
    imem_addr = 9'd0;

    #1;
    dut.instruction_memory[0] = 16'hffff;
    dut.instruction_memory[1] = 16'h1234;

    // Read address 0.
    #1;
    if (imem_data !== 16'hffff)
        $fatal(1, "Address 0: expected ffff, got %h", imem_data);

    // Switch to address 1.
    imem_addr = 9'd1;
    #1;
    if (imem_data !== 16'h1234)
        $fatal(1, "Address 1: expected 1234, got %h", imem_data);

    // Switch back to address 0.
    imem_addr = 9'd0;
    #1;
    if (imem_data !== 16'hffff)
        $fatal(1, "Return to address 0: expected ffff, got %h",
               imem_data);

    $display("All instruction-memory tests passed");
    $finish;
end


    
endmodule