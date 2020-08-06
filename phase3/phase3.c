/* ------------------------------------------------------------------------
   phase3.c

   University of Arizona
   Computer Science 452

   Austin Ramsey
   Raymond Howard
   ------------------------------------------------------------------------ */

#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <stdlib.h>
#include <usyscall.h>
#include <sems.h>
#include <libuser.h>
#include <string.h>


/* -------------------------- Globals ------------------------------------- */

// Process table, phase 3 version
proc_struct3 procTable[MAXPROC]; 

// Semaphore table
sem_struct semTable[MAXSEMS]; 

extern int start3(char *arg);


/* ------------------------- Prototypes ----------------------------------- */
void set_user_mode();
void spawn(sysargs *args);
int start2(char *); 
int spawn_launch(char *arg);
void wait(sysargs *args);
void terminate(sysargs *args);
void semCreate(sysargs *args);
void semP(sysargs *args);
void semV(sysargs *args);
void semFree(sysargs *args);
void getPID(sysargs *args);
void getTimeOfDay(sysargs *args);
void cpuTime(sysargs *args);
int spawn_real(char *name, int (*func)(char *), char *arg, int stack_size, int priority);
int wait_real(int *status);
void nullsys3(sysargs *args);
void check_kernel_mode(char * processName);
void add_child_to_list(proc_ptr3 child);
void remove_from_child_list(proc_ptr3 process);
void add_to_sem_block_list(proc_ptr3 process, int sem_index);


/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start2
   Purpose - Initializes process table, semaphore table, and system call 
             vector.
   Parameters - arg: function arguments. not used.
   Returns - int: zero for a normal quit. Should not be used.
   Side Effects - lots since it initializes the phase3 data structures.
   ----------------------------------------------------------------------- */
int start2(char *arg) {
	
    int	pid;
    int	status;
    int i;

    // Verify kernel mode
    check_kernel_mode("start2");
    
    /*
     * Data structure initialization as needed...
     */
	 
    // Init all structs in the process table to EMPTY
    for (i = 0; i < MAXPROC; i++) {
        procTable[i].status = EMPTY;
    }

    // Init semaphore table to EMPTY
    for (i = 0; i < MAXSEMS; i++) {
        semTable[i].status = EMPTY;
    }
	
    // Set 'nullsys3' for each sys_vec index
    for (i = 0; i < MAXSYSCALLS; i++) {
        sys_vec[i] = nullsys3;
    }

    // Init sys_vec to system call functions
    sys_vec[SYS_SPAWN] = spawn;
    sys_vec[SYS_WAIT] = wait;
    sys_vec[SYS_TERMINATE] = terminate;
    sys_vec[SYS_SEMCREATE] = semCreate;
    sys_vec[SYS_SEMP] = semP;
    sys_vec[SYS_SEMV] = semV;
    sys_vec[SYS_SEMFREE] = semFree;
    sys_vec[SYS_GETPID] = getPID;
    sys_vec[SYS_GETTIMEOFDAY] = getTimeOfDay;
    sys_vec[SYS_CPUTIME] = cpuTime;

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscall_handler; spawn_real is the function that
     * contains the implementation and is called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes usyscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawn_real().
     *
     * Here, we only call spawn_real(), since we are already in kernel mode.
     *
     * spawn_real() will create the process by using a call to fork1 to
     * create a process executing the code in spawn_launch().  spawn_real()
     * and spawn_launch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawn_real() will
     * return to the original caller of Spawn, while spawn_launch() will
     * begin executing the function passed to Spawn. spawn_launch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawn_real() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and 
     * return to the user code that called Spawn.
     */
    pid = spawn_real("start3", start3, NULL, 4*USLOSS_MIN_STACK, 3);
	
    // Check if we failed to create the start3 process
    if (pid < 0) {
        quit(pid);
    }
	
    pid = wait_real(&status);
	
	// failed to join with start3 child process
    if (pid < 0) {
        quit(pid);
    }

	quit(0);
    return 0;
} /* start2 */


/* ------------------------------------------------------------------------
   Name - spawn
   Purpose - Creates user-level process after checking passed in parameters
   Parameters - sysargs *args, the arguments that are passed from libuser.c
             arg1: address of the function to spawn
             arg2: parameter to pass to the spawned function
             arg3: stack size bytes
             arg4: priority
             arg5: process name
   Returns - N/A, just sets the following arg values:
             arg1: PID of the new process or -1 if a process could not be created
             arg2: -1 if illegal values are given as input; 0 otherwise
   Side Effects - spawn_real will be called using the parameters above
   ----------------------------------------------------------------------- */
void spawn(sysargs *args) {
    long pid;

    // Invalid argument checks
    if ((long) args->number != SYS_SPAWN) {
        args->arg4 = (void *) -1;
        return;
    }

    if ((long) args->arg3 < USLOSS_MIN_STACK) {
        args->arg4 = (void *) -1;
        return;
    }

    if ((long) args->arg4 > MINPRIORITY || (long) args->arg4 < MAXPRIORITY ) {
        args->arg4 = (void *) -1;
        return;
    }

    pid = spawn_real((char *) args->arg5, args->arg1, args->arg2, (long) args->arg3, (long) args->arg4);
    
    // new process PID, or -1
    args->arg1 = (void *) pid;

    // Inputs to function are valid
    args->arg4 = (void *) 0; 

    set_user_mode();
}


/* ------------------------------------------------------------------------
   Name - spawn_real
   Purpose - This is called by 'spawn' in order to create a user-level process using phase1 functions 
   Parameters - func: address of the function to spawn
                arg: parameter to be passed to the spawned function
                stack_size: stack size bytes
                priority: priority
                name: process name
   Returns - int: PID of the new process or -1 if a process couldn't be created
   Side Effects - new process info is added to the process table
   ----------------------------------------------------------------------- */
int spawn_real(char *name, int (* func)(char *), char *arg, int stack_size, int priority) {

    int child_PID;  
    int mailbox_ID;

    child_PID = fork1(name, spawn_launch, arg, stack_size, priority);
    
    // Check if fork1 failed
    if (child_PID < 0) {  
        return child_PID;
    }

    // Check for higher priority parent, so that the parent can init the process table
    if (procTable[child_PID % MAXPROC].status == EMPTY) {
        mailbox_ID = MboxCreate(0, 0);
        procTable[child_PID % MAXPROC].mbox_id = mailbox_ID;
        procTable[child_PID % MAXPROC].status = ACTIVE;
    } else {
        mailbox_ID = procTable[child_PID % MAXPROC].mbox_id;
    }

    // Now we can add the child info to the process table
    procTable[child_PID % MAXPROC].pid = child_PID;
    strcpy(procTable[child_PID % MAXPROC].name, name);
    procTable[child_PID % MAXPROC].priority = priority;
    procTable[child_PID % MAXPROC].func = func;
    procTable[child_PID % MAXPROC].child_proc_ptr = NULL;
    procTable[child_PID % MAXPROC].next_sibling_ptr = NULL;
    procTable[child_PID % MAXPROC].next_sem_block = NULL;
    if (arg == NULL) {
        procTable[child_PID % MAXPROC].start_arg[0] = 0;
    } else {
        strcpy(procTable[child_PID % MAXPROC].start_arg, arg);
    }
    procTable[child_PID % MAXPROC].stack_size = stack_size;

    // We don't want start2 to add its info to the process table, 
    // so verify that this PID doesn't match its PID before adding anything
    if (getpid() != START2_PID) {
        // This isn't start2, add info to table
        procTable[child_PID % MAXPROC].parent_ptr = &procTable[getpid() % MAXPROC];
        add_child_to_list(&procTable[child_PID % MAXPROC]);
    }

    // Could be children that need to be awaken
    MboxCondSend(mailbox_ID, NULL, 0); 

    return child_PID;
}


/* ------------------------------------------------------------------------
   Name - spawn_launch
   Purpose - This function is called by the phase1 launch to start user-level process code passed to spawn
   Parameters - arg: the parameter passed to the spawned function
   Returns - N/A
   Side Effects - If the process doesn't call 'Terminate' then spawn_launch will call 'Terminate'
   ----------------------------------------------------------------------- */
int spawn_launch(char *arg) {
    int mailbox_ID;           
    int pid = getpid();     
    int func_return_value; 

    // If the parent hasn't setup a process table yet, then the child will do it
    if (procTable[pid % MAXPROC].status == EMPTY) {
        procTable[pid % MAXPROC].status = ACTIVE;
        mailbox_ID = MboxCreate(0, 0);
        procTable[pid % MAXPROC].mbox_id = mailbox_ID;
        MboxReceive(mailbox_ID, NULL, 0);
    }

    // Terminate if the child was zapped while it was blocked waiting for the parent
    if (is_zapped()) {
        set_user_mode();
        Terminate(99);
    }

    set_user_mode();

    // Child process execution
    func_return_value = procTable[pid % MAXPROC].func(procTable[pid % MAXPROC].start_arg);
    
    Terminate(func_return_value);
    
    return 0;
}


/* ------------------------------------------------------------------------
   Name - wait
   Purpose - This function waits for a child process to terminate
   Parameters - *args, sysargs
   Returns - N/A, sets the argument values:
             arg1: the process ID of the terminating child process
             arg2: the termination code of the child process
   Side Effects - the process will become blocked if no children have terminated
   ----------------------------------------------------------------------- */
void wait(sysargs *args) {
    int status;
    long kid_pid;
 
    // Verify the system call
    if ((long) args->number != SYS_WAIT) {
        args->arg2 = (void *) -1;
        return;
    }

    kid_pid = wait_real(&status);

    // Set status to 'ACTIVE'
    procTable[getpid() % MAXPROC].status = ACTIVE;
    
    if (kid_pid == -2) {
        args->arg1 = (void *) 0;
        args->arg2 = (void *) -2;
    } else {
        args->arg1 = (void *) kid_pid;
        args->arg2 = ((void *) (long) status);
    }

    set_user_mode();
}


/* ------------------------------------------------------------------------
   Name - wait_real
   Purpose - This function is called by 'wait'. It sets the process table status of the calling process then calls phase1 'join'.
   Parameters - status: termination code of the child process
   Returns - int: the process ID of terminating child process
   Side Effects - the process  will be blocked if no children have terminated
   ----------------------------------------------------------------------- */
int wait_real(int *status) {
    procTable[getpid() % MAXPROC].status = WAIT_BLOCK;
    return join(status);
}


/* ------------------------------------------------------------------------
   Name - terminate
   Purpose - This function terminates the invoking process and also all of its children 
             It then synchronizes with the parent’s 'Wait' call 
             The processes are terminated by zapping
   Parameters - sysargs *args, the arguments that are passed from libuser.c
                arg1: termination code for the process
   Returns - N/A
   Side Effects - the process status in the process table is then set to EMPTY
   ----------------------------------------------------------------------- */
void terminate(sysargs *args) {
    // parent = the calling process
    proc_ptr3 parent = &procTable[getpid() % MAXPROC]; 

    // Check for children and zap them if they exist
    if (parent->child_proc_ptr != NULL) {
        // Iterate through children list and zap each one if existing
        while (parent->child_proc_ptr != NULL) {
            zap(parent->child_proc_ptr->pid);
        }
    }

    // The children will call terminate when they remove themselves from the parents
    if (parent->pid != START3_PID && parent->parent_ptr != NULL) {
        remove_from_child_list(&procTable[getpid() % MAXPROC]);
    }

    // Change the status to EMPTY, we shouldn't be using it anymore
    parent->status = EMPTY;

    // Return the ID of the terminating child process
    quit(((int) (long) args->arg1));
}


/* ------------------------------------------------------------------------
   Name - semCreate
   Purpose - This function creates a user-level semaphore
   Parameters - sysargs *args, the arguments that are passed from libuser.c
                args->arg1: initial semaphore value
   Returns - N/A, just sets arg values
             arg1: index of semaphore
             arg4: -1 if initial value is negative or no semaphore available; 0 otherwise.
   Side Effects - a semaphore will created and then added to the semaphore table
   ----------------------------------------------------------------------- */
void semCreate(sysargs *args) {
    
    // This is where we will store the index of the new semaphore
    int index; 

    // Ensure that the initial semaphore value is not negative 
    if ((long) args->arg1 < 0) {
        args->arg4 = ((void *) (long) -1);
        return;
    }

    // Try to find an empty semaphore in the table
    for (index = 0; index < MAXSEMS; index++) {
        if (semTable[index].status == EMPTY) {
            break;
        }
    }

    // There are no available semaphores
    if (index == MAXSEMS) {
        args->arg4 = ((void *) (long) -1);
        return;
    }

    // Now we can initialize the values of the semaphore
    semTable[index].status = ACTIVE;
    semTable[index].count = (long) args->arg1;
    semTable[index].blocked_list = NULL;
    semTable[index].mbox_id = MboxCreate(1, 0);

    // Modify the argument values with our found values
    args->arg1 = ((void *) (long) index);
    args->arg4 = ((void *) (long) 0);

    set_user_mode();
}


/* ------------------------------------------------------------------------
   Name - semP
   Purpose - This function decrements the semaphore count by one 
             It then blocks the process if the count is zero
   Parameters - sysargs *args, the arguments that are passed from libuser.c
                arg1: the index of the semaphore in the semaphore table
   Returns - N/A, just sets arg values
             arg4: sets -1 if the semaphore index is invalid, 0 if valid
   Side Effects - This function places a process on the block semphore list only if the count is zero
   ----------------------------------------------------------------------- */
void semP(sysargs *args) {
    
    int sem_index = ((int) (long) args->arg1); 

    // Check for invalid index, set arg4 accordingly 
    if (sem_index < 0 || sem_index > MAXSEMS) {
        args->arg4 = ((void *) (long) -1);
        return;
    }

    sem_struct *semaphore = &semTable[sem_index];

    // Check if requested semaphore is not an active one, if so set arg4 accordingly
    if (semaphore->status == EMPTY) {
        args->arg4 = ((void *) (long) -1);
        return;
    }

    // The process enters semaphore critical section
    MboxSend(semaphore->mbox_id, NULL, 0);

    // Now we can block the process on the semaphore block list
    if (semaphore->count < 1) {

        // Add to the sem block list
        add_to_sem_block_list(&procTable[getpid() % MAXPROC], sem_index);
        
        // We can release the mutex
        MboxReceive(semaphore->mbox_id, NULL, 0); 

        // Block the process on the private mailbox
        MboxReceive(procTable[getpid() % MAXPROC].mbox_id, NULL, 0);

        // Check if the mailbox was released while a process was waiting to enter
        if (semaphore->status == EMPTY) {
            set_user_mode();
            Terminate(1);
        }

    } else {
        MboxReceive(semaphore->mbox_id, NULL, 0); // release mutex
    }

    // Decrement the sem count
    semaphore->count--;

    // Set arg4 as 'success'
    args->arg4 = ((void *) (long) 0);
    
    set_user_mode();
}


/* ------------------------------------------------------------------------
   Name - semV
   Purpose - this function increments the semaphore count by one
   Parameters - sysargs *args, the arguments that are passed from libuser.c
   Returns - N/A, sets arg values
   Side Effects - this function will wake up any blocked semaphores if they exist
   ----------------------------------------------------------------------- */
void semV(sysargs *args) {
    int sem_index = ((int) (long) args->arg1);
	
    // Verify the index is valid, if not set arg4 accordingly
    if (sem_index < 0 || sem_index > MAXSEMS) {
        args->arg4 = ((void *) (long) -1);
        return;
    }

    // Pull sem from table
    sem_struct *semaphore = &semTable[sem_index];

    // Check that the semaphore is active, if not then set arg4 accordingly 
    if (semaphore->status == EMPTY) {
        args->arg4 = ((void *) (long) -1);
        return;
    }
	
    // Enter the mutex area, this is where no other processes may enter while we are in
    MboxSend(semaphore->mbox_id, NULL, 0);
	
    // Increment the semaphore count
    semaphore->count++;
    
    // We can wake up the first blocked process on the list if one is there 
    if (semaphore->blocked_list != NULL) {

        int blockProcmbox_id = semaphore->blocked_list->mbox_id;
        semaphore->blocked_list = semaphore->blocked_list->next_sem_block;
        
        //Release mutex
        MboxReceive(semaphore->mbox_id, NULL, 0);
        MboxSend(blockProcmbox_id, NULL, 0);

    } else {

        MboxReceive(semaphore->mbox_id, NULL, 0);

    }

    // Set arg4 as sucess
    args->arg4 = ((void *) (long) 0);

    set_user_mode();
}


/* ------------------------------------------------------------------------
   Name - semFree
   Purpose - This function frees a semaphore and makes it available in the table
   Parameters - sysargs *args, this is the arguments passed from libuser.c
   Returns - N/A, just sets arg values
   Side Effects - this function modifies the semaphore table 
   ----------------------------------------------------------------------- */
void semFree(sysargs *args) {

    // Pull index from args
    int sem_index = ((int) (long) args->arg1);

    // Check if the requested semaphore is valid, if not then set arg4 accordingly
    if (sem_index < 0 || sem_index > MAXSEMS) {
        args->arg4 = ((void *) (long) -1);
        return;
    }

    // Pull semaphore from the table
    sem_struct *semaphore = &semTable[sem_index];

    // Verify that the semaphore is not being used, if so then set arg4 accordingly
    if (semaphore->status == EMPTY) {
        args->arg4 = ((void *) (long) -1);
        return;
    }

    // Set status to EMPTY
    semaphore->status = EMPTY;

    // Handle the blocked list
    if (semaphore->blocked_list != NULL) {
        
        // Continue waking up the semaphore's blocked processes if they exist
        while (semaphore->blocked_list != NULL) {
            int privatembox_id = semaphore->blocked_list->mbox_id;
            MboxSend(privatembox_id, NULL, 0);
            semaphore->blocked_list = semaphore->blocked_list->next_sem_block;
        }

        args->arg4 = ((void *) (long) 1);

    } else {

        args->arg4 = ((void *) (long) 0);
    }

    set_user_mode();
}


// Halt USLOSS if not in kernel mode
void check_kernel_mode(char * processName) {
    if((PSR_CURRENT_MODE & psr_get()) == 0) {
        console("check_kernal_mode(): called while in user mode, by process %s. Halting...\n", processName);
        halt(1);
    }
}


// Sets arg1 from phase1 'getpid()'
void getPID(sysargs *args) {
    args->arg1 = ((void *) (long) getpid());
    set_user_mode();
}


// Returns the sys_clock time
void getTimeOfDay(sysargs *args) {
    args->arg1 = ((void *) (long) sys_clock());
    set_user_mode();
}


// Returns the phase1 readtime value
void cpuTime(sysargs *args) {
    args->arg1 = ((void *) (long) readtime());
    set_user_mode();
}


// Sets the mode from kernel mode to user mode 
void set_user_mode() {
    psr_set(psr_get() & 14);
}


// Handles invalid syscalls 
void nullsys3(sysargs *args) {
    console("nullsys3(): Invalid syscall %d\n.", args->number);
    console("nullsys3(): process %d terminating\n", getpid());
    Terminate(1);
}


/* ------------------------------------------------------------------------
   Name - add_child_to_list
   Purpose - This process will insert a child at the end of the parent child list
   Parameters - child, process to add
   Returns - N/A 
   Side Effects - modifies parent child list
   ----------------------------------------------------------------------- */
void add_child_to_list(proc_ptr3 child) {
    proc_ptr3 parent = &procTable[getpid() % MAXPROC];
    
    // If the process has no children add it to the beginning 
    if (parent->child_proc_ptr == NULL) {
        
        parent->child_proc_ptr = child;
    
    } else {
        
        // Add the child to end of list
        proc_ptr3 sibling = parent->child_proc_ptr;
        while (sibling->next_sibling_ptr != NULL) {
            sibling = sibling->next_sibling_ptr;
        }
        sibling->next_sibling_ptr = child;
    
    }
}


/* ------------------------------------------------------------------------
   Name - remove_from_child_list
   Purpose - This process finds a process in a parent child list, then removes the process, and reassigns
             all important processes
   Parameters - process, process to delete 
   Returns - N/A
   Side Effects - modifies parent child list
   ----------------------------------------------------------------------- */
void remove_from_child_list(proc_ptr3 process) {
    proc_ptr3 temp = process;
    
    // Check if the process is at the beginning of linked list
    if (process == process->parent_ptr->child_proc_ptr) {

        process->parent_ptr->child_proc_ptr = process->next_sibling_ptr;

    } else { 
        
        // The process is in the middle or the end of the linked list
        temp = process->parent_ptr->child_proc_ptr;

        while (temp->next_sibling_ptr != process) {
            temp = temp->next_sibling_ptr;
        }

        temp->next_sibling_ptr = temp->next_sibling_ptr->next_sibling_ptr;
    }    
}


/* ------------------------------------------------------------------------
   Name - add_to_sem_block_list
   Purpose - This process inserts a process into a semaphore's block list
   Parameters - 'process' to be inserted, 'index' of semaphore
   Returns - N/A
   Side Effects - the semaphore's block list will be modified
   ----------------------------------------------------------------------- */
void add_to_sem_block_list(proc_ptr3 process, int sem_index) {

    // Pull the semaphores block list
    proc_ptr3 block_list = semTable[sem_index].blocked_list;
    
    // Check if there are processes blocked 
    if (block_list == NULL) {
  
        semTable[sem_index].blocked_list = process;
    
    } else {

        proc_ptr3 temp = block_list;
	
        // Iterate to the end of the semaphore's block list
        
        while (temp->next_sem_block != NULL) {
            temp = temp->next_sem_block;
        }
        
        temp->next_sem_block = process;
    }
}
