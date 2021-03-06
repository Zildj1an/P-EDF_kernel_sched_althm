#include "mi_plugin.h" /* All the functions are in this header explained in detail */

static void mi_job_completion(struct task_struct *prev, int budget_exhausted){

    /* Simply delegates the job to litmus/jobs.h
       It computes the next release time, deadline, etc.
    */
    prepare_for_next_period(prev);
}

static void mi_requeue(struct task_struct *tsk, struct mi_cpu_state *cpu_state){

      /* is_released esta en include/litmus/litmus.h
         Solo coge la hora de release del task y comprueba restando que es antes de ahora
         (por tanto puede ir a la ready queue)
      */
      if (is_released(tsk, litmus_clock())) {
          /* Uses __add_ready() instead of add_ready() because we already hold the ready lock. */
          __add_ready(&cpu_state->local_queues, tsk);
      }
      else{
          /* Uses add_release() because we DON'T have the release lock. */
          add_release(&cpu_state->local_queues, tsk);
      }
}

static void mi_task_new(struct task_struct *tsk, int on_runqueue, int is_running){

      /* We store here IRQ flags */
      unsigned long flags;

      /* Retrieve the CPU-state of a task tsk
         At include/litmus/litmus.h
         #define get_partition(t) (tsk_rt(t)->task_params.cpu)
      */
      struct mi_cpu_state *state = cpu_state_for(get_partition(tsk));
      lt_t now;

      printk(KERN_INFO "%s -> New task (on_runqueue = %d, running = %d) \n", MODULE_NAME, on_runqueue, is_running);

      /* Acquire lock and disable interrupts */
      raw_spin_lock_irqsave(&state->local_queues.ready_lock, flags);

      /* At include/litmus/litmus.h - return ktime_to_ns(ktime_get()); */
      now = litmus_clock();

      /* Release the first job now
         At /litmus/jobs.c - Setup release
      */
      release_at(tsk,now);

      if(is_running){
          /* If tsk is running, no other can be running on the local CPU */
          BUG_ON(state->scheduled != NULL);
          state->scheduled = tsk;
      }
      else if(on_runqueue)
          mi_requeue(tsk, state);

      /* Preemptive */
      if(edf_preemption_needed(&state->local_queues, state->scheduled))
          preempt_if_preemptable(state->scheduled, state->cpu);

      raw_spin_unlock_irqrestore(&state->local_queues.ready_lock, flags);
}

static void mi_task_resume(struct task_struct *tsk){

      unsigned long flags;
      /* The CPU processing the wakeup is not necessarily the CPU that the task is assigned to */
      struct mi_cpu_state *state = cpu_state_for(get_partition(tsk));
      lt_t now;

      printk(KERN_INFO "%s -> Task woke up at %llu \n", MODULE_NAME, litmus_clock());

      raw_spin_lock_irqsave(&state->local_queues.ready_lock, flags);

      now = litmus_clock();

      /* Sporadic tasks suspended a long time ago are given a new budget and deadline */
      if(is_sporadic(tsk) && is_tardy(tsk,now)){
          /* Triggering a new job release give the task a new budget */
          release_at(tsk,now);
      }

      /* Used to avoid races between tasks that wake up when this function is been called */
      if(state->scheduled != tsk){
          mi_requeue(tsk,state);
          /* Preemptive */
          if(edf_preemption_needed(&state->local_queues,state->scheduled))
              preempt_if_preemptable(state->scheduled, state->cpu);
      }

      raw_spin_unlock_irqrestore(&state->local_queues.ready_lock, flags);
}

/* 2 Exiting tasks */
static void mi_task_exit(struct task_struct *tsk){

      unsigned long flags;
      struct mi_cpu_state *state = cpu_state_for(get_partition(tsk));

      raw_spin_lock_irqsave(&state->local_queues.ready_lock, flags);

      /* Assumed here that the task is no longer queued anywhere else. (required if task
         forced out of this scheduling mode by other tasks) */
      if(state->scheduled == tsk)
          state->scheduled = NULL;

      raw_spin_unlock_irqrestore(&state->local_queues.ready_lock,flags);
}

/* Provides a ptr to the next task to be executed  */
static struct task_struct* mi_schedule(struct task_struct * prev) {

      /* ------------------------------------------------
         I will implement here P-EDF scheduling algorithm
         ------------------------------------------------
      */

      struct mi_cpu_state *local_state = local_cpu_state();

      /* next == NULL means schedule bakground work */
      struct task_struct *next = NULL;

      /* previous task state values */
      int exists, out_of_time = 0, job_completed = 0, self_suspends = 0, preempt,resched;

      /* Obtain (core-local) ready queue lock
         I dont have to worry about interrupts since they are disabled at this point.
      */
      raw_spin_lock(&local_state->local_queues.ready_lock);

      /* BUG_ON executes BUG() when the condition holds. BUG() generates a kernel panic and shuts the
         system down. WARN_ON generates a printk WARN() on dmesg log when the condition holds.
         Here it checks that there is a previous task scheduled and:
         a. It is not the specified as previous prev.
         b. It is not a real time task
       */

      exists = (local_state->scheduled != NULL);

      BUG_ON(exists && (local_state->scheduled != prev || !is_realtime(prev)));

      /* Check now previous task state values */

      if(exists){

          /* True if prev cannot be scheduled any longer */
          self_suspends = !is_current_running();

          /* True if current job overran its budget
             Both macros at litmus.h
             (!)
             rt_param es una estructura que metieron en task_struct con litmus/rt_param.h
             para guardar el deadline, prioridad, budget, politicas RT...
             #define tsk_rt(t) (&(t)->rt_param)
             #define budget_enforced(t) (tsk_rt(t)->task_params.budget_policy != NO_ENFORCEMENT)
          */
          out_of_time = budget_enforced(prev) && budget_exhausted(prev);

          /* True if current job signaled completion via sycall
             is_completed() is at litmus.h, checks t && tsk_rt(t)->completed;
          */
          job_completed = is_completed(prev);
      }

      /* True if task 'prev' has lower priority than something on the ready queue
         (Ver /litmus/edf_common.c )
      */
      preempt = edf_preemption_needed(&local_state->local_queues,prev);

      /* check all conditions that make us reschedule
         If prev suspends it CANNOT be scheduled anymore, we need to reschedule.
      */
      resched = (preempt || self_suspends);

      /* Check for (in)voluntary job completions
         out_of_time es de real time y no tiene pq tenerlo (ej FIFO).
      */
      if(out_of_time || job_completed){
          mi_job_completion(prev,out_of_time); /* wrapper (helper below)*/
          resched = 1;
      }

      if(resched){

          /* First checks if the previous task goes back onto the ready queue, which it
          does if it did not self_suspend */
          if(!self_suspends)
            mi_requeue(prev, local_state);

          next = __take_ready(&local_state->local_queues); /* Puede usar __take_ready pq tenemos lock */
      }
      else {
        /* No preemption required */
        next = local_state->scheduled;
      }

      local_state->scheduled = next;

      if(exists && prev != next) printk(KERN_INFO "%s -> Previous task descheduled. \n", MODULE_NAME);
      if(next) printk(KERN_INFO "%s -> Task scheduled. \n", MODULE_NAME);

      /* This mandatory.
       * Informs the kernel that a scheduling decision has been made.
       * It triggers a transition in the LITMUS^RT remote
       * preemption state machine. Call this AFTER the plugin has made a local
       * scheduling decision.
       */
      sched_state_task_picked();

      raw_spin_unlock(&local_state->local_queues.ready_lock);

      /* With NULL we just delegate in the default Linux scheduler.*/
      return next;
}

static int mi_check_for_preemption_on_release(rt_domain_t *local_queues){

    /* Linux standard macro
       Having: "struct container{int some_other_data;int this_data;}" and a pointer ptr* to this_data
       you could get the struct with: struct container *ctr = container_of(ptr,struct container, this_data);
     */
    struct mi_cpu_state *state = container_of(local_queues, struct mi_cpu_state, local_queues);

    /* New job could have a shorter deadline than scheduled task (or scheduled is NULL)*/
    if(edf_preemption_needed(local_queues, state->scheduled)){
        preempt_if_preemptable(state->scheduled, state->cpu);
        return 1;
    }

    return 0;
}

static long mi_admit_task(struct task_struct *tsk) {

      /* task_cpu(task) is Linux's notion of where the task currently is.
         get_partition(task) is LITMUS's notion of where the task is logically assigned to.
         Since P-EDF is a particioned scheduler,we require that R-T tasks have migrated to the appropriated
          core before they become real-time tasks.

      if(task_cpu(tsk) == get_partition(tsk)){
          printk(KERN_INFO "%s -> Accepted by mi_plugin. \n", MODULE_NAME);
          return 0;
      }
	  else {
		  printk(KERN_ALERT "%s -> Plugin rejected. \n", MODULE_NAME);
	          return -EINVAL;
	} */

       return 0;
}

static long mi_get_domain_proc_info(struct domain_proc_info **ret){
    *ret = &mi_domain_proc_info;
  return 0;
}

static void mi_setup_domain_proc(void){

      int i, cpu;
      int num_rt_cpus = num_online_cpus();

      /* Los mapping relacionan cpus a domains y viceversa */
      struct cd_mapping *cpu_map, *domain_map;

      memset(&mi_domain_proc_info, 0, sizeof(mi_domain_proc_info));
      /* Y ésto solo reserva memoria dinamica*/
      init_domain_proc_info(&mi_domain_proc_info, num_rt_cpus, num_rt_cpus);
      mi_domain_proc_info.num_cpus = num_rt_cpus;
      mi_domain_proc_info.num_domains = num_rt_cpus;

      i = 0;
      for_each_online_cpu(cpu){
          cpu_map = &mi_domain_proc_info.cpu_to_domains[i];
          domain_map = &mi_domain_proc_info.domain_to_cpus[i];

          cpu_map->id = cpu;
          domain_map->id = i;
          cpumask_set_cpu(i, cpu_map->mask);
          cpumask_set_cpu(cpu, domain_map->mask);
          ++i;
      }
}

static long mi_deactivate_plugin(void){

      destroy_domain_proc_info(&mi_domain_proc_info);
      module_put(THIS_MODULE);
 
   return 0;
}

static long mi_activate_plugin(void){

      int cpu;
      struct mi_cpu_state *state;

      /* kernel macro at cpumask.h
         It would be better to use for_each_possible_cpu()
         just in case of CPU hotplug (creo...)
         Si no, creo que vale: for (i = 0; i < num_online_cpus(); i++)
      */
      for_each_online_cpu(cpu){
          printk("%s -> Initializing CPU %d...\n", MODULE_NAME, cpu);

          state = cpu_state_for(cpu);
          state->cpu = cpu;
          state->scheduled = NULL;
          /* If the alth is not preemptive then second arg should be NULL */
          edf_domain_init(&state->local_queues, mi_check_for_preemption_on_release, NULL);
      }

      mi_setup_domain_proc();
      try_module_get(THIS_MODULE);

    return 0;
}

static struct sched_plugin mi_plugin = {
        /*The full list of function callbacks that can be provided in the sched_plugin struct
        can be seen in the definition of sched_plugin in /include/litmus */
        .plugin_name            = MODULE_NAME,
        .schedule               = mi_schedule,
        .admit_task             = mi_admit_task,
        .activate_plugin        = mi_activate_plugin,
        .task_wake_up           = mi_task_resume,
        .task_new               = mi_task_new,
        .task_exit              = mi_task_exit,
        .complete_job           = complete_job, /* Made at /litmus/jobs.c */
        .deactivate_plugin      = mi_deactivate_plugin,
        .get_domain_proc_info   = mi_get_domain_proc_info,
};

/* Initialize the plugin */
static int __init init_mi(void){

      int err;

     /* Add the sched plugin to the list of available pugins */
      if ((err = register_sched_plugin(&mi_plugin)))
            printk(KERN_INFO "Module %s charged \n", MODULE_NAME);

    return err;
}

static void exit_mi(void){

    if (unregister_sched_plugin(&mi_plugin))
      printk(KERN_INFO "Module %s discharged \n", MODULE_NAME);
}
