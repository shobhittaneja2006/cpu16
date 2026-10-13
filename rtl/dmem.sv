module dmem(
    input logic clk,
    input logic dmem_write_enabled,
    input logic [8:0] dmem_addr,
    input logic [15:0] write_dmem_data,
    output logic [15:0] read_dmem_data

);

    logic [15:0] data_memory [0:511];

    initial begin
        for(int i = 0; i < 512; i++) begin 
            data_memory[i] = 0;
            //$readmemh("program.hex", data_memory);
            //for later use 
        end 
    end

    //in always_ff since the write needs to happen on a clock edge
    always @(posedge clk) begin 
        if(dmem_write_enabled)begin 
            data_memory[dmem_addr] <= write_dmem_data;
        end
    end 

    assign read_dmem_data = data_memory[dmem_addr];



endmodule