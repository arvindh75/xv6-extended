#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
int q_age[5] = {10, 20, 30, 40, 50};

static void wakeup1(void *chan);

void pinit(void) {
    initlock(&ptable.lock, "ptable");
}

void demote_q(struct proc* p) { //Moves process to a higher queue if timeslices are utilized
    acquire(&ptable.lock);
    if(p->prev_q != 4) {
#ifdef DEBUG_Y
        cprintf("Process %d has utilized timeslices %d, moving from %d to %d\n", p->pid,p->cur_q_ticks, p->prev_q, p->prev_q+1);
#endif
#ifdef DEBUG_P
        cprintf("%d %d %d %d\n", ticks, p->pid, p->prev_q, p->prev_q+1);
#endif
        p->prev_q++;
    }
    release(&ptable.lock);
}

void inc_q_ticks(struct proc* p) { //Increase ticks of current queue
    acquire(&ptable.lock);
    p->cur_q_ticks++;
    p->q_ticks[p->cur_q]++;
#ifdef DEBUG_YN
    cprintf("Process %d, increased ticks %d for queue %d\n", p->pid, p->cur_q_ticks, p->cur_q);
#endif
    release(&ptable.lock);
}

void inc_r_io_time() {
    struct proc *p;
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if(p->state == RUNNING) {
            p->rtime++;
            //p->q_ticks[p->prev_q]++;
        }
        if(p->state == SLEEPING) {
            p->iotime++;
            p->cur_q_waiting_time++;
            p->last_runtime = ticks;
        }
        if(p->state == RUNNABLE) {
            p->cur_q_waiting_time++;
        }
    }
    release(&ptable.lock);
}

// Must be called with interrupts disabled
int cpuid() {
    return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu* mycpu(void) {
    int apicid, i;

    if(readeflags()&FL_IF)
        panic("mycpu called with interrupts enabled\n");

    apicid = lapicid();
    // APIC IDs are not guaranteed to be contiguous. Maybe we should have
    // a reverse map, or reserve a register to store &cpus[i].
    for (i = 0; i < ncpu; ++i) {
        if (cpus[i].apicid == apicid)
            return &cpus[i];
    }
    panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc* myproc(void) {
    struct cpu *c;
    struct proc *p;
    pushcli();
    c = mycpu();
    p = c->proc;
    popcli();
    return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc* allocproc(void) {
    struct proc *p;
    char *sp;

    acquire(&ptable.lock);

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if(p->state == UNUSED)
            goto found;

    release(&ptable.lock);
    return 0;

found:
    p->state = EMBRYO;
    p->pid = nextpid++;

    release(&ptable.lock);

    // Allocate kernel stack.
    if((p->kstack = kalloc()) == 0){
        p->state = UNUSED;
        return 0;
    }
    sp = p->kstack + KSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *p->tf;
    p->tf = (struct trapframe*)sp;

    // Set up new context to start executing at forkret,
    // which returns to trapret.
    sp -= 4;
    *(uint*)sp = (uint)trapret;

    sp -= sizeof *p->context;
    p->context = (struct context*)sp;
    memset(p->context, 0, sizeof *p->context);
    p->context->eip = (uint)forkret;
    p->ctime = ticks;
    p->etime = 0;
    p->rtime = 0;
    p->priority = 60;
    p->n_run = 0;
    p->cur_q = 0;
    p->prev_q = 0;
    p->q_join_time = p->ctime;
#ifdef DEBUG_P
    cprintf("%d %d %d %d\n", p->q_join_time, p->pid, p->prev_q, p->prev_q);
#endif

    p->cur_q_ticks = 0;
    p->cur_q_waiting_time = 0;
    p->q_ticks[0] = 0;
    p->q_ticks[1] = 0;
    p->q_ticks[2] = 0;
    p->q_ticks[3] = 0;
    p->q_ticks[4] = 0;
    p->last_runtime = p->ctime;

    return p;
}

//PAGEBREAK: 32
// Set up first user process.
void userinit(void) {
    struct proc *p;
    extern char _binary_initcode_start[], _binary_initcode_size[];

    p = allocproc();

    initproc = p;
    if((p->pgdir = setupkvm()) == 0)
        panic("userinit: out of memory?");
    inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
    p->sz = PGSIZE;
    memset(p->tf, 0, sizeof(*p->tf));
    p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
    p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
    p->tf->es = p->tf->ds;
    p->tf->ss = p->tf->ds;
    p->tf->eflags = FL_IF;
    p->tf->esp = PGSIZE;
    p->tf->eip = 0;  // beginning of initcode.S

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");

    // this assignment to p->state lets other cores
    // run this process. the acquire forces the above
    // writes to be visible, and the lock is also needed
    // because the assignment might not be atomic.
    acquire(&ptable.lock);

    p->state = RUNNABLE;

#ifdef MLFQ //Push main user process to the queue
    p->cur_q = 0;
    p->prev_q = 0;
    p->q_join_time = ticks;
#endif

    release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n) {
    uint sz;
    struct proc *curproc = myproc();

    sz = curproc->sz;
    if(n > 0){
        if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
            return -1;
    } else if(n < 0){
        if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
            return -1;
    }
    curproc->sz = sz;
    switchuvm(curproc);
    return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void) {
    int i, pid;
    struct proc *np;
    struct proc *curproc = myproc();

    // Allocate process.
    if((np = allocproc()) == 0){
        return -1;
    }

    // Copy process state from proc.
    if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
        kfree(np->kstack);
        np->kstack = 0;
        np->state = UNUSED;
        return -1;
    }
    np->sz = curproc->sz;
    np->parent = curproc;
    *np->tf = *curproc->tf;

    // Clear %eax so that fork returns 0 in the child.
    np->tf->eax = 0;

    for(i = 0; i < NOFILE; i++)
        if(curproc->ofile[i])
            np->ofile[i] = filedup(curproc->ofile[i]);
    np->cwd = idup(curproc->cwd);

    safestrcpy(np->name, curproc->name, sizeof(curproc->name));

    pid = np->pid;

    acquire(&ptable.lock);

    np->state = RUNNABLE;
    /*
#ifdef MLFQ //Push the new process to the first queue
np->cur_q = 0;
np->prev_q = 0;
np->q_join_time = ticks;
#endif
*/

    release(&ptable.lock);

    return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void) {
    struct proc *curproc = myproc();
    struct proc *p;
    int fd;

    if(curproc == initproc)
        panic("init exiting");

    // Close all open files.
    for(fd = 0; fd < NOFILE; fd++){
        if(curproc->ofile[fd]){
            fileclose(curproc->ofile[fd]);
            curproc->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(curproc->cwd);
    end_op();
    curproc->cwd = 0;

    acquire(&ptable.lock);

    // Parent might be sleeping in wait().
    wakeup1(curproc->parent);

    // Pass abandoned children to init.
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->parent == curproc){
            p->parent = initproc;
            if(p->state == ZOMBIE)
                wakeup1(initproc);
        }
    }

    // Jump into the scheduler, never to return.
    curproc->state = ZOMBIE;
    curproc->etime = ticks;
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void) {
    struct proc *p;
    int havekids, pid;
    struct proc *curproc = myproc();

    acquire(&ptable.lock);
    for(;;){
        // Scan through table looking for exited children.
        havekids = 0;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
            if(p->parent != curproc)
                continue;
            havekids = 1;
            if(p->state == ZOMBIE){
                // Found one.
                pid = p->pid;
                kfree(p->kstack);
                p->kstack = 0;
                freevm(p->pgdir);
                /*
#ifdef MLFQ //Remove process from the queue as it is waiting
p->cur_q = -1;
#ifdef DEBUG_Y
cprintf("Process %d has relinquised CPU\n", p->pid);
#endif
#endif
*/
                p->pid = 0;
                p->parent = 0;
                p->name[0] = 0;
                p->killed = 0;
                p->state = UNUSED;
                release(&ptable.lock);
                return pid;
            }
        }

        // No point waiting if we don't have any children.
        if(!havekids || curproc->killed){
            release(&ptable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(curproc, &ptable.lock);  //DOC: wait-sleep
    }
}

int waitx(int* wtime,int* rtime) {
    struct proc *p;
    int havekids, pid;
    struct proc *curproc = myproc();

    acquire(&ptable.lock);
    for(;;){
        // Scan through table looking for exited children.
        havekids = 0;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
            if(p->parent != curproc)
                continue;
            havekids = 1;
            if(p->state == ZOMBIE){
                // Found one.
                pid = p->pid;
                *wtime = (p->etime - p->ctime - p->rtime - p->iotime);
                *rtime = p->rtime;
                kfree(p->kstack);
                p->kstack = 0;
                freevm(p->pgdir);
                /*
#ifdef MLFQ //Remove process from the queue as it is waiting
p->cur_q = -1;
#ifdef DEBUG_Y
cprintf("Process %d has relinquised CPU\n", p->pid);
#endif
#endif
*/
                p->pid = 0;
                p->parent = 0;
                p->name[0] = 0;
                p->killed = 0;
                p->state = UNUSED;
                release(&ptable.lock);
                return pid;
            }
        }

        // No point waiting if we don't have any children.
        if(!havekids || curproc->killed){
            release(&ptable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(curproc, &ptable.lock);  //DOC: wait-sleep
    }
}

int set_priority(int new_priority, int pid){
    if(new_priority > 100)
        return -1;
    if(new_priority < 0)
        return -1;
    int old_priority;
    struct proc *p;
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->pid == pid)
            break;
    }
    old_priority = p->priority;
    p->priority = new_priority;
    release(&ptable.lock);
    if(new_priority < old_priority)
        yield();
    return old_priority;
}

int ps_func() {
    int num_proc=0;
    struct proc *p;
    acquire(&ptable.lock);
#ifdef MLFQ
    //cprintf("PID  Priority  State  r_time  w_time  n_run  cur_q  q0  q1  q2  q3  q4\n");
    cprintf("%s %s   %s   %s %s %s %s  %s  %s   %s   %s   %s\n", "PID", "Priority", "State", "r_time", "w_time", "n_run", "cur_q", "q0", "q1", "q2", "q3", "q4");
#else
    //cprintf("PID  Priority  State  r_time  w_time  n_run\n");
    cprintf("%s %s %s %s %s %s\n", "PID", "Priority", "State", "r_time", "w_time", "n_run");
#endif
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state != UNUSED) {
            cprintf("%d     ",p->pid);
            cprintf("%d     ",p->priority);
            if(p->state == RUNNING)
                cprintf(" %s    ", "RUNNING");
            if(p->state == EMBRYO)
                cprintf(" %s     ", "EMBRYO");
            if(p->state == SLEEPING)
                cprintf(" %s   ", "SLEEPING");
            if(p->state == RUNNABLE)
                cprintf(" %s   ", "RUNNABLE");
            if(p->state == ZOMBIE)
                cprintf(" %s   ", "ZOMBIE");
            cprintf("  %d   ",p->rtime);
            cprintf("  %d   ",p->cur_q_waiting_time);
            cprintf("  %d   ",p->n_run);
#ifdef MLFQ
            cprintf("%d   ",p->prev_q);
            cprintf(" %d   ",p->q_ticks[0]);
            cprintf(" %d   ",p->q_ticks[1]);
            cprintf(" %d   ",p->q_ticks[2]);
            cprintf(" %d   ",p->q_ticks[3]);
            cprintf(" %d",p->q_ticks[4]);
#endif
            cprintf("\n");
            num_proc++;
        }
    }
    release(&ptable.lock);
    return num_proc;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void) {
    struct proc *p;
    struct cpu *c = mycpu();
    c->proc = 0;

    for(;;) {
#ifdef RR
        // Enable interrupts on this processor.
        sti();
        // Loop over process table looking for process to run.
        acquire(&ptable.lock);
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
            if(p->state != RUNNABLE)
                continue;
            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            c->proc = p;
            p->n_run++;
            switchuvm(p);
            p->state = RUNNING;
            swtch(&(c->scheduler), p->context);
            switchkvm();
            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
        }
        release(&ptable.lock);
#elif FCFS
        // Enable interrupts on this processor.
        sti();
        // Loop over process table looking for process to run.
        acquire(&ptable.lock);
        int minval = ticks+75;
        struct proc *p1=0;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
            if(p->state != RUNNABLE)
                continue;
            if(p->last_runtime < minval) {
                minval = p->last_runtime;
                p1=p;
            }
        }
        if (p1 == 0) {
            release(&ptable.lock);
            continue;
        }
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p1;
        p1->n_run++;
        p1->last_runtime = ticks;
        switchuvm(p1);
        p1->state = RUNNING;
        swtch(&(c->scheduler), p1->context);
        switchkvm();
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        release(&ptable.lock);
#elif PBS
        // Enable interrupts on this processor.
        sti();
        // Loop over process table looking for process to run.
        acquire(&ptable.lock);
        int minpri = 101;
        struct proc *p1;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if(p->state != RUNNABLE)
                continue;
            if(p->priority < minpri) {
                minpri = p->priority;
            }
        }
        if (minpri == 101) {
            release(&ptable.lock);
            continue;
        }
        int br_flag=0;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if(p->state != RUNNABLE || p->priority != minpri)
                continue;
            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            c->proc = p;
            p->n_run++;
            switchuvm(p);
            p->state = RUNNING;
            swtch(&(c->scheduler), p->context);
            switchkvm();
            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
            br_flag=0;
            for(p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++) {
                if(p1->state != RUNNABLE)
                    continue;
                if(p1->priority < minpri) {
                    br_flag = 1;
                    break;
                }
            }
            if(br_flag) {
                break;
            }
        }
        release(&ptable.lock);
#elif MLFQ
        // Enable interrupts on this processor.
        sti();
        // Loop over process table looking for process to run.
        struct proc *p1 = 0;
        int found_proc = 0;
        acquire(&ptable.lock);
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if(p->state == RUNNABLE && (p->cur_q_waiting_time >= q_age[p->cur_q]) && p->cur_q > 0) { //If the process has aged in current queue
#ifdef DEBUG_Y
                cprintf("Process %d has aged, value - %d, age for queue %d - %d, moving to %d\n", p->pid, ticks - p->q_join_time, p->prev_q, q_age[p->prev_q], p->prev_q-1);
#endif
#ifdef DEBUG_P
                cprintf("%d %d %d %d\n", ticks, p->pid, p->prev_q, p->prev_q-1);
#endif
                p->cur_q_ticks = 0; //Ticks in current queue in this round
                p->cur_q_waiting_time = 0;
                p->q_join_time = ticks; //Join time
                p->cur_q--; //Decrease the queue
                p->prev_q--;
            }
        }
        for(int it=0; it<=4; it++) {
            int min_join_time = -1;
            if(found_proc == 0) {
                for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
                    if(p->state == RUNNABLE && p->cur_q == it) { //Loop through RUNNABLE processes and which are not waiting
                        if(p->prev_q == 4) { //If the process is in the 4th queue
                            //Choose the one with min_join_time to simulate a queue
                            if(min_join_time == -1) {
                                min_join_time = p->q_join_time;
                                p1=p;
                                found_proc = 1;
                            }
                            else if(min_join_time > p->q_join_time) {
                                min_join_time = p->q_join_time;
                                p1=p;
                                found_proc = 1;
                            }
                        }
                        else {
                            p1=p;
                            found_proc = 1;
                        }
                    }
                }
            }
        }
        if (found_proc == 0) {
            release(&ptable.lock);
            continue;
        }
#ifdef DEBUG_Y
        cprintf("Process %d is picked from %d\n", p1->pid, p1->cur_q);
#endif
        //p1->cur_q_ticks++;
        p1->n_run++;
        //p->prev_q = p->cur_q;
        //p1->q_ticks[p1->cur_q]++;
        p1->cur_q = -1; //Remove from queue
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p1;
        switchuvm(p1);
        p1->state = RUNNING;
        swtch(&(c->scheduler), p1->context);
        switchkvm();
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        //TODO: RUNNABLE || SLEEPING 
        if(p1->state == RUNNABLE || p1->state == SLEEPING) {
            p->cur_q_ticks=0; //Ticks in current queue in this round
            p->cur_q_waiting_time = 0;
            p->q_join_time = ticks; //Join time
            p1->cur_q = p1->prev_q;
        }
        release(&ptable.lock);
#endif
    }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void) {
    int intena;
    struct proc *p = myproc();

    if(!holding(&ptable.lock))
        panic("sched ptable.lock");
    if(mycpu()->ncli != 1)
        panic("sched locks");
    if(p->state == RUNNING)
        panic("sched running");
    if(readeflags()&FL_IF)
        panic("sched interruptible");
    intena = mycpu()->intena;
    swtch(&p->context, mycpu()->scheduler);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void) {
    acquire(&ptable.lock);  //DOC: yieldlock
    myproc()->state = RUNNABLE;
    sched();
    release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void) {
    static int first = 1;
    // Still holding ptable.lock from scheduler.
    release(&ptable.lock);

    if (first) {
        // Some initialization functions must be run in the context
        // of a regular process (e.g., they call sleep), and thus cannot
        // be run from main().
        first = 0;
        iinit(ROOTDEV);
        initlog(ROOTDEV);
    }

    // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk) {
    struct proc *p = myproc();

    if(p == 0)
        panic("sleep");

    if(lk == 0)
        panic("sleep without lk");

    // Must acquire ptable.lock in order to
    // change p->state and then call sched.
    // Once we hold ptable.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with ptable.lock locked),
    // so it's okay to release lk.
    if(lk != &ptable.lock){  //DOC: sleeplock0
        acquire(&ptable.lock);  //DOC: sleeplock1
        release(lk);
    }
    // Go to sleep.
    p->chan = chan;
    p->state = SLEEPING;

    sched();

    // Tidy up.
    p->chan = 0;

    // Reacquire original lock.
    if(lk != &ptable.lock){  //DOC: sleeplock2
        release(&ptable.lock);
        acquire(lk);
    }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1(void *chan) {
    struct proc *p;

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if(p->state == SLEEPING && p->chan == chan) {
            p->state = RUNNABLE;
#ifdef MLFQ //Re-add the process back to its previous queue as it is woken up now
            p->q_join_time = ticks;
            p->cur_q_ticks = 0;
            p->cur_q_waiting_time = 0;
            p->cur_q = p->prev_q;
#endif
        }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan) {
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid) {
    struct proc *p;

    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->pid == pid){
            p->killed = 1;
            // Wake process from sleep if necessary.
            if(p->state == SLEEPING) {
                p->state = RUNNABLE;
#ifdef MLFQ //Re-add the process back to its previous queue as it is woken up now
                p->q_join_time = ticks;
                p->cur_q_ticks = 0;
                p->cur_q_waiting_time = 0;
                p->cur_q = p->prev_q;
#endif
            }
            release(&ptable.lock);
            return 0;
        }
    }
    release(&ptable.lock);
    return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void) {
    static char *states[] = {
        [UNUSED]    "unused",
        [EMBRYO]    "embryo",
        [SLEEPING]  "sleep ",
        [RUNNABLE]  "runble",
        [RUNNING]   "run   ",
        [ZOMBIE]    "zombie"
    };
    int i;
    struct proc *p;
    char *state;
    uint pc[10];

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state == UNUSED)
            continue;
        if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        cprintf("%d %s %s", p->pid, state, p->name);
        if(p->state == SLEEPING){
            getcallerpcs((uint*)p->context->ebp+2, pc);
            for(i=0; i<10 && pc[i] != 0; i++)
                cprintf(" %p", pc[i]);
        }
        cprintf("\n");
    }
}
