// Copyright 2015, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

`timescale 1ns / 1ps

import "DPI-C" function void dpi_mem_write(integer addr, integer data);
import "DPI-C" function void dpi_mem_read(integer addr, output integer data);

module simram(
	input clk,
	input [15:0]waddr,
	input [15:0]wdata,
	input we,
	input [15:0]raddr,
	output reg [15:0]rdata,
	input re
	);

	wire [31:0]rawdata;

	always @(posedge clk) begin
		if (we) begin
			dpi_mem_write({16'd0, waddr}, {16'd0, wdata});
		end
		if (re) begin
			dpi_mem_read({16'd0, raddr}, rawdata);
			rdata <= rawdata[15:0];
		end
	end
endmodule
