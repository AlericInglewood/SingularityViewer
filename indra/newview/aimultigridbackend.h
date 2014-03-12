/**
 * @file aimultigridbackend.h
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
 *   Creation of aimultigridbackend.h (AI)
 */

#ifndef AIMULTIGRIDBACKEND_H
#define AIMULTIGRIDBACKEND_H

#include <map>
#include <vector>
#include <deque>
#include "aiuploadedasset.h"	// AIUploadedAsset, AIUploadedAsset_wat, GridUUID, LLAssetType::EType, LLMD5, LLUUID, LLDate, std::set, std::string
#include "aimultigriddelta.h"	// Delta
#include "aicondition.h"		// AICondition
#include "aicommonuuid.h"       // is_common_uuid
#include "llsingleton.h"		// LLSingleton
#include "llthread.h"			// LLMutex
#include "llfile.h"				// LLFILE

namespace AIMultiGrid {

class BackEnd;
class BackEndAccess;
class LostAndFound;

// In-memory cache types.
typedef std::set<AIUploadedAsset*, AIUploadedAsset::ordering> asset_map_type;
typedef std::vector<asset_map_type::iterator> uais_type;
typedef std::map<LLMD5, uais_type> source2asset_map_type;
typedef std::map<LLUUID, asset_map_type::iterator> uuid2asset_map_type;

// Asset md5 --> UploadedAsset mapping.
typedef AIThreadSafeSimpleDC<asset_map_type> gAssets_t;
typedef AIAccessConst<asset_map_type> gAssets_crat;
typedef AIAccess<asset_map_type> gAssets_rat;
typedef AIAccess<asset_map_type> gAssets_wat;

// Source md5 --> UploadedAsset mapping.
typedef AIThreadSafeSimpleDC<source2asset_map_type> gSources_t;
typedef AIAccessConst<source2asset_map_type> gSources_crat;
typedef AIAccess<source2asset_map_type> gSources_rat;
typedef AIAccess<source2asset_map_type> gSources_wat;

// Asset UUID --> UploadedAsset mapping.
typedef AIThreadSafeSimpleDC<uuid2asset_map_type> gUUIDs_t;
typedef AIAccessConst<uuid2asset_map_type> gUUIDs_crat;
typedef AIAccess<uuid2asset_map_type> gUUIDs_rat;
typedef AIAccess<uuid2asset_map_type> gUUIDs_wat;

// Functions that may only be accessed by BackEndAccess.
class LockedBackEnd {
  private:
	BackEnd& mBackEnd;
	LLMutex mJournalMutex;

	// Member variables used during repair_database.
	friend class LostAndFound;
	bool mFixed;										// Return value of repair_database and rename_gridnick.
	LostAndFound* mLostFound;

  private:
	void write_to_disk(AIUploadedAsset_wat const& uploaded_asset_w);
	void write_to_disk(std::deque<LLMD5> const& md5s, std::string const& source_filename);
	void write_to_disk(LLMD5 const& md5, std::string const& uuid_filename);

	void read_from_disk(UploadedAsset& ua, std::string const& asset_filename);
	void read_from_disk(std::deque<LLMD5>& md5s, std::string const& source_filename);
	void read_from_disk(LLMD5& md5, std::string const& uuid_filename);

	// Used by LostAndFound.
	std::string const& getBaseFolder(void) const;

  public:
	LockedBackEnd(BackEnd& back_end) : mBackEnd(back_end), mLostFound(NULL) { }

	// Flush the in-memory cache of the database.
	void clear_memory_cache(void);

	// Register a previously uploaded asset with the database.
	void registerUpload(
		LLMD5 const& assetMd5,					// The md5 of the downloaded asset.
		LLAssetType::EType type,				// The type of the asset.
		GridUUID const& asset_id,				// The UUID and Grid of the uploaded asset.
		std::string const& name,				// A human readable name of the asset.
		std::string const& description,			// A human readable description.
		bool automated_upload,					// Set to false if name and description need to refresh even if this upload already exists,
												// and when (in case of !is_one_on_one) an existing asset hash should be moved up front in the by_source file.
		std::string const& sourceFilename,		// Path to the original source file.
		LLDate const& date,						// Date and time of the initial upload of the asset.
		LLMD5 const& sourceMd5,					// The md5 of the original source file.
		bool is_one_on_one,						// True if the source <--> asset relationship is one on one.
		LLPointer<Delta> const& delta);			// Pointer to extra data that was used to generate the asset, if any.

    // Update or create an asset in the database.
    AIUploadedAsset* storeAsset(
        unsigned char* buffer,                  // Pointer to the asset in memory.
        size_t buffer_length,                   // The size of the asset in bytes.
        LLAssetType::EType asset_type,          // The type of the asset.
        GridUUID const& asset_id,               // The UUID and Grid of the asset.
        std::string const& name,                // A human readable name of the asset.
        std::string const& description,         // A human readable description.
        std::string const& sourceFilename = std::string(),  // Path to the original source file, if any.
        LLMD5 const& sourceMd5 = LLMD5());                  // The md5 of the original source file (if any).

	// Run a consistency on the database and attempt to fix it.
	bool repair_database(void);

	// Replace the nick in <grid nick="from_gridnick"> with "to_gridnick".
	bool rename_gridnick(std::string const& from_gridnick, std::string const& to_gridnick);

	// The getUploadedAsset functions are slow as they accesses the disk, but only the first time for each id / md5 hash, since the result is cached.

	AIUploadedAsset* getUploadedAsset(LLUUID const& id);				// Returns previously uploaded asset with id, or NULL if none exists.
	AIUploadedAsset* getUploadedAsset(LLMD5 const& asset_hash);			// Checks if a file with md5 hash was uploaded before and return the UploadedAsset.
																		// is true, or NULL if none was found. May only be called for one_on_one sources!
	uais_type getUploadedAssets(LLMD5 const& source_hash);				// Returns a vector with all previously UploadedAssets for this source.

    // Calculate the asset md5 hash of an asset in memory of type 'type'.
    void calculateHash(LLAssetType::EType type, unsigned char* buffer, size_t size, LLMD5& asset_md5);

    // Update asset_md5 with the grid-independent hash value of uuid.
    void updateHash(LLUUID const& uuid, LLMD5& asset_md5);
    // Static function so this can be called from llcharacter.
    static void update_hash(AIMultiGrid::LockedBackEnd* self, LLUUID const& uuid, LLMD5& asset_md5) { self->updateHash(uuid, asset_md5); }

    // Find the matching UUID for the current grid. If the passed UUID could be found and needs to be changed,
    // changes it (possibly to the same value) and returns true. Otherwise the id is left unchanged and false
    // is returned.
    bool translate(LLUUID& uuid);

  private:
	asset_map_type::iterator readAndCacheUploadedAsset(gAssets_wat const& gAssets_w, LLMD5 const& hash);	// Reads and decodes uploaded asset file into
                                                                                                            // a new AIUploadedAsset which is inserted in
                                                                                                            // gAssets and returned.
	void readAndCacheAllUploadedAssets(void);							// Same for every asset file in the database (calls the one above).
};

class LockAttemps {
  private:
	int mLockAttempts;
  public:
	LockAttemps(void) : mLockAttempts(0) { }
	bool met(void) const { return mLockAttempts == 0; }
	void attempt_start(void) { ++mLockAttempts; }
	bool attempt_end(void) { llassert(mLockAttempts > 0); return --mLockAttempts == 0; }
};

typedef AICondition<LockAttemps> lock_condition_t;
typedef AIAccess<LockAttemps> lock_condition_wat;

enum mergedb_nt {
  mergedb_success						// Database merge succeeded.
};

class BackEnd : public LLSingleton<BackEnd>
{
	friend class LLSingleton<BackEnd>;
	friend class LockedBackEnd;

  private:
	bool mInitialized;					// Set to true when init() is called and this class was initialized.
	std::string mBaseFolder;

	static gAssets_t gAssets;
	static gSources_t gSources;
	static gUUIDs_t gUUIDs;
	static asset_map_type::iterator const gAssetsEnd;

  private:
	friend class BackEndAccess;
	LockedBackEnd mLocked;				// May only be accessed through BackEndAccess.

  public:
	BackEnd(void) : mInitialized(false), mLocked(*this) { }
	void init(void);

	// Check and repair the database. Throws if the current database is locked.
	bool repair_database(void);

	// Switch to a new base_folder. Throws if the switch fails.
	void switch_path(std::string base_folder);

	// Merge with another database.
	mergedb_nt merge_database(std::string base_folder);

	// Rename a grid nick 'from_gridnick' to 'to_gridnick'.
	static bool rename_gridnick(BackEndAccess& back_end, std::string const& from_gridnick, std::string const& to_gridnick);

    // Checks for the existence of id in the database. The result is cached so its fine to call it often for the same UUID.
    AIUploadedAsset* getUploadedAsset(LLUUID const& id);

    // Returns true if the uuid does not need to be stored in the database because it is either a common uuid or it is already stored.
    // Call this if the database is NOT locked.
    bool is_known(LLUUID const& id) { return is_common_uuid(id) || getUploadedAsset(id); }

    // Idem, but can be used when the database is locked. Note that back_end does not need to be locked;
    // if it isn't then the database will be locked especially for this occassion, but normally should
    // already be locked at the beginning of the transaction.
    bool is_known(LLUUID const& id, BackEndAccess& back_end);

  private:
	bool checkSubdirs(bool create);

	friend class FrontEnd;
	std::string getAssetFilename(LLMD5 const& assetMd5) const;
	std::string getUploadedAssetFilename(LLMD5 const& assetMd5) const;
	std::string getUUIDFilename(LLUUID const& id) const;
	std::string getSourceFilename(LLMD5 const& sourceMd5) const;
  public:
	std::string getJournalFilename(char const* filename) const;
};

} // namespace AIMultiGrid

#endif // AIMULTIGRIDBACKEND_H

