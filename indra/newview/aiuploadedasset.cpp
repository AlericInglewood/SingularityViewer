/**
 * @file aiuploadedasset.cpp
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
 *   24/07/2013
 *   Initial version, written by Aleric Inglewood @ SL
 *
 *   03/09/2013
 *   Created aiuploadedasset.cpp
 */

#include "llviewerprecompiledheaders.h"
#include "aiuploadedasset.h"
#include "aixml.h"
#include "aibvhanimdelta.h"
#include "hippogridmanager.h"

namespace AIMultiGrid {

//-----------------------------------------------------------------------------
// GridUUID

void GridUUID::toXML(std::ostream& os, int indentation) const
{
  AIXMLElement tag(os, "grid", indentation);
  tag.attribute("nick", mGrid);
  tag.child(mUUID);
}

GridUUID::GridUUID(AIXMLElementParser const& parser)
{
  parser.attribute("nick", mGrid.mGridNick);
  if (mGrid.mGridNick.empty())
  {
	THROW_ALERT("GridUUID_Missing_or_invalid_nick_attribute_in_grid_node");
  }
  parser.child(mUUID);
}

//-----------------------------------------------------------------------------
// UploadedAsset

void UploadedAsset::setSource(std::string const& filename, LLDate const& date, LLMD5 const& sourceMd5)
{
  mFilename = filename;
  mDate = date;
  mSourceMd5 = sourceMd5;
}

void UploadedAsset::setNameDescription(std::string const& name, std::string const& description, LLDate const& date)
{
  mName = name;
  mDescription = description;
  mDate = date;
}

void UploadedAsset::toXML(std::ostream& os, int indentation) const
{
  AIXMLElement tag(os, "asset", indentation);
  tag.attribute("type", LLAssetType::lookup(mAssetType));
  tag.attribute(DEFAULT_MD5STR_NAME, mAssetMd5);
  if (mSourceMd5.isFinalized())
  {
	tag.child("source", mSourceMd5);
  }
  if (!mFilename.empty())
  {
	tag.child("filename", mFilename);
  }
  if (!mName.empty())
  {
	tag.child("name", mName);
  }
  if (!mDescription.empty())
  {
	tag.child("description", mDescription);
  }
  if (mDate.secondsSinceEpoch() > 0)
  {
	tag.child(mDate);
  }
  if (mDelta.notNull())
  {
	tag.child(*mDelta);
  }
  if (!mAssetUUIDs.empty())
  {
	tag.child(mAssetUUIDs.begin(), mAssetUUIDs.end());
  }
}

UploadedAsset::UploadedAsset(AIXMLElementParser const& parser)
{
  // This is a constructor, so the const cast is ok.
  if (!parser.attribute(DEFAULT_MD5STR_NAME, const_cast<LLMD5&>(mAssetMd5)))
  {
	THROW_ALERT("UploadedAsset_Missing_or_invalid_md5_attribute_in_asset_node");
  }
  std::string asset_type;
  if (!parser.attribute("type", asset_type))
  {
	THROW_ALERT("UploadedAsset_Missing_or_invalid_type_attribute_in_asset_node");
  }
  mAssetType = LLAssetType::lookup(asset_type);
  parser.child("source", mSourceMd5);
  parser.child("filename", mFilename);
  parser.child("name", mName);
  parser.child("description", mDescription);
  parser.child(DEFAULT_LLDATE_NAME, mDate);
  if (mAssetType == LLAssetType::AT_ANIMATION)
  {
	LLPointer<BVHAnimDelta> delta = new BVHAnimDelta;
	if (parser.child("bvhanimdelta", *delta))
	{
	  mDelta = delta;
	}
  }
  parser.insert_children("grid", mAssetUUIDs);
}

UploadedAsset& UploadedAsset::operator=(UploadedAsset const& ua)
{
  if (!mAssetMd5.isFinalized() || mAssetMd5 != ua.mAssetMd5)
  {
	THROW_ALERT("UploadedAsset_Reading_assert_md5_attribute_md5_mismatches_the_xml_filename");
  }
  mAssetType = ua.mAssetType;
  mName = ua.mName;
  mDescription = ua.mDescription;
  mFilename = ua.mFilename;
  mDate = ua.mDate;
  mDelta = ua.mDelta;
  mSourceMd5 = ua.mSourceMd5;
  mAssetUUIDs = ua.mAssetUUIDs;
  return *this;
}

LLUUID const* UploadedAsset::find_uuid(void) const
{
  Grid const current_grid(gHippoGridManager->getConnectedGrid()->getGridNick());
  grid2uuid_map_type::const_iterator uuid = find_uuid(current_grid);
  return (uuid != end_uuid()) ? &uuid->getUUID() : NULL;
}

} // namespace AIMultiGrid
