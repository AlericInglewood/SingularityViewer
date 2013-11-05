/**
 * @file aimultigridbackendaccess.h
 * @brief Multi grid support API.
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

#ifndef AIMULTIGRIDBACKENDACCESS_H
#define AIMULTIGRIDBACKENDACCESS_H

#include "llaprpool.h"
#include <exception>

namespace AIMultiGrid {

class LockedBackEnd;

class DatabaseLocked : public std::exception {
};

// class BackEndAccess
//
// This class provides a locking mechanism for the (singleton) BackEnd.
// A lock is obtained by calling lock() and freed by calling unlock().
// Calling lock() on an already locked object does nothing.
// Destruction of the object automatically calls unlock().
// Using the operator->()s automatically locks the object (they call lock()).
//
// If a call to lock() fails, then an exception is thrown of type DatabaseLocked.

class BackEndAccess {
  private:
	LLAPRPool mPool;							// APR memory pool with a life time equal to the life time of this object.
	apr_thread_mutex_t* mMutex;					// APR mutex (uses mPool) that protects the members of this object.
	bool mLockObtained;							// If this is set then the lock was obtained by this object. If unset it is only unlikely that it was obtained (but possible).

	static apr_thread_mutex_t* gMutex;			// APR mutex that protects gLockObtained.
	static bool gLockObtained;					// If this is set then the lock was obtained by this process. If unset it is only unlikely that it was obtained (but possible).

  public:
	static void init(void);						// One time initialization needed for gMutex.

	BackEndAccess(void);
	~BackEndAccess();

	bool lock(void);							// Lock the database - returns true if it was unlocked before. Throws if it is locked by someone else.
	void unlock(void);

	// Should only be called by BackEnd::switch_path.
	void switch_path(std::string& base_folder_out, std::string const& base_folder_in);

	LockedBackEnd* operator->();
	LockedBackEnd const* operator->() const;
};

} // namespace AIMultiGrid

#endif // AIMULTIGRIDBACKENDACCESS_H

