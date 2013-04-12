/** 
 * @file llvieweraudio.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llaudioengine.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "llvieweraudio.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h" //For mInAir
#include "llvoiceclient.h"
#include "llviewermedia.h"
#include "llcallbacklist.h"
#include "llstartup.h"
#include "llviewerparcelmgr.h"
#include "llparcel.h"
#include "llviewermessage.h"

/////////////////////////////////////////////////////////

LLViewerAudio::LLViewerAudio() :
	mDone(true),
	mFadeState(FADE_IDLE),
	mFadeTime(),
    mIdleListnerActive(false),
	mForcedTeleportFade(false),
	mWasPlaying(false)
{
	mTeleportFailedConnection = LLViewerParcelMgr::getInstance()->
		setTeleportFailedCallback(boost::bind(&LLViewerAudio::onTeleportFailed, this));
	mTeleportFinishedConnection = LLViewerParcelMgr::getInstance()->
		setTeleportFinishedCallback(boost::bind(&LLViewerAudio::onTeleportFinished, this, _1, _2));
	mTeleportStartedConnection = LLViewerMessage::getInstance()->
		setTeleportStartedCallback(boost::bind(&LLViewerAudio::onTeleportStarted, this));
}

LLViewerAudio::~LLViewerAudio()
{
	mTeleportFailedConnection.disconnect();
	mTeleportFinishedConnection.disconnect();
	mTeleportStartedConnection.disconnect();
}

void LLViewerAudio::registerIdleListener()
{
	if(mIdleListnerActive==false)
	{
		mIdleListnerActive = true;
		doOnIdleRepeating(boost::bind(&LLViewerAudio::onIdleUpdate, this));
	}
}

void LLViewerAudio::startInternetStreamWithAutoFade(std::string streamURI)
{
	// Old and new stream are identical
	if (mNextStreamURI == streamURI)
	{
		return;
	}

	// Record the URI we are going to be switching to	
	mNextStreamURI = streamURI;

	switch (mFadeState)
	{
		case FADE_IDLE:
		// If a stream is playing fade it out first
		if (!gAudiop->getInternetStreamURL().empty())
		{
			// The order of these tests is important, state FADE_OUT will be processed below
			mFadeState = FADE_OUT;
		}
		// Otherwise the new stream can be faded in
		else
		{
			mFadeState = FADE_IN;
			gAudiop->startInternetStream(mNextStreamURI);
			startFading();
			registerIdleListener();
			break;
		}

		case FADE_OUT:
			startFading();
			registerIdleListener();
			break;

		case FADE_IN:
			registerIdleListener();
			break;

		default:
			llwarns << "Unknown fading state: " << mFadeState << llendl;
			break;
	}
}

// A return of false from onIdleUpdate means it will be called again next idle update.
// A return of true means we have finished with it and the callback will be deleted.
bool LLViewerAudio::onIdleUpdate()
{
	bool fadeIsFinished = false;

	// There is a delay in the login sequence between when the parcel information has
	// arrived and the music stream is started and when the audio system is called to set
	// initial volume levels.  This code extends the fade time so you hear a full fade in.
	if ((LLStartUp::getStartupState() < STATE_STARTED))
	{
		stream_fade_timer.reset();
		stream_fade_timer.setTimerExpirySec(mFadeTime);
	}

	if (mDone)
	{
		//  This should be a rare or never occurring state.
		if (mFadeState == FADE_IDLE)
		{
			deregisterIdleListener();
			fadeIsFinished = true; // Stop calling onIdleUpdate
		}

		// we have finished the current fade operation
		if (mFadeState == FADE_OUT)
		{
			// Clear URI
			gAudiop->startInternetStream(LLStringUtil::null);
			gAudiop->stopInternetStream();
				
			if (!mNextStreamURI.empty())
			{
				mFadeState = FADE_IN;
				gAudiop->startInternetStream(mNextStreamURI);
				startFading();
			}
			else
			{
				mFadeState = FADE_IDLE;
				deregisterIdleListener();
				fadeIsFinished = true; // Stop calling onIdleUpdate
			}
		}
		else if (mFadeState == FADE_IN)
		{
			if (mNextStreamURI != gAudiop->getInternetStreamURL())
			{
				mFadeState = FADE_OUT;
				startFading();
			}
			else
			{
				mFadeState = FADE_IDLE;
				deregisterIdleListener();
				fadeIsFinished = true; // Stop calling onIdleUpdate
			}
		}
	}

	return fadeIsFinished;
}

void LLViewerAudio::stopInternetStreamWithAutoFade()
{
	mFadeState = FADE_IDLE;
	mNextStreamURI = LLStringUtil::null;
	mDone = true;

	gAudiop->startInternetStream(LLStringUtil::null);
	gAudiop->stopInternetStream();
}

void LLViewerAudio::startFading()
{
	const F32 AUDIO_MUSIC_FADE_IN_TIME = 3.0f;
	const F32 AUDIO_MUSIC_FADE_OUT_TIME = 2.0f;
	// This minimum fade time prevents divide by zero and negative times
	const F32 AUDIO_MUSIC_MINIMUM_FADE_TIME = 0.01f;

	if(mDone)
	{
		// The fade state here should only be one of FADE_IN or FADE_OUT, but, in case it is not,
		// rather than check for both states assume a fade in and check for the fade out case.
		mFadeTime = AUDIO_MUSIC_FADE_IN_TIME;
		if (LLViewerAudio::getInstance()->getFadeState() == LLViewerAudio::FADE_OUT)
		{
			mFadeTime = AUDIO_MUSIC_FADE_OUT_TIME;
		}

		// Prevent invalid fade time
		mFadeTime = llmax(mFadeTime, AUDIO_MUSIC_MINIMUM_FADE_TIME);

		stream_fade_timer.reset();
		stream_fade_timer.setTimerExpirySec(mFadeTime);
		mDone = false;
	}
}

F32 LLViewerAudio::getFadeVolume()
{
	F32 fade_volume = 1.0f;

	if (stream_fade_timer.hasExpired())
	{
		mDone = true;
		// If we have been fading out set volume to 0 until the next fade state occurs to prevent
		// an audio transient.
		if (LLViewerAudio::getInstance()->getFadeState() == LLViewerAudio::FADE_OUT)
		{
			fade_volume = 0.0f;
		}
	}

	if (!mDone)
	{
		// Calculate how far we are into the fade time
		fade_volume = stream_fade_timer.getElapsedTimeF32() / mFadeTime;
		
		if (LLViewerAudio::getInstance()->getFadeState() == LLViewerAudio::FADE_OUT)
		{
			// If we are not fading in then we are fading out, so invert the fade
			// direction; start loud and move towards zero volume.
			fade_volume = 1.0f - fade_volume;
		}
	}

	return fade_volume;
}

void LLViewerAudio::onTeleportStarted()
{
	if (!LLViewerAudio::getInstance()->getForcedTeleportFade())
	{
		// Even though the music was turned off it was starting up (with autoplay disabled) occasionally
		// after a failed teleport or after an intra-parcel teleport.  Also, the music sometimes was not
		// restarting after a successful intra-parcel teleport. Setting mWasPlaying fixes these issues.
		LLViewerAudio::getInstance()->setWasPlaying(!gAudiop->getInternetStreamURL().empty());
		LLViewerAudio::getInstance()->setForcedTeleportFade(true);
		LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(LLStringUtil::null);
		LLViewerAudio::getInstance()->setNextStreamURI(LLStringUtil::null);
	}
}

void LLViewerAudio::onTeleportFailed()
{
	// Calling audio_update_volume makes sure that the music stream is properly set to be restored to
	// its previous value
	audio_update_volume(false);

	if (gAudiop && mWasPlaying)
	{
		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if (parcel)
		{
			mNextStreamURI = parcel->getMusicURL();
			llinfos << "Teleport failed -- setting music stream to " << mNextStreamURI << llendl;
		}
	}
	mWasPlaying = false;
}

void LLViewerAudio::onTeleportFinished(const LLVector3d& pos, const bool& local)
{
	// Calling audio_update_volume makes sure that the music stream is properly set to be restored to
	// its previous value
	audio_update_volume(false);

	if (gAudiop && local && mWasPlaying)
	{
		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if (parcel)
		{
			mNextStreamURI = parcel->getMusicURL();
			llinfos << "Intraparcel teleport -- setting music stream to " << mNextStreamURI << llendl;
		}
	}
	mWasPlaying = false;
}

void init_audio() 
{
	if (!gAudiop) 
	{
		llwarns << "Failed to create an appropriate Audio Engine" << llendl;
		return;
	}
	LLVector3d lpos_global = gAgentCamera.getCameraPositionGlobal();
	LLVector3 lpos_global_f;

	lpos_global_f.setVec(lpos_global);
					
	gAudiop->setListener(lpos_global_f,
						  LLVector3::zero,	// LLViewerCamera::getInstance()->getVelocity(),    // !!! BUG need to replace this with smoothed velocity!
						  LLViewerCamera::getInstance()->getUpAxis(),
						  LLViewerCamera::getInstance()->getAtAxis());

// load up our initial set of sounds we'll want so they're in memory and ready to be played

	BOOL mute_audio = gSavedSettings.getBOOL("MuteAudio");

	if (!mute_audio && FALSE == gSavedSettings.getBOOL("NoPreload"))
	{
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndAlert")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndBadKeystroke")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndChatFromObject")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndClick")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndClickRelease")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndHealthReductionF")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndHealthReductionM")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndIncomingChat")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndIncomingIM")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndInvApplyToObject")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndInvalidOp")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndInventoryCopyToInv")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndMoneyChangeDown")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndMoneyChangeUp")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndObjectCopyToInv")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndObjectCreate")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndObjectDelete")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndObjectRezIn")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndObjectRezOut")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndPieMenuAppear")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndPieMenuHide")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndPieMenuSliceHighlight0")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndPieMenuSliceHighlight1")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndPieMenuSliceHighlight2")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndPieMenuSliceHighlight3")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndPieMenuSliceHighlight4")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndPieMenuSliceHighlight5")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndPieMenuSliceHighlight6")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndPieMenuSliceHighlight7")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndSnapshot")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndStartAutopilot")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndStartFollowpilot")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndStartIM")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndStopAutopilot")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndTeleportOut")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndTextureApplyToObject")));
		//gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndTextureCopyToInv")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndTyping")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndWindowClose")));
		gAudiop->preloadSound(LLUUID(gSavedSettings.getString("UISndWindowOpen")));
	}

	audio_update_volume(true);
}

void audio_update_volume(bool force_update)
{
	static const LLCachedControl<F32> master_volume("AudioLevelMaster",1.0);
	static const LLCachedControl<F32> audio_level_sfx("AudioLevelSFX",1.0);
	static const LLCachedControl<F32> audio_level_ui("AudioLevelUI",1.0);
	static const LLCachedControl<F32> audio_level_ambient("AudioLevelAmbient",1.0);
	static const LLCachedControl<F32> audio_level_music("AudioLevelMusic",1.0);
	static const LLCachedControl<F32> audio_level_media("AudioLevelMedia",1.0);
	static const LLCachedControl<F32> audio_level_voice("AudioLevelVoice",1.0);
	static const LLCachedControl<F32> audio_level_mic("AudioLevelMic",1.0);
	static const LLCachedControl<bool> _mute_audio("MuteAudio",false);
	static const LLCachedControl<bool> mute_sounds("MuteSounds",false);
	static const LLCachedControl<bool> mute_ui("MuteUI",false);
	static const LLCachedControl<bool> mute_ambient("MuteAmbient",false);
	static const LLCachedControl<bool> mute_music("MuteMusic",false);
	static const LLCachedControl<bool> mute_media("MuteMedia",false);
	static const LLCachedControl<bool> mute_voice("MuteVoice",false);
	static const LLCachedControl<bool> mute_when_minimized("MuteWhenMinimized",true);
	static const LLCachedControl<F32> audio_level_doppler("AudioLevelDoppler",1.0);
	static const LLCachedControl<F32> audio_level_rolloff("AudioLevelRolloff",1.0);
	static const LLCachedControl<F32> audio_level_underwater_rolloff("AudioLevelUnderwaterRolloff",3.0);
	BOOL mute_audio = _mute_audio;
	if (!gViewerWindow->getActive() && mute_when_minimized)
	{
		mute_audio = TRUE;
	}
	F32 mute_volume = mute_audio ? 0.0f : 1.0f;

	// Sound Effects
	if (gAudiop) 
	{
		gAudiop->setMasterGain ( master_volume );

		gAudiop->setDopplerFactor(audio_level_doppler);
		if(!LLViewerCamera::getInstance()->cameraUnderWater())
			gAudiop->setRolloffFactor( audio_level_rolloff );
		else
			gAudiop->setRolloffFactor( audio_level_underwater_rolloff );

		gAudiop->setMuted(mute_audio);
		
		if (force_update)
		{
			audio_update_wind(true);
		}

		// handle secondary gains
		gAudiop->setSecondaryGain(LLAudioEngine::AUDIO_TYPE_SFX,
								  mute_sounds ? 0.f : audio_level_sfx);
		gAudiop->setSecondaryGain(LLAudioEngine::AUDIO_TYPE_UI,
								  mute_ui ? 0.f : audio_level_ui);
		gAudiop->setSecondaryGain(LLAudioEngine::AUDIO_TYPE_AMBIENT,
								  mute_ambient ? 0.f : audio_level_ambient);
	}

	// Streaming Music
	if (gAudiop) 
	{		
		F32 music_volume = mute_volume * master_volume * (audio_level_music*audio_level_music);
		gAudiop->setInternetStreamGain ( mute_music ? 0.f : music_volume );
	
	}

	// Streaming Media
	F32 media_volume = mute_volume * master_volume * (audio_level_media*audio_level_media);
	LLViewerMedia::setVolume( mute_media ? 0.0f : media_volume );

	// Voice
	if (gVoiceClient)
	{
		F32 voice_volume = mute_volume * master_volume * audio_level_voice;
		gVoiceClient->setVoiceVolume(mute_voice ? 0.f : voice_volume);
		gVoiceClient->setMicGain(mute_voice ? 0.f : audio_level_mic);

		if (!gViewerWindow->getActive() && mute_when_minimized)
		{
			gVoiceClient->setMuteMic(true);
		}
		else
		{
			gVoiceClient->setMuteMic(false);
		}
	}
}

void audio_update_listener()
{
	if (gAudiop)
	{
		// update listener position because agent has moved	
		LLVector3d lpos_global = gAgentCamera.getCameraPositionGlobal();		
		LLVector3 lpos_global_f;
		lpos_global_f.setVec(lpos_global);
	
		gAudiop->setListener(lpos_global_f,
							 // LLViewerCamera::getInstance()VelocitySmoothed, 
							 // LLVector3::zero,	
							 gAgent.getVelocity(),    // !!! *TODO: need to replace this with smoothed velocity!
							 LLViewerCamera::getInstance()->getUpAxis(),
							 LLViewerCamera::getInstance()->getAtAxis());
	}
}

void audio_update_wind(bool force_update)
{
#ifdef kAUDIO_ENABLE_WIND
	if(gAgent.getRegion())
	{
        // Scale down the contribution of weather-simulation wind to the
        // ambient wind noise.  Wind velocity averages 3.5 m/s, with gusts to 7 m/s
        // whereas steady-state avatar walk velocity is only 3.2 m/s.
        // Without this the world feels desolate on first login when you are
        // standing still.
        static LLCachedControl<F32> wind_level("AudioLevelWind", 0.5f);
        LLVector3 scaled_wind_vec = gWindVec * wind_level;

		// Mix in the avatar's motion, subtract because when you walk north,
		// the apparent wind moves south.
		LLVector3 final_wind_vec = scaled_wind_vec - gAgent.getVelocity();

		// rotate the wind vector to be listener (agent) relative
		gRelativeWindVec = gAgent.getFrameAgent().rotateToLocal(final_wind_vec);

		// don't use the setter setMaxWindGain() because we don't
		// want to screw up the fade-in on startup by setting actual source gain
		// outside the fade-in.
		F32 master_volume  = gSavedSettings.getBOOL("MuteAudio") ? 0.f : gSavedSettings.getF32("AudioLevelMaster");
		F32 ambient_volume = gSavedSettings.getBOOL("MuteAmbient") ? 0.f : gSavedSettings.getF32("AudioLevelAmbient");
		F32 max_wind_volume = master_volume * ambient_volume;

		const F32 WIND_SOUND_TRANSITION_TIME = 2.f;
		// amount to change volume this frame
		F32 volume_delta = (LLFrameTimer::getFrameDeltaTimeF32() / WIND_SOUND_TRANSITION_TIME) * max_wind_volume;
		if (force_update) 
		{
			// initialize wind volume (force_update) by using large volume_delta
			// which is sufficient to completely turn off or turn on wind noise
			volume_delta = 1.f;
		}

		static LLCachedControl<bool> MuteWind("MuteWind");
		static LLCachedControl<bool> ContinueFlying("LiruContinueFlyingOnUnsit");
		// mute wind entirely when the user asked or when the user is seated, but flying
		if (MuteWind || (ContinueFlying && gAgentAvatarp&& gAgentAvatarp->isSitting()))
		{
			// volume decreases by itself
			gAudiop->mMaxWindGain = 0.f;
		}
		// mute wind when not /*flying*/ in air
		else if /*(gAgent.getFlying())*/ (gAgentAvatarp && gAgentAvatarp->mInAir)
		{
			// volume increases by volume_delta, up to no more than max_wind_volume
			gAudiop->mMaxWindGain = llmin(gAudiop->mMaxWindGain + volume_delta, max_wind_volume);
		}
		else
		{
			// volume decreases by volume_delta, down to no less than 0
			gAudiop->mMaxWindGain = llmax(gAudiop->mMaxWindGain - volume_delta, 0.f);
		}
		
		gAudiop->updateWind(gRelativeWindVec, gAgentCamera.getCameraPositionAgent()[VZ] - gAgent.getRegion()->getWaterHeight());
	}
#endif
}
