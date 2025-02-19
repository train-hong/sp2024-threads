# Matrix Multiplication Machine

## Overview
This project implements a **multithreaded matrix multiplication system** using a **thread pool**. The system efficiently handles matrix multiplication requests by utilizing:
- A **frontend thread** that receives requests, transposes matrices, and distributes computation tasks.
- A **backend thread pool** that executes matrix multiplication using multiple worker threads.

The implementation ensures efficient parallel computation using **thread synchronization mechanisms** such as mutexes and condition variables.

## Features
- Supports **multi-threaded matrix multiplication** using a **thread pool**.
- Uses **two work queues**:
  - A **frontend queue** for handling matrix multiplication requests.
  - A **worker queue** for storing individual computation tasks.
- Implements **thread synchronization** to prevent race conditions.
- Supports dynamic allocation and cleanup of resources.

## Files
- `tpool.h` – Header file defining data structures and function prototypes.
- `tpool.c` – Implementation of the thread pool, including request handling and work execution.

## API Functions
### `struct tpool* tpool_init(int num_threads, int n);`
Initializes a thread pool with `num_threads` worker threads and matrix size `n x n`.

### `void tpool_request(struct tpool*, Matrix a, Matrix b, Matrix c, int num_works);`
Submits a matrix multiplication request for matrices `a` and `b`, storing results in `c`.

### `void tpool_synchronize(struct tpool*);`
Waits for all submitted tasks to complete before returning.

### `void tpool_destroy(struct tpool*);`
Cleans up resources and stops all threads.

## Implementation Details
- The **frontend thread** transposes matrix `b` and splits the multiplication into smaller work units.
- The **backend threads** compute the dot product for assigned matrix elements using the provided `calculation()` function.
- **Synchronization mechanisms** ensure that all requests are processed correctly before termination.

## Author
- **Train Hong**
