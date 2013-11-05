/**
 * @file aimultigridfrontend.h
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
 *   Creation of aimultigridfrontend.h (AI)
 */

#ifndef AIMULTIGRIDFRONTEND_H
#define AIMULTIGRIDFRONTEND_H

#include "aistatemachine.h"
#include "aiuploadedasset.h"
#include "aimultigriddelta.h"
#include "aimultigridbackendaccess.h"
#include <vector>
#include "llassettype.h"		// LLAssetType::EType
#include "lluuid.h"				// LLUUID, LLTransactionID, LLAssetID
#include "llsd.h"				// LLSD
#include "llmd5.h"				// LLMD5
#include "llfoldertype.h"		// LLFolderType::EType
#include "llinventorytype.h"	// LLInventoryType::EType
#include "llextendedstatus.h"	// LLExtStat

class HippoGridInfo;
class AIUploadedAsset;
class LLNewAgentInventoryResponder;
class AIArgs;
namespace AIAlert { class Error; }

namespace AIMultiGrid {

struct UploadResponderFactoryBase {
  typedef void (*Callback)(LLUUID const&, void*, bool);		// Must be the same type as LLNewAgentInventoryResponder::Callback.
  virtual LLNewAgentInventoryResponder* operator()(LLSD const& post_data, LLUUID const& vfile_id, LLAssetType::EType asset_type, Callback callback, void* userdata) const = 0;
};

template<class Responder>
struct UploadResponderFactory : public UploadResponderFactoryBase {
  /*virtual*/ LLNewAgentInventoryResponder* operator()(LLSD const& post_data, LLUUID const& vfile_id, LLAssetType::EType asset_type, Callback callback, void* userdata) const
  {
	return new Responder(post_data, vfile_id, asset_type, callback, userdata);
  }
};

// Interface class.
class FrontEnd : public AIStateMachine
{
  public:
	typedef void (*UserCallback)(bool, void*);

	enum ESource {
	  one_on_one,
	  one_source_many_assets
	};

  private:
	static int const source_filename_bit = 0x01;
	static int const source_hash_bit     = 0x02;
	static int const asset_hash_bit      = 0x04;

	//-------------------------------------------------------------------------
	// Thread-safety: these members are set before the state machine is started and after that only read.

	// Parameters of upload_new_resource.
	LLTransactionID mTid;
	LLAssetType::EType mAssetType;	// Also used in upload_new_resource_continued.
	std::string mName;
	std::string mDescription;
	LLFolderType::EType mDestinationFolder;
	LLInventoryType::EType mInventoryType;
	U32 mNextOwnerPerms;
	std::string mDisplayName;
	S32 mExpectedUploadCost;

	// Parameter passed to addDelta.
	LLPointer<Delta> mDelta;

	// The following members are used in upload_new_resource_continued.
	bool mCreatedTempFile;			// Set to true when mCreatedTempFile is temporary and created.
	std::string mTmpFilename;		// The file that is copied to VFS for uploading, either a temporary file (if mCreatedTempFile is true) or equal to mSourceFilename.

	// The following members are valid in the callback function.
	bool mHttpUpload;		// Set when this was an http upload.
	bool mTemporary;		// Set when this is a temporary upload.
	bool mBulkUpload;		// Set when this is part of a bulk upload.
	int mCollectedDataMask;	// Set with bits for collected data.

	std::string mSourceFilename;	// Set when source file is known (upload of original source).
	LLMD5 mSourceHash;				// Idem, md5 hash of above file.
	ESource mSourceHashType;		// Whether mSourceHash has a one-on-one relationship to mAssetHash or that the source is not complete (as is the case for BVH).
	bool mNativeFormat;				// Set if the to-be-uploaded source file is already in the format that can be uploaded directly to the server.

	LLMD5 mAssetHash;				// The md5 hash for the (to be uploaded) asset file.

	HippoGridInfo* mHippoGrid;		// The connected grid at moment of upload.
	LLAssetID mAssetUUID;			// The UUID of the to be uploaded asset.
	AIUploadedAsset* mUploadedAsset;// Pointer to found uploaded asset data.
	std::vector<AIUploadedAsset*> mUploadedAssets;	// Vector with pointers to previously uploaded asset data.

	// Information only used for HTTP upload.
	std::string mURL;							// The upload capability.
	LLSD mBody;									// The body to post.
	LLNewAgentInventoryResponder* mResponder;	// The responder to use.

	// Callback related variables.
	void* mUserData;				// The pointer to pass to the call back fucntion.
	UserCallback mUserCallback;		// Function pointer to requested user callback, or NULL when none is required.
	LLUUID mNewUUID;				// The UUID of the newly uploaded asset.

	// Variable(s) used in the statemachine between states.
	LLUUID mFoundUUID;				// The UUID of a previously to the current grid uploaded asset.
	BackEndAccess mBackEndAccess;	// Using this to access the back end will lock the database for any other BackEndAccess, until this object is destructed.
	bool mLocked;					// Set if this state machine obtained the lock.
	bool mAbort;					// Set if we need to abort because something went wrong while the database is locked.

	//-------------------------------------------------------------------------

  protected:
	typedef AIStateMachine direct_base_type;

	enum front_end_state_type {
	  FrontEnd_lockDatabase = direct_base_type::max_state,
	  FrontEnd_findUploadedAsset,
	  FrontEnd_uploadedAssetFound,
	  FrontEnd_fetchInventory,
	  FrontEnd_findInInventory,
	  FrontEnd_uploadNewResource,
	  FrontEnd_doUpload,
	  FrontEnd_registerUpload,
	  FrontEnd_unlockDatabase,
	  FrontEnd_done
	};
  public:
	static state_type const max_state = FrontEnd_done + 1;

  public:
	FrontEnd(CWD_ONLY(bool debug = true));

	void setSourceFilename(std::string const& source_filename);
	void setSourceHash(LLMD5 const& source_md5, ESource type = one_on_one, LLPointer<Delta> const& delta = LLPointer<Delta>());
	void setAssetHash(LLMD5 const& asset_md5);

  protected:
	/*virtual*/ ~FrontEnd() { }

  public:
	// If true is returned one can check if the source file already was uploaded before
	// and call upload_new_resource_continued to do the actual upload.
	bool prepare_upload(std::string const& src_filename, bool bulk_upload, bool repair_database = false);

	// Return the filename passed to prepare_upload.
	std::string const& getSourceFilename(void) const { return mSourceFilename; }

	// Return asset type of upload as decoded by prepare_upload.
	LLAssetType::EType getAssetType(void) const { return mAssetType; }

	// Return asset hash of the upload as calculated by prepare_upload.
	LLMD5 const& getAssetHash(void) const { return mAssetHash; }

	// Return pointer to delta object, if any, as decoded by prepare_upload.
	LLPointer<Delta> const& getDelta(void) const { return mDelta; }

	// Return whether or not it's an upload in the native format.
	bool isNativeFormat(void) const { llassert(mAssetType != LLAssetType::AT_NONE); /* Call prepare_upload() first. */ return mNativeFormat; }

	// After calling prepare_upload() this can be called to determine if that source file was uploaded before.
	void find_previous_uploads(void);

	// After calling prepare_upload() this can be called to get a previous name & description, if any (calls find_previous_uploads).
	// Returns true and set id if it was uploaded to the current grid.
	bool checkPreviousUpload(std::string& name, std::string& description, LLUUID& id);

	// After calling prepare_upload() this can be called to get a list of all known previous uploads.
	std::vector<AIUploadedAsset*> checkPreviousUploads(void);

	// After a call to find_previous_uploads() this can be called to find out if the file was uploaded before.
	bool uploaded_before(void) const;

	// Pass a pointer to data on top of the source file, that was needed to generate the asset file.
	void addDelta(Delta* delta) { mDelta = delta; }

	// Upload the source file that was passed to prepare_upload.
	void upload_new_resource_continued(
		std::string const& name,
		std::string const& desc,
		S32 compression_info,
		LLFolderType::EType destination_folder_type,
		LLInventoryType::EType inv_type,
		U32 next_owner_perms,
		U32 group_perms,
		U32 everyone_perms,
		std::string const& display_name,
		S32 expected_upload_cost);

	// Return false if no upload attempt was done (and the callback will not be called).
	bool upload_new_resource(
		LLTransactionID const& tid,
		LLAssetType::EType type,
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
		UserCallback user_callback,								// Stored in mUserCallback.
		void* userdata,											// Stored in mUserData.
		UploadResponderFactoryBase const& responder_factory);

  private:
	using AIStateMachine::run;			// Make this private: call upload_new_resource, not run.

	// This is called once a call to callback() (or abort) is guaranteed.
	void run(void)
	{
	  AIStateMachine::run();
	}

	// Prelocked version of uploaded_before().
	bool uploaded_before(AIUploadedAsset_rat const& uploaded_asset_r) const;

	// The actual callback functions.
	static void callback(LLUUID const& uuid, void* userdata, bool success);
	static void legacyCallback(LLUUID const& uuid, void* userdata, S32 result, LLExtStat ext_status);

	// The old (pre- AIMultiGrid) callbacks, called from legacyCallback. However, user_data is no longer passed and the functions are not static.
	void upload_done_callback(LLUUID const& uuid, S32 result, LLExtStat ext_status);
	void temp_upload_callback(LLUUID const& uuid, S32 result, LLExtStat ext_status);
	// Helper function for those two.
	void get_name_and_description(std::string& name, std::string& description);

	// Return true if we have enough funds for the expected upload cost.
	bool checkFunds(void) const;

	// Called from prepare_upload when a fatal error occured and possibly a temporary file needs to be deleted.
	void upload_error(char const* label, AIArgs const& args);
	void upload_error(char const* label, AIArgs const& args, AIAlert::Error const& error);

	// Called when the uploaded finished.
	void uploadFinished(bool success, LLUUID const& uuid);

  protected:
	/*virtual*/ void initialize_impl(void);
	/*virtual*/ void multiplex_impl(state_type run_state);
	/*virtual*/ void abort_impl(void);
	/*virtual*/ void finish_impl(void);

	/*virtual*/ char const* state_str_impl(state_type run_state) const;
};

} // namespace AIMultiGrid

#endif // AIMULTIGRIDFRONTEND_H

