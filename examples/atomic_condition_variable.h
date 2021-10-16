#pragma once
#include <atomic>
#include <cassert>

/** Tiny condition variable that keeps a count of waiters.

The interface intentionally resembles std::condition_variable.

In addition to wait(), we also define wait_shared() and wait_update(),
to go with atomic_shared_mutex.

A straightforward implementation of wait_until() would require the
existence of std::atomic::wait_until().

We define the predicate is_waiting().

There is no explicit constructor or destructor.
The object is expected to be zero-initialized, so that
!is_waiting() will hold.

The implementation counts pending wait() requests, so that signal()
and broadcast() will only invoke notify_one() or notify_all() when
pending requests exist. */

#ifdef _WIN32
#elif __cplusplus >= 202002L
#else /* Emulate the C++20 primitives */
# include <climits>
# if defined __linux__
#  include <linux/futex.h>
#  include <unistd.h>
#  include <sys/syscall.h>
#  define FUTEX(op,n) \
   syscall(SYS_futex, this, FUTEX_ ## op ## _PRIVATE, n, nullptr, nullptr, 0)
# elif defined __OpenBSD__
#  include <sys/time.h>
#  include <sys/futex.h>
#  define FUTEX(op,n) \
   futex((volatile uint32_t*) this, FUTEX_ ## op, n, nullptr, nullptr)
# else
#  error "no C++20 nor futex support"
# endif
#endif

class atomic_condition_variable : private std::atomic<uint32_t>
{
#if defined _WIN32 || __cplusplus >= 202002L
  void wait(uint32_t old) const noexcept { atomic::wait(old); }
#else /* Emulate the C++20 primitives */
  void notify_one() noexcept { FUTEX(WAKE, 1); }
  void notify_all() noexcept { FUTEX(WAKE, INT_MAX); }
  void wait(uint32_t old) const noexcept { FUTEX(WAIT, old); }
#endif
public:
  template<class mutex> void wait(mutex &m)
  {
    const uint32_t val = fetch_add(1, std::memory_order_acquire);
    m.unlock();
    wait(1 + val);
    m.lock();
  }

  template<class mutex> void wait_shared(mutex &m)
  {
    const uint32_t val = fetch_add(1, std::memory_order_acquire);
    m.unlock_shared();
    wait(1 + val);
    m.lock_shared();
  }

  template<class mutex> void wait_update(mutex &m)
  {
    const uint32_t val = fetch_add(1, std::memory_order_acquire);
    m.unlock_update();
    wait(1 + val);
    m.lock_update();
  }

  bool is_waiting() const noexcept { return load(std::memory_order_acquire); }

  void signal() noexcept
  { if (exchange(0, std::memory_order_release)) notify_one(); }

  void broadcast() noexcept
  { if (exchange(0, std::memory_order_release)) notify_all(); }
};
