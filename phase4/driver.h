#define EMPTY 0
#define ACTIVE 1

typedef struct proc_struct4 *proc_ptr4;
typedef struct driver_proc *driver_proc_ptr;

typedef struct proc_struct4 {
   proc_ptr4        child_proc_ptr;     // process's children 
   proc_ptr4        next_sibling_ptr;   // next process on parent child list 
   proc_ptr4        parent_ptr;        // parent process
   proc_ptr4        sleep_ptr;         // sleep list
   int             wake_time;        // time to be woken up in microseconds
   char            name[MAXNAME];    
   char            start_arg[MAXARG]; // function arguments
   short           pid;              
   int             priority;         
   int (* userFunc) (char *);        // code to execute
   unsigned int    stack_size;        
   int             status;           // can be: EMPTY / ACTIVE
   int             mboxID;           // mailbox ID to block on
} proc_struct4;

typedef struct driver_proc {
    int operation;   /* DISK_READ, DISK_WRITE, DISK_SEEK, DISK_TRACKS */ 
    int unit; 
    int start_track;
    int start_sector;
    int sectors;
    void *disk_buf;
    int mboxID;
    int status;
    driver_proc_ptr next;
} driver_proc;
