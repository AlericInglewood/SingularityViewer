/**
 * @file aitexturedelta.cpp
 * @brief Implementation of class TextureDelta
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

#include "linden_common.h"
#include "aitexturedelta.h"
#include "aixml.h"
#include <iostream>
#include "llmd5.h"

namespace AIMultiGrid {

TextureDelta::TextureDelta(void) : mLossless(false)
{
}

TextureDelta::TextureDelta(bool lossless) : mLossless(lossless)
{
}

bool TextureDelta::equals(Delta const* delta) const
{
  llassert(dynamic_cast<TextureDelta const*>(delta) != NULL);
  return mLossless == static_cast<TextureDelta const*>(delta)->mLossless;
}

// This produces the same hash value for two TextureDelta objects when TextureDelta::equals returns zero.
void TextureDelta::update_hash(LLMD5& hash_out)
{
  U8 lossless = mLossless ? 1 : 0;
  hash_out.update(&lossless, 1);
}

void TextureDelta::toXML(std::ostream& os, int indentation) const
{
  AIXMLElement tag(os, "texturedelta", indentation);
  tag.child("lossless", mLossless);
}

TextureDelta::TextureDelta(AIXMLElementParser const& parser)
{
  parser.child("lossless", mLossless);
}

} // namespace AIMultiGrid
