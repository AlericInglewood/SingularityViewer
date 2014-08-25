/**
 * @file aimultigridbackendaccess.h
 * @brief Manage the AIMultiGrid database locking.
 *
 * Copyright (c) 2014, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   14/02/2014
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIMULTIGRIDBACKENDACCESS_H
#define AIMULTIGRIDBACKENDACCESS_H

#include "llsingleton.h"
#include "llerror.h"
#include "aithreadsafe.h"
#include <boost/intrusive_ptr.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include "aistatemachine.h"
#include "aicondition.h"

namespace AIMultiGrid {

class LockedBackEnd;
class ScopedBlockingBackEndAccess;

// class DatabaseFileLockSingleton
//
// Manages the file lock of the 'uploads' database.
// The class is thread-safe. If any thread obtains the
// the file lock then the whole process is allowed
// to use the database: any thread will be allowed
// to obtain a thread-lock (see DatabaseThreadLockSingleton).
//
// The class is simply reference counting the number
// of DatabaseFileLock objects and obtains the file lock
// upon the first DatabaseFileLock created and releases it
// when the last one is destructed.
//
// To address the global destruction order fiasco, the
// destructor makes sure the file lock is released after
// which additional destructions of DatabaseFileLock
// objects will have no effect. No DatabaseFileLock
// should be created after main() because that would
// result in undefined behavior and possibly in leaving
// the database locked.

class DatabaseFileLockSingleton : public LLSingleton<DatabaseFileLockSingleton>
{
  private:
    LOG_CLASS(DatabaseFileLockSingleton);

    struct Data {
      int mRefCount;                            // The number of DatabaseFileLock objects that currently are in use.
      boost::interprocess::file_lock mFLock;    // The file lock. Note that this, too, must be protected by a mutex
                                                // (mostly for POSIX which does not guarantee thread synchronization).
                                                // The boost documentation advises to use the same thread to lock
                                                // and unlock a file-- but that is too restrictive imho.
    };

    AIThreadSafeSimpleDC<Data> mData;
    typedef AIAccess<Data> data_wat;

  protected:
    friend class LLSingleton<DatabaseFileLockSingleton>;

    DatabaseFileLockSingleton(void);
    ~DatabaseFileLockSingleton();

    friend void intrusive_ptr_add_ref(DatabaseFileLockSingleton* p);
    friend void intrusive_ptr_release(DatabaseFileLockSingleton* p);

  public:
    // Initialize mFlock.
    void createFileLock(void);
};

// class DatabaseFileLock
//
// A process that creates one or more DatabaseFileLock objects will have the
// file lock on the 'uploads' database.
//
// Creating multiple objects, by default- or copy constructor, increments the
// reference counter of DatabaseFileLockSingleton, guaranteeing that the lock
// is kept until the last DatabaseFileLock object is destructed.
//
// One needs to have a DatabaseFileLock object to create a DatabaseThreadLock,
// which will make a copy of the DatabaseFileLock, keeping the file lock alive.
//
// If obtaining the file lock fails (because another process (viewer) has it
// locked) then the constructor will throw, see intrusive_ptr_add_ref(DatabaseFileLockSingleton*).

class DatabaseFileLock
{
  private:
    boost::intrusive_ptr<DatabaseFileLockSingleton> mFileLock;

  public:
    DatabaseFileLock(void) : mFileLock(DatabaseFileLockSingleton::getInstance()) { }
};

// class DatabaseThreadLockSingleton
//
// Manages the thread lock of the 'uploads' database.
// Only one thread at a time may access the database; in order to
// obtain access it needs to have a DatabaseThreadLock state machine
// and lock it. Once locked a BackEndAccess object can be obtained
// from the DatabaseThreadLock object through which the database can
// be accessed.
//
// Following the same semantics as DatabaseFileLock, a thread is
// allowed to create multiple DatabaseThreadLock objects and use
// them to access the database. Hence, this class keeps a reference
// count of the number of DatabaseThreadLock objects that obtained
// access (all of which will be the same thread).

class DatabaseThreadLockSingleton : public LLSingleton<DatabaseThreadLockSingleton>
{
  private:
    LOG_CLASS(DatabaseThreadLockSingleton);

    LLMutex mMutex;                                 // Mutex protecting the database.

  public:
    struct DatabaseMutex
    {
      bool mLocked;                                 // Set when some thread grabbed mMutex and reset when mMutex was released.
      bool met(void) const { return !mLocked; }     // The condition is met when mLocked is false.
    };
    typedef AICondition<DatabaseMutex> Condition_t; // The condition type.
    typedef AIAccess<DatabaseMutex> Condition_wat;  // Write Access Type for DatabaseMutex.
    Condition_t mCondition;                         // Condition variable. It is signalled every time mLocked becomes false.

  protected:
    friend class LLSingleton<DatabaseThreadLockSingleton>;

    DatabaseThreadLockSingleton(void) { }

    friend void intrusive_ptr_add_ref(DatabaseThreadLockSingleton* p);
    friend void intrusive_ptr_release(DatabaseThreadLockSingleton* p);

    // The functions below are protected because they may only be accessed by DatabaseThreadLock.
    // DatabaseThreadLock may NOT access the rest of this class though.
    // The reason for this is to guarantee that the process also holds a DatabaseFileLock before
    // trying to obtain the thread lock.
    friend class BackEndAccess;
    // Returns a non-NULL pointer if the lock was obtained, or NULL if the mutex
    // is already locked by another thread. To unlock, just destruct the intrusive_ptr.
    boost::intrusive_ptr<DatabaseThreadLockSingleton> trylock(void);
    // Blocking version. Never returns a NULL pointer.
    boost::intrusive_ptr<DatabaseThreadLockSingleton> lock(void);
};

// class BackEndAccess
//
// Object for database access. Only use this when 'is_locked()' returns true.
// It is guaranteed to be usable (is_locked() returns true) when obtained from
// DatabaseThreadLock::back_end_access() after that state machine finished
// successfully, or after calling one of the blocking lock() methods.

class BackEndAccess
{
  private:
    DatabaseFileLock mFileLock;                                     // Kept to increment the reference count of DatabaseFileLockSingleton.
    boost::intrusive_ptr<DatabaseThreadLockSingleton> mThreadLock;  // Kept to increment the reference count of DatabaseThreadLockSingleton.

    // Only DatabaseThreadLock can create this object and set the lock.
    friend class DatabaseThreadLock;
    BackEndAccess(DatabaseFileLock const& file_lock) : mFileLock(file_lock) { }

    // Actually, it's also ok when LockedBackEnd wants to create a temporary BackEndAccess to access itself...
    // Note that this may only be automatic variables (ie, not a member of LockedBackEnd), or else the database would be locked forever.
    friend class LockedBackEnd;
    BackEndAccess(LockedBackEnd*) : mThreadLock(DatabaseThreadLockSingleton::getInstance()) { }

    // It's also ok for ScopedBlockingBackEndAccess, which is derived from BackEndAccess, to be constructed
    // from just a DatabaseFileLock - because the constructor of ScopedBlockingBackEndAccess calls lock().
    friend class ScopedBlockingBackEndAccess;

  public:
    bool trylock(void) { mThreadLock = DatabaseThreadLockSingleton::instance().trylock(); return mThreadLock;}
    bool is_locked(void) const { return mThreadLock; }
    void lock(void) { mThreadLock = DatabaseThreadLockSingleton::instance().lock(); }
    void unlock(void) { mThreadLock.reset(); }

    LockedBackEnd* operator->() const;

    // Accessor.
    DatabaseFileLock const& file_lock(void) const { return mFileLock; }
};

// class DatabaseThreadLock
//
// State machine that obtains a database lock.
//
// Usage:
//
// DatabaseFileLock file_lock;
// LLPointer<DatabaseThreadLock> thread_lock = new DatabaseThreadLock(file_lock);
// thread_lock->run(...);
//
// and then in the callback:
//
// BackEndAccess back_end = lock->back_end_access();
//
// For example, if we're inside another state machine with members:
//
//   DatabaseFileLock mFileLock;                    // Hold file lock for the lifetime of the state machine.
//   LLPointer<DatabaseThreadLock> mThreadLock;     // State machine to obtain database lock.
//
// where in initialize_impl it does:
//
//   // This test only make sense if the state machine might be reused.
//   if (!mThreadLock)
//   {
//     mThreadLock = new DatabaseThreadLock(mFileLock);
//   }
//   set_state(mThreadLock->is_locked() ? state2 : state1);
//
// and it needs access to the database, it could do (in multiplex_impl):
//
// case state1:
//   mThreadLock->run(this, state2);
//   idle();
//   break;
// case state2:
//   // use mThreadLock->back_end_access()
//
class DatabaseThreadLock : public AIStateMachine
{
  protected:
    // The base class of this state machine.
    typedef AIStateMachine direct_base_type;

    enum databasethreadlock_state_type {
        DatabaseThreadLock_trylock = direct_base_type::max_state
    };
  public:
    static state_type const max_state = DatabaseThreadLock_trylock + 1;    // One beyond the largest state.

  private:
    BackEndAccess mBackEndAccess;

  public:
    // The constructor demands passing a DatabaseFileLock to discourage that creation and
    // destruction of this object causes the file lock to be obtained and released as well;
    // one should create and keep a DatabaseFileLock during the whole operation (ie, uploading
    // of files, repairing the database, importing/exporting of objects, etc).
    DatabaseThreadLock(DatabaseFileLock const& file_lock) :
#ifdef CWDEBUG
        AIStateMachine(true),
#endif
        mBackEndAccess(file_lock) { }
    // Alternatively, pass a BackEndAccess object (which also has a DatabaseFileLock).
    DatabaseThreadLock(BackEndAccess const& back_end_access) :
#ifdef CWDEBUG
        AIStateMachine(true),
#endif
        mBackEndAccess(back_end_access) { }

    // Get access to the database.
    // May only be called when the database is successfully locked: from the call back of
    // the state machine, after a successful finish, or after calling DatabaseThreadLock::lock().
    BackEndAccess const& back_end_access(void) const
    {
      if (!mBackEndAccess.is_locked())
      {
        // This is a bug. Do not call this function except from the call back of the state machine after a successful finish.
        throw std::runtime_error("Trying to get database access without locking the database first!");
      }
      return mBackEndAccess;
    }
    BackEndAccess& back_end_access(void)
    {
      if (!mBackEndAccess.is_locked())
      {
        // This is a bug. Do not call this function except from the call back of the state machine after a successful finish.
        throw std::runtime_error("Trying to get database access without locking the database first!");
      }
      return mBackEndAccess;
    }

    // Direct access.
    bool trylock(void) { return mBackEndAccess.trylock(); }
    void unlock(void) { mBackEndAccess.unlock(); }
    bool is_locked(void) const { return mBackEndAccess.is_locked(); }

    // Blocking call.
    void lock(void) { mBackEndAccess.lock(); }

    // Accessor.
    DatabaseFileLock const& file_lock(void) const { return mBackEndAccess.file_lock(); }

  protected:
	// Return class name. Used for AIStateMachine profiling.
	/*virtual*/ char const* getName(void) const { return "DatabaseThreadLock"; }

	// Handle initializing the object.
	/*virtual*/ void initialize_impl(void);

	// Handle mRunState.
	/*virtual*/ void multiplex_impl(state_type run_state);

	// Handle aborting from current bs_multiplex state (the default AIStateMachine::abort_impl() does nothing).
	/*virtual*/ void abort_impl(void);

	// Handle cleaning up from initialization (or post abort) state (the default AIStateMachine::finish_impl() does nothing).
	/*virtual*/ void finish_impl(void);

	// Return human readable string for run_state.
	/*virtual*/ char const* state_str_impl(state_type run_state) const;
};

// Scoped, blocking, thread lock.
// Calls lock() upon creation and ulock() upon destruction.
class ScopedBlockingBackEndAccess : public BackEndAccess
{
  public:
    ScopedBlockingBackEndAccess(DatabaseThreadLock const& thread_lock) : BackEndAccess(thread_lock.file_lock()) { lock(); }
    ScopedBlockingBackEndAccess(BackEndAccess const& back_end_access) : BackEndAccess(back_end_access) { lock(); }
    ~ScopedBlockingBackEndAccess() { unlock(); }
};

} // namespace AIMultiGrid

#endif // AIMULTIGRIDBACKENDACCESS_H
