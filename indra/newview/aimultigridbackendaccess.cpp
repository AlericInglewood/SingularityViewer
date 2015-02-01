/**
 * @file aimultigridbackendaccess.cpp
 * @brief Implementation of Database*Lock* and BackEndAccess.
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

#include "linden_common.h"
#include "aimultigridbackendaccess.h"
#include "aialert.h"
#include "aimultigridbackend.h"
#include "aifile.h"

namespace AIMultiGrid {

DatabaseFileLockSingleton::DatabaseFileLockSingleton(void)
{
  data_wat data_w(mData);
  data_w->mRefCount = 0;
}

DatabaseFileLockSingleton::~DatabaseFileLockSingleton()
{
  data_wat data_w(mData);
  if (data_w->mRefCount > 0)
  {
    data_w->mFLock.unlock();
  }
  // Make sure that additional calls to intrusive_ptr_release will not call unlock again.
  data_w->mRefCount = 0;
}

void intrusive_ptr_add_ref(DatabaseFileLockSingleton* p)
{
  bool need_clear_memory_cache = false;
  {
    DatabaseFileLockSingleton::data_wat data_w(p->mData);
    if (data_w->mRefCount++ == 0)
    {
      if (!data_w->mFLock.try_lock())
      {
        data_w->mRefCount = 0;
        // If another process has the file lock, then we don't block but
        // instead throw an error. Users shouldn't try to upload and/or
        // import/export with two viewers at the same time.
        // Note that throwing aborts the constructor (of DatabaseFileLock)
        // causing its destructor not to be called, and therefore automatically
        // guarantees that the corresponding intrusive_ptr_release won't
        // be called.
        THROW_MALERT("AIMultiGridDatabaseLocked");
      }

      std::string lockfilename = BackEnd::instance().getJournalFilename("lock");
      try
      {
        AIFile lockfile(lockfilename, "r+");        // Throws if the file can't be opened.
        U32 pid = LLApp::getPid();
        U32 lastpid;
        // Read the PID of the last process that locked the database.
        if (!(fread(&lastpid, sizeof(lastpid), 1, lockfile) == 1 && lastpid == pid))
        {
            // Write our PID to the file if it wasn't already in there.
            rewind(lockfile);
            if (fwrite(&pid, sizeof(pid), 1, lockfile) != 1)
            {
                llwarns << "Could not write PID to the 'uploads' database lock file \"" << lockfilename << "\"!" << llendl;
            }
            fflush(lockfile);
        }
        // Flush the in-memory caches of the database, because they cannot be trusted anymore.
        need_clear_memory_cache = true;
      }
      catch (AIAlert::Error const&)
      {
        llwarns << "Failed to open the 'uploads' database lock file \"" << lockfilename << "\"!" << llendl;
        // Just leave it at a warning... the lock is there and it's extremely unlikely this is needed to begin with.
      }
    }
  }
  if (need_clear_memory_cache)
  {
    DatabaseFileLock file_lock;
    DatabaseThreadLock thread_lock(file_lock);
    ScopedBlockingBackEndAccess back_end_access(thread_lock);
    back_end_access->clear_memory_cache();
  }
}

void intrusive_ptr_release(DatabaseFileLockSingleton* p)
{
  DatabaseFileLockSingleton::data_wat data_w(p->mData);
  if (--data_w->mRefCount == 0)
  {
    data_w->mFLock.unlock();
  }
}

void DatabaseFileLockSingleton::createFileLock(void)
{
  bool success = false;
  do
  {
    std::string lockfilename = BackEnd::instance().getJournalFilename("lock");
    try
    {
      data_wat data_w(mData);
      // Open the file lock (this does not lock it).
      boost::interprocess::file_lock flock(lockfilename.c_str());
      // Transfer ownership to us.
      data_w->mFLock.swap(flock);
      success = true;
    }
    catch (boost::interprocess::interprocess_exception& error)
    {
      if (error.get_error_code() != boost::interprocess::not_found_error)
      {
        llerrs << "Failed to open 'uploads' database lock file \"" << lockfilename << "\"." << llendl;
        throw std::runtime_error("Failed to open 'uploads' database lock file.");
      }
      // File doesn't exist, create it and try again (this throws if it fails).
      AIFile lockfile(lockfilename, "wb");
    }
  }
  while (!success);
}

void intrusive_ptr_add_ref(DatabaseThreadLockSingleton* p)
{
  p->mMutex.lock();
  DatabaseThreadLockSingleton::Condition_wat(p->mCondition)->mLocked = true;
}

void intrusive_ptr_release(DatabaseThreadLockSingleton* p)
{
  // First lock mCondition, otherwise another thread might grab the mutex
  // and set mLocked to true - only to be overwritten below and set to
  // false again - which would be wrong.
  DatabaseThreadLockSingleton::Condition_wat condition_w(p->mCondition);
  p->mMutex.unlock();
  if (!p->mMutex.isSelfLocked())
  {
    // The mutex was just released.
    // It might have been grabbed by another thread in the meantime, in the
    // first line of intrusive_ptr_add_ref above, but that thread is then
    // hanging on the second line and didn't set mLocked to true yet, because
    // we have the lock on mCondition.
    //
    // Set mLocked to false and signal the condition.
    condition_w->mLocked = false;
    // This might unblock a state machine (see DatabaseThreadLock::multiplex_impl below),
    // which theoretically could immediately start running if it isn't running in this
    // thread, and then call trylock on the mutex. If that succeeded it would call
    // intrusive_ptr_add_ref and hang there until we leave this scope. Otherwise, if
    // the trylock fails because a third thread obtained the mutex (and is hanging in
    // intrusive_ptr_add_ref above) the state machine would also hang, on trying to get
    // the Condition_wat - until we leave scope.
    // Then, once we leave scope there are two possibilities:
    // 1) the state machine grabs the condition, tests if the condition is met, which
    // it is (ignoring a possible fourth thread), causing it to call trylock again,
    // which would fail again and so on until
    // 2) the third thread grabs the condition lock and sets mLocked to true.
    // Now the state machine sees that the condition is not met anymore, and adds
    // itself back to the queue.
    p->mCondition.broadcast();
  }
}

boost::intrusive_ptr<DatabaseThreadLockSingleton> DatabaseThreadLockSingleton::trylock(void)
{
  boost::intrusive_ptr<DatabaseThreadLockSingleton> res;
  if (mMutex.try_lock())
  {
    res = DatabaseThreadLockSingleton::getInstance();
    mMutex.unlock();    // This never releases the mutex, since the previous line caused lock() to be called, incrementing LLMutexBase::mCount.
  }
  return res;
}

boost::intrusive_ptr<DatabaseThreadLockSingleton> DatabaseThreadLockSingleton::lock(void)
{
  return DatabaseThreadLockSingleton::getInstance();
}

LockedBackEnd* BackEndAccess::operator->() const
{
  return &BackEnd::getInstance()->mLocked;
}

char const* DatabaseThreadLock::state_str_impl(state_type run_state) const
{
  switch (run_state)
  {
    AI_CASE_RETURN(DatabaseThreadLock_trylock);
  }
  llassert(false);
  return "UNKNOWN STATE";
}

void DatabaseThreadLock::initialize_impl(void)
{
  set_state(DatabaseThreadLock_trylock);
}

void DatabaseThreadLock::multiplex_impl(state_type run_state)
{
  switch (run_state)
  {
    case DatabaseThreadLock_trylock:
    {
      DatabaseThreadLockSingleton::Condition_t& condition(DatabaseThreadLockSingleton::instance().mCondition);
      int count = 0;
      // This causes DatabaseThreadLockSingleton::trylock to be called which, if mMutex.try_lock() succeeds,
      // causes intrusive_ptr_add_ref(DatabaseThreadLockSingleton*) to be called which causes mMutex to be
      // locked recursively a second time (which never blocks because we already hold the lock).
      // Subsequently mMutex is unlocked once (still leaving it locked).
      bool lock_obtained;
      while (!(lock_obtained = mBackEndAccess.trylock()))
      {
        // We failed to obtain the mutex, which means that if the condition is not met then another thread
        // (still) holds the mutex and we can safely add ourselves to the queue of the condition.
        // If the condition is met however, then it is possible that the mutex was unlocked right after our
        // call to trylock, and we have to call trylock again because it is not safe to add ourselves to
        // the queue.
        DatabaseThreadLockSingleton::Condition_wat condition_w(condition);
        if (!condition_w->met())
        {
          // Register this state machine to be woken up when the mutex becomes available.
          wait(condition);
          break;
        }
        // The condition was met: no thread is holding the mutex, or another thread is
        // hanging in intrusive_ptr_add_ref while trying to set mLocked to true.
        //
        // Try to lock the mutex again. However, if the second attempt fails too then the
        // most likely scenario is that another thread obtained the lock and is hanging in
        // intrusive_ptr_add_ref (see the comment in intrusive_ptr_release) and it makes
        // no sense to try again. Just yield to give the other thread the chance to run.
        if (++count == 2)
        {
          yield();
          break;
        }
      }
      if (lock_obtained)
      {
        // Locking succeeded.
        finish();
      }
      break;
    }
  }
}

void DatabaseThreadLock::abort_impl(void)
{
}

void DatabaseThreadLock::finish_impl(void)
{
}

} // namespace AIMultiGrid

