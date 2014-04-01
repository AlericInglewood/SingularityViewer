/** 
 * @file llfloaterbvhpreview.h
 * @brief LLFloaterBvhPreview class definition
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

#ifndef LL_LLFLOATERBVHPREVIEW_H
#define LL_LLFLOATERBVHPREVIEW_H

#include "llfloaternamedesc.h"
#include "lldynamictexture.h"
#include "llcharacter.h"
#include "llquaternion.h"
#include "llbvhloader.h"

class LLVOAvatar;
class LLViewerJointMesh;
class LLKeyframeMotion;

class LLPreviewAnimation : public LLViewerDynamicTexture
{
public:
	virtual ~LLPreviewAnimation();

public:
	LLPreviewAnimation(S32 width, S32 height);	

	/*virtual*/ S8 getType() const ;

	BOOL	render();
	void	requestUpdate();
	void	rotate(F32 yaw_radians, F32 pitch_radians);
	void	zoom(F32 zoom_delta);
	void	setZoom(F32 zoom_amt);
	void	pan(F32 right, F32 up);
	virtual BOOL needsUpdate() { return mNeedsUpdate; }

	LLVOAvatar* getDummyAvatar() { return mDummyAvatar; }

protected:
	BOOL				mNeedsUpdate;
	F32					mCameraDistance;
	F32					mCameraYaw;
	F32					mCameraPitch;
	F32					mCameraZoom;
	LLVector3			mCameraOffset;
	LLVector3			mCameraRelPos;
	LLPointer<LLVOAvatar>			mDummyAvatar;
};

class AIBVHLoader
{
  private:
	LLTransactionID mTransactionID;
	LLAssetID mMotionID;
	LLPointer<LLPreviewAnimation> mAnimPreview;
	LLBVHLoader* mLoader;
	LLKeyframeMotion* mMotionp;

  public:
	AIBVHLoader(void) : mLoader(NULL), mMotionp(NULL) { }
	~AIBVHLoader() { delete mLoader; }

	bool loadbvh(std::string const& filename, bool in_world, LLPointer<AIMultiGrid::FrontEnd> front_end);

	LLTransactionID const& getTransactionID(void) const { return mTransactionID; }
	LLAssetID const& getMotionID(void) const { return mMotionID; }
	LLPointer<LLPreviewAnimation> const& getAnimPreview(void) const { return mAnimPreview; }
	LLKeyframeMotion* getMotionp(void) const { return mMotionp; }

	ELoadStatus getStatus(void) const { return mLoader ? mLoader->getStatus() : E_ST_OK; }
	S32 getLineNumber(void) const { return mLoader ? mLoader->getLineNumber() : 0; }
	F32 getDuration(void ) const { return mLoader ? mLoader->getDuration() : 0.f; }
};

class LLFloaterBvhPreview : public LLFloaterNameDesc
{
public:
	explicit LLFloaterBvhPreview(LLPointer<AIMultiGrid::FrontEnd> const& front_end);
	virtual ~LLFloaterBvhPreview();
	
	BOOL postBuild();

	BOOL handleMouseDown(S32 x, S32 y, MASK mask);
	BOOL handleMouseUp(S32 x, S32 y, MASK mask);
	BOOL handleHover(S32 x, S32 y, MASK mask);
	BOOL handleScrollWheel(S32 x, S32 y, S32 clicks); 
	void onMouseCaptureLost();

	void refresh();

	void	onBtnPlay();
	void	onBtnStop();
	void onSliderMove();
	void onCommitBaseAnim();
	void onCommitLoop();
	void onCommitLoopIn();
	void onCommitLoopOut();
	bool validateLoopIn(const LLSD& data);
	bool validateLoopOut(const LLSD& data);
	void onCommitName();
	void onCommitHandPose();
	void onCommitEmote();
	void onCommitPriority();
	void onCommitEaseIn();
	void onCommitEaseOut();
	bool validateEaseIn(const LLSD& data);
	bool validateEaseOut(const LLSD& data);
	static void	onBtnOK(void*);
	static void onSaveComplete(const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data,
									   S32 status, LLExtStat ext_status);

	//<singu>
	// Overridden from LLFloaterNameDesc.
	/*virtual*/ void onChangePresetDelta(AIMultiGrid::Delta* delta);	// Accept a preset and update all settings.
	void checkForPreset(void);											// Accept a setting change and check if the new settings match an existing preset.
	//</singu>

private:
	void setAnimCallbacks() ;
	
protected:
	void			draw();
	void			resetMotion();

	LLPointer< LLPreviewAnimation> mAnimPreview;
	S32					mLastMouseX;
	S32					mLastMouseY;
	LLButton*			mPlayButton;
	LLButton*			mStopButton;
	LLRect				mPreviewRect;
	LLRectf				mPreviewImageRect;
	LLAssetID			mMotionID;
	LLTransactionID		mTransactionID;
	BOOL				mInWorld;
	LLAnimPauseRequest	mPauseRequest;
	//<singu>
	F32					mDuration;				// Singu extension. Cache for the duration of the BVH animation.
	bool				mPresetting;			// Singu extension. Set to indicate that we're committing a preset (as opposed to the user actually changing a setting).
	//</singu>

	std::map<std::string, LLUUID>	mIDList;
};

#endif  // LL_LLFLOATERBVHPREVIEW_H
