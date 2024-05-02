#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>

#if !defined(__INTELLISENSE__)
#include <proto/exec.h>
#include <proto/dos.h>
#else
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#endif

#include "wifimanager.h"
#include "config.h"

struct ExecBase *SysBase;
//struct IntuitionBase *IntuitionBase;
//struct GfxBase *GfxBase;
//struct Library *GadToolsBase;
struct DosLibrary *DOSBase;
//struct Library *MUIMasterBase;

#define RDA_TEMPLATE "DEVICE/K/A,SCAN/S,JOIN/S,SSID/K,STATUS/S,WAIT/S"

enum
{
    OPT_DEVICE_NAME,
    OPT_SCAN,
    OPT_JOIN,
    OPT_SSID_NAME,
    OPT_STATUS,
    OPT_WAIT,

    OPT_COUNT
};

LONG result[OPT_COUNT];

#include <config.h>

int main()
{
    const ConfigFile *config;
    bool scanNetworks = false;
    bool joinNetwork = false;
    bool getStatus = false;
    bool waitForConnect = false;
    const char *deviceName = nullptr;
    const char *joinSSID = nullptr;

    struct RDArgs *args;
    SysBase = *(struct ExecBase **)4;

    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37);
    if (DOSBase == NULL)
        return -1;

    config = ConfigFile::Create();
    if (config == nullptr)
    {
        CloseLibrary((struct Library *)DOSBase);
        return -1;
    }

    args = ReadArgs(RDA_TEMPLATE, result, NULL);

    if (args != NULL)
    {
        scanNetworks = !!result[OPT_SCAN];
        joinNetwork = !!result[OPT_JOIN];
        getStatus = !!result[OPT_STATUS];
        waitForConnect = !!result[OPT_WAIT];

        if (result[OPT_DEVICE_NAME]) {
            deviceName = (STRPTR)(result[OPT_DEVICE_NAME]);
        }

        if (result[OPT_SSID_NAME])
        {
            joinSSID = (STRPTR)(result[OPT_SSID_NAME]);
        }

        FreeArgs(args);
    }
    
    delete config;
    CloseLibrary((struct Library *)DOSBase);
    
    return 0;
}

int wbmain()
{
    SysBase = *(struct ExecBase **)4;

    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37);
    if (DOSBase == NULL)
        return -1;





    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
