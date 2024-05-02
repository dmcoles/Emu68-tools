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

bool ConfigFile::ParseConfig()
{
    // One shall work on string_view class here...
    tinystd::string_view config = m_Config;
    const tinystd::string_view startToken = "network={"_sv;
    Network net;
    ULONG error_line = 1;
    tinystd::string error_code;

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
        S_ERROR,
    } state = State::S_WAITING_FOR_START;

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
        Token()
    };

    while (config.length() > 0 && state != State::S_ERROR)
    {
        bool processState = true;

        // Skip empty lines and whitespace, but only if waiting for key or start
        if ((state == State::S_WAITING_FOR_KEY || state == State::S_WAITING_FOR_START))
        {
            if (config.starts_with('\n')) error_line++;

            // Whitespace - just skipping it
            if (config.starts_with('\n') || config.starts_with('\r') || config.starts_with(' ') || config.starts_with('\t'))
            {
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
                        net.Clear();
                        state = State::S_WAITING_FOR_KEY;
                        config.remove_prefix(startToken.length());
                    }
                    break;

                case State::S_WAITING_FOR_KEY:
                    if (config.starts_with('}'))
                    {
                        bool inserted = false;

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
                    state = State::S_WAITING_FOR_KEY;
                    break;

                case State::S_PSK:
                    state = State::S_WAITING_FOR_KEY;
                    break;

                case State::S_KEY_MGMT:
                    state = State::S_WAITING_FOR_KEY;
                    break;
                
                case State::S_PRIORITY:
                    /* Eat whitespace */
                    while(config.starts_with(' ') && !config.empty()) { config.remove_prefix(1); }
                    state = State::S_WAITING_FOR_KEY;
                    break;

                default:
                    break;
            }
        }
    }

    if (state == State::S_ERROR)
    {
        Printf("ERROR while parsing wireless.conf at line %d, reason '%s'\n",
            error_line, (ULONG)error_code.c_str());

        return false;
    }
    else return true;
}
