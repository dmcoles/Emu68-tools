#ifndef _BRCM_WIFI_H
#define _BRCM_WIFI_H

#include <exec/types.h>

/* Enumerate crypto algorithms */
#define CRYPTO_ALGO_OFF                 0
#define CRYPTO_ALGO_WEP1                1
#define CRYPTO_ALGO_TKIP                2
#define CRYPTO_ALGO_WEP128              3
#define CRYPTO_ALGO_AES_CCM             4
#define CRYPTO_ALGO_AES_RESERVED1       5
#define CRYPTO_ALGO_AES_RESERVED2       6
#define CRYPTO_ALGO_NALG                7

/* wireless security bitvec */

#define WEP_ENABLED             0x0001
#define TKIP_ENABLED            0x0002
#define AES_ENABLED             0x0004
#define WSEC_SWFLAG             0x0008
/* to go into transition mode without setting wep */
#define SES_OW_ENABLED          0x0040
/* MFP */
#define MFP_CAPABLE             0x0200
#define MFP_REQUIRED            0x0400

/* WPA authentication mode bitvec */
#define WPA_AUTH_DISABLED       0x0000  /* Legacy (i.e., non-WPA) */
#define WPA_AUTH_NONE           0x0001  /* none (IBSS) */
#define WPA_AUTH_UNSPECIFIED    0x0002  /* over 802.1x */
#define WPA_AUTH_PSK            0x0004  /* Pre-shared key */
#define WPA_AUTH_RESERVED1      0x0008
#define WPA_AUTH_RESERVED2      0x0010

#define WPA2_AUTH_RESERVED1     0x0020
#define WPA2_AUTH_UNSPECIFIED   0x0040  /* over 802.1x */
#define WPA2_AUTH_PSK           0x0080  /* Pre-shared key */
#define WPA2_AUTH_RESERVED3     0x0200
#define WPA2_AUTH_RESERVED4     0x0400
#define WPA2_AUTH_RESERVED5     0x0800
#define WPA2_AUTH_1X_SHA256     0x1000  /* 1X with SHA256 key derivation */
#define WPA2_AUTH_FT            0x4000  /* Fast BSS Transition */
#define WPA2_AUTH_PSK_SHA256    0x8000  /* PSK with SHA256 key derivation */

#define WPA3_AUTH_SAE_PSK       0x40000 /* SAE with 4-way handshake */


#endif /* _BRCM_WIFI_H */
