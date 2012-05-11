/**
 * @file aicurleasyrequeststatemachine.cpp
 * @brief Implementation of AICurlEasyRequestStateMachine
 *
 * Copyright (c) 2012, Aleric Inglewood.
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
 *   06/05/2012
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "linden_common.h"
#include "aicurleasyrequeststatemachine.h"

enum curleasyrequeststatemachine_state_type {
  AICurlEasyRequestStateMachine_checkFolderExists = AIStateMachine::max_state,
  AICurlEasyRequestStateMachine_fetchDescendents,
  AICurlEasyRequestStateMachine_folderCompleted
};

char const* AICurlEasyRequestStateMachine::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
	AI_CASE_RETURN(AICurlEasyRequestStateMachine_checkFolderExists);
	AI_CASE_RETURN(AICurlEasyRequestStateMachine_fetchDescendents);
	AI_CASE_RETURN(AICurlEasyRequestStateMachine_folderCompleted);
  }
  return "UNKNOWN STATE";
}

void AICurlEasyRequestStateMachine::initialize_impl(void)
{
  llassert(AICurlEasyRequest_rat(*get())->is_finalized());	// Call finalizeRequest(url) before calling run().
  addRequest();
  set_state(AICurlEasyRequestStateMachine_folderCompleted);
}

void AICurlEasyRequestStateMachine::multiplex_impl(void)
{
  switch (mRunState)
  {
	case AICurlEasyRequestStateMachine_checkFolderExists:
	{
	}
	case AICurlEasyRequestStateMachine_fetchDescendents:
	{
	  break;
	}
	case AICurlEasyRequestStateMachine_folderCompleted:
	{
	  finish();
	  break;
	}
  }
}

void AICurlEasyRequestStateMachine::abort_impl(void)
{
}

void AICurlEasyRequestStateMachine::finish_impl(void)
{
  Dout(dc::curl, "AICurlEasyRequestStateMachine::finish_impl called for = " << (void*)get());
}
