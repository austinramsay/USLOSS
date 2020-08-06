Phase 4 ReadMe

1. Austin Ramsay, Raymond Howard

2. Systems used to develop Phase 4 are: The OS's being used are Windows 10 and Ubuntu 19.10. The GCC version being used by the Windows 10 OS is GCC 	  4.4.7, the GCC version used by the Unbuntu 19.10 OS is GCC 9.2.1-9ubuntu2. 
   
3. The output is from my system as we were coming across the same error for the both of us. Everything parses until Terminate(0)
   is called then we start getting check_deadlock() to activate when there is no need for according to your testResults.txt. 
   I don't know if its a simple fix and we're both over-thinking it or its a foundational problem that we missed during this phase.
   
4. /* --------------------------
	CSCV 452

	Austin Ramsay
	Raymond Howard
	-------------------------- */
	|Tests with Failures: 
	 Tests 00 - 12.
	 I would consider these tests partial failures as they correctly
	 parse and run until the very end then our check_deadlock()
	 function activates instead of correctly terminating but otherwise
	 everything before that is correct.|
	-----------------------------------------------------------------
	starting test 00 ....

	start4(): Start 5 children who all sleep for 10 seconds. Upon
			  waking up, each child checks if its sleep time was at
			  least 10 seconds.
	Child1(): Going to sleep for 10 seconds
	Child2(): Going to sleep for 10 seconds
	Child3(): Going to sleep for 10 seconds
	Child4(): Going to sleep for 10 seconds
	Child5(): Going to sleep for 10 seconds
	Child1(): Sleep done at time 10090115
	Child2(): Sleep done at time 10090144
	Child3(): Sleep done at time 10090173
	Child4(): Sleep done at time 10090197
	Child5(): Sleep done at time 10090227
	start4(): Test sleep done.
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 01 ....

	start4(): Spawning Child(0)
	start4(): Spawning Child(1)
	start4(): Spawning Child(2)
	start4(): Spawning Child(3)
	start4(): Spawning Child(4)
	start4(): Spawning Child(5)
	start4(): Spawning Child(6)
	start4(): Spawning Child(7)
	start4(): Spawning Child(8)
	start4(): Spawning Child(9)
	start4(): Waiting on Child
	Child0(): Sleeping for 30 seconds
	Child1(): Sleeping for 27 seconds
	Child2(): Sleeping for 24 seconds
	Child3(): Sleeping for 21 seconds
	Child4(): Sleeping for 18 seconds
	Child5(): Sleeping for 15 seconds
	Child6(): Sleeping for 12 seconds
	Child7(): Sleeping for 9 seconds
	Child8(): Sleeping for 6 seconds
	Child9(): Sleeping for 3 seconds
	Child9(): After sleeping 3 seconds, diff in sys_clock is 3039705
	start4(): Wait returned 0, pid:18, status 19
	start4(): Waiting on Child
	Child8(): After sleeping 6 seconds, diff in sys_clock is 6039734
	start4(): Wait returned 0, pid:17, status 18
	start4(): Waiting on Child
	Child7(): After sleeping 9 seconds, diff in sys_clock is 9039762
	start4(): Wait returned 0, pid:16, status 17
	start4(): Waiting on Child
	Child6(): After sleeping 12 seconds, diff in sys_clock is 12039793
	start4(): Wait returned 0, pid:15, status 16
	start4(): Waiting on Child
	Child5(): After sleeping 15 seconds, diff in sys_clock is 15039814
	start4(): Wait returned 0, pid:14, status 15
	start4(): Waiting on Child
	Child4(): After sleeping 18 seconds, diff in sys_clock is 18039854
	start4(): Wait returned 0, pid:13, status 14
	start4(): Waiting on Child
	Child3(): After sleeping 21 seconds, diff in sys_clock is 21039886
	start4(): Wait returned 0, pid:12, status 13
	start4(): Waiting on Child
	Child2(): After sleeping 24 seconds, diff in sys_clock is 24039909
	start4(): Wait returned 0, pid:11, status 12
	start4(): Waiting on Child
	Child1(): After sleeping 27 seconds, diff in sys_clock is 27039929
	start4(): Wait returned 0, pid:10, status 11
	start4(): Waiting on Child
	Child0(): After sleeping 30 seconds, diff in sys_clock is 30039946
	start4(): Wait returned 0, pid:9, status 10
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 02 ....

	start4(): started
	start4(): unit 0, sector size 512, track size 16, disk size 16
	start4(): unit 1, sector size 512, track size 16, disk size 32
	start4(): calling Terminate
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 03 ....

	start4(): started
	start4(): unit 0, sector size 512, track size 16, disk size 16
	start4(): unit 1, sector size 512, track size 16, disk size 32

	start4(): writing to disk 0, track 5, sector 0
	start4(): DiskWrite returned status = 0

	start4(): reading from disk 0, track 5, sector 0
	start4(): DiskRead returned status = 0
	start4(): disk_buf_B contains:
	A wonderful message to put on the disk...
	start4(): calling Terminate
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 04 ....

	start4(): Testing disk 0
	start4(): Read from disk: This is a test

	start4(): Read from disk: Does it work?

	start4(): Read from disk: One last chance

	start4(): Testing disk 1
	start4(): Read from disk: This is a test

	start4(): Read from disk: Does it work?

	start4(): Read from disk: One last chance

	start4(): Terminating
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 05 ....

	start4(): Writing data to 3 disk sectors, then reading them back
			  Confirm that data read back matches data read in
	start4(): Testing Disk 0
	start4(): Test of disk 0 succeeded.
	start4(): Testing Disk 1
	start4(): Test of disk 1 succeeded.
	start4(): Done.
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 06 ....

	start4(): Writing data to 3 disk sectors, wrapping to next track

	start4(): Disk 0:
	start4(): Read from disk: This is a test

	start4(): Read from disk: Does it work?

	start4(): Read from disk: One last chance


	start4(): Disk 1:
	start4(): Read from disk: This is a test

	start4(): Read from disk: Does it work?

	start4(): Read from disk: One last chance

	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 07 ....

	start4(): disk scheduling test, create 4 processes that write
			  various places on the disk.  After the 4 processes are
			  done, start4 reads the 4 locations & prints the
			  results.  Uses disk 1.
	start4(): read: One flew East
	start4(): read: One flew West
	start4(): read: One flew over the coo-coo's nest
	start4(): read: --did it work?
	start4(): done 111
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 08 ....

	start4(): Attempt to write to a non-existant disk, disk 3
	start4(): done
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 09 ....

	start4(): disk scheduling test, 8 children
			  algorithm test           
			  different tracks
	going to write track 5
	going to write track 3
	going to write track 9
	going to write track 0
	going to write track 7
	going to write track 2
	going to write track 1
	going to write track 6
	after writing track 5
	process 9 quit with status 2
	after writing track 6
	process 16 quit with status 9
	after writing track 7
	process 13 quit with status 6
	after writing track 9
	process 11 quit with status 4
	after writing track 0
	process 12 quit with status 5
	after writing track 1
	process 15 quit with status 8
	after writing track 2
	process 14 quit with status 7
	after writing track 3
	process 10 quit with status 3
	start4(): done 33
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 10 ....

	start4(): disk scheduling test, 8 children
			  algorithm test           
			  same track different sectors
	going to write to sector 5
	going to write to sector 3
	going to write to sector 9
	going to write to sector 0
	going to write to sector 7
	going to write to sector 2
	going to write to sector 1
	going to write to sector 6
	after writing to sector 5
	process 9 quit with status 2
	after writing to sector 3
	process 10 quit with status 3
	after writing to sector 9
	process 11 quit with status 4
	after writing to sector 0
	process 12 quit with status 5
	after writing to sector 7
	process 13 quit with status 6
	after writing to sector 2
	process 14 quit with status 7
	after writing to sector 1
	process 15 quit with status 8
	after writing to sector 6
	process 16 quit with status 9
	start4(): done 33
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 11 ....

	start4(): disk scheduling test, create 6 processes that write
			  3 to disk0         
			  3 to disk1         
	ubiq0: going to write to track 5
	ubiq0: going to write to track 3
	ubiq0: going to write to track 9
	ubiq1: going to write to track 4
	ubiq1: going to write to track 2
	ubiq1: going to write to track 8
	ubiq0: after writing to track 5
	process 9 quit with status 2
	ubiq1: after writing to track 4
	process 12 quit with status 5
	ubiq0: after writing to track 9
	process 11 quit with status 4
	ubiq1: after writing to track 8
	process 14 quit with status 7
	ubiq0: after writing to track 3
	process 10 quit with status 3
	ubiq1: after writing to track 2
	process 13 quit with status 6
	start4(): done 31
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------
	starting test 12 ....

	start4(): Spawning 5 children to sleep
	ChildS(0): Sleeping for 5 seconds
	ChildS(1): Sleeping for 4 seconds
	ChildS(2): Sleeping for 3 seconds
	ChildS(3): Sleeping for 2 seconds
	ChildS(4): Sleeping for 1 seconds
	start4(): Spawning 4 children to diskfuncs

	ChildDW0(): writing to disk 0, track 5, sector 0

	ChildDW1(): writing to disk 1, track 5, sector 0

	ChildR0(): reading from disk 0, track 5, sector 0

	ChildR1(): reading from disk 1, track 5, sector 0
	ChildDW0(): DiskWrite0 returned status = 0
	ChildDW1(): DiskWrite1 returned status = 0
	ChildR0(): DiskRead returned status = 0
	ChildR0(): disk_buf_B contains:
	ChildDW0(): A wonderful message to put on the disk...
	ChildR1(): DiskRead returned status = 0
	ChildR1(): disk_buf_B contains:
	ChildDW1(): A wonderful message to put on the disk...
	ChildS(4): After sleeping 1 seconds, diff in sys_clock is 1049548
	ChildS(3): After sleeping 2 seconds, diff in sys_clock is 2049647
	ChildS(2): After sleeping 3 seconds, diff in sys_clock is 3049754
	ChildS(1): After sleeping 4 seconds, diff in sys_clock is 4049848
	ChildS(0): After sleeping 5 seconds, diff in sys_clock is 5049939
	start4(): done.
	check_deadlock(): num_proc = 6
	check_deadlock(): processes still present.  Halting...
	
	Evaluation: PARTIAL FAILURE
	-----------------------------------------------------------------


