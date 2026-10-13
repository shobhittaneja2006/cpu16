module decoder (
    input logic [15:0] instruction,

    output logic write_enabled, //for regs

    output logic [3:0] alu_op,
    output logic [15:0] alu_imm, 
    output logic alu_src_imm, // tells wether to use the imm or the ALU input B 

    output logic [2:0] rA_addr,
    output logic [2:0] rB_addr,
    output logic [2:0] rC_addr,

    output logic state_halt,

    output logic dmem_write_enabled,
    output logic writeback_from_mem,

    output logic is_bne,
    output logic is_bqe,
    output logic is_lui,
    output logic is_jal,
    output logic is_jalr
);

    logic [2:0] opcode;

    assign opcode = instruction[15:13];
    assign rA_addr = instruction[12:10];
    assign rB_addr = instruction[9:7];
    assign rC_addr = instruction[2:0];

    


    always_comb begin 

        write_enabled = 1'b0;
        alu_src_imm   = 1'b0;
        alu_op        = 4'b0000;
        alu_imm       = 16'b0;
        state_halt    = '0;
        dmem_write_enabled = '0;
        writeback_from_mem = '0;
        is_bne = '0;
        is_bqe = '0;
        is_lui = '0;
        is_jal = 0;
        is_jalr = 0;
        


        case (opcode) 
            3'b000: begin 
                alu_op = instruction[6:3];
                case (instruction[6:3])
                    4'b0000: begin 
                        write_enabled = 1'b1;
                        alu_op = 4'b0000;
                    end
                    4'b0001: begin 
                        write_enabled = 1'b1;
                        alu_op = 4'b0001;
                    end 
                    4'b0010: begin 
                        write_enabled = 1'b1;
                        alu_op = 4'b0010;
                    end
                    4'b0100: begin 
                        write_enabled = 1'b1;
                        alu_op = 4'b0100;
                    end
                    4'b0101: begin 
                        write_enabled = 1'b1;
                        alu_op = 4'b0101;
                    end
                    4'b0110: begin 
                        write_enabled = 1'b1;
                        alu_op = 4'b0110;
                    end
                    4'b0111: begin 
                        write_enabled = 1'b1;
                        alu_op = 4'b0111;
                    end
                    4'b1000: begin 
                        write_enabled = 1'b1;
                        alu_op = 4'b1000;
                    end
                    4'b1001: begin 
                        write_enabled = 1'b1;
                        alu_op = 4'b1001;
                    end
                    4'b1010: begin 
                        write_enabled = 1'b1;
                        alu_op = 4'b1010;
                    end
                    4'b1100: begin 
                        is_jalr = 1;
                        write_enabled = 1;
                    end
                    4'b1101: begin 
                        state_halt = '1;
                        write_enabled = '0;
                    end

                endcase
                
            end 
            3'b001:begin 
                write_enabled = 1'b1;
                alu_src_imm = 1;
                alu_imm = {{9{instruction[6]}},{instruction[6:0]}};
                alu_op = 4'b0000;
            end

            3'b010: begin //LW
                alu_op = 4'b0000;
                alu_src_imm = 1;
                alu_imm = {{9{instruction[6]}},{instruction[6:0]}};
                write_enabled = 1;
                writeback_from_mem = 1;

            end

            3'b011: begin //SW
                alu_op = 4'b0000;
                alu_src_imm = 1;
                alu_imm = {{9{instruction[6]}},{instruction[6:0]}};
                dmem_write_enabled = 1;
                write_enabled = 0;


            end

            3'b100: begin //BEQ
                is_bqe = '1;
                alu_imm = {{9{instruction[6]}},{instruction[6:0]}};


            end

            3'b101: begin //BNE
            is_bne = '1;
            alu_imm = {{9{instruction[6]}},{instruction[6:0]}};

            end

            3'b110: begin //LUI
                write_enabled = 1'b1;
                is_lui = '1;
                alu_imm = {{instruction[9:0]}, 6'b0};


            end 

            3'b111: begin //JAL
                is_jal = '1;
                write_enabled = '1;
                alu_imm = {{6{instruction[9]}}, {instruction[9:0]}};
                

            end




        endcase
    end 
    
endmodule