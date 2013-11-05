/**
 * @file aimultigriddelta.h
 * @brief Declaration of class Delta
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
 *   02/10/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIMULTIGRIDDELTA_H
#define AIMULTIGRIDDELTA_H

#include "llrefcount.h"
#include <iosfwd>

class AIXMLElementParser;
class LLMD5;

namespace AIMultiGrid {

//-----------------------------------------------------------------------------
// class Delta

// Base class of classes defining extra data in an asset file that is not in the corresponding source file.

class Delta : public LLRefCount
{
  public:
	virtual ~Delta() { }

  public:
	virtual void toXML(std::ostream& os, int indentation) const = 0;
	virtual void update_hash(LLMD5& hash_out) = 0;
};

} // namespace AIMultiGrid

#endif // AIMULTIGRIDDELTA_H

