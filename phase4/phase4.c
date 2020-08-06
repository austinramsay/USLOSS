/* ---------------------------------------------------------------------
   phase4.c

   CSCV 452

   Austin Ramsay
   Raymond Howard
------------------------------------------------------------------------ */

#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <provided_prototypes.h>
#include <driver.h>
#include <stdlib.h> 
#include <stdio.h>  
#include <string.h>  

/* ------------------------- Prototypes ----------------------------------- */

static int ClockDriver(char *);
static int DiskDriver(char *);
void sleep(sysargs *args);
int sleep_real(int seconds);
void diskRead(sysargs *args);
int diskRead_real(int unit, int start_track, int start_sector, int sectors, void *disk_buf);
void diskWrite(sysargs *args);
int diskWrite_real(int unit, int start_track, int start_sector, int sectors, void *disk_buf);
void diskSize(sysargs *args);
int diskSize_real(int unit, int *sectorSize, int *sectorsInTrack, int *tracksInDisk);
void check_kernel_mode(char * process_name);
void addToProcessTable();
void removeFromProcessTable();
int diskReadHandler(int unit);
int diskWriteHandler(int unit);
int proc_output(device_request *dev_request, int unit);
void insert_disk_request(driver_proc_ptr info);
void enableInterrupts();

/* -------------------------- Globals ------------------------------------- */

// Process Table
proc_struct4 proc_table[MAXPROC];

int clockSemaphore;
int diskSemaphore[DISK_UNITS];
int tracksOnDisk[DISK_UNITS];
proc_ptr4 head_sleep_list;
driver_proc_ptr head_disk_list[DISK_UNITS];


/* ------------------------------------------------------------------------
   Name - start3
   Purpose - Initializes the process table, the semaphore table, and the system call 
             vector. Then, sets up drivers. 
   Parameters - N/A
   Returns - void 
   ----------------------------------------------------------------------- */
void start3() {
    char name[128];
    char argBuffer[10];
    int	i;
    int	clockPID;
    int	diskPID[DISK_UNITS]; 
    int	pid;
    int	status;

    // Check kernel mode here
    check_kernel_mode("start3");

    // Empty out process table
    for (int i = 0; i < MAXPROC; i++) {
        proc_table[i].status = EMPTY;
        proc_table[i].pid = -1;
    }

    head_sleep_list = NULL;
    for (i = 0; i < DISK_UNITS; i++) {
        head_disk_list[i] = NULL;
    }

    // Initialize the system call vector
    sys_vec[SYS_SLEEP] = sleep;
    sys_vec[SYS_DISKREAD] = diskRead;
    sys_vec[SYS_DISKWRITE] = diskWrite;
    sys_vec[SYS_DISKSIZE] = diskSize; 

    // Create clock device driver 
    clockSemaphore = semcreate_real(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
        console("start3(): Can't create clock driver\n");
        halt(1);
    }

    // Add the clockPID to the process table
    strcpy(proc_table[clockPID % MAXPROC].name, "Clock driver");
    proc_table[clockPID % MAXPROC].pid = clockPID;
    proc_table[clockPID % MAXPROC].status = ACTIVE;

    // start3 blocks until clock driver is running
    semp_real(clockSemaphore);

    // Create the disk driver processes
    for (i = 0; i < DISK_UNITS; i++) {
        diskSemaphore[i] = semcreate_real(0);
        sprintf(argBuffer, "%d", i);
        sprintf(name, "diskDriver%d", i);
        pid = fork1(name, DiskDriver, argBuffer, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            console("start3(): Can't create disk driver %d\n", i);
            halt(1);
        }

        diskPID[i] = pid; 

        int sector, track;
        diskSize_real(i, &sector, &track, &tracksOnDisk[i]);

        strcpy(proc_table[pid % MAXPROC].name, name);
        proc_table[pid % MAXPROC].pid = pid;
        proc_table[pid % MAXPROC].status = ACTIVE;
    } 
    
    pid = spawn_real("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = wait_real(&status);

    /*
     * Zap the device drivers
     */
    zap(clockPID);  // clock driver
    for (i = 0; i < DISK_UNITS; i++) {  // disk drivers

        //Unblock the disk drivers
        semv_real(diskSemaphore[i]);
        zap(diskPID[i]);
    }

    quit(0);
    
} // start3


static int ClockDriver(char *arg) {
    int result;
    int status;

    // Alert parent that we're running, enable interrupts
    semv_real(clockSemaphore);
    enableInterrupts();

    // Infinite loop until we are zap'd
    while(! is_zapped()) {
        result = waitdevice(CLOCK_DEV, 0, &status);
        if (result != 0) {
            return 0;
        }
        
        // Check the time, and wake up any processes if needed
        while (head_sleep_list != NULL && 
            head_sleep_list->wake_time <= sys_clock()) {

            // Remove from the list
            int mboxID = head_sleep_list->mboxID;
            head_sleep_list = head_sleep_list->sleep_ptr;
            MboxSend(mboxID, NULL, 0);
        }
    }

    return 0;
}


/* ------------------------------------------------------------------------
    Name - DiskDriver
    Purpose - handles the disk request queue and processes the request by
              handing the request to the respective function
    Parameters - arg
    Returns - 0
    Side Effects - calls disk handler functions
    --------------------------------------------------------------------- */
static int DiskDriver(char *arg) {
    
    int result;
    int unit = atoi(arg);

    while(! is_zapped()) {
        
        // Check for requests on the queue
        if (head_disk_list[unit] != NULL){
            switch (head_disk_list[unit]->operation) {
                case DISK_READ:
                    result = diskReadHandler(unit); 
                    break;
                case DISK_WRITE:
                    result = diskWriteHandler(unit);
                    break;
                default:
                    console("DiskDriver: Invalid disk request.\n");
                    console("DiskDriver: %d.\n", head_disk_list[unit]->operation);
            }
        } else {
            // Nothing is on the list, block and wait for a new request
            semp_real(diskSemaphore[unit]); 
        }

        if (result < 0) {
            console("DiskDriver: Read/Write Fail!\n");
        }
    }

    return 0;
}


/* ------------------------------------------------------------------------
   Name - diskReadHandler
   Purpose - this is called by the disk driver to handle a disk reading request
             by reading and writing to the disk_buf
   Parameters - unit, unit for the device
   Returns - int, the result if successful
   Side Effects - Writes data to the disk_buf buffer
   ----------------------------------------------------------------------- */
int diskReadHandler(int unit) {
    char sectorBuffer[512];
    int bufferIndex = 0;
    int currentTrack = head_disk_list[unit]->start_track;
    int currentSector = head_disk_list[unit]->start_sector;

    // Setup disk seek request
    device_request dev_request;
    dev_request.opr = DISK_SEEK;
    dev_request.reg1 = ((void *) (long) currentTrack);
    
    // Start the initial seek 
    if (proc_output(&dev_request, unit) < 0) {
        return -1;
    }

    // Do all read operations taking into account that we might need to change tracks
    for (int i = 0; i < head_disk_list[unit]->sectors; i++) {
        if (currentSector == DISK_TRACK_SIZE) {
            currentSector = 0;
            currentTrack++;

            // Change the track to next track 
            if (currentTrack == tracksOnDisk[unit]) {  
                return -1;
            }

            dev_request.opr = DISK_SEEK;
            dev_request.reg1 = ((void *) (long) currentTrack);
            
            if (proc_output(&dev_request, unit) < 0) { 
                return -1;
            }
        }

        // Create a device request for read operation
        dev_request.opr = DISK_READ;
        dev_request.reg1 = ((void *) (long) currentSector);
        dev_request.reg2 = sectorBuffer;

        // Start the read
        if (proc_output(&dev_request, unit) < 0) {
            head_disk_list[unit] = head_disk_list[unit]->next;  // Take request off queue
            return -1;
        }

        // Copy what was read from the disk to the users buffer
        memcpy(((char *) head_disk_list[unit]->disk_buf) + bufferIndex, sectorBuffer, 512);

        // Offset buffer index
        bufferIndex += 512;
        currentSector++;
    }

    MboxSend(head_disk_list[unit]->mboxID, NULL, 0); // Now we can wake up the calling process
    head_disk_list[unit] = head_disk_list[unit]->next;  // Take request off the queue
    return 0;
}


/* ------------------------------------------------------------------------
   Name - diskWriteHandler
   Purpose - Called by the DiskDriver to process a disk write request. 
   Parameters - unit, the unit for the device
   Returns - int, the result if succesful
   Side Effects - Writes data to the given disk
   ----------------------------------------------------------------------- */
int diskWriteHandler(int unit){
    int status = 0;
    int currentTrack = head_disk_list[unit]->start_track;
    int currentSector = head_disk_list[unit]->start_sector;

    device_request dev_request;
    dev_request.opr = DISK_SEEK;
    dev_request.reg1 = ((void *) (long) currentTrack);
    
    // Initial seek to the track to write
    if (proc_output(&dev_request, unit) < 0) {
        return -1;
    }

    for (int i = 0; i < head_disk_list[unit]->sectors; i++) {

        // Check if track is full, if so move tracks and reset sector
        if (currentSector == DISK_TRACK_SIZE) {
            currentSector = 0;
            currentTrack++;

            // Check if all tracks on disk are used
            if (currentTrack == tracksOnDisk[unit]) {  
                return -1;
            }

            // Setup for a seek operation
            dev_request.opr = DISK_SEEK;
            dev_request.reg1 = ((void *) (long) currentTrack);

            // Seek to the next track to write
            if (proc_output(&dev_request, unit) < 0) {
                console("Seek fail\n");
                return -1;
            }
        }

        // Setup for a write operation
        dev_request.opr = DISK_WRITE;
        dev_request.reg1 = ((void *) (long) currentSector);
        dev_request.reg2 = head_disk_list[unit]->disk_buf + (512 * i);
        
        // Write sector to disk
        if (proc_output(&dev_request, unit) < 0) {
            head_disk_list[unit] = head_disk_list[unit]->next;  // Now we can remove the request from queue
            head_disk_list[unit]->status = status;
            console("write fail\n");
            return -1;
        }
        
        // Change sector
        currentSector++;
    }

    int mboxid = head_disk_list[unit]->mboxID;
    head_disk_list[unit] = head_disk_list[unit]->next;  // Remove the request from queue
    MboxSend(mboxid, NULL, 0);  // Wake up the calling process
    
    return 0;
}


/* ------------------------------------------------------------------------
   Name - proc_output
   Purpose - Processes all the device output requests for the disk devices
             Updates the head status when needed 
   Parameters - device_request *dev_request, unit
   Returns - int, the result of the request
   Side Effects - N/A
   ----------------------------------------------------------------------- */
int proc_output(device_request *dev_request, int unit){
    
    int status;
    int result;

    device_output(DISK_DEV, unit, dev_request);
    result = waitdevice(DISK_DEV, unit, &status);

    if (status == DEV_ERROR) {
        head_disk_list[unit]->status = status;
        return -1;
    }

    if (result != 0) {
        return -2;
    }

    head_disk_list[unit]->status = status;

    return 0;
}


/* ------------------------------------------------------------------------
   Name - sleep
   Purpose - Processes the sysargs and calls sleep_real 
             Also blocks the sleeping processes for seconds defined in args
   Parameters - sysargs args, arg1 - seconds to sleep
   Returns - N/A
   Side Effects - calls sleep_real()
   ----------------------------------------------------------------------- */
void sleep(sysargs *args) {
    int seconds = ((int) (long) args->arg1);
    if (sleep_real(seconds) < 0) {
        args->arg4 = ((void *) (long) -1);
    } else {
        args->arg4 = ((void *) (long) 0);
    }
}


/* ------------------------------------------------------------------------
   Name - sleep_real
   Purpose - Blocks the sleeping processes for given time in seconds
   Parameters - seconds, the seconds to sleep for
   Returns - int, the result
   Side Effects - blocks the sleeping process
   ----------------------------------------------------------------------- */
int sleep_real(int seconds) {
    if (seconds < 0) {
        return -1;
    }

    // Add to the process table
    addToProcessTable();

    // Process to add to the sleep list and block
    proc_ptr4 toAdd = &proc_table[getpid() % MAXPROC];

    int wake_time = sys_clock() + (1000000 * seconds);
    toAdd->wake_time = wake_time;

    // Add the process to the sleep list
    if (head_sleep_list == NULL) {
        head_sleep_list = toAdd;
    } else {
            proc_ptr4 temp = head_sleep_list;
        if (toAdd->wake_time >= head_sleep_list->wake_time) {
            proc_ptr4 temp2 = head_sleep_list->sleep_ptr;
            while (temp2 != NULL && toAdd->wake_time > temp2->wake_time) {
                temp = temp->sleep_ptr;
                temp2 = temp2->sleep_ptr;
            }
            temp->sleep_ptr = toAdd;
            toAdd->sleep_ptr = temp2;
        } else {
            toAdd->sleep_ptr = temp;
            head_sleep_list = toAdd;
        }
    }

    // Block on the private mailbox
    MboxReceive(proc_table[getpid() % MAXPROC].mboxID, NULL, 0);

    // Remove from the process table
    removeFromProcessTable();

    return 0;
}


/* ------------------------------------------------------------------------
   Name - diskRead
   Purpose - Takes sysargs and calls diskRead_real to add new disk read
             request to the request queue.
   Parameters - sysargs args
   Returns - N/A
   Side Effects - calls diskRead_real
   ----------------------------------------------------------------------- */
void diskRead(sysargs *args) {
    
    int result;
    
    void *buffer = args->arg1;
    int sectors = ((int) (long) args->arg2);
    int start_track = ((int) (long) args->arg3);
    int start_sector = ((int) (long) args->arg4);
    int unit = ((int) (long) args->arg5);

    result = diskRead_real(unit, start_track, start_sector, sectors, buffer);
    
    // If the arguments are invalid, store -1 in arg1 otherwise store 0 in arg4
    if (result == -1) {
        args->arg4 = ((void *) (long) -1);
    } else {
        args->arg4 = ((void *) (long) 0);
    }

    // Store the result of the disk read in arg1
    args->arg1 = ((void *) (long) result);
}


/* ------------------------------------------------------------------------
   Name - diskRead_real
   Purpose - Adds a new disk read request to the request queue and 
             blocks until the request is done
   Parameters - unit, start_track, start_sector, sectors, void *disk_buf
   Returns - the result
   Side Effects - Adds a new request to queue
   ----------------------------------------------------------------------- */
int diskRead_real(int unit, int start_track, int start_sector, int sectors, void *disk_buf) {

    driver_proc info;

    if (unit < 0 || unit > 1) {
        return -1;
    }
    if (start_track < 0 || start_track > tracksOnDisk[unit] - 1) {
        return -1;
    }
    if (start_sector < 0 || start_sector > DISK_TRACK_SIZE - 1) {
        return -1;
    }

    // New process started
    addToProcessTable();

    // Build the request to prepare for adding to queue
    info.unit = unit;
    info.start_track = start_track;
    info.start_sector = start_sector;
    info.sectors = sectors;
    info.disk_buf = disk_buf;
    info.mboxID = proc_table[getpid() % MAXPROC].mboxID;
    info.operation = DISK_READ;
    info.next = NULL;

    // Insert the request into the request queue
    insert_disk_request(&info);

    semv_real(diskSemaphore[unit]); // Wake up the driver

    MboxReceive(proc_table[getpid() % MAXPROC].mboxID, NULL, 0);
    
    // Done, remove process from table
    removeFromProcessTable();

    return info.status;
}


/* -----------------------------------------------------------------------
    Name - insert_disk_request
    Purpose - Inserts disk request into the request queue
    Parameters - driver_proc_ptr
    Returns - N/A
    Side Effects - the request queue is modified
    --------------------------------------------------------------------- */
void insert_disk_request(driver_proc_ptr info) {
    int unit = info->unit;

    // Check if the list is empty here so we can just insert now
    if (head_disk_list[unit] == NULL) {
        head_disk_list[unit] = info;

    // Insert at the correct position
    } else {
        driver_proc_ptr tempA = head_disk_list[unit];
        driver_proc_ptr tempB = head_disk_list[unit]->next;
        if (info->start_track > head_disk_list[unit]->start_track) {
            while (tempB != NULL && tempB->start_track < info->start_track 
		   && tempB->start_track > tempA->start_track) {
                tempA = tempA->next;
                tempB = tempB->next;
            }
            tempA->next = info;
            info->next = tempB;
        } else {
            while (tempB != NULL && tempA->start_track <= tempB->start_track) {
                tempA = tempA->next;
                tempB = tempB->next;
            }
            while (tempB != NULL && tempB->start_track <= info->start_track) {
                tempA = tempA->next;
                tempB = tempB->next;
            }
            tempA->next = info;
            info->next = tempB;
        }
    }
}


/* ------------------------------------------------------------------------
   Name - diskWrite
   Purpose - Takes sysargs and calls diskWrite_real to add new
             disk write request to the queue
   Parameters - sysargs args
   Returns - N/A
   Side Effects - function call to diskWrite_real
   ----------------------------------------------------------------------- */
void diskWrite(sysargs *args) {
    int result;
    
    void *buffer = args->arg1;
    int sectors = ((int) (long) args->arg2);
    int start_track = ((int) (long) args->arg3);
    int start_sector = ((int) (long) args->arg4);
    int unit = ((int) (long) args->arg5);

    result = diskWrite_real(unit, start_track, start_sector, sectors, buffer);
    
    // If the arguments are invalid, store -1 in arg1, otherwise store 0 in arg4
    if (result == -1) {
        args->arg4 = ((void *) (long) -1);
    } else {
        args->arg4 = ((void *) (long) 0);
    }

    // Store the result of the disk read in arg1
    args->arg1 = ((void *) (long) result);
}


/* ------------------------------------------------------------------------
   Name - diskWrite_real
   Purpose - Puts a new disk read request to the queue and blocks until 
             request is done.
   Parameters - unit, start_track, start_sector, sectors,
                *disk_buf
   Returns - the result
   Side Effects - Adds a new request to the queue
   ----------------------------------------------------------------------- */
int diskWrite_real(int unit, int start_track, int start_sector, int sectors, void *disk_buf) {

    driver_proc info;

    if (unit < 0 || unit > 1) {
        return -1;
    }
    if (start_track < 0 || start_track > tracksOnDisk[unit] - 1) {
        return -1;
    }
    if (start_sector < 0 || start_sector > DISK_TRACK_SIZE - 1) {
        return -1;
    }

    // New process started
    addToProcessTable();

    // Build a request and add it to the queue
    info.unit = unit;
    info.start_track = start_track;
    info.start_sector = start_sector;
    info.sectors = sectors;
    info.disk_buf = disk_buf;
    info.mboxID = proc_table[getpid() % MAXPROC].mboxID;
    info.operation = DISK_WRITE;
    info.next = NULL;

    // Insert the request to the queue
    insert_disk_request(&info);

    semv_real(diskSemaphore[unit]);

    MboxReceive(proc_table[getpid() % MAXPROC].mboxID, NULL, 0);
    
    // Done, remove the process from the table
    removeFromProcessTable();
    
    return info.status;
}


/* ------------------------------------------------------------------------
   Name - diskSize
   Purpose - Takes sysargs and calls diskSize_real to add new
             disk size request to the queue
   Parameters - sysargs args
   Returns - N/A
   Side Effects - calls diskSize_real
   ----------------------------------------------------------------------- */
void diskSize(sysargs *args) {

    int result;

    int sectorSize;
    int sectorsInTrack;
    int tracksInDisk;
    
    int unit = ((int) (long) args->arg1);

    result = diskSize_real(unit, &sectorSize, &sectorsInTrack, &tracksInDisk);
    
    //If arguments are invalid, store -1 in arg1, otherwise store 0 in arg4
    if (result == -1) {
        args->arg4 = ((void *) (long) -1);
    } else {
        args->arg4 = ((void *) (long) 0);
    }

    // Store result of the read in arg1
    args->arg1 = ((void *) (long) sectorSize);
    args->arg2 = ((void *) (long) sectorsInTrack);
    args->arg3 = ((void *) (long) tracksInDisk);

    return;
}


/* ------------------------------------------------------------------------
   Name - diskSize_real
   Purpose - Requests disk size using the device_output function
   Parameters - sysargs args
   Returns - result of request
   Side Effects - N/A
   ----------------------------------------------------------------------- */
int diskSize_real(int unit, int *sectorSize, int *sectorsInTrack, int *tracksInDisk) {
    
    int status;
    int result;
    
    if (unit < 0 || unit > 1) {
        return -1;
    }
   
    // New process started
    addToProcessTable();

    // Create the device request 
    device_request dev_request;
    dev_request.opr = DISK_TRACKS;
    dev_request.reg1 = (void *) tracksInDisk;

    device_output(DISK_DEV, unit, &dev_request);
    result = waitdevice(DISK_DEV, unit, &status);

    if (status == DEV_ERROR) {
        return -1;
    }
    if (result != 0) {
        return -1;
    }

    *sectorSize = DISK_SECTOR_SIZE;
    *sectorsInTrack = DISK_TRACK_SIZE;

    // Done, remove from process table
    removeFromProcessTable();

    return 0;
}


/* -----------------------------------------------------------------------
    Halt USLOSS if the process is not in kernel mode 
    ------------------------------------------------------------------- */
void check_kernel_mode(char * process_name) {
    if((PSR_CURRENT_MODE & psr_get()) == 0) {
        console("check_kernel_mode(): called while in user mode, by process %s. Halting...\n", process_name);
        halt(1);
    }   
}


/* ----------------------------------------------------------------------
    Adds a process to the proc_table and creates a mailbox for it
   ------------------------------------------------------------------- */
void addToProcessTable() {
    if (getpid() !=  proc_table[getpid() % MAXPROC].pid) {
        proc_table[getpid() % MAXPROC].pid = getpid();
        proc_table[getpid() % MAXPROC].status = ACTIVE;
        proc_table[getpid() % MAXPROC].mboxID = MboxCreate(0,0);
        proc_table[getpid() % MAXPROC].sleep_ptr = NULL;
    }
}


/* ---------------------------------------------------------------------
    Removes a completed process from the proc_table and releases the 
    mailbox.
   ------------------------------------------------------------------ */
void removeFromProcessTable() {
    MboxRelease(proc_table[getpid() % MAXPROC].mboxID);
    proc_table[getpid() % MAXPROC].pid = -1;
    proc_table[getpid() % MAXPROC].status = EMPTY;
    proc_table[getpid() % MAXPROC].mboxID = -1;
    proc_table[getpid() % MAXPROC].sleep_ptr = NULL;
}


/* ---------------------------------------------------------------------
    Enables interrupts
   ------------------------------------------------------------------ */
void enableInterrupts() {
    psr_set(psr_get() | PSR_CURRENT_INT);
}
