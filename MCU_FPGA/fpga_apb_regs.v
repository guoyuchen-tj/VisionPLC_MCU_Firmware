`timescale 1ns / 1ps

module fpga_apb_regs (
    input wire clk,
    input wire reset,
    input wire [19:0] paddr,
    input wire psel,
    input wire penable,
    input wire pwrite,
    input wire [31:0] pwdata,
    input wire [3:0] pstrb,
    output reg [31:0] prdata,
    output wire pready,
    output wire pslverr,
    output wire irq
);
    reg [31:0] scratch;
    reg irq_enable;
    reg irq_pending;
    reg valid_addr;
    integer lane;

    wire access = psel && penable;
    wire read_only = (paddr == 20'h00000 || paddr == 20'h00004);
    assign pready = 1'b1;
    assign pslverr = access && (!valid_addr || (pwrite && read_only));
    assign irq = irq_enable && irq_pending;

    always @(*) begin
        valid_addr = 1'b1;
        prdata = 32'd0;
        case (paddr)
            20'h00000: prdata = 32'h56504C43;
            20'h00004: prdata = 32'h00010000;
            20'h00008: prdata = scratch;
            20'h0000C: prdata = {31'd0, irq_enable};
            20'h00010: prdata = {31'd0, irq_pending};
            20'h00014: prdata = {31'd0, irq};
            default: valid_addr = 1'b0;
        endcase
    end

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            scratch <= 32'd0;
            irq_enable <= 1'b0;
            irq_pending <= 1'b0;
        end else if (access && pwrite && !pslverr) begin
            case (paddr)
                20'h00008: for (lane = 0; lane < 4; lane = lane + 1)
                    if (pstrb[lane])
                        scratch[lane*8 +: 8] <= pwdata[lane*8 +: 8];
                20'h0000C: if (pstrb[0]) irq_enable <= pwdata[0];
                20'h00010: if (pstrb[0] && pwdata[0]) irq_pending <= 1'b0;
                20'h00014: if (pstrb[0] && pwdata[0]) irq_pending <= 1'b1;
                default: begin end
            endcase
        end
    end
endmodule
