/*
    Copyright 2017-2018, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <exec/tasks.h>
#include <exec/ports.h>
#include <exec/lists.h>
#include <libraries/Picasso96.h>

#include <proto/Picasso96.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/alib_protos.h>
#include <clib/timer_protos.h>

#include <math-68881.h>

#include <stdlib.h>

#include "work.h"
#include "support.h"

CONST_STRPTR version = "$VER: Buddhabrot 1.0 (03.03.2017) (C) 2017 The AROS Development Team";

struct Window * createMainWindow(int req_size)
{
    struct Screen *pubScreen;
    struct Window *displayWin = NULL;
    int width, height;

    pubScreen = LockPubScreen(0);

    if (pubScreen)
    {
        width = ((pubScreen->Width * 3) / 4);
        height = ((pubScreen->Height * 3) / 4);

        if (req_size == 0)
        {
            if (width < height)
                req_size = width;
            else
                req_size = height;
        }
    }

    if (req_size == 0)
        req_size = 200;

    if ((displayWin = OpenWindowTags(0,
                                     WA_PubScreen, (Tag)pubScreen,
                                     WA_Left, 0,
                                     WA_Top, (pubScreen) ? pubScreen->BarHeight : 10,
                                     WA_InnerWidth, req_size,
                                     WA_InnerHeight, req_size,
                                     WA_Title, (Tag) "Buddhabrot",
                                     WA_SmartRefresh, TRUE,
                                     WA_CloseGadget, TRUE,
                                     WA_DepthGadget, TRUE,
                                     WA_DragBar, TRUE,
                                     WA_SizeGadget, FALSE,
                                     WA_SizeBBottom, FALSE,
                                     WA_SizeBRight, FALSE,
                                     WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_INTUITICKS,
                                     TAG_DONE)) != NULL)
    {
        if (pubScreen)
            UnlockPubScreen(0, pubScreen);
    }

    return displayWin;
}


#define ARG_TEMPLATE "X0/K,Y0/K,SCALE/K,SIZE/K/N,SUBDIVIDE/K/N,THREADS/K/N,MAXITER/K/N,OVERSAMPLE/K/N,BUDDHA/S"

enum {
    ARG_X0,
    ARG_Y0,
    ARG_SCALE,
    ARG_SIZE,
    ARG_SUBDIVIDE,
    ARG_MAXCPU,
    ARG_MAXITER,
    ARG_OVERSAMPLE,
    ARG_BUDDHA,

    ARG_COUNT
};

struct TimerBase *TimerBase = NULL;

struct RedrawMessage {
    struct RenderInfo * rinfo;
    struct Window *     window;
    ULONG               width;
    ULONG               height;
    struct Library *    pbase;
    ULONG *             workBuffer;
};

void redrawTaskMain(struct RedrawMessage *rm)
{
    struct Library *P96Base = rm->pbase;
    ULONG sigset;
    ULONG *LUT = AllocVec(sizeof(ULONG) * (MAX_LUT + 1), MEMF_ANY);

    for (int i=0; i <= MAX_LUT; i++)
    {
        const double gamma_r = 1.2;
        const double gamma_g = 0.95;
        const double gamma_b = 1.0;
        const double pre_r = 96.0;
        const double pre_g = 128.0;
        const double pre_b = 256.0;
        double rgb = i / 256.0;

        ULONG r=256.0*pow(rgb, gamma_r) * pre_r;
        ULONG g=256.0*pow(rgb, gamma_g) * pre_g;
        ULONG b=256.0*pow(rgb, gamma_b) * pre_b;

        b = (b + 255) / 512;
        r = (r + 255) / 512;
        g = (g + 255) / 512;

        if (b > 255) b = 255;
        if (r > 255) r = 255;
        if (g > 255) g = 255;

        ULONG c = 0xff | (r << 8) | (g << 16) | (b << 24);
        LUT[i] = c;
    }

    p96WritePixelArray(rm->rinfo, 0, 0, rm->window->RPort, rm->window->BorderLeft, rm->window->BorderTop,
        rm->width, rm->height);

    while((sigset = Wait(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D)) != 0)
    {
        if (sigset & SIGBREAKF_CTRL_D)
        {
            ULONG *rgba = rm->rinfo->Memory;

            for (ULONG i = 0; i < rm->width * rm->height; i++)
            {
                ULONG c = LUT[rm->workBuffer[i]];
                rgba[i] = c;
            }

            p96WritePixelArray(rm->rinfo, 0, 0, rm->window->RPort, rm->window->BorderLeft, rm->window->BorderTop,
                rm->width, rm->height);
        }

        if (sigset & SIGBREAKF_CTRL_C)
            break;
    }

    FreeVec(LUT);
}

APTR memPool;

int main()
{
    struct SignalSemaphore lock;
    struct Task *masterTask = NULL;
    struct Task *redrawTask = NULL;
    struct MsgPort *masterPort = NULL;
    struct MsgPort *mainPort = NULL;
    struct MyMessage *cmd;

    struct RDArgs *rda;
    struct Library *P96Base;
    struct Window *displayWin;

    ULONG args[ARG_COUNT] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    ULONG coreCount = 1;
    ULONG maxWork = 0;
    ULONG oversample = 0;
    ULONG subdivide = 0;
    ULONG req_size = 0;
    ULONG type = TYPE_NORMAL;

    double x0 = 0.0;
    double y0 = 0.0;
    double size = 4.0;

    InitSemaphore(&lock);
    memPool = CreatePool(MEMF_ANY | MEMF_CLEAR, 8192, 4096);

    struct MsgPort *timerPort = CreateMsgPort();
    struct timerequest *tr = CreateIORequest(timerPort, sizeof(struct timerequest));

    struct timeval start_time;
    struct timeval last_redraw;
    struct timeval now;

    P96Base = OpenLibrary("Picasso96API.library", 0);
    if (P96Base == NULL) {
        Printf("Failed to open Picasso96API.library\n");
        return -1;
    }

    if (timerPort)
    {
        FreeSignal(timerPort->mp_SigBit);
        timerPort->mp_SigBit = -1;
        timerPort->mp_Flags = PA_IGNORE;
    }

    if (tr)
    {
        if (!OpenDevice("timer.device", UNIT_VBLANK, (struct IORequest *)tr, 0))
        {
            TimerBase = (struct TimerBase *)tr->tr_node.io_Device;
        }
    } else return 0;

    rda = ReadArgs(ARG_TEMPLATE, args, NULL);
    if (rda != NULL)
    {
        LONG *ptr = (LONG *)args[ARG_MAXCPU];
        if (ptr) {
            coreCount = *ptr;
            if (coreCount == 0)
                coreCount = 1;
        }

        ptr = (LONG *)args[ARG_SUBDIVIDE];
        if (ptr) {
            subdivide = *ptr;
            if (subdivide == 0)
                subdivide = 1;
        }

        ptr = (LONG *)args[ARG_SIZE];
        if (ptr)
            req_size = *ptr;

        ptr = (LONG *)args[ARG_MAXITER];
        if (ptr)
        {
            maxWork = *ptr;
            if (maxWork < 2)
                maxWork = 2;
            else if (maxWork > 10000000)
                maxWork = 10000000;
        }

        ptr = (LONG *)args[ARG_OVERSAMPLE];
        if (ptr)
        {
            oversample = *ptr;
            if (oversample < 1)
                oversample = 1;
            else if (oversample > 10)
                oversample = 10;
        }

        if (args[ARG_BUDDHA])
            type = TYPE_BUDDHA;

        if (args[ARG_X0])
            x0 = atof((char *)args[ARG_X0]);
        
        if (args[ARG_Y0])
            y0 = atof((char *)args[ARG_Y0]);

        if (args[ARG_SCALE])
            size = atof((char *)args[ARG_SCALE]);

        if (size == 0.0)
            size = 4.0;
    }

    if (type == TYPE_BUDDHA)
    {
        if (maxWork == 0)
            maxWork = 4000;
        if (oversample == 0)
            oversample = 4;
    }
    else
    {
        if (maxWork == 0)
            maxWork = 1000;
        if (oversample == 0)
            oversample = 1;
    }

    mainPort = CreateMsgPort();

    SetTaskPri(FindTask(NULL), 5);

    masterTask = NewCreateTask( TASKTAG_NAME,       (Tag)"Buddha Master",
                                TASKTAG_PRI,        2,
                                TASKTAG_PC,         (Tag)SMPTestMaster,
                                TASKTAG_ARG1,       (Tag)mainPort,
                                TASKTAG_STACKSIZE,  65536,
                                TAG_DONE);

    Printf("Waiting for master to wake up\n");

    struct Message *msg = WaitPort(mainPort);
    masterPort = msg->mn_ReplyPort;

    Printf("Master alive\n");

    displayWin = createMainWindow(req_size);

    if (displayWin)
    {
        int width, height;
        BOOL windowClosing = FALSE;
        BOOL renderingDone = FALSE;
        ULONG signals;
        ULONG *workBuffer;
        ULONG *rgba;

        width = (displayWin->Width - displayWin->BorderLeft - displayWin->BorderRight);
        height = (displayWin->Height - displayWin->BorderTop - displayWin->BorderBottom);

        if (subdivide == 0)
            subdivide = width * width / 5000;
        if (subdivide == 0)
            subdivide = 1;

        workBuffer = AllocPooled(memPool, width * height * sizeof(ULONG));
        rgba = AllocPooled(memPool, width * height * sizeof(ULONG));

        if (workBuffer && rgba)
        {
            struct RedrawMessage rm;
            struct RenderInfo ri;
            ri.Memory = rgba;
            ri.BytesPerRow = width * sizeof(ULONG);
            ri.RGBFormat = RGBFB_B8G8R8A8;

            rm.rinfo = &ri;
            rm.window = displayWin;
            rm.width = width;
            rm.height = height;
            rm.pbase = P96Base;
            rm.workBuffer = workBuffer;

            redrawTask = NewCreateTask(
                    TASKTAG_NAME,       (Tag)"Buddha redraw",
                    TASKTAG_PC,         (Tag)redrawTaskMain,
                    TASKTAG_PRI,        0,
                    TASKTAG_ARG1,       (Tag)&rm,
                    TASKTAG_STACKSIZE,  65536,
                    TAG_DONE);

            Printf("Sending startup message to master\n");

            cmd = AllocPooled(memPool, sizeof(struct MyMessage));
            cmd->mm_Type = MSG_STARTUP;
            cmd->mm_Message.mn_ReplyPort = mainPort;
            cmd->mm_Message.mn_Length = sizeof(struct MyMessage);
            cmd->mm_Body.Startup.width = width;
            cmd->mm_Body.Startup.height = height;
            cmd->mm_Body.Startup.threadCount = coreCount;
            cmd->mm_Body.Startup.subdivide = subdivide;
            cmd->mm_Body.Startup.maxIterations = maxWork;
            cmd->mm_Body.Startup.oversample = oversample;
            cmd->mm_Body.Startup.type = type;
            cmd->mm_Body.Startup.x0 = x0;
            cmd->mm_Body.Startup.y0 = y0;
            cmd->mm_Body.Startup.size = size;
            cmd->mm_Body.Startup.workBuffer = workBuffer;
            cmd->mm_Body.Startup.writeLock = &lock;
            cmd->mm_Body.Startup.redrawTask = redrawTask;

            PutMsg(masterPort, &cmd->mm_Message);
            WaitPort(mainPort);
            GetMsg(mainPort);

            GetSysTime(&start_time);
            last_redraw = start_time;

            while (!(windowClosing && renderingDone) && ((signals = Wait((1 << displayWin->UserPort->mp_SigBit) | (1 << mainPort->mp_SigBit))) != 0))
            {
                if (signals & (1 << mainPort->mp_SigBit))
                {
                    while ((cmd = (struct MyMessage *)GetMsg(mainPort)))
                    {                      
                        /* Reply message, discard and continue */
                        if (cmd->mm_Message.mn_Node.ln_Type == NT_REPLYMSG)
                        {
                            FreePooled(memPool, cmd, cmd->mm_Message.mn_Length);
                            continue;
                        }

                        if (cmd->mm_Type == MSG_STATS)
                        {
                            char tmpbuf[128];

                            GetSysTime(&now);
                            SubTime(&now, &start_time);
                            if (renderingDone)
                            {
                                _sprintf(tmpbuf, "Buddhabrot finished; %d:%02d:%02d",
                                    now.tv_secs / 3600,
                                    (now.tv_secs / 60) % 60,
                                    now.tv_secs % 60);
                            }
                            else
                            {
                                _sprintf(tmpbuf, "Buddhabrot; %d threads; Packages: %d in, %d out; %d:%02d:%02d",
                                        cmd->mm_Body.Stats.tasksWork, 
                                        cmd->mm_Body.Stats.tasksIn, 
                                        cmd->mm_Body.Stats.tasksOut,
                                        now.tv_secs / 3600,
                                        (now.tv_secs / 60) % 60,
                                        now.tv_secs % 60);
                            }
                            SetWindowTitles(displayWin, tmpbuf, NULL);
                            
                            ReplyMsg(&cmd->mm_Message);
                        }
                        else if (cmd->mm_Type == MSG_DONE)
                        {
                            char tmpbuf[128];

                            FreePooled(memPool, msg, cmd->mm_Message.mn_Length);
                            GetSysTime(&now);
                            SubTime(&now, &start_time);

                            _sprintf(tmpbuf, "Buddhabrot finished; %d:%02d:%02d",
                                    now.tv_secs / 3600,
                                    (now.tv_secs / 60) % 60,
                                    now.tv_secs % 60);
                            SetWindowTitles(displayWin, tmpbuf, NULL);

                            Printf("Rendering time: %ld:%02ld:%02ld\n",
                                now.tv_secs / 3600, (now.tv_secs / 60) % 60, now.tv_secs % 60);
                            
                            Signal(redrawTask, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
                            redrawTask = NULL;
                            renderingDone = TRUE;
                            masterPort = NULL;
                        }
                    }
                }

                if (signals & (1 << displayWin->UserPort->mp_SigBit))
                {
                    struct IntuiMessage *msg;
                    while ((msg = (struct IntuiMessage *)GetMsg(displayWin->UserPort)))
                    {
                        switch(msg->Class)
                        {
                            case IDCMP_CLOSEWINDOW:
                                if (windowClosing == FALSE && masterPort != NULL)
                                {
                                    struct MyMessage *m = AllocPooled(memPool, sizeof(struct MyMessage));
                                    m->mm_Type = MSG_DIE;
                                    m->mm_Message.mn_Length = sizeof(struct MyMessage);
                                    m->mm_Message.mn_ReplyPort = mainPort;
                                    PutMsg(masterPort, &m->mm_Message);
                                }
                                windowClosing = TRUE;
                                break;
                            case IDCMP_INTUITICKS:
                                GetSysTime(&now);
                                SubTime(&now, &last_redraw);
                                if (now.tv_secs >= 5) {
                                    last_redraw = now;
                                    if (redrawTask)
                                        Signal(redrawTask, SIGBREAKF_CTRL_D);
                                }
                                break;
                        }
                        ReplyMsg((struct Message *)msg);
                    }
                }
            }
        }

        CloseWindow(displayWin);

        if (workBuffer)
            FreePooled(memPool, workBuffer, sizeof(ULONG) * width * height);
        if (rgba)
            FreePooled(memPool, rgba, sizeof(ULONG) * width * height);
    }

    DeleteMsgPort(mainPort);
    CloseLibrary(P96Base);

    DeletePool(memPool);

    return 0;
}
