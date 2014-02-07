/**
 * @file aimultigridnotecard.cpp
 * @brief Multi grid support.
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
 *   02/02/2014
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "llviewerprecompiledheaders.h"
#include <boost/iostreams/stream.hpp>
#include "lltrans.h"
#include "aimultigridnotecard.h"
#include "aimultigridbackend.h"
#include "aialert.h"
#include "llmd5.h"

namespace AIMultiGrid {

void Notecard::import(std::istream& stream)
{
  if (!importStream(stream))
  {
    THROW_ALERT("AIMultiGrid_Error_decoding_NAME_asset", AIArgs("[NAME]", LLTrans::getString("notecard")));
  }
}

void Notecard::import(unsigned char* buffer, size_t size)
{
  boost::iostreams::stream<boost::iostreams::basic_array_source<char> > stream((char*)buffer, size);
  import(stream);
}

void Notecard::calculateHash(LLMD5& asset_md5) const
{
  S32 const buffer_size = sizeof(S32);
  U8 v[buffer_size];
  LLDataPackerBinaryBuffer buffer(v, buffer_size);
  buffer.packS32(mVersion, "Notecard::mVersion");
  asset_md5.update(v, buffer_size);
  for (std::vector<LLPointer<LLInventoryItem> >::const_iterator item = mItems.begin(); item != mItems.end(); ++item)
  {
    mBackEnd->updateHash((*item)->getAssetUUID(), asset_md5);
  }
  asset_md5.update(mText);
  asset_md5.finalize();
}

void Notecard::translate(void)
{
  for (std::vector<LLPointer<LLInventoryItem> >::iterator item = mItems.begin(); item != mItems.end(); ++item)
  {
    LLUUID uuid = (*item)->getAssetUUID();
    if (mBackEnd->translate(uuid))
    {
      (*item)->setAssetUUID(uuid);
    }
  }
}

} // namespace AIMultiGrid

