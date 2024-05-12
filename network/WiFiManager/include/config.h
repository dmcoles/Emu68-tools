/*  -*- C++ -*- */
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <tinystd/string>
#include <tinystd/string_view>
#include <tinystd/list>
#include <tinystd/vector>

struct Network {
    enum struct Cipher {
        CCMP, TKIP, WEP104, WEP40, NONE
    };

    enum struct KeyMgmt {
        WPA_PSK, WPA_PSK_SHA256, WPA_EAP, WPA_EAP_SHA256, IEEE8021X, NONE
    };

    enum struct Proto {
        WPA, RSN
    };

    tinystd::string SSID;
    tinystd::string PSK;
    tinystd::vector<Cipher> Pairwise;
    tinystd::vector<Cipher> Group;
    tinystd::vector<KeyMgmt> KeyMgmt;
    tinystd::vector<Proto> Proto;
    bool            ScanSSID;
    bool            Disabled;
    bool            AsciiPSK;
    int             Priority;
    uint8_t         BSSID[6];

    void Clear() {
        SSID.clear();
        PSK.clear();
        ScanSSID = false;
        Disabled = false;
        Priority = 0;
        BSSID[0] = 0;
        BSSID[1] = 0;
        BSSID[2] = 0;
        BSSID[3] = 0;
        BSSID[4] = 0;
        BSSID[5] = 0;
        KeyMgmt = { KeyMgmt::WPA_PSK, KeyMgmt::WPA_EAP };
        Pairwise = { Cipher::CCMP, Cipher::TKIP };
        Group = { Cipher::CCMP, Cipher::TKIP, Cipher::WEP104, Cipher::WEP40};
        Proto = { Proto::WPA, Proto::RSN };
    }
};

class ConfigFile {
public:
    static ConfigFile * Create();


private:
    ConfigFile();
    bool ParseConfig();

    tinystd::string         m_Config;
    tinystd::list<Network>  m_Networks;
};

#endif /* _CONFIG_H_ */
