/*
    Copyright � 2017, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <exec/tasks.h>
#include <exec/ports.h>
#include <exec/semaphores.h>

#define MAX_LUT 0xfff

struct MyMessage {
    struct Message      mm_Message;
    ULONG               mm_Type;
    union {
        struct {
            ULONG               width;
            ULONG               height;
            ULONG               threadCount;
            ULONG               subdivide;
            ULONG               maxIterations;
            ULONG               oversample;
            ULONG               type;
            ULONG               *workBuffer;
            double              x0;
            double              y0;
            double              size;
            struct SignalSemaphore *writeLock;
            struct Task         *redrawTask;
        } Startup;
         
        struct {
            ULONG               width;
            ULONG               height;
            ULONG               maxIterations;
            ULONG               oversample;
            ULONG               type;
            ULONG               workStart;
            ULONG               workEnd;
            ULONG               *workBuffer;
            double              x0;
            double              y0;
            double              size;
            struct SignalSemaphore *writeLock;
            struct Task         *redrawTask;
        } WorkPackage;

        struct {
            ULONG   tasksIn;
            ULONG   tasksOut;
            ULONG   tasksWork;
        } Stats;
#if 0
        struct {
            ULONG               TileX;
            ULONG               TileY;
        } RedrawTile;

        struct {
            struct tileWork *   tile;
            struct MsgPort *    guiPort;
            ULONG *             buffer;
            ULONG               width;
            ULONG               height;
            ULONG               numberOfSamples;
            BYTE                explicitMode;
        } RenderTile;


#endif
    } mm_Body;
};

enum {
    TYPE_NORMAL,
    TYPE_BUDDHA
};

enum {
    MSG_STARTUP,
    MSG_DIE,
    MSG_WORKPACKAGE,
    MSG_HUNGRY,
    MSG_STATS,
    MSG_DONE,
};


#if 0
#define MSG_REDRAWTILE  2
#define MSG_RENDERTILE  3
#define MSG_HUNGRY      4
#define MSG_RENDERREADY 5
#define MSG_STATS       6
#endif

struct SMPMaster
{
    ULONG                       smpm_WorkerCount;
    struct List                 smpm_Workers;
    struct List                 smpm_Tasks;
    struct Task                 *smpm_Master;
    struct MsgPort              *smpm_MasterPort;
    ULONG                       *smpm_WorkBuffer;
    UWORD                       smpm_Width;
    UWORD                       smpm_Height;    
    struct SignalSemaphore      smpm_Lock;
    ULONG                       smpm_MaxWork;
    ULONG                       smpm_Oversample;
    BOOL                        smpm_Buddha;
};

struct SMPWorker
{
    struct Node                 smpw_Node;
    struct Task                 *smpw_Task;
    struct MsgPort              *smpw_MasterPort;
    struct MsgPort              *smpw_MsgPort;
    struct Task                 *smpw_SyncTask;
    struct SignalSemaphore      *smpw_Lock;
    ULONG                       smpw_MaxWork;
    ULONG                       smpw_Oversample;
};

struct SMPWorkMessage
{
    struct Message              smpwm_Msg;
    ULONG                       smpwm_Type;
    ULONG                       *smpwm_Buffer;
    ULONG                       smpwm_Width;
    ULONG                       smpwm_Height;
    ULONG                       smpwm_Start;
    ULONG                       smpwm_End;
    struct SignalSemaphore      *smpwm_Lock;
};

#define SPMWORKTYPE_FINISHED    (1 << 0)
#define SPMWORKTYPE_MANDLEBROT  (1 << 1)
#define SPMWORKTYPE_BUDDHA      (1 << 2)

void SMPTestMaster();
void SMPTestWorker();
