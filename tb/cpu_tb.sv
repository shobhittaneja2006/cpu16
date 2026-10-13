`timescale 1ns/1ps

module cpu_tb;
    logic clk = 0;
    logic reset = 1;
    logic halted;

    // Set either parameter to 0 to trace the other test on its own.
    parameter RUN_JAL = 1;
    parameter RUN_JALR = 1;

    cpu dut (.clk(clk), .reset(reset), .halted(halted));
    always #5 clk = ~clk;

    task automatic check_jump(
        input logic expected_jalr,
        input logic [15:0] expected_pc, expected_target,
        input logic [2:0] destination,
        input logic [15:0] expected_link
    );
        logic [15:0] saved_regs [0:7];
        if (dut.pc_out !== expected_pc ||
            dut.is_jal !== !expected_jalr || dut.is_jalr !== expected_jalr)
            $fatal(1, "Wrong jump decode at PC %0d", expected_pc);
        if (dut.jump_addr !== expected_target || dut.write_data !== expected_link ||
            dut.rA_addr !== destination || dut.regs_write_enabled !== 1'b1 ||
            dut.mem_write_enabled !== 1'b0 || dut.dmem_write_enabled !== 1'b0 ||
            dut.branch !== 1'b0)
            $fatal(1, "Wrong jump target, link, destination, or controls at PC %0d", expected_pc);
        for (int i = 0; i < 8; i++)
            saved_regs[i] = dut.regs0.registers[i];
        @(posedge clk);
        #1;
        if (dut.pc_out !== expected_target || dut.halted !== 1'b0)
            $fatal(1, "Jump at PC %0d: expected target %0d, got %0d",
                   expected_pc, expected_target, dut.pc_out);
        for (int i = 0; i < 8; i++) begin
            if (i != 0 && i == destination) begin
                if (dut.regs0.registers[i] !== expected_link)
                    $fatal(1, "Wrong link in r%0d", i);
            end
            else if (dut.regs0.registers[i] !== saved_regs[i])
                $fatal(1, "Jump unexpectedly changed r%0d", i);
        end
        if (dut.dmem0.data_memory[3] !== 16'h55AA)
            $fatal(1, "Jump changed memory[3]");
        $display("%0t: JALR=%0b PC %0d -> %0d, link=%0d, destination=r%0d",
                 $time, expected_jalr, expected_pc, expected_target, expected_link, destination);
    endtask

    task automatic check_halt(input logic [15:0] halt_pc);
        @(posedge clk);
        #1;
        if (dut.halted !== 1'b1 || dut.pc_out !== halt_pc)
            $fatal(1, "Expected HALT at PC %0d", halt_pc);
        repeat (2) begin
            @(posedge clk);
            #1;
            if (dut.halted !== 1'b1 || dut.pc_out !== halt_pc ||
                dut.regs_write_enabled !== 1'b0 || dut.mem_write_enabled !== 1'b0)
                $fatal(1, "CPU did not remain halted");
        end
    endtask

    initial begin
        #1; // Wait for memory initialisation.
        dut.dmem0.data_memory[3] = 16'h55AA; // Detect unwanted memory writes.

        if (RUN_JAL) begin
            $display("Starting JAL test: forward, backward, and r0 destination");
            for (int i = 0; i < 512; i++)
                dut.imem0.instruction_memory[i] = 16'h0068;
            dut.imem0.instruction_memory[0] = 16'hF802; // JAL r6, +2 -> 3; link 1
            dut.imem0.instruction_memory[1] = 16'hE003; // JAL r0, +3 -> 5; discard link 2
            dut.imem0.instruction_memory[3] = 16'hEBFD; // JAL r2, -3 -> 1; link 4
            // Addresses 2 and 4 remain HALT: executing either is an error.
            @(posedge clk);
            #1;
            if (dut.pc_out !== 16'd0 || dut.halted !== 1'b0)
                $fatal(1, "JAL test reset failed");
            @(negedge clk);
            reset = 0;
            #1;
            check_jump(1'b0, 16'd0, 16'd3, 3'd6, 16'd1);
            check_jump(1'b0, 16'd3, 16'd1, 3'd2, 16'd4);
            check_jump(1'b0, 16'd1, 16'd5, 3'd0, 16'd2);
            check_halt(16'd5);
            $display("JAL tests passed");
        end

        if (RUN_JALR) begin
            @(negedge clk);
            reset = 1;
            $display("Starting JALR test: call, return, and same-register target/link");
            for (int i = 0; i < 512; i++)
                dut.imem0.instruction_memory[i] = 16'h0068;
            dut.imem0.instruction_memory[0] = 16'h2404; // ADDI r1, r0, 4: target
            dut.imem0.instruction_memory[1] = 16'h18E0; // JALR r6, r1 -> 4; link 2
            dut.imem0.instruction_memory[2] = 16'h2807; // ADDI r2, r0, 7: target
            dut.imem0.instruction_memory[3] = 16'h0960; // JALR r2, r2 -> old r2=7; new r2=4
            dut.imem0.instruction_memory[4] = 16'h0360; // JALR r0, r6 -> 2: return
            @(posedge clk);
            #1;
            if (dut.pc_out !== 16'd0 || dut.halted !== 1'b0)
                $fatal(1, "JALR test reset failed");
            for (int i = 0; i < 8; i++) begin
                if (dut.regs0.registers[i] !== 16'd0)
                    $fatal(1, "Reset did not clear r%0d", i);
            end
            @(negedge clk);
            reset = 0;
            @(posedge clk);
            #1;
            if (dut.pc_out !== 16'd1 || dut.regs0.registers[1] !== 16'd4)
                $fatal(1, "JALR target setup failed");
            check_jump(1'b1, 16'd1, 16'd4, 3'd6, 16'd2);
            check_jump(1'b1, 16'd4, 16'd2, 3'd0, 16'd5);
            @(posedge clk);
            #1;
            if (dut.pc_out !== 16'd3 || dut.regs0.registers[2] !== 16'd7)
                $fatal(1, "Same-register JALR setup failed");
            check_jump(1'b1, 16'd3, 16'd7, 3'd2, 16'd4);
            check_halt(16'd7);
            $display("JALR tests passed");
        end
        $display("All enabled jump tests passed");
        $finish;
    end

    // Previous arithmetic, memory, and branch tests preserved but disabled.
/*
    // Check a branch before its edge, then verify PC and preserved state.
    task automatic check_branch(
        input logic [15:0] expected_pc,
        input logic expected_taken,
        input logic [15:0] expected_next_pc
    );
        logic [15:0] saved_regs [0:7];
        logic [15:0] saved_memory;
        if (dut.pc_out !== expected_pc || dut.branch !== expected_taken)
            $fatal(1, "Branch at PC %0d: wrong PC or taken decision", expected_pc);
        if (dut.regs_write_enabled !== 1'b0 || dut.mem_write_enabled !== 1'b0)
            $fatal(1, "Branch at PC %0d enabled a write", expected_pc);
        if (expected_taken && dut.jump_addr !== expected_next_pc)
            $fatal(1, "Branch at PC %0d calculated the wrong target", expected_pc);
        for (int i = 0; i < 8; i++)
            saved_regs[i] = dut.regs0.registers[i];
        saved_memory = dut.dmem0.data_memory[3];
        @(posedge clk);
        #1;
        if (dut.pc_out !== expected_next_pc || dut.halted !== 1'b0)
            $fatal(1, "Branch at PC %0d: expected next PC %0d, got %0d",
                   expected_pc, expected_next_pc, dut.pc_out);
        for (int i = 0; i < 8; i++) begin
            if (dut.regs0.registers[i] !== saved_regs[i])
                $fatal(1, "Branch changed register %0d", i);
        end
        if (dut.dmem0.data_memory[3] !== saved_memory)
            $fatal(1, "Branch changed memory[3]");
        $display("Branch passed: PC %0d -> %0d, taken = %0b",
                 expected_pc, expected_next_pc, expected_taken);
    endtask

    // Add your initial test sequence here.

    initial begin
    reset = 1;

    #1; // Allow instruction-memory initialisation to finish.
    dut.imem0.instruction_memory[0] = 16'h2405; // ADDI r1, r0, 5
    dut.imem0.instruction_memory[1] = 16'h2807; // ADDI r2, r0, 7
    dut.imem0.instruction_memory[2] = 16'h0C82; // ADD r3, r1, r2
    dut.imem0.instruction_memory[3] = 16'h6C03; // SW r3, r0, 3
    dut.imem0.instruction_memory[4] = 16'h5003; // LW r4, r0, 3
    dut.imem0.instruction_memory[5] = 16'h0068; // HALT


    // Apply synchronous reset.
    @(posedge clk);
    #1;

    if (dut.pc_out !== 16'd0 || dut.halted !== 1'b0)
        $fatal(1, "CPU did not reset");

    @(negedge clk);
    reset = 0;

    // Execute first ADDI.
    @(posedge clk);
    #1;
    if (dut.regs0.registers[1] !== 16'd5)
        $fatal(1, "First ADDI failed");

    // Execute second ADDI.
    @(posedge clk);
    #1;
    if (dut.regs0.registers[2] !== 16'd7)
        $fatal(1, "Second ADDI failed");

    // Execute ADD.
    @(posedge clk);
    #1;
    if (dut.regs0.registers[3] !== 16'd12)
        $fatal(1, "ADD failed: expected 12, got %0d",
               dut.regs0.registers[3]);

    if (dut.pc_out !== 16'd3)
        $fatal(1, "Expected PC = 3 after ADD");

    if (dut.halted !== 1'b0)
        $fatal(1, "CPU halted too early");

    // Execute SW r3, r0, 3.
    @(posedge clk);
    #1;

    if (dut.dmem0.data_memory[3] !== 16'd12)
        $fatal(1, "SW failed: expected memory[3] = 12, got %0d",
            dut.dmem0.data_memory[3]);

    if (dut.regs0.registers[3] !== 16'd12)
        $fatal(1, "SW incorrectly changed r3");

    if (dut.pc_out !== 16'd4)
        $fatal(1, "Expected PC = 4 after SW");

    if (dut.halted !== 1'b0)
        $fatal(1, "CPU halted before executing HALT");

    $display("SW passed: memory[3] = %0d", dut.dmem0.data_memory[3]);

    // LW 
    if (dut.result !== 16'd3 || dut.rA_addr !== 3'd4)
        $fatal(1, "LW selected the wrong address or destination");

    if (dut.read_dmem_data !== 16'd12 || dut.write_data !== 16'd12)
        $fatal(1, "LW read/writeback path did not select memory data");

    if (dut.writeback_from_mem !== 1'b1 ||
        dut.regs_write_enabled !== 1'b1 ||
        dut.dmem_write_enabled !== 1'b0 ||
        dut.mem_write_enabled !== 1'b0)
        $fatal(1, "LW control signals are incorrect");

    if (dut.regs0.registers[4] !== 16'd0)
        $fatal(1, "r4 changed before the LW execution edge");

    // Execute LW r4, r0, 3.
    @(posedge clk);
    #1;

    if (dut.regs0.registers[4] !== 16'd12)
        $fatal(1, "LW failed: expected r4 = 12, got %0d",
               dut.regs0.registers[4]);

    if (dut.dmem0.data_memory[3] !== 16'd12)
        $fatal(1, "LW incorrectly changed memory[3]");

    if (dut.pc_out !== 16'd5 || dut.halted !== 1'b0)
        $fatal(1, "Expected PC = 5 and CPU running after LW");

    $display("LW passed: r4 = %0d", dut.regs0.registers[4]);

    // Execute HALT.
    @(posedge clk);
    #1;
    if (dut.halted !== 1'b1)
        $fatal(1, "HALT did not set halted");

    if (dut.pc_out !== 16'd5)
        $fatal(1, "PC advanced while executing HALT");

    // Keep clocking: PC and registers must hold.
    repeat (3) begin
        @(posedge clk);
        #1;

        if (dut.halted !== 1'b1 || dut.pc_out !== 16'd5)
            $fatal(1, "CPU did not remain halted");

        if (dut.regs0.registers[1] !== 16'd5 ||
            dut.regs0.registers[2] !== 16'd7 ||
            dut.regs0.registers[3] !== 16'd12 ||
            dut.regs0.registers[4] !== 16'd12)
            $fatal(1, "Register values changed after HALT");

        if (dut.dmem0.data_memory[3] !== 16'd12)
            $fatal(1, "Stored value changed after HALT");
    end

    // Reset must clear halted, PC, and registers.
    @(negedge clk);
    reset = 1;

    @(posedge clk);
    #1;
    if (dut.halted !== 1'b0 || dut.pc_out !== 16'd0)
        $fatal(1, "Reset did not clear the halt state");

    for (int i = 0; i < 8; i++) begin
        if (dut.regs0.registers[i] !== 16'd0)
            $fatal(1, "Reset did not clear register %0d", i);
    end

    $display("Arithmetic, SW, LW, HALT, and reset tests passed");

    // Load a separate branch program while reset remains asserted.
    @(negedge clk);
    dut.imem0.instruction_memory[0]  = 16'h2403; // ADDI r1, r0, 3
    dut.imem0.instruction_memory[1]  = 16'h2803; // ADDI r2, r0, 3
    dut.imem0.instruction_memory[2]  = 16'h8501; // BEQ r1, r2, +1 -> 4
    dut.imem0.instruction_memory[3]  = 16'h343F; // ADDI r5, r0, 63: must be skipped
    dut.imem0.instruction_memory[4]  = 16'hA501; // BNE r1, r2, +1: not taken
    dut.imem0.instruction_memory[5]  = 16'h3401; // ADDI r5, r0, 1: must execute
    dut.imem0.instruction_memory[6]  = 16'h8401; // BEQ r1, r0, +1: not taken
    dut.imem0.instruction_memory[7]  = 16'h3801; // ADDI r6, r0, 1: must execute
    dut.imem0.instruction_memory[8]  = 16'hA401; // BNE r1, r0, +1 -> 10
    dut.imem0.instruction_memory[9]  = 16'h3C3F; // ADDI r7, r0, 63: must be skipped
    dut.imem0.instruction_memory[10] = 16'h24FF; // ADDI r1, r1, -1
    dut.imem0.instruction_memory[11] = 16'hA47E; // BNE r1, r0, -2 -> 10
    dut.imem0.instruction_memory[12] = 16'h0068; // HALT
    reset = 0;

    // Initialise the two equal comparison operands.
    repeat (2) begin
        @(posedge clk);
        #1;
    end
    if (dut.regs0.registers[1] !== 16'd3 ||
        dut.regs0.registers[2] !== 16'd3)
        $fatal(1, "Branch program initialisation failed");

    check_branch(16'd2, 1'b1, 16'd4); // BEQ equal: taken
    check_branch(16'd4, 1'b0, 16'd5); // BNE equal: not taken
    @(posedge clk);
    #1; // Execute r5 marker.
    if (dut.regs0.registers[5] !== 16'd1)
        $fatal(1, "BNE not-taken instruction did not execute");

    check_branch(16'd6, 1'b0, 16'd7); // BEQ unequal: not taken
    @(posedge clk);
    #1; // Execute r6 marker.
    if (dut.regs0.registers[6] !== 16'd1)
        $fatal(1, "BEQ not-taken instruction did not execute");
    check_branch(16'd8, 1'b1, 16'd10); // BNE unequal: taken

    // Decrement 3 -> 2 -> 1 -> 0; branch backward twice, then exit.
    for (int count = 2; count >= 0; count--) begin
        @(posedge clk);
        #1;
        if (dut.pc_out !== 16'd11 || dut.regs0.registers[1] !== count)
            $fatal(1, "Countdown failed: expected r1 = %0d", count);
        if (count != 0)
            check_branch(16'd11, 1'b1, 16'd10);
        else
            check_branch(16'd11, 1'b0, 16'd12);
    end

    @(posedge clk);
    #1;
    if (dut.halted !== 1'b1 || dut.pc_out !== 16'd12)
        $fatal(1, "Branch program did not halt at PC 12");
    if (dut.regs0.registers[5] !== 16'd1 ||
        dut.regs0.registers[6] !== 16'd1 ||
        dut.regs0.registers[7] !== 16'd0 ||
        dut.dmem0.data_memory[3] !== 16'd12)
        $fatal(1, "Branch program final state is incorrect");

    repeat (3) begin
        @(posedge clk);
        #1;
        if (dut.halted !== 1'b1 || dut.pc_out !== 16'd12)
            $fatal(1, "Branch program did not remain halted");
    end
    $display("BEQ, BNE, and backward countdown loop tests passed");
    $display("All CPU tests passed");
    $finish;
end


*/
endmodule

