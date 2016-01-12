# DAVE

## Description

Distributed Asynchronous Verification Emulator (DAVE) is a software to verify the correctness of distributed and hence asynchronous algorithms in case of different failings including node crashes, disconnections etc

## Build Requirements

* Supported compilers (must support C++11):
    * GCC
    * Clang
    * MSVC
* Libraries: BOOST, version >= 1.56

## Replicated Object Verification

DAVE allows to verify masterless consensus algorithm known as *replob*. For detailed information please read the following articles:

1. [Replicated Object. Part 1: Introduction](http://gridem.blogspot.com/2015/09/replicated-object-part-1-introduction.html)
1. [Replicated Object. Part 2: God Adapter](http://gridem.blogspot.com/2015/11/replicated-object-part-2-god-adapter.html)

Configuration parameters:

1. Number of nodes `nodes`: 3
2. Maximum number of failed nodes per each execution `maxFailedNodes`: 1
3. Minimal node id of unreliable node `minUnreliableNode`: 1 (meaning that node `id == 0` is reliable always while others are not).

```cpp
struct Config
{
    int nodes = 3;
    int maxFailedNodes = 1;
    int minUnreliableNode = 1;
	// ...
};
```

Number of performed executions to check all variants:

| Proposals | Proposal Nodes, id | Executions |
|:-:|:-:|--:|
| 1 | 0 | 59 986 |
| 2 | 0, 1 | 148 995 211 |
| 3 | 0, 1, 2 | 734 368 600 |

Clients propose the messages started from node `id == 0`.

Verification checks:

1. Nonfailed nodes must commit.
2. Failed node may or may not commit.
3. Any committed node (failed or nonfailed) must have the same committed sequence of messages: agreement.
