#pragma once

#include <string.h> // memcpy

// memory order rationale:
// - acq_rel on head/tail counters ensures visibility of slot state
// - acquire load ensures reader sees slot->turn and slot->data updated
// - release store ensures writer publishes data before updating turn

#define MPMC_LOCKFREE_INTERNAL static inline
#define MPMC_LOCKFREE_API      static inline

typedef signed long long __mpmc_int64;

#if MPMC_USE_C11_ATOMICS
  // use c11 atomic* api

  #include <stdatomic.h>
  #include <stdalign.h>

  #define MPMC_LOCKFREE_CACHE_ALIGNED alignas(MPMC_LOCKFREE_CACHE_LINE)
  #define MPMC_LOCKFREE_ATOMIC _Atomic __mpmc_int64

  #define ATOMIC_FETCH_ADD(Ptr, Val, MemoryOrder) atomic_fetch_add_explicit((Ptr), (Val), (MemoryOrder))
  #define ATOMIC_LOAD(Ptr, MemoryOrder)           atomic_load_explicit((Ptr), (MemoryOrder))
  #define ATOMIC_STORE(Ptr, Val, MemoryOrder)     atomic_store_explicit((Ptr), (Val), (MemoryOrder))
  #define ATOMIC_COMPARE_EXCHANGE_STRONG(Ptr, Expected, Desired, Success, Failure) \
    atomic_compare_exchange_strong_explicit((Ptr), (Expected), (Desired), (Success), (Failure))

  #define MPMC_LOCKFREE_MO_RELAXED memory_order_relaxed
  #define MPMC_LOCKFREE_MO_CONSUME memory_order_consume
  #define MPMC_LOCKFREE_MO_ACQUIRE memory_order_acquire
  #define MPMC_LOCKFREE_MO_RELEASE memory_order_release
  #define MPMC_LOCKFREE_MO_ACQ_REL memory_order_acq_rel
  #define MPMC_LOCKFREE_MO_SEQ_CST memory_order_seq_cst

#else
  #if defined(_MSC_VER)
    // use Interlocked* api

    #pragma warning(disable:4324) // alignment/struct padding warning

    #include <windows.h>

    #define MPMC_LOCKFREE_ATOMIC volatile __mpmc_int64
    #define MPMC_LOCKFREE_CACHE_ALIGNED __declspec(align(MPMC_LOCKFREE_CACHE_LINE))

    #define ATOMIC_FETCH_ADD(Ptr, Val, MemoryOrder) InterlockedExchangeAdd64((Ptr), (Val))
    #define ATOMIC_LOAD(Ptr, MemoryOrder)           InterlockedCompareExchange64((Ptr), 0, 0)
    #define ATOMIC_STORE(Ptr, Val, MemoryOrder)     InterlockedExchange64((LONGLONG volatile *)(Ptr), (Val))

    MPMC_LOCKFREE_INTERNAL int
    __mpmc_interlocked_compare_exchange64(__mpmc_int64 volatile *ptr, __mpmc_int64 *expected, __mpmc_int64 desired) {
      __mpmc_int64 old = InterlockedCompareExchange64(ptr, desired, *expected);
      if (old == *expected) {
        return 1;
      } else {
        *expected = old;
        return 0;
      }
    }

    #define ATOMIC_COMPARE_EXCHANGE_STRONG(Ptr, Expected, Desired, Success, Failure) \
      __mpmc_interlocked_compare_exchange64((__mpmc_int64 volatile *)(Ptr), (Expected), (Desired))

    #define MPMC_LOCKFREE_MO_RELAXED 
    #define MPMC_LOCKFREE_MO_CONSUME 
    #define MPMC_LOCKFREE_MO_ACQUIRE 
    #define MPMC_LOCKFREE_MO_RELEASE 
    #define MPMC_LOCKFREE_MO_ACQ_REL 
    #define MPMC_LOCKFREE_MO_SEQ_CST 

  #elif defined(__clang__) || defined(__GNUC__)
    #define MPMC_LOCKFREE_ATOMIC __mpmc_int64
    #define MPMC_LOCKFREE_CACHE_ALIGNED __attribute__((aligned(MPMC_LOCKFREE_CACHE_LINE)))

    #if defined(__clang__)
      #if __clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 3)
        #define HAS_ATOMIC_BUILTINS 1
      #else
        #define HAS_ATOMIC_BUILTINS 0
      #endif
    #elif defined(__GNUC__)
      #if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)
        #define HAS_ATOMIC_BUILTINS 1
      #else
        #define HAS_ATOMIC_BUILTINS 0
      #endif
    #else
      #define HAS_ATOMIC_BUILTINS 0
    #endif // defined(__clang__)

    #if HAS_ATOMIC_BUILTINS
      // use __atomic* api

      #define MPMC_LOCKFREE_MO_RELAXED __ATOMIC_RELAXED
      #define MPMC_LOCKFREE_MO_CONSUME __ATOMIC_CONSUME
      #define MPMC_LOCKFREE_MO_ACQUIRE __ATOMIC_ACQUIRE
      #define MPMC_LOCKFREE_MO_RELEASE __ATOMIC_RELEASE
      #define MPMC_LOCKFREE_MO_ACQ_REL __ATOMIC_ACQ_REL
      #define MPMC_LOCKFREE_MO_SEQ_CST __ATOMIC_SEQ_CST

      #define ATOMIC_FETCH_ADD(Ptr, Val, MemoryOrder) __atomic_fetch_add((Ptr), (Val), (MemoryOrder))
      #define ATOMIC_LOAD(Ptr, MemoryOrder)           __atomic_load_n((Ptr), (MemoryOrder))
      #define ATOMIC_STORE(Ptr, Val, MemoryOrder)     __atomic_store_n((Ptr), (Val), (MemoryOrder))
      #define ATOMIC_COMPARE_EXCHANGE_STRONG(Ptr, Expected, Desired, Success, Failure) \
        __atomic_compare_exchange_n( \
          (Ptr), (Expected), (Desired), \
          0, /* strong CAS */ \
          (Success), \
          (Failure) \
        )
    #else
      // use __sync* api

      #define ATOMIC_FETCH_ADD(Ptr, Val, MemoryOrder) __sync_fetch_and_add((Ptr), (Val))
      #define ATOMIC_LOAD(Ptr, MemoryOrder)           __sync_fetch_and_add((Ptr), 0)
      #define ATOMIC_STORE(Ptr, Val, MemoryOrder) do { \
        __sync_synchronize(); \
        *(Ptr) = (Val); \
        __sync_synchronize(); \
      } while (0)
      #define ATOMIC_COMPARE_EXCHANGE_STRONG(Ptr, Expected, Desired, Success, Failure) \
        ({ \
          __mpmc_int64 _old = __sync_val_compare_and_swap((Ptr), *(Expected), (Desired)); \
          if (_old == *(Expected)) { \
            1; \
          } else { \
            *(Expected) = _old; \
            0; \
          } \
        })

      #define MPMC_LOCKFREE_MO_RELAXED 
      #define MPMC_LOCKFREE_MO_CONSUME 
      #define MPMC_LOCKFREE_MO_ACQUIRE 
      #define MPMC_LOCKFREE_MO_RELEASE 
      #define MPMC_LOCKFREE_MO_ACQ_REL 
      #define MPMC_LOCKFREE_MO_SEQ_CST 

    #endif // HAS_ATOMIC_BUILTINS
  #else
    #error "Unsupported C compiler"
  #endif // defined(__clang__) || defined(__GNUC__)
#endif // MPMC_USE_C11_ATOMICS

#define MPMC_LOCKFREE_CACHE_LINE  64
#define MPMC_LOCKFREE_SLOTS_COUNT 12
#define MPMC_LOCKFREE_SLOT        64

typedef struct mpmc_lockfree_slot_t mpmc_lockfree_slot_t;
struct mpmc_lockfree_slot_t {
  MPMC_LOCKFREE_CACHE_ALIGNED MPMC_LOCKFREE_ATOMIC turn;
  unsigned char data[MPMC_LOCKFREE_SLOT];
};

typedef struct mpmc_lockfree_queue_t mpmc_lockfree_queue_t;
struct mpmc_lockfree_queue_t {
  MPMC_LOCKFREE_CACHE_ALIGNED MPMC_LOCKFREE_ATOMIC head;
  MPMC_LOCKFREE_CACHE_ALIGNED MPMC_LOCKFREE_ATOMIC tail;
  MPMC_LOCKFREE_CACHE_ALIGNED mpmc_lockfree_slot_t slots[MPMC_LOCKFREE_SLOTS_COUNT];
};

MPMC_LOCKFREE_API void 
mpmc_lockfree_enqueue(mpmc_lockfree_queue_t *queue, void *item) {
  __mpmc_int64 head = ATOMIC_FETCH_ADD(&queue->head, 1, MPMC_LOCKFREE_MO_ACQ_REL);
  mpmc_lockfree_slot_t *slot = &queue->slots[head % MPMC_LOCKFREE_SLOTS_COUNT];
  while ((head / MPMC_LOCKFREE_SLOTS_COUNT) * 2 != ATOMIC_LOAD(&slot->turn, MPMC_LOCKFREE_MO_ACQUIRE)) {
    // Busy loop.
  }
  memcpy(slot->data, item, MPMC_LOCKFREE_SLOT);
  ATOMIC_STORE(&slot->turn, (head / MPMC_LOCKFREE_SLOTS_COUNT) * 2 + 1, MPMC_LOCKFREE_MO_RELEASE);
}

MPMC_LOCKFREE_API void
mpmc_lockfree_dequeue(mpmc_lockfree_queue_t *queue, void *item) {
  __mpmc_int64 tail = ATOMIC_FETCH_ADD(&queue->tail, 1, MPMC_LOCKFREE_MO_ACQ_REL);
  mpmc_lockfree_slot_t *slot = &queue->slots[tail % MPMC_LOCKFREE_SLOTS_COUNT];
  while ((tail / MPMC_LOCKFREE_SLOTS_COUNT) * 2 + 1 != ATOMIC_LOAD(&slot->turn, MPMC_LOCKFREE_MO_ACQUIRE)) { 
    // Busy loop.
  }
  memcpy(item, slot->data, MPMC_LOCKFREE_SLOT);
  ATOMIC_STORE(&slot->turn, (tail / MPMC_LOCKFREE_SLOTS_COUNT) * 2 + 2, MPMC_LOCKFREE_MO_RELEASE);
}

MPMC_LOCKFREE_API int 
mpmc_lockfree_try_enqueue(mpmc_lockfree_queue_t *queue, void *item) {
  __mpmc_int64 head = ATOMIC_LOAD(&queue->head, MPMC_LOCKFREE_MO_ACQUIRE);
  while (1) {
    mpmc_lockfree_slot_t *slot = &queue->slots[head % MPMC_LOCKFREE_SLOTS_COUNT];
    __mpmc_int64 cycle = (head / MPMC_LOCKFREE_SLOTS_COUNT) * 2;
    if (cycle == ATOMIC_LOAD(&slot->turn, MPMC_LOCKFREE_MO_ACQUIRE)) {
      __mpmc_int64 desired = head + 1;
      __mpmc_int64 expected = head;
      if (ATOMIC_COMPARE_EXCHANGE_STRONG(&queue->head, &expected, desired, MPMC_LOCKFREE_MO_ACQ_REL, MPMC_LOCKFREE_MO_ACQUIRE)) {
        memcpy(slot->data, item, MPMC_LOCKFREE_SLOT);
        ATOMIC_STORE(&slot->turn, cycle + 1, MPMC_LOCKFREE_MO_RELEASE);
        return 1;
      }
      head = expected;
      continue;
    }
    __mpmc_int64 prev_head = head;
    head = ATOMIC_LOAD(&queue->head, MPMC_LOCKFREE_MO_ACQUIRE);
    if (head == prev_head) {
      return 0;
    }
  }
}

MPMC_LOCKFREE_API int
mpmc_lockfree_try_dequeue(mpmc_lockfree_queue_t *queue, void *item) {
  __mpmc_int64 tail = ATOMIC_LOAD(&queue->tail, MPMC_LOCKFREE_MO_ACQUIRE);
  while (1) {
    mpmc_lockfree_slot_t *slot = &queue->slots[tail % MPMC_LOCKFREE_SLOTS_COUNT];
    if ((tail / MPMC_LOCKFREE_SLOTS_COUNT) * 2 + 1 == ATOMIC_LOAD(&slot->turn, MPMC_LOCKFREE_MO_ACQUIRE)) {
      __mpmc_int64 desired = tail + 1;
      __mpmc_int64 expected = tail;
      if (ATOMIC_COMPARE_EXCHANGE_STRONG(&queue->tail, &expected, desired, MPMC_LOCKFREE_MO_ACQ_REL, MPMC_LOCKFREE_MO_ACQUIRE)) {
        memcpy(item, slot->data, MPMC_LOCKFREE_SLOT);
        ATOMIC_STORE(&slot->turn, (tail / MPMC_LOCKFREE_SLOTS_COUNT) * 2 + 2, MPMC_LOCKFREE_MO_RELEASE);
        return 1;
      } else {
        tail = expected;
      }
    } else {
      __mpmc_int64 prev_tail = tail;
      tail = ATOMIC_LOAD(&queue->tail, MPMC_LOCKFREE_MO_ACQUIRE);
      if (tail == prev_tail) {
        return 0;
      }
    }
  }
}
