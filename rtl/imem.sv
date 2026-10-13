module imem (
    input logic [8:0] imem_addr,
    output logic [15:0] imem_data 
);
    logic [15:0] instruction_memory [0:511];

    initial begin
        for(int i = 0; i < 512; i++) begin 
            instruction_memory[i] = 0;
            //$readmemh("program.hex", instruction_memory);
            //for later use 
        end 
    end

    assign imem_data = instruction_memory[imem_addr];
    
endmodule

