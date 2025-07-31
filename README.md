# MPMC Lock-Free Bounded Queue

This is a **multi-producer, multi-consumer (MPMC) lock-free queue** implemented as a bounded ring/circular buffer. The queue size is fixed at creation to allow **full control over memory allocation** and enable natural **backpressure**.

## Features

- **Bounded capacity** for predictable memory usage.
- Designed for **high performance and low latency**.
- **False sharing avoidance** via cache line (64-byte) aligned structure members.
- Portable across **MSVC, GCC, and Clang** compilers.
- Can be used with C99 or later.

## Implementation details

- Uses **C11 atomics** when available for optimal atomic operations and explicit memory ordering.
- On **MSVC with C99**, falls back to Windows `Interlocked*` API calls, which always use `memory_order_seq_cst` semantics.
- On **GCC 4.7+ and Clang 3.3.3+**, uses the GCC/Clang `__atomic*` built-ins with explicit memory ordering.
- On older GCC and Clang versions, uses the older `__sync*` built-ins, which always use `memory_order_seq_cst` semantics.

## Platform / Compiler support summary

| Platform | Compiler Version           | Atomic API        | Memory Ordering Support       |
|----------|---------------------------|-------------------|------------------------------|
| MSVC     | Any (C99)                 | `Interlocked*`    | Only `memory_order_seq_cst`   |
| GCC      | ≥ 4.7                     | `__atomic*`       | Explicit memory order support |
| GCC      | < 4.7                     | `__sync*`         | Only `memory_order_seq_cst`   |
| Clang    | ≥ 3.3.3                   | `__atomic*`       | Explicit memory order support |
| Clang    | < 3.3.3                   | `__sync*`         | Only `memory_order_seq_cst`   |
