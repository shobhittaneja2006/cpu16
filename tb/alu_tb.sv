`timescale 1ns/1ps

module alu_tb;
    logic [15:0] a, b;
    logic [3:0] alu_op;
    logic [15:0] result;
    logic [15:0] values [0:11] = '{
        16'h0000, 16'h0001, 16'h0002, 16'h000F,
        16'h0010, 16'h7FFF, 16'h8000, 16'h8001,
        16'hFFFE, 16'hFFFF, 16'h5555, 16'hAAAA
    };
    int checks = 0;

    alu dut (.a(a), .b(b), .alu_op(alu_op), .result(result));

    // Apply inputs, allow combinational logic to settle, then check.
    task automatic check(
        input logic [3:0] operation,
        input logic [15:0] input_a, input_b, expected
    );
        a = input_a;
        b = input_b;
        alu_op = operation;
        #1;
        if (result !== expected)
            $fatal(1, "op=%h a=%h b=%h: expected %h, got %h",
                   alu_op, a, b, expected, result);
        checks++;
    endtask

    initial begin
        // Every pair of boundary/pattern values for binary operations.
        for (int i = 0; i < 12; i++) begin
            for (int j = 0; j < 12; j++) begin
                check(4'h0, values[i], values[j], values[i] + values[j]);
                check(4'h1, values[i], values[j], values[i] - values[j]);
                check(4'h2, values[i], values[j],
                      $signed(values[i]) < $signed(values[j]) ? 16'd1 : 16'd0);
                check(4'h4, values[i], values[j], values[i] & values[j]);
                check(4'h5, values[i], values[j], values[i] | values[j]);
                check(4'h6, values[i], values[j], values[i] ^ values[j]);
                check(4'h7, values[i], values[j], ~values[i]);
            end
        end
        $display("Arithmetic, signed comparison, and bitwise boundary tests passed");

        // Every 16-bit input and every effective shift amount.
        // Upper bits of b are set to ensure only b[3:0] is used.
        for (int value = 0; value < 65536; value++) begin
            check(4'h7, value[15:0], 16'h1234, ~value[15:0]);
            for (int shift = 0; shift < 16; shift++) begin
                // Multiplication/division provide independent shift expectations.
                // Adding the high-bit mask models sign filling for negative SRA.
                check(4'h8, value[15:0], 16'hFFF0 | shift[15:0],
                      value * (1 << shift));
                check(4'h9, value[15:0], 16'hFFF0 | shift[15:0],
                      value / (1 << shift));
                check(4'hA, value[15:0], 16'hFFF0 | shift[15:0],
                      (value / (1 << shift)) |
                      (value >= 32768 ? (16'hFFFF << (16 - shift)) : 0));
            end
        end
        $display("Exhaustive NOT and SLL/SRL/SRA tests passed");

        // All unsupported ALU controls must return zero.
        for (int operation = 0; operation < 16; operation++) begin
            if (operation == 3 || operation >= 11) begin
                for (int i = 0; i < 12; i++)
                    check(operation[3:0], values[i], 16'hFFFF, 16'd0);
            end
        end
        $display("All %0d ALU checks passed", checks);
        $finish;
    end
endmodule
