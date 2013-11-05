/** 
 * @file llfloaternamedesc.cpp
 * @brief LLFloaterNameDesc class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaternamedesc.h"

// project includes
#include "lllineeditor.h"
#include "llresmgr.h"
#include "lltextbox.h"
#include "llbutton.h"
#include "llviewerwindow.h"
#include "llfocusmgr.h"
#include "llradiogroup.h"
#include "lldbstrings.h"
#include "lldir.h"
#include "llfloaterperms.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "llstring.h"
#include "lleconomy.h"

// linden includes
#include "llassetstorage.h"
#include "llinventorytype.h"

#include "hippogridmanager.h"
#include "aimultigridfrontend.h"
#include "lltrans.h"
#include "llnameeditor.h"
#include "llcombobox.h"
#include "llwindow.h"
#include "llnotificationsutil.h"

const S32 PREVIEW_LINE_HEIGHT = 19;
const S32 PREVIEW_CLOSE_BOX_SIZE = 16;
const S32 PREVIEW_BORDER_WIDTH = 2;
const S32 PREVIEW_RESIZE_HANDLE_SIZE = S32(RESIZE_HANDLE_WIDTH * OO_SQRT2) + PREVIEW_BORDER_WIDTH;
const S32 PREVIEW_VPAD = 2;
const S32 PREVIEW_HPAD = PREVIEW_RESIZE_HANDLE_SIZE;
const S32 PREVIEW_HEADER_SIZE = 3 * PREVIEW_LINE_HEIGHT + PREVIEW_VPAD;
const S32 PREF_BUTTON_WIDTH = 64;
const S32 PREF_BUTTON_HEIGHT = 16;

//-----------------------------------------------------------------------------
// LLFloaterNameDesc()
//-----------------------------------------------------------------------------
LLFloaterNameDesc::LLFloaterNameDesc(LLPointer<AIMultiGrid::FrontEnd> const& front_end)
	: LLFloater(std::string("Name/Description Floater")),
	  mIsAudio(FALSE),
	  mFrontEnd(front_end),
	  mUserEdittedName(false),
	  mUserEdittedDescription(false)
{
	mFilenameAndPath = front_end->getSourceFilename();
	mFilename = gDirUtilp->getBaseFileName(mFilenameAndPath, false);
}

//-----------------------------------------------------------------------------
// postBuild()
//-----------------------------------------------------------------------------
BOOL LLFloaterNameDesc::postBuild()
{
	LLRect r;
	bool is_one_on_one = getChildView("asset_id", TRUE, FALSE) != NULL;
	mUploadedBefore = false;

	std::string asset_name, asset_description;

	for(U32 sleeptime = 100;; sleeptime <<= 2)
	{
	  try
	  {
		// Get previous name and description iff this asset was uploaded before to any grid,
		// but sets mExistingUUID and returns true only if it was uploaded to the current grid!
		if (is_one_on_one)
		{
		  mUploadedBefore = mFrontEnd->checkPreviousUpload(asset_name, asset_description, mExistingUUID);
		}
		else
		{
		  mPreviousUploads = mFrontEnd->checkPreviousUploads();
		  if (!mPreviousUploads.empty())
		  {
			AIUploadedAsset_rat ua_r(*mPreviousUploads[0]);			// Uploads to the current grid are sorted first, the last upload first.
			asset_name = ua_r->getName();
			asset_description = ua_r->getDescription();
			mUploadedBefore = ua_r->find_uuid();
		  }
		}
	  }
	  catch (AIMultiGrid::DatabaseLocked&)
	  {
		if (sleeptime < 2000)
		{
		  llwarns << "checkPreviousUpload() failed because the database is locked!" << llendl;
		  // Ugh! Lets try again a few times even though that freezes the viewer for 3 seconds.
		  ms_sleep(sleeptime);
		  continue;
		}
		AIAlert::add_modal("AIMultiGridDatabaseLocked");
	  }
	  catch (AIAlert::Error const& error)
	  {
		AIAlert::add_modal(error, "AIMultiGridAbortUploadRepairDatabase");
	  }
	  break;
	}

	if (asset_name.empty())
	{
		asset_name = gDirUtilp->getBaseFileName(mFilename, true); // Strip extension.
	}

	LLStringUtil::replaceNonstandardASCII( asset_name, '?' );
	LLStringUtil::replaceChar(asset_name, '|', '?');
	LLStringUtil::stripNonprintable(asset_name);
	LLStringUtil::trim(asset_name);

	// In case users editted the uploaded database...
	LLStringUtil::replaceNonstandardASCII( asset_description, '?' );
	LLStringUtil::replaceChar(asset_description, '|', '?');
	LLStringUtil::stripNonprintable(asset_description);
	LLStringUtil::trim(asset_description);

	setTitle(mFilename);

	centerWithin(gViewerWindow->getRootView()->getRect());

	S32 line_width = getRect().getWidth() - 2 * PREVIEW_HPAD;
	S32 y = getRect().getHeight() - 2 * PREVIEW_LINE_HEIGHT;

	r.setLeftTopAndSize( PREVIEW_HPAD, y, line_width, PREVIEW_LINE_HEIGHT );    

	getChild<LLUICtrl>("name_form")->setCommitCallback(boost::bind(&LLFloaterNameDesc::doCommitName, this));
	getChild<LLUICtrl>("name_form")->setValue(LLSD(asset_name));

	LLLineEditor *NameEditor = getChild<LLLineEditor>("name_form");
	if (NameEditor)
	{
		NameEditor->setMaxTextLength(DB_INV_ITEM_NAME_STR_LEN);
		NameEditor->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);
	}

	y -= llfloor(PREVIEW_LINE_HEIGHT * 1.2f);
	y -= PREVIEW_LINE_HEIGHT;

	r.setLeftTopAndSize( PREVIEW_HPAD, y, line_width, PREVIEW_LINE_HEIGHT );  
	getChild<LLUICtrl>("description_form")->setCommitCallback(boost::bind(&LLFloaterNameDesc::doCommitDescription, this));
	getChild<LLUICtrl>("description_form")->setValue(LLSD(asset_description));

	LLLineEditor *DescEditor = getChild<LLLineEditor>("description_form");
	if (DescEditor)
	{
		DescEditor->setMaxTextLength(DB_INV_ITEM_DESC_STR_LEN);
		DescEditor->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);
	}

	y -= llfloor(PREVIEW_LINE_HEIGHT * 1.2f);

	// Cancel button
	getChild<LLUICtrl>("cancel_btn")->setCommitCallback(boost::bind(&LLFloaterNameDesc::onBtnCancel, this));

	getChild<LLUICtrl>("ok_btn")->setLabelArg("[UPLOADFEE]", llformat("%s%d", gHippoGridManager->getConnectedGrid()->getCurrencySymbol().c_str(), LLGlobalEconomy::Singleton::getInstance()->getPriceUpload()));

	setDefaultBtn("ok_btn");

	return TRUE;
}

void LLFloaterNameDesc::postBuildUploadedBefore(std::string const& asset_name, S32 extend)
{
	// Only one of these exists. The first one for one-on-one sources, when the source file
	// being uploaded is linked to an asset UUID. The second one when for source with a
	// delta (currently only BVH animation files), when there can be a whole list of
	// previously uploaded assets related to one source file.
	LLView* asset_id = getChildView("asset_id", TRUE, FALSE);
	LLView* previously_uploaded = getChildView("previously_uploaded", TRUE, FALSE);

	// One on one assets have the "asset_id" child, others don't (but have the "previously_uploaded" child).
	bool const is_one_on_one = asset_id != NULL;

	// uploaded_before is set when this asset (or the currently selected one in the case of a one-to-many asset) was uploaded before to the *current* grid.
	BOOL const uploaded_before = mUploadedBefore ? TRUE : FALSE;

	if (uploaded_before)
	{
	  if (is_one_on_one)
	  {
		// Show the "already exists" stuff.

		// Move the bottom of the floater 'extend' pixels down.
		LLRect rect = getRect();
		rect.mBottom -= extend;
		reshape(rect.getWidth(), rect.getHeight(), TRUE);

		// Fill in the grid name in the "already_exists_text".
		LLSD args;
		args["ASSET"] = asset_name;
		args["GRID"] = gHippoGridManager->getConnectedGrid()->getGridNick();
		childSetText("already_exists_text", LLTrans::getString("already_uploaded", args));
		// Fill in the UUID.
		static_cast<LLNameEditor*>(asset_id)->setText(mExistingUUID.asString());
	  }
	  // Link the "copy_uuid" button.
	  childSetAction("copy_uuid", &LLFloaterNameDesc::onClickGetKey, this);
	}
	if (previously_uploaded)
	{
	  // The combo box is currently always shown.
	  LLComboBox* combo = static_cast<LLComboBox*>(previously_uploaded);
	  combo->removeall();
	  combo->setCommitCallback(boost::bind(&LLFloaterNameDesc::onChangePreset, this, _1));
	  AIMultiGrid::Grid const current_grid(gHippoGridManager->getConnectedGrid()->getGridNick());
	  LLSD::Integer id = 0;
	  for (std::vector<AIUploadedAsset*>::iterator iter = mPreviousUploads.begin(); iter != mPreviousUploads.end(); ++iter, ++id)
	  {
		AIUploadedAsset_rat ua_r(**iter);
		LLSD value;
		value["id"] = id;
		value["name"] = ua_r->getName();
		value["description"] = ua_r->getDescription();
		std::string text = ua_r->getName() + " - " + ua_r->getDate().asString() + " - ";
		bool added_grid = false;
		if (ua_r->find_uuid())
		{
		  text += current_grid.getGridNick();
		  added_grid = true;
		}
		AIMultiGrid::grid2uuid_map_type const& grid2uuid_map = ua_r->getGrid2UUIDmap();
		for (AIMultiGrid::grid2uuid_map_type::const_iterator grid = grid2uuid_map.begin(); grid != grid2uuid_map.end(); ++grid)
		{
		  if (grid->getGrid() == current_grid)
		  {
			value["uuid"] = grid->getUUID();
			continue;
		  }
		  if (added_grid)
		  {
			text += "/";
		  }
		  text += grid->getGrid().getGridNick();
		  added_grid = true;
		}
		LLScrollListItem* item = combo->add(text, value);
		LLScrollListCell* column = item->getColumn(0);
		column->setToolTip(ua_r->getDescription());
	  }
	  // Only enable the combo box when it is not empty.
	  combo->setEnabled(combo->selectFirstItem());
	}

	// Fix visibility.
	if (is_one_on_one)
	{
	  getChildView("already_exists_text")->setVisible(uploaded_before);
	  asset_id->setVisible(uploaded_before);
	  getChildView("copy_uuid")->setVisible(uploaded_before);
	}
	else
	{
	  getChildView("copy_uuid")->setEnabled(uploaded_before);
	  previously_uploaded->setEnabled(!mPreviousUploads.empty());
	}
}

void LLFloaterNameDesc::onChangePreset(LLUICtrl* ctrl)
{
	LLComboBox* combo_box = static_cast<LLComboBox*>(ctrl);
	LLSD const& value = combo_box->getSelectedValue();
	AIUploadedAsset* uploaded_asset = mPreviousUploads[value["id"].asInteger()];
	AIUploadedAsset_wat uploaded_asset_w(*uploaded_asset);
	onChangePresetDelta(uploaded_asset_w->getDelta());
	if (value.has("uuid"))
	{
	  getChildView("copy_uuid")->setEnabled(TRUE);
	  mExistingUUID = value["uuid"].asUUID();
	}
	else
	{
	  getChildView("copy_uuid")->setEnabled(FALSE);
	  mExistingUUID.setNull();
	}
	// Change name and/or description fields.
	if (!mUserEdittedName)
	{
	  getChildView("name_form")->setValue(value["name"]);
	}
	if (!mUserEdittedDescription)
	{
	  getChildView("description_form")->setValue(value["description"]);
	}
	// Change 'Upload' to 'Reupload' and fill in the upload fee.
	LLSD args;
	args["UPLOADFEE"] = gHippoGridManager->getConnectedGrid()->getUploadFee();
	getChild<LLButton>("ok_btn")->setLabel(LLTrans::getString("reupload", args));
}

//static
//-----------------------------------------------------------------------------
// onClickGetKey()
//-----------------------------------------------------------------------------
void LLFloaterNameDesc::onClickGetKey(void* userdata)
{
    LLFloaterNameDesc* self = (LLFloaterNameDesc*)userdata;
    LLUUID asset_id = self->mExistingUUID;

    llinfos << "Copy asset id: " << asset_id << llendl;

    gViewerWindow->getWindow()->copyTextToClipboard(utf8str_to_wstring(asset_id.asString()));
}

//-----------------------------------------------------------------------------
// LLFloaterNameDesc()
//-----------------------------------------------------------------------------
LLFloaterNameDesc::~LLFloaterNameDesc()
{
	gFocusMgr.releaseFocusIfNeeded( this ); // calls onCommit()
}

// Sub-classes should override this function if they allow editing
//-----------------------------------------------------------------------------
// onCommit()
//-----------------------------------------------------------------------------
void LLFloaterNameDesc::onCommit()
{
}

//-----------------------------------------------------------------------------
// onCommit()
//-----------------------------------------------------------------------------
void LLFloaterNameDesc::doCommitName()
{
	mUserEdittedName = true;
	onCommit();
}

void LLFloaterNameDesc::doCommitDescription()
{
	mUserEdittedDescription = true;
	onCommit();
}

//-----------------------------------------------------------------------------
// onBtnOK()
//-----------------------------------------------------------------------------
void LLFloaterNameDesc::onBtnOK()
{
	getChildView("ok_btn")->setEnabled(FALSE); // don't allow inadvertent extra uploads
	
	S32 expected_upload_cost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload(); // kinda hack - assumes that unsubclassed LLFloaterNameDesc is only used for uploading chargeable assets.
	std::string display_name = LLStringUtil::null;
	mFrontEnd->upload_new_resource_continued(
			getChild<LLUICtrl>("name_form")->getValue().asString(),
			getChild<LLUICtrl>("description_form")->getValue().asString(),
			0, LLFolderType::FT_NONE, LLInventoryType::IT_NONE,
			LLFloaterPerms::getNextOwnerPerms(), LLFloaterPerms::getGroupPerms(), LLFloaterPerms::getEveryonePerms(),
			display_name, expected_upload_cost);
	close(false);
}

//-----------------------------------------------------------------------------
// onBtnCancel()
//-----------------------------------------------------------------------------
void LLFloaterNameDesc::onBtnCancel()
{
	close(false);
}


//-----------------------------------------------------------------------------
// LLFloaterSoundPreview()
//-----------------------------------------------------------------------------

LLFloaterSoundPreview::LLFloaterSoundPreview(LLPointer<AIMultiGrid::FrontEnd> const& front_end) : LLFloaterNameDesc(front_end)
{
	mIsAudio = TRUE;
}

BOOL LLFloaterSoundPreview::postBuild()
{
	if (!LLFloaterNameDesc::postBuild())
	{
		return FALSE;
	}
	// If already uploaded before, move the buttons 42 pixels down and show the 'already_exists' text, the UUID and the "copy UUID" button.
	postBuildUploadedBefore("sound", 42);
	getChild<LLUICtrl>("ok_btn")->setCommitCallback(boost::bind(&LLFloaterNameDesc::onBtnOK, this));
	return TRUE;
}


//-----------------------------------------------------------------------------
// LLFloaterAnimPreview()
//-----------------------------------------------------------------------------

LLFloaterAnimPreview::LLFloaterAnimPreview(LLPointer<AIMultiGrid::FrontEnd> const& front_end) : LLFloaterNameDesc(front_end)
{
}

BOOL LLFloaterAnimPreview::postBuild()
{
	if (!LLFloaterNameDesc::postBuild())
	{
		return FALSE;
	}
	// If already uploaded before, move the buttons 42 pixels down and show the 'already_exists' text, the UUID and the "copy UUID" button.
	postBuildUploadedBefore("animation", 42);
	getChild<LLUICtrl>("ok_btn")->setCommitCallback(boost::bind(&LLFloaterNameDesc::onBtnOK, this));
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLFloaterScriptPreview()
//-----------------------------------------------------------------------------

LLFloaterScriptPreview::LLFloaterScriptPreview(LLPointer<AIMultiGrid::FrontEnd> const& front_end) : LLFloaterNameDesc(front_end)
{
	mIsText = TRUE;
}

BOOL LLFloaterScriptPreview::postBuild()
{
	if (!LLFloaterNameDesc::postBuild())
	{
		return FALSE;
	}
	getChild<LLUICtrl>("ok_btn")->setCommitCallback(boost::bind(&LLFloaterNameDesc::onBtnOK, this));
	return TRUE;
}
