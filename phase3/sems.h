#ifndef _SEMS_H
#define _SEMS_H

// min and max priorities
#define MINPRIORITY     5
#define MAXPRIORITY     1

// specifice process pids
#define START2_PID      3
#define START3_PID      4

// process table status
#define EMPTY           0
#define ACTIVE          1
#define WAIT_BLOCK      11

#endif

typedef struct proc_struct3 *proc_ptr3;

typedef struct sem_struct {
    int count;               // value of the semaphore
    proc_ptr3 blocked_list;  // processes waiting to enter semaphore
    int status;              // EMPTY or ACTIVE
    int mbox_id;             // mailbox to block on
} sem_struct;

typedef struct proc_struct3 {
   proc_ptr3        child_proc_ptr;    	// process's children 
   proc_ptr3        next_sibling_ptr;  	// next process on parent child list 
   proc_ptr3        parent_ptr;        	// parent process
   proc_ptr3        next_sem_block;    	// next process on semaphore block list 
   char            name[MAXNAME];    	// process name
   char            start_arg[MAXARG]; 	// function arguments
   short           pid;              	// process ID
   int             priority;         	// process priority
   int (* func) (char *);				// process code
   unsigned int    stack_size;       	// stack size
   int             status;          	// EMPTY or ACTIVE
   int             mbox_id;           	// mailbox to block on
} proc_struct3;
