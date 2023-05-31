# Project 3

## Group Members

- Jace Halvorson (halvo561)
- Ryan Johnsen (joh18447)

## Running Test Programs

```
$ make virtmem
$ ./virtmem <npages> <nframes> <rand|fifo|cust> <scan|sort|focus>
```

## Page Replacement Algorithms

### rand (Random eviction)

- The rand algorithm chooses a random frame to evict

### fifo (First-in-first-out)

- The fifo algorithm evicts the frame that has been in physical memory the longest

### cust (Least-frequently-faulted)

- This is the custom page replacement algorithm that our group came up with
- This algorithm evicts the page in physical memory that has the lowest number
  of faults throughout its lifetime.
- It is implemented using a min-priority queue based on the number of faults
  a page has had.
- When it comes time to evict a page, all we have to do is...
  1. Pop the top element off of the priority queue and write that element's page
  back to disk. 
  2. Add the new page coming into physical memory and its updated fault count 
  into the priority queue.
- The fault counts of pages not in physical memory are stored in a hash map for 
  fast lookup. 

If you want to see this algorithm in action, set `DEBUGC = 1` in main.cpp
to get more detailed debugging statements.