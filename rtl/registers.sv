module regs (
    input logic clk,
    input logic reset,
    input logic [2:0] rA_addr,
    input logic [2:0] rB_addr,
    input logic [2:0] rC_addr,

    output logic [15:0] rA_data,
    output logic [15:0] rB_data,
    output logic [15:0] rC_data,

    input logic write_enabled,
    input logic [15:0] write_data, // data to write to register 
    input logic [2:0] write_addr // address of register to write the data to 
);

logic [15:0] registers [0:7];

always_ff @(posedge clk) begin
    if(reset) begin 
        for(int i = 0; i <8; i++)begin
            registers[i] <= 0;
    end
    end
    else if (write_enabled && (write_addr != 3'b000))begin 
        registers[write_addr] <= write_data;
    end
end

always_comb begin 

    rA_data = (rA_addr == 3'b000) ? 16'b0 : registers[rA_addr];
    rB_data = (rB_addr == 3'b000) ? 16'b0 : registers[rB_addr];
    rC_data = (rC_addr == 3'b000) ? 16'b0 : registers[rC_addr];

end

endmodule