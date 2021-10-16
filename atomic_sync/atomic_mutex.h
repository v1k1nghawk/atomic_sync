#pragma once
#include <atomic>
#include <cassert>

/** Tiny, non-recursive mutex that keeps a count of waiters.

The interface intentionally resembles std::mutex.
We do not define native_handle().

We define the predicates is_locked_or_waiting() and is_locked().

We define spin_lock(), which is like lock(), but with an initial spinloop.

There is no explicit constructor or destructor.
The object is expected to be zero-initialized, so that
!is_locked_or_waiting() will hold.

The implementation counts pending lock() requests, so that unlock()
will only invoke notify_one() when pending requests exist. */
class atomic_mutex : std::atomic<uint32_t>
{
  /** number of spin loops in spin_wait_and_lock() */
  static unsigned spin_rounds;
#if !defined _WIN32 && __cplusplus < 202002L /* Emulate the C++20 primitives */
  void notify_one() noexcept;
  inline void wait(uint32_t old) const noexcept;
#endif
  /** A flag identifying that the lock is being held */
  static constexpr uint32_t HOLDER = 1U << 31;
  /** Wait until the mutex has been acquired, with initial spinloop */
  void spin_wait_and_lock() noexcept;
  /** Wait until the mutex has been acquired */
  void wait_and_lock() noexcept;
public:
  /** @return whether the mutex is being held or waited for */
  bool is_locked_or_waiting() const
  { return load(std::memory_order_acquire) != 0; }
  /** @return whether the mutex is being held by any thread */
  bool is_locked() const
  { return (load(std::memory_order_acquire) & HOLDER) != 0; }

  /** @return whether the mutex was acquired */
  bool trylock() noexcept
  {
    uint32_t lk = 0;
    return compare_exchange_strong(lk, HOLDER + 1,
                                   std::memory_order_acquire,
                                   std::memory_order_relaxed);
  }

  void lock() noexcept { if (!trylock()) wait_and_lock(); }
  void spin_lock() noexcept { if (!trylock()) spin_wait_and_lock(); }
  void unlock() noexcept
  {
    const uint32_t lk = fetch_sub(HOLDER + 1, std::memory_order_release);
    if (lk != HOLDER + 1)
    {
      assert(lk & HOLDER);
      notify_one();
    }
  }
};

/** Like atomic_mutex, but with a spinloop in lock() */
class atomic_spin_mutex : public atomic_mutex
{
public:
  void lock() noexcept { spin_lock(); }
};
