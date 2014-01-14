/** 
 * @file llfloaternamedesc.h
 * @brief LLFloaterNameDesc class definition
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

#ifndef LL_LLFLOATERNAMEDESC_H
#define LL_LLFLOATERNAMEDESC_H

#include "llfloater.h"
#include "llresizehandle.h"
#include "llstring.h"

namespace AIMultiGrid {
  class FrontEnd;
  class Delta;
}

class LLLineEditor;
class LLButton;
class LLRadioGroup;
class AIUploadedAsset;

class LLFloaterNameDesc : public LLFloater
{
public:
	explicit LLFloaterNameDesc(LLPointer<AIMultiGrid::FrontEnd> const& front_end);
	virtual ~LLFloaterNameDesc();

protected:
	virtual BOOL postBuild();
	static void onClickGetKey(void* userdata);
	//<singu>
	void updateUploadedBefore(std::string const& asset_name, S32 extend);
	void check_previous_uploads_and_set_name_description_and_uuid();
	void onChangePreset(LLUICtrl* ctrl);
	virtual void onChangePresetDelta(AIMultiGrid::Delta*) { }
	void checkForPreset(AIMultiGrid::Delta const* delta);
	//</singu>

public:
	void		onBtnOK();
	void		onBtnCancel();
	void		doCommitName();
	void		doCommitDescription();

protected:
	virtual void		onCommit();

protected:
	BOOL        mIsAudio;
	bool		mIsText;

	std::string		mFilenameAndPath;
	std::string		mFilename;
	// <singu>
	LLPointer<AIMultiGrid::FrontEnd> mFrontEnd;		// The upload manager state machine.
	bool mUploadedBefore;							// True if this asset already exists on the _current_ grid.
	LLUUID mExistingUUID;							// UUID of previously uploaded asset. Only valid when mUploadedBefore is true.
	std::vector<AIUploadedAsset*> mPreviousUploads;	// A list of all previous uploads (to any grid), only set for one_source_many_assets source types.
	bool			mUserEdittedName;
	bool			mUserEdittedDescription;
	bool			mExtended;
	// </singu>
};

class LLFloaterSoundPreview : public LLFloaterNameDesc
{
public:
	explicit LLFloaterSoundPreview(LLPointer<AIMultiGrid::FrontEnd> const& front_end);
	virtual BOOL postBuild();
};

class LLFloaterAnimPreview : public LLFloaterNameDesc
{
public:
	explicit LLFloaterAnimPreview(LLPointer<AIMultiGrid::FrontEnd> const& front_end);
	virtual BOOL postBuild();
};

class LLFloaterScriptPreview : public LLFloaterNameDesc
{
public:
	explicit LLFloaterScriptPreview(LLPointer<AIMultiGrid::FrontEnd> const& front_end);
	virtual BOOL postBuild();
};

#endif  // LL_LLFLOATERNAMEDESC_H
