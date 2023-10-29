#include <exec/types.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/classes.h>
#include <workbench/startup.h>
#include <graphics/gfxbase.h>
#include <graphics/gfx.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/devicetree.h>
#include <proto/mathieeesingbas.h>
#include <clib/muimaster_protos.h>
#include <clib/alib_protos.h>
#include <utility/tagitem.h>
#include <stdarg.h>
#include <stdint.h>

#include "messages.h"

#define FLOAT ULONG
#define F_1000  (0x447a0000)

int main(int);

/* Startup code including workbench message support */
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

    ret = main(wbmsg ? 1 : 0);

    if (wbmsg)
    {
        Forbid();
        ReplyMsg((struct Message *)wbmsg);
    }

    return ret;
}

struct MsgPort *        vc4Port;
struct MsgPort *        replyPort;
struct Library *        MUIMasterBase;
struct ExecBase *       SysBase;
struct IntuitionBase *  IntuitionBase;
struct GfxBase *        GfxBase;
struct DosLibrary *     DOSBase;
struct MathIEEEBase *   MathIeeeSingBasBase;

static const char version[] __attribute__((used)) = "$VER: " VERSION_STRING;

Object *app, *MainWindow, *NoInterp, *AGC, *Kernel, *KernB, *KernC, *Phase;

ULONG UpdateKernel()
{
    struct VC4Msg cmd;
    cmd.msg.mn_Length = sizeof(cmd);
    cmd.msg.mn_ReplyPort = replyPort;
    ULONG value;

    get(Kernel, MUIA_Selected, &value);
    
    if (value)
        cmd.SetKernel.kernel = 1;
    else
        cmd.SetKernel.kernel = 0;

    get(KernB, MUIA_Numeric_Value, &value);
    cmd.SetKernel.b = IEEESPDiv(IEEESPFlt(value), F_1000);

    get(KernC, MUIA_Numeric_Value, &value);
    cmd.SetKernel.c = IEEESPDiv(IEEESPFlt(value), F_1000);

    cmd.cmd = VCMD_SET_KERNEL;
    PutMsg(vc4Port, &cmd.msg);
    WaitPort(replyPort);
    GetMsg(replyPort);

    return 0;
}

ULONG UpdateScaler()
{
    struct VC4Msg cmd;
    cmd.msg.mn_Length = sizeof(cmd);
    cmd.msg.mn_ReplyPort = replyPort;
    ULONG value;

    get(Phase, MUIA_Numeric_Value, &value);
    
    cmd.cmd = VCMD_SET_PHASE;
    cmd.SetPhase.val = value;
    PutMsg(vc4Port, &cmd.msg);
    WaitPort(replyPort);
    GetMsg(replyPort);

    get(AGC, MUIA_Selected, &value);
    cmd.SetScaler.val = value ? 1 : 0;

    get(NoInterp, MUIA_Selected, &value);
    if (value)
        cmd.SetScaler.val |= 2;

    cmd.cmd = VCMD_SET_SCALER;
    PutMsg(vc4Port, &cmd.msg);
    WaitPort(replyPort);
    GetMsg(replyPort);

    return 0;
}

struct Hook hook_Kernel = {
    .h_Entry = UpdateKernel
};

struct Hook hook_Scaler = {
    .h_Entry = UpdateScaler
};

void MUIMain()
{
    app = ApplicationObject, 
            MUIA_Application_Title, (ULONG)"VC4 Tweak",
            MUIA_Application_Version, (ULONG)version,
            MUIA_Application_Copyright, (ULONG)"(C) 2022 Michal Schulz",
            MUIA_Application_Author, (ULONG)"Michal Schulz",
            MUIA_Application_Description, (ULONG)"VC4 Tweak",
            MUIA_Application_Base, (ULONG)"VC4TWEAK",
            MUIA_Application_SingleTask, TRUE,

            SubWindow, MainWindow = WindowObject,
                MUIA_Window_Title, (ULONG)"VC4 Tweak",
                WindowContents, VGroup,
                    Child, HGroup,
                        Child, NoInterp = MUI_MakeObject(MUIO_Button, "8Phase"),
                        Child, AGC = MUI_MakeObject(MUIO_Button, "AGC"),
                        Child, Kernel = MUI_MakeObject(MUIO_Button, "Kernel"),
                    End,
                    Child, VGroup,
                        Child, ColGroup(2),
                            Child, Label("Phase:"),
                            Child, Phase = SliderObject,
                                MUIA_Numeric_Min, 0,
                                MUIA_Numeric_Max, 255,
                                MUIA_Numeric_Value, 128,
                            End,
                            Child, Label("Kernel B:"),
                            Child, KernB = SliderObject,
                                MUIA_Numeric_Min, 0,
                                MUIA_Numeric_Max, 1000,
                                MUIA_Numeric_Value, 1,
                            End,
                            Child, Label("Kernel C:"),
                            Child, KernC = SliderObject,
                                MUIA_Numeric_Min, 0,
                                MUIA_Numeric_Max, 1000,
                                MUIA_Numeric_Value, 1,
                            End,
                        End,
                    End,
                End,
            End,
        End;
    
    if (app)
    {
        ULONG isOpen;
        APTR ssp;
        ULONG tmp, cacr, thresh, debug_low, debug_high, debug_ctrl;
        struct VC4Msg cmd;

        DoMethod(MainWindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
            (ULONG)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
        
        DoMethod(app, MUIM_Notify, MUIA_Application_DoubleStart, MUIV_EveryTime,
            (ULONG)app, 3, MUIM_Set, MUIA_Application_Iconified, FALSE);

        DoMethod(app, MUIM_Notify, MUIA_Application_DoubleStart, MUIV_EveryTime,
            (ULONG)MainWindow, 1, MUIM_Window_ToFront);

        set(NoInterp, MUIA_InputMode, MUIV_InputMode_Toggle);
        set(AGC, MUIA_InputMode, MUIV_InputMode_Toggle);
        set(Kernel, MUIA_InputMode, MUIV_InputMode_Toggle);

        if (vc4Port)
        {         
            cmd.msg.mn_ReplyPort = replyPort;
            cmd.msg.mn_Length = sizeof(struct VC4Msg);

            cmd.cmd = VCMD_GET_PHASE;
            PutMsg(vc4Port, &cmd.msg);
            WaitPort(replyPort);
            GetMsg(replyPort);
            set(Phase, MUIA_Numeric_Value, cmd.GetPhase.val);

            cmd.cmd = VCMD_GET_SCALER;
            PutMsg(vc4Port, &cmd.msg);
            WaitPort(replyPort);
            GetMsg(replyPort);

            if (cmd.GetScaler.val & 1)
                set(AGC, MUIA_Selected, TRUE);
            
            if (cmd.GetScaler.val & 2)
                set(NoInterp, MUIA_Selected, TRUE);
            
            cmd.cmd = VCMD_GET_KERNEL;
            PutMsg(vc4Port, &cmd.msg);
            WaitPort(replyPort);
            GetMsg(replyPort);

            if (cmd.GetKernel.kernel)
                set(Kernel, MUIA_Selected, TRUE);
            
            set(KernB, MUIA_Numeric_Value, IEEESPFix(IEEESPMul(F_1000, cmd.GetKernel.b)));
            set(KernC, MUIA_Numeric_Value, IEEESPFix(IEEESPMul(F_1000, cmd.GetKernel.c)));
        }

        DoMethod(Kernel, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            (ULONG)app, 2, MUIM_CallHook, &hook_Kernel);
        DoMethod(KernB, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
            (ULONG)app, 2, MUIM_CallHook, &hook_Kernel);
        DoMethod(KernC, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
            (ULONG)app, 2, MUIM_CallHook, &hook_Kernel);

        DoMethod(Phase, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
            (ULONG)app, 2, MUIM_CallHook, &hook_Scaler);
        DoMethod(AGC, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            (ULONG)app, 2, MUIM_CallHook, &hook_Scaler);
        DoMethod(NoInterp, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
            (ULONG)app, 2, MUIM_CallHook, &hook_Scaler);

        set(MainWindow, MUIA_Window_Open, TRUE);
        get(MainWindow, MUIA_Window_Open, &isOpen);
        if (isOpen) {
            ULONG signals = 0L;

            while(DoMethod(app, MUIM_Application_NewInput, &signals) != MUIV_Application_ReturnID_Quit)
            {
                if(signals != 0)
                {
                    signals = Wait(signals | SIGBREAKF_CTRL_C);
                    if(signals & SIGBREAKF_CTRL_C)
                        break;
                }
            }
            set(MainWindow, MUIA_Window_Open, FALSE);
        }
        MUI_DisposeObject(app);
    }
}

int main(int wb)
{
    SysBase = *(struct ExecBase **)4;

    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37);
    if (DOSBase == NULL)
        return -1;

    vc4Port = FindPort("VideoCore");
    replyPort = CreateMsgPort();

    if (vc4Port == NULL) {
        Printf("Cannot find VideoCore port. Is the driver running?\n");
    }

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (IntuitionBase != NULL)
    {
        GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37);
        if (GfxBase != NULL)
        {
            MathIeeeSingBasBase = (struct MathIEEEBase *)OpenLibrary("mathieeesingbas.library", 0);
            if (MathIeeeSingBasBase != NULL)
            {
                MUIMasterBase = OpenLibrary("muimaster.library", 0);
                if (MUIMasterBase != NULL)
                {
                    MUIMain();
                    CloseLibrary(MUIMasterBase);
                }
                CloseLibrary((struct Library *)MathIeeeSingBasBase);
            }
            CloseLibrary((struct Library *)GfxBase);
        }
        CloseLibrary((struct Library *)IntuitionBase);
    }

    DeleteMsgPort(replyPort);

    return 0;
}
