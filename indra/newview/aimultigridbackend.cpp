/**
 * @file aimultigridbackend.cpp
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
 *   Creation of aimultigridbackend.cpp (AI)
 */

#include "llviewerprecompiledheaders.h"
#include "aimultigridbackend.h"
#include "aimultigridbackendaccess.h"
#include "aimultigridfrontend.h"
#include "aixml.h"
#include "aifile.h"
#include "aibvhanimdelta.h"
#include "lldir.h"
#include "llnotificationsutil.h"
#include "llcontrol.h"
#include "lltrans.h"

extern LLControlGroup gSavedSettings;

// On the database structure (BackEnd).
//
// I chose to use a database based on the file system because
// * it is portable and with not too many files per directory it allows for a fast lookup.
// * it has proof of concept in the texture cache implementation which makes it easy for
//   me to write portable code like this.
// * it results in a human readable database that allows new people to learn
//   and understand how this works by browsing through the database, which is
//   beneficial for adoption and collaboration.
// * it theoretically allows for manual merging of two databases.
//
// We need the following data to be stored:
// 1) The asset files (by their md5).
// 2) XML files representing all UploadedAsset objects corresponding to those asset files (by the same md5).
// 3) XML files listing those asset file md5 hashes as function of UUID.
// 4) XML files by md5 of uploaded source files that list the md5 of corresponding asset files.
// 5) Temporary data.

namespace {

// Everything related to AIMultiGrid is stored in a directory called 'some/path/uploads/'
// where the directory 'uploads' can be moved to a different directory, or backed up and/or
// given to others if the creator (uploader) wishes to share their work.
// Note that such backups can normally be unpacked on top of eachother, since every file is
// either an md5 or an UUID and therefore uniquely representing itself. Only when the exact
// same source file was uploaded by two different people then a more intelligent merge tool
// is needed.
//
// Inside 'uploads' we have the following directory structure:

static char const* database_structure[] = {
  "assets",			// 1)	followed by md5 lookup directory structure.
  "by_asset_md5",	// 2)	followed by md5 lookup directory structure.
  "by_uuid",		// 3)	followed by uuid lookup directory structure.
  "by_source_md5",	// 4)	followed by md5 lookup directory structure.
  "journal"			// 5)
};

static char const subdirs[17] = "0123456789abcdef";

enum folder_id {
  fi_assets			= 0,
  fi_by_asset_md5	= 1,
  fi_by_uuid		= 2,
  fi_by_source_md5	= 3,
  fi_journal		= 4,
  database_structure_size
};

char const* const xml_type[] = {
  "",
  "asset_data",
  "uuid_map",
  "source_map"
};

} // namespace

namespace AIMultiGrid {

//-----------------------------------------------------------------------------
// BackEnd

std::string BackEnd::getAssetFilename(LLMD5 const& assetMd5) const
{
  llassert_always(assetMd5.isFinalized());
  char digest[33];
  assetMd5.hex_digest(digest);
  std::string delim = gDirUtilp->getDirDelimiter();
  return gDirUtilp->add(mBaseFolder, database_structure[fi_assets] + delim + digest[0] + delim + digest);
}

std::string BackEnd::getUploadedAssetFilename(LLMD5 const& assetMd5) const
{
  llassert_always(assetMd5.isFinalized());
  char digest[33];
  assetMd5.hex_digest(digest);
  std::string delim = gDirUtilp->getDirDelimiter();
  return gDirUtilp->add(mBaseFolder, database_structure[fi_by_asset_md5] + delim + digest[0] + delim + digest + ".xml");
}

std::string BackEnd::getUUIDFilename(LLUUID const& id) const
{
  llassert_always(id.notNull());
  std::string uuid_str;
  id.toString(uuid_str);
  std::string delim = gDirUtilp->getDirDelimiter();
  return gDirUtilp->add(mBaseFolder, database_structure[fi_by_uuid] + delim + uuid_str[0] + delim + uuid_str + ".xml");
}

std::string BackEnd::getSourceFilename(LLMD5 const& sourceMd5) const
{
  llassert_always(sourceMd5.isFinalized());
  char digest[33];
  sourceMd5.hex_digest(digest);
  std::string delim = gDirUtilp->getDirDelimiter();
  return gDirUtilp->add(mBaseFolder, database_structure[fi_by_source_md5] + delim + digest[0] + delim + digest + ".xml");
}

std::string BackEnd::getJournalFilename(char const* filename) const
{
  std::string delim = gDirUtilp->getDirDelimiter();
  return gDirUtilp->add(mBaseFolder, database_structure[fi_journal] + delim + filename);
}

std::string BackEnd::getUniqueLostFoundDirname(void) const
{
  std::string dirname;
  std::string base = gDirUtilp->add(mBaseFolder, "lost+found");
  LLDate now = LLDate::now();
  S32 year, month, day, hour, min, sec;
  now.split(&year, &month, &day, &hour, &min, &sec);
  for (S32 count = 0;; ++count)
  {
	dirname = base + llformat("-%d%d%d.%d", year, month, day, count);
	if (!gDirUtilp->fileExists(dirname))
	{
	  break;
	}
  }
  return dirname;
}

void BackEnd::createFileLock(void)
{
  bool success = false;
  do
  {
	std::string lockfilename = getJournalFilename("lock");
	try
	{
	  // Open the file lock (this does not lock it).
	  boost::interprocess::file_lock flock(lockfilename.c_str());
	  // Transfer ownership to us.
	  mFLock.swap(flock);
	  success = true;
	}
	catch (boost::interprocess::interprocess_exception& error)
	{
	  if (error.get_error_code() != boost::interprocess::not_found_error)
	  {
		llerrs << "Failed to open 'uploads' database lock file \"" << lockfilename << "\"." << llendl;
		throw std::runtime_error("Failed to open 'uploads' database lock file.");
	  }
	  // File doesn't exist, create it and try again (this throws if it fails).
	  AIFile lockfile(lockfilename, "wb");
	}
  }
  while (!success);
}

int const mkdir_error = 0;
int const corrupt_database = 1;

// Return true if the subdirectories exists, false otherwise.
// If 'create' is true, attempt to create missing subdirectories.
// Throws std::runtime_error if the creation of a missing subdirectory fails.
bool BackEnd::checkSubdirs(bool create)
{
  bool all_already_existed = true;
  // Create the directory structure of the database.
  for (int fi = 0; fi < database_structure_size; ++fi)
  {
	folder_id top_folder = static_cast<folder_id>(fi);
	std::string path = gDirUtilp->add(mBaseFolder, database_structure[top_folder]);
	if (!LLFile::isdir(path))
	{
	  all_already_existed = false;
	  if (create)
	  {
		try
		{
		  AIFile::mkdir(path);
		}
		catch (AIAlert::Error const& error)
		{
		  THROW_ALERTC(mkdir_error, "BackEnd_While_creating_uploads_database_folder", error);
		}
	  }
	}
	if (top_folder != fi_journal)
	{
	  // Create the hash sub folders.
	  for (int sdi = 0; sdi < 16; ++sdi)
	  {
		std::string subdir = path + gDirUtilp->getDirDelimiter() + subdirs[sdi];
		if (!LLFile::isdir(subdir))
		{
		  all_already_existed = false;
		  if (create)
		  {
			try
			{
			  AIFile::mkdir(subdir);
			}
			catch (AIAlert::Error const& error)
			{
			  THROW_ALERTC(mkdir_error, "BackEnd_While_creating_uploads_database_folder", error);
			}
		  }
		}
	  }
	}
  }
  return all_already_existed;
}

void BackEnd::init(void)
{
  // This function may only be called once (at the start of the application).
  llassert_always(!mInitialized);

  // Strip trailing separator(s), if any.
  mBaseFolder = LLDir::stripTrailingSeparators(gDirUtilp->getUploadsDir());

  // Create the folder if it does not exist. Bail on failure.
  try
  {
	AIFile::mkdir(mBaseFolder);

	if (!checkSubdirs(true))
	{
	  llinfos << "Created 'uploads' database directory structure in \"" << mBaseFolder << "\"." << llendl;
	}

	createFileLock();
  }
  catch (AIAlert::Error const& error)
  {
	gDirUtilp->setUploadsDir("");
	gSavedSettings.setString("AIMultiGridBaseFolder", "");
	llerrs << LLTrans::getString("BackEnd_While_creating_uploads_database_folder") << AIAlert::text(error) << ". " <<
	          LLTrans::getString("BackEnd_Resetting_to_default_value") << llendl;
	// User chooses to continue with default path.
	mBaseFolder = gDirUtilp->getUploadsDir();
  }

  mInitialized = true;

  // Clean up failed commit to database.
  std::string updating_filename = getJournalFilename("updating");
  if (LLFile::isfile(updating_filename))
  {
	LLMD5 asset_hash;
	unsigned char digest[16];
	size_t len = 0;
	LLFILE* updating_fp = LLFile::fopen(updating_filename, "rb");
	if (updating_fp)
	{
	  len = fread(digest, sizeof(char), 16, updating_fp);
	  LLFile::close(updating_fp);
	}
	asset_hash.clone(digest);
	llwarns << "Running repair_database because journal file \"" << updating_filename << "\" exists";
	if (len == 16)
	{
	  llcont << " (with asset hash " << asset_hash << ")";
	}
	llcont << "." << llendl;
	try
	{
	  if (repair_database())
	  {
		AIAlert::add_modal("AIUploadsCheckDBfixed");
	  }
	}
	catch (AIAlert::ErrorCode const& error)
	{
	  if (error.getCode() == mkdir_error)
	  {
		AIAlert::add_modal("AIUploadsCheckDBmkdir", error);
	  }
	  else
	  {
		AIAlert::add_modal("AIUploadsCheckDBcorrupt", error);
	  }
	}
  }
  // Remove the journal file.
  LLFile::remove(updating_filename);
}

bool BackEnd::repair_database(void)
{
  DoutEntering(dc::notice, "BackEnd::repair_database() with mBaseFolder = \"" << mBaseFolder << "\".");
  llinfos << "Locking database for repair..." << llendl;
  BackEndAccess back_end;
  back_end.lock();
  bool fixed = back_end->repair_database();
  llinfos << "Repair finished. Unlocking database." << llendl;
  return fixed;
}

mergedb_nt BackEnd::merge_database(std::string base_folder)
{
  DoutEntering(dc::notice, "BackEnd::merge_database(\"" << base_folder << "\")");
  return mergedb_success;
}

void BackEnd::switch_path(std::string base_folder)
{
  DoutEntering(dc::notice, "BackEnd::switch_path(\"" << base_folder << "\")");

  // Strip trailing separator(s) from base_folder, if any.
  base_folder = LLDir::stripTrailingSeparators(base_folder);

  // Check if the path exists.
  if (!LLFile::isdir(base_folder))
  {
	THROW_ALERT("BackEnd_switch_path_FOLDER_does_not_exist", AIArgs("[FOLDER]", base_folder)("[OLDFOLDER]", mBaseFolder));
  }

  // Check if the new path has the right subdirectory structure (aka, is a database at all).
  for (int fi = 0; fi < database_structure_size; ++fi)
  {
	std::string subdir = gDirUtilp->add(base_folder, database_structure[fi]);
	if (!LLFile::isdir(subdir))
	{
	  THROW_ALERT("BackEnd_switch_path_FOLDER_does_not_appear_to_be_an_uploads_database_folder", AIArgs("[FOLDER]", base_folder)("[SUBDIR]", subdir));
	}
  }

  // Lock the old database, change the database path, lock the new database, run a sanity check on the new database.
  BackEndAccess new_back_end;
  BackEndAccess old_back_end;
  try
  {
	old_back_end.lock();
  }
  catch (DatabaseLocked const&)
  {
	THROW_ALERT("AIUploadsSwitchDBlocked");
  }

  // Change the path of the singleton to a new path.

  // Remember the old path in case it fails.
  std::string old_base_folder = mBaseFolder;
  // This unlocks old_back_end.
  old_back_end.switch_path(mBaseFolder, base_folder);

  // Lock the new database and run a consistency check on it.
  try
  {
	new_back_end.lock();
	try
	{
	  if (new_back_end->repair_database())
	  {
		AIAlert::add("BackEnd_New_database_repaired");
	  }
	}
	catch (AIAlert::Error const& error)
	{
	  // Oops, switch back the old database.
	  new_back_end.switch_path(mBaseFolder, old_base_folder);
	  THROW_ALERTC(corrupt_database, error, "BackEnd_New_database_repair_failed");
	}
  }
  catch (DatabaseLocked const&)
  {
	// The new database is already locked?!
	mBaseFolder = old_base_folder;
	THROW_ALERT("AIUploadsSwitchDBnewlocked");
  }

  // Let the UI know too.
  gDirUtilp->setUploadsDir(mBaseFolder);
  // Use this path also after a relog.
  gSavedSettings.setString("AIMultiGridBaseFolder", mBaseFolder);
}

// Some obscure voodoo to guess the likeliness of sensibility of a filename.
double entropy(std::string const& filepath)
{
  std::string const basename = gDirUtilp->getBaseFileName(filepath);
  int base = 1;	// Assume at least one character from the set "abcdefghijklmnopqrstuvwxyz".
  bool saw_upper = false;
  bool saw_digit = false;
  bool saw_other = false;
  int underscores = 0;
  for (std::string::const_iterator iter = basename.begin(); iter != basename.end(); ++iter)
  {
	if (std::isupper(*iter))
	{
	  if (!saw_upper) ++base;
	  saw_upper = true;
	}
	else if (std::isdigit(*iter))
	{
	  if (!saw_digit) ++base;
	  saw_digit = true;
	}
	else if (*iter == '_')
	{
	  ++underscores;
	}
	else
	{
	  if (!saw_other) ++base;
	  saw_other = true;
	}
  }
  std::string const extension = gDirUtilp->getExtension(filepath);
  static double logbase[5] = { 0.0, 0.0, 0.301, 0.477, 0.602 };		// log base
  return logbase[base] * (basename.length() - underscores) + (extension.empty() ? 2.0  : 0.0) + (underscores ? 0.0 : 0.5);
}

//-----------------------------------------------------------------------------
// LockedBackEnd

// Thread safe cache objects for Assets, Sources and UUIDs.
gAssets_t LockedBackEnd::gAssets;
gSources_t LockedBackEnd::gSources;
gUUIDs_t LockedBackEnd::gUUIDs;

// Cache end() to avoid the need to lock every time.
asset_map_type::iterator const LockedBackEnd::gAssetsEnd = gAssets_wat(LockedBackEnd::gAssets)->end();

void LockedBackEnd::clear_memory_cache(void)
{
  gAssets_wat gAssets_w(gAssets);
  gSources_wat gSources_w(gSources);
  gUUIDs_wat gUUIDs_w(gUUIDs);
  gAssets_w->clear();
  gSources_w->clear();
  gUUIDs_w->clear();
}

void LockedBackEnd::write_to_disk(AIUploadedAsset_wat const& uploaded_asset_w)
{
  std::string uploaded_asset_filename = mBackEnd.getUploadedAssetFilename(uploaded_asset_w->mAssetMd5);
  AIFile uploaded_asset_fp(uploaded_asset_filename, "wb");
  // Write the uploaded asset XML file (fi_by_asset_md5).
  AIXMLRootElement tag(uploaded_asset_fp, "aimultigrid");
  tag.attribute("version", "1.0");
  tag.attribute("type", xml_type[fi_by_asset_md5]);
  tag.child(LLDate::now());
  tag.child(*uploaded_asset_w);
}

void LockedBackEnd::write_to_disk(std::deque<LLMD5> const& md5s, std::string const& source_filename)
{
  AIFile source_fp(source_filename, "wb");
  // Write the source lookup file (fi_by_source_md5).
  AIXMLRootElement tag(source_fp, "aimultigrid", false);
  tag.attribute("version", "1.0");
  tag.attribute("type", xml_type[fi_by_source_md5]);
  tag.child(md5s.begin(), md5s.end());
}

void LockedBackEnd::write_to_disk(LLMD5 const& md5, std::string const& uuid_filename)
{
  AIFile uuid_fp(uuid_filename, "wb");
  // Write the uuid lookup file (fi_by_uuid).
  AIXMLRootElement tag(uuid_fp, "aimultigrid", false);
  tag.attribute("version", "1.0");
  tag.attribute("type", xml_type[fi_by_uuid]);
  tag.child(md5);
}

void LockedBackEnd::read_from_disk(UploadedAsset& ua, std::string const& asset_filename)
{
  AIXMLParser asset_data(asset_filename, "uploaded-asset database file", "aimultigrid", 1);
  asset_data.attribute("type", xml_type[fi_by_asset_md5]);
  asset_data.child("asset", ua);
}

void LockedBackEnd::read_from_disk(std::deque<LLMD5>& md5s, std::string const& source_filename)
{
  md5s.clear();
  AIXMLParser source_map(source_filename, "uploaded-asset database file", "aimultigrid", 1);
  source_map.attribute("type", xml_type[fi_by_source_md5]);
  source_map.push_back_children("md5", md5s);
  if (md5s.empty())
  {
	THROW_ALERT("BackEnd_Uploaded_asset_database_file_FILE_exists_but_contains_no_asset_hash", AIArgs("[FILE]", source_filename));
  }
}

void LockedBackEnd::read_from_disk(LLMD5& md5, std::string const& uuid_filename)
{
  AIXMLParser uuid_map(uuid_filename, "uploaded-asset database file", "aimultigrid", 1);
  uuid_map.attribute("type", xml_type[fi_by_uuid]);
  uuid_map.child(md5);
}

void LockedBackEnd::registerUpload(
	LLMD5 const& assetMd5,
	LLAssetType::EType type,
	GridUUID const& asset_id,
	std::string const&  name,
	std::string const& description,
	bool automated_upload,
	std::string const& sourceFilename,
	LLDate const& date,
	LLMD5 const& sourceMd5,
	bool is_one_on_one,
	LLPointer<Delta> const& delta)
{
  bool need_update = false;
  bool need_insertion = false;
  bool need_source_update = false;

  // Sanity check.
  llassert_always(!is_one_on_one || !delta);

  AIUploadedAsset* uploaded_asset = getUploadedAsset(assetMd5);
  if (uploaded_asset)
  {
	// This asset already exists!
	//
	// We can come here for the following reasons (see description at the top of this file):
	// 3) An upload of an asset is attempted to a grid where it was not uploaded before (but it was uploaded to another grid). [this continuous at 5)]
	// 4) An upload of an asset is needed where only the UUID (possibly of another grid) is known.                             [this continuous at 5)]
	// 5) An upload of an asset is needed where the asset file is known (possibly of another grid).                            [this continuous at 6)]
	// 6) A previously exported asset is registered, without uploading it.
	//
	// In all cases, if the source hash was already known then we do not want to change it, nor will we want to change the source filename and date of upload.
	// In all cases, we want to add the GridUUID to the list, if not already there.
	//
	// 3) We don't want to touch the source filename or date of the original upload, unless we change the source hash too.
	// 4) We don't even have a new source hash, the source filename and date passed are to be discarded.
	// 5) Unless we get here via 3), then the same holds as for 4).
	// 6) Same as 5).
	//
	// Thus, in all cases we only want to change source filename, date and source hash when it wasn't set before and now we know it.

	AIUploadedAsset_wat uploaded_asset_w(*uploaded_asset);
	llassert_always(uploaded_asset_w->mAssetType == type);
	if (sourceMd5.isFinalized())
	{
	  bool source_hashes_different =
		  !uploaded_asset_w->mSourceMd5.isFinalized() ||		// No source hash was known before.
		  sourceMd5 != uploaded_asset_w->mSourceMd5;			// The source hash changed! This should never happen, but theoretically
		  														// it is possible that a different source file results in a different
																// source hash, even though it results in an asset file with the same
																// hash; in that case the algorithm to calculate the source hash must
																// be wrong-- but IF it happens, lets keep the latest calculated source
																// hash (corresponding to the most recently uploaded source file):
	  if (source_hashes_different ||
		  entropy(sourceFilename) <								// If the source hashes are equal, then only replace the source file
			  entropy(uploaded_asset_w->mFilename))				// name when the 'entropy' of the new name is lower than the one we had.
	  {
		uploaded_asset_w->setSource(sourceFilename, date, sourceMd5);
		need_update = true;
	  }
	  // If this is a one_on_one source then we do not want to update an existing by_source_md5 file (it will still be created if it doesn't exists).
	  // Otherwise, if it isn't an automated upload, we always have to rewrite it in case the asset hash is not up front.
	  need_source_update = (source_hashes_different || !automated_upload) && !is_one_on_one;
	}
	// Insert the GridUUID for a new grid, or update the UUID of an existing one.
	grid2uuid_map_type::const_iterator iter = uploaded_asset_w->find_uuid(asset_id.getGrid());
	if (iter == uploaded_asset_w->end_uuid())
	{
	  uploaded_asset_w->add(asset_id);
	  need_update = true;
	}
	else if (*iter != asset_id)
	{
	  // It's possible that we're changing the UUID (when a new upload occured because there wasn't a full perm item in then inventory anymore).
	  // The const_cast here is OK because setUUID does not change the 'key' part that determines the elements ordering in the set.
	  const_cast<GridUUID&>(*iter).setUUID(asset_id.getUUID());
	  need_update = true;
	}
	// Refresh the name and/or description when this was a new manual upload.
	if (!automated_upload)
	{
	  if (uploaded_asset_w->mName != name || uploaded_asset_w->mDescription != description)
	  {
		uploaded_asset_w->setNameDescription(name, description, date);
		need_update = true;
	  }
	}
	// If for some reason no delta exists yet and we have one, then add it.
	if (delta)
	{
	  if (!uploaded_asset_w->mDelta)
	  {
		uploaded_asset_w->mDelta = delta;
		need_update = true;
	  }
	}
  }
  else	// Create a new UploadedAsset.
  {
	uploaded_asset = new AIUploadedAsset(assetMd5, type, name, description, sourceFilename, date, sourceMd5, delta);
	AIUploadedAsset_wat(*uploaded_asset)->add(asset_id);
	need_insertion = true;
	need_update = true;
	need_source_update = true;
  }
  if (need_update)
  {
	// Write uploaded asset to disk.

	// Lock the journal.
	mJournalMutex.lock();

	// Lock the asset and get the raw digest.
	AIUploadedAsset_wat uploaded_asset_w(*uploaded_asset);
	unsigned char digest[16];
	uploaded_asset_w->mAssetMd5.raw_digest(digest);

	// Create a file in the journal called "updating" with as content the asset md5.
	std::string updating_filename = mBackEnd.getJournalFilename("updating");
	bool already_exists = LLFile::isfile(updating_filename);
	if (already_exists)
	{
	  llwarns << "The Journal is ALREADY locked! Trying to register new upload anyway..." << llendl;
	}
	else
	{
	  size_t len = 0;
	  LLFILE* updating_fp = LLFile::fopen(updating_filename, "wb");
	  if (updating_fp)
	  {
		len = fwrite(digest, sizeof(char), 16, updating_fp);
		LLFile::close(updating_fp);	// Calls fclose, which also flushes the file.
	  }
	  if (len != 16)
	  {
		llwarns << "Failed to create the journal file \"" << updating_filename << "\"!" << llendl;
	  }
	}

	{
	  // Make sure the asset file exists (fi_assets).
	  std::string asset_filename = mBackEnd.getAssetFilename(uploaded_asset_w->mAssetMd5);
	  if (!LLFile::isfile(asset_filename))
	  {
	    llerrs << "Registering uploaded asset that doesn't exist (\"" << asset_filename << "\" is not a file)! Bailing out." << llendl;
	  }
	  else
	  {
		try
		{
		  // Now actually register the new upload.
		  if (need_insertion)
		  {
			// In RAM.
			gAssets_wat(gAssets)->insert(uploaded_asset);
		  }
		  // To harddisk.
		  write_to_disk(uploaded_asset_w);

		  if (uploaded_asset_w->mSourceMd5.isFinalized())
		  {
			// Write the source lookup file (fi_by_source_md5).
			std::string source_filename = mBackEnd.getSourceFilename(uploaded_asset_w->mSourceMd5);
			// Never overwrite an existing source mapping of a one-on-one source.
			bool by_source_md5_file_exists = LLAPRFile::isExist(source_filename);
			if (need_source_update || !by_source_md5_file_exists)
			{
			  // Read any existing asset hashes from the source lookup file.
			  std::deque<LLMD5> md5s;
			  if (by_source_md5_file_exists)
			  {
				try
				{
				  read_from_disk(md5s, source_filename);
				}
				catch (AIAlert::Error const& error)
				{
				  //AIFIXME: Can't we catch the error at the statemachine level and then attempt a repair - and retry?
				  llerrs << AIAlert::text(error) << ". " << LLTrans::getString("BackEnd_Cannot_recover_from_database_corruption") << llendl;
				  // Leave journal locked.
				  return;
				}
			  }
			  // Put the asset md5 at front, unless it already exists and this is an automated upload.
			  bool order_changed = false;
			  std::deque<LLMD5>::iterator md5i = std::find(md5s.begin(), md5s.end(), uploaded_asset_w->mAssetMd5);
			  if (md5i == md5s.end())
			  {
				md5s.push_front(uploaded_asset_w->mAssetMd5);
				order_changed = true;	// Rather, a new entry was inserted at the beginning.
			  }
			  else if (!automated_upload && md5i != md5s.begin())
			  {
				md5s.erase(md5i);
				md5s.push_front(uploaded_asset_w->mAssetMd5);
				order_changed = true;
			  }
			  if (order_changed)
			  {
				write_to_disk(md5s, source_filename);
			  }
			}
		  }

		  // Write the UUID map file(s).
		  for (grid2uuid_map_type::iterator iter = uploaded_asset_w->mAssetUUIDs.begin(); iter != uploaded_asset_w->mAssetUUIDs.end(); ++iter)
		  {
			// Write the uuid lookup file (fi_by_uuid).
			std::string uuid_filename = mBackEnd.getUUIDFilename(iter->getUUID());
			// Never overwrite an existing uuid mapping.
			if (!LLAPRFile::isExist(uuid_filename))
			{
			  write_to_disk(uploaded_asset_w->mAssetMd5, uuid_filename);
			}
		  }
		}
		catch (AIAlert::Error const& error)
		{
		  std::ostringstream ss;
		  ss << uploaded_asset_w->mAssetMd5;
		  LLSD args(LLSD::emptyMap());
		  args["ASSETMD5"] = ss.str();
		  args["FILE"] = uploaded_asset_w->mFilename;
		  args["BASEFOLDER"] = mBackEnd.mBaseFolder;
		  llerrs << LLTrans::getString("BackEnd_Failed_to_commit_asset_to_database", args) << " [" << AIAlert::text(error) << "]. " <<
			  LLTrans::getString("BackEnd_Failed_to_commit_asset_to_database_cont", args) << llendl;
		  // Leave journal locked.
		  return;
		}
		catch(...)
		{
		  std::ostringstream ss;
		  ss << uploaded_asset_w->mAssetMd5;
		  LLSD args(LLSD::emptyMap());
		  args["ASSETMD5"] = ss.str();
		  args["FILE"] = uploaded_asset_w->mFilename;
		  args["BASEFOLDER"] = mBackEnd.mBaseFolder;
		  llerrs << LLTrans::getString("BackEnd_Failed_to_commit_asset_to_database", args) << "! " <<
			  LLTrans::getString("BackEnd_Failed_to_commit_asset_to_database_cont", args) << llendl;
		  // Leave journal locked.
		  return;
		}
	  }
	}

	// Do not remove the "updating" file when it already exists: we want to run repair_database upon relog.
	if (!already_exists)
	{
	  // Remove the journal file.
	  LLFile::remove(updating_filename);
	}

	// Unlock the journal.
	mJournalMutex.unlock();
  }
}

AIUploadedAsset* LockedBackEnd::getUploadedAsset(LLUUID const& id)
{
  // Try to find it in memory cache.
  {
	gUUIDs_rat gUUIDs_r(gUUIDs);
	uuid2asset_map_type::const_iterator iter = gUUIDs_r->find(id);
	if (iter != gUUIDs_r->end())
	{
	  return *iter->second;
	}
  }

  // Try to find it in the database.
  std::string uuid_filename = mBackEnd.getUUIDFilename(id);
  if (LLFile::isfile(uuid_filename))
  {
	LLMD5 md5;
	read_from_disk(md5, uuid_filename);
	// Get the uploaded asset data.
	asset_map_type::iterator uai = readAndCacheUploadedAsset(md5);
	if (uai == gAssetsEnd)
	{
	  llwarns << "Uploaded-asset database file \"" << uuid_filename << "\" exists, but points to non-existing asset " << md5 << "!" << llendl;
	  return NULL;
	}
	// Cache the result.
	gUUIDs_wat(gUUIDs)->insert(uuid2asset_map_type::value_type(id, uai));
	return *uai;
  }

  // Not found.
  return NULL;
}

AIUploadedAsset* LockedBackEnd::getUploadedAsset(LLMD5 const& asset_hash)
{
  // Try to find it in memory cache.
  {
	AIUploadedAsset key(asset_hash);
	gAssets_rat gAssets_r(gAssets);
	asset_map_type::const_iterator iter = gAssets_r->find(&key);
	if (iter != gAssets_r->end())
	{
	  return *iter;
	}
  }
  // Try to find it in the database.
  asset_map_type::iterator uai = readAndCacheUploadedAsset(asset_hash);
  return (uai == gAssetsEnd) ? NULL : *uai;
}

uais_type LockedBackEnd::getUploadedAssets(LLMD5 const& source_hash)
{
  // Try to find it in memory cache.
  {
	gSources_rat gSources_r(gSources);
	source2asset_map_type::const_iterator iter = gSources_r->find(source_hash);
	if (iter != gSources_r->end())
	{
	  // Return a copy, then release the lock on gSources.
	  return iter->second;
	}
  }
  // Try to find it in the database.
  uais_type uais;
  std::string source_filename = mBackEnd.getSourceFilename(source_hash);
  if (LLFile::isfile(source_filename))
  {
	std::deque<LLMD5> md5s;
	read_from_disk(md5s, source_filename);
	// Get the uploaded asset data.
	// Convert the md5 hashes to previously uploaded assets.
	for (std::deque<LLMD5>::iterator md5i = md5s.begin(); md5i != md5s.end(); ++md5i)
	{
	  asset_map_type::iterator uai = readAndCacheUploadedAsset(*md5i);
	  if (uai == gAssetsEnd)
	  {
		llwarns << "Uploaded-asset database file \"" << source_filename << "\" exists, but points to non-existing asset " << *md5i << "!" << llendl;
		continue;
	  }
	  uais.push_back(uai);
	}
	// Cache the result.
	gSources_wat(gSources)->insert(source2asset_map_type::value_type(source_hash, uais));
  }
  return uais;
}

asset_map_type::iterator LockedBackEnd::readAndCacheUploadedAsset(LLMD5 const& asset_hash)
{
  std::string asset_filename = mBackEnd.getUploadedAssetFilename(asset_hash);
  if (LLFile::isfile(asset_filename))
  {
	// Read the uploaded asset data from disk.
	UploadedAsset ua(asset_hash);
	read_from_disk(ua, asset_filename);
	// Insert a new AIUploadedAsset in the in-memory cache and return an iterator to it.
	return gAssets_wat(gAssets)->insert(new AIUploadedAsset(ua)).first;
  }
  return gAssetsEnd;
}

void LockedBackEnd::move_to_lostfound(std::string const& filepath, int fi, int sdi)
{
  std::string filename = gDirUtilp->getBaseFileName(filepath, false);
  std::string newpath = gDirUtilp->add(mLostfoundDirname, database_structure[fi]);
  try
  {
	AIFile::mkdir(newpath);
	if (sdi >= 0 && sdi < 16)
	{
	  newpath += gDirUtilp->getDirDelimiter() + subdirs[sdi];
	}
	else
	{
	  newpath += gDirUtilp->getDirDelimiter() + filename[0];
	}
	AIFile::mkdir(newpath);
  }
  catch (AIAlert::Error const& error)
  {
	THROW_ALERTC(mkdir_error, error);
  }
  newpath = gDirUtilp->add(newpath, filename);
  llwarns << "Moving file \"" << filepath << "\" to \"" << newpath << "\"." << llendl;
  AIFile::rename(filepath, newpath);
  mLostfound_used = true;
  mFixed = true;
}

bool LockedBackEnd::repair_database(void)
{
  llinfos << "Checking database in \"" << mBackEnd.mBaseFolder << "\"." << llendl;

  mFixed = false;

  // Create the directory structure of the database.
  if (!mBackEnd.checkSubdirs(true))			// Create if they do not exist. Throw if that fails.
  {
	llwarns << "One of more subdirectories were missing! Created." << llendl;
	mFixed = true;
  }

  // Lock the memory cache.
  gAssets_wat gAssets_w(gAssets);
  gSources_wat gSources_w(gSources);
  gUUIDs_wat gUUIDs_w(gUUIDs);
  // Clear it.
  clear_memory_cache();

  // Find an unused lost+find directory (to make sure we can rename files to it without getting collisions with old files).
  mLostfoundDirname = mBackEnd.getUniqueLostFoundDirname();
  // Try to create it.
  try
  {
	AIFile::mkdir(mLostfoundDirname);
  }
  catch (AIAlert::Error const& error)
  {
	THROW_ALERTC(mkdir_error, error);
  }
  mLostfound_used = false;

  try
  {

	//---------------------------------------------------------------------------
	// Fill the memory cache with all by_asset_md5 files.

	// Run over all meta files.
	std::string path = gDirUtilp->add(mBackEnd.mBaseFolder, database_structure[fi_by_asset_md5]);
	for (int sdi = 0; sdi < 16; ++sdi)
	{
	  std::string subdir = path + gDirUtilp->getDirDelimiter() + subdirs[sdi];
	  std::vector<std::string> files = gDirUtilp->getFilesInDir(subdir);
	  for (std::vector<std::string>::iterator filename = files.begin(); filename != files.end(); ++filename)
	  {
		// Reconstruct the full path.
		std::string filepath = subdir + gDirUtilp->getDirDelimiter() + *filename;

		// Convert the filename to a hash value.
		std::string hash_str = gDirUtilp->getBaseFileName(*filename, true);
		std::string ext = gDirUtilp->getExtension(*filename);
		// Make sure the filename looks like an md5 hash.
		if (ext != "xml" || hash_str.length() != 32 || hash_str.find_first_not_of(subdirs) != std::string::npos)
		{
		  llinfos << "Skipping alien file \"" << filepath << "\"." << llendl;
		  continue;
		}

		// Check if the file is in the right subdirectory.
		if (hash_str[0] != subdirs[sdi])
		{
		  // That way it would never be found, so move it to the right place.
		  std::string new_filepath = path + gDirUtilp->getDirDelimiter() + hash_str[0] + gDirUtilp->getDirDelimiter() + *filename;
		  if (LLFile::isfile(new_filepath))
		  {
			llwarns << "Moving file \"" << filepath << "\", which is in the wrong subdirectory, to \"" << mLostfoundDirname << "\" because \"" << new_filepath << "\" already exists!" << llendl;
			move_to_lostfound(filepath, fi_by_asset_md5, sdi);
			continue;
		  }
		  else
		  {
			llwarns << "Moving file \"" << filepath << "\" to \"" << new_filepath << "\"." << llendl;
			AIFile::rename(filepath, new_filepath);
			filepath = new_filepath;
			mFixed = true;
		  }
		}

		// Decode the name into it's digest.
		LLMD5 md5;
		md5.clone(hash_str);

		// Read the file using the normal readAndCacheUploadedAsset function.
		bool success;
		try
		{
		  success = readAndCacheUploadedAsset(md5) != gAssetsEnd;
		}
		catch (AIAlert::Error const& error)
		{
		  llwarns << AIAlert::text(error) << llendl;
		  // Move the file to lost+found and skip to the next file on any error.
		  move_to_lostfound(filepath, fi_by_asset_md5, sdi);
		  continue;
		}
		if (!success)
		{
		  // This should normally never happen (it means that filepath isn't a file, but it is!)
		  // in other words, I don't know what is wrong if we get here and therefore do not bail,
		  // but try to continue with the repair by just ignoring this file :/
		  llwarns << "Failed to read back file \"" << *filename << "\" using hash " << md5 << " !!??" << llendl;
		  continue;
		}
	  }
	}

	// Next we read all asset files.
	std::map<LLMD5, LLMD5> hash_translation;
	LLPointer<FrontEnd> front_end = new FrontEnd;
	path = gDirUtilp->add(mBackEnd.mBaseFolder, database_structure[fi_assets]);
	for (int sdi = 0; sdi < 16; ++sdi)
	{
	  std::string subdir = path + gDirUtilp->getDirDelimiter() + subdirs[sdi];
	  std::vector<std::string> files = gDirUtilp->getFilesInDir(subdir);
	  for (std::vector<std::string>::iterator filename = files.begin(); filename != files.end(); ++filename)
	  {
		// Reconstruct the full path.
		std::string filepath = subdir + gDirUtilp->getDirDelimiter() + *filename;

		// Make sure the filename looks like an md5 hash.
		if (filename->length() != 32 || filename->find_first_not_of(subdirs) != std::string::npos)
		{
		  llinfos << "Skipping alien file \"" << filepath << "\"." << llendl;
		  continue;
		}

		// Pretend we are about to upload this in order to determine the type, real hash value and delta.
		if (!front_end->prepare_upload(filepath, false, true))
		{
		  // Move asset files that are unreadable, corrupt or that we fail to determine the type or hash of, to lost+found.
		  move_to_lostfound(filepath, fi_assets, sdi);
		  continue;
		}

		// Get the type and calculated hash.
		LLAssetType::EType asset_type = front_end->getAssetType();
		LLMD5 md5 = front_end->getAssetHash();
		char digest[33];
		md5.hex_digest(digest);
		std::string hash_str = digest;

		std::string metapath = gDirUtilp->add(mBackEnd.mBaseFolder, database_structure[fi_by_asset_md5]);
		std::string meta_filename = metapath + gDirUtilp->getDirDelimiter() + hash_str[0] + gDirUtilp->getDirDelimiter() + hash_str + ".xml";

		// Check if the hash is still the same...
		if (*filename != hash_str ||
			(*filename)[0] != subdirs[sdi])		// and the file is in the right subdirectory.
		{
		  // Move it to the right place.
		  std::string new_subdir = path + gDirUtilp->getDirDelimiter() + hash_str[0];
		  std::string new_filepath = new_subdir + gDirUtilp->getDirDelimiter() + hash_str;
		  if (LLFile::isfile(new_filepath))
		  {
			llwarns << "Moving file \"" << filepath << "\", ";
			if (*filename != hash_str) { llcont << "which has the wrong name, "; }
			if ((*filename)[0] != subdirs[sdi]) { llcont << "which is in the wrong subdirectory, "; }
			llcont << "to \"" << mLostfoundDirname << "\" because \"" << new_filepath << "\" already exists!" << llendl;
			move_to_lostfound(filepath, fi_assets, sdi);
			continue;
		  }
		  llwarns << "Moving file \"" << filepath << "\" to \"" << new_filepath << "\"." << llendl;
		  AIFile::rename(filepath, new_filepath);
		  filepath = new_filepath;
		  mFixed = true;
		  if (*filename != hash_str)
		  {
			// The hash changed!
			// Check if the meta data exists.
			std::string old_meta_filename = metapath + gDirUtilp->getDirDelimiter() + (*filename)[0] + gDirUtilp->getDirDelimiter() + *filename + ".xml";
			if (LLFile::isfile(old_meta_filename))
			{
			  // Find it in the memory cache.
			  LLMD5 old_md5;
			  old_md5.clone(*filename);
			  AIUploadedAsset key(old_md5);
			  asset_map_type::iterator uploaded_asset_iter = gAssets_w->find(&key);
			  if (uploaded_asset_iter != gAssetsEnd)	// This should really always be true: we just read all asset meta data files into this set,
			  {											// or if they couldn't be parsed they were moved to lost+found.
				// Erase it from the memory cache.
				AIUploadedAsset* uploaded_asset = *uploaded_asset_iter;
				gAssets_w->erase(uploaded_asset_iter);
				// Change the hash value.
				AIUploadedAsset_wat uploaded_asset_w(*uploaded_asset);
				const_cast<LLMD5&>(uploaded_asset_w->mAssetMd5) = md5;
				try
				{
				  // Write the changed uploaded_asset to disk.
				  write_to_disk(uploaded_asset_w);
				  // Add it back to the memory cache (in the right place for the changed md5 hash).
				  gAssets_w->insert(uploaded_asset);
				}
				catch (AIAlert::Error const& error)
				{
				  llwarns << AIAlert::text(error) << llendl;
				  LLFile::remove(meta_filename);
				}
			  }
			  else
			  {
				llwarns << "Cannot find meta data with old hash " << old_md5 << " in memory cache !??!" << llendl;
			  }
			  if (LLFile::isfile(meta_filename))
			  {
				LLFile::remove(old_meta_filename);
				// Remember that this hash has changed into a new hash.
				hash_translation[old_md5] = md5;
			  }
			  else
			  {
				llwarns << "Failed to generate new meta data file \"" << meta_filename << "\" for new hash value." << llendl;
				move_to_lostfound(old_meta_filename, fi_assets, sdi);
				continue;
			  }
			}
		  }
		}

		// Find the asset hash in the memory cache.
		AIUploadedAsset* uploaded_asset = NULL;
		asset_map_type::iterator iter;
		{
		  AIUploadedAsset key(md5);
		  iter = gAssets_w->find(&key);
		  if (iter != gAssetsEnd)
		  {
			uploaded_asset = *iter;
		  }
		}

		if (!uploaded_asset)
		{
		  // Found an asset without meta data...
		  llwarns << "Found " << asset_type << " asset file " << hash_str << " without meta data! Moving it to " << mLostfoundDirname << llendl;
		  move_to_lostfound(filepath, fi_assets, sdi);
		  continue;
		}

		// Check if the type and delta in the meta data matches the one we decoded from the asset file.
		{
		  bool need_write = false;
		  AIUploadedAsset_wat uploaded_asset_w(*uploaded_asset);
		  LLPointer<Delta> delta = uploaded_asset_w->getDelta();
		  bool type_has_delta = asset_type == LLAssetType::AT_ANIMATION;

		  if ((delta && !type_has_delta) || (!delta && type_has_delta))
		  {
			llwarns << "There is a ";
			if (delta) { llcont << "very strange "; }
			llcont << "mismatch between the asset file \"" << filepath << "\" which appears to be a " << asset_type << " and should ";
			if (!type_has_delta) { llcont << "not "; }
			llcont << "have a 'delta', and its corresponding meta file \"" << meta_filename << "\" (with type " << uploaded_asset_w->getAssetType() << ") that ";
			if (delta) { llcont << "has "; } else { llcont << "does not have "; }
			llcont << "a delta stored.";
			if (delta) { llcont << " These two can not belong together."; } else { llcont << " Adding decoded delta to meta file."; }
			llcont << llendl;
			if (delta)
			{
			  // Move asset file and meta file to lost+found.
			  move_to_lostfound(filepath, fi_assets, sdi);
			  move_to_lostfound(meta_filename, fi_by_asset_md5, sdi);
			  gAssets_w->erase(iter);
			  continue;
			}
			else
			{
			  // Add the delta to the meta file.
			  uploaded_asset_w->mDelta = front_end->getDelta();
			  need_write = true;
			}
		  }
		  if (uploaded_asset_w->getAssetType() != asset_type)
		  {
			llwarns << "There is a very strange asset-type mismatch between the asset file \"" << filepath << "\" which appears to be a " << asset_type <<
				" and its corresponding meta file \"" << meta_filename << "\" with type " << uploaded_asset_w->getAssetType() << "; changing the latter to match..." << llendl;
			// Change the type of the meta file.
			uploaded_asset_w->mAssetType = asset_type;
			need_write = true;
		  }
		  if (uploaded_asset_w->mDelta && front_end->getDelta())
		  {
			bool equal = true;
			{
			  switch(asset_type)
			  {
				case LLAssetType::AT_ANIMATION:
				{
				  BVHAnimDelta* asset_delta = dynamic_cast<BVHAnimDelta*>(front_end->getDelta().get());
				  BVHAnimDelta* meta_delta = dynamic_cast<BVHAnimDelta*>(uploaded_asset_w->mDelta.get());
				  equal = !asset_delta || (meta_delta && meta_delta->equals(*asset_delta));
				}
				default:
				  break;
			  }
			}
			if (!equal)
			{
			  llwarns << "The delta information in the meta file \"" << meta_filename << "\" does not match the delta extracted from \"" << filepath << "\". Changing the former to match." << llendl;
			  uploaded_asset_w->mDelta = front_end->getDelta();
			  need_write = true;
			}
		  }
		  if (need_write)
		  {
			write_to_disk(uploaded_asset_w);
			mFixed = true;
		  }
		}
	  }
	}
	// Delete the FrontEnd.
	front_end = NULL;

	// Run over all collected meta data in the memory cache and check if we have an asset file for each of them.
	for (asset_map_type::iterator metadata = gAssets_w->begin(); metadata != gAssets_w->end();)
	{
	  AIUploadedAsset_wat uploaded_asset_w(**metadata);
	  LLMD5 asset_md5 = uploaded_asset_w->getAssetMd5();
	  std::string asset_filename = mBackEnd.getAssetFilename(asset_md5);
	  if (!LLFile::isfile(asset_filename))
	  {
		std::string meta_filename = mBackEnd.getUploadedAssetFilename(asset_md5);
		llwarns << "The meta data file \"" << meta_filename << "\" has no associated asset data file (\"" << asset_filename << "\" is missing). That is not very useful. Moving it out of the way." << llendl;
		move_to_lostfound(meta_filename, fi_assets);
		gAssets_w->erase(metadata++);
		continue;
	  }
	  ++metadata;
	}

	// Run over all by_source_md5 files.
	path = gDirUtilp->add(mBackEnd.mBaseFolder, database_structure[fi_by_source_md5]);
	for (int sdi = 0; sdi < 16; ++sdi)
	{
	  std::string subdir = path + gDirUtilp->getDirDelimiter() + subdirs[sdi];
	  std::vector<std::string> files = gDirUtilp->getFilesInDir(subdir);
	  for (std::vector<std::string>::iterator filename = files.begin(); filename != files.end(); ++filename)
	  {
		// Reconstruct the full path.
		std::string filepath = subdir + gDirUtilp->getDirDelimiter() + *filename;

		// Get the basename without extension.
		std::string hash_str = gDirUtilp->getBaseFileName(*filename, true);
		std::string ext = gDirUtilp->getExtension(*filename);
		// Make sure the filename looks like an md5 hash.
		if (ext != "xml" || hash_str.length() != 32 || hash_str.find_first_not_of(subdirs) != std::string::npos)
		{
		  llinfos << "Skipping alien file \"" << filepath << "\"." << llendl;
		  continue;
		}

		// Check if the file is in the right subdirectory.
		if (hash_str[0] != subdirs[sdi])
		{
		  // That way it would never be found, so move it to the right place.
		  std::string new_subdir = path + gDirUtilp->getDirDelimiter() + hash_str[0];
		  std::string new_filepath = new_subdir + gDirUtilp->getDirDelimiter() + *filename;
		  if (LLFile::isfile(new_filepath))
		  {
			llwarns << "Moving file \"" << filepath << "\", which is in the wrong subdirectory, to \"" << mLostfoundDirname << "\" because \"" << new_filepath << "\" already exists!" << llendl;
			move_to_lostfound(filepath, fi_by_source_md5, sdi);
			continue;
		  }
		  else
		  {
			llwarns << "Moving file \"" << filepath << "\" to \"" << new_filepath << "\"." << llendl;
			AIFile::rename(filepath, new_filepath);
			filepath = new_filepath;
		  }
		  mFixed = true;
		}

		// Try to decode the file.
		std::deque<LLMD5> md5s;
		try
		{
		  read_from_disk(md5s, filepath);
		}
		catch (AIAlert::Error const& error)
		{
		  AIAlert::add(error);
		  // Move the file to lost+found and skip to the next file on any error.
		  try
		  {
			move_to_lostfound(filepath, fi_by_source_md5, sdi);
		  }
		  catch (AIAlert::Error const& error)
		  {
			AIAlert::add(error);
		  }
		  continue;
		}

		// Reconstruct a list of verified md5s.
		std::deque<LLMD5> new_md5s;
		bool changed = false;
		for (std::deque<LLMD5>::iterator iter = md5s.begin(); iter != md5s.end(); ++iter)
		{
		  LLMD5 md5 = *iter;
		  std::map<LLMD5, LLMD5>::iterator translation_iter = hash_translation.find(md5);
		  if (translation_iter != hash_translation.end())
		  {
			md5 = translation_iter->second;
			llinfos << "Updating the file \"" << filepath << "\" which contained a reference to " << *iter << " which has changed into " << md5 << "." << llendl;
			changed = true;
		  }
		  AIUploadedAsset* uploaded_asset = NULL;
		  asset_map_type::iterator uai;
		  {
			AIUploadedAsset key(md5);
			uai = gAssets_w->find(&key);
			if (uai != gAssetsEnd)
			{
			  uploaded_asset = *uai;
			}
		  }
		  if (uploaded_asset)
		  {
			AIUploadedAsset_wat uploaded_asset_w(*uploaded_asset);
			LLMD5 source_hash;
			source_hash.clone(hash_str);
			if (!uploaded_asset_w->mSourceMd5.isFinalized())
			{
			  llwarns << "The file \"" << filepath << "\" contains a reference to " << md5 << " that had no source hash! Added." << llendl;
			  uploaded_asset_w->setSource("unknown (database recovery)", LLDate::now(), source_hash);
			  write_to_disk(uploaded_asset_w);
			  mFixed = true;
			  new_md5s.push_back(md5);
			}
			else if (uploaded_asset_w->mSourceMd5 == source_hash)
			{
			  new_md5s.push_back(md5);
			  // If there are more than one asset hash values, but the type is one-on-one, then just use the first one.
			  if (uploaded_asset_w->getAssetType() != LLAssetType::AT_ANIMATION)
			  {
				if (md5s.size() > 1)
				{
				  llwarns << "The file \"" << filepath << " contains more than one reference, but is of a the one-on-one type " << uploaded_asset_w->getAssetType() << "!" << llendl;
				  llwarns << "Removing the references to:";
				  for (++iter; iter != md5s.end(); ++iter)
				  {
					md5 = *iter;
					translation_iter = hash_translation.find(md5);
					if (translation_iter != hash_translation.end())
					{
					  md5 = translation_iter->second;
					}
					llcont << " " << md5;
				  }
				  llcont << llendl;
				  changed = true;
				}
				break;
			  }
			}
			else
			{
			  llwarns << "The file \"" << filepath << "\" contains a reference to " << md5 << " that refers to a different source! Removed." << llendl;
			  changed = true;
			}
		  }
		  else
		  {
			llwarns << "The file \"" << filepath << "\" contains a reference to " << md5 << " that (no longer) exists. Removed." << llendl;
			changed = true;
		  }
		}
		if (changed)
		{
		  if (new_md5s.empty())
		  {
			LLFile::remove(filepath);
		  }
		  else
		  {
			write_to_disk(new_md5s, filepath);
		  }
		  mFixed = true;
		}
	  }
	}

	// Run over all collected meta data in the memory cache and check that the source reference exists.
	for (asset_map_type::iterator metadata = gAssets_w->begin(); metadata != gAssets_w->end(); ++metadata)
	{
	  AIUploadedAsset_wat uploaded_asset_w(**metadata);
	  LLMD5 asset_md5 = uploaded_asset_w->getAssetMd5();
	  LLMD5 source_md5 = uploaded_asset_w->getSourceMd5();
	  if (source_md5.isFinalized())
	  {
		bool changed = false;
		std::deque<LLMD5> md5s;
		std::string source_filename = mBackEnd.getSourceFilename(source_md5);
		if (!LLFile::isfile(source_filename))
		{
		  llwarns << "Creating non-existed source hash -> meta data file \"" << source_filename << "\", referenced in meta file of " << asset_md5 << "." << llendl;
		  md5s.push_back(asset_md5);
		  changed = true;
		}
		else
		{
		  read_from_disk(md5s, source_filename);
		  if (std::find(md5s.begin(), md5s.end(), asset_md5) == md5s.end())
		  {
			llwarns << "Adding " << asset_md5 << " to source hash -> meta data file \"" << source_filename << "\"." << llendl;
			md5s.push_back(asset_md5);
			changed = true;
		  }
		}
		if (changed)
		{
		  if (md5s.empty())
		  {
			AIFile::remove(source_filename);
		  }
		  else
		  {
			write_to_disk(md5s, source_filename);
		  }
		}
	  }
	}

	// Run over all by_uuid files.
	path = gDirUtilp->add(mBackEnd.mBaseFolder, database_structure[fi_by_uuid]);
	for (int sdi = 0; sdi < 16; ++sdi)
	{
	  std::string subdir = path + gDirUtilp->getDirDelimiter() + subdirs[sdi];
	  std::vector<std::string> files = gDirUtilp->getFilesInDir(subdir);
	  for (std::vector<std::string>::iterator filename = files.begin(); filename != files.end(); ++filename)
	  {
		// Reconstruct the full path.
		std::string filepath = subdir + gDirUtilp->getDirDelimiter() + *filename;

		// Get the basename without extension.
		std::string uuid_str = gDirUtilp->getBaseFileName(*filename, true);
		std::string ext = gDirUtilp->getExtension(*filename);
		// Make sure the filename looks like an UUID hash.
		if (ext != "xml" || uuid_str.length() != 36 || !LLUUID::validate(uuid_str))
		{
		  llinfos << "Skipping alien file \"" << filepath << "\"." << llendl;
		  continue;
		}

		// Check if the file is in the right subdirectory.
		if (uuid_str[0] != subdirs[sdi])
		{
		  // That way it would never be found, so move it to the right place.
		  std::string new_subdir = path + gDirUtilp->getDirDelimiter() + uuid_str[0];
		  std::string new_filepath = new_subdir + gDirUtilp->getDirDelimiter() + *filename;
		  if (LLFile::isfile(new_filepath))
		  {
			llwarns << "Moving file \"" << filepath << "\", which is in the wrong subdirectory, to \"" << mLostfoundDirname << "\" because \"" << new_filepath << "\" already exists!" << llendl;
			move_to_lostfound(filepath, fi_by_uuid, sdi);
			continue;
		  }
		  else
		  {
			llwarns << "Moving file \"" << filepath << "\" to \"" << new_filepath << "\"." << llendl;
			AIFile::rename(filepath, new_filepath);
			filepath = new_filepath;
		  }
		  mFixed = true;
		}

		// Try to decode the file.
		LLMD5 asset_hash;
		try
		{
		  read_from_disk(asset_hash, filepath);
		}
		catch (AIAlert::Error const& error)
		{
		  AIAlert::add(error);
		  // Move the file to lost+found and skip to the next file on any error.
		  try
		  {
			move_to_lostfound(filepath, fi_by_uuid, sdi);
		  }
		  catch (AIAlert::Error const& error)
		  {
			AIAlert::add(error);
		  }
		  continue;
		}

		// Convert the name to a LLUUID.
		LLUUID uuid;
		uuid.set(uuid_str);

		// Verified if the hash is usable.
		bool changed = false;
		bool valid = false;
		std::map<LLMD5, LLMD5>::iterator translation_iter = hash_translation.find(asset_hash);
		if (translation_iter != hash_translation.end())
		{
		  llinfos << "Updating the file \"" << filepath << "\" which contained a reference to " << asset_hash << " which has changed into " << translation_iter->second << "." << llendl;
		  asset_hash = translation_iter->second;
		  changed = true;
		}
		AIUploadedAsset* uploaded_asset = NULL;
		asset_map_type::iterator uai;
		{
		  AIUploadedAsset key(asset_hash);
		  uai = gAssets_w->find(&key);
		  if (uai != gAssetsEnd)
		  {
			uploaded_asset = *uai;
		  }
		}
		if (uploaded_asset)
		{
		  AIUploadedAsset_wat uploaded_asset_w(*uploaded_asset);
		  for (grid2uuid_map_type::iterator iter = uploaded_asset_w->mAssetUUIDs.begin(); iter != uploaded_asset_w->mAssetUUIDs.end(); ++iter)
		  {
			if (iter->getUUID() == uuid)
			{
			  valid = true;
			  break;
			}
		  }
		  if (!valid)
		  {
			llwarns << "The file \"" << filepath << "\" contains a reference to " << asset_hash << " that lacks this UUID! Removing because we don't know the grid." << llendl;
			changed = true;
		  }
		}
		else
		{
		  llwarns << "The file \"" << filepath << "\" contains a reference to " << asset_hash << " that (no longer) exists. Removed." << llendl;
		  changed = true;
		}

		if (changed)
		{
		  if (!valid)
		  {
			move_to_lostfound(filepath, fi_by_uuid, sdi);
		  }
		  else
		  {
			write_to_disk(asset_hash, filepath);
		  }
		  mFixed = true;
		}
	  }
	}

	// Run over all collected meta data in the memory cache and check that the uuid reference exists.
	for (asset_map_type::iterator metadata = gAssets_w->begin(); metadata != gAssets_w->end(); ++metadata)
	{
	  AIUploadedAsset_wat uploaded_asset_w(**metadata);
	  // Write the UUID map file(s).
	  for (grid2uuid_map_type::iterator iter = uploaded_asset_w->mAssetUUIDs.begin(); iter != uploaded_asset_w->mAssetUUIDs.end(); ++iter)
	  {
		// Write the uuid lookup file (fi_by_uuid).
		std::string uuid_filename = mBackEnd.getUUIDFilename(iter->getUUID());
		// Never overwrite an existing uuid mapping.
		if (!LLAPRFile::isExist(uuid_filename))
		{
		  llwarns << "Meta data for " << uploaded_asset_w->mAssetMd5 << " lists the UUID " << iter->getUUID() << " but its reference file does not exist! Creating \"" << uuid_filename << "\"." << llendl;
		  write_to_disk(uploaded_asset_w->mAssetMd5, uuid_filename);
		  mFixed = true;
		}
	  }
	}

  }
  catch (AIAlert::ErrorCode const& error)
  {
	throw;
  }
  catch (AIAlert::Error const& error)
  {
	THROW_ALERTC(corrupt_database, error);
  }

  if (!mLostfound_used)
  {
	LLFile::rmdir(mLostfoundDirname);
  }

  return mFixed;
}

} // namespace AIMultiGrid

