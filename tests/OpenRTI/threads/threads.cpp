/* -*-c++-*- OpenRTI - Copyright (C) 2009-2011 Mathias Froehlich
 *
 * This file is part of OpenRTI.
 *
 * OpenRTI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * OpenRTI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenRTI.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cstdlib>
#include <iostream>
#include <sstream>

#include "Clock.h"
#include "Condition.h"
#include "Mutex.h"
#include "ScopeLock.h"
#include "ScopeUnlock.h"
#include "Thread.h"

namespace OpenRTI {

class ThreadTest : public Thread {
public:

  static bool exec()
  {
    ThreadTest testThread;
    if (!testThread.start())
      return false;
    testThread.wait();
    if (!testThread._threadWasRun)
      return false;
    return true;
  }

protected:
  ThreadTest() : _threadWasRun(false)
  { }
  virtual void run()
  { _threadWasRun = true; }
  bool _threadWasRun;
};

/// Long living thread that does never end. Make sure this does not crash
class DetachedTest : public Thread {
public:

  static bool exec()
  {
    SharedPtr<DetachedTest> testThread = new DetachedTest;
    if (!testThread->start())
      return false;
    return true;
  }

protected:
  virtual void run()
  {
    Clock::sleep(Clock::final());
  }
};

class AtomicTest : public Thread {
public:

  static bool exec()
  {
    AtomicTest threads[4];
    for (unsigned i = 0; i < sizeof(threads)/sizeof(threads[0]); ++i)
      threads[i].start();
    for (unsigned i = 0; i < sizeof(threads)/sizeof(threads[0]); ++i)
      threads[i].wait();
    return getCounter() == 0;
  }

protected:
  virtual void run()
  {
    Atomic& counter = getCounter();
    for (unsigned i = 0; i < 1000000; ++i) {
      ++counter;
      --counter;
    }
  }
  static Atomic& getCounter()
  {
    static Atomic counter;
    return counter;
  }
};

class MutexTest : public Thread {
public:

  static bool exec()
  {
    MutexTest threads[4];
    for (unsigned i = 0; i < sizeof(threads)/sizeof(threads[0]); ++i)
      threads[i].start();
    for (unsigned i = 0; i < sizeof(threads)/sizeof(threads[0]); ++i)
      threads[i].wait();
    LockedData& lockedData = getLockedData();
    return lockedData._count == lockedData._atomic;
  }

protected:
  struct LockedData {
    LockedData() : _count(0) {}
    void exec()
    {
      ++_atomic;
      ScopeLock scopeLock(_mutex);
      ++_count;
    }

    Atomic _atomic;
    Mutex _mutex;
    unsigned _count;
  };

  virtual void run()
  {
    LockedData& lockedData = getLockedData();
    for (unsigned i = 0; i < 1000000; ++i)
      lockedData.exec();
  }
  static LockedData& getLockedData()
  {
    static LockedData lockedData;
    return lockedData;
  }
};


class ConditionTest : public Thread {
public:

  static bool exec()
  {
    ConditionData ping, pong;
    ConditionTest testThread(ping, pong);
    testThread.start();

    ping.signal();
    pong.wait();

    Clock start = Clock::now();
    for (unsigned i = 0; i < 10000; ++i) {
      ping.signal();
      pong.wait();
    }
    Clock stop = Clock::now();

    std::cout << "Average thread conditon latency is: " << (stop - start).getNSec()*1e-9/10000 << std::endl;

    testThread.wait();
    return true;
  }

protected:
  virtual void run()
  {
    _ping.wait();
    _pong.signal();
    for (unsigned i = 0; i < 10000; ++i) {
      _ping.wait();
      _pong.signal();
    }
  }

  struct ConditionData {
    ConditionData() : _signaled(false) {}
    void signal()
    {
      ScopeLock scopeLock(_mutex);
      _signaled = true;
      _condition.signal();
    }
    void wait()
    {
      ScopeLock scopeLock(_mutex);
      while (!_signaled)
        _condition.wait(_mutex);
      _signaled = false;
    }

    Condition _condition;
    Mutex _mutex;
    bool _signaled;
  };

  ConditionTest(ConditionData& ping, ConditionData& pong) :
    _ping(ping),
    _pong(pong)
  { }

  ConditionData& _ping;
  ConditionData& _pong;
};

} // namespace OpenRTI

using namespace OpenRTI;

int
main(int argc, char* argv[])
{
  if (!ThreadTest::exec()) {
    std::cerr << "ThreadTest failed!" << std::endl;
    return EXIT_FAILURE;
  }
  if (!DetachedTest::exec()) {
    std::cerr << "DetachedTest failed!" << std::endl;
    return EXIT_FAILURE;
  }
  if (!AtomicTest::exec()) {
    std::cerr << "AtomicTest failed!" << std::endl;
    return EXIT_FAILURE;
  }
  if (!MutexTest::exec()) {
    std::cerr << "MutexTest failed!" << std::endl;
    return EXIT_FAILURE;
  }
  if (!ConditionTest::exec()) {
    std::cerr << "ConditionTest failed!" << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
