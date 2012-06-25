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
  AICurlEasyRequestStateMachine_addRequest = AIStateMachine::max_state,
  AICurlEasyRequestStateMachine_waitAdded,
  AICurlEasyRequestStateMachine_waitDone,
  AICurlEasyRequestStateMachine_finished
};

char const* AICurlEasyRequestStateMachine::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
	AI_CASE_RETURN(AICurlEasyRequestStateMachine_addRequest);
	AI_CASE_RETURN(AICurlEasyRequestStateMachine_waitAdded);
	AI_CASE_RETURN(AICurlEasyRequestStateMachine_waitDone);
	AI_CASE_RETURN(AICurlEasyRequestStateMachine_finished);
  }
  return "UNKNOWN STATE";
}

void AICurlEasyRequestStateMachine::initialize_impl(void)
{
  {
	AICurlEasyRequest_wat curlEasyRequest_w(*mCurlEasyRequest);
	llassert(curlEasyRequest_w->is_finalized());	// Call finalizeRequest(url) before calling run().
	curlEasyRequest_w->send_events_to(this);
  }
  set_state(AICurlEasyRequestStateMachine_addRequest);
}

void AICurlEasyRequestStateMachine::added_to_multi_handle(AICurlEasyRequest_wat&)
{
  set_state(AICurlEasyRequestStateMachine_waitDone);
}

void AICurlEasyRequestStateMachine::finished(AICurlEasyRequest_wat&)
{
  set_state(AICurlEasyRequestStateMachine_finished);
}

void AICurlEasyRequestStateMachine::removed_from_multi_handle(AICurlEasyRequest_wat&)
{
}

void AICurlEasyRequestStateMachine::multiplex_impl(void)
{
  switch (mRunState)
  {
	case AICurlEasyRequestStateMachine_addRequest:
	{
	  mCurlEasyRequest.addRequest();
	  set_state(AICurlEasyRequestStateMachine_waitAdded);
	}
	case AICurlEasyRequestStateMachine_waitAdded:
	{
	  idle();			// Wait till added_to_multi_handle() is called.
	  break;
	}
	case AICurlEasyRequestStateMachine_waitDone:
	{
	  idle();			// Wait till done() is called.
	  break;
	}
	case AICurlEasyRequestStateMachine_finished:
	{
	  finish();
	  break;
	}
  }
}

void AICurlEasyRequestStateMachine::abort_impl(void)
{
  AICurlEasyRequest_wat(*mCurlEasyRequest)->send_events_to(NULL);
}

void AICurlEasyRequestStateMachine::finish_impl(void)
{
  Dout(dc::curl, "AICurlEasyRequestStateMachine::finish_impl called for = " << (void*)mCurlEasyRequest.get());
}

AICurlEasyRequestStateMachine::~AICurlEasyRequestStateMachine()
{
  Dout(dc::statemachine, "Calling ~AICurlEasyRequestStateMachine() [" << (void*)this << "]");
  switch (mRunState)
  {
	case AICurlEasyRequestStateMachine_waitAdded:
	case AICurlEasyRequestStateMachine_waitDone:
	case AICurlEasyRequestStateMachine_finished:
	  mCurlEasyRequest.removeRequest();
	  break;
	default:
	  break;
  }
}

