#define DEBUG 1

/* Constants defined for phase1 */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY LOWEST_PRIORITY
#define TIME_SLICE 80000

/* Process statuses */
#define READY 1
#define RUNNING 2
#define QUIT 4
#define EMPTY 5
#define BLOCKED 8
#define JOIN_BLOCKED 9
#define ZAP_BLOCKED 10

typedef struct proc_struct proc_struct;
typedef struct proc_struct * proc_ptr;

struct proc_struct {
   proc_ptr         next_proc_ptr;
   proc_ptr         child_proc_ptr;
   proc_ptr         next_sibling_ptr;
   proc_ptr         parent_ptr;
   proc_ptr         quit_child_ptr;
   proc_ptr         next_quit_sibling;
   proc_ptr         who_zapped;
   proc_ptr         next_who_zapped;
   char            name[MAXNAME];     /* process's name */
   char            start_arg[MAXARG];  /* args passed to process */
   context         state;             /* current context for process */
   short           pid;               /* process id */
   int             priority;
   int (* start_func) (char *);        /* function where process begins -- launch */
   char           *stack;
   unsigned int    stacksize;
   int             status;            /* READY, BLOCKED, QUIT, etc. */
   int             quit_status;
   int             start_time;
   int             zapped;
};

struct psr_bits {
        unsigned int cur_mode:1;
		unsigned int cur_int_enable:1;
        unsigned int prev_mode:1;
        unsigned int prev_int_enable:1;
		unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};

int get_psr_cur_mode(int psr_value) {
    return psr_value & 1;
}

int get_psr_cur_interrupt_mode(int psr_value) {
    return psr_value & 2;
}

int get_psr_prev_mode(int psr_value) {
    return psr_value & 4;
}

int get_psr_prev_interrupt_mode(int psr_value) {
    return psr_value & 8;
}


