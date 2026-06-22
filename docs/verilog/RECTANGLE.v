module RECTANGLE(
	input  wire         clk,
//Input
	input  wire [ 11:0] ix,
	input  wire [ 11:0] iy,
	input  wire         ivs,
	input  wire         ihs,
	input  wire         ide,
	input  wire [ 23:0] idat,
//Output
	output reg  [ 11:0] ox,
	output reg  [ 11:0] oy,
	output reg          ovs,
	output reg          ohs,
	output reg          ode,
	output reg  [ 23:0] odat,
//Parameters
	input  wire [149:0] iparam
);

	wire        enable;
	wire [23:0] colorb;
	wire [23:0] color;
	wire [11:0] x0;
	wire [11:0] y0;
	wire [11:0] w;
	wire [11:0] h;
	wire [11:0] a;

	wire is_in_box;
	wire is_in_border;

	assign enable = iparam[0];
	assign color  = iparam[24:1];
	assign colorb = iparam[48:25];
	assign x0     = iparam[60:49];
	assign y0     = iparam[72:61];
	assign w      = iparam[84:73];
	assign h      = iparam[96:85];
	assign a      = iparam[108:97];

	assign    is_in_box = ( ix >= x0     ) && ( ix < x0 + w     ) && ( iy >= y0     ) && ( iy < y0 + h     );
	assign is_in_border = ( ix >= x0 - a ) && ( ix < x0 + w + a ) && ( iy >= y0 - a ) && ( iy < y0 + h + a );

	always @( posedge clk )
	begin
		ox  <= ix;
		oy  <= iy;
		ovs <= ivs;
		ohs <= ihs;
		ode <= ide;

		if( enable )
		begin
			if( is_in_box )
				odat <= color;
			else if( is_in_border )
				odat <= colorb;
			else
				odat <= idat;
		end
		else 
				odat <= idat;
	end
endmodule

