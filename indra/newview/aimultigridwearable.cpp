/**
 * @file aimultigridwearable.cpp
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
#include "llcontrol.h"
#include "llvoavatar.h"
#include "llagent.h"
#include "aimultigridwearable.h"
#include "aialert.h"
#include "aicommonuuid.h"
#include "aimultigridbackend.h"

namespace AIMultiGrid {

void Wearable::import(std::istream& stream)
{
  if (!importStream(stream, NULL))
  {
    THROW_ALERT("AIMultiGrid_Error_decoding_NAME_asset", AIArgs("[NAME]", LLTrans::getString("wearable")));
  }
}

void Wearable::import(unsigned char* buffer, size_t size)
{
  boost::iostreams::stream<boost::iostreams::basic_array_source<char> > stream((char*)buffer, size);
  import(stream);
}

void Wearable::calculateHash(LLMD5& asset_md5)
{
  S32 const buffer_size = 13 + 8 * mSavedVisualParamMap.size() + 4 * mSavedTEMap.size();
  std::vector<U8> v(buffer_size);
  LLDataPackerBinaryBuffer buffer(&v[0], buffer_size);

  buffer.packS32(mDefinitionVersion, "Wearable::mDefinitionVersion");
  buffer.packU8(mType, "Wearable::mType");
  buffer.packU32(mSavedVisualParamMap.size(), "Wearable::mSavedVisualParamMap.size()");
  for (param_map_t::iterator param = mSavedVisualParamMap.begin(); param != mSavedVisualParamMap.end(); ++param)
  {
    buffer.packS32(param->first, "Wearable param index");
    // To avoid floating round off errors having influence on the calculate hash, use a precision of 0.0001.
    buffer.packU32((U32)floor(10000.0 * param->second + 0.5), "Wearable param weight");
  }
  buffer.packU32(mSavedTEMap.size(), "Wearable::mSavedTEMap.size()");
  for (te_map_t::iterator te = mSavedTEMap.begin(); te != mSavedTEMap.end(); ++te)
  {
    buffer.packS32(te->first, "Wearable texture index");
    mBackEnd->updateHash(te->second->getID(), asset_md5);
  }
  llassert(buffer.getCurrentSize() == buffer_size);
  asset_md5.update(buffer.getBuffer(), buffer.getCurrentSize());
  asset_md5.finalize();
}

void Wearable::translate(void)
{
  for (te_map_t::iterator te = mSavedTEMap.begin(); te != mSavedTEMap.end(); ++te)
  {
    LLUUID uuid = te->second->getID();
    if (mBackEnd->translate(uuid))
    {
      te->second->setID(uuid);
    }
  }
}

LLUUID const Wearable::getDefaultTextureImageID(LLAvatarAppearanceDefines::ETextureIndex index) const
{
  LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::TextureEntry const* texture_dict = LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary::getInstance()->getTexture(index);
  std::string const& default_image_name = texture_dict->mDefaultImageName;
  if (default_image_name == "")
  {   
    return IMG_DEFAULT_AVATAR;
  }
  return LLUUID(gSavedSettings.getString(default_image_name));
}

} // namespace AIMultiGrid

