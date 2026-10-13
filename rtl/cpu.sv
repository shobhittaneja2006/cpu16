module cpu (
    input logic clk,
    input logic reset,
    output logic halted
);
    //interal logic 
    //pc 
    logic pc_enable, branch, jump;
    logic [15:0] pc_in, pc_out, jump_addr;

    //decoder 
    logic write_enabled, alu_src_imm;
    logic [2:0] rA_addr, rB_addr, rC_addr;
    logic [3:0] alu_op; // for alu asw 
    logic [15:0] alu_imm, instruction;
    logic state_halt;
    logic dmem_write_enabled;
    logic writeback_from_mem;
    logic is_bne;
    logic is_bqe;
    logic is_lui;
    logic is_jal;
    logic is_jalr;


    //alu 
    logic [15:0] a, b, result;

    //imem
    // pc_out for imem_addr
    // instruction for imem_data

    //regs
    logic [15:0] rA_data, rB_data, rC_data, write_data;
    logic [2:0] write_addr;
    // new for HALT
    logic regs_write_enabled;

    //dmem
    logic [15:0] read_dmem_data;
    logic mem_write_enabled;
    
    assign b = alu_src_imm ? alu_imm : rC_data;


    assign write_data = (is_jal || is_jalr) ? pc_out + 16'd1 : //first check for JUMP
                    is_lui ? alu_imm :                         // then check for LUI
                    writeback_from_mem ? read_dmem_data :      // then LW
                    result;

    // checks done before writing, to make sure a reset or HALT is not asserted in the time being
    assign pc_enable = !(halted || state_halt);
    assign regs_write_enabled = !(halted || state_halt) && write_enabled;
    assign mem_write_enabled = dmem_write_enabled && !(halted || state_halt) && !reset;

    //branch calculations
    assign branch = (is_bne && (rA_data != rB_data)) || (is_bqe && (rA_data == rB_data));
    assign jump_addr = is_jalr ? rB_data : pc_out + 16'd1 + alu_imm;
                    // if jalr, then rB else branch and jal take same value
    assign jump = (is_jal || is_jalr) ? 1 : 0;

    always_ff @(posedge clk) begin 
        if(reset) begin 
            halted <= '0;
        end 
        else if(state_halt)begin 
            halted <= '1;
        end
    end  


    pc pc0 (
    .clk(clk),
    .reset(reset),
    .pc_in(16'b0),
    .pc_enable(pc_enable),
    .branch(branch),
    .jump(jump),
    .jump_addr(jump_addr),
    .pc_out(pc_out)
);

imem imem0 (
    .imem_addr(pc_out[8:0]),
    .imem_data(instruction)
);

decoder decoder0 (
    .instruction(instruction),
    .write_enabled(write_enabled),
    .alu_op(alu_op),
    .alu_imm(alu_imm),
    .alu_src_imm(alu_src_imm),
    .rA_addr(rA_addr),
    .rB_addr(rB_addr),
    .rC_addr(rC_addr),
    .state_halt(state_halt),
    .dmem_write_enabled(dmem_write_enabled),
    .writeback_from_mem(writeback_from_mem),
    .is_bne(is_bne),
    .is_bqe(is_bqe),
    .is_lui(is_lui),
    .is_jal(is_jal),
    .is_jalr(is_jalr)
);

regs regs0 (
    .clk(clk),
    .reset(reset),
    .rA_addr(rA_addr),
    .rB_addr(rB_addr),
    .rC_addr(rC_addr),
    .rA_data(rA_data),
    .rB_data(rB_data),
    .rC_data(rC_data),
    .write_enabled(regs_write_enabled),
    .write_data(write_data),
    .write_addr(rA_addr)
);

alu alu0 (
    .a(rB_data),
    .b(b),
    .alu_op(alu_op),
    .result(result)
);

dmem dmem0 (
    .clk(clk),
    .dmem_write_enabled(mem_write_enabled),
    .dmem_addr(result[8:0]),
    .write_dmem_data(rA_data),
    .read_dmem_data(read_dmem_data)
);
endmodule