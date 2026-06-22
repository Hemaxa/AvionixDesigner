module RECTANGLEa(
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
    wire [ 2:0] alph;

    wire is_in_box;
    wire is_in_border;
    wire is_in;

    reg [11:0] x;
    reg [11:0] y;
    reg        vs;
    reg        hs;
    reg        de;
    reg [23:0] dat;

    reg        ebuf[3:0];
    reg        hbuf[3:0];
    reg        vbuf[3:0];
    reg [11:0] xbuf[3:0];
    reg [11:0] ybuf[3:0];
    reg [23:0] dbuf[3:0];

    reg [15:0] dat1r;
    reg [15:0] dat1g;
    reg [15:0] dat1b;
    reg [15:0] dat2r;
    reg [15:0] dat2g;
    reg [15:0] dat2b;

    reg [23:0] dat0;

    assign enable = iparam[0];
    assign color  = iparam[24:1];
    assign colorb = iparam[48:25];
    assign alph   = iparam[51:49];
    assign x0     = iparam[63:52];
    assign y0     = iparam[75:64];
    assign w      = iparam[87:76];
    assign h      = iparam[99:88];
    assign a      = iparam[111:100];

    assign    is_in_box = ( ix >= x0     ) && ( ix < x0 + w     ) && ( iy >= y0     ) && ( iy < y0 + h     );
    assign is_in_border = ( ix >= x0 - a ) && ( ix < x0 + w + a ) && ( iy >= y0 - a ) && ( iy < y0 + h + a );

    assign        is_in = ( ix >= xbuf[0]      ) && ( ix < xbuf[0]  + w     ) &&
                          ( iy >= ybuf[0]      ) && ( iy < ybuf[0]  + h     ) ||
                          ( ix >= xbuf[0]  - a ) && ( ix < xbuf[0]  + w + a ) &&
                          ( iy >= ybuf[0]  - a ) && ( iy < ybuf[0]  + h + a );
 
    always @( posedge clk )
    begin
        odat <= dat;
        ode  <= ebuf[3];
        ohs  <= hbuf[3];
        ovs  <= vbuf[3];
        ox   <= xbuf[3];
        oy   <= ybuf[3];

        { ebuf[3], ebuf[2], ebuf[1], ebuf[0] } <= { ebuf[2], ebuf[1], ebuf[0], ide  };
        { hbuf[3], hbuf[2], hbuf[1], hbuf[0] } <= { hbuf[2], hbuf[1], hbuf[0], ihs  };
        { vbuf[3], vbuf[2], vbuf[1], vbuf[0] } <= { vbuf[2], vbuf[1], vbuf[0], ivs  };
        { xbuf[3], xbuf[2], xbuf[1], xbuf[0] } <= { xbuf[2], xbuf[1], xbuf[0], ix   };
        { ybuf[3], ybuf[2], ybuf[1], ybuf[0] } <= { ybuf[2], ybuf[1], ybuf[0], iy   };
        { dbuf[3], dbuf[2], dbuf[1], dbuf[0] } <= { dbuf[2], dbuf[1], dbuf[0], idat };

        if( enable )
        begin
            if( is_in_box )
                dat0 <= color;
            else if( is_in_border )
                dat0 <= colorb;
            else
                dat0 <= idat;
        end
        else 
            dat0 <= idat;

        if( is_in )
        begin
            dat1r[15:0] <= ( alph + 1 ) * dat0[ 7: 0];
            dat1g[15:0] <= ( alph + 1 ) * dat0[15: 8];
            dat1b[15:0] <= ( alph + 1 ) * dat0[23:16];
            dat2r[15:0] <= ( 7 - alph ) * dbuf[0][ 7: 0];
            dat2g[15:0] <= ( 7 - alph ) * dbuf[0][15: 8];
            dat2b[15:0] <= ( 7 - alph ) * dbuf[0][23:16];
        end

        dat[ 7: 0] <= ( dat1r[15:0] + dat2r[15:0] ) / 8;
        dat[15: 8] <= ( dat1g[15:0] + dat2g[15:0] ) / 8;
        dat[23:16] <= ( dat1b[15:0] + dat2b[15:0] ) / 8;
    end
endmodule

