module TUBE_GRAPH(
	input  wire         iCLKPIX,
	input  wire         iDE, iHS, iVS,
	input  wire [ 11:0] iX, iY,
	input  wire [ 23:0] iRGB,

	input  wire [  9:0] iNUM,
	input  wire [149:0] iPARAM,

	input  wire [ 15:0] rama,
	input  wire [242:0] ramd,
	input  wire [  9:0] ramid,

	output wire         oDE, oHS, oVS,
	output wire [ 11:0] oX, oY,
	output wire [ 23:0] oRGB
);

	wire         de   [  3:0];
	wire         hs   [  3:0];
	wire         vs   [  3:0];
	wire [ 11:0] x    [  3:0];
	wire [ 11:0] y    [  3:0];
	wire [ 23:0] rgb  [  3:0];

	reg  [149:0] param[139:0];

	always @(posedge iCLKPIX)
		param[iNUM] = iPARAM;

	assign    de[0] = iDE;
	assign    hs[0] = iHS;
	assign    vs[0] = iVS;
	assign     x[0] = iX;
	assign     y[0] = iY;
	assign   rgb[0] = iRGB;

	RECTANGLE 
	sec0000
	(
		.clk   (    iCLKPIX ),
//Input
		.ix    (     x[  0] ),
		.iy    (     y[  0] ),
		.ivs   (    vs[  0] ),
		.ihs   (    hs[  0] ),
		.ide   (    de[  0] ),
		.idat  (   rgb[  0] ),
//Output
		.ox    (     x[  1] ),
		.oy    (     y[  1] ),
		.ovs   (    vs[  1] ),
		.ohs   (    hs[  1] ),
		.ode   (    de[  1] ),
		.odat  (   rgb[  1] ),
//Parameters
		.iparam( param[  0] )
	);

	SYMBOLs 
	#
	(
		.RAMID (          0 )
		.BD    (      97977 )
		.N     (          7 )
	)
	sec0001
	(
		.clk   (    iCLKPIX ),
//Input
		.ix    (     x[  1] ),
		.iy    (     y[  1] ),
		.ivs   (    vs[  1] ),
		.ihs   (    hs[  1] ),
		.ide   (    de[  1] ),
		.idat  (   rgb[  1] ),
//Output
		.ox    (     x[  2] ),
		.oy    (     y[  2] ),
		.ovs   (    vs[  2] ),
		.ohs   (    hs[  2] ),
		.ode   (    de[  2] ),
		.odat  (   rgb[  2] ),
//Parameters
		.iparam ({
		          param[  1],
		          param[  2],
		          param[  3],
		          param[  4],
		          param[  5],
		          param[  6],
		          param[  7]
		        }), 
		.ramid (      ramid ),
		.rama  (       rama ),
		.ramd  (       ramd )
	);


	SYMBOLs 
	#
	(
		.RAMID (          1 )
		.BD    (      72876 )
		.N     (        131 )
	)
	sec0002
	(
		.clk   (    iCLKPIX ),
//Input
		.ix    (     x[  2] ),
		.iy    (     y[  2] ),
		.ivs   (    vs[  2] ),
		.ihs   (    hs[  2] ),
		.ide   (    de[  2] ),
		.idat  (   rgb[  2] ),
//Output
		.ox    (     x[  3] ),
		.oy    (     y[  3] ),
		.ovs   (    vs[  3] ),
		.ohs   (    hs[  3] ),
		.ode   (    de[  3] ),
		.odat  (   rgb[  3] ),
//Parameters
		.iparam ({
		          param[  8],
		          param[  9],
		          param[ 10],
		          param[ 11],
		          param[ 12],
		          param[ 13],
		          param[ 14],
		          param[ 15],
		          param[ 16],
		          param[ 17],
		          param[ 18],
		          param[ 19],
		          param[ 20],
		          param[ 21],
		          param[ 22],
		          param[ 23],
		          param[ 24],
		          param[ 25],
		          param[ 26],
		          param[ 27],
		          param[ 28],
		          param[ 29],
		          param[ 30],
		          param[ 31],
		          param[ 32],
		          param[ 33],
		          param[ 34],
		          param[ 35],
		          param[ 36],
		          param[ 37],
		          param[ 38],
		          param[ 39],
		          param[ 40],
		          param[ 41],
		          param[ 42],
		          param[ 43],
		          param[ 44],
		          param[ 45],
		          param[ 46],
		          param[ 47],
		          param[ 48],
		          param[ 49],
		          param[ 50],
		          param[ 51],
		          param[ 52],
		          param[ 53],
		          param[ 54],
		          param[ 55],
		          param[ 56],
		          param[ 57],
		          param[ 58],
		          param[ 59],
		          param[ 60],
		          param[ 61],
		          param[ 62],
		          param[ 63],
		          param[ 64],
		          param[ 65],
		          param[ 66],
		          param[ 67],
		          param[ 68],
		          param[ 69],
		          param[ 70],
		          param[ 71],
		          param[ 72],
		          param[ 73],
		          param[ 74],
		          param[ 75],
		          param[ 76],
		          param[ 77],
		          param[ 78],
		          param[ 79],
		          param[ 80],
		          param[ 81],
		          param[ 82],
		          param[ 83],
		          param[ 84],
		          param[ 85],
		          param[ 86],
		          param[ 87],
		          param[ 88],
		          param[ 89],
		          param[ 90],
		          param[ 91],
		          param[ 92],
		          param[ 93],
		          param[ 94],
		          param[ 95],
		          param[ 96],
		          param[ 97],
		          param[ 98],
		          param[ 99],
		          param[100],
		          param[101],
		          param[102],
		          param[103],
		          param[104],
		          param[105],
		          param[106],
		          param[107],
		          param[108],
		          param[109],
		          param[110],
		          param[111],
		          param[112],
		          param[113],
		          param[114],
		          param[115],
		          param[116],
		          param[117],
		          param[118],
		          param[119],
		          param[120],
		          param[121],
		          param[122],
		          param[123],
		          param[124],
		          param[125],
		          param[126],
		          param[127],
		          param[128],
		          param[129],
		          param[130],
		          param[131],
		          param[132],
		          param[133],
		          param[134],
		          param[135],
		          param[136],
		          param[137],
		          param[138]
		        }), 
		.ramid (      ramid ),
		.rama  (       rama ),
		.ramd  (       ramd )
	);


	assign
		oDE  =  de[  3],
		oHS  =  hs[  3],
		oVS  =  vs[  3],
		oX   =   x[  3],
		oY   =   y[  3],
		oRGB = rgb[  3];

endmodule


