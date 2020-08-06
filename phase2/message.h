#define DEBUG2 1
#define EMPTY 0
#define USED 1
#define ACTIVE 1
#define FAILED -1
#define SEND_BLOCK 11
#define RECV_BLOCK 12

typedef struct mailbox mailbox;
typedef struct mbox_proc mbox_proc;
typedef struct mail_slot mail_slot;
typedef struct mailbox *mailbox_ptr;
typedef struct mail_slot *slot_ptr;
typedef struct mbox_proc *mbox_proc_ptr;

struct mbox_proc {
    short pid;
    int status;
    void * message;
    int msg_size;
    int mbox_released;
    mbox_proc_ptr next_block_send;
    mbox_proc_ptr next_block_recv;
};

struct mailbox {
    int mbox_id;
    int num_slots;
    int slots_used;
    int slot_size;
    mbox_proc_ptr block_send_list;
    mbox_proc_ptr block_recv_list;
    slot_ptr slot_list;
    int status;
};

struct mail_slot {
    int slot_id;
    int mbox_id;
    int status;
    char message[MAX_MESSAGE];
    int msg_size;
    slot_ptr next_slot;
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

