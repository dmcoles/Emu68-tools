#include <exec/types.h>
#include <exec/execbase.h>

#if !defined(__INTELLISENSE__)
#include <proto/exec.h>
#include <proto/dos.h>
#else
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#endif

#include "wifimanager.h"

/*
    Startup code including workbench message support.
    IMPORTANT! This must be the **very first** file linked!
*/
int _start()
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    struct Process *p = NULL;
    struct WBStartup *wbmsg = NULL;
    int ret = 0;

    p = (struct Process *)SysBase->ThisTask;

    if (p->pr_CLI == 0)
    {
        WaitPort(&p->pr_MsgPort);
        wbmsg = (struct WBStartup *)GetMsg(&p->pr_MsgPort);
    }

    if (wbmsg == NULL)
        ret = main();
    else
        ret = wbmain();

    if (wbmsg)
    {
        Forbid();
        ReplyMsg((struct Message *)wbmsg);
    }

    return ret;
}

static const char version[] __attribute__((used)) = "$VER: " VERSION_STRING;
