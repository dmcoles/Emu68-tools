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
                                     WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
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
    struct RastPort *   rport;
    struct BitMap *     bmap;
    struct Library *    pbase;
    ULONG *             workBuffer;
};

void redrawTaskMain(struct RedrawMessage *rm)
{
    struct Library *P96Base = rm->pbase;
    ULONG sigset;

    p96WritePixelArray(rm->rinfo, 0, 0, rm->window->RPort, rm->window->BorderLeft, rm->window->BorderTop,
        rm->width, rm->height);

    while((sigset = Wait(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D)) != 0)
    {
        if (sigset & SIGBREAKF_CTRL_D)
        {
            ULONG *rgba = rm->rinfo->Memory;

            for (ULONG i = 0; i < rm->width * rm->height; i++)
            {
                ULONG rgb = rm->workBuffer[i];
                ULONG r=96*rgb,g=128*rgb,b=256*rgb;

                b = (b + 255) / 512;
                r = (r + 255) / 512;
                g = (g + 255) / 512;

                if (b > 255) b = 255;
                if (r > 255) r = 255;
                if (g > 255) g = 255;

                ULONG c = 0xff | (r << 8) | (g << 16) | (b << 24);
                rgba[i] = c;
            }

            p96WritePixelArray(rm->rinfo, 0, 0, rm->window->RPort, rm->window->BorderLeft, rm->window->BorderTop,
                rm->width, rm->height);
#if 0
            BltBitMapRastPort (rm->bmap, 0, 0,
                rm->window->RPort, rm->window->BorderLeft, rm->window->BorderTop,
                rm->width, rm->height, 0xC0);  
#endif
        }

        if (sigset & SIGBREAKF_CTRL_C)
            break;
    }
}

int main()
{
    struct SignalSemaphore lock;
    struct Task *masterTask = NULL;
    struct Task *redrawTask = NULL;
    struct MsgPort *masterPort = NULL;
    struct MsgPort *mainPort = NULL;
    struct MyMessage cmd;

    struct RDArgs *rda;
    struct Library *P96Base;
    struct Window *displayWin;

    ULONG args[ARG_COUNT] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    ULONG coreCount = 1;
    ULONG maxWork = 0;
    ULONG oversample = 0;
    ULONG subdivide = 16;
    ULONG req_size = 0;
    ULONG type = TYPE_NORMAL;

    double x0 = 0.0;
    double y0 = 0.0;
    double size = 4.0;

#if 0

    ULONG signals;
    struct Screen *pubScreen;

    struct BitMap *outputBMap = NULL;
    struct MemList *coreML;
    struct SMPWorker *coreWorker;
    struct SMPWorkMessage *workMsg;
    char buffer[100];
    BOOL complete = FALSE;
#endif

    InitSemaphore(&lock);

    struct MsgPort *timerPort = CreateMsgPort();
    struct timerequest *tr = CreateIORequest(timerPort, sizeof(struct timerequest));

    struct timeval start_time;
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

    WaitPort(mainPort);
    struct Message *msg = GetMsg(mainPort);
    masterPort = msg->mn_ReplyPort;
    ReplyMsg(msg);

    Printf("Master alive\n");

    displayWin = createMainWindow(req_size);

    if (displayWin)
    {
        int width, height;
        BOOL windowClosing = FALSE;
        BOOL renderingDone = FALSE;
        ULONG signals;
        struct BitMap *outputBMap;
        ULONG *workBuffer;
        ULONG *rgba;
        struct RastPort *outBMRastPort;

        width = (displayWin->Width - displayWin->BorderLeft - displayWin->BorderRight);
        height = (displayWin->Height - displayWin->BorderTop - displayWin->BorderBottom);

        outputBMap = AllocBitMap(   width,
                                    height,
                                    GetBitMapAttr(displayWin->WScreen->RastPort.BitMap, BMA_DEPTH),
                                    BMF_DISPLAYABLE, displayWin->WScreen->RastPort.BitMap);
        
        outBMRastPort = (struct RastPort *)AllocMem(sizeof(struct RastPort), MEMF_ANY);
        workBuffer = AllocMem(width * height * sizeof(ULONG), MEMF_ANY | MEMF_CLEAR);
        rgba = AllocMem(width * height * sizeof(ULONG), MEMF_ANY | MEMF_CLEAR);

        if (outputBMap && outBMRastPort && workBuffer && rgba)
        {
            struct RedrawMessage rm;
            struct RenderInfo ri;
            ri.Memory = rgba;
            ri.BytesPerRow = width * sizeof(ULONG);
            ri.RGBFormat = RGBFB_B8G8R8A8;

            InitRastPort(outBMRastPort);
            outBMRastPort->BitMap = outputBMap;

            rm.rinfo = &ri;
            rm.window = displayWin;
            rm.width = width;
            rm.height = height;
            rm.rport = outBMRastPort;
            rm.bmap = outputBMap;
            rm.pbase = P96Base;
            rm.workBuffer = workBuffer;

            redrawTask = NewCreateTask(
                    TASKTAG_NAME,       (Tag)"Buddha redraw",
                    TASKTAG_PC,         (Tag)redrawTaskMain,
                    TASKTAG_PRI,        0,
                    TASKTAG_ARG1,       (Tag)&rm,
                    TASKTAG_STACKSIZE,  65536,
                    TAG_DONE);
#if 0
            p96WritePixelArray(&ri, 0, 0, outBMRastPort, 0, 0, width, height);

            BltBitMapRastPort (outputBMap, 0, 0,
                displayWin->RPort, displayWin->BorderLeft, displayWin->BorderTop,
                width, height, 0xC0); 
#endif
            Printf("Sending startup message to master\n");

            cmd.mm_Type = MSG_STARTUP;
            cmd.mm_Message.mn_ReplyPort = mainPort;
            cmd.mm_Message.mn_Length = sizeof(cmd);
            cmd.mm_Body.Startup.width = width;
            cmd.mm_Body.Startup.height = height;
            cmd.mm_Body.Startup.threadCount = coreCount;
            cmd.mm_Body.Startup.subdivide = subdivide;
            cmd.mm_Body.Startup.maxIterations = maxWork;
            cmd.mm_Body.Startup.oversample = oversample;
            cmd.mm_Body.Startup.type = type;
            cmd.mm_Body.Startup.x0 = x0;
            cmd.mm_Body.Startup.y0 = y0;
            cmd.mm_Body.Startup.size = size;
            cmd.mm_Body.Startup.workBuffer = workBuffer;
            cmd.mm_Body.Startup.writeLock = &lock;
            cmd.mm_Body.Startup.redrawTask = redrawTask;

            PutMsg(masterPort, &cmd.mm_Message);
            WaitPort(mainPort);
            GetMsg(mainPort);

            GetSysTime(&start_time);

            while (!(windowClosing && renderingDone) && ((signals = Wait((1 << displayWin->UserPort->mp_SigBit) | (1 << mainPort->mp_SigBit))) != 0))
            {
                if (signals & (1 << mainPort->mp_SigBit))
                {
                    struct MyMessage *msg;
                    while ((msg = (struct MyMessage *)GetMsg(mainPort)))
                    {
                        VPrintf("msg=%08lx\n", msg);
                        /* If we receive our own message, ignore it */
                        if (&cmd == msg)
                            continue;
                        
                        if (msg->mm_Message.mn_Node.ln_Type == NT_REPLYMSG)
                            continue;

                        else if (msg->mm_Type == MSG_STATS)
                        {
                            char tmpbuf[128];

                            GetSysTime(&now);
                            SubTime(&now, &start_time);
                            if (renderingDone)
                            {
                                _sprintf(tmpbuf, "Buddhabrot finished: %d:%02d:%02d",
                                    now.tv_secs / 3600,
                                    (now.tv_secs / 60) % 60,
                                    now.tv_secs % 60);
                            }
                            else
                            {
                                _sprintf(tmpbuf, "Buddhabrot (%d in work, %d waiting, %d done): %d:%02d:%02d",
                                        msg->mm_Body.Stats.tasksWork, 
                                        msg->mm_Body.Stats.tasksIn, 
                                        msg->mm_Body.Stats.tasksOut,
                                        now.tv_secs / 3600,
                                        (now.tv_secs / 60) % 60,
                                        now.tv_secs % 60);
                            }
                            SetWindowTitles(displayWin, tmpbuf, NULL);
                            
                            ReplyMsg(&msg->mm_Message);
                        }
                        else if (msg->mm_Type == MSG_DONE)
                        {
                            char tmpbuf[128];

                            FreeMem(msg, msg->mm_Message.mn_Length);
                            GetSysTime(&now);
                            SubTime(&now, &start_time);

                            _sprintf(tmpbuf, "Buddhabrot finished: %d:%02d:%02d",
                                    now.tv_secs / 3600,
                                    (now.tv_secs / 60) % 60,
                                    now.tv_secs % 60);
                            SetWindowTitles(displayWin, tmpbuf, NULL);

                            Printf("Rendering time: %ld:%02ld:%02ld\n",
                                now.tv_secs / 3600, (now.tv_secs / 60) % 60, now.tv_secs % 60);
                            
                            Signal(redrawTask, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
#if 0
                            for (ULONG i = 0; i < width * height; i++)
                            {
                                ULONG rgb = workBuffer[i];
                                ULONG r=96*rgb,g=128*rgb,b=256*rgb;

                                b = (b + 255) / 512;
                                r = (r + 255) / 512;
                                g = (g + 255) / 512;

                                if (b > 255) b = 255;
                                if (r > 255) r = 255;
                                if (g > 255) g = 255;

                                ULONG c = 0xff | (b << 8) | (g << 16) | (r << 24);
                                rgba[i] = c;
                            }

                            p96WritePixelArray(&ri, 0, 0, 
                                                outBMRastPort,
                                                0, 0,
                                                width, height);

                            BltBitMapRastPort (outputBMap, 0, 0,
                                displayWin->RPort, displayWin->BorderLeft, displayWin->BorderTop,
                                width, height, 0xC0);
#endif
                            renderingDone = TRUE;
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
                                windowClosing = TRUE;
                                {
                                    struct MyMessage *m = AllocMem(sizeof(struct MyMessage), MEMF_ANY);
                                    m->mm_Type = MSG_DIE;
                                    m->mm_Message.mn_Length = sizeof(struct MyMessage);
                                    m->mm_Message.mn_ReplyPort = mainPort;
                                    PutMsg(masterPort, &m->mm_Message);
                                }
                                break;
#if 0
                            case IDCMP_REFRESHWINDOW:
                                BeginRefresh(msg->IDCMPWindow);
                                BltBitMapRastPort (outputBMap, 0, 0,
                                    msg->IDCMPWindow->RPort, msg->IDCMPWindow->BorderLeft, msg->IDCMPWindow->BorderTop,
                                    width, height, 0xC0);
                                EndRefresh(msg->IDCMPWindow, TRUE);
                                break;
#endif
                        }
                        ReplyMsg((struct Message *)msg);
                    }
                }
            }
        }

#if 0
        while(threadCnt) {
            Printf("Waiting for threads to finis: %ld\n", threadCnt);
            Delay(50);
        } 
#endif

        CloseWindow(displayWin);

        if (workBuffer)
            FreeMem(workBuffer, sizeof(ULONG) * width * height);
        if (rgba)
            FreeMem(rgba, sizeof(ULONG) * width * height);
        if (outBMRastPort)
            FreeMem(outBMRastPort, sizeof(struct RastPort));
        if (outputBMap)
            FreeBitMap(outputBMap);
    }

#if 0


    /* Create a port that workers/masters will signal us using .. */
    if ((workMaster.smpm_MasterPort = CreateMsgPort()) == NULL)
        return 0;

    NewList(&workMaster.smpm_Workers);

    Printf("%s\n", version);
    Printf("Work Master MsgPort @ 0x%08lx\n", workMaster.smpm_MasterPort);
    Printf("SigTask = 0x%08lx\n", workMaster.smpm_MasterPort->mp_SigTask);


    InitSemaphore(&workMaster.smpm_Lock);

    Printf("Initializing workers\n");

    for (core = 0; core < max_cpus; core++)
    {
        if ((coreML = AllocMem(sizeof(struct MemList) + sizeof(struct MemEntry), MEMF_PUBLIC|MEMF_CLEAR)) != NULL)
        {
            coreML->ml_ME[0].me_Length = sizeof(struct SMPWorker);
            if ((coreML->ml_ME[0].me_Addr = AllocMem(sizeof(struct SMPWorker), MEMF_PUBLIC|MEMF_CLEAR)) != NULL)
            {
                coreWorker = coreML->ml_ME[0].me_Addr;

                coreML->ml_ME[1].me_Length = 22;
                if ((coreML->ml_ME[1].me_Addr = AllocMem(22, MEMF_PUBLIC|MEMF_CLEAR)) != NULL)
                {
                    coreML->ml_NumEntries = 2;

                    _sprintf(coreML->ml_ME[1].me_Addr, "Worker #%d", core);
                    
                    coreWorker->smpw_MasterPort = workMaster.smpm_MasterPort;
                    coreWorker->smpw_Node.ln_Type = 0;
                    coreWorker->smpw_SyncTask = FindTask(NULL);
                    coreWorker->smpw_Lock = &workMaster.smpm_Lock;
                    coreWorker->smpw_MaxWork = workMaster.smpm_MaxWork;
                    coreWorker->smpw_Oversample = workMaster.smpm_Oversample;
                    coreWorker->smpw_Task = NewCreateTask(
                                        TASKTAG_NAME,       (Tag)coreML->ml_ME[1].me_Addr,
                                        TASKTAG_PRI,        (Tag)0,
                                        TASKTAG_PC,         (Tag)SMPTestWorker,
                                        TASKTAG_STACKSIZE,  (Tag)((workMaster.smpm_MaxWork / 50000) + 1) * 40960,
                                        TASKTAG_USERDATA,   (Tag)coreWorker,
                                        TAG_DONE);

                    if (coreWorker->smpw_Task)
                    {
                        workMaster.smpm_WorkerCount++;
                        Wait(SIGBREAKF_CTRL_C);
                        AddTail(&workMaster.smpm_Workers, &coreWorker->smpw_Node);
                        AddHead(&coreWorker->smpw_Task->tc_MemEntry, &coreML->ml_Node);
                    }
                }
                else
                {
                    FreeMem(coreML->ml_ME[0].me_Addr, sizeof(struct SMPWorker));
                    FreeMem(coreML, sizeof(struct MemList) + sizeof(struct MemEntry));
                }
            }
            else
            {
                FreeMem(coreML, sizeof(struct MemList) + sizeof(struct MemEntry));
            }
        }
    }

    Printf("Waiting for workers to become ready ...\n");

    do {
        Delay(1);
        complete = TRUE;
        ForeachNode(&workMaster.smpm_Workers, coreWorker)
        {
            if (coreWorker->smpw_Node.ln_Type != 1)
            {
                complete = FALSE;
                break;
            }
        }
    } while (!complete);

    /*
        * We now have our workers all launched,
        * and a node for each on our list.
        * lets get them to do some work ...
        */
    Printf("Sending out work to do ...\n");

    _sprintf(buffer, "Buddhabrot fractal (%ld workers on %ld threads)", workMaster.smpm_WorkerCount, coreCount);

    complete = FALSE;
    pubScreen = LockPubScreen(0);

    SetTaskPri(FindTask(NULL), 5);

    if (req_size == 0)
        req_size = (pubScreen) ? ((pubScreen->Height - pubScreen->BarHeight) * 3) / 4 : 256;

    if ((displayWin = OpenWindowTags(0,
                                    WA_PubScreen, (Tag)pubScreen,
                                    WA_Left, 0,
                                    WA_Top, (pubScreen) ? pubScreen->BarHeight : 10,
                                    WA_InnerWidth, req_size,
                                    WA_InnerHeight, req_size,
                                    WA_Title, (Tag)buffer,
                                    WA_SimpleRefresh, TRUE,
                                    WA_CloseGadget, TRUE,
                                    WA_DepthGadget, TRUE,
                                    WA_DragBar, TRUE,
                                    WA_SizeGadget, FALSE,
                                    WA_SizeBBottom, FALSE,
                                    WA_SizeBRight, FALSE,
                                    WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
                                    TAG_DONE)) != NULL)
    {
        struct RastPort *outBMRastPort;
        BOOL working = TRUE;
        UWORD width, height;

        SetWindowPointer( displayWin, WA_BusyPointer, TRUE, TAG_DONE );

        if (pubScreen)
            UnlockPubScreen(0, pubScreen);

        width = workMaster.smpm_Width  = (displayWin->Width - displayWin->BorderLeft - displayWin->BorderRight);
        height = workMaster.smpm_Height = (displayWin->Height - displayWin->BorderTop - displayWin->BorderBottom);

        outputBMap = AllocBitMap(   workMaster.smpm_Width,
                                    workMaster.smpm_Height,
                                    GetBitMapAttr(displayWin->WScreen->RastPort.BitMap, BMA_DEPTH),
                                    BMF_DISPLAYABLE, displayWin->WScreen->RastPort.BitMap);

        workMaster.smpm_WorkBuffer = AllocMem(workMaster.smpm_Width * workMaster.smpm_Height * sizeof(ULONG), MEMF_ANY|MEMF_CLEAR);

        ULONG *rgba = AllocMem(workMaster.smpm_Width * workMaster.smpm_Height * sizeof(ULONG), MEMF_ANY|MEMF_CLEAR);

        struct RenderInfo ri;
        ri.Memory = rgba;
        ri.BytesPerRow = workMaster.smpm_Width * sizeof(ULONG);
        ri.RGBFormat = RGBFB_R8G8B8A8;

        Printf("Target BitMap @ 0x%08lx\n", outputBMap);
        Printf("    %ldx%ldx%ld\n", workMaster.smpm_Width, workMaster.smpm_Height, GetBitMapAttr(outputBMap, BMA_DEPTH));
        Printf("Buffer @ 0x%08lx\n", workMaster.smpm_WorkBuffer);
        Printf("RGBA Buffer @ 0x%08lx\n", rgba);

        outBMRastPort = (struct RastPort *)AllocMem(sizeof(struct RastPort), MEMF_ANY);
        InitRastPort(outBMRastPort);
        outBMRastPort->BitMap = outputBMap;

        Printf("Target BitMap RastPort @ 0x%08lx\n", outBMRastPort);

        p96WritePixelArray(&ri, 0, 0, 
                            outBMRastPort,
                            0, 0,
                            workMaster.smpm_Width, workMaster.smpm_Height);

        BltBitMapRastPort (outputBMap, 0, 0,
                displayWin->RPort, displayWin->BorderLeft, displayWin->BorderTop,
                width, height, 0xC0); 

        workMaster.smpm_Master = NewCreateTask( TASKTAG_NAME,       (Tag)"Buddha Master",
                                                TASKTAG_PRI,        -1,
                                                TASKTAG_PC,         (Tag)SMPTestMaster,
                                                TASKTAG_USERDATA,   (Tag)&workMaster,
                                                TAG_DONE);

        GetSysTime(&start_time);

        Printf("Waiting for the work to be done ...\n");

        /* Wait for the workers to finish processing the data ... */
        
        while ((working) && ((signals = Wait(SIGBREAKF_CTRL_D | (1 << displayWin->UserPort->mp_SigBit))) != 0))
        {
            if ((signals & SIGBREAKF_CTRL_D) && (!complete))
            {
                complete = TRUE;

                /* Is work still being issued? */
                if (workMaster.smpm_Master)
                    complete = FALSE;

                /* Are workers still working ? */
                ForeachNode(&workMaster.smpm_Workers, coreWorker)
                {
                    if (!IsListEmpty(&coreWorker->smpw_MsgPort->mp_MsgList))
                        complete = FALSE;
                }

                for (ULONG i = 0; i < workMaster.smpm_Width * workMaster.smpm_Height; i++)
                {
                    ULONG rgb = workMaster.smpm_WorkBuffer[i];
                    ULONG r=96*rgb,g=128*rgb,b=256*rgb;

                    b = (b + 255) / 512;
                    r = (r + 255) / 512;
                    g = (g + 255) / 512;

                    if (b > 255) b = 255;
                    if (r > 255) r = 255;
                    if (g > 255) g = 255;

                    ULONG c = 0xff | (b << 8) | (g << 16) | (r << 24);
                    rgba[i] = c;
                }

                p96WritePixelArray(&ri, 0, 0, 
                                    outBMRastPort,
                                    0, 0,
                                    workMaster.smpm_Width, workMaster.smpm_Height);

                if (complete)
                {
                    SetWindowPointer( displayWin, WA_BusyPointer, FALSE, TAG_DONE );

                    GetSysTime(&now);
                    SubTime(&now, &start_time);
                    Printf("Fractal rendered in %ld:%02ld:%02ld\n",
                                                now.tv_secs / 3600,
                                                (now.tv_secs / 60) % 60,
                                                now.tv_secs % 60);

                    rawArgs[0] = coreCount;
                    _sprintf(buffer, "Buddhabrot fractal (0 workers on %ld threads) - Finished", coreCount);
                    SetWindowTitles( displayWin, buffer, NULL);

                    p96WritePixelArray(&ri, 0, 0, 
                            outBMRastPort,
                            0, 0,
                            workMaster.smpm_Width, workMaster.smpm_Height);

                    BltBitMapRastPort (outputBMap, 0, 0,
                            displayWin->RPort, displayWin->BorderLeft, displayWin->BorderTop,
                            width, height, 0xC0); 
                }
            }
            else if (signals & (1 << displayWin->UserPort->mp_SigBit))
            {
                struct IntuiMessage *msg;
                while ((msg = (struct IntuiMessage *)GetMsg(displayWin->UserPort)))
                {
                    switch(msg->Class)
                    {
                        case IDCMP_CLOSEWINDOW:
                            working = FALSE;
                            break;

                        case IDCMP_REFRESHWINDOW:
                            BeginRefresh(msg->IDCMPWindow);
                            BltBitMapRastPort (outputBMap, 0, 0,
                                msg->IDCMPWindow->RPort, msg->IDCMPWindow->BorderLeft, msg->IDCMPWindow->BorderTop,
                                width, height, 0xC0);
                            EndRefresh(msg->IDCMPWindow, TRUE);
                            break;
                    }
                    ReplyMsg((struct Message *)msg);
                }
            }
            BltBitMapRastPort (outputBMap, 0, 0,
                displayWin->RPort, displayWin->BorderLeft, displayWin->BorderTop,
                width, height, 0xC0); 
        }


        Printf("Letting workers know we are finished ...\n");

        ForeachNode(&workMaster.smpm_Workers, coreWorker)
        {
            /* Tell the workers they are finished ... */
            if ((workMsg = (struct SMPWorkMessage *)AllocMem(sizeof(struct SMPWorkMessage), MEMF_CLEAR)) != NULL)
            {
                workMsg->smpwm_Type = SPMWORKTYPE_FINISHED;
                PutMsg(coreWorker->smpw_MsgPort, (struct Message *)workMsg);
            }
        }
        FreeMem(workMaster.smpm_WorkBuffer, workMaster.smpm_Width * workMaster.smpm_Height * sizeof(ULONG));
        FreeMem(rgba, workMaster.smpm_Width * workMaster.smpm_Height * sizeof(ULONG));
        CloseWindow(displayWin);
        outBMRastPort->BitMap = NULL;
        FreeMem(outBMRastPort, sizeof(struct RastPort));
        FreeBitMap(outputBMap);
    }
    else if (pubScreen) UnlockPubScreen(0, pubScreen);

    Printf("Waiting for workers to finish ...\n");

    /* wait for the workers to finish up before we exit ... */
    if (workMaster.smpm_MasterPort)
    {
        while ((signals = Wait(SIGBREAKF_CTRL_C)) != 0)
        {
            if ((signals & SIGBREAKF_CTRL_C) && IsListEmpty(&workMaster.smpm_Workers))
                break;
        }
        DeleteMsgPort(workMaster.smpm_MasterPort);
    }

    Printf("Done ...\n");


#endif

    DeleteMsgPort(mainPort);
    CloseLibrary(P96Base);

    return 0;
}
