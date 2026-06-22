module STATICGROUP #
(
    parameter            RAMID = 0
)
(
    input  wire          clk,
//Input
    input  wire [  11:0] ix,
    input  wire [  11:0] iy,
    input  wire          ivs,
    input  wire          ihs,
    input  wire          ide,
    input  wire [  23:0] idat,
//Output
    output reg  [  11:0] ox,
    output reg  [  11:0] oy,
    output reg           ovs,
    output reg           ohs,
    output reg           ode,
    output reg  [  23:0] odat,
//Parameters
    input  wire [   9:0] ramid,
    input  wire [  15:0] rama,
    input  wire [   2:0] ramd,
    input  wire [1199:0] iparam
);

    parameter N  = 8;
    parameter BD = 16384;

    reg  [ BD:0] ram_addr;
    reg          wea;
    reg          en;
    reg          endelay;
    reg          ebuf[2:0];
    reg          hbuf[2:0];
    reg          vbuf[2:0];
    reg  [  2:0] bram[BD:0];
    reg  [  2:0] ram_datao;
    reg  [ 11:0] xbuf[2:0];
    reg  [ 11:0] ybuf[2:0];
    reg  [ 15:0] ram_addro;
    reg  [ 17:0] addr[0:N-1];
    reg  [ 17:0] rom_addr;
    reg  [ 23:0] dbuf[2:0];
    reg  [ 23:0] dat;
    reg  [ 23:0] color;
    reg  [ 31:0] i;
    reg  [149:0] objparam[0:N-1];
    reg  [711:0] param_;

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
        ode  <= ebuf[2];
        ohs  <= hbuf[2];
        ovs  <= vbuf[2];
        ox   <= xbuf[2];
        oy   <= ybuf[2];

        { ebuf[2], ebuf[1], ebuf[0] } <= { ebuf[1], ebuf[0], ide  };
        { hbuf[2], hbuf[1], hbuf[0] } <= { hbuf[1], hbuf[0], ihs  };
        { vbuf[2], vbuf[1], vbuf[0] } <= { vbuf[1], vbuf[0], ivs  };
        { xbuf[2], xbuf[1], xbuf[0] } <= { xbuf[1], xbuf[0], ix   };
        { ybuf[2], ybuf[1], ybuf[0] } <= { ybuf[1], ybuf[0], iy   };
        { dbuf[2], dbuf[1], dbuf[0] } <= { dbuf[1], dbuf[0], idat };

        if( ramid == RAMID ) wea <= 1'b1;else wea <= 1'b0;

        if( ivs )
        begin
            objparam[0][127:  0] <= iparam[ 149:   0];
            objparam[1][127:  0] <= iparam[ 299: 150];
            objparam[2][127:  0] <= iparam[ 449: 300];
            objparam[3][127:  0] <= iparam[ 599: 450];
            objparam[4][127:  0] <= iparam[ 749: 600];
            objparam[5][127:  0] <= iparam[ 899: 750];
            objparam[6][127:  0] <= iparam[1049: 900];
            objparam[7][127:  0] <= iparam[1199:1050];
            for( i = 0; i <= N - 1; i = i + 1 )
            begin
                addr[i] <= 0;
            end
        end

        if( ram_datao != 0 && endelay )
        begin
            dat[23:16] <= ( color[23:16] * ( ram_datao + 1 ) 
            + dbuf[1][23:16] * ( 7 - ram_datao ) ) / 8;

            dat[15: 8] <= ( color[15: 8] * ( ram_datao + 1 ) 
            + dbuf[1][23:16] * ( 7 - ram_datao ) ) / 8;

            dat[ 7: 0] <= ( color[ 7: 0] * ( ram_datao + 1 ) 
            + dbuf[1][23:16] * ( 7 - ram_datao ) ) / 8;
        end
        else
            dat[23: 0] <= dbuf[1];

        endelay <= en;
        en      <= 1'b0;

        for( i = 0; i <= N-1; i = i + 1 )
        if( ix >= objparam[i][36:25] 
            &&
            ix <  objparam[i][36:25] + objparam[i][60:49]
            &&
            iy >= objparam[i][48:37]
            &&
            iy <  objparam[i][48:37] + objparam[i][72:61]
            &&
            objparam[i][0] )
        begin
            ram_addro <= objparam[i][88:73] + addr[i];
            addr[i]   <= addr[i] + 1;
            color     <= objparam[i][24:1];
            en        <= 1'b1;
        end
    end
endmodule

