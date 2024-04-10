#ifndef _PACKET_H
#define _PACKET_H

#include "sdio.h"
//#include "wifipi.h"

/*******************************************************************************
 * Dongle command codes that are interpreted by firmware
 ******************************************************************************/
#define BRCMF_C_GET_VERSION			1
#define BRCMF_C_UP				2
#define BRCMF_C_DOWN				3
#define BRCMF_C_SET_PROMISC			10
#define BRCMF_C_GET_RATE			12
#define BRCMF_C_GET_INFRA			19
#define BRCMF_C_SET_INFRA			20
#define BRCMF_C_GET_AUTH			21
#define BRCMF_C_SET_AUTH			22
#define BRCMF_C_GET_BSSID			23
#define BRCMF_C_GET_SSID			25
#define BRCMF_C_SET_SSID			26
#define BRCMF_C_TERMINATED			28
#define BRCMF_C_GET_CHANNEL			29
#define BRCMF_C_SET_CHANNEL			30
#define BRCMF_C_GET_SRL				31
#define BRCMF_C_SET_SRL				32
#define BRCMF_C_GET_LRL				33
#define BRCMF_C_SET_LRL				34
#define BRCMF_C_GET_RADIO			37
#define BRCMF_C_SET_RADIO			38
#define BRCMF_C_GET_PHYTYPE			39
#define BRCMF_C_SET_KEY				45
#define BRCMF_C_GET_REGULATORY			46
#define BRCMF_C_SET_REGULATORY			47
#define BRCMF_C_SET_PASSIVE_SCAN		49
#define BRCMF_C_SCAN				50
#define BRCMF_C_SCAN_RESULTS			51
#define BRCMF_C_DISASSOC			52
#define BRCMF_C_REASSOC				53
#define BRCMF_C_SET_ROAM_TRIGGER		55
#define BRCMF_C_SET_ROAM_DELTA			57
#define BRCMF_C_GET_BCNPRD			75
#define BRCMF_C_SET_BCNPRD			76
#define BRCMF_C_GET_DTIMPRD			77
#define BRCMF_C_SET_DTIMPRD			78
#define BRCMF_C_SET_COUNTRY			84
#define BRCMF_C_GET_PM				85
#define BRCMF_C_SET_PM				86
#define BRCMF_C_GET_REVINFO			98
#define BRCMF_C_GET_MONITOR			107
#define BRCMF_C_SET_MONITOR			108
#define BRCMF_C_GET_CURR_RATESET		114
#define BRCMF_C_GET_AP				117
#define BRCMF_C_SET_AP				118
#define BRCMF_C_SET_SCB_AUTHORIZE		121
#define BRCMF_C_SET_SCB_DEAUTHORIZE		122
#define BRCMF_C_GET_RSSI			127
#define BRCMF_C_GET_WSEC			133
#define BRCMF_C_SET_WSEC			134
#define BRCMF_C_GET_PHY_NOISE			135
#define BRCMF_C_GET_BSS_INFO			136
#define BRCMF_C_GET_GET_PKTCNTS			137
#define BRCMF_C_GET_BANDLIST			140
#define BRCMF_C_SET_SCB_TIMEOUT			158
#define BRCMF_C_GET_ASSOCLIST			159
#define BRCMF_C_GET_PHYLIST			180
#define BRCMF_C_SET_SCAN_CHANNEL_TIME		185
#define BRCMF_C_SET_SCAN_UNASSOC_TIME		187
#define BRCMF_C_SCB_DEAUTHENTICATE_FOR_REASON	201
#define BRCMF_C_SET_ASSOC_PREFER		205
#define BRCMF_C_GET_VALID_CHANNELS		217
#define BRCMF_C_SET_FAKEFRAG			219
#define BRCMF_C_GET_KEY_PRIMARY			235
#define BRCMF_C_SET_KEY_PRIMARY			236
#define BRCMF_C_SET_SCAN_PASSIVE_TIME		258
#define BRCMF_C_GET_VAR				262
#define BRCMF_C_SET_VAR				263
#define BRCMF_C_SET_WSEC_PMK			268

// Dongle events
#define BRCMF_E_SET_SSID                        0
#define BRCMF_E_JOIN                            1
#define BRCMF_E_START                           2
#define BRCMF_E_AUTH                            3
#define BRCMF_E_AUTH_IND                        4
#define BRCMF_E_DEAUTH                          5
#define BRCMF_E_DEAUTH_IND                      6
#define BRCMF_E_ASSOC                           7
#define BRCMF_E_ASSOC_IND                       8
#define BRCMF_E_REASSOC                         9
#define BRCMF_E_REASSOC_IND                     10
#define BRCMF_E_DISASSOC                        11
#define BRCMF_E_DISASSOC_IND                    12
#define BRCMF_E_QUIET_START                     13
#define BRCMF_E_QUIET_END                       14
#define BRCMF_E_BEACON_RX                       15
#define BRCMF_E_LINK                            16
#define BRCMF_E_MIC_ERROR                       17
#define BRCMF_E_NDIS_LINK                       18
#define BRCMF_E_ROAM                            19
#define BRCMF_E_TXFAIL                          20
#define BRCMF_E_PMKID_CACHE                     21
#define BRCMF_E_RETROGRADE_TSF                  22
#define BRCMF_E_PRUNE                           23
#define BRCMF_E_AUTOAUTH                        24
#define BRCMF_E_EAPOL_MSG                       25
#define BRCMF_E_SCAN_COMPLETE                   26
#define BRCMF_E_ADDTS_IND                       27
#define BRCMF_E_DELTS_IND                       28
#define BRCMF_E_BCNSENT_IND                     29
#define BRCMF_E_BCNRX_MSG                       30
#define BRCMF_E_BCNLOST_MSG                     31
#define BRCMF_E_ROAM_PREP                       32
#define BRCMF_E_PFN_NET_FOUND                   33
#define BRCMF_E_PFN_NET_LOST                    34
#define BRCMF_E_RESET_COMPLETE                  35
#define BRCMF_E_JOIN_START                      36
#define BRCMF_E_ROAM_START                      37
#define BRCMF_E_ASSOC_START                     38
#define BRCMF_E_IBSS_ASSOC                      39
#define BRCMF_E_RADIO                           40
#define BRCMF_E_PSM_WATCHDOG                    41
#define BRCMF_E_PROBREQ_MSG                     44
#define BRCMF_E_SCAN_CONFIRM_IND                45
#define BRCMF_E_PSK_SUP                         46
#define BRCMF_E_COUNTRY_CODE_CHANGED            47
#define BRCMF_E_EXCEEDED_MEDIUM_TIME            48
#define BRCMF_E_ICV_ERROR                       49
#define BRCMF_E_UNICAST_DECODE_ERROR            50
#define BRCMF_E_MULTICAST_DECODE_ERROR          51
#define BRCMF_E_TRACE                           52
#define BRCMF_E_IF                              54
#define BRCMF_E_P2P_DISC_LISTEN_COMPLETE        55
#define BRCMF_E_RSSI                            56
#define BRCMF_E_EXTLOG_MSG                      58
#define BRCMF_E_ACTION_FRAME                    59
#define BRCMF_E_ACTION_FRAME_COMPLETE           60
#define BRCMF_E_PRE_ASSOC_IND                   61
#define BRCMF_E_PRE_REASSOC_IND                 62
#define BRCMF_E_CHANNEL_ADOPTED                 63
#define BRCMF_E_AP_STARTED                      64
#define BRCMF_E_DFS_AP_STOP                     65
#define BRCMF_E_DFS_AP_RESUME                   66
#define BRCMF_E_ESCAN_RESULT                    69
#define BRCMF_E_ACTION_FRAME_OFF_CHAN_COMPLETE  70
#define BRCMF_E_PROBERESP_MSG                   71
#define BRCMF_E_P2P_PROBEREQ_MSG                72
#define BRCMF_E_DCS_REQUEST                     73
#define BRCMF_E_FIFO_CREDIT_MAP                 74
#define BRCMF_E_ACTION_FRAME_RX                 75
#define BRCMF_E_TDLS_PEER_EVENT                 92
#define BRCMF_E_BCMC_CREDIT_SUPPORT             127
#define BRCMF_E_LAST                            139

struct WiFiNetwork;

void StartPacketReceiver(struct SDIO *sdio);
int PacketSetVar(struct SDIO *sdio, char *varName, const void *setBuffer, int setSize);
int PacketSetVarInt(struct SDIO *sdio, char *varName, ULONG varValue);
int PacketCmdInt(struct SDIO *sdio, ULONG cmd, ULONG cmdValue);
int PacketCmdIntGet(struct SDIO *sdio, ULONG cmd, ULONG *cmdValue);
int PacketCmdGet(struct SDIO *sdio, ULONG cmd, ULONG *cmdValue);
void PacketSetVarAsync(struct SDIO *sdio, char *varName, const void *setBuffer, int setSize);
void PacketSetVarIntAsync(struct SDIO *sdio, char *varName, ULONG varValue);
void PacketCmdIntAsync(struct SDIO *sdio, ULONG cmd, ULONG cmdValue);
int PacketGetVar(struct SDIO *sdio, char *varName, void *getBuffer, int getSize);
void StartNetworkScan(struct IOSana2Req *io);
int PacketUploadCLM(struct SDIO *sdio);
int Connect(struct SDIO *sdio, struct WiFiNetwork *network);
int SendDataPacket(struct SDIO *sdio, struct IOSana2Req *io);

#ifndef	PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif

enum JoinPrefTypes {
    JOIN_PREF_RSSI = 1,
    JOIN_PREF_WPA,
    JOIN_PREF_BAND,
    JOIN_PREF_RSSI_DELTA
};

struct JoinPrefParams {
    UBYTE jp_Type;
    UBYTE jp_Len;
    UBYTE jp_RSSIGain;
    UBYTE jp_Band;
};

/* scan params for extended join */
struct JoinScanParams {
    UBYTE js_ScanYype;          /* 0 use default, active or passive scan */
    UBYTE PAD[3];
    ULONG js_NProbes;           /* -1 use default, nr of probes per channel */
    ULONG js_ActiveTime;        /* -1 use default, dwell time per channel for active scanning */
    ULONG js_PassiveTime;       /* -1 use default, dwell time per channel for passive scanning */
    ULONG js_HomeTime;          /* -1 use default, dwell time for the home channel between channel scans */
};

/* used for association with a specific BSSID and chanspec list */
struct AssocParams {
    UBYTE ap_BSSID[6];          /* 00:00:00:00:00:00: broadcast scan */
    ULONG ap_ChanspecNum;       /* 0: all available channels, otherwise count of chanspecs in chanspec_list */
    UWORD ap_ChanSpecList[1];   /* list of chanspecs */
};

struct SSID {
    ULONG   ssid_Length;
    UBYTE   ssid_Value[32];
};

/* extended join params */
struct ExtJoinParams {
    struct SSID             ej_SSID;   /* {0, ""}: wildcard scan */
    struct JoinScanParams   ej_Scan;
    struct AssocParams      ej_Assoc;
};

/* join params */
struct JoinParams {
    struct SSID             j_SSID;
    struct AssocParams      j_Assoc;
};

#endif /* _PACKET_H */
