/**
 * @file llfloaterbvhpreview.cpp
 * @brief LLFloaterBvhPreview class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llfloaterbvhpreview.h"

#include "llbvhloader.h"
#include "lldatapacker.h"
#include "lldir.h"
#include "lleconomy.h"
#include "llnotificationsutil.h"
#include "llvfile.h"
#include "llapr.h"
#include "llstring.h"

#include "llagent.h"
#include "llanimationstates.h"
#include "llbbox.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldrawable.h"
#include "lldrawpoolavatar.h"
#include "llrender.h"
#include "llface.h"
#include "llfocusmgr.h"
#include "llkeyframemotion.h"
#include "lllineeditor.h"
#include "llfloaterperms.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltoolmgr.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "pipeline.h"
#include "lluictrlfactory.h"
#include "llassetuploadresponders.h"

#include "hippogridmanager.h"

//<edit>
#include "lltrans.h"				// LLTrans
#include "llinventorymodel.h" 		// gInventoryModel
#include "aimultigridfrontend.h"
//</edit>

const S32 PREVIEW_BORDER_WIDTH = 2;
const S32 PREVIEW_RESIZE_HANDLE_SIZE = S32(RESIZE_HANDLE_WIDTH * OO_SQRT2) + PREVIEW_BORDER_WIDTH;
const S32 PREVIEW_HPAD = PREVIEW_RESIZE_HANDLE_SIZE;
const S32 PREF_BUTTON_HEIGHT = 16;
const S32 PREVIEW_TEXTURE_HEIGHT = 300;

const F32 PREVIEW_CAMERA_DISTANCE = 4.f;

const F32 MIN_CAMERA_ZOOM = 0.5f;
const F32 MAX_CAMERA_ZOOM = 10.f;

const F32 BASE_ANIM_TIME_OFFSET = 5.f;

// <edit>
struct LLSaveInfo
{
	LLSaveInfo(const LLUUID& item_id, const LLUUID& object_id, const std::string& desc,
				const LLTransactionID tid)
		: mItemUUID(item_id), mObjectUUID(object_id), mDesc(desc), mTransactionID(tid)
	{
	}

	LLUUID mItemUUID;
	LLUUID mObjectUUID;
	std::string mDesc;
	LLTransactionID mTransactionID;
};
// </edit>

std::string STATUS[] =
{
	"E_ST_OK",
	"E_ST_EOF",
	"E_ST_NO_CONSTRAINT",
	"E_ST_NO_FILE",
	"E_ST_NO_HIER",
	"E_ST_NO_JOINT",
	"E_ST_NO_NAME",
	"E_ST_NO_OFFSET",
	"E_ST_NO_CHANNELS",
	"E_ST_NO_ROTATION",
	"E_ST_NO_AXIS",
	"E_ST_NO_MOTION",
	"E_ST_NO_FRAMES",
	"E_ST_NO_FRAME_TIME",
	"E_ST_NO_POS",
	"E_ST_NO_ROT",
	"E_ST_NO_XLT_FILE",
	"E_ST_NO_XLT_HEADER",
	"E_ST_NO_XLT_NAME",
	"E_ST_NO_XLT_IGNORE",
	"E_ST_NO_XLT_RELATIVE",
	"E_ST_NO_XLT_OUTNAME",
	"E_ST_NO_XLT_MATRIX",
	"E_ST_NO_XLT_MERGECHILD",
	"E_ST_NO_XLT_MERGEPARENT",
	"E_ST_NO_XLT_PRIORITY",
	"E_ST_NO_XLT_LOOP",
	"E_ST_NO_XLT_EASEIN",
	"E_ST_NO_XLT_EASEOUT",
	"E_ST_NO_XLT_HAND",
	"E_ST_NO_XLT_EMOTE",
"E_ST_BAD_ROOT"
};

//-----------------------------------------------------------------------------
// LLFloaterBvhPreview()
//-----------------------------------------------------------------------------
LLFloaterBvhPreview::LLFloaterBvhPreview(LLPointer<AIMultiGrid::FrontEnd> const& front_end) : LLFloaterNameDesc(front_end)
{
	mLastMouseX = 0;
	mLastMouseY = 0;

	mIDList["Standing"] = ANIM_AGENT_STAND;
	mIDList["Walking"] = ANIM_AGENT_FEMALE_WALK;
	mIDList["Sitting"] = ANIM_AGENT_SIT_FEMALE;
	mIDList["Flying"] = ANIM_AGENT_HOVER;

	mIDList["[None]"] = LLUUID::null;
	mIDList["Aaaaah"] = ANIM_AGENT_EXPRESS_OPEN_MOUTH;
	mIDList["Afraid"] = ANIM_AGENT_EXPRESS_AFRAID;
	mIDList["Angry"] = ANIM_AGENT_EXPRESS_ANGER;
	mIDList["Big Smile"] = ANIM_AGENT_EXPRESS_TOOTHSMILE;
	mIDList["Bored"] = ANIM_AGENT_EXPRESS_BORED;
	mIDList["Cry"] = ANIM_AGENT_EXPRESS_CRY;
	mIDList["Disdain"] = ANIM_AGENT_EXPRESS_DISDAIN;
	mIDList["Embarrassed"] = ANIM_AGENT_EXPRESS_EMBARRASSED;
	mIDList["Frown"] = ANIM_AGENT_EXPRESS_FROWN;
	mIDList["Kiss"] = ANIM_AGENT_EXPRESS_KISS;
	mIDList["Laugh"] = ANIM_AGENT_EXPRESS_LAUGH;
	mIDList["Plllppt"] = ANIM_AGENT_EXPRESS_TONGUE_OUT;
	mIDList["Repulsed"] = ANIM_AGENT_EXPRESS_REPULSED;
	mIDList["Sad"] = ANIM_AGENT_EXPRESS_SAD;
	mIDList["Shrug"] = ANIM_AGENT_EXPRESS_SHRUG;
	mIDList["Smile"] = ANIM_AGENT_EXPRESS_SMILE;
	mIDList["Surprise"] = ANIM_AGENT_EXPRESS_SURPRISE;
	mIDList["Wink"] = ANIM_AGENT_EXPRESS_WINK;
	mIDList["Worry"] = ANIM_AGENT_EXPRESS_WORRY;

	//<edit>
	mPresetting = false;
	//</edit>
}

//-----------------------------------------------------------------------------
// setAnimCallbacks()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::setAnimCallbacks()
{
	getChild<LLUICtrl>("playback_slider")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onSliderMove, this));
	
	getChild<LLUICtrl>("preview_base_anim")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitBaseAnim, this));
	getChild<LLUICtrl>("preview_base_anim")->setValue("Standing");

	getChild<LLUICtrl>("priority")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitPriority, this));
	getChild<LLUICtrl>("loop_check")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoop, this));
	getChild<LLUICtrl>("loop_in_point")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoopIn, this));
	getChild<LLUICtrl>("loop_in_point")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateLoopIn, this, _1));
	getChild<LLUICtrl>("loop_out_point")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoopOut, this));
	getChild<LLUICtrl>("loop_out_point")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateLoopOut, this, _1));

	getChild<LLUICtrl>("hand_pose_combo")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitHandPose, this));
	
	getChild<LLUICtrl>("emote_combo")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitEmote, this));
	getChild<LLUICtrl>("emote_combo")->setValue("[None]");

	getChild<LLUICtrl>("ease_in_time")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitEaseIn, this));
	getChild<LLUICtrl>("ease_in_time")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateEaseIn, this, _1));
	getChild<LLUICtrl>("ease_out_time")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitEaseOut, this));
	getChild<LLUICtrl>("ease_out_time")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateEaseOut, this, _1));
}

// Singu note: this function used to be part of LLFloaterBvhPreview::postBuild below.
bool AIBVHLoader::loadbvh(std::string const& filename, bool in_world, LLPointer<AIMultiGrid::FrontEnd> front_end)
{
	bool success = false;

	// Load the bvh file.
	S32 file_size;

	LLAPRFile infile(filename, LL_APR_RB, &file_size);

	if (!infile.getFileHandle())
	{
		llwarns << "Can't open BVH file:" << filename << llendl;
	}
	else
	{
		ELoadStatus load_status;
		char* file_buffer = new char[file_size + 1];

		if (file_size == infile.read(file_buffer, file_size))
		{
			S32 error_line;
			file_buffer[file_size] = '\0';
			llinfos << "Loading BVH file " << filename << llendl;
			mLoader = new LLBVHLoader(file_buffer, load_status, error_line);
		}

		infile.close();
		delete[] file_buffer;

		if (mLoader && mLoader->isInitialized() && load_status == E_ST_OK && mLoader->getDuration() <= MAX_ANIM_DURATION)
		{
			// generate unique id for this motion
			mTransactionID.generate();
			mMotionID = mTransactionID.makeAssetID(gAgent.getSecureSessionID());

			mAnimPreview = new LLPreviewAnimation(256, 256);

			// motion will be returned, but it will be in a load-pending state, as this is a new motion
			// this motion will not request an asset transfer until next update, so we have a chance to
			// load the keyframe data locally
			if (in_world)
				mMotionp = (LLKeyframeMotion*)gAgentAvatarp->createMotion(mMotionID);
			else
				mMotionp = (LLKeyframeMotion*)mAnimPreview->getDummyAvatar()->createMotion(mMotionID);

			// create data buffer for keyframe initialization
			S32 buffer_size = mLoader->getOutputSize();
			U8* buffer = new U8[buffer_size];

			LLDataPackerBinaryBuffer dp(buffer, buffer_size);

			// pass animation data through memory buffer
			mLoader->serialize(dp);
			dp.reset();
			if (mMotionp)
			{
				LLMD5 source_md5;
				try
				{
					mMotionp->deserialize(dp, source_md5);

					source_md5.finalize();
					LLPointer<AIMultiGrid::Delta> delta = new AIMultiGrid::BVHAnimDelta(mMotionp->getDelta());
					front_end->setSourceHash(source_md5, AIMultiGrid::FrontEnd::one_source_many_assets, delta);
					success = true;
				}
				catch (AIAlert::Error const& error)
				{
					AIAlert::add_modal("AIProblemWithFile_ERROR", AIArgs("[FILE]", filename)("[ERROR]", LLTrans::getString("AICouldNotDecodeJustGeneratedAnim")), error);
				}
			}
		}
		if (!success)
		{
			if (mMotionp && in_world)
			{
				gAgentAvatarp->removeMotion(mMotionID);
			}
			mTransactionID.setNull();
			mMotionID.setNull();
			mAnimPreview = NULL;
		}
	}
	return success;
}

//-----------------------------------------------------------------------------
// postBuild()
//-----------------------------------------------------------------------------
BOOL LLFloaterBvhPreview::postBuild()
{
	LLRect r;

	if (!LLFloaterNameDesc::postBuild())
	{
		return FALSE;
	}

	mInWorld = gSavedSettings.getBOOL("PreviewAnimInWorld");

	getChild<LLUICtrl>("name_form")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitName, this));

	if (gSavedSettings.getBOOL("AscentPowerfulWizard"))
	{
		getChild<LLSlider>("priority")->setMaxValue(7);
	}

	childSetLabelArg("ok_btn", "[UPLOADFEE]", gHippoGridManager->getConnectedGrid()->getUploadFee());
	childSetAction("ok_btn", onBtnOK, this);
	setDefaultBtn();

	if (mInWorld)
	{
		r = getRect();
		translate(0, 230);
		reshape(r.getWidth(), r.getHeight() - 230);
		getChild<LLUICtrl>("bad_animation_text")->setValue(getString("in_world"));
		getChildView("bad_animation_text")->setVisible(TRUE);
	}
	else
	{
		getChildView("bad_animation_text")->setVisible(FALSE);
	}

	mPreviewRect.set(PREVIEW_HPAD, 
		PREVIEW_TEXTURE_HEIGHT,
		getRect().getWidth() - PREVIEW_HPAD, 
		PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
	mPreviewImageRect.set(0.f, 1.f, 1.f, 0.f);

	S32 y = mPreviewRect.mTop + BTN_HEIGHT;
	S32 btn_left = PREVIEW_HPAD;

	r.set( btn_left, y, btn_left + 32, y - BTN_HEIGHT );
	mPlayButton = getChild<LLButton>( "play_btn");
	mPlayButton->setClickedCallback(boost::bind(&LLFloaterBvhPreview::onBtnPlay,this));

	mPlayButton->setImages(std::string("button_anim_play.tga"),
						   std::string("button_anim_play_selected.tga"));

	mPlayButton->setImageDisabled(NULL);
	mPlayButton->setImageDisabledSelected(NULL);

	mPlayButton->setScaleImage(TRUE);

	mStopButton = getChild<LLButton>( "stop_btn");
	mStopButton->setClickedCallback(boost::bind(&LLFloaterBvhPreview::onBtnStop, this));

	mStopButton->setImages(std::string("button_anim_stop.tga"),
						   std::string("button_anim_stop_selected.tga"));

	mStopButton->setImageDisabled(NULL);
	mStopButton->setImageDisabledSelected(NULL);

	mStopButton->setScaleImage(TRUE);

	r.set(r.mRight + PREVIEW_HPAD, y, getRect().getWidth() - PREVIEW_HPAD, y - BTN_HEIGHT);

	//<edit>
	// This is where the code that is now (mostly) in AIBVHLoader::loadbvh used to be.
	AIBVHLoader loader;
	bool success = loader.loadbvh(mFilenameAndPath, mInWorld, mFrontEnd);

	LLKeyframeMotion* motionp = NULL;	// Avoid compiler warning.
	if (success)
	{
		mTransactionID = loader.getTransactionID();
		mMotionID = loader.getMotionID();
		mAnimPreview = loader.getAnimPreview();
		motionp = loader.getMotionp();
		mDuration = loader.getDuration();
	}
	else if (loader.getStatus() != E_ST_OK)
	{
		if (loader.getStatus() == E_ST_NO_XLT_FILE)
		{
			llwarns << "NOTE: No translation table found." << llendl;
		}
		else
		{
			std::string status = getString(STATUS[loader.getStatus()]);
			llwarns << "ERROR: [line: " << loader.getLineNumber() << "] " << status << llendl;
		}
	}
	else if (loader.getDuration() > MAX_ANIM_DURATION)
	{
		LLUIString out_str = getString("anim_too_long");
		out_str.setArg("[LENGTH]", llformat("%.1f", loader.getDuration()));
		out_str.setArg("[MAX_LENGTH]", llformat("%.1f", MAX_ANIM_DURATION));
		getChild<LLUICtrl>("bad_animation_text")->setValue(out_str.getString());
	}
	else
	{
		LLUIString out_str = getString("failed_file_read");
		out_str.setArg("[STATUS]", "");	// There is no status if we get here.
		getChild<LLUICtrl>("bad_animation_text")->setValue(out_str.getString());
	}
	//</edit>

	// If already uploaded before, move the buttons 20 pixels down and show the previously uploaded combobox, and the "copy UUID" button.
	postBuildUploadedBefore("animation", 40);

		if (success)
		{
			setAnimCallbacks() ;

			if (!mInWorld)
			{
				const LLBBoxLocal &pelvis_bbox = motionp->getPelvisBBox();

				LLVector3 temp = pelvis_bbox.getCenter();
				// only consider XY?
				//temp.mV[VZ] = 0.f;
				F32 pelvis_offset = temp.magVec();

				temp = pelvis_bbox.getExtent();
				//temp.mV[VZ] = 0.f;
				F32 pelvis_max_displacement = pelvis_offset + (temp.magVec() * 0.5f) + 1.f;

				F32 camera_zoom = LLViewerCamera::getInstance()->getDefaultFOV() / (2.f * atan(pelvis_max_displacement / PREVIEW_CAMERA_DISTANCE));

				mAnimPreview->setZoom(camera_zoom);
			}

			motionp->setName(getChild<LLUICtrl>("name_form")->getValue().asString());
			if (!mInWorld)
				mAnimPreview->getDummyAvatar()->startMotion(mMotionID);

			getChild<LLSlider>("playback_slider")->setMinValue(0.0);
			getChild<LLSlider>("playback_slider")->setMaxValue(1.0);

			getChild<LLUICtrl>("loop_check")->setValue(LLSD(motionp->getLoop()));
			getChild<LLUICtrl>("loop_in_point")->setValue(LLSD(motionp->getLoopIn() / motionp->getDuration() * 100.f));
			getChild<LLUICtrl>("loop_out_point")->setValue(LLSD(motionp->getLoopOut() / motionp->getDuration() * 100.f));
			getChild<LLUICtrl>("priority")->setValue(LLSD((F32)motionp->getPriority()));
			getChild<LLUICtrl>("hand_pose_combo")->setValue(LLHandMotion::getHandPoseName(motionp->getHandPose()));
			getChild<LLUICtrl>("ease_in_time")->setValue(LLSD(motionp->getEaseInDuration()));
			getChild<LLUICtrl>("ease_out_time")->setValue(LLSD(motionp->getEaseOutDuration()));
			setEnabled(TRUE);
			std::string seconds_string;
			seconds_string = llformat(" - %.2f seconds", motionp->getDuration());

			setTitle(mFilename + std::string(seconds_string));

			// Set everything to the first preset, if any.
			if (!mPreviousUploads.empty())
			{
			  onChangePreset(getChild<LLUICtrl>("previously_uploaded"));
			}
		}
		else
		{
			mAnimPreview = NULL;
			mMotionID.setNull();
			getChild<LLUICtrl>("bad_animation_text")->setValue(getString("failed_to_initialize"));
		}

	refresh();

	return TRUE;
}

void LLFloaterBvhPreview::onChangePresetDelta(AIMultiGrid::Delta* delta)
{
	AIMultiGrid::BVHAnimDelta* bvh_anim_delta = static_cast<AIMultiGrid::BVHAnimDelta*>(delta);
	if (bvh_anim_delta)
	{
		// Update the widgets to the new preset.
		getChild<LLUICtrl>("priority")->setValue(LLSD((F32)bvh_anim_delta->getBasePriority()));
		getChild<LLUICtrl>("loop_check")->setValue(LLSD((BOOL)bvh_anim_delta->getLoop()));
		getChild<LLUICtrl>("loop_in_point")->setValue(LLSD(bvh_anim_delta->getLoopInPoint() / mDuration * 100.f));
		getChild<LLUICtrl>("loop_out_point")->setValue(LLSD(bvh_anim_delta->getLoopOutPoint() / mDuration * 100.f));
		getChild<LLUICtrl>("hand_pose_combo")->setValue(LLHandMotion::getHandPoseName((LLHandMotion::eHandPose)bvh_anim_delta->getHandPose()));
		LLComboBox* emote_box = getChild<LLComboBox>("emote_combo");
		LLUUID anim_state = gAnimLibrary.stringToAnimState(bvh_anim_delta->getEmoteName(), FALSE);
		for (std::map<std::string, LLUUID>::iterator iter = mIDList.begin(); iter != mIDList.end(); ++iter)
		{
		  if (iter->second == anim_state)
		  {
			emote_box->setValue(iter->first);
			break;
		  }
		}
		getChild<LLUICtrl>("ease_in_time")->setValue(LLSD(bvh_anim_delta->getEaseInDuration()));
		getChild<LLUICtrl>("ease_out_time")->setValue(LLSD(bvh_anim_delta->getEaseOutDuration()));
		// Commit the new preset.
		mPresetting = true;
		onCommitPriority();
		onCommitLoop();
		onCommitEaseIn();
		onCommitEaseOut();
		mPresetting = false;
	}
}

void LLFloaterBvhPreview::checkForPreset(void)
{
	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	LLSD args;
	args["UPLOADFEE"] = gHippoGridManager->getConnectedGrid()->getUploadFee();
	LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
	AIMultiGrid::BVHAnimDelta const& bvh_anim_delta = motionp->getDelta();
	LLSD::Integer id = 0;
	for (std::vector<AIUploadedAsset*>::iterator preset = mPreviousUploads.begin(); preset != mPreviousUploads.end(); ++preset, ++id)
	{
		AIUploadedAsset_rat AIUploadedAsset_r(**preset);
		AIMultiGrid::BVHAnimDelta const* delta = dynamic_cast<AIMultiGrid::BVHAnimDelta const*>(AIUploadedAsset_r->getDelta().get());
		if (delta && delta->equals(bvh_anim_delta))
		{
			getChild<LLComboBox>("previously_uploaded")->setCurrentByIndex(id);
			if (!mUserEdittedName)
			{
			  getChildView("name_form")->setValue(LLSD(AIUploadedAsset_r->getName()));
			}
			if (!mUserEdittedDescription)
			{
			  getChildView("description_form")->setValue(LLSD(AIUploadedAsset_r->getDescription()));
			}
			LLUUID const* uuid = AIUploadedAsset_r->find_uuid();
			if (uuid)
			{
				mExistingUUID = *uuid;
				getChildView("copy_uuid")->setEnabled(TRUE);
				getChild<LLButton>("ok_btn")->setLabel(LLTrans::getString("reupload", args));
				return;
			}
			break;
		}
	}
	mExistingUUID.setNull();
	getChildView("copy_uuid")->setEnabled(FALSE);
	getChild<LLButton>("ok_btn")->setLabel(LLTrans::getString("upload", args));
}

//-----------------------------------------------------------------------------
// LLFloaterBvhPreview()
//-----------------------------------------------------------------------------
LLFloaterBvhPreview::~LLFloaterBvhPreview()
{
	if (mInWorld)
	{
		LLVOAvatar* avatarp = gAgentAvatarp;
		if (avatarp)
		{
			if (mMotionID.notNull())
			{
				avatarp->stopMotion(mMotionID, TRUE);
				avatarp->removeMotion(mMotionID);
			}
			avatarp->deactivateAllMotions();
			avatarp->startMotion(ANIM_AGENT_HEAD_ROT);
			avatarp->startMotion(ANIM_AGENT_EYE);
			avatarp->startMotion(ANIM_AGENT_BODY_NOISE);
			avatarp->startMotion(ANIM_AGENT_BREATHE_ROT);
			avatarp->startMotion(ANIM_AGENT_HAND_MOTION);
			avatarp->startMotion(ANIM_AGENT_PELVIS_FIX);
			avatarp->startMotion(ANIM_AGENT_STAND, BASE_ANIM_TIME_OFFSET);
		}
	}
	mAnimPreview = NULL;

	setEnabled(FALSE);
}

//-----------------------------------------------------------------------------
// draw()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::draw()
{
	LLFloater::draw();
	LLRect r = getRect();

	refresh();

	if (mMotionID.notNull() && mAnimPreview && !mInWorld)
	{
		gGL.color3f(1.f, 1.f, 1.f);

		gGL.getTexUnit(0)->bind(mAnimPreview);

		gGL.begin( LLRender::QUADS );
		{
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT);
			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2i(PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(1.f, 1.f);
			gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT);
		}
		gGL.end();

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
		if (!avatarp->areAnimationsPaused())
		{
			mAnimPreview->requestUpdate();
		}
	}
}

//-----------------------------------------------------------------------------
// resetMotion()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::resetMotion()
{
	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	BOOL paused = avatarp->areAnimationsPaused();

	LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
	if( motionp )
	{
		// Set emotion
		std::string emote = getChild<LLUICtrl>("emote_combo")->getValue().asString();
		motionp->setEmote(mIDList[emote]);
	}
	
	LLUUID base_id = mIDList[getChild<LLUICtrl>("preview_base_anim")->getValue().asString()];
	avatarp->deactivateAllMotions();
	avatarp->startMotion(mMotionID, 0.0f);
	avatarp->startMotion(base_id, BASE_ANIM_TIME_OFFSET);
	getChild<LLUICtrl>("playback_slider")->setValue(0.0f);

	// Set pose
	std::string handpose = getChild<LLUICtrl>("hand_pose_combo")->getValue().asString();
	avatarp->startMotion( ANIM_AGENT_HAND_MOTION, 0.0f );

	if( motionp )
	{
		motionp->setHandPose(LLHandMotion::getHandPose(handpose));
	}

	if (paused)
	{
		mPauseRequest = avatarp->requestPause();
	}
	else
	{
		mPauseRequest = NULL;	
	}

	if (!mPresetting)
	{
	  checkForPreset();
	}
}

//-----------------------------------------------------------------------------
// handleMouseDown()
//-----------------------------------------------------------------------------
BOOL LLFloaterBvhPreview::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (!mInWorld && mPreviewRect.pointInRect(x, y))
	{
		bringToFront( x, y );
		gFocusMgr.setMouseCapture(this);
		gViewerWindow->hideCursor();
		mLastMouseX = x;
		mLastMouseY = y;
		return TRUE;
	}

	return LLFloater::handleMouseDown(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleMouseUp()
//-----------------------------------------------------------------------------
BOOL LLFloaterBvhPreview::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (!mInWorld)
	{
		gFocusMgr.setMouseCapture(FALSE);
		gViewerWindow->showCursor();
	}
	return LLFloater::handleMouseUp(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleHover()
//-----------------------------------------------------------------------------
BOOL LLFloaterBvhPreview::handleHover(S32 x, S32 y, MASK mask)
{
	if (mInWorld)
		return TRUE;

	MASK local_mask = mask & ~MASK_ALT;

	if (mAnimPreview && hasMouseCapture())
	{
		if (local_mask == MASK_PAN)
		{
			// pan here
			mAnimPreview->pan((F32)(x - mLastMouseX) * -0.005f, (F32)(y - mLastMouseY) * -0.005f);
		}
		else if (local_mask == MASK_ORBIT)
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;
			
			mAnimPreview->rotate(yaw_radians, pitch_radians);
		}
		else 
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;
			
			mAnimPreview->rotate(yaw_radians, 0.f);
			mAnimPreview->zoom(zoom_amt);
		}

		mAnimPreview->requestUpdate();

		LLUI::setMousePositionLocal(this, mLastMouseX, mLastMouseY);
	}

	if (!mPreviewRect.pointInRect(x, y) || !mAnimPreview)
	{
		return LLFloater::handleHover(x, y, mask);
	}
	else if (local_mask == MASK_ORBIT)
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLCAMERA);
	}
	else if (local_mask == MASK_PAN)
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLPAN);
	}
	else
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLZOOMIN);
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// handleScrollWheel()
//-----------------------------------------------------------------------------
BOOL LLFloaterBvhPreview::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (mInWorld || !mAnimPreview)
		return false;

	mAnimPreview->zoom((F32)clicks * -0.2f);
	mAnimPreview->requestUpdate();

	return TRUE;
}

//-----------------------------------------------------------------------------
// onMouseCaptureLost()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onMouseCaptureLost()
{
	if (!mInWorld)
		gViewerWindow->showCursor();
}

//-----------------------------------------------------------------------------
// onBtnPlay()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnPlay()
{
	if (!getEnabled())
		return;

	if (mMotionID.notNull())
	{
		LLVOAvatar* avatarp;
		if (mInWorld && gAgentAvatarp)
			avatarp = gAgentAvatarp;
		else if (mAnimPreview)
			avatarp = mAnimPreview->getDummyAvatar();
		else
			return;

		if(!avatarp->isMotionActive(mMotionID))
		{
			resetMotion();
			mPauseRequest = NULL;
		}
		else if (avatarp->areAnimationsPaused())
		{
			mPauseRequest = NULL;
		}
		else //onBtnPause() does this in v3
		{
			mPauseRequest = avatarp->requestPause();
		}
	}
}

//-----------------------------------------------------------------------------
// onBtnStop()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnStop()
{
	if (!getEnabled())
		return;

	if (mMotionID.notNull())
	{
		LLVOAvatar* avatarp;
		if (mInWorld && gAgentAvatarp)
			avatarp = gAgentAvatarp;
		else if (mAnimPreview)
			avatarp = mAnimPreview->getDummyAvatar();
		else
			return;
#if 0 // Singu note: See https://jira.secondlife.com/browse/SH-2590
		resetMotion();
		mPauseRequest = avatarp->requestPause();
#else
		//<singu>
		// https://jira.secondlife.com/browse/SH-2590?focusedCommentId=286661&page=com.atlassian.jira.plugin.system.issuetabpanels:comment-tabpanel#comment-286661
		// Written by Coaldust Numbers.
		// Is the motion looping and have we passed the loop in point?
		if (getChild<LLUICtrl>("loop_check")->getValue().asBoolean() &&
			(F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal() <= (F32)getChild<LLUICtrl>("playback_slider")->getValue().asReal() * 100.f)
		{
			avatarp->stopMotion(mMotionID, FALSE);
		}
		else
		{
			avatarp->stopMotion(mMotionID, TRUE);
		}
		//</singu>
#endif
	}
}

//-----------------------------------------------------------------------------
// onSliderMove()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onSliderMove()
{
	if (!getEnabled())
		return;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	F32 slider_value = (F32)getChild<LLUICtrl>("playback_slider")->getValue().asReal();
	LLUUID base_id = mIDList[getChild<LLUICtrl>("preview_base_anim")->getValue().asString()];
	LLMotion* motionp = avatarp->findMotion(mMotionID);
	F32 duration = motionp->getDuration();// + motionp->getEaseOutDuration();
	F32 delta_time = duration * slider_value;
	avatarp->deactivateAllMotions();
	avatarp->startMotion(base_id, delta_time + BASE_ANIM_TIME_OFFSET);
	avatarp->startMotion(mMotionID, delta_time);
	mPauseRequest = avatarp->requestPause();
	refresh();
}

//-----------------------------------------------------------------------------
// onCommitBaseAnim()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitBaseAnim()
{
	if (!getEnabled())
		return;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	BOOL paused = avatarp->areAnimationsPaused();

	// stop all other possible base motions
	avatarp->stopMotion(mIDList["Standing"], TRUE);
	avatarp->stopMotion(mIDList["Walking"], TRUE);
	avatarp->stopMotion(mIDList["Sitting"], TRUE);
	avatarp->stopMotion(mIDList["Flying"], TRUE);

	resetMotion();

	if (!paused)
	{
		mPauseRequest = NULL;
	}
}

//-----------------------------------------------------------------------------
// onCommitLoop()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoop()
{
	if (!getEnabled())
		return;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

	if (motionp)
	{
		motionp->setLoop(getChild<LLUICtrl>("loop_check")->getValue().asBoolean());
		motionp->setLoopIn((F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal() * 0.01f * motionp->getDuration());
		motionp->setLoopOut((F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal() * 0.01f * motionp->getDuration());
	}

	if (!mPresetting)
	{
	  checkForPreset();
	}
}

//-----------------------------------------------------------------------------
// onCommitLoopIn()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoopIn()
{
	if (!getEnabled())
		return;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

	if (motionp)
	{
		motionp->setLoopIn((F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal() / 100.f);
		resetMotion();
		getChild<LLUICtrl>("loop_check")->setValue(LLSD(TRUE));
		onCommitLoop();
	}

	if (!mPresetting)
	{
	  checkForPreset();
	}
}

//-----------------------------------------------------------------------------
// onCommitLoopOut()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoopOut()
{
	if (!getEnabled())
		return;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

	if (motionp)
	{
		motionp->setLoopOut((F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal() * 0.01f * motionp->getDuration());
		resetMotion();
		getChild<LLUICtrl>("loop_check")->setValue(LLSD(TRUE));
		onCommitLoop();
	}

	if (!mPresetting)
	{
	  checkForPreset();
	}
}

//-----------------------------------------------------------------------------
// onCommitName()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitName()
{
	if (!getEnabled())
		return;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

	if (motionp)
	{
		motionp->setName(getChild<LLUICtrl>("name_form")->getValue().asString());
	}

	doCommitName();
}

//-----------------------------------------------------------------------------
// onCommitHandPose()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitHandPose()
{
	if (!getEnabled())
		return;

	resetMotion(); // sets hand pose
}

//-----------------------------------------------------------------------------
// onCommitEmote()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitEmote()
{
	if (!getEnabled())
		return;

	resetMotion(); // ssts emote
}

//-----------------------------------------------------------------------------
// onCommitPriority()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitPriority()
{
	if (!getEnabled())
		return;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

	motionp->setPriority(llfloor((F32)getChild<LLUICtrl>("priority")->getValue().asReal()));

	if (!mPresetting)
	{
	  checkForPreset();
	}
}

//-----------------------------------------------------------------------------
// onCommitEaseIn()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitEaseIn()
{
	if (!getEnabled())
		return;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

	motionp->setEaseIn((F32)getChild<LLUICtrl>("ease_in_time")->getValue().asReal());
	resetMotion();
}

//-----------------------------------------------------------------------------
// onCommitEaseOut()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitEaseOut()
{
	if (!getEnabled())
		return;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return;

	LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);

	motionp->setEaseOut((F32)getChild<LLUICtrl>("ease_out_time")->getValue().asReal());
	resetMotion();
}

//-----------------------------------------------------------------------------
// validateEaseIn()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateEaseIn(const LLSD& data)
{
	if (!getEnabled())
		return false;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return false;

	LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
	
	if (!motionp->getLoop())
	{
		F32 new_ease_in = llclamp((F32)getChild<LLUICtrl>("ease_in_time")->getValue().asReal(), 0.f, motionp->getDuration() - motionp->getEaseOutDuration());
		getChild<LLUICtrl>("ease_in_time")->setValue(LLSD(new_ease_in));
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// validateEaseOut()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateEaseOut(const LLSD& data)
{
	if (!getEnabled())
		return false;

	LLVOAvatar* avatarp;
	if (mInWorld && gAgentAvatarp)
		avatarp = gAgentAvatarp;
	else if (mAnimPreview)
		avatarp = mAnimPreview->getDummyAvatar();
	else
		return false;

	LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
	
	if (!motionp->getLoop())
	{
		F32 new_ease_out = llclamp((F32)getChild<LLUICtrl>("ease_out_time")->getValue().asReal(), 0.f, motionp->getDuration() - motionp->getEaseInDuration());
		getChild<LLUICtrl>("ease_out_time")->setValue(LLSD(new_ease_out));
	}

	return true;
}

//-----------------------------------------------------------------------------
// validateLoopIn()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateLoopIn(const LLSD& data)
{
	if (!getEnabled())
		return false;

	F32 loop_in_value = (F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal();
	F32 loop_out_value = (F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal();

	if (loop_in_value < 0.f)
	{
		loop_in_value = 0.f;
	}
	else if (loop_in_value > 100.f)
	{
		loop_in_value = 100.f;
	}
	else if (loop_in_value > loop_out_value)
	{
		loop_in_value = loop_out_value;
	}

	getChild<LLUICtrl>("loop_in_point")->setValue(LLSD(loop_in_value));
	return true;
}

//-----------------------------------------------------------------------------
// validateLoopOut()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateLoopOut(const LLSD& data)
{
	if (!getEnabled())
		return false;

	F32 loop_out_value = (F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal();
	F32 loop_in_value = (F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal();

	if (loop_out_value < 0.f)
	{
		loop_out_value = 0.f;
	}
	else if (loop_out_value > 100.f)
	{
		loop_out_value = 100.f;
	}
	else if (loop_out_value < loop_in_value)
	{
		loop_out_value = loop_in_value;
	}

	getChild<LLUICtrl>("loop_out_point")->setValue(LLSD(loop_out_value));
	return true;
}


//-----------------------------------------------------------------------------
// refresh()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::refresh()
{
	// Are we showing the play button (default) or the pause button?
	bool show_play = true;
	if (!mAnimPreview)
	{
		getChildView("bad_animation_text")->setVisible(TRUE);
		// play button visible but disabled
		mPlayButton->setEnabled(FALSE);
		mStopButton->setEnabled(FALSE);
		getChildView("ok_btn")->setEnabled(FALSE);
	}
	else
	{
		if (!mInWorld)
			getChildView("bad_animation_text")->setVisible(FALSE);
		// re-enabled in case previous animation was bad
		mPlayButton->setEnabled(TRUE);
		mStopButton->setEnabled(TRUE);
		LLVOAvatar* avatarp;
		if (mInWorld && gAgentAvatarp)
			avatarp = gAgentAvatarp;
		else if (mAnimPreview)
			avatarp = mAnimPreview->getDummyAvatar();
		else
			return;
		if (avatarp->isMotionActive(mMotionID))
		{
			mStopButton->setEnabled(TRUE);
			LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
			if (!avatarp->areAnimationsPaused())
			{
				// animation is playing
				if (motionp)
				{
					F32 fraction_complete = motionp->getLastUpdateTime() / motionp->getDuration();
					getChild<LLUICtrl>("playback_slider")->setValue(fraction_complete);
				}
				show_play = false;
			}
		}
		else
		{
			// Motion just finished playing
			mPauseRequest = avatarp->requestPause();
		}
		getChildView("ok_btn")->setEnabled(TRUE);
		if (!mInWorld)
			mAnimPreview->requestUpdate();
	}
	if (show_play)
		mPlayButton->setImages(std::string("button_anim_play.tga"), std::string("button_anim_play_selected.tga"));
	else
		mPlayButton->setImages(std::string("button_anim_pause.tga"), std::string("button_anim_pause_selected.tga"));

}

//-----------------------------------------------------------------------------
// onBtnOK()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnOK(void* userdata)
{
	LLFloaterBvhPreview* floaterp = (LLFloaterBvhPreview*)userdata;
	if (!floaterp->getEnabled()) return;

	if ((!floaterp->mInWorld && floaterp->mAnimPreview) || (floaterp->mInWorld && gAgentAvatarp))
	{
		LLKeyframeMotion* motionp;
		if (floaterp->mInWorld)
		{
			motionp = (LLKeyframeMotion*)gAgentAvatarp->findMotion(floaterp->mMotionID);
		}
		else
		{
			motionp = (LLKeyframeMotion*)floaterp->mAnimPreview->getDummyAvatar()->findMotion(floaterp->mMotionID);
		}

		S32 file_size = motionp->getFileSize();
		U8* buffer = new U8[file_size];

		LLDataPackerBinaryBuffer dp(buffer, file_size);
		if (motionp->serialize(dp))
		{
			LLVFile file(gVFS, motionp->getID(), LLAssetType::AT_ANIMATION, LLVFile::APPEND);

			S32 size = dp.getCurrentSize();

			//<edit>
			LLMD5 asset_md5;
			asset_md5.update(buffer, size);
			asset_md5.finalize();
			floaterp->mFrontEnd->setAssetHash(asset_md5);
			//</edit>

			file.setMaxSize(size);
			if (file.write((U8*)buffer, size))
			{
				std::string name = floaterp->getChild<LLUICtrl>("name_form")->getValue().asString();
				std::string desc = floaterp->getChild<LLUICtrl>("description_form")->getValue().asString();
				S32 expected_upload_cost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();

#if 0 // mItem was always NULL
				// <edit>
				if(floaterp->mItem)
				{
				        // Update existing item instead of creating a new one
				        LLViewerInventoryItem* item = (LLViewerInventoryItem*)floaterp->mItem;
				        LLSaveInfo* info = new LLSaveInfo(item->getUUID(), LLUUID::null, desc, floaterp->mTransactionID);
				        gAssetStorage->storeAssetData(floaterp->mTransactionID, LLAssetType::AT_ANIMATION, NULL, info, FALSE);

				        // I guess I will do this now because the floater is closing...
				        LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
				        new_item->setDescription(desc);
				        new_item->setTransactionID(floaterp->mTransactionID);
				        new_item->setAssetUUID(motionp->getID());
				        new_item->updateServer(FALSE);
				        gInventory.updateItem(new_item);
				        gInventory.notifyObservers();
				}
				else
				// </edit>
#endif
				{
					floaterp->mFrontEnd->addDelta(new AIMultiGrid::BVHAnimDelta(motionp->getDelta()));
					floaterp->mFrontEnd->upload_new_resource(floaterp->mTransactionID, // tid
						    LLAssetType::AT_ANIMATION,
						    name,
						    desc,
						    0,
						    LLFolderType::FT_NONE,
						    LLInventoryType::IT_ANIMATION,
						    LLFloaterPerms::getNextOwnerPerms(), LLFloaterPerms::getGroupPerms(), LLFloaterPerms::getEveryonePerms(),
						    name,
						    expected_upload_cost,
							NULL, NULL,
							AIMultiGrid::UploadResponderFactory<LLNewAgentInventoryResponder>());
				}
			}
			else
			{
				llwarns << "Failure writing animation data." << llendl;
				LLNotificationsUtil::add("WriteAnimationFail");
			}
		}

		delete [] buffer;
		// clear out cache for motion data
		if (floaterp->mInWorld)
		{
			gAgentAvatarp->removeMotion(floaterp->mMotionID);
			gAgentAvatarp->deactivateAllMotions();
		}
		else
		{
			floaterp->mAnimPreview->getDummyAvatar()->removeMotion(floaterp->mMotionID);
		}
		LLKeyframeDataCache::removeKeyframeData(floaterp->mMotionID);
	}

	floaterp->close(false);
}

//-----------------------------------------------------------------------------
// LLPreviewAnimation
//-----------------------------------------------------------------------------
LLPreviewAnimation::LLPreviewAnimation(S32 width, S32 height) : LLViewerDynamicTexture(width, height, 3, ORDER_MIDDLE, FALSE)
{
	mNeedsUpdate = TRUE;
	mCameraDistance = PREVIEW_CAMERA_DISTANCE;
	mCameraYaw = 0.f;
	mCameraPitch = 0.f;
	mCameraZoom = 1.f;

	mDummyAvatar = (LLVOAvatar*)gObjectList.createObjectViewer(LL_PCODE_LEGACY_AVATAR, gAgent.getRegion());
	mDummyAvatar->createDrawable(&gPipeline);
	mDummyAvatar->mIsDummy = TRUE;
	mDummyAvatar->mSpecialRenderMode = 1;
	mDummyAvatar->setPositionAgent(LLVector3::zero);
	mDummyAvatar->slamPosition();
	mDummyAvatar->updateJointLODs();
	mDummyAvatar->updateGeometry(mDummyAvatar->mDrawable);
	mDummyAvatar->startMotion(ANIM_AGENT_STAND, BASE_ANIM_TIME_OFFSET);
	mDummyAvatar->hideSkirt();
	//gPipeline.markVisible(mDummyAvatar->mDrawable, *LLViewerCamera::getInstance());

	// stop extraneous animations
	mDummyAvatar->stopMotion( ANIM_AGENT_HEAD_ROT, TRUE );
	mDummyAvatar->stopMotion( ANIM_AGENT_EYE, TRUE );
	mDummyAvatar->stopMotion( ANIM_AGENT_BODY_NOISE, TRUE );
	mDummyAvatar->stopMotion( ANIM_AGENT_BREATHE_ROT, TRUE );
}

//-----------------------------------------------------------------------------
// LLPreviewAnimation()
//-----------------------------------------------------------------------------
LLPreviewAnimation::~LLPreviewAnimation()
{
	mDummyAvatar->markDead();
}

//virtual
S8 LLPreviewAnimation::getType() const
{
	return LLViewerDynamicTexture::LL_PREVIEW_ANIMATION ;
}

//-----------------------------------------------------------------------------
// update()
//-----------------------------------------------------------------------------
BOOL	LLPreviewAnimation::render()
{
	mNeedsUpdate = FALSE;
	LLVOAvatar* avatarp = mDummyAvatar;
	
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.ortho(0.0f, mFullWidth, 0.0f, mFullHeight, -1.0f, 1.0f);

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	if (LLGLSLShader::sNoFixedFunction)
	{
		gUIProgram.bind();
	}

	LLGLSUIDefault def;
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.color4f(0.15f, 0.2f, 0.3f, 1.f);

	gl_rect_2d_simple( mFullWidth, mFullHeight );

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	gGL.flush();

	LLVector3 target_pos = avatarp->mRoot->getWorldPosition();

	LLQuaternion camera_rot = LLQuaternion(mCameraPitch, LLVector3::y_axis) * 
		LLQuaternion(mCameraYaw, LLVector3::z_axis);

	LLQuaternion av_rot = avatarp->mRoot->getWorldRotation() * camera_rot;
	LLViewerCamera::getInstance()->setOriginAndLookAt(
		target_pos + ((LLVector3(mCameraDistance, 0.f, 0.f) + mCameraOffset) * av_rot),		// camera
		LLVector3::z_axis,																	// up
		target_pos + (mCameraOffset  * av_rot) );											// point of interest

	LLViewerCamera::getInstance()->setView(LLViewerCamera::getInstance()->getDefaultFOV() / mCameraZoom);
	LLViewerCamera::getInstance()->setPerspective(FALSE, mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight, FALSE);

	mCameraRelPos = LLViewerCamera::getInstance()->getOrigin() - avatarp->mHeadp->getWorldPosition();

	//avatarp->setAnimationData("LookAtPoint", (void *)&mCameraRelPos);

	//SJB: Animation is updated in LLVOAvatar::updateCharacter
	
	if (avatarp->mDrawable.notNull())
	{
		avatarp->updateLOD();
		
		LLVertexBuffer::unbind();
		LLGLDepthTest gls_depth(GL_TRUE);

		LLFace* face = avatarp->mDrawable->getFace(0);
		if (face)
		{
			LLDrawPoolAvatar *avatarPoolp = (LLDrawPoolAvatar *)face->getPool();
			avatarp->dirtyMesh();
			avatarPoolp->renderAvatars(avatarp);  // renders only one avatar
		}
	}

	gGL.color4f(1,1,1,1);
	return TRUE;
}

//-----------------------------------------------------------------------------
// requestUpdate()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::requestUpdate()
{ 
	mNeedsUpdate = TRUE; 
}

//-----------------------------------------------------------------------------
// rotate()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::rotate(F32 yaw_radians, F32 pitch_radians)
{
	mCameraYaw = mCameraYaw + yaw_radians;

	mCameraPitch = llclamp(mCameraPitch + pitch_radians, F_PI_BY_TWO * -0.8f, F_PI_BY_TWO * 0.8f);
}

//-----------------------------------------------------------------------------
// zoom()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::zoom(F32 zoom_delta)
{
	setZoom(mCameraZoom + zoom_delta);
}

//-----------------------------------------------------------------------------
// setZoom()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::setZoom(F32 zoom_amt)
{
	mCameraZoom	= llclamp(zoom_amt, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);
}

//-----------------------------------------------------------------------------
// pan()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::pan(F32 right, F32 up)
{
	mCameraOffset.mV[VY] = llclamp(mCameraOffset.mV[VY] + right * mCameraDistance / mCameraZoom, -1.f, 1.f);
	mCameraOffset.mV[VZ] = llclamp(mCameraOffset.mV[VZ] + up * mCameraDistance / mCameraZoom, -1.f, 1.f);
}



