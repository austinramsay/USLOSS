#ifndef _MYPHASE1_H
#define _MYPHASE1_H

/* -------------------------- Functions ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void launch();
static void check_deadlock();
void add_proc_to_ready_list(proc_ptr proc);
void print_ready_list();
int get_proc_slot();
void init_proc_table(int pid);
void remove_from_child_list(proc_ptr process);
void remove_from_quit_list(proc_ptr process);
void clock_handler();
void disableInterrupts();
void enableInterrupts();
void add_to_quit_child_list(proc_ptr ptr);
void remove_from_ready_list(proc_ptr process);
void unblock_zappers(proc_ptr ptr);

#endif
