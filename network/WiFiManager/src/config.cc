#include <dos/dos.h>
#include <config.h>

#if !defined(__INTELLISENSE__)
#include <proto/dos.h>
#include <proto/exec.h>
#else
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>
#endif

#include <tinystd/string_view>
using namespace tinystd::string_view_literals;

ConfigFile * ConfigFile::Create()
{
    char buffer[4];

    Printf("Loading config\n");

    /* Get the variable into a too small buffer. Required length will be returned in IoErr() */
    if (GetVar("SYS/Wireless.prefs", buffer, 4, GVF_BINARY_VAR) > 0)
    {
        ULONG requiredLength = IoErr();

        ConfigFile * cfg = new ConfigFile();
        if (cfg != NULL)
        {
            cfg->m_Config.resize(requiredLength);
            GetVar("SYS/Wireless.prefs", cfg->m_Config.data(), requiredLength + 1, GVF_BINARY_VAR);

            if (cfg->ParseConfig())
            {
                return cfg;
            }

            delete cfg;
        }

        return nullptr;
    }

    return nullptr;
}

ConfigFile::ConfigFile() : m_Networks()
{

}

int get_int(tinystd::string_view &s)
{
    bool negative = false;
    int value = 0;

    if (s.front() == '-') {
        negative = true;
        s.remove_prefix(1);
    }

    while(!s.empty()) {
        unsigned c = s.front() - '0';
        if (c <= '9') {
            value = value * 10 + c;
            s.remove_prefix(1);
        }
        else break;
    }

    if (negative) return -value;
    else return value;
}

bool ConfigFile::ParseConfig()
{
    // One shall work on string_view class here...
    tinystd::string_view config = m_Config;
    constexpr tinystd::string_view startToken = "network={"sv;
    constexpr tinystd::string_view noneToken = "NONE"sv;
    constexpr tinystd::string_view wpapskToken = "WPA-PSK"sv;
    constexpr tinystd::string_view wpapskshaToken = "WPA-PSK-SHA256"sv;
    constexpr tinystd::string_view rsnToken = "RSN"sv;
    constexpr tinystd::string_view wpa2Token = "WPA2"sv;
    constexpr tinystd::string_view wpaToken = "WPA"sv;
    constexpr tinystd::string_view ccmpToken = "CCMP"sv;
    constexpr tinystd::string_view tkipToken = "TKIP"sv;
    constexpr tinystd::string_view wep104Token = "WEP104"sv;
    constexpr tinystd::string_view wep40Token = "WEP40"sv;
    Network net;
    ULONG error_line = 1;
    tinystd::string error_code = "";

    Printf("ConfigFile::ParseConfig()\n");

    enum class State
    {
        S_WAITING_FOR_START,
        S_WAITING_FOR_KEY,
        S_SSID,
        S_KEY_MGMT,
        S_PSK,
        S_SCAN_SSID,
        S_PRIORITY,
        S_DISABLED,
        S_BSSID,
        S_PAIRWISE,
        S_GROUP,
        S_PROTO,
        S_ERROR,
    } state = State::S_WAITING_FOR_START;
    State next;

    struct Token
    {
        tinystd::string_view token;
        State target;

        constexpr int strlen(const char *s)
        {
            int len = 0;
            while (*s++)
                len++;
            return len;
        }
        // Token(const Token &) = delete;
        // Token(Token &&) = delete;
        constexpr explicit Token(const char *tok, State targ) : token(tok), target(targ)
        {
        }
        constexpr explicit Token() : target(State::S_ERROR)
        {
        }
    } tokens[] = {
        Token("ssid=", State::S_SSID),
        Token("scan_ssid=", State::S_SCAN_SSID),
        Token("bssid=", State::S_BSSID),
        Token("disabled=", State::S_DISABLED),
        Token("priority=", State::S_PRIORITY),
        Token("psk=", State::S_PSK),
        Token("key_mgmt=", State::S_KEY_MGMT),
        Token("pairwise=", State::S_PAIRWISE),
        Token("group=", State::S_GROUP),
        Token("proto=", State::S_PROTO),
        Token()};

    while (config.length() > 0 && state != State::S_ERROR)
    {
        bool processState = true;

        // Skip empty lines and whitespace, but only if waiting for key or start
        if ((state == State::S_WAITING_FOR_KEY || state == State::S_WAITING_FOR_START))
        {
            // Skip enter characters
            while (config.starts_with('\n')) { error_line++; config.remove_prefix(1); }

            // Whitespace - just skipping it
            if (config.starts_with('\n') || config.starts_with('\r') || config.starts_with(' ') || config.starts_with('\t'))
            {
                Printf("Removed white space, char %02lx\n", (ULONG)config[0]);
                config.remove_prefix(1);
                processState = false;
            }
            // Comment - skip all until end of the line
            if (config.starts_with('#'))
            {
                while (!config.starts_with('\n') && config.length() > 0)
                {
                    config.remove_prefix(1);
                }
                Printf("Removed comment\n");

                processState = false;
            }
        }
        
        if(processState)
        {
            switch (state)
            {
                case State::S_WAITING_FOR_START:
                    if (config.starts_with(startToken))
                    {
                        Printf("going to S_WAITING_FOR_KEY\n");
                        net.Clear();
                        state = State::S_WAITING_FOR_KEY;
                        config.remove_prefix(startToken.length());
                        Printf(config.data());
                        Printf("StartToken length: %ld\n", startToken.length());
                    }
                    break;

                case State::S_WAITING_FOR_KEY:
                    //Printf("In state S_WAITING_FOR_KEY, string:\n");
                    //Printf(config.data());
                    //Printf("Length: %ld\n", config.length());
                    if (config.starts_with('}'))
                    {
                        bool inserted = false;

                        Printf("Inserting network\n");
                        for (auto it = m_Networks.begin(); it != m_Networks.end(); ++it)
                        {
                            if ((*it).Priority < net.Priority)
                            {
                                m_Networks.insert(it, net);
                                inserted = true;
                                break;
                            }
                        }

                        if (!inserted)
                        {
                            m_Networks.push_back(net);
                        }
                        
                        config.remove_prefix(1);
                        Printf("Network added\n");
                        state = State::S_WAITING_FOR_START;
                    }
                    else
                    {
                        int i=0;
                        while(!tokens[i].token.empty()) {
                            if (config.starts_with(tokens[i].token))
                            {
                                config.remove_prefix(tokens[i].token.length());
                                break;
                            }
                            i++;
                        }
                        state = tokens[i].target;

                        if (state == State::S_ERROR)
                        {
                            error_code = "Unknown key";
                        }
                    }
                    break;
                
                case State::S_SSID:
                    next = State::S_WAITING_FOR_KEY;

                    // If SSID is starting with double quote, consider it a string-based ssid, 
                    // otherwise it is a sequence of hexadecimal bytes
                    if (config.starts_with('"'))
                    {
                        int length;
                        for (length=1; length <= 33; length++)
                        {
                            // If double quote is found, this ends the SSID
                            if (config[length] == '"') {
                                break;
                            }
                            else if (config[length] == '\n' || config[length] == '\r' || config[length] == '\t') {
                                error_code = "Wrong character in SSID";
                                next = State::S_ERROR;
                            }
                            else {
                                net.SSID += config[length];
                            }
                        }
                        if (net.SSID.length() == 0) { error_code = "SSID empty"; next = State::S_ERROR; }
                        if (net.SSID.length() > 32) { error_code = "SSID key too long"; next = State::S_ERROR; }
                        config.remove_prefix(length + 1);
                    }
                    else
                    {
                        int length = 0;
                        while(config[length] == ' ' || config[length] == '\n' || config[length] == '\r' || config[length] == '\t')
                        {
                            char cu = 0, cd = 0;
                            char chr = 0;

                            cu = config[length++];
                            cd = config[length++];

                            if (cu >= '0' && cu <= '9') cu -= '0';
                            else if (cu >= 'a' && cu <= 'f') cu -= 'a' - 10;
                            else if (cu >= 'A' && cu <= 'F') cu -= 'A' - 10;
                            else {
                                error_code = "Wrong HEX sequence in SSID";
                                next = State::S_ERROR;
                                break;
                            }
                            if (cd >= '0' && cd <= '9') cd -= '0';
                            else if (cd >= 'a' && cd <= 'f') cd -= 'a' - 10;
                            else if (cd >= 'A' && cd <= 'F') cd -= 'A' - 10;
                            else {
                                error_code = "Wrong HEX sequence in SSID";
                                next = State::S_ERROR;
                                break;
                            }
                            chr = (cu << 4) | cd;
                            net.SSID += chr;
                            if (net.SSID.length() > 32) {
                                error_code = "SSID key too long";
                                next = State::S_ERROR;
                                break;
                            }
                        }
                        if (next == State::S_ERROR && net.SSID.length() == 0) {
                            error_code = "SSID empty";
                            next = State::S_ERROR;
                        }
                        config.remove_prefix(length);
                    }

                    state = next;
                    break;

                case State::S_PSK:
                    next = State::S_WAITING_FOR_KEY;

                    // If PSK is starting with double quote, consider it a string-based ssid, 
                    // otherwise it is a sequence of hexadecimal bytes
                    if (config.starts_with('"'))
                    {
                        int length;
                        net.AsciiPSK = true;
                        for (length=1; length <= 64; length++)
                        {
                            // If double quote is found, this ends the SSID
                            if (config[length] == '"') {
                                break;
                            }
                            else if (config[length] == '\n' || config[length] == '\r' || config[length] == '\t') {
                                error_code = "Wrong character in PSK";
                                next = State::S_ERROR;
                            }
                            else {
                                net.PSK += config[length];
                            }
                        }
                        if (net.PSK.length() < 8) { error_code = "PSK too short"; next = State::S_ERROR; }
                        if (net.PSK.length() > 63) { error_code = "PSK too long"; next = State::S_ERROR; }
                        config.remove_prefix(length + 1);
                    }
                    else
                    {
                        int length = 0;
                        net.AsciiPSK = false;
                        while(config[length] == ' ' || config[length] == '\n' || config[length] == '\r' || config[length] == '\t')
                        {
                            char cu = 0, cd = 0;
                            char chr = 0;

                            cu = config[length++];
                            cd = config[length++];

                            if (cu >= '0' && cu <= '9') cu -= '0';
                            else if (cu >= 'a' && cu <= 'f') cu -= 'a' - 10;
                            else if (cu >= 'A' && cu <= 'F') cu -= 'A' - 10;
                            else {
                                error_code = "Wrong HEX sequence in PSK";
                                next = State::S_ERROR;
                                break;
                            }
                            if (cd >= '0' && cd <= '9') cd -= '0';
                            else if (cd >= 'a' && cd <= 'f') cd -= 'a' - 10;
                            else if (cd >= 'A' && cd <= 'F') cd -= 'A' - 10;
                            else {
                                error_code = "Wrong HEX sequence in PSK";
                                next = State::S_ERROR;
                                break;
                            }
                            chr = (cu << 4) | cd;
                            net.PSK += chr;
                        }
                        if (next != State::S_ERROR && net.PSK.length() != 32) {
                            error_code = "Wrong length of binary PSK";
                            next = State::S_ERROR;
                        }
                        config.remove_prefix(length);
                    }
                    state = next;
                    break;

                case State::S_KEY_MGMT:
                    net.KeyMgmt.clear();
                    next = State::S_WAITING_FOR_KEY;

                    while(!(config.starts_with('\n') || config.starts_with('\r')))
                    {
                        /* Eat whitespace */
                        while(config.starts_with(' ') && !config.empty()) { config.remove_prefix(1); }
                        if (config.starts_with(wpapskToken) && 
                            (config[wpapskToken.length() + 1] == ' ' || 
                             config[wpapskToken.length() + 1] == '\n' ||
                             config[wpapskToken.length() + 1] == '\r'))
                        {
                            net.KeyMgmt.push_back(Network::KeyMgmt::WPA_PSK);
                            config.remove_prefix(wpapskToken.length());
                        }
                        else if (config.starts_with(wpapskshaToken) && 
                            (config[wpapskshaToken.length() + 1] == ' ' || 
                             config[wpapskshaToken.length() + 1] == '\n' ||
                             config[wpapskshaToken.length() + 1] == '\r'))
                        {
                            net.KeyMgmt.push_back(Network::KeyMgmt::WPA_PSK_SHA256);
                            config.remove_prefix(wpapskshaToken.length());
                        }
                        else if (config.starts_with(noneToken) && 
                            (config[noneToken.length() + 1] == ' ' || 
                             config[noneToken.length() + 1] == '\n' ||
                             config[noneToken.length() + 1] == '\r'))
                        {
                            net.KeyMgmt.push_back(Network::KeyMgmt::NONE);
                            config.remove_prefix(noneToken.length());
                        }
                        else
                        {
                            error_code = "Unknown key_mgmt entry";
                            next = State::S_ERROR;
                        }
                    }
                    state = next;
                    break;
                
                case State::S_PAIRWISE:
                    net.Pairwise.clear();
                    next = State::S_WAITING_FOR_KEY;

                    while(!(config.starts_with('\n') || config.starts_with('\r')))
                    {
                        /* Eat whitespace */
                        while(config.starts_with(' ') && !config.empty()) { config.remove_prefix(1); }

                        if (config.starts_with(ccmpToken) && 
                            (config[ccmpToken.length() + 1] == ' ' || 
                             config[ccmpToken.length() + 1] == '\n' ||
                             config[ccmpToken.length() + 1] == '\r'))
                        {
                            net.Pairwise.push_back(Network::Cipher::CCMP);
                            config.remove_prefix(ccmpToken.length());
                        }
                        else if (config.starts_with(tkipToken) && 
                            (config[tkipToken.length() + 1] == ' ' || 
                             config[tkipToken.length() + 1] == '\n' ||
                             config[tkipToken.length() + 1] == '\r'))
                        {
                            net.Pairwise.push_back(Network::Cipher::TKIP);
                            config.remove_prefix(tkipToken.length());
                        }
                        else if (config.starts_with(noneToken) && 
                            (config[noneToken.length() + 1] == ' ' || 
                             config[noneToken.length() + 1] == '\n' ||
                             config[noneToken.length() + 1] == '\r'))
                        {
                            net.Pairwise.push_back(Network::Cipher::NONE);
                            config.remove_prefix(noneToken.length());
                        }
                        else
                        {
                            error_code = "Unknown pairwise entry";
                            next = State::S_ERROR;
                        }
                    }
                    state = next;
                    break;

                case State::S_GROUP:
                    net.Group.clear();
                    next = State::S_WAITING_FOR_KEY;

                    while(!(config.starts_with('\n') || config.starts_with('\r')))
                    {
                        /* Eat whitespace */
                        while(config.starts_with(' ') && !config.empty()) { config.remove_prefix(1); }

                        if (config.starts_with(ccmpToken) && 
                            (config[ccmpToken.length() + 1] == ' ' || 
                             config[ccmpToken.length() + 1] == '\n' ||
                             config[ccmpToken.length() + 1] == '\r'))
                        {
                            net.Group.push_back(Network::Cipher::CCMP);
                            config.remove_prefix(ccmpToken.length());
                        }
                        else if (config.starts_with(tkipToken) && 
                            (config[tkipToken.length() + 1] == ' ' || 
                             config[tkipToken.length() + 1] == '\n' ||
                             config[tkipToken.length() + 1] == '\r'))
                        {
                            net.Group.push_back(Network::Cipher::TKIP);
                            config.remove_prefix(tkipToken.length());
                        }
                        else if (config.starts_with(wep104Token) && 
                            (config[wep104Token.length() + 1] == ' ' || 
                             config[wep104Token.length() + 1] == '\n' ||
                             config[wep104Token.length() + 1] == '\r'))
                        {
                            net.Group.push_back(Network::Cipher::WEP104);
                            config.remove_prefix(wep104Token.length());
                        }
                        else if (config.starts_with(wep40Token) && 
                            (config[wep40Token.length() + 1] == ' ' || 
                             config[wep40Token.length() + 1] == '\n' ||
                             config[wep40Token.length() + 1] == '\r'))
                        {
                            net.Group.push_back(Network::Cipher::WEP40);
                            config.remove_prefix(wep40Token.length());
                        }
                        else
                        {
                            error_code = "Unknown group entry";
                            next = State::S_ERROR;
                        }
                    }
                    state = next;
                    break;

                case State::S_PROTO:
                    net.Proto.clear();
                    next = State::S_WAITING_FOR_KEY;

                    while(!(config.starts_with('\n') || config.starts_with('\r')))
                    {
                        /* Eat whitespace */
                        while(config.starts_with(' ') && !config.empty()) { config.remove_prefix(1); }

                        if (config.starts_with(rsnToken) && 
                            (config[rsnToken.length() + 1] == ' ' || 
                             config[rsnToken.length() + 1] == '\n' ||
                             config[rsnToken.length() + 1] == '\r'))
                        {
                            net.Proto.push_back(Network::Proto::RSN);
                            config.remove_prefix(rsnToken.length());
                        }
                        else if (config.starts_with(wpa2Token) && 
                            (config[wpa2Token.length() + 1] == ' ' || 
                             config[wpa2Token.length() + 1] == '\n' ||
                             config[wpa2Token.length() + 1] == '\r'))
                        {
                            net.Proto.push_back(Network::Proto::RSN);
                            config.remove_prefix(wpa2Token.length());
                        }
                        else if (config.starts_with(wpaToken) && 
                            (config[wpaToken.length() + 1] == ' ' || 
                             config[wpaToken.length() + 1] == '\n' ||
                             config[wpaToken.length() + 1] == '\r'))
                        {
                            net.Proto.push_back(Network::Proto::WPA);
                            config.remove_prefix(wpaToken.length());
                        }
                        else
                        {
                            error_code = "Unknown group entry";
                            next = State::S_ERROR;
                        }
                    }
                    state = next;
                    break;

                case State::S_SCAN_SSID:
                    /* Eat whitespace */
                    while(config.starts_with(' ') && !config.empty()) { config.remove_prefix(1); }
                    if (get_int(config) != 0)
                    {
                        net.ScanSSID = true;
                    }
                    state = State::S_WAITING_FOR_KEY;
                    break;

                case State::S_DISABLED:
                    /* Eat whitespace */
                    while(config.starts_with(' ') && !config.empty()) { config.remove_prefix(1); }
                    if (get_int(config) != 0) {
                        net.Disabled = true;
                    }
                    state = State::S_WAITING_FOR_KEY;
                    break;

                case State::S_BSSID:
                    /* Eat whitespace */
                    while(config.starts_with(' ') && !config.empty()) { config.remove_prefix(1); }
                    
                    state = State::S_WAITING_FOR_KEY;
                    break;

                case State::S_PRIORITY:
                    /* Eat whitespace */
                    while(config.starts_with(' ') && !config.empty()) { config.remove_prefix(1); }
                    net.Priority = get_int(config);
                    state = State::S_WAITING_FOR_KEY;
                    break;

                default:
                    break;
            }
        }
    }

    if (state == State::S_ERROR)
    {
        Printf("ERROR while parsing wireless.conf at line %ld, reason '%s'\n",
            error_line, (ULONG)error_code.c_str());

        return false;
    }
    else {
        Printf("Parsing successful\n");
        return true;
    }
}
