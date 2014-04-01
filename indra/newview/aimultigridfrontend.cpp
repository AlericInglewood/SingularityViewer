/**
 * @file aimultigridfrontend.cpp
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
 *   Created aimultigridfrontend.cpp (AI)
 */

#include "llviewerprecompiledheaders.h"
#include <algorithm>
#include "aimultigridfrontend.h"
#include "aimultigridbackend.h"
#include "aifetchinventoryfolder.h"
#include "aialert.h"
#include "aifile.h"
#include "llviewerstats.h"
#include "llappviewer.h"
#include "llagent.h"
#include "llstatusbar.h"
#include "hippogridmanager.h"
#include "llfloaterbuycurrency.h"
#include "lltrans.h"
#include "llnotificationsutil.h"
#include "lldir.h"
#include "llviewertexturelist.h"
#include "llvorbisencode.h"
#include "llviewerregion.h"
#include "llassetuploadresponders.h"
#include "llvfile.h"
#include "llassetstorage.h"
#include "lluploaddialog.h"
#include "message.h"
#include "message_prehash.h"
#include "lluploaddialog.h"
#include "lltransactiontypes.h"
#include "llinventoryfunctions.h"
#include "aioggvorbisverifier.h"
#include "aianimverifier.h"
#include "aibvhanimdelta.h"
#include "aitexturedelta.h"
#include "llvoavatarself.h"
#include "llviewerobjectlist.h"
#include "llfloaterbvhpreview.h"
#include "llimagej2c.h"
#include "aimultigridwearable.h"

// This statemachine replaces Linden Labs 'upload_new_resource'.
//
// Usage
//
// Where the old code called,
//
//   upload_new_resource(
//           tid, asset_type,			// Or, instead of these two, a filename is passed. See below.
//           name, desc, compression_info, destination_folder_type, inv_type, next_owner_perms, group_perms, everyone_perms, display_name,
//           udp_callback,				// void (*)(LLUUID const& asset_id, void* user_data, S32 status, LLExtStat ext_status)
//           expected_upload_cost,
//           userdata);
//
// You now would do,
//
//   LLPointer<AIMultiGrid::FrontEnd> mg_front_end = new AIMultiGrid::FrontEnd;
//   mg_front_end->upload_new_resource(
//           tid, asset_type,		// Same as above.
//           name, desc, compression_info, destination_folder_type, inv_type, next_owner_perms, group_perms, everyone_perms, display_name,	// Same as above.
//           expected_upload_cost,
//           user_callback,			// void (*)(bool, void*)
//           userdata,
//           AIMultiGrid::UploadResponderFactory<LLNewAgentInventoryResponder>());
//
// The state machine runs itself: there is no need to call mg_front_end->run() after this as you'd do with normal statemachines.
//
// Note that passing 'udp_callback' is no longer needed! The user_callback is a different callback and entirely optional.
// The parameters of user_callback are: void cb(bool success, void* userdata) where 'success' is true iff the upload succeeded.
// You can pass NULL, NULL if you don't need a call back.
//
// The last parameter, as shown, gives the old behavior, but allows one to replace the responder with
// a class derived from LLNewAgentInventoryResponder which has one or more virtual methods overridden.
// For example,
//
//   class MyResponder : public LLNewAgentInventoryResponder {
//     public:
//       MyResponder(LLSD const& post_data, LLUUID const& vfile_id, LLAssetType::EType asset_type, Callback callback, void* userdata) :
//           LLNewAgentInventoryResponder(post_data, vfile_id, asset_type, callback, userdata) { }		// This is NOT the userdata (or user_callback) you passed above!
//       /*virtual*/ void uploadComplete(LLSD const& content);                                          // Overridden method.
//       /*virtual*/ char const* getName(void) const { return "myResponder"; }
//   };
//
// and then pass as last parameter to upload_new_resource: AIMultiGrid::UploadResponderFactory<MyResponder>().
//
//
// Secondly, where the old code called,
//
//   upload_new_resource(
//           src_filename,
//           name, desc, compression_info, destination_folder_type, inv_type, next_owner_perms, group_perms, everyone_perms, display_name,
//           udp_callback,				// void (*)(LLUUID const& asset_id, void* user_data, S32 status, LLExtStat ext_status)
//           expected_upload_cost,
//           userdata);
//
// after the user clicked 'Upload' in a preview floater, you now would do, where the floater is being built:
//
//   LLPointer<AIMultiGrid::FrontEnd> mg_front_end = new AIMultiGrid::FrontEnd;
//   if (mg_front_end->prepare_upload(src_filename, false))	// Passing false because this is not a bulk upload.
//   {
//     LLFloaterFooPreview* floaterp = new LLFloaterFooPreview(mg_front_end);
//     LLUICtrlFactory::getInstance()->buildFloater(floaterp, "floater_foo_preview.xml");
//   }
//
// Note that all preview floaters are derived from LLFloaterNameDesc which in turn gets
// the front_end passed and takes care of the rest (and actually, all of the above--
// asking for a file name, showing the preview floater, etc-- is now taken care of
// by AIFileUpload).
//
//
// If no preview floater is to be shown (just a forced upload, see for example LLFileUploadBulk),
// then you'd do:
//
//   LLPointer<AIMultiGrid::FrontEnd> mg_front_end = new AIMultiGrid::FrontEnd;
//   if (mg_front_end->prepare_upload(src_filename, bulk_upload))
//   {
//   #if 0
//     // Optionally, see here if the source file was already uploaded before.
//     try
//     {
//       mg_front_end->find_previous_uploads();
//     }
//     catch(AIAlert::Error const& error)
//     {
//       // Failure.
//     }
//     if (mg_front_end->uploaded_before())
//     {
//       .. do stuff ..
//     }
//   #endif
//     // Actually upload it.
//     mg_front_end->upload_new_resource_continued(
//           name, desc, compression_info, destination_folder_type, inv_type, next_owner_perms, group_perms, everyone_perms, display_name,  // Same as above.
//           expected_upload_cost);
//   }
//
// Again, udp_callback and its userdata have dropped out and are taken care of internally.

namespace AIMultiGrid {

//-----------------------------------------------------------------------------
// FrontEnd

char const* FrontEnd::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
	AI_CASE_RETURN(FrontEnd_lockDatabase);
	AI_CASE_RETURN(FrontEnd_lockedDatabase);
	AI_CASE_RETURN(FrontEnd_findUploadedAsset);
	AI_CASE_RETURN(FrontEnd_uploadedAssetFound);
	AI_CASE_RETURN(FrontEnd_fetchInventory);
	AI_CASE_RETURN(FrontEnd_findInInventory);
	AI_CASE_RETURN(FrontEnd_uploadNewResource);
	AI_CASE_RETURN(FrontEnd_doUpload);
	AI_CASE_RETURN(FrontEnd_registerUpload);
	AI_CASE_RETURN(FrontEnd_unlockDatabase);
	AI_CASE_RETURN(FrontEnd_done);
  }
  llassert(false);
  return "UNKNOWN STATE";
}

FrontEnd::FrontEnd(CWD_ONLY(bool debug)) :
#ifdef CWDEBUG
	AIStateMachine(debug),
#endif
	mAssetType(LLAssetType::AT_NONE), mBulkUpload(false), mCollectedDataMask(0), mUploadedAsset(NULL), mThreadLock(new DatabaseThreadLock(mFileLock))
{
}

void FrontEnd::setSourceFilename(std::string const& source_filename)
{
  llassert_always(!source_filename.empty());
  mSourceFilename = source_filename;
  mCollectedDataMask |= source_filename_bit;
}

void FrontEnd::setSourceHash(LLMD5 const& source_md5)
{
  llassert_always(source_md5.isFinalized());
  mSourceHash = source_md5;
  mCollectedDataMask |= source_hash_bit;
}

void FrontEnd::setAssetHash(LLMD5 const& asset_md5, LLPointer<Delta> const& delta)
{
  mSourceHashType = delta ? one_source_many_assets : one_on_one;
  mDelta = delta;
  llassert_always(asset_md5.isFinalized());
  mAssetHash = asset_md5;
  mCollectedDataMask |= asset_hash_bit;
  // For textures we toggle the asset hash between lossy and lossless (for small enough texture).
  // This function can therefore be called more than once and we have to reset the associated
  // assets found with find_previous_uploads().
  mUploadedAsset = NULL;
  mUploadedAssets.clear();
}

void FrontEnd::callback(LLUUID const& uuid, void* userdata, bool success)
{
  FrontEnd* self = static_cast<FrontEnd*>(userdata);

  // This was an HTTP upload.
  llassert(self->mHttpUpload);

  // Do user callback if any.
  if (self->mUserCallback)
  {
	(*self->mUserCallback)(success, self->mUserData);
  }

  self->uploadFinished(success, uuid);
}

void FrontEnd::legacyCallback(LLUUID const& uuid, void* userdata, S32 result, LLExtStat ext_status)
{
  FrontEnd* self = static_cast<FrontEnd*>(userdata);

  // Legacy upload (including temporaries).
  llassert(!self->mHttpUpload);

  // Do user callback if any.
  if (self->mUserCallback)
  {
	// Currently only used to call back LLSnapshotLivePreview::saveTextureDone.
	(*self->mUserCallback)(result >= 0, self->mUserData);
  }

  // Continue with default legacy callbacks.
  if (self->mTemporary)
  {
	self->temp_upload_callback(uuid, result, ext_status);
  }
  else
  {
	self->upload_done_callback(uuid, result, ext_status);
  }

  bool success = result >= 0;
  self->uploadFinished(success, uuid);
}

void FrontEnd::uploadFinished(bool success, LLUUID const& uuid)
{
  if (!LLApp::isRunning())
  {
	// If the application isn't running then the statemachine engine might already be destructed.
	return;
  }

  if (!success)
  {
	Dout(dc::notice, "FrontEnd::uploadFinished: upload failed.");
	abort();
  }
  else
  {
	Dout(dc::notice, "FrontEnd::uploadFinished(" << uuid << ", ...)");
	mNewUUID = uuid;
	// Only store supported assets in the database, that are permanent of course.
	bool store_in_database = (mCollectedDataMask & asset_hash_bit) && !mTemporary;
	advance_state(store_in_database ? FrontEnd_registerUpload : (mLocked ? FrontEnd_unlockDatabase : FrontEnd_done));
  }
}

void increase_new_upload_stats(LLAssetType::EType asset_type)
{
  if (LLAssetType::AT_SOUND == asset_type)
  {
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_UPLOAD_SOUND_COUNT);
  }
  else if (LLAssetType::AT_TEXTURE == asset_type)
  {
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_UPLOAD_TEXTURE_COUNT);
  }
  else if (LLAssetType::AT_ANIMATION == asset_type)
  {
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_UPLOAD_ANIM_COUNT);
  }
}

LLAssetID generate_asset_id_for_new_upload(LLTransactionID const& tid)
{
  LLAssetID uuid;

  if (gDisconnected)
  {
	uuid.setNull();
  }
  else
  {
	uuid = tid.makeAssetID(gAgent.getSecureSessionID());
  }

  return uuid;
}

bool FrontEnd::checkFunds(void) const
{
  // Check for adequate funds.
  if (LLAssetType::AT_SOUND == mAssetType ||
	  LLAssetType::AT_TEXTURE == mAssetType ||
	  LLAssetType::AT_ANIMATION == mAssetType)
  {
	S32 balance = gStatusBar->getBalance();
	if (balance < mExpectedUploadCost)
	{
	  // Insufficient funds, bail on this upload.
	  LLStringUtil::format_map_t args;
	  args["[NAME]"] = mName;
	  args["[CURRENCY]"] = mHippoGrid->getCurrencySymbol();
	  args["[AMOUNT]"] = llformat("%d", mExpectedUploadCost);
	  LLFloaterBuyCurrency::buyCurrency(LLTrans::getString("UploadingCosts", args), mExpectedUploadCost);
	  return false;
	}
  }
  return true;
}

void FrontEnd::upload_error(char const* label, AIArgs const& args, AIAlert::Error const& error)
{
  AIAlert::add(label, args, error);
  if (mCreatedTempFile && LLFile::remove(mTmpFilename) == -1)
  {
	lldebugs << "unable to remove temp file" << llendl;
  }
}

void FrontEnd::upload_error(char const* label, AIArgs const& args)
{
  AIAlert::add(label, args);
  if (mCreatedTempFile && LLFile::remove(mTmpFilename) == -1)
  {
	lldebugs << "unable to remove temp file" << llendl;
  }
}

// Determine mAssetType from file magic or extension and calculate the source
// and asset hash for bulk uploads and one-on-one sources.
//
// For bulk uploads FrontEnd::upload_new_resource_continued must be called upon return.
// That is also allowed when this is a native file.
//
// If we are called from AIFileUpload::filepicker_callback (FloaterLocalAssetBrowser::onClickUpload)
// a LLFloater*Preview (LLFloaterImagePreview) will be spawned that will call FrontEnd::upload_new_resource_continued
// when the user clicks "Upload".
//
// The preview floaters for asset types with delta (animations and textures) are responsible to call
// FrontEnd::setSourceHash once, and FrontEnd::setAssetHash when the upload is committed.
bool FrontEnd::prepare_upload(std::string const& src_filename, bool bulk_upload, bool repair_database)
{
  // Reset things in case this front end is being reused.
  mCollectedDataMask = 0;
  mUploadedAsset = NULL;

  // Initialize.
  mAssetType = LLAssetType::AT_NONE;
  mCreatedTempFile = false;
  mBulkUpload = bulk_upload;
  mNativeFormat = false;
  setSourceFilename(src_filename);

  LLSD args;
  std::string error_message;

  std::string exten = gDirUtilp->getExtension(src_filename);

  mCodec = LLImageBase::getCodecFromExtension(exten);

  // Try to open the file and read it's magic to determine the type, instead of relying on file extension.
  // Set mAssetType accordingly.
  mAssetType = LLAssetType::AT_NONE;
  char magic[256];
  int len;
  try
  {
	AIFile infile(src_filename, "rb");
	len = fread(magic, 1, sizeof(magic), infile);
  }
  catch (AIAlert::Error const& error)
  {
	AIAlert::add_modal("AIProblemWithFile", AIArgs("[FILE]", src_filename), error);
	return false;
  }
  // If the file is shorter than 4 bytes, then it is nothing (AT_NONE).
  // Note: tga files cannot be recognized by their first 4 bytes.
  if (len >= 4  && exten != "tga")
  {
    bool maybe_lsl_text = false;          // This is set when we detect LSL text but we're not sure about it.
    int const number_of_keywords = 10;
    char const* keywords[number_of_keywords] = { "default", "state", "float", "integer", "key", "list", "quaternion", "rotation", "string", "vector" };
    int remaining_len = len;
    int kw, pos;
    char* data;
    // A lot of asset and source files could also be LSL text, because that can start with a function name.
    // In order to tell the difference we have to start with detecting LSL text and be very demanding,
    // so that we won't think it is LSL text when in fact it is something else.
    int wsskip = 0; // The position of the first non-whitespace character.
    // Start with skipping leading white-space.
    while (wsskip < len && (magic[wsskip] == '\n' || magic[wsskip] == '\t' || magic[wsskip] == ' '))
    {
      ++wsskip;
    }
    // If there is less than 4 bytes left after that (meaning we saw leading white space), then assume it is LSL text.
    if (len - wsskip < 4)
    {
      goto detected_lsl_text;
    }
    // Let data point to the first non-whitespace character.
    data = &magic[wsskip];
    // If the file starts with a C-style comment, then it has to be LSL text.
    if (data[0] == '/' && (data[1] == '/' || data[1] == '*'))
    {
      goto detected_lsl_text;
    }
    // Test for LSL keywords.
    remaining_len -= wsskip;
    kw = 0;
    while (kw < 10)
    {
      if (strncmp(data, keywords[kw], remaining_len) == 0)
      {
        // Every other asset file starts with well-defined magic, not equal to (the first 4 characters of) any of these keywords.
        goto detected_lsl_text;
      }
      ++kw;
    }
    // Test for user defined functions without return type.
    // If the file doesn't start with white space in this case, then we can never be sure this is LSL text.
    maybe_lsl_text = !wsskip;
    // Function name.
    pos = 0;
    while (pos < remaining_len && (std::isalnum(data[pos]) || data[pos] == '_'))
    {
      ++pos;
    }
    if (pos == 0)
    {
      // A function name exists of at least one character.
      goto not_lsl_text;
    }
    // If the opening paren (or following white space) happens within the first four bytes,
    // then it can't be anything but LSL text (I never saw a bitmap that started with "BM(").
    maybe_lsl_text = !wsskip && pos >= 4;
    // Gobble up white space.
    while (pos < remaining_len && (data[pos] == '\n' || data[pos] == '\t' || data[pos] == ' '))
    {
      ++pos;
    }
    // Nothing more to test?
    if (pos == remaining_len)
    {
      // If this started with white space then it has to be LSL text really (otherwise it's just damn likely).
      goto detected_lsl_text;
    }
    // If next we see a C-style comment, then assume it is LSL text (ugh)...
    if (pos + 1 < remaining_len && data[pos] == '/' && (data[pos + 1] == '/' || data[pos + 1] == '*'))
    {
      goto detected_lsl_text;
    }
    // Otherwise, the next character must be the opening parenthesis of a function declaration.
    if (data[pos] != '(')
    {
      goto not_lsl_text;
    }
    // Skip the paren and following white space.
    do
    {
      ++pos;
    }
    while (pos < remaining_len && (data[pos] == '\n' || data[pos] == '\t' || data[pos] == ' '));
    remaining_len -= pos;
    // If now there is less nothing remaining then we still can't be 100% sure,
    // but it starts to be extremely likely that this is not any of the other asset types...
    // Otherwise we demand that there is a closing paren, or a type.
    if (remaining_len == 0 || data[pos] == ')')
    {
      goto detected_lsl_text;
    }
    kw = 2;     // Skip 'default' and 'state'.
    while (kw < 10)
    {
      if (strncmp(data, keywords[kw], remaining_len) == 0)
      {
        goto detected_lsl_text;
      }
      ++kw;
    }
not_lsl_text:
    maybe_lsl_text = false;
    goto lsl_text_done;
detected_lsl_text:
    if (!maybe_lsl_text)
    {
      mAssetType = LLAssetType::AT_LSL_TEXT;
    }
lsl_text_done:
    if (mAssetType == LLAssetType::AT_LSL_TEXT)
    {
      // Apparently this is 100% sure LSL text!
      mNativeFormat = true;
    }
    // Otherwise, see if it maybe it looks like another type.
    // First try to detect the text based assets.
    else if (len >= 19 && strncmp(magic, "Linden text version", 19) == 0)        // Note cards start with 'Linden text version 2'.
    {
      // Most likely a note card.
      mAssetType = LLAssetType::AT_NOTECARD;
	  mNativeFormat = true;
    }
    else if (len >= 16 && strncmp(magic, "Landmark version", 16) == 0)           // Landmarks start with 'Landmark version 2'.
    {
      // Most likely a landmark.
      mAssetType = LLAssetType::AT_LANDMARK;
	  mNativeFormat = true;
    }
    else if (std::isdigit(magic[0]) && magic[1] == '\n' && std::isdigit(magic[2]) &&
             (std::isdigit(magic[3]) || magic[3] == '\n'))  // Gestures start with "2\n<key>\n" where <key> is an U8 number.
    {
      // Most likely a gesture.
      mAssetType = LLAssetType::AT_GESTURE;
	  mNativeFormat = true;
    }
    else if (len >= 18 && strncmp(magic, "LLWearable version", 18) == 0)         // Body parts and clothing start with 'LLWearable version 22'.
    {
      // The only way to find out what this is is to open the file again and read more.
      // Just set the type to body part because both are treated in the same way anyway.
      mAssetType = LLAssetType::AT_BODYPART;        // or AT_CLOTHING.
	  mNativeFormat = true;
    }
	// Do a best guess (being optimistic) based on the magic (using http://www.garykessler.net/library/file_sigs.html).
    else if (strncmp(magic, "RIFF", 4) == 0)
	{
	  // Possibly a RIFF/WAVE file.
	  mAssetType = LLAssetType::AT_SOUND;
	}
	else if (strncmp(magic, "OggS", 4) == 0)
	{
	  // Possibly a Ogg/Vorbis file.
	  mAssetType = LLAssetType::AT_SOUND;
	  mNativeFormat = true;
	}
	else if (strncmp(magic, "\211PNG", 4) == 0)
	{
	  // Possibly a Portable Network Graphics file file.
	  mAssetType = LLAssetType::AT_TEXTURE;
	  mCodec = IMG_CODEC_PNG;
	}
	else if (strncmp(magic, "BM", 2) == 0)
	{
	  // Possibly a Windows (.bmp), or Device-Independent (.dib), Bitmap image.
	  mAssetType = LLAssetType::AT_TEXTURE;
	  mCodec = IMG_CODEC_BMP;
	}
	else if (strncmp(magic, "\377\330\377\340", 4) == 0)
	{
	  // Possibly a JPEG/JFIF file.
	  mAssetType = LLAssetType::AT_TEXTURE;
	  mCodec = IMG_CODEC_JPEG;
	}
	else if (strncmp(magic, "\377O\377Q", 4) == 0)
	{
	  // Possibly a JPEG 2000 stream.
	  mAssetType = LLAssetType::AT_TEXTURE;
	  mNativeFormat = true;
	  mCodec = IMG_CODEC_J2C;
	}
	else if (len >= 9 && strncmp(magic, "HIERARCHY", 9) == 0)
	{
	  // Might be a BioVision Hierarchy (bvh) file, which starts with the word HIERARCHY.
	  mAssetType = LLAssetType::AT_ANIMATION;
	}
	else if (strncmp(magic, "\001\000\000\000", 4) == 0 ||		// KEYFRAME_MOTION_VERSION = 1, KEYFRAME_MOTION_SUBVERSION = 0
			 strncmp(magic, "\000\000\001\000", 4) == 0)		// Version 0, subversion 1 (old version)
	{
	  // Might be a .anim file.
	  mAssetType = LLAssetType::AT_ANIMATION;
	  mNativeFormat = true;
	}
    if (maybe_lsl_text)
    {
      if (mAssetType == LLAssetType::AT_NONE ||
          (repair_database && !mNativeFormat))      // There are no non-native formats in the asset database.
      {
        // We couldn't detect any other type, so it's safe to assume it's indeed LSL text.
        mAssetType = LLAssetType::AT_LSL_TEXT;
        mNativeFormat = true;
      }
      else
      {
        // Ok, we have a problem -- this looked like LSL text, but it also looks like another type!
        // I don't believe this is possible except for RIFF files that might start like 'RIFF(',
        // although even that is already pretty unlikely to be followed by '//', an lsl type or ')'.
        // In other words, this is VERY likely an LSL text file. Therefore we'll just look at
        // the first 256 bytes and "only" demand that 90% looks like ASCII and is printable.
        int printable_ascii = 0;
        for (int i = 0; i < len; ++i)
        {
          if ((magic[i] & 0x80) == 0 && std::isprint(magic[i]))
          {
            ++printable_ascii;
          }
        }
        if (printable_ascii * 1.1 >= len)
        {
          mAssetType = LLAssetType::AT_LSL_TEXT;
        }
      }
    }
  }
  if (mAssetType == LLAssetType::AT_NONE)
  {
    if (repair_database)
    {
      // Failed to determine the type; move the asset to lost+found. This very abnormal, so print a warning.
      llwarns << "Failed to determine the asset type of \"" << src_filename << "\". This is not normal." << llendl;
      return false;
    }
	// The magic failed; use the file extension.
	if (exten.empty())
	{
      AIAlert::add_modal("AINoFileExtension", AIArgs("[FILE]", gDirUtilp->getBaseFileName(src_filename)));
	  return false;
	}
	else if (mCodec != IMG_CODEC_INVALID)
	{
	  // Image
	  mAssetType = LLAssetType::AT_TEXTURE;
	  mNativeFormat = mCodec == IMG_CODEC_J2C;
	}
	else if (exten == "wav")
	{
	  mAssetType = LLAssetType::AT_SOUND;
	}
	else if (exten == "ogg")
	{
	  mAssetType = LLAssetType::AT_SOUND;
	  mNativeFormat = true;
	}
	else if (exten == "bvh")
	{
	  mAssetType = LLAssetType::AT_ANIMATION;
	}
	else if (exten == "anim" || exten == "animatn")
	{
	  mAssetType = LLAssetType::AT_ANIMATION;
	  mNativeFormat = true;
	}
	else
	{
	  // Unknown extension.
	  AIAlert::add_modal("AIUnknownFileExtension", AIArgs("[EXTENSION]", exten));
	  return false;
	}
  }

  if (!mNativeFormat)
  {
    if (repair_database)
    {
      AIAlert::add_modal("AINotNativeMagic", AIArgs("[FILE]", src_filename));
      return false;
    }
	mTmpFilename = gDirUtilp->getTempFilename();
  }

  if (mAssetType == LLAssetType::AT_TEXTURE && mNativeFormat)
  {
	LLMD5 md5;
	LLPointer<TextureDelta> delta;
	if (!verifyUploadFile(src_filename, true, md5, delta))
	{
	  upload_error("AIProblemWithFile_ERROR", AIArgs("[FILE]", src_filename)("[ERROR]", LLImage::getLastError()));
	  return false;
	}
    if (!repair_database)
    {
      setSourceHash(md5);
    }
	setAssetHash(md5, delta);
  }
  else if (mAssetType == LLAssetType::AT_TEXTURE)
  {
	if (bulk_upload)
	{
	  // For bulk uploads make lossless the default for TGA, and lossy the default for other formats.
	  // Otherwise leave LosslessJ2CUpload alone so it doesn't change over sequential manual uploads.
	  gSavedSettings.setBOOL("LosslessJ2CUpload", mCodec == IMG_CODEC_TGA);
	}
	LLMD5 source_md5, asset_md5;
	// It's an image file, the upload procedure is the same for all.
	LLPointer<AIMultiGrid::TextureDelta> delta;
	bool success = false;
	LLPointer<LLImageRaw> raw_image = createRawImage(src_filename, mCodec, source_md5);
	if (raw_image && createJ2CUploadFile(raw_image, asset_md5, delta))
	{
	  success = true;
	}
	if (!success)
	{
	  upload_error("AIProblemWithFile_ERROR", AIArgs("[FILE]", src_filename)("[ERROR]", LLImage::getLastError()));
	  return false;
	}
	setSourceHash(source_md5);
	setAssetHash(asset_md5, delta);
	// Non-native non-bulk sets the hash values and delta again from LLFloaterImagePreview
	// when the lossy/losscheck was changed. We set it here so that we can find back previous
	// uploads before the preview floater is started.
  }
  else if (mAssetType == LLAssetType::AT_SOUND && mNativeFormat)
  {
	LLMD5 md5;
	AIOggVorbisVerifier verifier(src_filename);
	try
	{
	  verifier.calculateHash(md5);
	}
	catch (AIAlert::Error const& error)
	{
	  upload_error("AIProblemWithFile", AIArgs("[FILE]", src_filename), error);
	  return false;
	}
    if (!repair_database)
    {
      setSourceHash(md5);
    }
	setAssetHash(md5);
  }
  else if (mAssetType == LLAssetType::AT_SOUND)
  {
	LLMD5 source_md5, asset_md5;
	S32 encode_result = 0;

	llinfos << "Attempting to encode wav as an ogg file" << llendl;

	mCreatedTempFile = true;
	encode_result = encode_vorbis_file(src_filename, mTmpFilename, source_md5, asset_md5);	//Singu note: passing extra params to get a hash of the source and asset file.

	if (LLVORBISENC_NOERR != encode_result)
	{
	  switch(encode_result)
	  {
		case LLVORBISENC_DEST_OPEN_ERR:
		  upload_error("AICannotOpenTemporarySoundFile", AIArgs("[FILE]", mTmpFilename));
		  break;

		default:
		  upload_error("AIUnknownVorbisEncodeFailure", AIArgs("[FILE]", src_filename));
		  break;
	  }
	  return false;
	}
	setSourceHash(source_md5);
	setAssetHash(asset_md5);
  }
  else if (mAssetType == LLAssetType::AT_ANIMATION && mNativeFormat)
  {
	LLMD5 source_md5, asset_md5;
	LLPointer<BVHAnimDelta> delta;
	AIAnimVerifier verifier(src_filename);
	try
	{
	  verifier.calculateHash(source_md5, asset_md5, delta);
	}
	catch (AIAlert::Error const& error)
	{
	  upload_error("AIProblemWithFile", AIArgs("[FILE]", src_filename), error);
	  return false;
	}

	setSourceHash(source_md5);
	setAssetHash(asset_md5, delta);
  }
  else if (mAssetType == LLAssetType::AT_ANIMATION)
  {
	if (bulk_upload)
	{
	  upload_error("AIDoNotSupportBulkBVHAnimationUpload", AIArgs());
	  return false;
	}
	//else
	// Non-native non-bulk sets the hash values and delta from LLFloaterBvhPreview.
  }
  else
  {
    llassert(mNativeFormat);
    // Make sure mThreadLock is locked.
    ScopedBlockingBackEndAccess back_end_access(*mThreadLock);
    LLMD5 asset_md5;
    try
    {
      AIFile infile(src_filename, "rb");
      long size = infile.file_size();
      // Note this file is not binary (those assets were handled above); so the file size is likely pretty small.
      if (size > 1000000)
      {
        llwarns << "Not allocating " << size << " bytes for file \"" << src_filename << "\" of type " << LLAssetType::lookup(mAssetType) << '.' << llendl;
        return false;
      }
      std::vector<unsigned char> buffer(size);
      size_t len = fread((char*)&buffer[0], 1, size, infile);
      if (len != size)
      {
        llwarns << "Short read of file \"" << src_filename << "\" with size " << size << ": could only read " << len << " bytes!" << llendl;
        return false;
      }
      // Now that we read the file into memory, fix the asset type for clothing.
      if (mAssetType == LLAssetType::AT_BODYPART)
      {
        Wearable wearable(back_end_access);
        wearable.import(&buffer[0], size);
        mAssetType = wearable.getAssetType();
        wearable.calculateHash(asset_md5);
      }
      else
      {
        back_end_access->calculateHash(mAssetType, &buffer[0], size, asset_md5);
      }
    }
    catch (AIAlert::Error const& error)
    {
      AIAlert::add_modal("AIProblemWithFile", AIArgs("[FILE]", src_filename), error);
      return false;
    }
    if (!repair_database)
    {
      setSourceHash(asset_md5);
    }
	setAssetHash(asset_md5);
  }

  return true;
}

//-----------------------------------------------------------------------------
// Texture specific functions.
// Most of this, except for the hash values and delta, used to be part of LLViewerTextureList.
// Moved here because it has everything to do with uploading (aka FrontEnd) and nothing with LLViewerTextureList.
// Afterwards the original LLViewerTextureList functions were chopped into pieces, merged with FrontEnd
// variables, reindented and just generally unrecognizably changed.
//

//static
LLPointer<LLImageRaw> FrontEnd::createRawImage(std::string const& filename, U8 const codec)
{
  // Load the image
  LLPointer<LLImageFormatted> image = LLImageFormatted::createFromType(codec);
  if (image.isNull())
  {
	LLImage::setLastError("Couldn't open the image to be uploaded.");
	return NULL;
  }
  if (!image->load(filename))
  {
	LLImage::setLastError("Couldn't load the image to be uploaded.");
	return NULL;
  }
  // Decompress or expand it in a raw image structure
  LLPointer<LLImageRaw> raw_image = new LLImageRaw;
  if (!image->decode(raw_image, 0.0f))
  {
	LLImage::setLastError("Couldn't decode the image to be uploaded.");
	return NULL;
  }
  // Check the image constraints
  if ((image->getComponents() != 3) && (image->getComponents() != 4))
  {
	LLImage::setLastError("Image files with less than 3 or more than 4 components are not supported.");
	return NULL;
  }

  return raw_image;
}

//static
LLPointer<LLImageRaw> FrontEnd::createRawImage(std::string const& filename, U8 const codec, LLMD5& source_md5)
{
  LLPointer<LLImageRaw> raw_image = createRawImage(filename, codec);
  if (raw_image && !raw_image->calculateHash(source_md5))
  {
	// This can't happen or createRawImage would already have failed.
	LLImage::setLastError("Internal error.");
	raw_image = NULL;
  }
  return raw_image;
}

bool FrontEnd::createJ2CUploadFile(LLPointer<LLImageRaw> const& raw_image, LLMD5& asset_md5, LLPointer<AIMultiGrid::TextureDelta>& delta)
{
  // Convert to j2c (JPEG2000) and save the file locally
  LLPointer<LLImageJ2C> compressedImage = convertToUploadFile(raw_image);
  if (compressedImage.isNull())
  {
	LLImage::setLastError("Couldn't convert the image to jpeg2000.");
	llinfos << "Couldn't convert raw_image to j2c." << llendl;
	return false;
  }
  mCreatedTempFile = true;
  if (!compressedImage->save(mTmpFilename))
  {
	LLImage::setLastError("Couldn't create the jpeg2000 image for upload.");
	llinfos << "Couldn't create output file : " << mTmpFilename << llendl;
	return false;
  }
  if (!verifyUploadFile(mTmpFilename, mNativeFormat, asset_md5, delta))
  {
    // verifyUploadFile should throw errors... but for now, just pass on the English error text.
    AIAlert::add("AIError", AIArgs("[ERROR]", std::string("Verification of texture failed: ") + LLImage::getLastError()));
	return false;
  }
  // Verify that we could decode whether or not the image was lossy or lossless.
  llassert(!mNativeFormat &&																	// We shouldn't even get here for native uploads.
	  delta->getLossless() == (gSavedSettings.getBOOL("LosslessJ2CUpload") &&
		                       raw_image->getWidth() * raw_image->getHeight() <=
							   LL_IMAGE_REZ_LOSSLESS_CUTOFF * LL_IMAGE_REZ_LOSSLESS_CUTOFF));
  return true;
}

static bool positive_power_of_two(int dim)
{
  return dim > 0 && !(dim & (dim - 1));
}

//static
bool FrontEnd::verifyUploadFile(std::string const& out_filename, bool native, LLMD5& md5, LLPointer<AIMultiGrid::TextureDelta>& delta)
{
  // Test to see if the encode and save worked
  LLPointer<LLImageJ2C> integrity_test = new LLImageJ2C;
  if (!integrity_test->loadAndValidate(out_filename, md5, delta))
  {
	LLImage::setLastError(std::string("The ") + (native ? "" : "created ") + "jpeg2000 image is corrupt: " + LLImage::getLastError());
	llinfos << "Image file : " << out_filename << " is corrupt" << llendl;
	return false;
  }
  if (native)
  {
	if (integrity_test->getComponents() < 3 || integrity_test->getComponents() > 4)
	{
	  LLImage::setLastError("Image files with less than 3 or more than 4 components are not supported.");
	  return false;
	}
	else if (!positive_power_of_two(integrity_test->getWidth()) ||
			 !positive_power_of_two(integrity_test->getHeight()))
	{
	  LLImage::setLastError("The width or height is not a (positive) power of two.");
	  return false;
	}
  }
  return true;
}

// Note: modifies the argument raw_image.
//static
LLPointer<LLImageJ2C> FrontEnd::convertToUploadFile(LLPointer<LLImageRaw> raw_image)
{
  raw_image->biasedScaleToPowerOfTwo(LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT);
  LLPointer<LLImageJ2C> compressedImage = new LLImageJ2C();
  compressedImage->setRate(0.f);

  if (gSavedSettings.getBOOL("LosslessJ2CUpload") &&
	  (raw_image->getWidth() * raw_image->getHeight() <= LL_IMAGE_REZ_LOSSLESS_CUTOFF * LL_IMAGE_REZ_LOSSLESS_CUTOFF))
	  compressedImage->setReversible(TRUE);

#if 0	// Was commented out in the old code.
  if (gSavedSettings.getBOOL("Jpeg2000AdvancedCompression"))
  {
	// This test option will create jpeg2000 images with precincts for each level, RPCL ordering
	// and PLT markers. The block size is also optionally modifiable.
	// Note: the images hence created are compatible with older versions of the viewer.
	// Read the blocks and precincts size settings
	S32 block_size = gSavedSettings.getS32("Jpeg2000BlocksSize");
	S32 precinct_size = gSavedSettings.getS32("Jpeg2000PrecinctsSize");
	llinfos << "Advanced JPEG2000 Compression: precinct = " << precinct_size << ", block = " << block_size << llendl;
	compressedImage->initEncode(*raw_image, block_size, precinct_size, 0);
  }
#endif

  if (!compressedImage->encode(raw_image, 0.0f))
  {
	llinfos << "convertToUploadFile : encode returns with error!" << llendl;
	// Clear up the pointer so we don't leak.
	compressedImage = NULL;
  }

  return compressedImage;
}

// End of texture specific part.
//-----------------------------------------------------------------------------

bool FrontEnd::checkPreviousUpload(std::string& name, std::string& description, LLUUID& id)
{
  // Only call this function when prepare_upload() returned true.
  llassert_always(mAssetType != LLAssetType::AT_NONE);
  // Should only be called for one-on-one asset types.
  llassert(mAssetType != LLAssetType::AT_ANIMATION && mAssetType != LLAssetType::AT_TEXTURE);

  ScopedBlockingBackEndAccess back_end_access(*mThreadLock);

  find_previous_uploads();
  bool found = false;
  if (mUploadedAsset && !mBulkUpload)
  {
	AIUploadedAsset_rat uploaded_asset_r(*mUploadedAsset);
	name = uploaded_asset_r->getName();
	description = uploaded_asset_r->getDescription();
	// uploaded_asset might be a match of an incomplete source hash.
	// Only set id when that is not the case!
	if (uploaded_before(uploaded_asset_r))
	{
	  Grid const current_grid(gHippoGridManager->getConnectedGrid()->getGridNick());
	  grid2uuid_map_type::iterator iter = uploaded_asset_r->find_uuid(current_grid);
	  if (iter != uploaded_asset_r->end_uuid())
	  {
		id = iter->getUUID();
		found = true;
	  }
	}
  }

  return found;
}

struct checkPreviousUploads_sortPred
{
  Grid mCurrentGrid;
  checkPreviousUploads_sortPred(std::string const& current_grid_nick) : mCurrentGrid(current_grid_nick) { }
  bool operator()(AIUploadedAsset* ua1, AIUploadedAsset* ua2)
  {
	AIUploadedAsset_rat ua1_r(*ua1);
	AIUploadedAsset_rat ua2_r(*ua2);
	bool exists_on_current_grid1 = ua1_r->find_uuid(mCurrentGrid) != ua1_r->end_uuid();
	bool exists_on_current_grid2 = ua2_r->find_uuid(mCurrentGrid) != ua2_r->end_uuid();
	// Sort such that all assets that where uploaded to the current grid come first.
	if (exists_on_current_grid1 != exists_on_current_grid2)
	{
	  return exists_on_current_grid1;
	}
	// Put the youngest dates at top.
	return ua1_r->getDate() >= ua2_r->getDate();
  }
};

std::vector<AIUploadedAsset*> FrontEnd::checkPreviousUploads(void)
{
  std::vector<AIUploadedAsset*> result;
  ScopedBlockingBackEndAccess back_end_access(*mThreadLock);
  find_previous_uploads();
  result = mUploadedAssets;
  std::stable_sort(result.begin(), result.end(), checkPreviousUploads_sortPred(gHippoGridManager->getConnectedGrid()->getGridNick()));
  return result;
}

class NewResourceItemCallback : public LLInventoryCallback
{
  /*virtual*/ void fire(LLUUID const& new_item_id);
};

void NewResourceItemCallback::fire(LLUUID const& new_item_id)
{
  LLViewerInventoryItem* new_item = (LLViewerInventoryItem*)gInventory.getItem(new_item_id);
  if (!new_item) return;
  LLUUID vfile_id = LLUUID(new_item->getDescription());
  if (vfile_id.isNull()) return;
  new_item->setDescription("(No Description)");
  new_item->updateServer(FALSE);
  gInventory.updateItem(new_item);
  gInventory.notifyObservers();

  std::string agent_url;
  LLSD body;
  body["item_id"] = new_item_id;

  if (new_item->getType() == LLAssetType::AT_GESTURE)
  {
	agent_url = gAgent.getRegion()->getCapability("UpdateGestureAgentInventory");
  }
  else if (new_item->getType() == LLAssetType::AT_LSL_TEXT)
  {
	agent_url = gAgent.getRegion()->getCapability("UpdateScriptAgent");
	body["target"] = "lsl2";
  }
  else if(new_item->getType() == LLAssetType::AT_NOTECARD)
  {
	agent_url = gAgent.getRegion()->getCapability("UpdateNotecardAgentInventory");
  }
  else
  {
	return;
  }

  if (agent_url.empty()) return;
  LLHTTPClient::post(agent_url, body, new LLUpdateAgentInventoryResponder(body, vfile_id, new_item->getType()));
}

void FrontEnd::upload_new_resource_continued(
	std::string const& name,
	std::string const& desc, S32 compression_info,
	LLFolderType::EType destination_folder_type,
	LLInventoryType::EType inv_type,
	U32 next_owner_perms,
	U32 group_perms,
	U32 everyone_perms,
	std::string const& display_name,
	S32 expected_upload_cost)
{
  // First call prepare_upload.
  llassert_always((mCollectedDataMask & source_filename_bit));

  // Don't try to upload a non-native source file!
  llassert_always(mCreatedTempFile || mNativeFormat);

  // Generate the temporary UUID.
  LLTransactionID tid;
  tid.generate();
  LLAssetID uuid = tid.makeAssetID(gAgent.getSecureSessionID());

  bool error = false;

  // Copy this file into the vfs for upload.
  std::string src_filename = mCreatedTempFile ? mTmpFilename : mSourceFilename;
  S32 file_size;
  LLAPRFile infile(src_filename, LL_APR_RB, &file_size);
  if (infile.getFileHandle())
  {
	LLVFile file(gVFS, uuid, mAssetType, LLVFile::WRITE);

	file.setMaxSize(file_size);

	S32 const buf_size = 65536;
	U8 copy_buf[buf_size];
	while ((file_size = infile.read(copy_buf, buf_size)))
	{
	  file.write(copy_buf, file_size);
	}
  }
  else
  {
	error = true;
  }

  // Clean up.
  if (mCreatedTempFile && LLFile::remove(mTmpFilename) == -1)
  {
	lldebugs << "unable to remove temp file" << llendl;
  }

  if (error)
  {
	AIAlert::add_modal("AIUnableToAccessOutputFile", AIArgs("[FILE]", src_filename));
	return;
  }

  std::string exten = gDirUtilp->getExtension(mSourceFilename);
  if (exten == "lsl" || exten == "gesture" || exten == "notecard")
  {
	LLInventoryType::EType inv_type = LLInventoryType::IT_GESTURE;
	if (exten == "lsl") inv_type = LLInventoryType::IT_LSL;
	else if (exten == "gesture") inv_type = LLInventoryType::IT_GESTURE;
	else if (exten == "notecard") inv_type = LLInventoryType::IT_NOTECARD;
	create_inventory_item(
		gAgent.getID(),
		gAgent.getSessionID(),
		gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(mAssetType)),
		LLTransactionID::tnull,
		name,
		uuid.asString(), // Fake asset id, but in vfs.
		mAssetType,
		inv_type,
		NOT_WEARABLE,
		PERM_ITEM_UNRESTRICTED,
		new NewResourceItemCallback);
	// AIFIXME: If lsl, gestures and or notecard is supported too, then
	// here they should be registered with the AIMultiGrid database.
	// Since at the moment we don't run() the FrontEnd statemachine,
	// nothing will be done.
  }
  else
  {
	// Use base name of source file as display name by default.
	std::string t_display_name = display_name.empty() ? gDirUtilp->getBaseFileName(mSourceFilename) : display_name;
	upload_new_resource(tid, mAssetType, name, desc, compression_info,
		destination_folder_type, inv_type, next_owner_perms, group_perms, everyone_perms,
		t_display_name, expected_upload_cost, NULL, NULL,
		UploadResponderFactory<LLNewAgentInventoryResponder>());
  }
}

void FrontEnd::get_name_and_description(std::string& name, std::string& description)
{
  // Fix name and description (mAssetUUID, gAgentID and mAssetType are not used here).
  LLAssetInfo name_and_desc(mAssetUUID, gAgentID, mAssetType, mName.c_str(), mDescription.c_str());
  name = name_and_desc.getName();
  description = name_and_desc.getDescription();
  if (name.empty())
  {
	name = "(No Name)";
  }
  if (description.empty())
  {
	description = "(No Description)";
  }
}

void FrontEnd::temp_upload_callback(LLUUID const& uuid, S32 result, LLExtStat ext_status)
{
  if (result >= 0)
  {
	std::string name, description;
	get_name_and_description(name, description);

	LLUUID item_id;
	item_id.generate();
	LLPermissions perms;
	perms.set(LLPermissions::DEFAULT);
	perms.setOwnerAndGroup(gAgentID, gAgentID, gAgentID, false);

	perms.setMaskBase(PERM_ALL);
	perms.setMaskOwner(PERM_ALL);
	perms.setMaskEveryone(PERM_ALL);
	perms.setMaskGroup(PERM_ALL);
	perms.setMaskNext(PERM_ALL);

	LLViewerInventoryItem* item = new LLViewerInventoryItem(
		item_id,
		gInventory.findCategoryUUIDForType(LLFolderType::FT_TEXTURE),
		perms,
		uuid,
		mAssetType,
		mInventoryType,
		name,
		description,
		LLSaleInfo::DEFAULT,
		0,
		time_corrected());

	item->updateServer(TRUE);
	gInventory.updateItem(item);
	gInventory.notifyObservers();
  }
  else
  {
	LLSD args;
	args["FILE"] = LLInventoryType::lookupHumanReadable(mInventoryType);
	args["REASON"] = std::string(LLAssetStorage::getErrorString(result));
	LLNotificationsUtil::add("CannotUploadReason", args);
  }

  LLUploadDialog::modalUploadFinished();
}

void FrontEnd::upload_done_callback(LLUUID const& uuid, S32 result, LLExtStat ext_status)
{
  bool is_balance_sufficient = true;

  if (result >= 0)
  {
	std::string name, description;
	get_name_and_description(name, description);

	LLFolderType::EType dest_loc = mDestinationFolder;

	if (LLAssetType::AT_SOUND == mAssetType ||
		LLAssetType::AT_TEXTURE == mAssetType ||
		LLAssetType::AT_ANIMATION == mAssetType)
	{
	  // Charge the user for the upload.
	  LLViewerRegion* region = gAgent.getRegion();

	  if (!(can_afford_transaction(mExpectedUploadCost)))
	  {
		LLStringUtil::format_map_t args;
		args["[NAME]"] = name;
		args["[CURRENCY]"] = mHippoGrid->getCurrencySymbol();
		args["[AMOUNT]"] = llformat("%d", mExpectedUploadCost);
		LLFloaterBuyCurrency::buyCurrency(LLTrans::getString("UploadingCosts", args), mExpectedUploadCost);
		is_balance_sufficient = false;
	  }
	  else if (region)
	  {
		// Charge user for upload.
		gStatusBar->debitBalance(mExpectedUploadCost);

		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_MoneyTransferRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_MoneyData);
		msg->addUUIDFast(_PREHASH_SourceID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_DestID, LLUUID::null);
		msg->addU8("Flags", 0);
		// We tell the sim how much we were expecting to pay so it can respond to any discrepancy.
		msg->addS32Fast(_PREHASH_Amount, mExpectedUploadCost);
		msg->addU8Fast(_PREHASH_AggregatePermNextOwner, (U8)LLAggregatePermissions::AP_EMPTY);
		msg->addU8Fast(_PREHASH_AggregatePermInventory, (U8)LLAggregatePermissions::AP_EMPTY);
		msg->addS32Fast(_PREHASH_TransactionType, TRANS_UPLOAD_CHARGE);
		msg->addStringFast(_PREHASH_Description, NULL);
		msg->sendReliable(region->getHost());
	  }
	}

	if (is_balance_sufficient)
	{
	  // Actually add the upload to inventory.
	  llinfos << "Adding " << uuid << " to inventory." << llendl;
	  LLUUID const folder_id = gInventory.findCategoryUUIDForType(dest_loc);
	  if (folder_id.notNull())
	  {
		U32 next_owner_perms = mNextOwnerPerms;
		if (PERM_NONE == next_owner_perms)
		{
		  next_owner_perms = PERM_MOVE | PERM_TRANSFER;
		}
		create_inventory_item(
			gAgent.getID(),
			gAgent.getSessionID(),
			folder_id,
			mTid,
			name,
			description,
			mAssetType,
			mInventoryType,
			NOT_WEARABLE,
			next_owner_perms,
			LLPointer<LLInventoryCallback>(NULL));
	  }
	  else
	  {
		llwarns << "Can't find a folder to put it in" << llendl;
	  }
	}
  }
  else // if (result >= 0)
  {
	LLSD args;
	args["FILE"] = LLInventoryType::lookupHumanReadable(mInventoryType);
	args["REASON"] = std::string(LLAssetStorage::getErrorString(result));
	LLNotificationsUtil::add("CannotUploadReason", args);
  }

  LLUploadDialog::modalUploadFinished();
}

bool FrontEnd::upload_new_resource(
	LLTransactionID const& tid,
	LLAssetType::EType asset_type,
	std::string const& name,
	std::string const& desc,
	S32 compression_info,
	LLFolderType::EType destination_folder_type,
	LLInventoryType::EType inv_type,
	U32 next_owner_perms,
	U32 group_perms,
	U32 everyone_perms,
	std::string const& display_name,
	S32 expected_upload_cost,
	UserCallback user_callback,
	void* userdata,
	UploadResponderFactoryBase const& responder_factory)
{
	if (gDisconnected)
	{
		return false;
	}

	// Store everything in the state machine object (fill in defaults for destination folder and inventory type if needed.).
	mTid = tid;
	mAssetType = asset_type;
	mName = name;
	mDescription = desc;
	mDestinationFolder = (destination_folder_type != LLFolderType::FT_NONE) ? destination_folder_type : LLFolderType::assetTypeToFolderType(asset_type);
	mInventoryType = (inv_type != LLInventoryType::IT_NONE) ? inv_type : LLInventoryType::defaultForAssetType(asset_type);
	mNextOwnerPerms = next_owner_perms;
	mDisplayName = display_name;
	mExpectedUploadCost = expected_upload_cost;
	mUserCallback = user_callback;
	mUserData = userdata;

	// Remember the grid nick that mAssetUUID was generated for.
    mHippoGrid = gHippoGridManager->getConnectedGrid();

	// Prepare upload (this used to be upload_new_resource_prep).
	mAssetUUID = generate_asset_id_for_new_upload(tid);
	LLStringUtil::stripNonprintable(mName);
	LLStringUtil::stripNonprintable(mDescription);

	// Initialize some more variables.
	mURL = gAgent.getRegion()->getCapability("NewFileAgentInventory");
	mTemporary = gSavedSettings.getBOOL("TemporaryUpload");
	gSavedSettings.setBOOL("TemporaryUpload",FALSE);
	mResponder = NULL;
	mHttpUpload = !mURL.empty() && !mTemporary;

	if (!mHttpUpload && !mTemporary)
	{
		llinfos << "NewAgentInventory capability not found, new agent inventory via asset system." << llendl;

		// This is kind of a hack:
		// Currently mUserCallback is only ever set to LLSnapshotLivePreview::saveTextureDone, which
		// is also the only place that checks the return value of upload_new_resource.
		// Thus, if mUserCallback is not set it doesn't really matter what we return and
		// we delay the funds check until we known if we really need to upload this asset
		// while if mUserCallback is set then this is a newly made snapshot and we already
		// know that we'll need to upload it, so we can check the funds here.
		if (mUserCallback && !checkFunds())
		{
		  // Return false to indicate that mUserCallback will not be called.
		  return false;
		}
	}

	if (mHttpUpload)
	{
		std::string name = mName;
		std::string description = mDescription;
		if (mName.empty())
		{
		  name = "(No Name)";
		}
		if (mDescription.empty())
		{
		  description = "(No Description)";
		}
		mBody["folder_id"] = gInventory.findCategoryUUIDForType(mDestinationFolder);
		mBody["asset_type"] = LLAssetType::lookup(mAssetType);
		mBody["inventory_type"] = LLInventoryType::lookup(mInventoryType);
		mBody["name"] = name;
		mBody["description"] = description;
		mBody["next_owner_mask"] = LLSD::Integer(mNextOwnerPerms);
		mBody["group_mask"] = LLSD::Integer(group_perms);
		mBody["everyone_mask"] = LLSD::Integer(everyone_perms);
		mBody["expected_upload_cost"] = LLSD::Integer(mExpectedUploadCost);

		// responder_factory is a reference to a temporary; we have to use it before we leave upload_new_resource.
		mResponder = responder_factory(mBody, mAssetUUID, mAssetType, &FrontEnd::callback, this);
	}

	// Start the state machine.
	run();

	// A call to a callback is guaranteed now that the state machine is running.
	return true;
}

void FrontEnd::initialize_impl(void)
{
  mLocked = mThreadLock->is_locked();
  mAbort = false;

  // Temporaries are always uploaded regardless (and not registered).
  set_state(mTemporary ? FrontEnd_doUpload : FrontEnd_findUploadedAsset);
}

void FrontEnd::find_previous_uploads(void)
{
  // Only call this function when prepare_upload() returned true.
  llassert_always(mAssetType != LLAssetType::AT_NONE);

  ScopedBlockingBackEndAccess back_end_access(*mThreadLock);

  // Set mUploadedAsset to the data related to the asset hash, if any.
  if (!mUploadedAsset && (mCollectedDataMask & asset_hash_bit))
  {
	mUploadedAsset = back_end_access->getUploadedAsset(mAssetHash);
  }
  // Set mUploadedAssets to the data related to the source hash, if any.
  if (mUploadedAssets.empty() && (mCollectedDataMask & source_hash_bit))
  {
	uais_type uais = back_end_access->getUploadedAssets(mSourceHash);
	for (uais_type::iterator uai = uais.begin(); uai != uais.end(); ++uai)
	{
	  mUploadedAssets.push_back(**uai);
	}
	// In case no asset hash is known (or when it changed), allow us to recover by still finding the asset by source hash.
	if (!mUploadedAsset && !mUploadedAssets.empty())
	{
	  // If mSourceHashType is one_on_one then this will be the only entry and setting mUploadedAsset
	  // will be enough for uploaded_before() to return true.
	  // If mSourceHashType is one_source_many_assets then the first asset in the list is the one
	  // that was uploaded last (to any grid) and uploaded_before() will only return true when no
	  // asset hash is known (as a means of recovery).
	  // Note that if the asset hash IS known (and we didn't find it above because mUploadedAsset
	  // is still NULL when we get here), then whatever we set mUploadedAsset to here will never
	  // have a matching mAssetHash and therefore will not cause uploaded_before() to return true.
	  mUploadedAsset = mUploadedAssets[0];
	}
  }
}

bool FrontEnd::uploaded_before(AIUploadedAsset_rat const& uploaded_asset_r) const
{
  // See above: if mUploadedAsset is set, then assume that was the (only) previously uploaded asset
  // if there is a one-on-one relationship between asset and source hashes, or if no asset hash is
  // known. Otherwise, only when the found asset matches the required asset hash.
  // Note that if !(mCollectedDataMask & source_hash_bit) then mSourceHashType isn't set (it's
  // undefined), but we know that asset_hash_bit must be set since mUploadedAsset was set and
  // thus uploaded_asset_r->getAssetMd5() == mAssetHash because that's how it was found...
  // hence, need to return true.
  return !(mCollectedDataMask & source_hash_bit) || mSourceHashType == one_on_one ||
	     !(mCollectedDataMask & asset_hash_bit) || uploaded_asset_r->getAssetMd5() == mAssetHash;
}

bool FrontEnd::uploaded_before(void) const
{
  return mUploadedAsset && uploaded_before(AIUploadedAsset_rat(*mUploadedAsset));
}

class FixDescAndNextOwnerPermsCallBack : public LLInventoryCallback
{
  private:
	U32 mNextOwnerPerms;
	std::string mDescription;

  public:
	FixDescAndNextOwnerPermsCallBack(U32 next_owner_perms, std::string const& description) : mNextOwnerPerms(next_owner_perms), mDescription(description) { }

	/*virtual*/ void fire(LLUUID const& inv_item)
	{
	  // The item was copied - now set the description.
	  LLInventoryItem* item = gInventory.getItem(inv_item);
	  if (item)
	  {
		LLPermissions perms(item->getPermissions());
		perms.setMaskNext(mNextOwnerPerms);
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setPermissions(perms);
		new_item->setDescription(mDescription);
		new_item->updateServer(FALSE);
		gInventory.updateItem(new_item);
		gInventory.notifyObservers();
	  }
	}
};

void FrontEnd::multiplex_impl(state_type run_state)
{
  try
  {
	switch (run_state)
	{
	  case FrontEnd_lockDatabase:
	  {
		// Leave main thread, because calls to the BackEnd are slow.
		if (yield_if_not(&gStateMachineThreadEngine))
		{
		  break;
		}
        // We should only get here when the database wasn't locked before.
		llassert(!mLocked);
        // Sanity check.
        if (mLocked)
        {
          set_state(FrontEnd_findUploadedAsset);
          break;
        }
        // Lock database a-synchronusly.
        mThreadLock->run(this, FrontEnd_lockedDatabase);
        idle();
        break;
      }
      case FrontEnd_lockedDatabase:
      {
		mLocked = true;
		set_state(FrontEnd_findUploadedAsset);
	  }
	  case FrontEnd_findUploadedAsset:
	  {
		// Try to find this asset in the database.
		if ((mCollectedDataMask & (asset_hash_bit|source_hash_bit)))
		{
		  if (!mLocked)
		  {
			// Lock the database before we call find_previous_uploads.
			set_state(FrontEnd_lockDatabase);
			break;
		  }
		  find_previous_uploads();
		}
		set_state(uploaded_before() ? FrontEnd_uploadedAssetFound : FrontEnd_uploadNewResource);
		break;
	  }
	  case FrontEnd_uploadedAssetFound:
	  {
		// DATABASE LOCKED.
		//
		// We only get here from FrontEnd_findUploadedAsset if uploaded_before() returned true,
		// which only returns true when mUploadedAsset is set, which only can be set if find_previous_uploads()
		// was called in state FrontEnd_findUploadedAsset, which is done after locking the database.
		llassert(mLocked);

		// Did we already upload this asset to the current grid?
		Grid const current_grid(mHippoGrid->getGridNick());
		AIUploadedAsset_rat found_ua_r(*mUploadedAsset);
		grid2uuid_map_type::iterator iter = found_ua_r->find_uuid(current_grid);
		if (iter == found_ua_r->end_uuid())
		{
		  // Was not uploaded to this grid before. Proceed with the upload.
		  set_state(FrontEnd_doUpload);
		  break;
		}
		// The user is trying to upload the same thing again.
		Dout(dc::notice, "FrontEnd_uploadedAssetFound: Trying to upload an already existing asset to the same grid.");
		// Find inventory items with this asset id.
		mFoundUUID = iter->getUUID();
		set_state(FrontEnd_fetchInventory);
		// Run FrontEnd_fetchInventory in the main thread.
		yield(&gMainThreadEngine);
		break;
	  }
	  case FrontEnd_fetchInventory:
	  {
		// DATABASE LOCKED.
		//
		// We only get here from FrontEnd_uploadedAssetFound.
		llassert(mLocked);

		// This must run in the main thread because gInventory is not thread-safe.
		llassert(AIThreadID::in_main_thread());	// Should still be in main thread because we switched to it using yield().
		// Advance state to FrontEnd_findInInventory as soon as the whole inventory is fetched.
		new AIInventoryFetchDescendentsObserver(this, FrontEnd_findInInventory, gInventory.getRootFolderID());
		idle();
		break;
	  }
	  case FrontEnd_findInInventory:
	  {
		// DATABASE LOCKED.
		//
		// We only get here from FrontEnd_fetchInventory.
		llassert(mLocked);

		// This must run in the main thread because gInventory is not thread-safe.
		llassert(AIThreadID::in_main_thread());	// Should still be in main thread because FrontEnd_fetchInventory runs in the main thread as does fetching the inventory.
		LLUUID const trash_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH, false);
		// Try to find a full perm item in the inventory.
		AINoLinkAssetIDMatches no_link_asset_id_matches(mFoundUUID);
		LLViewerInventoryCategory::cat_array_t cats;
		LLViewerInventoryItem::item_array_t items;
		gInventory.collectDescendentsIf(LLUUID::null, cats, items, LLInventoryModel::INCLUDE_TRASH, no_link_asset_id_matches);
		LLViewerInventoryItem* full_perm_item = NULL;
		bool in_trash;
		for(int i = 0; i < items.count(); ++i)
		{
		  LLViewerInventoryItem* item = dynamic_cast<LLViewerInventoryItem*>(items.get(i).get());
		  // Sanity check, this should never fail.
		  if (!item)
		  {
			llerrs << "dynamic_cast of inventory item failed!" << llendl;
			continue;
		  }
		  if ((item->getPermissions().getMaskOwner() & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED)
		  {
			in_trash = item->getParentUUID() == trash_id;
			if (full_perm_item && in_trash)
			{
			  continue;
			}
			full_perm_item = item;
			if (!in_trash)
			{
			  // Prefer items that are not in the trash.
			  break;
			}
		  }
		}
		if (full_perm_item)
		{
		  // The item already exists in the inventory.
		  LLUUID const folder_id = gInventory.findCategoryUUIDForType(mDestinationFolder);
		  if (folder_id.isNull())
		  {
			llwarns << "Upload of previously uploaded item with UUID " << mFoundUUID << " exists in inventory (as \"" << full_perm_item->getName() << "\"), "
				"but failed to copy it because can't find a folder for type " << mDestinationFolder << " to put it in." << llendl;
			abort();
			break;
		  }
		  // Copy the item, so we can change the name and reset the create time stamp; if the item is not in
		  // the trash we could create a link to it, but then the name will be arbitrary which could be confusing...
		  // Moving and renaming would be another option, but on opensim it's impossible to change the creation time
		  // stamp so that in inventories sorted by date the "uploaded" asset wouldn't appear in the expected place.
		  copy_inventory_item(gAgentID, gAgentID, full_perm_item->getUUID(), folder_id, mName,
			  new FixDescAndNextOwnerPermsCallBack(mNextOwnerPerms, mDescription));		// Fix next owner perms and description once the copy succeeded.
		  // We're not going to upload anything, but we might need to call registerUpload in case the name or description changed.
		  if (mBulkUpload)
		  {
			set_state(FrontEnd_unlockDatabase);
			break;
		  }
		  else
		  {
			// Fake a finished upload.
			mNewUUID = mFoundUUID;
			set_state(FrontEnd_registerUpload);
		  }
		  break;
		}

		// The fact that the state machine is being run means that we DO want to do a
		// new upload (if we couldn't find a pull perm item in the inventory already),
		// even though the UUID of a previous upload is known.
		llinfos << "Uploading asset that was already uploaded before (with UUID " << mFoundUUID << ") because a full perm copy couldn't be found in the inventory anymore." << llendl;
		set_state(FrontEnd_doUpload);
		break;
	  }
	  case FrontEnd_uploadNewResource:
	  {
		// Database might or might not be locked.

		// This is a brand new asset that we never saw before.
		// If no hash was calculated then this asset type is not supported yet and not put in the database.
		if ((mCollectedDataMask & asset_hash_bit))
		{
		  // DATABASE LOCKED.
		  llassert(mLocked);

		  // Make copy of it into our database.
		  LLVFile vfile(gVFS, mAssetUUID, mAssetType);
		  // This feels horrible, but it's how it's done everywhere else in the viewer code (copied from LLPreviewSound::gotAssetForSave) :/
		  // Copy the whole asset into memory.
		  S32 size = vfile.getSize();
		  char* buffer = new char[size];
		  if (buffer == NULL)
		  {
			llerrs << "Out of memory, allocating " << size << " bytes." << llendl;
			abort();
			break;
		  }
		  vfile.read((U8*)buffer, size);
		  // And write it to disk.
		  std::string asset_filename = BackEnd::instance().getAssetFilename(mAssetHash);
		  std::ofstream file(asset_filename.c_str(), std::ofstream::binary);
		  file.write(buffer, size);
		  file.close();
		  delete [] buffer;
		}
		set_state(FrontEnd_doUpload);
		break;
	  }
	  case FrontEnd_doUpload:
	  {
		// Database might or might not be locked.

		// This must run in the main thread.
		if (!AIThreadID::in_main_thread())
		{
		  yield(&gMainThreadEngine);
		  break;
		}

		// We are actually uploading!
		// Don't count temporary uploads for the stats.
		if (!mTemporary)
		{
		  increase_new_upload_stats(mAssetType);
		}
		std::string upload_message = "Uploading...\n\n";
		upload_message.append(mDisplayName);
		LLUploadDialog::modalUploadDialog(upload_message);

		llinfos << "*** Uploading: "
				<< "\nType: " << LLAssetType::lookup(mAssetType)
				<< "\nUUID: " << mAssetUUID
				<< "\nName: " << mName
				<< "\nDesc: " << mDescription
				<< "\nFolder: "
				<< gInventory.findCategoryUUIDForType(mDestinationFolder)
				<< "\nExpected Upload Cost: " << mExpectedUploadCost << llendl;

		if (mHttpUpload)
		{
			LLHTTPClient::post(mURL, mBody, mResponder);
			mResponder = NULL;	// Stop us from deleting it; LLHTTPClient takes care of deletion now.
		}
		else
		{
			gAssetStorage->storeAssetData(mTid, mAssetType, &FrontEnd::legacyCallback, this, mTemporary, TRUE, mTemporary);
		}

		idle();
		break;
	  }
	  case FrontEnd_registerUpload:
	  {
		// DATABASE LOCKED.
		//
		// We only get here from FrontEnd_findInInventory, which has the database already locked, or after
		// a call to FrontEnd::uploadFinished provided that (mCollectedDataMask & asset_hash_bit) && !mTemporary
		// is true; and if that is true then the database was locked in FrontEnd_findUploadedAsset, because
		// that is where the statemachine starts when !mTemporary and it locks the database when
		// (mCollectedDataMask & asset_hash_bit).
		llassert(mLocked);

		// Do some sanity checks.
		if (mTemporary)
		{
		  llerrs << "FrontEnd_registerUpload: Trying to register a temporary?!" << llendl;
		  mAbort = true;
		}
		else if (mNewUUID.isNull())
		{
		  llwarns << "FrontEnd_registerUpload: returned UUID is NULL?!" << llendl;
		  mAbort = true;
		}
		else if (!(mCollectedDataMask & asset_hash_bit))
		{
		  llerrs << "FrontEnd_registerUpload: not all required data was collected?!" << llendl;
		  mAbort = true;
		}
		else if (((mCollectedDataMask & source_hash_bit) && !(mCollectedDataMask & source_filename_bit)) ||
			(!(mCollectedDataMask & source_hash_bit) == mSourceHash.isFinalized()))
		{
		  llerrs << "FrontEnd_registerUpload: Source filename/hash inconsistency!" << llendl;
		  mAbort = true;
		}
		// Leave mainthread, because calls to the BackEnd are slow.
		else if (yield_if_not(&gStateMachineThreadEngine))
		{
		  break;
		}
		else
		{
		  // Register this upload in the database.
		  mThreadLock->back_end_access()->registerUpload(
			  mAssetHash,
			  mAssetType,
			  GridUUID(mHippoGrid->getGridNick(), mNewUUID),
			  mName,
			  mDescription,
			  mBulkUpload,
			  mSourceFilename,
			  LLDate::now(),
			  mSourceHash,
			  !(mCollectedDataMask & source_hash_bit) || mSourceHashType == one_on_one,		// Pass true when no source hash was set: only affects an assert that will demand mDelta to be NULL.
			  mDelta);
		}
		set_state(FrontEnd_unlockDatabase);
		break;
	  }
	  case FrontEnd_unlockDatabase:
	  {
		llassert(mLocked);
		// Unlock must be called by the same thread that locked it.
		if (yield_if_not(&gStateMachineThreadEngine))
		{
		  break;
		}
		// Unlock the database.
		mThreadLock->unlock();
		mLocked =false;
		if (mAbort)
		{
		  abort();
		  break;
		}
		set_state(FrontEnd_done);
		break;
	  }
	  case FrontEnd_done:
	  {
		if (mLocked)
		{
		  set_state(FrontEnd_unlockDatabase);
		  break;
		}
		if (mAbort)
		{
		  abort();
		  break;
		}
		finish();
		break;
	  }
	}
  }
  catch (AIAlert::Error const& error)
  {
	llerrs << AIAlert::text(error) << ". " << LLTrans::getString("FrontEnd_Cannot_recover_from_database_corruption") << llendl;
	mAbort = true;
	set_state(FrontEnd_done);
  }
}

void FrontEnd::abort_impl(void)
{
  mAbort = false;
}

void FrontEnd::finish_impl(void)
{
  // Should never have the database locked anymore at this point and abort should have been handled, if any.
  llassert(!mLocked && !mAbort);

  // Delete the responder if not already done.
  delete mResponder;
}

} // namespace AIMultiGrid

