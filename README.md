#  An Educational Parallel Algorithm Collection

This is an educational parallel algorithm collection in C of some samples of the book "The Art of Multiprocessor Programming (M. Herlihy, N. Shavit)" .


## Sources

### Queue

 + LLSCLockFreeQueue
  - <a href="http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.170.1375">"Bringing Practical LockFree Synchronization to 64Bit Applications"</a> by Simon Doherty, Maurice Herlihy, Victor Luchangco, Mark Moir

### List

 + CoarseGrainedSynchroList
  - Coarse-Grained Synchronization Singly-linked List
 + FineGrainedSynchroList
  - Fine-Grained Synchronization Singly-linked List
 + LazySynchroList
  - Lazy Synchronization Singly-linked List
 + NonBlockingList
  - <a href="http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.16.1384">"A Pragmatic Implementation of Non-Blocking Linked-Lists"</a> Timothy L. Harris 
 + LockFreeList
  - <a href="http://www.cse.yorku.ca/~ruppert/papers/lfll.pdf">"Lock-Free Linked Lists and Skip Lists"</a> Mikhail Fomitchev, Eric Ruppert

#### SkipList
 + Skiplist
 + LazySkiplist
  -  <a href="http://www.cs.brown.edu/~levyossi/Pubs/LazySkipList.pdf">"A Simple Optimistic skip-list Algorithm"</a> Maurice Herlihy, Yossi Lev, Victor Luchangco, Nir Shavit
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
  -  <a href="http://cs.nyu.edu/courses/fall05/G22.3520-001/cuckoo-jour.pdf">"R.Pagh, F.F.Rodler, Cuchoo Hashing" </a>
 + ConcurrentCuckooHash
  - Concurrent Cuckoo Hash Table


## Compile

    make
    make test

## Example
Here, I will be explained using queue/LLSCLockFreeQueue.


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

Help message is:

     ./queue/LLSCLockFreeQueue -h
    simple algorithm test bench
    usage: ./queue/LLSCLockFreeQueue [Options<default>]
    		-t number_of_thread<10>
    		-n number_of_item<1000>
    		-v               :verbose
    		-V               :debug mode
    		-h               :help

Default parameters are able to change using '-n' and '-t' options.

    $./queue/LLSCLockFreeQueue -t 20 -n 10000
    <<simple algorithm test bench>>
    RESULT: test OK
    condition =>
    	  20 threads run
    	  10000 items inserted and deleted / thread, total 200000 items
    performance =>
    	    interval =  0.102954 [sec]
    	    thread info:
    	      ave. = 0.050064[sec], min = 0.008257[sec], max = 0.059089[sec]

Run the LLSCLockFreeQueue_test.You can see how to work in the LLSCLockFreeQueue program in a single thread.


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


Every programs have test programs.

