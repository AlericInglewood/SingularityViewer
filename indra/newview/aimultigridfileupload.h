/**
 * @file aimultigridfileupload.h
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
 *   04/09/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIMULTIGRIDFILEUPLOAD_H
#define AIMULTIGRIDFILEUPLOAD_H

#include "aistatemachine.h"

namespace AIMultiGrid {

class FileUpload : public AIStateMachine
{
  protected:
	typedef AIStateMachine direct_base_type;

	enum file_upload_state_type {
	  FileUpload_prepare = direct_base_type::max_state,
	  FileUpload_done
	};
  public:
	static state_type const max_state = FileUpload_done + 1;

  public:
	FileUpload(void);

  protected:
	/*virtual*/ ~FileUpload() { }

  protected:
	/*virtual*/ void initialize_impl(void);
	/*virtual*/ void multiplex_impl(state_type run_state);
	/*virtual*/ void abort_impl(void);
	/*virtual*/ void finish_impl(void);

	/*virtual*/ char const* state_str_impl(state_type run_state) const;
};

} // namespace AIMultiGrid

#endif // AIMULTIGRIDFILEUPLOAD_H

