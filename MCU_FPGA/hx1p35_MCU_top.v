`timescale 1ns / 1ps

module hx1p35_MCU_top (
    input  wire clk_50m,
    input  wire uart0_rx,
    output wire uart0_tx,
    output wire [7:0] O_DIG,
    output wire [7:0] O_SEL
);

    wire core_clk;
    wire timer_clk;
    wire pll_locked;
    wire mcu_reset;
    wire apb_reset;
    wire ahb_clk;
    wire [1:0] htrans;
    wire hwrite;
    wire [30:0] haddr;
    wire [2:0] hsize;
    wire [31:0] hwdata;
    wire [31:0] hrdata;
    wire echo_pass;
    wire [31:0] echo_hrdata;
    wire [31:0] bridge_hrdata;
    wire apb_test_irq;
    reg [1:0] irq_sync = 2'b00;

    reg [15:0] reset_count = 16'd0;
    reg [2:0] apb_reset_sync = 3'b111;
    reg [2:0] ahb_reset_sync = 3'b111;
    reg [1:0] pass_sync = 2'b00;
    reg [15:0] scan_count = 16'd0;
    reg [2:0] scan_digit = 3'd0;

    MCU_PLL u_mcu_pll (
        .refclk   (clk_50m),
        .clk0_out (core_clk),     // 300 MHz
        .clk1_out (timer_clk),    // 100 MHz
        .lock     (pll_locked),
        .reset    (1'b0)
    );

    // Assert immediately on loss of lock; release after about 218 us at 300 MHz.
    always @(posedge core_clk or negedge pll_locked) begin
        if (!pll_locked)
            reset_count <= 16'd0;
        else if (!(&reset_count))
            reset_count <= reset_count + 16'd1;
    end

    assign mcu_reset = !pll_locked || !(&reset_count);

    // Synchronize APB reset release to its 50 MHz clock.
    always @(posedge clk_50m or posedge mcu_reset) begin
        if (mcu_reset)
            apb_reset_sync <= 3'b111;
        else
            apb_reset_sync <= {apb_reset_sync[1:0], 1'b0};
    end

    assign apb_reset = apb_reset_sync[2];

    always @(posedge ahb_clk or posedge mcu_reset) begin
        if (mcu_reset)
            ahb_reset_sync <= 3'b111;
        else
            ahb_reset_sync <= {ahb_reset_sync[1:0], 1'b0};
    end

    fpga_echo_ahb u_fpga_echo (
        .clk    (ahb_clk),
        .reset  (ahb_reset_sync[2]),
        .htrans (htrans),
        .hwrite (hwrite),
        .haddr  (haddr),
        .hsize  (hsize),
        .hwdata (hwdata),
        .hrdata (echo_hrdata),
        .pass   (echo_pass)
    );

    fpga_apb_bridge u_apb_test (
        .ahb_clk(ahb_clk), .ahb_reset(ahb_reset_sync[2]),
        .htrans(htrans), .hwrite(hwrite), .haddr(haddr), .hsize(hsize),
        .hwdata(hwdata), .hrdata(bridge_hrdata),
        .apb_clk(clk_50m), .apb_reset(apb_reset), .irq(apb_test_irq)
    );
    assign hrdata = echo_hrdata | bridge_hrdata;

    // A level request remains asserted until software clears the APB source.
    always @(posedge core_clk or posedge mcu_reset) begin
        if (mcu_reset)
            irq_sync <= 2'b00;
        else
            irq_sync <= {irq_sync[0], apb_test_irq};
    end

    always @(posedge clk_50m or posedge apb_reset) begin
        if (apb_reset)
            pass_sync <= 2'b00;
        else
            pass_sync <= {pass_sync[0], echo_pass};
    end

    // 50 MHz / 50000 / 8 = 125 Hz full-display refresh.
    always @(posedge clk_50m or posedge apb_reset) begin
        if (apb_reset) begin
            scan_count <= 16'd0;
            scan_digit <= 3'd0;
        end else if (scan_count == 16'd49999) begin
            scan_count <= 16'd0;
            scan_digit <= scan_digit + 3'd1;
        end else begin
            scan_count <= scan_count + 16'd1;
        end
    end

    // Active-low {DP,g,f,e,d,c,b,a}: 0=C0, 2=A4; DP stays off.
    // Blank one cycle between digits. PASS shows only a rightmost 2; otherwise all zeros.
    assign O_DIG = pass_sync[1] ? 8'hA4 : 8'hC0;
    assign O_SEL = (apb_reset || scan_count == 16'd0
                    || (pass_sync[1] && scan_digit != 3'd7))
                   ? 8'hFF : ~(8'b00000001 << scan_digit);

    RISCV_F6F1D1P0_0 u_riscv_mcu (
        .core_sysrst (),
        // Board USB-JTAG uses the native PAD path: generate the IP with JTAG unchecked.
        .uart0_tx    (uart0_tx),
        .uart0_rx    (uart0_rx),

        // Fabric echo registers: zero-wait-state AHB slave.
        .ahb_clk     (ahb_clk),
        .htrans      (htrans),
        .hwrite      (hwrite),
        .haddr       (haddr),
        .hsize       (hsize),
        .hburst      (),
        .hprot       (),
        .hmastlock   (),
        .hwdata      (hwdata),
        .hrdata      (hrdata),
        .hresp       (2'b00),
        .hready      (1'b1),

        // No Fabric APB slave yet: complete accesses without an error.
        .apb_clk     (clk_50m),
        .apb_rst     (apb_reset),
        .paddr       (),
        .psel        (),
        .penable     (),
        .pwrite      (),
        .pwdata      (),
        .pstrobe     (),
        .pprot       (),
        .prdata      (32'd0),
        .pready      (1'b1),
        .pslverr     (1'b0),

        .core_clk    (core_clk),
        .timer_clk   (timer_clk),
        .core_reset  (mcu_reset),
        .por_reset   (mcu_reset),
        .nmi         (1'b0),
        .clic_irq    ({22'd0, irq_sync[1]})
    );

endmodule
