/* ------------------------------------------------------------------------
	phase2.c

	CSCV 452

	Austin Ramsay
	Raymond Howard
------------------------------------------------------------------------ */

#include <phase1.h>
#include <phase2.h>
#include "message.h"
#include "my_phase2.h"
#include <usloss.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------- Globals ------------------------------------- */
int debugflag2 = 0;

// Mailbox table and mailbox slot array
mailbox mailbox_table[MAXMBOX];
mail_slot slot_table[MAXSLOTS];

// Process table
mbox_proc mbox_proc_table[MAXPROC];

// System call vector
void (*sys_vec[MAXSYSCALLS])(sysargs *args);

// Counter used by clock
int clock_counter = 0;

/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg) {
    // Debug info
    if (DEBUG2 && debugflag2) {
        console("start1(): at beginning\n");
    }

    int kid_pid;
    int status;

    // Check kernel mode, disable interrupts
    check_kernel_mode("start1");
    disableInterrupts();

    /* Initialize the mail box table, slots, & other data structures.
    * Initialize int_vec and sys_vec, allocate mailboxes for interrupt
    * handlers.  Etc... */

    // Initialize mail box table by iterating through each possible index and calling 'zero_mailbox' for each one
    // mailbox_table is an array of mailboxes
    int i;
    for (i = 0; i < MAXMBOX; i++) {
        mailbox_table[i].mbox_id = i;
        zero_mailbox(i);
    }

    // Create boxes for all the interrupt handlers
    for (i = 0; i < 7; i++) {
        MboxCreate(0,0);
    }

    // Initialize all slots in the slot table
    // slot_table is an array of mail_slot's
    for (i = 0; i < MAXSLOTS; i++) {
        slot_table[i].slot_id = i;
        zero_slot(i);
    }

    // Initialize process table
    for (i = 0; i < MAXPROC; i++) {
        zero_mbox_proc(i);
    }

    // Initialize handlers
    int_vec[CLOCK_DEV] = (void*)clock_handler2;
    int_vec[DISK_DEV] = (void*)disk_handler;
    int_vec[TERM_DEV] = (void*)term_handler;
    int_vec[SYSCALL_INT] = (void*)syscall_handler;

    // Set 'nullsys' for each sys_vec index
    for (i = 0; i < MAXSYSCALLS; i++) {
        sys_vec[i] = nullsys;
    }

    // Now we can enable interrupts
    enableInterrupts();

    // Debug info 
    if (DEBUG2 && debugflag2) {
        console("start1(): forking start2 process\n");
    }

    // Create a process for 'start2', then block on a join until start2 quits
    kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if (join(&status) != kid_pid) {
        console("start2(): join returned something other than start2's pid.");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array.
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size) {
    
    // Check kernel mode, disable interrupts 
    check_kernel_mode("MboxCreate");
    disableInterrupts();

    // Check for invalid parameters
    // Slots cannot be given as less than zero, slot sizes cannot be less than zero or defined larger than the MAX_MESSAGE from 'phase2.h'
    if (slots < 0) {
        enableInterrupts();
        return -1;
    }
    if (slot_size < 0 || slot_size > MAX_MESSAGE) {
        enableInterrupts();
        return -1;
    }

    // Setup the next available mailbox from the mailbox table
    int i;
    for (i = 0; i < MAXMBOX; i++) {
        if (mailbox_table[i].status == EMPTY) {

            // Box is available, initialize it
            mailbox_table[i].num_slots = slots;
            mailbox_table[i].slots_used = 0;
            mailbox_table[i].slot_size = slot_size;
            mailbox_table[i].status = USED;

            // Done creating box, enable interrupts
            enableInterrupts();

            // Box created, return the ID we just setup
            return i;
        }
    }

    // No mailboxes available, enable interrupts and return -1 to indicate no box was created
    enableInterrupts();
    return -1;

} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {
    
    // Check kernel mode and disable interrupts
    check_kernel_mode("MboxSend");
    disableInterrupts();

    // Check for invalid parameters
    // Cannot put a message into an unused mailbox, and cannot specify a 'mbox_id' outside of 0 to MAXMBOX defined in 'phase2.h'
    if (mailbox_table[mbox_id].status == EMPTY) {
        enableInterrupts();
        return -1;
    }
    if (mbox_id > MAXMBOX || mbox_id < 0) {
        enableInterrupts();
        return -1;
    }

    // Retrieve the pointer to the specified mailbox
    mailbox_ptr mbptr = &mailbox_table[mbox_id];

    // Ensure the box has a slot that can receive the message of this given size
    if (mbptr->num_slots != 0 && msg_size > mbptr->slot_size) {
        enableInterrupts();
        return -1;
    }

    // Add process to the process table
    // getpid() is a phase1.c function returning the current process ID
    int pid = getpid(); 
    
    // MAXPROC - maximum number of processes defined in USLOSS/src/phase1.h (50)
    // pid % MAXPROC = pid of the current process (who called this function)
    // Set the mbox_proc up for this pid
    // Assign the pid, an ACTIVE status, set the '*message' = msg_ptr argument value, and the msg_size given by the argument value
    mbox_proc_table[pid % MAXPROC].pid = pid;
    mbox_proc_table[pid % MAXPROC].status = ACTIVE;
    mbox_proc_table[pid % MAXPROC].message = msg_ptr;
    mbox_proc_table[pid % MAXPROC].msg_size = msg_size;

    // Block if there no available slots and no process on receive list
    // Add to the next block send list
    // mbptr - the destination mailbox for the message

    // There are no available slots in the destination mailbox,
    // and the destination mailbox block receive list is empty...
    // We will block until a slot is available
    if (mbptr->num_slots <= mbptr->slots_used && mbptr->block_recv_list == NULL) {
        
        // If the destination mailbox block send list is empty, 
	// set the block send list mbox_proc_ptr as the mbox_proc of this pid
	if (mbptr->block_send_list == NULL) {
            
            // Add this process as the first on the list
            mbptr->block_send_list = &mbox_proc_table[pid % MAXPROC];

        // The block send list is not empty
        } else {

            // Add this process to the end of the list
            // by iterating through the list until we get to the end
            mbox_proc_ptr temp = mbptr->block_send_list;
            while (temp->next_block_send != NULL) {
                temp = temp->next_block_send;
            }

            // Now that we're at the end of the list,
            // we can add this process to the end of the list
            temp->next_block_send = &mbox_proc_table[pid % MAXPROC];
        }

        // Block this process now that we've added to the block send list
        // block_me is a phase 1 function that blocks the current process, then calls the dispatcher afterwards
        // We are blocking because we are waiting for a slot to become available
        block_me(SEND_BLOCK);
         
        // If the mailbox was released, enable interrupts
        // Return -3 
        if(mbox_proc_table[pid % MAXPROC].mbox_released){
          enableInterrupts();
          return -3;
        }
        return is_zapped() ? -3 : 0;
    }

    // There are slots available,
    // but the block receive list is not empty
    if (mbptr->block_recv_list != NULL) {

        // Check if the message size is bigger than receive buffer size
        if (msg_size > mbptr->block_recv_list->msg_size) {
            mbptr->block_recv_list->status = FAILED;
            int pid = mbptr->block_recv_list->pid;
            mbptr->block_recv_list = mbptr->block_recv_list->next_block_recv;
            unblock_proc(pid);
            enableInterrupts();
            return -1;
        }

        // Copy the message to the receive process buffer
        memcpy(mbptr->block_recv_list->message, msg_ptr, msg_size);
        mbptr->block_recv_list->msg_size = msg_size;
        int recvPid = mbptr->block_recv_list->pid;
        mbptr->block_recv_list = mbptr->block_recv_list->next_block_recv;
        unblock_proc(recvPid);
        enableInterrupts();
        return is_zapped() ? -3 : 0;
    }

    // Find an empty slot in the slot_table
    int slot = get_slot_index();
    if (slot == -2) {
        console("MboxSend(): No slots in system. Halting...\n");
        halt(1);
    }

    // Initialize the slot
    slot_ptr slot_to_add = init_slot(slot, mbptr->mbox_id, msg_ptr, msg_size);

    // Move the found slot onto the slot_list
    add_slot_to_list(slot_to_add, mbptr);

    enableInterrupts();
    return is_zapped() ? -3 : 0;
} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size) {
    check_kernel_mode("MboxReceive");

    disableInterrupts();

    // Check for invalid parameters
    // Cannot receive a message from an unused mailbox
    if (mailbox_table[mbox_id].status == EMPTY) {
        enableInterrupts();
        return -1;
    }

    // Message size cannot be less than zero
    if (msg_size < 0) {
        enableInterrupts();
        return -1;
    }

    // Retrieve the pointer to the specified mailbox
    mailbox_ptr mbptr = &mailbox_table[mbox_id];

    // Add process to process Table
    int pid = getpid();
    mbox_proc_table[pid % MAXPROC].pid = pid;
    mbox_proc_table[pid % MAXPROC].status = ACTIVE;
    mbox_proc_table[pid % MAXPROC].message = msg_ptr;
    mbox_proc_table[pid % MAXPROC].msg_size = msg_size;

    // The mailbox is has zero slots and there is a process on send list
    if (mbptr->num_slots == 0 && mbptr->block_send_list != NULL) {
        mbox_proc_ptr sender = mbptr->block_send_list;
        memcpy(msg_ptr, sender->message, sender->msg_size);
        mbptr->block_send_list = mbptr->block_send_list->next_block_send;
        unblock_proc(sender->pid);
        return sender->msg_size;
    }

    // Retrieve the pointer to the first slot in the list
    slot_ptr first_slot = mbptr->slot_list;

    // Block because no message available
    if (first_slot == NULL) {

        // Receive process adds itself to receive list
        if (mbptr->block_recv_list == NULL) {
            mbptr->block_recv_list = &mbox_proc_table[pid % MAXPROC];
        } else {
            mbox_proc_ptr temp = mbptr->block_recv_list;
            while (temp->next_block_recv != NULL) {
                temp = temp->next_block_recv;
            }
            temp->next_block_recv = &mbox_proc_table[pid % MAXPROC];
        }
        
        // Debug info for test 13
        if (DEBUG2 && debugflag2) {
            console("Process about to be blocked on receive...\n");
        }

        // Block until sender arrives at mailbox
        block_me(RECV_BLOCK);

        // The process was zapped or the mailbox was released
        if(mbox_proc_table[pid % MAXPROC].mbox_released || is_zapped()){
           enableInterrupts();
           return -3;
        }

        // Check if we failed to receive the message, if so, return failed
        if(mbox_proc_table[pid % MAXPROC].status == FAILED) {
            enableInterrupts();
            return -1;
        }

        enableInterrupts();
        return mbox_proc_table[pid % MAXPROC].msg_size;

    } else {
        // There's a message available on the slot list

        // Message size is bigger than receive buffer size
        if (first_slot->msg_size > msg_size) {
            enableInterrupts();
            return -1;
        }

        // Copy message into receive messsage buffer
        memcpy(msg_ptr, first_slot->message, first_slot->msg_size);
        mbptr->slot_list = first_slot->next_slot;
        int msg_size = first_slot->msg_size;
        zero_slot(first_slot->slot_id);
        mbptr->slots_used--;

        // there is a message on the send list waiting for a slot
        if (mbptr->block_send_list != NULL) {

            // Determine the index from the list
            int slot_index = get_slot_index();

            // Initialize the slot with the message and message size
            slot_ptr slot_to_add = init_slot(slot_index, mbptr->mbox_id, mbptr->block_send_list->message, mbptr->block_send_list->msg_size);

            // Add the slot to the slot list
            add_slot_to_list(slot_to_add, mbptr);

            // Wake up the process blocked on the send list
            int pid = mbptr->block_send_list->pid;
            mbptr->block_send_list = mbptr->block_send_list->next_block_send;
            unblock_proc(pid);
        }

        enableInterrupts();

        return is_zapped() ? -3 : msg_size;
    }
} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - MboxRelease
   Purpose - Releases the mailbox, and alerts any blocked processes
             waiting on mailbox.
   Parameters - one, mbox_id, the ID of the mailbox to release
   Returns - 0 = normal, -1 = abnormal, -3 = zapped
   Side Effects - Zeros the mailbox and alert the blocks procs
   ----------------------------------------------------------------------- */
int MboxRelease(int mbox_id) {
    check_kernel_mode("MboxRelease");
    disableInterrupts();

    // Check for invalid parameters
    if (mbox_id < 0 || mbox_id >= MAXMBOX) {
        enableInterrupts();
        return -1;
    }
    if (mailbox_table[mbox_id].status == EMPTY) {
        enableInterrupts();
        return -1;
    }

    // Retrieve the pointer to the specified mailbox
    mailbox_ptr mbptr = &mailbox_table[mbox_id];

    // There are no processes on the send and receive block lists
    if (mbptr->block_send_list == NULL && mbptr->block_recv_list == NULL) {
        zero_mailbox(mbox_id);
        enableInterrupts();
        return is_zapped() ? -3 : 0;
    } else {
        // Set the mailbox as empty
        mbptr->status = EMPTY;

        // Set all processes on the block send and receive list as released
        // Send list
        while (mbptr->block_send_list != NULL) {
            mbptr->block_send_list->mbox_released = 1;
            int pid = mbptr->block_send_list->pid;
            mbptr->block_send_list = mbptr->block_send_list->next_block_send;
            unblock_proc(pid);
            disableInterrupts();
        }
        // Receive list
        while (mbptr->block_recv_list != NULL) {
            mbptr->block_recv_list->mbox_released = 1;
            int pid = mbptr->block_recv_list->pid;
            mbptr->block_recv_list = mbptr->block_recv_list->next_block_recv;
            unblock_proc(pid);
            disableInterrupts();
        }
    }

    // Zero all the variables of the mailbox
    zero_mailbox(mbox_id);

    enableInterrupts();
    return is_zapped() ? -3 : 0;
}

/* ------------------------------------------------------------------------
   Name - MboxCondSend
   Purpose - Put a message into a slot for the indicated mailbox.
             return -2 if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size){
    check_kernel_mode("MboxCondSend");
    disableInterrupts();

    // Check for invalid parameters
    if (mbox_id > MAXMBOX || mbox_id < 0) {
        enableInterrupts();
        return -1;
    }

    // Retrieve the pointer to the specified mailbox
    mailbox_ptr mbptr = &mailbox_table[mbox_id];
    if (mbptr->num_slots != 0 && msg_size > mbptr->slot_size) {
        enableInterrupts();
        return -1;
    }

    // Add process to the process Table
    int pid = getpid();
    mbox_proc_table[pid % MAXPROC].pid = pid;
    mbox_proc_table[pid % MAXPROC].status = ACTIVE;
    mbox_proc_table[pid % MAXPROC].message = msg_ptr;
    mbox_proc_table[pid % MAXPROC].msg_size = msg_size;

    // No empty slots in mailbox or no slots in system
    if (mbptr->num_slots != 0 && mbptr->num_slots == mbptr->slots_used) {
        return -2;
    }

    // Zero slot mailbox and no process blocked on recveive list
    if (mbptr->block_recv_list == NULL && mbptr->num_slots == 0) {
        return -1;
    }

    // Check if the process is on the receive block list
    if (mbptr->block_recv_list != NULL) {
        if (msg_size > mbptr->block_recv_list->msg_size) {
            enableInterrupts();
            return -1;
        }

        // Copy message into the blocked receive process message buffer
        memcpy(mbptr->block_recv_list->message, msg_ptr, msg_size);
        mbptr->block_recv_list->msg_size = msg_size;
        int recvPid = mbptr->block_recv_list->pid;
        mbptr->block_recv_list = mbptr->block_recv_list->next_block_recv;
        unblock_proc(recvPid);
        enableInterrupts();
        return is_zapped() ? -3 : 0;
    }

    // Search for an empty slot in slot_table, if it returns -2, no slot is available
    int slot = get_slot_index();
    if (slot == -2) {
        return -2;
    }

    // Initialize the slot
    slot_ptr slot_to_add = init_slot(slot, mbptr->mbox_id, msg_ptr, msg_size);

    // Add the newly found slot to the list
    add_slot_to_list(slot_to_add, mbptr);

    enableInterrupts();
    return is_zapped() ? -3 : 0;
}

/* ------------------------------------------------------------------------
   Name - MboxCondReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             return -2 if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondReceive(int mbox_id, void *msg_ptr,int msg_size){
    check_kernel_mode("MboxCondReceive");
    disableInterrupts();

    // Check for invalid parameters
    if (mailbox_table[mbox_id].status == EMPTY) {
        enableInterrupts();
        return -1;
    }
    if (msg_size < 0) {
        enableInterrupts();
        return -1;
    }

    // Retrieve the pointer to the given mailbox
    mailbox_ptr mbptr = &mailbox_table[mbox_id];

    // Add process to the process table
    int pid = getpid();
    mbox_proc_table[pid % MAXPROC].pid = pid;
    mbox_proc_table[pid % MAXPROC].status = ACTIVE;
    mbox_proc_table[pid % MAXPROC].message = msg_ptr;
    mbox_proc_table[pid % MAXPROC].msg_size = msg_size;

    // The mailbox has zero slots and there is a process on the send list
    if (mbptr->num_slots == 0 && mbptr->block_send_list != NULL) {
        mbox_proc_ptr sender = mbptr->block_send_list;
        memcpy(msg_ptr, sender->message, sender->msg_size);
        mbptr->block_send_list = mbptr->block_send_list->next_block_send;
        unblock_proc(sender->pid);
        return sender->msg_size;
    }

    // Retrieve the pointer to the first slot on the list
    slot_ptr first_slot = mbptr->slot_list;

    // Check if there is no message available
    if (first_slot == NULL) {
        enableInterrupts();
        return -2;

    } else {
        // There is a message available on the slot list

        // If the message size is bigger than receive buffer size, failed
        if (first_slot->msg_size > msg_size) {
            enableInterrupts();
            return -1;
        }

        // Copy the message into the receive messsage buffer
        memcpy(msg_ptr, first_slot->message, first_slot->msg_size);
        mbptr->slot_list = first_slot->next_slot;
        int msg_size = first_slot->msg_size;
        zero_slot(first_slot->slot_id);
        mbptr->slots_used--;

        // Check if there is a message on the send list waiting for a slot
        if (mbptr->block_send_list != NULL) {

            // Determine the next slot
            int slot_index = get_slot_index();

            // Now initialize the slot with the message and message size
            slot_ptr slot_to_add = init_slot(slot_index, mbptr->mbox_id,
                    mbptr->block_send_list->message,
                    mbptr->block_send_list->msg_size);

            // Add the slot to the slot list
            add_slot_to_list(slot_to_add, mbptr);

            // Wake up a process blocked on the send list
            int pid = mbptr->block_send_list->pid;
            mbptr->block_send_list = mbptr->block_send_list->next_block_send;
            unblock_proc(pid);
        }

        enableInterrupts();
        return is_zapped() ? -3 : msg_size;
    }
}

/* ------------------------------------------------------------------------
   Name - waitdevice
   Purpose - Block the process on the device until the device sends msg.
   Parameters - type, unit, status
   Returns - -1 if zapped, 0 otherwise
   Side Effects - none.
   ----------------------------------------------------------------------- */
int waitdevice(int type, int unit, int *status){
    
    // Check kernel mode and disable interrupts
    check_kernel_mode("waitdevice");
    disableInterrupts();

    int return_code;               // -1 if process was zapped, 0 otherwise
    int deviceID;                 // the index of the i/o mailbox
    int clockID = 0;              // index of the clock i/o mailbox
    int diskID[] = {1, 2};        // indexes of the disk i/o mailboxes
    int termID[] = {3, 4, 5, 6};  // indexes of the terminal i/o mailboxes

    // Determine the index of the IO mailbox for the given device type and unit
    switch (type) {
        case CLOCK_DEV:
            deviceID = clockID;
            break;
        case DISK_DEV:
            if (unit >  1 || unit < 0) {
                console("waitdevice(): invalid unit. Halting\n");
		halt(1);
            }
            deviceID = diskID[unit];
            break;
        case TERM_DEV:
            if (unit >  3 || unit < 0) {
                console("waitdevice(): invalid unit. Halting...\n");
		halt(1);
            }
            deviceID = termID[unit];
            break;
        default:
            console("waitdevice(): invalid device or unit type. Halting...\n");
	    halt(1);
    }

    // Now we wait for the return code of the device
    return_code = MboxReceive(deviceID, status, sizeof(int));
    return return_code == -3 ? -1 : 0;
}

/*
 *check_kernel_mode
 */
void check_kernel_mode(char * processName) {
    if((PSR_CURRENT_MODE & psr_get()) == 0) {
        console("check_kernal_mode(): called while in user mode, by"
                " process %s. Halting...\n", processName);
        halt(1);
    }
}

/*
 * Determines if any procs are blocked on an IO mailbox
 */
int check_io() {
    int i;
    for (i = 0; i < 7; i++) {
        if (mailbox_table[i].block_recv_list != NULL) {
            return 1;
        }
    }
    return 0;
}

/*
 * Zeros all variables of the mailbox for the given mailbox ID parameter
 */
void zero_mailbox(int mbox_id) {
    mailbox_table[mbox_id].num_slots = -1;
    mailbox_table[mbox_id].slots_used = -1;
    mailbox_table[mbox_id].slot_size = -1;
    mailbox_table[mbox_id].block_send_list = NULL;
    mailbox_table[mbox_id].block_recv_list = NULL;
    mailbox_table[mbox_id].slot_list = NULL;
    mailbox_table[mbox_id].status = EMPTY;
}

/*
 * Zeros all variables of the slot for the given slot ID parameter
 */
void zero_slot(int slot_id) {
    slot_table[slot_id].mbox_id = -1;
    slot_table[slot_id].status = EMPTY;
    slot_table[slot_id].next_slot = NULL;
}

/*
 * Zeros all variables of the process for the given PID parameter
 */
void zero_mbox_proc(int pid) {
   mbox_proc_table[pid % MAXPROC].pid = -1;
   mbox_proc_table[pid % MAXPROC].status = EMPTY;
   mbox_proc_table[pid % MAXPROC].message = NULL;
   mbox_proc_table[pid % MAXPROC].msg_size = -1;
   mbox_proc_table[pid % MAXPROC].mbox_released = 0;
   mbox_proc_table[pid % MAXPROC].next_block_send = NULL;
   mbox_proc_table[pid % MAXPROC].next_block_recv = NULL;
}

/*
* Method to handle invalid syscalls
*/
void nullsys(sysargs *args) {
    console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
    halt(1);
}

/* ------------------------------------------------------------------------
   Name - clock_handler2
   Purpose - called when interrupt vector is activated for this device
   Parameters - device, unit
   Returns - void
   Side Effects - increases clock counted by 1.
   ----------------------------------------------------------------------- */
void clock_handler2(int dev, long unit) {
    check_kernel_mode("clock_handler2");

    disableInterrupts();

    if (dev != CLOCK_DEV || unit != 0) {
        console("clock_handler2(): wrong device or unit\n");
        halt(1);
    }

    int status;

    clock_counter++;
    
    if (DEBUG2 && debugflag2) {
        console("clock_handler2(): clock counter incremented...");
    }

    if (clock_counter >= 5) {
        device_input(CLOCK_DEV, 0, &status);
        int sent = MboxCondSend(0, &status, sizeof(int));
        
        if (DEBUG2 && debugflag2) {
            console("clock_handler2(): conditional message sent status was %d", sent);
        }

        clock_counter = 0;
    }

    time_slice();

    enableInterrupts();
} /* clockHandler */

/* ------------------------------------------------------------------------
   Name - disk_handler
   Purpose - called when interrupt vector is activated for this device
   Parameters - device, unit
   Returns - void
   Side Effects - none
   ----------------------------------------------------------------------- */
void disk_handler(int dev, long unit) {
    check_kernel_mode("disk_handler");

    disableInterrupts();

    if (dev != DISK_DEV || unit < 0 || unit > 1) {
        console("disk_handler(): wrong device or unit\n");
        halt(1);
    }

    int status;
    int mbox_id = unit + 1;

    device_input(DISK_DEV, unit, &status);

    MboxCondSend(mbox_id, &status, sizeof(status));

    enableInterrupts();
} /* disk_handler */

/* ------------------------------------------------------------------------
   Name - term_handler
   Purpose - called when interrupt vector is activated for this device
   Parameters - device, unit
   Returns - void
   Side Effects - none
   ----------------------------------------------------------------------- */
void term_handler(int dev, long unit) {
    check_kernel_mode("term_handler");

    disableInterrupts();

    if (dev != TERM_DEV || unit < 0 || unit > 3) {
        console("term_handler(): wrong device or unit\n");
        halt(1);
    }

    int status;
    int mbox_id = unit + 3;

    device_input(TERM_DEV, unit, &status);

    MboxCondSend(mbox_id, &status, sizeof(int));

    enableInterrupts();
} /* term_handler */

/* ------------------------------------------------------------------------
   Name - syscall_handler
   Purpose - called when interrupt vector is activated for this device
   Parameters - device, unit
   Returns - void
   Side Effects - none
   ----------------------------------------------------------------------- */
void syscall_handler(int dev, void *unit) {
    check_kernel_mode("syscall_handler");

    disableInterrupts();

    sysargs *args = unit;
    int sysCall = args->number;

    if (dev != SYSCALL_INT || sysCall < 0 || sysCall >= MAXSYSCALLS) {
        console("syscall_handler(): sys number %d is wrong.  Halting...\n",
                sysCall);
        halt(1);
    }

    (*sys_vec[sysCall])(args);

    enableInterrupts();
} /* syscall_handler */

/*
 * Returns the index of the next available slot from the slot array or -2 if
 * there is no available slot.
 */
int get_slot_index() {
    int i;
    for (i = 0; i < MAXSLOTS; i++) {
        if (slot_table[i].status == EMPTY) {
           return i;
        }
    }
    return -2;
}

/*
 * Initializes a new slot in the slot tables
 */
slot_ptr init_slot(int slot_index, int mbox_id, void *msg_ptr, int msg_size) {
    slot_table[slot_index].mbox_id = mbox_id;
    slot_table[slot_index].status = USED;
    memcpy(slot_table[slot_index].message, msg_ptr, msg_size);
    slot_table[slot_index].msg_size = msg_size;
    return &slot_table[slot_index];
}

/*
 * Adds a slot to the slot list for a mailbox
 */
int add_slot_to_list(slot_ptr slot_to_add, mailbox_ptr mbptr) {
    slot_ptr head = mbptr->slot_list;
    if (head == NULL) {
        mbptr->slot_list = slot_to_add;
    } else {
        while (head->next_slot != NULL) {
            head = head->next_slot;
        }
        head->next_slot = slot_to_add;
    }
    return ++mbptr->slots_used;
}

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
