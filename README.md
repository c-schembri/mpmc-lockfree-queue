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
- On **MSVC with C11**, uses the C11 `<stdatomic.h>` atomics for explicit memory ordering.
- On **GCC and Clang with C11 enabled**, uses C11 atomics (`<stdatomic.h>`) for explicit memory ordering.
- On **GCC 4.7+ and Clang 3.3.3+ without C11**, uses the GCC/Clang `__atomic*` built-ins with explicit memory ordering.
- On older GCC and Clang versions, or without C11, uses the older `__sync*` built-ins, which always use `memory_order_seq_cst` semantics.

## Platform / Compiler support summary

| Platform | Compiler Version          | C Standard          | Atomic API        | Memory Ordering Support       |
|----------|---------------------------|---------------------|-------------------|-------------------------------|
| MSVC     | ≤ 2022 17.5               | C99                 | `Interlocked*`    | Only `memory_order_seq_cst`   |
| MSVC     | ≥ 2022 17.5               | C11                 | C11 atomics       | Explicit                      |
| GCC      | ≥ 4.7                     | C11                 | C11 atomics       | Explicit                      |
| GCC      | ≥ 4.7                     | C99                 | `__atomic*`       | Explicit                      |
| GCC      | < 4.7                     | C99                 | `__sync*`         | Only `memory_order_seq_cst`   |
| Clang    | ≥ 3.3.3                   | C11                 | C11 atomics       | Explicit                      |
| Clang    | ≥ 3.3.3                   | C99                 | `__atomic*`       | Explicit                      |
| Clang    | < 3.3.3                   | C99                 | `__sync*`         | Only `memory_order_seq_cst`   |
