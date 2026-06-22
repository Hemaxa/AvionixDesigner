module AVIAGORIZONT
(
	input          clk,
//Input
	input  [ 11:0] ix,
	input  [ 11:0] iy,
	input          ivs,
	input          ihs,
	input          ide,
	input  [ 23:0] idat,
//Output
	output [ 11:0] ox,
	output [ 11:0] oy,
	output         ovs,
	output         ohs,
	output         ode,
	output [ 23:0] odat,
//Parameters
	input  [149:0] iparam
);

	// Object Enable
	wire               enable = iparam[  0:  0];
	// Earth Color
	wire        [23:0] earth  = iparam[ 24:  1]; // {B[7:0], G[7:0], R[7:0]}
	// Sky Color
	wire        [23:0] sky    = iparam[ 48: 25]; // {B[7:0], G[7:0], R[7:0]}
	// Horizon Line Color
	wire        [23:0] hline  = iparam[ 72: 49]; // {B[7:0], G[7:0], R[7:0]}
	// Horizon Line Width
	wire        [ 3:0] width  = iparam[ 76: 73]; // width  = Width - 1
	// Horizon Rotation Center
	wire        [11:0] xo     = iparam[ 88: 77];
	wire        [11:0] yo     = iparam[100: 89];
	// Horizon Rotation Sin and Cos
	wire signed [17:0] sn     = iparam[118:101]; // sn = (1 << 16) * sin(roll)
	wire signed [17:0] cs     = iparam[136:119]; // cs = (1 << 16) * cos(roll)

	reg  signed [11:0] xd = 0;
	reg  signed [11:0] yd = 0;

	reg  signed [28:0] yr = 0;

	reg         [23:0] dat = 0;
	wire        [23:0] clr;

	wire signed [20:0] yy = yr[28:8];

	reg         [20:0] trk = 0;
	reg         [20:0] xrt = 0;
	reg         [ 8:0] msk = 0;

	always @(posedge clk)
	begin
		xd <= ix - xo;
		yd <= iy - yo;

		yr <= xd * sn + yd * cs;

		if( yy[20] ) // yy < 0
		begin
			dat <=  sky;
			trk <=  yy;
		end
		else
		begin
			dat <=  earth;
			trk <= ~yy;
		end

		if( !enable )
			dat <= idat;

		xrt[20:7] <= trk[20:7] + width;
		xrt[ 6:0] <= trk[ 6:0];

		msk <= 0;
		if( xrt[20] == 1'b0 )
			msk <= 9'd256;
		else if( ~xrt[20:8] == 13'h0000 )
			msk <= {1'b0, xrt[7:0]};

	end

	pipeline
	#
	(
		.WIDTH( 27 ),
		.DEPTH(  5 )
	)
	pipe5
	(
		.clk( clk                     ),
		.inp( {ide, ivs, ihs, iy, ix} ),
		.out( {ode, ovs, ohs, oy, ox} )
	);

	pipeline
	#
	(
		.WIDTH( 24 ),
		.DEPTH(  2 )
	)
	pipe2
	(
		.clk( clk ),
		.inp( dat ),
		.out( clr )
	);

	interpol_8
	int_8_r
	(
		.clk_i( clk          ),
		.enb_i( enable       ),
		.msk_i( msk          ),
		.clr_i( hline[ 7: 0] ),
		.inp_i( clr  [ 7: 0] ),
		.out_o( odat [ 7: 0] )
	);

	interpol_8
	int_8_g
	(
		.clk_i( clk          ),
		.enb_i( enable       ),
		.msk_i( msk          ),
		.clr_i( hline[15: 8] ),
		.inp_i( clr  [15: 8] ),
		.out_o( odat [15: 8] )
	);

	interpol_8
	int_8_b
	(
		.clk_i( clk          ),
		.enb_i( enable       ),
		.msk_i( msk          ),
		.clr_i( hline[23:16] ),
		.inp_i( clr  [23:16] ),
		.out_o( odat [23:16] )
	);


/*
	pipeline
	#
	(
		.WIDTH( 24 ),
		.DEPTH(  5 )
	)
	pipe5
	(
		.clk( clk  ),
		.inp( idat ),
		.out( inp  )
	);
*/
endmodule

