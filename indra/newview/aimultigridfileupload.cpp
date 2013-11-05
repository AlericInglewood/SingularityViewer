/**
 * @file aimultigridfileupload.cpp
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
 *   04/09/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "llviewerprecompiledheaders.h"
#include "aimultigridfileupload.h"

// This statemachine replaces Linden Labs 'upload_new_resource'.
//
// Usage
//
// Where the old code called,
//
//   upload_new_resource(
//           src_filename,
//           name, desc, compression_info, destination_folder_type, inv_type, next_owner_perms, group_perms, everyone_perms, display_name,
//           udp_callback,				// void (*)(LLUUID const& asset_id, void* user_data, S32 status, LLExtStat ext_status)
//           expected_upload_cost,
//           userdata);
//
// You now would do,
//
//   LLPointer<AIMultiGrid::FileUpload> mg_file_upload = new AIMultiGrid::FileUpload;
//   mg_file_upload->(asset_type);
//   mg_file_upload->run();
//
// This causes a file picker to pop up to ask for the filename, and then a preview floater
// to get the name, desc and compression_info. The rest is derived from that.
// This state machine calls FrontEnd for the real work.

namespace AIMultiGrid {

//-----------------------------------------------------------------------------
// FileUpload

char const* FileUpload::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
	AI_CASE_RETURN(FileUpload_prepare);
	AI_CASE_RETURN(FileUpload_done);
  }
  llassert(false);
  return "UNKNOWN STATE";
}

void FileUpload::initialize_impl(void)
{
  set_state(FileUpload_prepare);
}

void FileUpload::multiplex_impl(state_type run_state)
{
  switch (run_state)
  {
	case FileUpload_prepare:
	{
	  break;
	}
	case FileUpload_done:
	{
	  finish();
	  break;
	}
  }
}

void FileUpload::abort_impl(void)
{
}

void FileUpload::finish_impl(void)
{
}

} // namespace AIMultiGrid

