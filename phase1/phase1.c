/* ------------------------------------------------------------------------
	phase1.c

	CSCV 452

	Austin Ramsay
	Raymond Howard
------------------------------------------------------------------------ */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <phase1.h>
#include "kernel.h"
#include "my_phase1.h"

/* -------------------------- Globals ------------------------------------- */

int debugflag = 0;

// The process table with a max number of slots defined by MAXPROC
proc_struct ProcTable[MAXPROC];

// Process lists
// The ReadyList process pointer will act as a linked list of 'ready' processes
// As a proc_ptr, contains the 'next_proc_ptr' pointer, etc.
static proc_ptr ReadyList;

// 'Current' is the running process managed by the dispatcher
proc_ptr Current;

// the next pid to be assigned
unsigned int next_pid = SENTINELPID;



/* ------------------------------------------------------------------------
|  Name - startup
|
|  Purpose - Initializes process lists and clock interrupt vector.
|            Start up sentinel process and the test process.
|
|  Parameters - none, called by USLOSS
|
|  Returns - nothing
|
|  Side Effects - lots, starts the whole thing
*------------------------------------------------------------------------- */
void startup() {
    int result; // Value returned by call to fork1()

    // Debug info 
    if (DEBUG && debugflag) {
        console("startup(): initializing process table, ProcTable[]\n");
    }

    // Initialize each slot on the process table
    int i;
    for (i = 0; i < MAXPROC; i++) {
        init_proc_table(i);
    }

    // Debug info 
    if (DEBUG && debugflag) {
        console("startup(): initializing the Ready list\n");
    }

    // Initialize Ready List
    ReadyList = NULL;

    // Initialize the clock interrupt handler
    int_vec[CLOCK_DEV] = clock_handler;

    // Debug info 
    if (DEBUG && debugflag) {
        console("startup(): calling fork1() for sentinel\n");
    }

    // Start the Sentinel process
    // If fork1 returns < 0, halt USLOSS
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, SENTINELPRIORITY);
    if (result < 0) {
        // Debug info
        if (DEBUG && debugflag) {
            console("startup(): fork1 of sentinel returned error, ");
            console("halting...\n");
        }

        // Halt USLOSS, can't continue without Sentinel process
        halt(1);
    }

    // Debug info
    if (DEBUG && debugflag) {
        console("startup(): calling fork1() for start1\n");
    }

    // Start the 'start1' process
    // If fork1 reutnrs < 0, halt USLOSS
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        // Debug info
        console("startup(): fork1 for start1 returned an error, ");
        console("halting...\n");
        
        // Halt USLOSS, 'start1' failed 
        halt(1);
    }

    // Debug info
    console("startup(): Should not see this message! ");
    console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
|  Name - finish
|
|  Purpose - Required by USLOSS
|
|  Parameters - none
|
|  Returns - nothing
|
|  Side Effects - none
*------------------------------------------------------------------------- */
void finish() {
    // Debug info
    if (DEBUG && debugflag) {
        console("Finish.\n");
    }
} /* finish */

/* ------------------------------------------------------------------------
|  Name - fork1
|
|  Purpose - Gets a new process from the process table and initializes
|            information of the process.  Updates information in the
|            parent process to reflect this child process creation.
|
|  Parameters - the process procedure address, the size of the stack and
|               the priority to be assigned to the child process.
|
|  Returns - The process id of the created child.
|            -1 if no child could be created or if priority is not between
|            max and min priority. A value of -2 is returned if stacksize is
|            less than USLOSS_MIN_STACK
|
|  Side Effects - ReadyList is changed, ProcTable is changed.
*-------------------------------------------------------------------------- */
int fork1(char *name, int (*start_func)(char *), char *arg, int stacksize, int priority) {

    // The location in process table to store PCB
    int proc_slot = -1; 

    // Ensure we are in kernel mode, halt if not 
    if( (PSR_CURRENT_MODE & psr_get()) == 0 ) {
        console("fork1(): called while in user mode, by process %d. Halting...\n", Current->pid);
        halt(1);
    }

    // Debug info
    if (DEBUG && debugflag) {
        console("fork1(): Process %s is disabling interrupts.\n", name);
    }

    // Disable clock interrupts before continuing
    disableInterrupts();

    // Debug info
    if (DEBUG && debugflag) {
        console("fork1(): creating process %s\n", name);
    }

    // Return -1 if the given priority is out of bounds
    if ((next_pid != SENTINELPID) && (priority > MINPRIORITY || priority < MAXPRIORITY)) {
        if (DEBUG && debugflag) {
            console("fork1(): Process %s priority is out of bounds!\n", name);
        }
        return -1;
    }

    // Return -2 if stack size is too small 
    if (stacksize < USLOSS_MIN_STACK) {
        if (DEBUG && debugflag) {
            console("fork1(): Process %s stack size too small!\n", name);
        }
        return -2;
    }

    // Find an empty slot in the process table using get_proc_slot() 
    proc_slot = get_proc_slot();
    if (proc_slot == -1) {
        if (DEBUG && debugflag) {
            console("fork1(): Process %s - no empty slot.\n", name);
        }
        return -1;
    }

    // Halt USLOSS if process name is too long
    if ( strlen(name) >= (MAXNAME - 1) ) {
        console("fork1(): Process name is too long.  Halting...\n");
        halt(1);
    }

    // Initializing proc_struct in ProcTable for index proc_slot 
    ProcTable[proc_slot].pid = next_pid;
    strcpy(ProcTable[proc_slot].name, name);
    ProcTable[proc_slot].start_func = start_func;

    // Initialization of the process slot info, and error checking for process argument 
    if (arg == NULL) {
        ProcTable[proc_slot].start_arg[0] = '\0';
    } else if ( strlen(arg) >= (MAXARG - 1) ) {
        console("fork1(): argument too long.  Halting...\n");
        halt(1);
    } else {
        strcpy(ProcTable[proc_slot].start_arg, arg);
    }
    ProcTable[proc_slot].stacksize = stacksize;
    if ((ProcTable[proc_slot].stack = malloc(stacksize)) == NULL) {
        console("fork1(): malloc fail!  Halting...\n");
        halt(1);
    }
    ProcTable[proc_slot].priority = priority;

    // Set parent, child, and sibling pointers 
    if (Current != NULL) {                      // Current is the parent process
        if (Current->child_proc_ptr == NULL) {  // Current has no children
            Current->child_proc_ptr = &ProcTable[proc_slot];
        } else {  // Current has children
            // Extract the 'Current's child process
            proc_ptr child = Current->child_proc_ptr;

            // Insert the new child at end of sibling list
            // Continue moving down the chain until we find an empty slot
            while (child->next_sibling_ptr != NULL) {
                child = child->next_sibling_ptr;
            }

            // We are at an empty slot, store the new child process
            child->next_sibling_ptr = &ProcTable[proc_slot];
        }
    }

    ProcTable[proc_slot].parent_ptr = Current;

    // Initialize context for this process, but use launch function pointer
    // for the initial value of the process's program counter (PC) 
    // The launch function is a wrapper to enable interrupts and then run the 
    // function set in 'start_func' of the new process with its argument of 'start_arg'
    // By the time the 'launch' function is called, the process created will be set as the current by dispatcher
    context_init(
        &(ProcTable[proc_slot].state), 
        psr_get(),
        ProcTable[proc_slot].stack,
        ProcTable[proc_slot].stacksize,
        launch
    );

    // p1_fork, for future phases
    p1_fork(ProcTable[proc_slot].pid); 

    // Make process ready and add to ready list 
    ProcTable[proc_slot].status = READY;
    add_proc_to_ready_list(&ProcTable[proc_slot]);

    // Increment for next process to start at this pid
    next_pid++;  

    // Sentinel doesn't call dispatcher when it is first created, handle it now
    if (ProcTable[proc_slot].pid != SENTINELPID) {
        dispatcher();
    }

    // Return the PID created
    return ProcTable[proc_slot].pid;
} /* End fork1 */

/*-------------------------------------------------------------------------
|  Name - launch
|
|  Purpose - Dummy function to enable interrupts and launch a given process
|            upon startup.
|
|  Parameters - none
|
|  Returns - nothing
|
|  Side Effects - enable interrupts
*-------------------------------------------------------------------------- */
void launch()
{
    int result;

    if (DEBUG && debugflag)
        console("launch(): started\n");

    // Enable interrupts
    enableInterrupts();

    // Call the function passed to fork1, and capture its return value
    result = Current->start_func(Current->start_arg);

    // Debug info
    if (DEBUG && debugflag) {
        console("launch(): Process %d returned to launch\n", Current->pid);
    }

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
|  Name - join
|  Purpose - Wait for a child process (if one has been forked) to quit.  If
|            one has already quit, don't wait.
|
|  Parameters - a pointer to an int where the termination code of the
|               quitting process is to be stored.
|
|  Returns - the process id of the quitting child joined on.
|            -1 if the process was zapped in the join
|            -2 if the process has no children
|
|  Side Effects - If no child process has quit before join is called, the
|                 parent is removed from the ready list and blocked.
*-------------------------------------------------------------------------- */
int join(int *status) {
    int child_pid = -3;  // The child PID to return
    
    // Use 'child' to store the child this process is joining with
    proc_ptr child;      

    // Ensure we are in kernel mode
    if( (PSR_CURRENT_MODE & psr_get()) == 0 ) {
        console("join(): called while in user mode, by process %d. Halting...\n", Current->pid);
        halt(1);
    }

    // Debug info
    if (DEBUG && debugflag) {
        console("join(): Process %s is disabling interrupts.\n", Current->name);
    }

    // Disable interrupts
    disableInterrupts();

    // Check if the running process even has a child process to join on
    if (Current->child_proc_ptr == NULL && Current->quit_child_ptr == NULL) {
        // Debug info
	if (DEBUG && debugflag) {
            console("join(): Process %s has no children.\n", Current->name);
        }

	// Return -2, running process has no children
	return -2;
    }
        

    // No children have called 'quit' yet
    if (Current->quit_child_ptr == NULL) {
        // The current process is now waiting for the child to call 'quit',
	// set status accordingly
	Current->status = JOIN_BLOCKED;

	// The process is blocked (waiting on an event, for child to call 'quit')
        remove_from_ready_list(Current);
        
	// Debug info
	if (DEBUG && debugflag) {
            console("join(): %s is JOIN_BLOCKED.\n", Current->name);
            dump_processes();
            print_ready_list();
        }

	// Call the dispatcher to set a new current process while this is blocked
        dispatcher();
    }
    
    // REMOVE
    // Todo: Debugging line
    printf("Returning here\n"); 

    // A child has quit and reactivated the parent 
    // The process is no longer JOIN_BLOCKED
    // Get the 'child' process
    child = Current->quit_child_ptr;

    // Debug info
    if (DEBUG && debugflag) {
        console("join(): Child %s has status of quit.\n", child->name);
        dump_processes();
        print_ready_list();
    }

    // Get the PID and quit status of the child process
    child_pid = child->pid;
    *status = child->quit_status;

    // Remove the child from the quit list
    remove_from_quit_list(child);

    // Re-initialize the PID on the process table since the child was removed
    init_proc_table(child_pid);

    /* Process was zapped while JOIN_BLOCKED */
    if(is_zapped()){
        return -1;
    }

    // Success, return child PID
    return child_pid;
} /* join */


/* ------------------------------------------------------------------------
|  Name - quit
|
|  Purpose - Kills the 'Current' process and notifies the parent of the death by
|            putting child quit info on the parents child completion code
|            list.
|
|  Parameters - the code to return to the grieving parent
|
|  Returns - nothing
|
|  Side Effects - changes the parent of pid child completion status list.
|
|  This function is ONLY called by the 'launch' function 
|  'launch' is used to start a process' function at fork
*-------------------------------------------------------------------------- */
void quit(int status) {
    int currentPID; // the PID of the currently running process

    // Ensure we are in kernel mode 
    if( (PSR_CURRENT_MODE & psr_get()) == 0 ) {
        console("quit(): called while in user mode, by process %d. Halting...\n", Current->pid);
        halt(1);
    }

    // Debug info
    if (DEBUG && debugflag) {
        console("quit(): Process %s is disabling interrupts.\n", Current->name);
    }

    // Disable interrupts
    disableInterrupts();

    // Debug info
    if (DEBUG && debugflag) {
        console("quit(): Quitting %s, status is %d.\n", Current->name, status);
    }

    // The process has an active child
    // Halt USLOSS
    if (Current->child_proc_ptr != NULL) {
        console("quit(): process %d, '%s', has active children. Halting...\n", Current->pid, Current->name);
        halt(1);
    }

    // Update the 'Current' process variables including: exit status, set status to QUIT
    // Remove from the ReadyList
    Current->quit_status = status;
    Current->status = QUIT;
    remove_from_ready_list(Current);

    // For all processes that zapped this process, add to ready list and
    // set status to READY. 
    if (is_zapped()) {
        unblock_zappers(Current->who_zapped);
    }

    // The process that is quitting is a child and has its own quit child 
    if (Current->parent_ptr != NULL && Current->quit_child_ptr != NULL) {

        // Clean up all children on child quit list 
        // Clean up includes removing the children from the list,
        // as well as initializing the child's process table slot
        while (Current->quit_child_ptr != NULL) {
            int child_pid = Current->quit_child_ptr->pid;
            remove_from_quit_list(Current->quit_child_ptr);
            init_proc_table(child_pid);
        }

        // Clean up self and activate parent by changing status to READY 
        Current->parent_ptr->status = READY;
        remove_from_child_list(Current);

        // Add self to the parent's quit child list
        add_to_quit_child_list(Current->parent_ptr);

        // Finally, add the parent back to the ready list
        add_proc_to_ready_list(Current->parent_ptr);
        
        // Only prints in debug mode
        print_ready_list();                        

        // Update the current PID 
        currentPID = Current->pid;

    // The process that is quitting is only a child 
    } else if (Current->parent_ptr != NULL) {
        
        // Add self to the parent's quit child list
        add_to_quit_child_list(Current->parent_ptr);
        
        // Clean up self
        remove_from_child_list(Current);
        
        // If the parent is JOIN_BLOCKED, activate the parent by changing status to READY,
        // and add to the Ready List
        if(Current->parent_ptr->status == JOIN_BLOCKED){
           add_proc_to_ready_list(Current->parent_ptr);
           Current->parent_ptr->status = READY;
        }

        // Only prints in debug mode
        print_ready_list();                        

    // The process that is quitting is only a parent 
    } else {

        // Clean up all children on child quit list
        // Clean up includes removing the children from the list,
        // as well as initializing the child's process table slot
        while (Current->quit_child_ptr != NULL) {
            int child_pid = Current->quit_child_ptr->pid;
            remove_from_quit_list(Current->quit_child_ptr);
            init_proc_table(child_pid);
        }

        // Update the current PID
        currentPID = Current->pid;

        // Clear the process table slot this process was occupying
        init_proc_table(Current->pid);
    }

    // p1_quit
    p1_quit(currentPID);
    
    // Debug info
    if (DEBUG && debugflag) {
        dump_processes();
    }

    // Call the dispatcher to determine the next process
    dispatcher();

} /* quit */

/* ------------------------------------------------------------------------
|  Name - zap
|
|  Purpose - Marks a process pid as being zapped. zape does not return until
|            the zapped process has called quit. USLOSS will halt if a
|            process tries to zap itself or attempts to zap a nonexistent
|            process.
|
|  Parameters - pid (IN) - The process to mark as zapped.
|
|  Returns - 0: The zapped process has called quit.
|           -1: The calling process itself was zapped while in zap.
|
|  Side Effects - The process being zapped zapped marker is set to true.
|                 The process calling zap is added to the zapped process's
|                 list of processes that have zapped it.
*-------------------------------------------------------------------------- */
int zap(int pid) {
    proc_ptr zap_ptr; // The process to zap

    /* Make sure PSR is in kernal mode */
    if( (PSR_CURRENT_MODE & psr_get()) == 0 ) {
        console("zap(): called while in user mode, by process %d."
                       " Halting...\n", Current->pid);
        halt(1);
    }
    if (DEBUG && debugflag) {
        console("zap(): Process %s is disabling interrupts.\n",
                       Current->name);
    }
    disableInterrupts();

    /* Current process tried to zap itself */
    if(Current->pid == pid) {
        console("zap(): process %d tried to zap itself."
                       "  Halting...\n", pid);
        halt(1);
    }

    /* Process to zap does not exist */
    if (ProcTable[pid % MAXPROC].status == EMPTY ||
            ProcTable[pid % MAXPROC].pid != pid) {

        console("zap(): process being zapped does not exist."
                       "  Halting...\n");
        halt(1);
    }

    /* Process to zap has finished running, but is still waiting for parent */
    if (ProcTable[pid % MAXPROC].status == QUIT) {
        if (DEBUG && debugflag) {
            console("zap(): process being zapped has quit but not"
                    " joined.\n");
        }

        /* Process was zapped by another process */
        if (is_zapped()) {
            return -1;
        }
     return 0;
    }
    if (DEBUG && debugflag) {
        console("zap(): Process %d is zapping process %d.\n",
                Current->pid, pid);
    }
    Current->status = ZAP_BLOCKED;
    remove_from_ready_list(Current);
    zap_ptr = &ProcTable[pid % MAXPROC];
    zap_ptr->zapped = 1;

    /* Add this process to the list of process who have zapped the process */
    if (zap_ptr->who_zapped == NULL) {
        zap_ptr->who_zapped = Current;
    } else {
        proc_ptr ptr = zap_ptr->who_zapped;
        zap_ptr->who_zapped = Current;
        zap_ptr->who_zapped->next_who_zapped = ptr;
    }
    dispatcher();
    if (is_zapped()) {
        return -1;
    }
    return 0;
}/* zap */

/*
 * is_zapped returns whether a process has been zapped.
 */
int is_zapped() {
    return Current->zapped;
}

/* ------------------------------------------------------------------------
|  Name - dispatcher
|
|  Purpose - dispatches ready processes.  The process with the highest
|            priority (the first on the ready list) is scheduled to
|            run.  The old process is swapped out and the new process
|            swapped in.
|
|  Parameters - none
|
|  Returns - nothing
|
|  Side Effects - the context of the machine is changed
|
|  Dispatcher is run at each time_slice()
|  
|  Checking for higher priority processes in the Ready List, determine to keep running process running
|  
|  Perform context switch if higher priority process is available
*------------------------------------------------------------------------- */
void dispatcher(void) {
    // Debug info
    if (DEBUG && debugflag) {
        console("dispatcher(): started.\n");
    }

    // Dispatcher is called for the first time for starting process (start1) 
    if (Current == NULL) {
	// Set 'Current' as 'ReadyList'
        Current = ReadyList;

	// Debug info
        if (DEBUG && debugflag) {
            console("dispatcher(): dispatching %s.\n", Current->name);
        }

	// Set the Current process start time to now
        Current->start_time = sys_clock();

        // Enable Interrupts - returning to user code 
        psr_set( psr_get() | PSR_CURRENT_INT );
        
	// Perform context switch
	// Current->state is a context containing the function pointer for the process
	context_switch(NULL, &Current->state);
    
    // There is a current running process
    } else {
	// Set the current running process into the 'old' process pointer	
        proc_ptr old = Current;

	// Change status from RUNNING to READY
        if (old->status == RUNNING) {
            old->status = READY;
        }
	
	// Set the Current process as the next in the Ready List
        Current = ReadyList;
	
	// Remove the new Current process from the Ready List
        remove_from_ready_list(Current);

	// Set the new Current process status to RUNNING
        Current->status = RUNNING;

	// Add the new Current process back to the end of the Ready List
        add_proc_to_ready_list(Current);

	// Debug info
        if (DEBUG && debugflag) {
            console("dispatcher(): dispatching %s.\n", Current->name);
        }

	// Set the Current process start time to now
        Current->start_time = sys_clock();

	// p1_switch 
        p1_switch(old->pid, Current->pid);

        // Enable Interrupts - returning to user code 
	enableInterrupts();

	// Perform context switch from old process to new current process
	// Current->state is a context containing the function pointer for the process
        context_switch(&old->state, &Current->state);
    }

    // Debug info
    if (DEBUG && debugflag){
        console("dispatcher(): Printing process table");
        dump_processes();
    }

} /* dispatcher */


/* ------------------------------------------------------------------------
|  Name - sentinel
|
|  Purpose - The purpose of the sentinel routine is two-fold.  One
|            responsibility is to keep the system going when all other
|            processes are blocked.  The other is to detect and report
|            simple deadlock states.
|
|  Parameters - none
|
|  Returns - nothing
|
|  Side Effects -  if system is in deadlock, print appropriate error and halt.
*------------------------------------------------------------------------- */
int sentinel (char *dummy) {
    if (DEBUG && debugflag) {
        console("sentinel(): called\n");
    }
    while (1) {
        // Todo: Reimplement check_deadlock()? 
        check_deadlock();
        if (DEBUG && debugflag) {
            console("sentinel(): before WaitInt()\n");
        }
        waitint();
    }
} /* sentinel */

/* ------------------------------------------------------------------------
|  Name - check_deadlock
|
|  Purpose - Checks to determine if a deadlock has occured. In phase1, a
|            deadlock will occur if check_deadlock is called and there
|            are any processes, other then Sentinel, with a status other
|            then empty in the process table.
|
|  Parameters - none
|
|  Returns - nothing
|
|  Side Effects - The USLOSS simulation is terminated. Either with an exit
|                 code of 0, if all process completed normally or an exit
|                 code of 1, a process other than Sentinel is in the process
|                 table.
*------------------------------------------------------------------------- */
static void check_deadlock(){
    int numProc = 0; // Number of processes in the process table

    /* Check the status of every entry in the process table. Increment
     * numProc if a process status in not EMPTY
     */
    int i;
    for (i = 0; i < MAXPROC; i++) {
        if (ProcTable[i].status != EMPTY) {
            numProc++;
        }
    }

    // EDITING THIS FOR PHASE 2 TESTING

    // THIS IS NOT A COMPLETE CHECK DEADLOCK! 
    // It should use a dummy form of check_io() defined in p1.c that just constantly returns 0 for phase 1
    // In phase 2, we implement a version of check_io() that checks for mailboxes waiting    

    // Check for processes still running
    // If processes exist, return back to the sentinel to continue waiting
    // Todo: how will the Sentinel know if processes are stuck?
    if(numProc > 1){
        if (DEBUG && debugflag) {
            console("check_deadlock(): dumping processes...\n");
            dump_processes();
            console("check_deadlock(): There are still processes on the table.");
        }
        
        // Removed these lines from phase 1 implementation:
        // console("check_deadlock(): numProc = %d. Only Sentinel should be left. Halting...\n", numProc);
        // halt(1);

        // Changed to 'return' to allow Sentinel to continue, instead of halting in phase 1
        // This will allow blocked processes to continue running so they can complete
        return;
    } 
    
    // At this point, there are no other processes on the process table. USLOSS can halt.
    console("All processes completed.\n");
    halt(0);
} /* check_deadlock */

/*
 * Enable interrupts
 */
void enableInterrupts() {
    psr_set(psr_get() | PSR_CURRENT_INT);
}

/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    if( (PSR_CURRENT_MODE & psr_get()) == 0 ) {
        //not in kernel mode
        console("Kernel Error: Not in kernel mode, can't disable interrupts.\n");
        halt(1);
    } else {
        // We ARE in kernel mode
        psr_set( psr_get() & ~PSR_CURRENT_INT );
    }
} /* disableInterrupts */

/*---------------------------- add_proc_to_ready_list -----------------------
|  Function add_proc_to_ready_list
|
|  Purpose:  Adds a new process to the ready list. Process is added to
|            the list based on priority. Lower priorities are placed at
|            the front of the list. A process is placed at the end of
|            all processes with the same pritority.
|
|  Parameters:  proc (IN) -- The process to be added to the ready list.
|
|  Returns:  None
|
|  Side Effects:  proc is added to the correct location in ready list.
|  
|  Priority numbering:
|	1 - Highest priority
|	5 - Lowest priority
|  The lower the number, the higher the priority.
*-------------------------------------------------------------------*/
void add_proc_to_ready_list(proc_ptr proc) {
    // Debug info
    if (DEBUG && debugflag) {
      console("add_proc_to_ready_list(): Adding process %s to ReadyList\n", proc->name);
    }

    // Process being added is the Sentinel process 
    if (ReadyList == NULL) {
        ReadyList = proc;
    } else {
        // Check if all priorities in list are less than the given process in the argument
        // If the priority of the first process in the list is a higher priority number (less important),
        // insert this process at the front of the list since it is more important.
        if(ReadyList->priority > proc->priority) {
            proc_ptr temp = ReadyList;
            ReadyList = proc;
            proc->next_proc_ptr = temp;

        // Add process before first greater priority 
        // Continue iterating through ReadyList until we find the next process that is less important,
        // then, insert this process in front of it.
        } else {
            // Temp variables to keep track of iterations and position in chain
            proc_ptr next = ReadyList->next_proc_ptr;
            proc_ptr last = ReadyList;

            // Continue down chain until 'next' has a priority that is less important than the given new process
            while (next->priority <= proc->priority) {
                last = next;
                next = next->next_proc_ptr;
            }

            // Insert new process in the list since we are now in the correct priority position
            last->next_proc_ptr = proc;
            proc->next_proc_ptr = next;
        }
    }
    
    // Debug info
    if (DEBUG && debugflag) {
        console("add_proc_to_ready_list(): Process %s added to ReadyList\n", proc->name);
        print_ready_list();
    }

} /* add_proc_to_ready_list */

/*---------------------------- print_ready_list -----------------------
|  Function print_ready_list
|
|  Purpose:  Prints a string representation of the ready list using
|            the console containing name, priority of process,
|            and process ID. Debugging must be enable.
|
|  Parameters:  None
|
|  Returns:  None
*-------------------------------------------------------------------*/
void print_ready_list(){
    char str[10000], str1[40];

    proc_ptr head = ReadyList;

    sprintf(str, "%s(%d:PID=%d)", head->name, head->priority, head->pid);

    while (head->next_proc_ptr != NULL) {
        head = head->next_proc_ptr;
        sprintf(str1, " -> %s(%d:PID=%d)", head->name, head->priority,
                head->pid);
        strcat(str, str1);
    }
    if (DEBUG && debugflag){
      console("print_ready_list(): %s\n", str);
    }
} /* print_ready_list */

/*---------------------------- get_proc_slot -----------------------
|  Function get_proc_slot
|
|  Purpose:  Finds an empty index in the process table (ProcTable).
|
|  Parameters:  None
|
|  Returns:  -1 if no slot is available or the index of the next
|            empty slot in the process table.
*-------------------------------------------------------------------*/
int get_proc_slot() {
    int hashedIndex = next_pid % MAXPROC;
    int counter = 0;
    while (ProcTable[hashedIndex].status != EMPTY) {
        next_pid++;
        hashedIndex = next_pid % MAXPROC;
        if (counter >= MAXPROC) {
            return -1;
        }
        counter++;
    }
    return hashedIndex;
} /* get_proc_slot */

/*---------------------------- init_proc_table -----------------------
|  Function init_proc_table
|
|  Purpose:  Initializes a proc_struct to become the table. Each field is set to 0, NULL, or -1.
|
|  Parameters:
|      pid (IN) --  The process ID to be 'reset'
|
|  Returns:  void
|
|  Side Effects: The fields of the proc_struct for the given pid are modified
*-------------------------------------------------------------------*/
void init_proc_table(int pid) {
    int index = pid % MAXPROC;
    ProcTable[index].pid = -1;
    ProcTable[index].stacksize = -1;
    ProcTable[index].stack = NULL;
    ProcTable[index].priority = -1;
    ProcTable[index].status = EMPTY;
    ProcTable[index].child_proc_ptr = NULL;
    ProcTable[index].next_sibling_ptr = NULL;
    ProcTable[index].next_proc_ptr = NULL;
    ProcTable[index].quit_child_ptr = NULL;
    ProcTable[index].next_quit_sibling = NULL;
    ProcTable[index].who_zapped = NULL;
    ProcTable[index].next_who_zapped = NULL;
    ProcTable[index].name[0] = '\0';
    ProcTable[index].start_arg[0] = '\0';
    ProcTable[index].start_func = NULL;
    ProcTable[index].parent_ptr = NULL;
    ProcTable[index].quit_status = -1;
    ProcTable[index].start_time = -1;
    ProcTable[index].zapped = 0;
} /* init_proc_table */

/*---------------------------- dump_processes -----------------------
|  Function dump_processes
|
|  Purpose:  Loops through all procesess and prints all active
|            processes (non empty processes).
|
|  Parameters:
|            void
|
|  Returns:  void
|
|  Side Effects:  Process printed to screen using console
*-------------------------------------------------------------------*/
void dump_processes() {
    char *ready = "READY";
    char *running = "RUNNING";
    char *blocked = "BLOCKED";
    char *join_blocked = "JOIN_BLOCKED";
    char *quit = "QUIT";
    char *zap_blocked = "ZAP_BLOCKED";

    // Print the header of the process output table
    console("\n     PID       Name      Priority     Status     Parent     CPU Time    Child Count\n");

    int i;
    for(i = 0; i < MAXPROC; i++){
        char buf[30];
        char *status = buf;
        char *parent;

        // Calculate the CPU time of the process
        int run_time;
        if (Current->pid == ProcTable[i].pid) {
            // This process is the current process, we can calculate its CPU time
            run_time = sys_clock() - read_cur_start_time();
        } else {
            run_time = ProcTable[i].start_time;
        }

        if(ProcTable[i].status != EMPTY){
           switch(ProcTable[i].status) {
               case READY : status = ready;
                   break;
               case RUNNING : status = running;
                   break;
               case BLOCKED  : status = blocked;
                   break;
               case JOIN_BLOCKED : status = join_blocked;
                   break;
               case QUIT  : status = quit;
                   break;
               case ZAP_BLOCKED  : status = zap_blocked;
                   break;
               default : sprintf(status, "%d", ProcTable[i].status);
           }

           if(ProcTable[i].parent_ptr != NULL){
               parent = ProcTable[i].parent_ptr->name;
           } else {
               parent = "NULL";
           }

		   // Count children
		   proc_ptr child = ProcTable[i].child_proc_ptr;

		   // If a child exists, count all siblings as children as well.
		   int child_count = 0;
		   while (child != NULL){
				child_count++;
				child = child->next_sibling_ptr;
		   }

           // Display process iteration information
           console("%8d %10s %10d %13s %10s %10d %10d\n", ProcTable[i].pid, ProcTable[i].name, ProcTable[i].priority, status, parent, run_time, child_count);
        }
    }
}/* dump_processes */

/*------------------------------------------------------------------
|  Function remove_from_child_list
|
|  Purpose:  Finds process in parent's childlist, removes process, reasigns
|            all important processes
|
|  Parameters:
|            proc_ptr process, process to be deleted
|
|  Returns:  void
|
|  Side Effects:  Process is removed from parent's childList
*-------------------------------------------------------------------*/
void remove_from_child_list(proc_ptr process) {
    proc_ptr temp = process;
    // process is at the head of the linked list
    if (process == process->parent_ptr->child_proc_ptr) {
        process->parent_ptr->child_proc_ptr = process->next_sibling_ptr;
    } else { // process is in the middle or end of linked list
        temp = process->parent_ptr->child_proc_ptr;
        while (temp->next_sibling_ptr != process) {
            temp = temp->next_sibling_ptr;
        }
        temp->next_sibling_ptr = temp->next_sibling_ptr->next_sibling_ptr;
    }
    if (DEBUG && debugflag) {
       console("remove_from_child_list(): Process %d removed.\n",
                      temp->pid);
    }
}/* remove_from_child_list */

/*------------------------------------------------------------------
|  Function remove_from_quit_list
|
|  Purpose: Removes process from parent's quit list
|
|  Parameters:
|            proc_ptr process, process to be removed
|
|  Returns:  void
|
|  Side Effects:  Process is removed from parent's quitList
*-------------------------------------------------------------------*/
void remove_from_quit_list(proc_ptr process) {
    process->parent_ptr->quit_child_ptr = process->next_quit_sibling;

    if (DEBUG && debugflag) {
       console("remove_from_quit_list(): Process %d removed.\n",
                      process->pid);
    }
}/* remove_from_quit_list */

void clock_handler() {
    time_slice();
}

/*------------------------------------------------------------------
|  Function add_to_quit_child_list
|
|  Purpose:  Adds a process to it's parent's quit child list
|
|  Parameters:
|            proc_ptr ptr - the parent process to add the child to
|
|  Returns:  void
|
|  Side Effects: the process is added back of the quit child list
*-------------------------------------------------------------------*/
void add_to_quit_child_list(proc_ptr parent) {
    // 
    if (parent->quit_child_ptr == NULL) {
        parent->quit_child_ptr = Current;
        return;
    }
    proc_ptr child = parent->quit_child_ptr;
    while (child->next_quit_sibling != NULL) {
        child = child->next_quit_sibling;
    }
    child->next_quit_sibling = Current;
}/* add_to_quit_child_list */

/*
 * Returns the pid of the current process
 */
int getpid(){
    return Current->pid;
}

/*
 * Returns the start time of the current process
 */
int read_cur_start_time() {
    return Current->start_time;
}

/*
 * Calls dispatcher if a process has been time sliced
 * If the time allowed for each process has been past,
 * call the dispatcher to determine if another process should run.
*/
void time_slice() {
    if (readtime() >= TIME_SLICE) {
        dispatcher();
    }
    return;
}

/*
 * Returns the difference between the current process's start time and
 * the USLOSS clock
 */
int readtime() {
    return sys_clock() - read_cur_start_time();
}

/*------------------------------------------------------------------
|  Function block_me
|
|  Purpose:  Blocks the Current process and removes from Readylist
|
|  Parameters:
|            int new_status - the status for the process to block on
|
|  Returns:  int - the return code
|
|  Side Effects:  Process status is changed, removed from readyList
*-------------------------------------------------------------------*/
int block_me(int new_status) {
    // Ensure we are in kernel mode before continuing
    if( (PSR_CURRENT_MODE & psr_get()) == 0 ) {
        console("block_me(): called while in user mode, by process %d. Halting...\n", Current->pid);
        halt(1);
    }

    // Debug info
    if (DEBUG && debugflag) {
        console("block_me(): Process %s is disabling interrupts.\n", Current->name);
    }

    // Disable interrupts
    disableInterrupts();

    // Check for out of bounds argument
    if (new_status < 11) {
        console("block_me(): called with invalid status of %d. Halting...\n", new_status);
        halt(1);
    }

    // Set current process status to the new value specified by the argument
    Current->status = new_status;

    // Remove the current process from the Ready List
    // This process will now be waiting for some event to happen before being unblocked
    remove_from_ready_list(Current);

    // Call the dispatcher to set the new process
    dispatcher();
    
    // Debug info
    if (DEBUG && debugflag) {
        console("block_me(): Process %s is unblocked.\n", Current->name);
    }

    // Check for zapped condition
    if (is_zapped()) {
      return -1;
    }

    // Success
    return 0;

}/*block_me */

/*------------------------------------------------------------------
|  Function unblock_proc
|
|  Purpose:  Unblocks a process blocked by block_me
|
|  Parameters:
|            int pid - the pid of the process to unblock
|
|  Returns:  int - the return code
|
|  Side Effects:  Process status is changed, added back to readyList
*-------------------------------------------------------------------*/
int unblock_proc(int pid){
    // Verify given PID is valid, and is potentially blocked
    if (ProcTable[pid % MAXPROC].pid != pid) {
        return -2;
    }
    if (Current->pid == pid) {
        return -2;
    }
    if (ProcTable[pid % MAXPROC].status < 11) {
        return -2;
    }
    if (is_zapped()) {
        return -1;
    }

    // Change status of the given PID back to 'READY'
    ProcTable[pid % MAXPROC].status = READY;

    // Add the process back to the Ready List
    add_proc_to_ready_list(&ProcTable[pid % MAXPROC]);
    
    // Call the dispatcher to set the new process
    dispatcher();

    // Success
    return 0;
}/* unblock_proc */

/*------------------------------------------------------------------
|  Function remove_from_ready_list
|
|  Purpose:  Finds process in ReadyList, removes process, reasigns
|            all important processes|
|
|  Parameters:
|            proc_ptr process, process to be deleted
|
|  Returns:  void
|
|  Side Effects:  Process is removed from ReadyList
*-------------------------------------------------------------------*/
void remove_from_ready_list(proc_ptr process) {
    // Check if the given process IS the ReadyList variable
    // In other words, the process to be deleted from the Ready List,
    // is the first process in the Ready List
    if(process == ReadyList) {
    	// Move ready list to the next process
        ReadyList = ReadyList->next_proc_ptr;
    } else {
    	// The process to be removed is not the first in the list
	// Start at the first process in the list
        proc_ptr proc = ReadyList;

	// Continuing moving down the list until we find the process to be removed from the list
        while (proc->next_proc_ptr != process) {
            proc = proc->next_proc_ptr;
        }

	// 'proc' is now the process located right before the process to be removed from the list
	// Remove the target process by setting 'proc's next pointer to the process after the one to be removed
	// We are basically setting the pointer to skip over the process we no longer want in the ready list
        proc->next_proc_ptr = proc->next_proc_ptr->next_proc_ptr;
    }
    if (DEBUG && debugflag) {
        console("remove_from_ready_list(): Process %d removed from ReadyList.\n", process->pid);
    }
}/* remove_from_ready_list */

/*------------------------------------------------------------------
|  Function unblock_zappers
|
|  Purpose:  Unblocks all processes that zapped a process.
|
|  Parameters:
|            proc_ptr ptr - head of linked list of processes that zapped
|                          the calling process.
|
|  Returns:  void
|
|  Side Effects:  Process status is set to READY and process is added
|                 to the ready list.
*-------------------------------------------------------------------*/
void unblock_zappers(proc_ptr ptr) {
    if (ptr == NULL) {
        return;
    }
    unblock_zappers(ptr->next_who_zapped);
    ptr->status = READY;
    add_proc_to_ready_list(ptr);
} /* unblock_zappers */
