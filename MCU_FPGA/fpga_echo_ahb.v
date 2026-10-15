`timescale 1ns / 1ps

module fpga_echo_ahb (
    input  wire        clk,
    input  wire        reset,
    input  wire [1:0]  htrans,
    input  wire        hwrite,
    input  wire [30:0] haddr,
    input  wire [2:0]  hsize,
    input  wire [31:0] hwdata,
    output reg  [31:0] hrdata,
    output wire        pass
);

    // MCU address 0x80000000 maps to haddr=0 (the IP omits address bit 31).
    // +0 STATUS: bit0 ready, bit1 done, bit2 pass; +4 OPERAND_A; +8 RESULT;
    // +12 START: write 1 to clear the result; +16 OPERAND_B.
    // Both sides use unsigned 32-bit addition (overflow wraps modulo 2^32).
    localparam [31:0] OPERAND_A = 32'd123;
    localparam [31:0] OPERAND_B = 32'd456;
    wire [31:0] expected_sum = OPERAND_A + OPERAND_B;
    reg        data_valid;
    reg        data_write;
    reg [4:0]  data_addr;
    reg [31:0] result;
    reg        ready;
    reg        done;
    reg        result_pass;

    assign pass = result_pass;

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            data_valid <= 1'b0;
            data_write <= 1'b0;
            data_addr <= 5'd0;
            result <= 32'd0;
            ready <= 1'b0;
            done <= 1'b0;
            result_pass <= 1'b0;
        end else begin
            // HREADY is permanently 1: capture every valid word address phase.
            data_valid <= htrans[1] && (haddr[30:5] == 26'd0)
                          && (haddr[1:0] == 2'b00) && (hsize == 3'b010);
            data_write <= hwrite;
            data_addr <= haddr[4:0];

            // AHB write data belongs to the PREVIOUS cycle's address phase.
            if (data_valid && data_write) begin
                case (data_addr)
                    5'h0C: if (hwdata == 32'd1) begin
                        result <= 32'd0;
                        ready <= 1'b1;
                        done <= 1'b0;
                        result_pass <= 1'b0;
                    end
                    5'h08: if (ready) begin
                        result <= hwdata;
                        ready <= 1'b0;
                        done <= 1'b1;
                        result_pass <= (hwdata == expected_sum);
                    end
                    default: begin end
                endcase
            end
        end
    end

    always @(*) begin
        hrdata = 32'd0;
        if (data_valid && !data_write) begin
            case (data_addr)
                5'h00: hrdata = {29'd0, result_pass, done, ready};
                5'h04: hrdata = OPERAND_A;
                5'h08: hrdata = result;
                5'h10: hrdata = OPERAND_B;
                default: hrdata = 32'd0;
            endcase
        end
    end

endmodule
