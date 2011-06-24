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
#include "llvoiceclient.h"
#include "llviewermedia.h"

/////////////////////////////////////////////////////////

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
		gAudiop->setRolloffFactor(audio_level_rolloff);
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
	//
	//  Extract height above water to modulate filter by whether above/below water 
	// 
	LLViewerRegion* region = gAgent.getRegion();
	if (region)
	{
		static F32 last_camera_water_height = -1000.f;
		LLVector3 camera_pos = gAgentCamera.getCameraPositionAgent();
		F32 camera_water_height = camera_pos.mV[VZ] - region->getWaterHeight();
		
		//
		//  Don't update rolloff factor unless water surface has been crossed
		//
		if (force_update || (last_camera_water_height * camera_water_height) < 0.f)
		{
			static const LLCachedControl<F32> audio_level_rolloff("AudioLevelRolloff",1);
			if (camera_water_height < 0.f)
			{
				gAudiop->setRolloffFactor(audio_level_rolloff * LL_ROLLOFF_MULTIPLIER_UNDER_WATER);
			}
			else 
			{
				gAudiop->setRolloffFactor(audio_level_rolloff);
			}
		}
		// this line rotates the wind vector to be listener (agent) relative
		// unfortunately we have to pre-translate to undo the translation that
		// occurs in the transform call
		gRelativeWindVec = gAgent.getFrameAgent().rotateToLocal(gWindVec - gAgent.getVelocity());

		// don't use the setter setMaxWindGain() because we don't
		// want to screw up the fade-in on startup by setting actual source gain
		// outside the fade-in.
		static const LLCachedControl<bool> mute_audio("MuteAudio",false);
		static const LLCachedControl<bool> mute_ambient("MuteAmbient",false);
		static const LLCachedControl<F32> audio_level_master("AudioLevelMaster", 1.0f);
		static const LLCachedControl<F32> audio_level_ambient("AudioLevelAmbient",1.0f);
		F32 master_volume  = mute_audio ? 0.f : (F32)audio_level_master;
		F32 ambient_volume = mute_ambient ? 0.f : (F32)audio_level_ambient;

		F32 wind_volume = master_volume * ambient_volume;
		gAudiop->mMaxWindGain = wind_volume;
		
		
		last_camera_water_height = camera_water_height;
		gAudiop->updateWind(gRelativeWindVec, camera_water_height);
	}
#endif
}
