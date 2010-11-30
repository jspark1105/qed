/*
 * qed_base.h
 *
 * Queue Enchaned With Dynamic sizing base classes.
 *
 *  Created on: Nov 29, 2010
 *      Author: jongsoo
 */

#ifndef _QED_BASE_H_
#define _QED_BASE_H_

#include <cstdarg>
#include <cstdlib>
#include <iostream>

namespace qed {

// Allocate at cache line boundaries.
template<class T>
T * const alignedMalloc(size_t n) {
  long addr = (long)malloc((n + 64)*sizeof(T));
  char *ptr = (char *)(((addr + 63) >> 5) << 5);
  return (T *)ptr;
}

template<class T>
T * const alignedCalloc(size_t n) {
  long addr = (long)calloc((n + 64), sizeof(T));
  char *ptr = (char *)(((addr + 63) >> 5) << 5);
  return (T *)ptr;
}

static const int TRACE_LENGTH = 65536*4;

static inline unsigned long long int readTsc(void)
{
  unsigned a, d;

  __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

  return ((unsigned long long)a) | (((unsigned long long)d) << 32);
}

enum EventId {
  SET_CAPACITY = 0,
  RESERVE_ENQUEUE,
  RESERVE_DEQUEUE,
  COMMIT_ENQUEUE,
  COMMIT_DEQUEUE,
  FULL,
  EMPTY,
};

struct Trace {
  unsigned long long tsc;
  EventId id;
  int value;
};

/*
 * The base class of our queue interface.
 */
template<class T>
class BaseQ {
public :
  BaseQ(size_t N) : N(N), buf(alignedMalloc<T>(N)) {
#if QED_TRACE_LEVEL > 0
    memset(traces, 0, sizeof(traces));
    traceIndex = 0;
#endif
#if QED_TRACE_LEVEL >= 2
    isSpinningFull = false;
    isSpinningEmpty = false;
#endif
  }

  T * const getBuf() {
    return buf;
  }

  size_t getCapacity() const {
    return N;
  }

  void dumpToCout() __attribute__((noinline)) {
    dump(std::cout);
  }

  void dumpToCoutHumanFriendy() __attribute__((noinline)) {
    dumpHumanFriendly(std::cout);
  }

#if QED_TRACE_LEVEL > 0
  void trace(EventId id, int value) {
    int i = __sync_fetch_and_add(&traceIndex, 1);
    Trace *t = traces + (i&(TRACE_LENGTH - 1));
    t->tsc = readTsc();
    t->id = id;
    t->value = value;
  }

  void traceResizing(size_t newCapacity) {
    trace(SET_CAPACITY, newCapacity);
  }

  void dump(std::ostream& out) __attribute__((noinline)) {
    if (traceIndex >= TRACE_LENGTH) {
      for (int i = traceIndex - TRACE_LENGTH; i < traceIndex; i++) {
        Trace *t = traces + (i&(TRACE_LENGTH - 1));
        out << t->tsc << " " << t->id << " " << t->value << std::endl;
      }
    }
    else {
      for (int i = 0; i < traceIndex; i++) {
        Trace *t = traces + i;
        out << t->tsc << " " << t->id << " " << t->value << std::endl;
      }
    }
  }

  void dumpHumanFriendly(std::ostream& out) __attribute__((noinline)) {
    int begin = 0, end = traceIndex;
    if (traceIndex >= TRACE_LENGTH) {
      begin = traceIndex - TRACE_LENGTH;
    }
    for (int i = begin; i < end; i++) {
      Trace *t = traces + (i&(TRACE_LENGTH - 1));
      out << t->tsc << " ";
      switch (t->id) {
      case SET_CAPACITY: 
        out << "setCapacity";
        break;
      case RESERVE_ENQUEUE :
        out << "reserveEnqueue";
        break;
      case RESERVE_DEQUEUE :
        out << "reserveDequeue";
        break;
      case COMMIT_ENQUEUE :
        out << "commitEnqueue";
        break;
      case COMMIT_DEQUEUE :
        out << "commitDequeue";
        break;
      case FULL :
        out << "full";
        break;
      case EMPTY :
        out << "empty";
        break;
      }
      
      out << " " << t->value << std::endl;
    }
  }
#else
  void traceResizing(size_t newCapacity) { }

  void trace(const char *fmt, ...) { }

  void dump(std::ostream& out) { }

  void dumpHumanFriendly(std::ostream& out) { }
#endif

#if QED_TRACE_LEVEL >= 2
  void traceReserveEnqueue(int tail) {
    trace(RESERVE_ENQUEUE, tail);
  }

  void traceReserveDequeue(int head) {
    trace(RESERVE_DEQUEUE, head);
  }

  void traceCommitEnqueue(int tail) {
    trace(COMMIT_ENQUEUE, tail);
  }

  void traceCommitDequeue(int head) {
    trace(COMMIT_DEQUEUE, head);
  }

  void traceFull() {
    if (!isSpinningFull) {
      isSpinningFull = true;
      trace(FULL, 0);
    }
  }

  void traceEmpty() {
    if (!isSpinningEmpty) {
      isSpinningEmpty = true;
      trace(EMPTY, 0);
    }
  }

  void traceFull(bool isFull) {
    if (isFull) {
      traceFull();
    }
    else {
      isSpinningFull = false;
    }
  }

  void traceEmpty(bool isEmpty) {
    if (isEmpty) {
      traceEmpty();
    }
    else {
      isSpinningEmpty = false;
    }
  }
#else
  void traceReserveEnqueue(int tail) { }
  void traceReserveDequeue(int head) { }
  void traceCommitEnqueue(int tail) { }
  void traceCommitDequeue(int head) { }
  void traceFull() { };
  void traceEmpty() { };
  void traceFull(bool isFull) { };
  void traceEmpty(bool isEmpty) { };
#endif

protected :
  const size_t N;
  T * const buf __attribute__((aligned (64)));

#if QED_TRACE_LEVEL > 0
  Trace traces[TRACE_LENGTH];
  volatile int traceIndex;
#endif
#if QED_TRACE_LEVEL >= 2
  volatile bool isSpinningFull, isSpinningEmpty;
#endif
};

#define QED_USING_BASEQ_MEMBERS_ \
  using BaseQ<T>::N; \
  using BaseQ<T>::traceReserveEnqueue; \
  using BaseQ<T>::traceReserveDequeue; \
  using BaseQ<T>::traceCommitEnqueue; \
  using BaseQ<T>::traceCommitDequeue; \
  using BaseQ<T>::traceFull; \
  using BaseQ<T>::traceEmpty;

#if QED_TRACE_LEVEL >= 2
#define QED_USING_BASEQ_MEMBERS \
  QED_USING_BASEQ_MEMBERS_ \
  using BaseQ<T>::isSpinningFull; \
  using BaseQ<T>::isSpinningEmpty;
#else  
#define QED_USING_BASEQ_MEMBERS QED_USING_BASEQ_MEMBERS_
#endif

} // namespace qed

#endif // _QED_BASE_H_
