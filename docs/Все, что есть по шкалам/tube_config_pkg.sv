package tube_config_pkg;

    typedef enum int {
        TYPE_RECTANGLE = 0,
        TYPE_ROTATIONOBJECT = 1,
        TYPE_STATICGROUP = 2,
        TYPE_RECTANGLEa = 3,
        TYPE_DASHLINE = 4,
        TYPE_AVIAHORIZONT = 5,
        TYPE_SPEEDSCALE = 6,
        TYPE_SYMBOLs = 7,
        TYPE_PITCH_SCALE = 8,
        TYPE_ROLL_SCALE = 9,
        TYPE_COMPASS_SCALE = 10
    } stage_type_t;

    localparam int N_STAGES = 4;//56;
    localparam int PARAM_ARRAY_SIZE = 200;

    localparam stage_type_t STAGE_TYPE [0:N_STAGES-1] = '{
       /* TYPE_AVIAHORIZONT, // sec0000
        TYPE_RECTANGLEa, // sec0001
        TYPE_STATICGROUP, // sec0002
        TYPE_RECTANGLEa, // sec0003
        TYPE_RECTANGLEa, // sec0004
        TYPE_RECTANGLEa, // sec0005
        TYPE_RECTANGLEa, // sec0006
        TYPE_RECTANGLEa, // sec0007
        TYPE_DASHLINE, // sec0008
        TYPE_RECTANGLEa, // sec0009
        TYPE_RECTANGLEa, // sec0010
        TYPE_RECTANGLEa, // sec0011
        TYPE_RECTANGLEa, // sec0012
        TYPE_RECTANGLEa, // sec0013
        TYPE_RECTANGLEa, // sec0014
        TYPE_RECTANGLEa, // sec0015
        TYPE_RECTANGLEa, // sec0016
        TYPE_RECTANGLEa, // sec0017
        TYPE_RECTANGLEa, // sec0018
        TYPE_RECTANGLEa, // sec0019
        TYPE_RECTANGLEa, // sec0020
        TYPE_RECTANGLEa, // sec0021
        TYPE_RECTANGLEa, // sec0022
        TYPE_RECTANGLEa, // sec0023
        TYPE_RECTANGLEa, // sec0024
        TYPE_RECTANGLEa, // sec0025
        TYPE_RECTANGLEa, // sec0026
        TYPE_SPEEDSCALE, // sec0027
        TYPE_RECTANGLEa, // sec0028
        TYPE_SPEEDSCALE, // sec0029
        TYPE_SPEEDSCALE, // sec0030
        TYPE_RECTANGLEa, // sec0031
        TYPE_RECTANGLEa, // sec0032
        TYPE_RECTANGLEa, // sec0033
        TYPE_SYMBOLs, // sec0034
        TYPE_SYMBOLs, // sec0035
        TYPE_SYMBOLs, // sec0036
        TYPE_SYMBOLs, // sec0037
        TYPE_SYMBOLs, // sec0038
        TYPE_SYMBOLs, // sec0039
        TYPE_SYMBOLs, // sec0040
        TYPE_STATICGROUP, // sec0041
        TYPE_STATICGROUP, // sec0042
        TYPE_STATICGROUP, // sec0043
        TYPE_SYMBOLs, // sec0044
        TYPE_STATICGROUP, // sec0045
        TYPE_SYMBOLs, // sec0046
        TYPE_ROTATIONOBJECT, // sec0047
        TYPE_STATICGROUP, // sec0048
        TYPE_RECTANGLEa, // sec0049
        TYPE_DASHLINE, // sec0050
        TYPE_STATICGROUP, // sec0051
        TYPE_STATICGROUP, // sec0052
        TYPE_STATICGROUP, // sec0053
        TYPE_SYMBOLs, // sec0054*/
        TYPE_RECTANGLEa,
        TYPE_ROLL_SCALE,
        TYPE_PITCH_SCALE, // sec0055
        TYPE_COMPASS_SCALE
    };

    localparam int STAGE_PARAM_COUNT [0:N_STAGES-1] = '{
        /*1, // sec0000
        1, // sec0001
        2, // sec0002
        1, // sec0003
        1, // sec0004
        1, // sec0005
        1, // sec0006
        1, // sec0007
        1, // sec0008
        1, // sec0009
        1, // sec0010
        1, // sec0011
        1, // sec0012
        1, // sec0013
        1, // sec0014
        1, // sec0015
        1, // sec0016
        1, // sec0017
        1, // sec0018
        1, // sec0019
        1, // sec0020
        1, // sec0021
        1, // sec0022
        1, // sec0023
        1, // sec0024
        1, // sec0025
        1, // sec0026
        3, // sec0027
        1, // sec0028
        3, // sec0029
        3, // sec0030
        1, // sec0031
        1, // sec0032
        1, // sec0033
        75, // sec0034
        2, // sec0035
        9, // sec0036
        3, // sec0037
        6, // sec0038
        1, // sec0039
        4, // sec0040
        1, // sec0041
        1, // sec0042
        1, // sec0043
        4, // sec0044
        1, // sec0045
        29, // sec0046
        1, // sec0047
        2, // sec0048
        1, // sec0049
        1, // sec0050
        2, // sec0051
        2, // sec0052
        3, // sec0053
        1, // sec0054*/
        1,
        1,
        1,
        1 // sec0055
    };

    localparam int STAGE_RAMID [0:N_STAGES-1] = '{
       /* 0, // sec0000
        0, // sec0001
        0, // sec0002
        0, // sec0003
        0, // sec0004
        0, // sec0005
        0, // sec0006
        0, // sec0007
        0, // sec0008
        0, // sec0009
        0, // sec0010
        0, // sec0011
        0, // sec0012
        0, // sec0013
        0, // sec0014
        0, // sec0015
        0, // sec0016
        0, // sec0017
        0, // sec0018
        0, // sec0019
        0, // sec0020
        0, // sec0021
        0, // sec0022
        0, // sec0023
        0, // sec0024
        0, // sec0025
        0, // sec0026
        0, // sec0027
        0, // sec0028
        0, // sec0029
        0, // sec0030
        0, // sec0031
        0, // sec0032
        0, // sec0033
        1, // sec0034
        2, // sec0035
        1, // sec0036
        2, // sec0037
        1, // sec0038
        2, // sec0039
        1, // sec0040
        3, // sec0041
        4, // sec0042
        5, // sec0043
        1, // sec0044
        6, // sec0045
        1, // sec0046
        7, // sec0047
        8, // sec0048
        0, // sec0049
        0, // sec0050
        9, // sec0051
        10, // sec0052
        11, // sec0053
        1, // sec0054*/
        0,
        0,
        0,
        0 // sec0055
    };

endpackage
