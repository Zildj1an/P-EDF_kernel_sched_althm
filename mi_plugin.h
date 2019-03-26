/* 1_LIBRARIES___________________________________DESCRIPTIONS_________________*/
#include <linux/module.h>
#include <linux/percpu.h>       /* Needed for making CPU-local allocations */
#include <linux/sched.h>        /* Definition of struct task_struct */
#include <litmus/litmus.h>      /* Many required definitions for LITMUS-RT*/
#include <litmus/rt_domain.h>   /* Contains definitions for real-time domains, release and ready queues*/
#include <litmus/edf_common.h>   /* EDF helper functions like edf_preemption_needed() and definitions*/
#include <litmus/preempt.h>     /* Used for changing scheduling_state with sched_state_<state>()
                                States: TASK_(SCHEDULED,PICKED), (SHOULD,WILL)_SCHEDULE, PICKED_WRONG_TASK */
#include <litmus/sched_plugin.h> /*Sched plugin definition and register_sched_plugin()*/
#include <litmus/jobs.h>        /* For managing jobs in the scheduler*/
#include <litmus/budget.h>      /* For managing budget in the scheduler*/
#include <litmus/litmus_proc.h> /* Wrapper API for the /proc FS */

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("My P-EDF scheduling plugin with a kernel module, using litmus libraries");

#define MODULE_NAME         "MI_PLUGIN"

/* Allows showing domain info on /proc for migrations */
static struct domain_proc_info mi_domain_proc_info;

/* 2_PER CPU________________________________________________________________*/
/* We need a ready and release queue for P-EDF. We use this struct below
   for keeping track of the scheduled task, the id and the queues (with rt_domain) at each CPU */
struct mi_cpu_state {

    rt_domain_t local_queues;
    int         cpu;

    struct task_struct* scheduled;
};

/* Linux's per-processor allocation macro - statically allocate states*/
static DEFINE_PER_CPU(struct mi_cpu_state, mi_cpu_state);

/* Two macros to wrap Linux's per-cpu data structure API */
#define cpu_state_for(cpu_id)   (&per_cpu(mi_cpu_state, cpu_id))
#define local_cpu_state()       (this_cpu_ptr(&mi_cpu_state))

/* 3_HELPER FUNCTIONS (for mi_schedule())________________________________________________________________*/

/* This helper is called when task `prev` exhausted its budget or when it signaled a job completion */
static void mi_job_completion(struct task_struct *prev, int budget_exhausted);

/* Add the task `tsk` to the appropriate queue. Assumes the caller holds the ready lock.
   If the task has a pending job, it is placed in the (core-local) ready queue.
   Otherwise, if the next job's earliest release time is in the future, the task is placed in the (core-local)
   release queue
 */
static void mi_requeue(struct task_struct *tsk, struct mi_cpu_state *cpu_state);

/* 4_SCHEDULING FUNCTION________________________________________________________________*/
/*
   A scheduler plugin needs to manage:
   0. New tasks (enter in this scheduling mode) and self_suspended => mi_task_new(tsk,...)
      Either running or suspended tasks. The scheduler state of the task must be initialized:
      release time, if running recorded in ->scheduled of mi_cpu_state ,else added to the ready queue.

   1. Resuming tasks (become available for execution after self-suspended)
   2. Exiting tasks (leave this mode, unexpected exit) => mi_task_exit(tsk)
*/

/* 0. New tasks */
static void mi_task_new(struct task_struct *tsk, int on_runqueue, int is_running);

/* 1 Resuming tasks
   Called when the state of the task changes back to TASK_RUNNING
   We need to requeue the task, to the ready or release queue, depending on when they resume.
*/
static void mi_task_resume(struct task_struct *tsk);

/* 2 Exiting tasks */
static void mi_task_exit(struct task_struct *tsk);

/* Provides a ptr to the next task to be executed  */
static struct task_struct* mi_schedule(struct task_struct * prev);

/*
  Preemption check callback for implementing preemptive P-EDF.
  Called by rt_domain_t whenever a job is transferred from the release queue to the ready queue.
  Since it is called by rt_domain_t, it already has the ready queue lock.
*/
static int mi_check_for_preemption_on_release(rt_domain_t *local_queues);

/* Called whenever a task attempts to be admitted under to the system under this scheduler*/
static long mi_admit_task(struct task_struct *tsk);

/*
    In order for the library liblitmus to migrate CPUs it requires to know the CPUs topology(this is,
    what the plugin considers to be a "partition" or "cluster", and the plugin uses /proc/ FS to let it
    know, using a wrapper API (<litmys/litmus_proc.h) and a struct called domain_proc_info.

    Segun https://lwn.net/Articles/80911/ un scheduling domain es un conjunto de CPUs que comparten
    caracteristicas, es bueno (eficiente) balancear entre ellos. Un domain se divide en CPU groups de varios cores
    y se considera unidad. Por tanto el balanceo es jer?rquico: 1º-Que  Domain? 2º- Que? grupo?

    Obtener el domain => mi_get_domain_proc_info()
    Inicializar el domain => mi_setup_domain_proc()
*/

static long mi_get_domain_proc_info(struct domain_proc_info **ret);

/* Stores current topology on mi_domain_proc_info
   In a simple partitioned plugin like this, each CPU forms its own 'scheduling domain'
   Threfore you have a domain for each available CPU.
*/
static void mi_setup_domain_proc(void);

static long mi_deactivate_plugin(void);

/* Function called when user selects this scheduler (setsched, liblitmus for now)
   Therefore we are here going to initialize the per-CPU state */
static long mi_activate_plugin(void);

/* Initialize the plugin */
static int __init init_mi(void);

static void exit_mi(void);

module_init(init_mi);
module_exit(exit_mi);