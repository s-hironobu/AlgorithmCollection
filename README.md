#  Educational Parallel Algorithm Collection

This is a parallel algorithm collection written in C. This collection contains several principal programs described in the book "The Art of Multiprocessor Programming (M. Herlihy, N. Shavit)" .

## Algorithms

### Queue

 + LLSCLockFreeQueue
  - <a href="http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.170.1375">"Bringing Practical LockFree Synchronization to 64Bit Applications"</a> by Simon Doherty, Maurice Herlihy, Victor Luchangco, Mark Moir
 + CASLockFreeQueue
  - <a href="http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf">"Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms"</a> by M. Michael and M. Scott

### List

 + CoarseGrainedSynchroList
  - Coarse-Grained Synchronization Singly-linked List
 + FineGrainedSynchroList
  - Fine-Grained Synchronization Singly-linked List
 + LazySynchroList
  - Lazy Synchronization Singly-linked List
 + NonBlockingList
  - <a href="http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.16.1384">"A Pragmatic Implementation of Non-Blocking Linked-Lists"</a> by Timothy L. Harris 
 + LockFreeList
  - <a href="http://www.cse.yorku.ca/~ruppert/papers/lfll.pdf">"Lock-Free Linked Lists and Skip Lists"</a> by Mikhail Fomitchev, Eric Ruppert

#### SkipList
 + Skiplist
 + LazySkiplist
  -  <a href="http://www.cs.brown.edu/~levyossi/Pubs/LazySkipList.pdf">"A Simple Optimistic skip-list Algorithm"</a> by Maurice Herlihy, Yossi Lev, Victor Luchangco, Nir Shavit
 + LockFreeSkiplist
  - <a href="http://www.cs.brown.edu/courses/csci1760/ch14.ppt">"A Lock-Free concurrent skiplist with wait-free search"</a> by Maurice Herlihy & Nir Shavit

### Hash
 + Hash
  - (Chain) Hash Table
 + OpenAddressHash
  - Open-Addressed Hash Table
 + StripedHash
  - Striped Hash Table
 + RefinableHash
  - Refinable Hash Table
 + CuckooHash
  -  <a href="http://cs.nyu.edu/courses/fall05/G22.3520-001/cuckoo-jour.pdf">"Cuckoo Hashing"</a> by R.Pagh, F.F.Rodler
 + ConcurrentCuckooHash
  - Concurrent Cuckoo Hash Table


## Compile on Linux(X86_64) and OSX

    $ make
    $ make test

## How to use
Here, I will be explained using queue/LLSCLockFreeQueue.

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

