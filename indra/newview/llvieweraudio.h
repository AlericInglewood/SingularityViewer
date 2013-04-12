/** 
 * @file llvieweraudio.h
 * @brief Audio functions that used to be in viewer.cpp
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
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

#ifndef LL_VIEWERAUDIO_H
#define LL_VIEWERAUDIO_H

#include "llframetimer.h"
#include "llsingleton.h"

// comment out to turn off wind
#define kAUDIO_ENABLE_WIND 
//#define kAUDIO_ENABLE_WATER 1	// comment out to turn off water
#define kAUDIO_NUM_BUFFERS 30
#define kAUDIO_NUM_SOURCES 30 

void init_audio();
void audio_update_volume(bool force_update = true);
void audio_update_listener();
void audio_update_wind(bool force_update = true);

class LLViewerAudio : public LLSingleton<LLViewerAudio>
{
public:

	enum EFadeState
	{
		FADE_IDLE,
		FADE_IN,
		FADE_OUT,
	};

	LLViewerAudio();
	virtual ~LLViewerAudio();
	
	void startInternetStreamWithAutoFade(std::string streamURI);
	void stopInternetStreamWithAutoFade();
	
	bool onIdleUpdate();

	EFadeState getFadeState() { return mFadeState; }
	bool isDone() { return mDone; };
	F32 getFadeVolume();
	bool getForcedTeleportFade() { return mForcedTeleportFade; };
	void setForcedTeleportFade(bool fade) { mForcedTeleportFade = fade;} ;
	void setNextStreamURI(std::string stream) { mNextStreamURI = stream; } ;
	void setWasPlaying(bool playing) { mWasPlaying = playing;} ;

private:

	bool mDone;
	F32 mFadeTime;
	std::string mNextStreamURI;
	EFadeState mFadeState;
	LLFrameTimer stream_fade_timer;
	bool mIdleListnerActive;
	bool mForcedTeleportFade;
	bool mWasPlaying;
	boost::signals2::connection	mTeleportFailedConnection;
	boost::signals2::connection	mTeleportFinishedConnection;
	boost::signals2::connection mTeleportStartedConnection;

	void registerIdleListener();
	void deregisterIdleListener() { mIdleListnerActive = false; };
	void startFading();
	void onTeleportFailed();
	void onTeleportFinished(const LLVector3d& pos, const bool& local);
	void onTeleportStarted();
};

#endif //LL_VIEWER_H
