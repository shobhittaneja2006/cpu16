module alu(
    input logic [15:0] a,
    input logic [15:0] b,
    input logic [3:0] alu_op,
    output logic [15:0] result
    
    );

    always_comb begin 
        case (alu_op)

            4'b0000: result = a + b;
            4'b0001: result = a - b;
            4'b0010: result = ($signed(a) < $signed(b)) ? 16'd1 : 16'd0;
            4'b0100: result = a & b;
            4'b0101: result = a | b;
            4'b0110: result = a ^ b;
            4'b0111: result = ~a;
            4'b1000: result = a << b[3:0];
            4'b1001: result = a >> b[3:0];
            4'b1010: result = $signed(a) >>> b[3:0];



            default: result = 0;
        endcase
    end

endmodule

//simple, just add and sub for now 
