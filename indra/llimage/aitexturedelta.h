/**
 * @file aitexturedelta.h
 * @brief Declaration of class TextureDelta
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
 *   01/01/2014
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef TEXTUREDELTA_H
#define TEXTUREDELTA_H

#include <iosfwd>
#include "aimultigriddelta.h"

namespace AIMultiGrid {

//-----------------------------------------------------------------------------
// class TextureDelta

// The extra data in a J2C asset file that is not in the corresponding image source file.

class TextureDelta : public Delta
{
  protected:
	bool mLossless;			// Whether or not the conversion to j2c has been done lossless.

  public:
	TextureDelta(void);
	TextureDelta(bool lossless);
	TextureDelta(AIXMLElementParser const& parser);

	// Compare.
	bool equals(Delta const* delta) const;

	// Accessors.
	bool getLossless(void) const { return mLossless; }

	// Manipulators.
	void setLossless(bool lossless) { mLossless = lossless; }

	/*virtual*/ void toXML(std::ostream& os, int indentation) const;
	/*virtual*/ void update_hash(LLMD5& hash_out);
};

} // namespace AIMultiGrid

#endif // AITEXTUREDELTA_H

