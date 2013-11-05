/**
 * @file aimultigridbackendaccess.cpp
 * @brief Multi grid support.
 *
 * Copyright (c) 2013, Aleric Inglewood.
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
 *   11/10/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "llviewerprecompiledheaders.h"
#include <cstdio>
#include "llapr.h"
#include "llapp.h"
#include "aifile.h"
#include "aimultigridbackendaccess.h"
#include "aimultigridbackend.h"
#include "aialert.h"

namespace AIMultiGrid {

//-----------------------------------------------------------------------------
// BackEndAccess

apr_thread_mutex_t* BackEndAccess::gMutex;
bool BackEndAccess::gLockObtained;

//static
void BackEndAccess::init(void)
{
  apr_thread_mutex_create(&gMutex, APR_THREAD_MUTEX_UNNESTED, LLAPRRootPool::get()());
}

BackEndAccess::BackEndAccess(void) : mMutex(NULL), mLockObtained(false)
{
  mPool.create();
  apr_thread_mutex_create(&mMutex, APR_THREAD_MUTEX_UNNESTED, mPool());
}

BackEndAccess::~BackEndAccess()
{
  unlock();
  apr_thread_mutex_destroy(mMutex);
}

bool BackEndAccess::lock(void)
{
  // Start of critical area of mLockObtained.
  LLScopedLock lock1(mMutex);
  if (mLockObtained)
  {
	// We already have the lock!
	llassert(gLockObtained);
	return false;
  }

  // Start of critical area of gLockObtained.
  {
	LLScopedLock lock2(gMutex);
	if (gLockObtained)
	{
	  // Some other BackEndAccess object has the lock, because we don't (that was checked with mLockObtained).
	  throw DatabaseLocked();
	}

	// Obtain exclusive lock on the database.
	try
	{
	  BackEnd::getInstance()->lock();
	}
	catch (...)
	{
	  // Lock is already taken by another viewer process.
	  throw DatabaseLocked();
	}

    gLockObtained = true;
  }
  mLockObtained = true;

  std::string lockfilename = BackEnd::getInstance()->getJournalFilename("lock");
  try
  {
	AIFile lockfile(lockfilename, "r+");		// throws if the can't be opened.
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
	BackEnd::getInstance()->mLocked.clear_memory_cache();
  }
  catch (AIAlert::Error const&)
  {
	llwarns << "Failed to open the 'uploads' database lock file \"" << lockfilename << "\"!" << llendl;
	// Just leave it at a warning... the lock is there and it's extremely unlikely this is needed to begin with.
  }
  return true;
}

void BackEndAccess::unlock(void)
{
  // Start of critical area of mLockObtained.
  LLScopedLock lock1(mMutex);
  if (!mLockObtained)
  {
	// This object never obtained the lock or already explicitly released it (this is probably called from the destructor).
	return;
  }

  // Start of critical area of gLockObtained.
  {
	LLScopedLock lock2(gMutex);
	llassert(gLockObtained);

	BackEnd::getInstance()->unlock();

	gLockObtained = false;
  }
  mLockObtained = false;
}

// The same as unlock(), but changes the database before unlocking.
void BackEndAccess::switch_path(std::string& base_folder_out, std::string const& base_folder_in)
{
  // Start of critical area of mLockObtained.
  LLScopedLock lock1(mMutex);
  // We must have the lock.
  llassert(mLockObtained);

  // Start of critical area of gLockObtained.
  {
	LLScopedLock lock2(gMutex);
	llassert(gLockObtained);

	// Unlock the file lock.
	BackEnd::getInstance()->unlock();
	// Set a new path.
	// Note: this is NOT thread-safe. Theoretically the access to whatever base_folder_out is pointing to (namely BackEnd::mBaseFolder)
	// should be wrapped in an AIThreadSafeDC (replacing gMutex) together with gLockObtained, and kept read locked for the duration
	// of operations that generate a database path and use it, while being write locked here. However, I chose not to do that because
	// in practise it is just not necessary. Basically, the user should not be uploading stuff while changing the database path :p
	base_folder_out = base_folder_in;
	// Create a file lock with the new path.
	BackEnd::getInstance()->createFileLock();

	gLockObtained = false;
  }
  mLockObtained = false;
}

// The caller of these functions is responsible for first calling lock().
// It is also responsible for making sure that two threads never access the same BackEndAccess object at the same time,
// which is not enforced with an AIThreadSafe wrapper around BackEndAccess. Instead BackEndAccess should be
// member of an AIStateMachine that makes sure that only one thread at a time access the object.

LockedBackEnd* BackEndAccess::operator->()
{
  // If this fails then you forgot to call BackEndAccess::lock().
  llassert(mLockObtained);
  return &BackEnd::getInstance()->mLocked;
}

LockedBackEnd const* BackEndAccess::operator->() const
{
  // If this fails then you forgot to call BackEndAccess::lock().
  llassert(mLockObtained);
  return &BackEnd::getInstance()->mLocked;
}

} // namespace AIMultiGrid

