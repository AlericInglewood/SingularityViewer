/**
 * @file aiuploadedasset.h
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
 *   24/07/2013
 *   Initial version, written by Aleric Inglewood @ SL
 *
 *   03/09/2013
 *   Created aiuploadedasset.h (AI)
 */

#ifndef AIUPLOADEDASSET_H
#define AIUPLOADEDASSET_H

#include <set>
#include <string>
#include <iosfwd>
#include "aithreadsafe.h"
#include "aimultigriddelta.h"
#include "llassettype.h"		// LLAssetType::EType
#include "lluuid.h"				// LLUUID
#include "llmd5.h"				// LLMD5
#include "lldate.h"				// LLDate

class AIXMLElementParser;

namespace AIMultiGrid {

class GridUUID;

//-----------------------------------------------------------------------------
// class Grid

// Grid identifier.
// Ideally this should be unique and forever the same.

class Grid
{
  private:
	friend class GridUUID;
	std::string mGridNick;

	Grid(void) { }

  public:
	Grid(std::string const& grid_nick) : mGridNick(grid_nick) { }

	friend std::ostream& operator<<(std::ostream& os, Grid const& grid) { return os << grid.mGridNick; }
	friend bool operator<(Grid const& g1, Grid const& g2);
	friend bool operator==(Grid const& g1, Grid const& g2) { return g1.mGridNick == g2.mGridNick; }
	friend bool operator!=(Grid const& g1, Grid const& g2) { return g1.mGridNick != g2.mGridNick; }

	std::string const& getGridNick(void) const { return mGridNick; }
};

// Arbitrary but unique Grid ordering.
inline bool operator<(Grid const& g1, Grid const& g2)
{
  return g1.mGridNick < g2.mGridNick;
}

//-----------------------------------------------------------------------------
// class GridUUID

// Grid / UUID pair.
// Ideally, any given UUID should only appear in a single grid.

class GridUUID
{
  private:
	Grid mGrid;
	LLUUID mUUID;		// This is not part of the key and is after insertion into a grid2uuid_map_type.

  public:
	GridUUID(Grid const& grid, LLUUID const& id) : mGrid(grid), mUUID(id) { }
	GridUUID(Grid const& grid) : mGrid(grid) { }
	GridUUID(AIXMLElementParser const& parser);

	// Accessors.
	Grid const& getGrid(void) const { return mGrid; }
	LLUUID const& getUUID(void) const { return mUUID; }

	// Manipulator.
	void setUUID(LLUUID const& uuid) { mUUID = uuid; }

	// Functor for ordering GridUUID by Grid (only).
	struct ordering {
	  bool operator()(GridUUID const& gu1, GridUUID const& gu2) const
	  {
		return gu1.mGrid < gu2.mGrid;
	  }
	};

	// Comparison.
	friend bool operator==(GridUUID const& gu1, GridUUID const& gu2) { return gu1.mUUID == gu2.mUUID && gu1.mGrid == gu2.mGrid; }
	friend bool operator!=(GridUUID const& gu1, GridUUID const& gu2) { return gu1.mUUID != gu2.mUUID || gu1.mGrid != gu2.mGrid; }

	void toXML(std::ostream& os, int indentation) const;
};

// Grid --> UUID mapping.
typedef std::set<GridUUID, GridUUID::ordering> grid2uuid_map_type;

//-----------------------------------------------------------------------------
// class UploadedAsset

// Representation of an (uploaded) asset.
//
// Note that any given asset can only have one source; if two different originals are lossless
// representations of the same thing then the resulting asset md5 could still be the same,
// but if this happens then all but one source is simply disgarded, as they are essentially
// the same anyway.
//
// If the same source file is uploaded to two different grids, but due to a difference
// in encoding the resulting asset md5 is different, then those assets are basically
// different. The two asset UUIDs will still be considered equivalent, but we'll have two
// different UploadedAsset objects (with the same mSourceMd5).
//
// If an upload to a second grid results in the same mAssetMd5, then no new UploadedAsset
// is created (name and source are not updated).
//
// If an asset is uploaded from an export (so no original source exists) then, if the resulting
// mAssetMd5 differs, the name and source hash are copied from the UploadedAsset that was used.
// If no UploadedAsset is availabe then the name is set to the name of the exported asset,
// and mSourceMd5 is left uninitialized.

class UploadedAsset
{
  private:
	LLMD5 const mAssetMd5;			// The md5 hash of the local asset file. This is unique: there is only one UploadedAsset with this hash.
	LLAssetType::EType mAssetType;	// Type of the asset.
	std::string mName;				// The upload name during the last manual upload.
	std::string mDescription;		// The description (during last manual upload).
	std::string mFilename;			// The original source file path (during last manual upload).
	LLDate mDate;					// The time and date of when filename, name and description were last set.
	LLMD5 mSourceMd5;				// The md5 hash of the originally uploaded file. Only valid if mSourceMd5.isFinalized() returns true.
									// If the source is a local asset file from our database (with corresponding UploadedAsset object thus)
									// then mSourceMd5 is not set.
	LLPointer<Delta> mDelta;		// Pointer to additional data (on top of source file) used to generate this asset. NULL if there wasn't any.

	grid2uuid_map_type mAssetUUIDs;	// The grid/UUID combinations of this asset.

  public:
	// Constructor.
	UploadedAsset(LLMD5 const& assetMd5, LLAssetType::EType type, std::string const& name, std::string const& description,
		std::string const& filename, LLDate const& date, LLMD5 const& sourceMd5, LLPointer<AIMultiGrid::Delta> const& delta) :
	    mAssetMd5(assetMd5), mAssetType(type), mName(name), mDescription(description), mFilename(filename), mDate(date), mSourceMd5(sourceMd5), mDelta(delta) { }
	// Construct a UploadedAsset that can be used as search key.
	UploadedAsset(LLMD5 const& assetMd5) : mAssetMd5(assetMd5) { }
	// Construct a UploadedAsset from XML.
	UploadedAsset(AIXMLElementParser const& parser);

	// Copy over all variable members from ua; this doesn't change mAssetMd5 - but checks if mAssetMd5 is already equal to ua.mAssetMd5.
	UploadedAsset& operator=(UploadedAsset const& ua);

	// Return the UUID on the current grid, if any.
	LLUUID const* find_uuid(void) const;
	grid2uuid_map_type::const_iterator find_uuid(Grid const& grid) const { return mAssetUUIDs.find(GridUUID(grid)); }
	grid2uuid_map_type::const_iterator end_uuid(void) const { return mAssetUUIDs.end(); }

	// Accessors.
	LLMD5 const& getAssetMd5(void) const { return mAssetMd5; }
	LLAssetType::EType getAssetType(void) const { return mAssetType; };
	std::string const& getName(void) const { return mName; }
	LLDate const& getDate(void) const { return mDate; }
	LLMD5 const& getSourceMd5(void) const { return mSourceMd5; }
	std::string const& getFilename(void) const { return mFilename; }
	std::string const& getDescription(void) const { return mDescription; }
	LLPointer<Delta> const& getDelta(void) const { return mDelta; }
	grid2uuid_map_type const& getGrid2UUIDmap(void) const { return mAssetUUIDs; }

	// XML methods.
	void toXML(std::ostream& os, int indentation) const;

  private:
	friend class LockedBackEnd;
	void setSource(std::string const& name, LLDate const& date, LLMD5 const& sourceMd5);
	void setNameDescription(std::string const& name, std::string const& description, LLDate const& date);
	// Add or change Grid/UUID for this asset.
	void add(GridUUID const& asset_id) { mAssetUUIDs.insert(asset_id); }
	// Obtain access to the whole Grid/UUID map.
	grid2uuid_map_type& getGrid2UUIDmap(void) { return mAssetUUIDs; }
};

} // namespace AIMultiGrid

//=============================================================================
// AIUploadedAsset: thread safe access to AIMultiGrid::UploadedAsset.

typedef AIAccessConst<AIMultiGrid::UploadedAsset> AIUploadedAsset_crat;
typedef AIAccess<AIMultiGrid::UploadedAsset> AIUploadedAsset_rat;
typedef AIAccess<AIMultiGrid::UploadedAsset> AIUploadedAsset_wat;

class AIUploadedAsset : public AIThreadSafeSimple<AIMultiGrid::UploadedAsset>
{
  public:
	// Construct a new AIUploadedAsset.
	AIUploadedAsset(LLMD5 const& assetMd5, LLAssetType::EType type, std::string const& name, std::string const& description,
		std::string const& filename, LLDate const& date, LLMD5 const& sourceMd5, LLPointer<AIMultiGrid::Delta> const& delta)
		{ new (ptr()) AIMultiGrid::UploadedAsset(assetMd5, type, name, description, filename, date, sourceMd5, delta); }
	// Construct a AIUploadedAsset that can be used as search key.
	AIUploadedAsset(LLMD5 const& assetMd5) { new (ptr()) AIMultiGrid::UploadedAsset(assetMd5); }
	// Construct a UploadedAsset from a UploadedAsset.
	AIUploadedAsset(AIMultiGrid::UploadedAsset const& ua1) { new (ptr()) AIMultiGrid::UploadedAsset(ua1); }

	// Functor for ordering AIUploadedAsset.
	struct ordering {
	  inline bool operator()(AIUploadedAsset* ua1, AIUploadedAsset* ua2) const
	  {
		// Since mAssetMd5 is unique, it is enough to order by that.
		return AIUploadedAsset_crat(*ua1)->getAssetMd5() < AIUploadedAsset_crat(*ua2)->getAssetMd5();
	  }
	};
};

#endif // AIUPLOADEDASSET_H

