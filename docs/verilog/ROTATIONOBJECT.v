module ROTATIONOBJECT #
(
    parameter           RAMID = 0
)
(
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
    input  wire [  9:0] ramid,
    input  wire [ 15:0] rama,
    input  wire [242:0] ramd,
    input  wire [149:0] iparam
);
    parameter         BD = 2047;

    reg               wea;
    reg signed [14:0] left;
    reg signed [14:0] top;
    reg signed [14:0] right;
    reg signed [14:0] bot;
    reg signed [11:0] shiftx;
    reg signed [11:0] shifty;
    reg        [11:0] xpoint;
    reg        [11:0] ypoint;
    reg        [ 7:0] sqx;
    reg        [23:0] color;
    reg               enb;

    reg         ebuf[11:0];
    reg         hbuf[11:0];
    reg         vbuf[11:0];
    reg [ 11:0] xbuf[11:0]; 
    reg [ 11:0] ybuf[11:0]; 
    reg [ 23:0] dbuf[11:0];

    reg [ 23:0] dat;

    reg [242:0] bram[BD:0];
    reg [ 15:0] ram_addro;
    reg [242:0] ram_datao;

    reg signed [15:0] x;
    reg signed [15:0] y;
    reg signed [15:0] xxx;
    reg signed [15:0] yyy;
    reg signed [15:0] xxxbuf[7:0];
    reg signed [15:0] yyybuf[7:0];
    reg signed [17:0] sinbuf;
    reg signed [17:0] cosbuf;
    reg signed [17:0] sin;
    reg signed [17:0] cos;
    reg signed [29:0] xx;
    reg signed [29:0] yy;

    wire [242:0] doutb;

    reg [ 2:0] addraxbuf[4:0];
    reg [ 2:0] addraybuf[4:0];
    reg [ 2:0] tA;
    reg [ 2:0] tB;
    reg [ 2:0] tC;
    reg [ 2:0] tD;
    reg [15:0] addrax;
    reg [15:0] addray;
    reg [15:0] ktA;
    reg [15:0] ktB;
    reg [15:0] ktC;
    reg [13:0] addra;
    reg [26:0] datash[8:0];

    assign doutb = ram_datao;

    (* RAM_STYLE="{AUTO | BLOCK |  BLOCK_POWER1 | BLOCK_POWER2}" *) 
    always @( posedge clk )
    begin
        if( wea ) bram[rama] <= ramd;
    end
    always @( posedge clk )
    begin
        ram_datao <= bram[ram_addro];
    end

    always @( posedge clk )
    begin
        odat <= dat;
        ode  <= ebuf[11];
        ohs  <= hbuf[11];
        ovs  <= vbuf[11];
        ox   <= xbuf[11];
        oy   <= ybuf[11];

        { ebuf[11], ebuf[10], ebuf[ 9], ebuf[ 8], ebuf[ 7], ebuf[ 6],
          ebuf[ 5], ebuf[ 4], ebuf[ 3], ebuf[ 2], ebuf[ 1], ebuf[ 0] }
        <= 
        { ebuf[10], ebuf[ 9], ebuf[ 8], ebuf[ 7], ebuf[ 6], ebuf[ 5],
          ebuf[ 4], ebuf[ 3], ebuf[ 2], ebuf[ 1], ebuf[ 0], ide      };

        { hbuf[11], hbuf[10], hbuf[ 9], hbuf[ 8], hbuf[ 7], hbuf[ 6],
          hbuf[ 5], hbuf[ 4], hbuf[ 3], hbuf[ 2], hbuf[ 1], hbuf[ 0] }
        <= 
        { hbuf[10], hbuf[ 9], hbuf[ 8], hbuf[ 7], hbuf[ 6], hbuf[ 5],
          hbuf[ 4], hbuf[ 3], hbuf[ 2], hbuf[ 1], hbuf[ 0], ihs      };

        { vbuf[11], vbuf[10], vbuf[ 9], vbuf[ 8], vbuf[ 7], vbuf[ 6],
          vbuf[ 5], vbuf[ 4], vbuf[ 3], vbuf[ 2], vbuf[ 1], vbuf[ 0] }
        <= 
        { vbuf[10], vbuf[ 9], vbuf[ 8], vbuf[ 7], vbuf[ 6], vbuf[ 5],
          vbuf[ 4], vbuf[ 3], vbuf[ 2], vbuf[ 1], vbuf[ 0], ivs      };

        { xbuf[11], xbuf[10], xbuf[ 9], xbuf[ 8], xbuf[ 7], xbuf[ 6],
          xbuf[ 5], xbuf[ 4], xbuf[ 3], xbuf[ 2], xbuf[ 1], xbuf[ 0] }
        <= 
        { xbuf[10], xbuf[ 9], xbuf[ 8], xbuf[ 7], xbuf[ 6], xbuf[ 5],
          xbuf[ 4], xbuf[ 3], xbuf[ 2], xbuf[ 1], xbuf[ 0], ix       };

        { ybuf[11], ybuf[10], ybuf[ 9], ybuf[ 8], ybuf[ 7], ybuf[ 6],
          ybuf[ 5], ybuf[ 4], ybuf[ 3], ybuf[ 2], ybuf[ 1], ybuf[ 0] }
        <= 
        { ybuf[10], ybuf[ 9], ybuf[ 8], ybuf[ 7], ybuf[ 6], ybuf[ 5],
          ybuf[ 4], ybuf[ 3], ybuf[ 2], ybuf[ 1], ybuf[ 0], iy       };

        { dbuf[11], dbuf[10], dbuf[ 9], dbuf[ 8], dbuf[ 7], dbuf[ 6],
          dbuf[ 5], dbuf[ 4], dbuf[ 3], dbuf[ 2], dbuf[ 1], dbuf[ 0] }
        <= 
        { dbuf[10], dbuf[ 9], dbuf[ 8], dbuf[ 7], dbuf[ 6], dbuf[ 5],
          dbuf[ 4], dbuf[ 3], dbuf[ 2], dbuf[ 1], dbuf[ 0], idat     };

        if( ramid == RAMID ) wea <= 1'b1; else wea <= 1'b0;

        if( ivs ) 
        begin
            enb    <= iparam[0];

            color  <= iparam[24:1];

            xpoint <= iparam[36:25];
            ypoint <= iparam[48:37];

            left   <= { iparam[60:49], 3'b000 };
            top    <= { iparam[72:61], 3'b000 };
            right  <= { iparam[84:73], 3'b000 };
            bot    <= { iparam[96:85], 3'b000 };

            sqx    <= iparam[104:97];

            shiftx <= iparam[60:49];
            shifty <= iparam[72:61];


            sinbuf <= iparam[122:105]; 
            cosbuf <= iparam[140:123];
            sin    <= sinbuf >>> 8;
            cos    <= cosbuf >>> 8;
        end

        x   <= ix - xpoint;
        y   <= iy - ypoint;
        xx  <= -x * sin + y * cos;
        yy  <=  x * cos + y * sin;
        xxx <= xx >>> 5;
        yyy <= yy >>> 5;

        xxxbuf[0][15:0] <= xxx[15:0];
        yyybuf[0][15:0] <= yyy[15:0];
        xxxbuf[1][15:0] <= xxxbuf[0][15:0];
        yyybuf[1][15:0] <= yyybuf[0][15:0];
        xxxbuf[2][15:0] <= xxxbuf[1][15:0];
        yyybuf[2][15:0] <= yyybuf[1][15:0];
        xxxbuf[3][15:0] <= xxxbuf[2][15:0];
        yyybuf[3][15:0] <= yyybuf[2][15:0];
        xxxbuf[4][15:0] <= xxxbuf[3][15:0];
        yyybuf[4][15:0] <= yyybuf[3][15:0];
        xxxbuf[5][15:0] <= xxxbuf[4][15:0];
        yyybuf[5][15:0] <= yyybuf[4][15:0];
        xxxbuf[6][15:0] <= xxxbuf[5][15:0];
        yyybuf[6][15:0] <= yyybuf[5][15:0];
        xxxbuf[7][15:0] <= xxxbuf[6][15:0];
        yyybuf[7][15:0] <= yyybuf[6][15:0];

        addrax <= xxx[15:3] - {shiftx[11],shiftx[11:0]};
        addray <= yyy[15:3] - {shifty[11],shifty[11:0]};

        addraxbuf[0][2:0] <= addrax[2:0];
        addraybuf[0][2:0] <= addray[2:0];
        addraxbuf[1][2:0] <= addraxbuf[0][2:0];
        addraybuf[1][2:0] <= addraybuf[0][2:0];
        addraxbuf[2][2:0] <= addraxbuf[1][2:0];
        addraybuf[2][2:0] <= addraybuf[1][2:0];
        addraxbuf[3][2:0] <= addraxbuf[2][2:0];
        addraybuf[3][2:0] <= addraybuf[2][2:0];
        addraxbuf[4][2:0] <= addraxbuf[3][2:0];
        addraybuf[4][2:0] <= addraybuf[3][2:0];

        ram_addro <= addrax[9:3] * sqx + addray[9:3];

        datash[0] <= doutb[ 26:  0];
        datash[1] <= doutb[ 53: 27];
        datash[2] <= doutb[ 80: 54];
        datash[3] <= doutb[107: 81];
        datash[4] <= doutb[134:108];
        datash[5] <= doutb[161:135];
        datash[6] <= doutb[188:162];
        datash[7] <= doutb[215:189];
        datash[8] <= doutb[242:216];

        case(addraybuf[2][2:0])
        0:
        begin
            tA[2:0] <= datash[addraxbuf[2]  ][ 2: 0];
            tB[2:0] <= datash[addraxbuf[2]  ][ 5: 3];
            tC[2:0] <= datash[addraxbuf[2]+1][ 2: 0];
            tD[2:0] <= datash[addraxbuf[2]+1][ 5: 3];
        end
        1:
        begin
            tA[2:0] <= datash[addraxbuf[2]  ][ 5: 3];
            tB[2:0] <= datash[addraxbuf[2]  ][ 8: 6];
            tC[2:0] <= datash[addraxbuf[2]+1][ 5: 3];
            tD[2:0] <= datash[addraxbuf[2]+1][ 8: 6];
        end
        2:
        begin
            tA[2:0] <= datash[addraxbuf[2]  ][ 8: 6];
            tB[2:0] <= datash[addraxbuf[2]  ][11: 9];
            tC[2:0] <= datash[addraxbuf[2]+1][ 8: 6];
            tD[2:0] <= datash[addraxbuf[2]+1][11: 9];
        end
        3:
        begin
            tA[2:0] <= datash[addraxbuf[2]  ][11: 9];
            tB[2:0] <= datash[addraxbuf[2]  ][14:12];
            tC[2:0] <= datash[addraxbuf[2]+1][11: 9];
            tD[2:0] <= datash[addraxbuf[2]+1][14:12];
        end
        4:
        begin
            tA[2:0] <= datash[addraxbuf[2]  ][14:12];
            tB[2:0] <= datash[addraxbuf[2]  ][17:15];
            tC[2:0] <= datash[addraxbuf[2]+1][14:12];
            tD[2:0] <= datash[addraxbuf[2]+1][17:15];
        end
        5:
        begin
            tA[2:0] <= datash[addraxbuf[2]  ][17:15];
            tB[2:0] <= datash[addraxbuf[2]  ][20:18];
            tC[2:0] <= datash[addraxbuf[2]+1][17:15];
            tD[2:0] <= datash[addraxbuf[2]+1][20:18];
        end
        6:
        begin
            tA[2:0] <= datash[addraxbuf[2]  ][20:18];
            tB[2:0] <= datash[addraxbuf[2]  ][23:21];
            tC[2:0] <= datash[addraxbuf[2]+1][20:18];
            tD[2:0] <= datash[addraxbuf[2]+1][23:21];
        end
        7:
        begin
            tA[2:0] <= datash[addraxbuf[2]  ][23:21];
            tB[2:0] <= datash[addraxbuf[2]  ][26:24];
            tC[2:0] <= datash[addraxbuf[2]+1][23:21];
            tD[2:0] <= datash[addraxbuf[2]+1][26:24];
        end
        endcase 

        ktA <= yyybuf[4][2:0] * tD  + (8 - yyybuf[4][2:0]) * tC;
        ktB <= yyybuf[4][2:0] * tB  + (8 - yyybuf[4][2:0]) * tA; 
        ktC <= xxxbuf[5][2:0] * ktA + (8 - xxxbuf[5][2:0]) * ktB;

        if( xxxbuf[6] >= left  && 
            xxxbuf[6] <  right && 
            yyybuf[6] >= top   && 
            yyybuf[6] <  bot   && 
            enb )
        begin
            dat[23:16] <= ( 
                            (       ktC[8:1] ) *    color[23:16] + 
                            ( 255 - ktC[8:1] ) * dbuf[10][23:16]
                           ) / 256;

            dat[15: 8] <= ( 
                            (       ktC[8:1] ) *    color[15: 8] + 
                            ( 255 - ktC[8:1] ) * dbuf[10][15: 8]
                           ) / 256;

            dat[ 7: 0] <= ( 
                            (       ktC[8:1] ) *    color[ 7: 0] + 
                            ( 255 - ktC[8:1] ) * dbuf[10][ 7: 0]
                           ) / 256;
        end
        else
            dat[23:0] <= dbuf[10][23:0];
    end
endmodule

