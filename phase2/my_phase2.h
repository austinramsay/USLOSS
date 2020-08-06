#ifndef _MY_PHASE2_H
#define _MY_PHASE2_H

int start1(char *);
void check_kernel_mode(char * processName);
int check_io();
void zero_mailbox(int mbox_id);
void zero_slot(int slot_id);
void zero_mbox_proc(int pid);
void nullsys(sysargs *args);
void clock_handler2(int dev, long unit);
void disk_handler(int dev, long unit);
void term_handler(int dev, long unit);
void syscall_handler(int dev, void *unit);
slot_ptr init_slot(int slot_index, int mbox_id, void *msg_ptr, int msg_size);
int get_slot_index();
int add_slot_to_list(slot_ptr slot_to_add, mailbox_ptr mbptr);
extern int start2(char *);
void enableInterrupts();
void disableInterrupts();

#endif
