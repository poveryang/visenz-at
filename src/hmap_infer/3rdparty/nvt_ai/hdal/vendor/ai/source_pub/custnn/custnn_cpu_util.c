/**
	@brief Source file of custom layers.

	@file net_cust_layer.c

	@ingroup custnn

	@note Nothing.

	Copyright Novatek Microelectronics Corp. 2019.  All rights reserved.
*/

/*-----------------------------------------------------------------------------*/
/* Include Files                                                               */
/*-----------------------------------------------------------------------------*/
#include "kwrap/error_no.h"
#include "kwrap/type.h"
#include "net_util_sample.h"
#include "net_gen_sample/net_gen_sample.h"
#include "custnn/net_cust_layer.h"
#include "custnn_cpu.h"
#include "hd_common.h"

/*-----------------------------------------------------------------------------*/
/* Macro Constant Definitions                                                  */
/*-----------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------*/
/* Interface Functions                                                         */
/*-----------------------------------------------------------------------------*/
NN_AI_IO_TYPE custnn_get_type(NN_FMT format)
{
	NN_AI_IO_TYPE ret = NN_AI_IO_INT8;
	UINT32 bitdepth = format.sign_bits + format.int_bits + format.frac_bits;
	
	if (format.sign_bits) {
		if (bitdepth == 16) {
			ret = NN_AI_IO_INT16;
		} else {
			ret = NN_AI_IO_INT8;
		}
	} else {
		if (bitdepth == 16) {
			ret = NN_AI_IO_UINT16;
		} else {
			ret = NN_AI_IO_UINT8;
		}
	}
	
	return ret;
}

INT64 custnn_int64_rightShift(INT64 val, unsigned char shiftBits)
{
    INT64 oVal = 0;
    INT64 sign = (val < 0) ? -1 : 1;
    INT64 absVal = ( val >= 0 ) ? val : -val;
    INT64 lbVal = ((INT64)(-((UINT64)(0x1)<<63)));
	INT64 ubVal = (INT64)(((UINT64)(0x1)<<63)-1);

    if (shiftBits>63) {
        printf("custnn: right shift 64 might overflow: val = %d, shiftBits = %d\r\n", (unsigned int)val, (unsigned int)shiftBits);
		return oVal;
    }

    if (shiftBits > 0) {
        absVal += ((INT64)1<<(shiftBits-1));     // rounding
        oVal = (absVal>>shiftBits) * sign;
    } else {
        oVal = val;
    }

    if (oVal > ubVal) {
        oVal = ubVal;
    } else if(oVal < lbVal) {
        oVal = lbVal;
    }

    return oVal;
}

INT64 custnn_int64_leftShift(INT64 val, unsigned char shiftBits)
{
    INT64 oVal = 0;
    INT64 sign = (val < 0) ? -1 : 1;
    INT64 absVal = ( val >= 0 ) ? val : -val;
    INT64 lbVal = ((INT64)(-((UINT64)(0x1)<<63)));
	INT64 ubVal = (INT64)(((UINT64)(0x1)<<63)-1);

    if (shiftBits>31 ) {
        printf("custnn: Rounding might overflow: val = %d, shiftBits = %d\r\n", (unsigned int)val, (unsigned int)shiftBits);
    }

    if (shiftBits > 0) {
        oVal = (absVal<<shiftBits) * sign;
    } else {
        oVal = val;
    }

    if (oVal > ubVal) {
        oVal = ubVal;
    } else if(oVal < lbVal) {
        oVal = lbVal;
    }

    return (INT64)oVal;
}
/*
static INT64 custnn_rightShf(INT64 val, unsigned char shiftBits)
{
    INT64 oVal = 0;
    INT64 sign = (val < 0) ? -1 : 1;
    INT64 absVal = ( val >= 0 ) ? val : -val;
    INT64 lbVal = (-((INT64)(1)<<31));
	INT64 ubVal = (((INT64)(1)<<31)-1);

    if(shiftBits>63){
        printf("cusynn: right shift might overflow: val = %d, shiftBits = %d\r\n", (int)val, (int)shiftBits);
    }

    if(shiftBits > 0){
        absVal += ((INT64)1<<(shiftBits-1));
        oVal = (absVal>>shiftBits) * sign;
    }else{
        // FFFFFFFFFFE00000 * FFFFFFFFFFFFFC00 = 0000000080000000
        // (INT32)0000000080000000(positive) -> 80000000(negative)
        oVal = val;
    }

    if(oVal > ubVal){
        oVal = ubVal;
    }else if(oVal < lbVal){
        oVal = lbVal;
    }

    return (int)oVal;
}

static INT64 custnn_leftShift(INT64 val, unsigned char shiftBits)
{
    INT64 oVal  = 0;
    INT64 lbVal = (-((INT64)(1)<<31));
	INT64 ubVal = (((INT64)(1)<<31)-1);

    if(shiftBits>63){
        printf("custnn: left shift might overflow: val = %d, shiftBits = %d\r\n", (int)val, (int)shiftBits);
    }

    oVal = val * (INT64)(1<<shiftBits);
    if(oVal > ubVal){
        oVal = ubVal;
    }else if(oVal < lbVal){
        oVal = lbVal;
    }

    return (int)oVal;
}
*/

INT64 custnn_int64_clamp(INT64 prx, INT64 lb, INT64 ub)
{
	/* avoid integer overflow or underflow */
	if (prx < lb) {
		return (INT64)lb;
	} else if (prx > ub) {
		return (INT64)ub;
	} else {
		return (INT64)prx;
	}

}

INT32 custnn_r_scale_shift(INT64 ival, int ShiftDir, int Shift, int Scale, int bitdepth, int sign)
{
    int out, rbit, sbitupb;
    INT64 clamp_upb, clamp_dnb;
    INT64 oval=0, scval=0, sfval=0, rval;
    const int maxbit = 64;

    if (sign) {
        clamp_upb   = (((INT64)1)<<(bitdepth-1))-(INT64)1;
        clamp_dnb   = -(((INT64)1)<<(bitdepth-1));
    } else {
        clamp_upb   = (((INT64)1)<<(bitdepth))-(INT64)1;
        clamp_dnb   = 0;
    }

    //Scale
    scval = ival * (INT64)Scale;

    //check remaining bit
    if (scval<0) {
        rval = -1*scval;
    } else {
        rval = scval;
    }
    rbit = 0;
    while (rval) {
        rval = (rval>>1);
        rbit++;
    }
    sbitupb = maxbit-rbit-1;

	//Shift
	if (ShiftDir) {
	    // left shift

        // To prevent leftshift > 64-bit
        if (Shift>sbitupb) {
            Shift = sbitupb;
        }

		sfval = custnn_int64_leftShift(scval, Shift);
	} else {
        // right shift
		sfval = custnn_int64_rightShift(scval, Shift);
	}

    oval = custnn_int64_clamp(sfval, (INT64)clamp_dnb, (INT64)clamp_upb);
    out = (int)oval;

    return out;
}

HD_RESULT custnn_cpu_scaleshift_process(UINT32 in_addr, UINT32 out_addr, UINT32 buf_size, UINT32 io_type, INT32 scale, INT32 shift)
{
	UINT32 i = 0;
	int in_sign = ((io_type & 0x1) > 0)?0:1;
	int in_bitdepth = ((io_type >> 1) > 0)?16:8;
	int tmp_shf = (shift > 0)?shift:(-1*shift);
	int tmp_shf_dir = (shift > 0)?0:1;
	INT16*  in_16   = NULL;
	INT8*   in_8    = NULL;
	UINT16* in_u16  = NULL;
	UINT8*  in_u8   = NULL;
	INT16*  out_16  = NULL;
	INT8*   out_8   = NULL;
	UINT16* out_u16 = NULL;
	UINT8*  out_u8  = NULL;
	
	if (in_addr == 0 || out_addr == 0) {
		printf("custnn: input addr is NULL\n");
		return -1;
	}
	
	in_16   = (INT16*)in_addr;
	in_8    = (INT8*)in_addr;
	in_u16  = (UINT16*)in_addr;
	in_u8   = (UINT8*)in_addr;
	out_16  = (INT16*)out_addr;
	out_8   = (INT8*)out_addr;
	out_u16 = (UINT16*)out_addr;
	out_u8  = (UINT8*)out_addr;
	
	
	for (i = 0; i < buf_size*8/in_bitdepth; i++) {
		if (io_type == 0) {
			out_8[i]   = (INT8)custnn_r_scale_shift(in_8[i], tmp_shf_dir, tmp_shf, scale, in_bitdepth, in_sign);
		} else if (io_type == 1) {
			out_u8[i]  = (UINT8)custnn_r_scale_shift(in_u8[i], tmp_shf_dir, tmp_shf, scale, in_bitdepth, in_sign);
		} else if (io_type == 2) {
			out_16[i]  = (INT16)custnn_r_scale_shift(in_16[i], tmp_shf_dir, tmp_shf, scale, in_bitdepth, in_sign);
		} else if (io_type == 3) {
			out_u16[i] = (UINT16)custnn_r_scale_shift(in_u16[i], tmp_shf_dir, tmp_shf, scale, in_bitdepth, in_sign);
		} else {
			printf("custnn: unknown type\n");
			return -1;
		}
	}
	
	return HD_OK;
}
