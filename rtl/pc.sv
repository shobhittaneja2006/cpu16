module pc(
    input logic        clk,
    input logic        reset,
    input logic [15:0] pc_in,
    input logic        pc_enable,
    input logic        branch, //means branch taken, addr for it will be calculated in alu and set to jump_addr 
    input logic        jump,
    //input logic        jalr, ignoring for now 
    input logic [15:0] jump_addr, //address needed for jal or either branch

    output logic  [15:0] pc_out
);

always_ff @(posedge clk) begin
    if(reset) begin
        pc_out <= 0;
    end
    else if(pc_enable) begin 
        if(branch || jump)begin 
            pc_out <= jump_addr;
        end 
        else begin 
        pc_out <= pc_out + 1;
        end
    end 
    else begin 
        pc_out <= pc_out;
        //Halt
    end

end
endmodule