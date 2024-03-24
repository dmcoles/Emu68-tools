#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <devices/timer.h>

#include <proto/exec.h>

#include "d11.h"

/*
 * A chanspec (u16) holds the channel number, band, bandwidth and control
 * sideband
 */

/* channel defines */
#define CH_UPPER_SB			0x01
#define CH_LOWER_SB			0x02
#define CH_EWA_VALID			0x04
#define CH_70MHZ_APART			14
#define CH_50MHZ_APART			10
#define CH_30MHZ_APART			6
#define CH_20MHZ_APART			4
#define CH_10MHZ_APART			2
#define CH_5MHZ_APART			1 /* 2G band channels are 5 Mhz apart */
#define CH_MIN_2G_CHANNEL		1
#define CH_MAX_2G_CHANNEL		14	/* Max channel in 2G band */
#define CH_MIN_5G_CHANNEL		34

static inline void brcmu_maskset32(ULONG *var, ULONG mask, UBYTE shift, ULONG value)
{
    value = (value << shift) & mask;
    *var = (*var & ~mask) | value;
}
static inline ULONG brcmu_maskget32(ULONG var, ULONG mask, UBYTE shift)
{
    return (var & mask) >> shift;
}
static inline void brcmu_maskset16(UWORD *var, UWORD mask, UBYTE shift, UWORD value)
{
    value = (value << shift) & mask;
    *var = (*var & ~mask) | value;
}
static inline UWORD brcmu_maskget16(UWORD var, UWORD mask, UBYTE shift)
{
    return (var & mask) >> shift;
}

static void brcmu_d11n_decchspec(struct ChannelInfo *ch)
{
    UWORD val;

    ch->ci_CHNum = (UBYTE)(ch->ci_CHSpec & BRCMU_CHSPEC_CH_MASK);
    ch->ci_ControlChannel = ch->ci_CHNum;

    switch (ch->ci_CHSpec & BRCMU_CHSPEC_D11N_BW_MASK) {
        case BRCMU_CHSPEC_D11N_BW_20:
            ch->ci_Bandwidth = BRCMU_CHAN_BW_20;
            ch->ci_Sideband = BRCMU_CHAN_SB_NONE;
            break;
        case BRCMU_CHSPEC_D11N_BW_40:
            ch->ci_Bandwidth = BRCMU_CHAN_BW_40;
            val = ch->ci_CHSpec & BRCMU_CHSPEC_D11N_SB_MASK;
            if (val == BRCMU_CHSPEC_D11N_SB_L) {
                ch->ci_Sideband = BRCMU_CHAN_SB_L;
                ch->ci_ControlChannel -= CH_10MHZ_APART;
            } else {
                ch->ci_Sideband = BRCMU_CHAN_SB_U;
                ch->ci_ControlChannel += CH_10MHZ_APART;
            }
            break;
        default:
            break;
    }

    switch (ch->ci_CHSpec & BRCMU_CHSPEC_D11N_BND_MASK) {
        case BRCMU_CHSPEC_D11N_BND_5G:
            ch->ci_Band = BRCMU_CHAN_BAND_5G;
            break;
        case BRCMU_CHSPEC_D11N_BND_2G:
            ch->ci_Band = BRCMU_CHAN_BAND_2G;
            break;
        default:
            break;
    }
}

static void brcmu_d11ac_decchspec(struct ChannelInfo *ch)
{
    UWORD val;

    ch->ci_CHNum = (UBYTE)(ch->ci_CHSpec & BRCMU_CHSPEC_CH_MASK);
    ch->ci_ControlChannel = ch->ci_CHNum;

    switch (ch->ci_CHSpec & BRCMU_CHSPEC_D11AC_BW_MASK) {
        case BRCMU_CHSPEC_D11AC_BW_20:
            ch->ci_Bandwidth = BRCMU_CHAN_BW_20;
            ch->ci_Sideband = BRCMU_CHAN_SB_NONE;
            break;
        case BRCMU_CHSPEC_D11AC_BW_40:
            ch->ci_Bandwidth = BRCMU_CHAN_BW_40;
            val = ch->ci_CHSpec & BRCMU_CHSPEC_D11AC_SB_MASK;
            if (val == BRCMU_CHSPEC_D11AC_SB_L) {
                ch->ci_Sideband = BRCMU_CHAN_SB_L;
                ch->ci_ControlChannel -= CH_10MHZ_APART;
            } else if (val == BRCMU_CHSPEC_D11AC_SB_U) {
                ch->ci_Sideband = BRCMU_CHAN_SB_U;
                ch->ci_ControlChannel += CH_10MHZ_APART;
            }
            break;
        case BRCMU_CHSPEC_D11AC_BW_80:
            ch->ci_Bandwidth = BRCMU_CHAN_BW_80;
            ch->ci_Sideband = brcmu_maskget16(ch->ci_CHSpec, BRCMU_CHSPEC_D11AC_SB_MASK,
                            BRCMU_CHSPEC_D11AC_SB_SHIFT);
            switch (ch->ci_Sideband) {
                case BRCMU_CHAN_SB_LL:
                    ch->ci_ControlChannel -= CH_30MHZ_APART;
                    break;
                case BRCMU_CHAN_SB_LU:
                    ch->ci_ControlChannel -= CH_10MHZ_APART;
                    break;
                case BRCMU_CHAN_SB_UL:
                    ch->ci_ControlChannel += CH_10MHZ_APART;
                    break;
                case BRCMU_CHAN_SB_UU:
                    ch->ci_ControlChannel += CH_30MHZ_APART;
                    break;
                default:
                    break;
            }
            break;
        case BRCMU_CHSPEC_D11AC_BW_160:
            ch->ci_Bandwidth = BRCMU_CHAN_BW_160;
            ch->ci_Sideband = brcmu_maskget16(ch->ci_CHSpec, BRCMU_CHSPEC_D11AC_SB_MASK,
                            BRCMU_CHSPEC_D11AC_SB_SHIFT);
            switch (ch->ci_Sideband) {
                case BRCMU_CHAN_SB_LLL:
                    ch->ci_ControlChannel -= CH_70MHZ_APART;
                    break;
                case BRCMU_CHAN_SB_LLU:
                    ch->ci_ControlChannel -= CH_50MHZ_APART;
                    break;
                case BRCMU_CHAN_SB_LUL:
                    ch->ci_ControlChannel -= CH_30MHZ_APART;
                    break;
                case BRCMU_CHAN_SB_LUU:
                    ch->ci_ControlChannel -= CH_10MHZ_APART;
                    break;
                case BRCMU_CHAN_SB_ULL:
                    ch->ci_ControlChannel += CH_10MHZ_APART;
                    break;
                case BRCMU_CHAN_SB_ULU:
                    ch->ci_ControlChannel += CH_30MHZ_APART;
                    break;
                case BRCMU_CHAN_SB_UUL:
                    ch->ci_ControlChannel += CH_50MHZ_APART;
                    break;
                case BRCMU_CHAN_SB_UUU:
                    ch->ci_ControlChannel += CH_70MHZ_APART;
                    break;
                default:
                    break;
            }
            break;
        case BRCMU_CHSPEC_D11AC_BW_8080:
        default:
            break;
    }

    switch (ch->ci_CHSpec & BRCMU_CHSPEC_D11AC_BND_MASK) {
        case BRCMU_CHSPEC_D11AC_BND_5G:
            ch->ci_Band = BRCMU_CHAN_BAND_5G;
            break;
        case BRCMU_CHSPEC_D11AC_BND_2G:
            ch->ci_Band = BRCMU_CHAN_BAND_2G;
            break;
        default:
            break;
    }
}

static UWORD d11ac_bw(enum brcmu_chan_bw bw)
{
    switch (bw) {
        case BRCMU_CHAN_BW_20:
            return BRCMU_CHSPEC_D11AC_BW_20;
        case BRCMU_CHAN_BW_40:
            return BRCMU_CHSPEC_D11AC_BW_40;
        case BRCMU_CHAN_BW_80:
            return BRCMU_CHSPEC_D11AC_BW_80;
        case BRCMU_CHAN_BW_160:
            return BRCMU_CHSPEC_D11AC_BW_160;
        default:
            break;
    }
    return 0;
}

static UWORD d11n_sb(enum brcmu_chan_sb sb)
{
    switch (sb) {
        case BRCMU_CHAN_SB_NONE:
            return BRCMU_CHSPEC_D11N_SB_N;
        case BRCMU_CHAN_SB_L:
            return BRCMU_CHSPEC_D11N_SB_L;
        case BRCMU_CHAN_SB_U:
            return BRCMU_CHSPEC_D11N_SB_U;
        default:
            break;
    }
    return 0;
}

static UWORD d11n_bw(enum brcmu_chan_bw bw)
{
    switch (bw) {
        case BRCMU_CHAN_BW_20:
            return BRCMU_CHSPEC_D11N_BW_20;
        case BRCMU_CHAN_BW_40:
            return BRCMU_CHSPEC_D11N_BW_40;
        default:
            break;
    }
    return 0;
}

static void brcmu_d11n_encchspec(struct ChannelInfo *ch)
{
    if (ch->ci_Bandwidth == BRCMU_CHAN_BW_20)
        ch->ci_Sideband = BRCMU_CHAN_SB_NONE;

    ch->ci_CHSpec = 0;
    brcmu_maskset16(&ch->ci_CHSpec, BRCMU_CHSPEC_CH_MASK,
            BRCMU_CHSPEC_CH_SHIFT, ch->ci_CHNum);
    brcmu_maskset16(&ch->ci_CHSpec, BRCMU_CHSPEC_D11N_SB_MASK,
            0, d11n_sb(ch->ci_Sideband));
    brcmu_maskset16(&ch->ci_CHSpec, BRCMU_CHSPEC_D11N_BW_MASK,
            0, d11n_bw(ch->ci_Bandwidth));

    if (ch->ci_CHNum <= CH_MAX_2G_CHANNEL)
        ch->ci_CHSpec |= BRCMU_CHSPEC_D11N_BND_2G;
    else
        ch->ci_CHSpec |= BRCMU_CHSPEC_D11N_BND_5G;
}

static void brcmu_d11ac_encchspec(struct ChannelInfo *ch)
{
    if (ch->ci_Bandwidth == BRCMU_CHAN_BW_20 || ch->ci_Sideband == BRCMU_CHAN_SB_NONE)
        ch->ci_Sideband = BRCMU_CHAN_SB_L;

    brcmu_maskset16(&ch->ci_CHSpec, BRCMU_CHSPEC_CH_MASK,
            BRCMU_CHSPEC_CH_SHIFT, ch->ci_CHNum);
    brcmu_maskset16(&ch->ci_CHSpec, BRCMU_CHSPEC_D11AC_SB_MASK,
            BRCMU_CHSPEC_D11AC_SB_SHIFT, ch->ci_Sideband);
    brcmu_maskset16(&ch->ci_CHSpec, BRCMU_CHSPEC_D11AC_BW_MASK,
            0, d11ac_bw(ch->ci_Bandwidth));

    ch->ci_CHSpec &= ~BRCMU_CHSPEC_D11AC_BND_MASK;
    if (ch->ci_CHNum <= CH_MAX_2G_CHANNEL)
        ch->ci_CHSpec |= BRCMU_CHSPEC_D11AC_BND_2G;
    else
        ch->ci_CHSpec |= BRCMU_CHSPEC_D11AC_BND_5G;
}

void EncodeChanSpec(struct ChannelInfo *ci, UBYTE ioType)
{
    if (ioType == BRCMU_D11N_IOTYPE) {
        brcmu_d11n_encchspec(ci);
    }
    else {
        brcmu_d11ac_encchspec(ci);
    }
}

void DecodeChanSpec(struct ChannelInfo *ci, UBYTE ioType)
{
    if (ioType == BRCMU_D11N_IOTYPE) {
        brcmu_d11n_decchspec(ci);
    }
    else {
        brcmu_d11ac_decchspec(ci);
    }
}
