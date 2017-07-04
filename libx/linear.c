#include "sysdefs.h"
#include "stdform.h"

static const short seg_aend[8] = {0x1F, 0x3F, 0x7F, 0xFF,  
                0x1FF, 0x3FF, 0x7FF, 0xFFF};  
static const short seg_uend[8] = {0x3F, 0x7F, 0xFF, 0x1FF,  
                0x3FF, 0x7FF, 0xFFF, 0x1FFF};

static short search ( short  val, short  *table, short   size)  
{  
    short       i;  
  
    for (i = 0; i < size; i++)  
    {  
        if (val <= *table++)     /** Find a signal the same with input one (greater than0 */
        {  
            return (i);  
        }  
    }  
    return (size);
}  

/********************* PCM 2 a-law Compress  ****************************/  
  
char linear2alaw (short  audio_val)    /* 2's complement (16-bit range) */
{  
    short       mask;  
    short       seg;  
    unsigned char   aval;  
  
  
    audio_val = audio_val >> 3;  
  
    if (audio_val >= 0)  
    {  
        mask = 0xD5;        /* sign (7th) bit = 1 */  
    }  
    else  
    {  
        mask = 0x55;        /* sign bit = 0 */  
        audio_val = -audio_val - 1;  
    }  
  
    /* Convert the scaled magnitude to segment number. */  
    seg = search(audio_val, (short *)seg_aend, (short)8);  
  
    /* Combine the sign, segment, and quantization bits. */  
  
    if (seg >= 8)        /* out of range, return maximum value. */  
    {  
        return (unsigned char) (0x7F ^ mask);  
    }  
    else  
    {  
        aval = (unsigned char) seg << SEG_SHIFT;  
  
        if (seg < 2)  
        {  
            aval |= (audio_val >> 1) & QUANT_MASK;  
        }  
        else  
        {  
            aval |= (audio_val >> seg) & QUANT_MASK;  
        }  
        return (aval ^ mask);  
    }  
}  

/*********************** pcm 2 µ-law *************************/  
  
char linear2ulaw(short audio_val) /* 2's complement (16-bit range) */
{  
    short       mask;  
    short       seg;  
    unsigned char   uval;  
  
    /* Get the sign and the magnitude of the value. */  
    audio_val = audio_val >> 2;  
    if (audio_val < 0)  
    {  
        audio_val = -audio_val;  
        mask = 0x7F;  
    }  
    else  
    {  
        mask = 0xFF;  
    }  
    if ( audio_val > CLIP )  
    {  
        audio_val = CLIP;     /* clip the magnitude */  
    }  
    audio_val += (BIAS >> 2);  
  
    /* Convert the scaled magnitude to segment number. */  
    seg = search(audio_val, (short *)seg_uend, (short)8);  
  
    /* 
     * Combine the sign, segment, quantization bits; 
     * and complement the code word. 
     */  
    if (seg >= 8)        /* out of range, return maximum value. */  
    {  
        return (unsigned char) (0x7F ^ mask);  
    }  
    else  
    {  
        uval = (unsigned char) (seg << 4) | ((audio_val >> (seg + 1)) & 0xF);  
        return (uval ^ mask);  
    }  
}  
