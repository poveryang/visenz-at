#ifndef _NN_VERINFO_H_
#define _NN_VERINFO_H_

typedef enum {
	NN_CHIP_CNN20   = 0,         ///< chip CNN 313 
	NN_CHIP_CNN25_A = 1,         ///< chip CNN 520 
	NN_CHIP_CNN25_B = 2,         ///< chip CNN 321 
	NN_CHIP_CNN25_C = 3,         ///< chip CNN 528 
	ENUM_DUMMY4WORD(NN_CHIP_ID)
} NN_CHIP_ID;

typedef enum { 
    NN_CHIP_AI1 = 0,         //v2 
    NN_CHIP_AI2 = 1,         //v3 
    ENUM_DUMMY4WORD(NN_CHIP_FMT) 
} NN_CHIP_FMT;

typedef enum { 
    NN_CHIP_AI_SUBVER0 = 0,         //vx.0 
    NN_CHIP_AI_SUBVER1 = 1,         //vx.1 
	NN_CHIP_AI_SUBVER2 = 2,         //vx.2 
    ENUM_DUMMY4WORD(NN_CHIP_FMT_SUBVER) 
} NN_CHIP_FMT_SUBVER;

#define NN_VERSION_ENTRY(_major, _minor, _bugfix, _branch)  ((_major << 24) + (_minor << 16) + (_bugfix << 8) + _branch)
#if CNN_25_MATLAB
#define NN_GEN_VERSION			NN_VERSION_ENTRY(0x13, 0x0, 0x0, 0x0)
#define NN_CHIP                 NN_VERSION_ENTRY(0x0, NN_CHIP_AI1, 0x0, NN_CHIP_CNN25_A)
#else
#define NN_GEN_VERSION			NN_VERSION_ENTRY(0x15, 0x0, 0x0, 0x0)
#define NN_CHIP                 NN_VERSION_ENTRY(NN_CHIP_AI_SUBVER1, NN_CHIP_AI2, 0x0, NN_CHIP_CNN25_A)
#endif



#endif