#  Educational Parallel Algorithm Collection

This is a parallel algorithm collection written in C. It contains fifteen programs that are explained in the book "The Art of Multiprocessor Programming (M. Herlihy, N. Shavit)".

## Algorithms

### Queue

 1. LLSCLockFreeQueue
  - <a href="http://www.cs.tau.ac.il/~afek/p31-64bitCASdoherty.pdf">"Bringing Practical LockFree Synchronization to 64Bit Applications"</a> by Simon Doherty, Maurice Herlihy, Victor Luchangco, Mark Moir
 2. CASLockFreeQueue
  - <a href="http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf">"Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms"</a> by M. Michael and M. Scott

### List

 1. CoarseGrainedSynchroList
  - Coarse-Grained Synchronization Singly-linked List
 2. FineGrainedSynchroList
  - Fine-Grained Synchronization Singly-linked List
 3. LazySynchroList
  - Lazy Synchronization Singly-linked List
 4. NonBlockingList
  - <a href="https://www.cl.cam.ac.uk/research/srg/netos/papers/2001-caslists.pdf">"A Pragmatic Implementation of Non-Blocking Linked-Lists"</a> by Timothy L. Harris
 5. LockFreeList
  - <a href="http://www.cse.yorku.ca/~ruppert/papers/lfll.pdf">"Lock-Free Linked Lists and Skip Lists"</a> by Mikhail Fomitchev, Eric Ruppert

#### SkipList
 1. Skiplist
 2. LazySkiplist
  -  <a href="http://people.csail.mit.edu/shanir/publications/LazySkipList.pdf">"A Simple Optimistic skip-list Algorithm"</a> by Maurice Herlihy, Yossi Lev, Victor Luchangco, Nir Shavit
 3. LockFreeSkiplist
  - <a href="http://www.cs.brown.edu/courses/csci1760/ch14.ppt">"A Lock-Free concurrent skiplist with wait-free search"</a> by Maurice Herlihy & Nir Shavit

### Hash
 1. Hash
  - (Chain) Hash Table
 2. OpenAddressHash
  - Open-Addressed Hash Table
 3. StripedHash
  - Striped Hash Table
 4. RefinableHash
  - Refinable Hash Table
 5. CuckooHash
  -  <a href="https://www.brics.dk/RS/01/32/BRICS-RS-01-32.pdf">"Cuckoo Hashing"</a> by R.Pagh, F.F.Rodler
 6. ConcurrentCuckooHash
  - Concurrent Cuckoo Hash Table


## Compile on Linux(X86_64) and OSX

    $ make
    $ make test

## How to use

### Usage

    $ ./queue/LLSCLockFreeQueue -h
    simple algorithm test bench
    usage: ./queue/LLSCLockFreeQueue [Options<default>]
    		-t number_of_thread<10>
    		-n number_of_item<1000>
    		-v               :verbose
    		-V               :debug mode
    		-h               :help


Some programs have other options. Please check each.

### Execute

By default, run 10 threads, and each thread inserts and deletes 1000 items.

    $ ./queue/LLSCLockFreeQueue
    <<simple algorithm test bench>>
    RESULT: test OK
    condition =>
    	  10 threads run
    	  1000 items inserted and deleted / thread, total 10000 items
    performance =>
    	    interval =  0.006693 [sec]
    	    thread info:
    	      ave. = 0.002807[sec], min = 0.000832[sec], max = 0.004397[sec]  


You can change the number of items and the number of threads.

    $ ./queue/LLSCLockFreeQueue -t 20 -n 10000
    <<simple algorithm test bench>>
    RESULT: test OK
    condition =>
    	  20 threads run
    	  10000 items inserted and deleted / thread, total 200000 items
    performance =>
    	    interval =  0.102954 [sec]
    	    thread info:
    	      ave. = 0.050064[sec], min = 0.008257[sec], max = 0.059089[sec]

#### Verbose & Debug mode
You can see how to work in the LLSCLockFreeQueue program.

    $ ./queue/LLSCLockFreeQueue -t 2 -n 4 -V
    <<simple algorithm test bench>>
    thread[1] add: 5
    [5]
    thread[1] add: 6
    [5][6]
    thread[1] add: 7
    [5][6][7]
    thread[1] add: 8
    [5][6][7][8]
    thread[0] add: 1
    [5][6][7][8][1]
    thread[0] add: 2
    [5][6][7][8][1][2]
    thread[0] add: 3
    [5][6][7][8][1][2][3]
    thread[0] add: 4
    [5][6][7][8][1][2][3][4]
    thread[0] delete: 5
    [6][7][8][1][2][3][4]
    thread[0] delete: 6
    [7][8][1][2][3][4]
    thread[0] delete: 7
    [8][1][2][3][4]
    thread[0] delete: 8
    [1][2][3][4]
    thread[1] delete: 1
    [2][3][4]
    thread[1] delete: 2
    [3][4]
    thread[1] delete: 3
    [4]
    thread[1] delete: 4

    thread(0) end 0.002006[sec]
    thread(1) end 0.006205[sec]
    RESULT: test OK
    condition =>
    	2 threads run
    	4 items inserted and deleted / thread, total 8 items
    performance =>
    	interval =  0.006519 [sec]
    	thread info:
    	  ave. = 0.004106[sec], min = 0.002006[sec], max = 0.006205[sec]



### Test
Run the LLSCLockFreeQueue_test. You can see more easily how to work this program.

    $ ./queue/LLSCLockFreeQueue_test
    [0]
    [0][1]
    [0][1][2]
    [0][1][2][3]
    [0][1][2][3][4]
    [0][1][2][3][4][5]
    [0][1][2][3][4][5][6]
    [0][1][2][3][4][5][6][7]
    [0][1][2][3][4][5][6][7][8]
    [0][1][2][3][4][5][6][7][8][9]
    [1][2][3][4][5][6][7][8][9]
    [2][3][4][5][6][7][8][9]
    [3][4][5][6][7][8][9]
    [4][5][6][7][8][9]
    [5][6][7][8][9]
    [6][7][8][9]
    [7][8][9]
    [8][9]
    [9]

