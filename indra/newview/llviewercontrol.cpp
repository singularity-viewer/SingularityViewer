/** 
 * @file llviewercontrol.cpp
 * @brief Viewer configuration
 * @author Richard Nelson
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llviewercontrol.h"

#include "indra_constants.h"

// For Listeners
#include "llaudioengine.h"
#include "llagent.h"
#include "llavatarnamecache.h"
#include "llconsole.h"
#include "lldrawpoolterrain.h"
#include "llflexibleobject.h"
#include "llfeaturemanager.h"
#include "llviewershadermgr.h"
#include "llpanelgeneral.h"
#include "llpanelinput.h"
#include "llsky.h"
#include "llvieweraudio.h"
#include "llviewertexturelist.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvoiceclient.h"
#include "llvosky.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llworld.h"
#include "pipeline.h"
#include "llviewerjoystick.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llparcel.h"
#include "llnotify.h"
#include "llkeyboard.h"
#include "llerrorcontrol.h"
#include "llversionviewer.h"
#include "llappviewer.h"
#include "llvosurfacepatch.h"
#include "llvowlsky.h"
#include "llworldmapview.h"
#include "llnetmap.h"
#include "llrender.h"
#include "llfloaterchat.h"
#include "statemachine/aistatemachine.h"
#include "aithreadsafe.h"
#include "llviewerobjectlist.h"
#include "lldrawpoolbump.h"
#include "emeraldboobutils.h"

#ifdef TOGGLE_HACKED_GODLIKE_VIEWER
BOOL 				gHackGodmode = FALSE;
#endif

AITHREADSAFE(settings_map_type, gSettings,);
LLControlGroup gSavedSettings;	// saved at end of session
LLControlGroup gSavedPerAccountSettings; // saved at end of session
LLControlGroup gColors;			// read-only
LLControlGroup gCrashSettings;	// saved at end of session

std::string gLastRunVersion;
std::string gCurrentVersion;

extern BOOL gResizeScreenTexture;
extern BOOL gDebugGL;
extern BOOL gAuditTexture;
////////////////////////////////////////////////////////////////////////////
// Listeners

static bool handleRenderAvatarMouselookChanged(const LLSD& newvalue)
{
	LLVOAvatar::sVisibleInFirstPerson = newvalue.asBoolean();
	return true;
}

static bool handleRenderFarClipChanged(const LLSD& newvalue)
{
	F32 draw_distance = (F32) newvalue.asReal();
	gAgent.mDrawDistance = draw_distance;
	LLWorld::getInstance()->setLandFarClip(draw_distance);
	return true;
}

static bool handleTerrainDetailChanged(const LLSD& newvalue)
{
	LLDrawPoolTerrain::sDetailMode = newvalue.asInteger();
	return true;
}

bool handleStateMachineMaxTimeChanged(const LLSD& newvalue)
{
	AIStateMachine::updateSettings();
	return true;
}

static bool handleSetShaderChanged(const LLSD& newvalue)
{
	// changing shader level may invalidate existing cached bump maps, as the shader type determines the format of the bump map it expects - clear and repopulate the bump cache
	gBumpImageList.destroyGL();
	gBumpImageList.restoreGL();

	// Changing shader also changes the terrain detail to high, reflect that change here
	if (newvalue.asBoolean())
	{
		// shaders enabled, set terrain detail to high
		gSavedSettings.setS32("RenderTerrainDetail", 1);
	}
	// else, leave terrain detail as is
	LLViewerShaderMgr::instance()->setShaders();
	return true;
}

static bool handleAvatarBoobMassChanged(const LLSD& newvalue)
{
	LLVOAvatar::sBoobConfig.mass = EmeraldBoobUtils::convertMass((F32) newvalue.asReal());
	return true;
}

static bool handleAvatarBoobHardnessChanged(const LLSD& newvalue)
{
	LLVOAvatar::sBoobConfig.hardness = EmeraldBoobUtils::convertHardness((F32) newvalue.asReal());
	return true;
}

static bool handleAvatarBoobVelMaxChanged(const LLSD& newvalue)
{
	LLVOAvatar::sBoobConfig.velMax = EmeraldBoobUtils::convertVelMax((F32) newvalue.asReal());
	LLVOAvatar::sBoobConfig.velMin = LLVOAvatar::sBoobConfig.velMin*LLVOAvatar::sBoobConfig.velMax;
	return true;
}

static bool handleAvatarBoobFrictionChanged(const LLSD& newvalue)
{
	LLVOAvatar::sBoobConfig.friction = EmeraldBoobUtils::convertFriction((F32) newvalue.asReal());
	return true;
}

static bool handleAvatarBoobVelMinChanged(const LLSD& newvalue)
{
	LLVOAvatar::sBoobConfig.velMin = EmeraldBoobUtils::convertVelMin((F32) newvalue.asReal())*LLVOAvatar::sBoobConfig.velMax;
	return true;
}

static bool handleAvatarBoobToggleChanged(const LLSD& newvalue)
{
	LLVOAvatar::sBoobConfig.enabled = (BOOL) newvalue.asReal();
	return true;
}

static bool handleAvatarBoobXYInfluence(const LLSD& newvalue)
{
	LLVOAvatar::sBoobConfig.XYInfluence = (F32) newvalue.asReal();
	return true;
}

static bool handleSetSelfInvisible( const LLSD& newvalue)
{
	LLVOAvatar::onChangeSelfInvisible( newvalue.asBoolean() );
	return true;
}

bool handleRenderTransparentWaterChanged(const LLSD& newvalue)
{
	LLWorld::getInstance()->updateWaterObjects();
	return true;
}

static bool handleReleaseGLBufferChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
	}
	return true;
}

static bool handleAnisotropicChanged(const LLSD& newvalue)
{
	LLImageGL::sGlobalUseAnisotropic = newvalue.asBoolean();
	LLImageGL::dirtyTexOptions();
	return true;
}

static bool handleVolumeLODChanged(const LLSD& newvalue)
{
	LLVOVolume::sLODFactor = (F32) newvalue.asReal();
	LLVOVolume::sDistanceFactor = 1.f-LLVOVolume::sLODFactor * 0.1f;
	return true;
}

static bool handleAvatarLODChanged(const LLSD& newvalue)
{
	LLVOAvatar::sLODFactor = (F32) newvalue.asReal();
	return true;
}

static bool handleAvatarPhysicsLODChanged(const LLSD& newvalue)
{
	LLVOAvatar::sPhysicsLODFactor = (F32) newvalue.asReal();
	return true;
}

static bool handleAvatarMaxVisibleChanged(const LLSD& newvalue)
{
	LLVOAvatar::sMaxVisible = (U32) newvalue.asInteger();
	return true;
}

static bool handleTerrainLODChanged(const LLSD& newvalue)
{
		LLVOSurfacePatch::sLODFactor = (F32)newvalue.asReal();
		//sqaure lod factor to get exponential range of [0,4] and keep
		//a value of 1 in the middle of the detail slider for consistency
		//with other detail sliders (see panel_preferences_graphics1.xml)
		LLVOSurfacePatch::sLODFactor *= LLVOSurfacePatch::sLODFactor;
		return true;
}

static bool handleTreeLODChanged(const LLSD& newvalue)
{
	LLVOTree::sTreeFactor = (F32) newvalue.asReal();
	return true;
}

static bool handleFlexLODChanged(const LLSD& newvalue)
{
	LLVolumeImplFlexible::sUpdateFactor = (F32) newvalue.asReal();
	return true;
}

static bool handleGammaChanged(const LLSD& newvalue)
{
	F32 gamma = (F32) newvalue.asReal();
	if (gamma == 0.0f)
	{
		gamma = 1.0f; // restore normal gamma
	}
	if (gViewerWindow && gViewerWindow->getWindow() && gamma != gViewerWindow->getWindow()->getGamma())
	{
		// Only save it if it's changed
		if (!gViewerWindow->getWindow()->setGamma(gamma))
		{
			llwarns << "setGamma failed!" << llendl;
		}
	}

	return true;
}

const F32 MAX_USER_FOG_RATIO = 10.f;
const F32 MIN_USER_FOG_RATIO = 0.5f;

static bool handleFogRatioChanged(const LLSD& newvalue)
{
	F32 fog_ratio = llmax(MIN_USER_FOG_RATIO, llmin((F32) newvalue.asReal(), MAX_USER_FOG_RATIO));
	gSky.setFogRatio(fog_ratio);
	return true;
}

static bool handleMaxPartCountChanged(const LLSD& newvalue)
{
	LLViewerPartSim::setMaxPartCount(newvalue.asInteger());
	return true;
}

static bool handleVideoMemoryChanged(const LLSD& newvalue)
{
	gTextureList.updateMaxResidentTexMem(newvalue.asInteger());
	return true;
}

static bool handleBandwidthChanged(const LLSD& newvalue)
{
	gViewerThrottle.setMaxBandwidth((F32) newvalue.asReal());
	return true;
}

static bool handleChatFontSizeChanged(const LLSD& newvalue)
{
	if(gConsole)
	{
		gConsole->setFontSize(newvalue.asInteger());
	}
	return true;
}

static bool handleChatPersistTimeChanged(const LLSD& newvalue)
{
	if(gConsole)
	{
		gConsole->setLinePersistTime((F32) newvalue.asReal());
	}
	return true;
}

static void handleAudioVolumeChanged(const LLSD& newvalue)
{
	audio_update_volume(true);
}

static bool handleJoystickChanged(const LLSD& newvalue)
{
	LLViewerJoystick::getInstance()->setCameraNeedsUpdate(TRUE);
	return true;
}

static bool handleAudioStreamMusicChanged(const LLSD& newvalue)
{
	if (gAudiop)
	{
		if ( newvalue.asBoolean() )
		{
			if (LLViewerParcelMgr::getInstance()->getAgentParcel()
				&& !LLViewerParcelMgr::getInstance()->getAgentParcel()->getMusicURL().empty())
			{
				// if stream is already playing, don't call this
				// otherwise music will briefly stop
				if ( !gAudiop->isInternetStreamPlaying() )
				{
					LLViewerParcelMedia::playStreamingMusic(LLViewerParcelMgr::getInstance()->getAgentParcel());
				}
			}
		}
		else
		{
			gAudiop->stopInternetStream();
		}
	}
	return true;
}

static bool handleUseOcclusionChanged(const LLSD& newvalue)
{
	LLPipeline::sUseOcclusion = (newvalue.asBoolean() && gGLManager.mHasOcclusionQuery 
		&& LLFeatureManager::getInstance()->isFeatureAvailable("UseOcclusion") && !gUseWireframe) ? 2 : 0;
	return true;
}

static bool handleUploadBakedTexOldChanged(const LLSD& newvalue)
{
	LLPipeline::sForceOldBakedUpload = newvalue.asBoolean();
	return true;
}

static bool handleNumpadControlChanged(const LLSD& newvalue)
{
	if (gKeyboard)
	{
		gKeyboard->setNumpadDistinct(static_cast<LLKeyboard::e_numpad_distinct>(newvalue.asInteger()));
	}
	return true;
}

static bool handleRenderUseVBOChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		gPipeline.setUseVBO(newvalue.asBoolean());
	}
	return true;
}

static bool handleRenderUseVBOMappingChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		gPipeline.setDisableVBOMapping(newvalue.asBoolean());
	}
	return true;
}

static bool handleWLSkyDetailChanged(const LLSD&)
{
	if (gSky.mVOWLSkyp.notNull())
	{
		gSky.mVOWLSkyp->updateGeometry(gSky.mVOWLSkyp->mDrawable);
	}
	return true;
}

static bool handleResetVertexBuffersChanged(const LLSD&)
{
	if (gPipeline.isInit())
	{
		gPipeline.resetVertexBuffers();
	}
	return true;
}

static bool handleRenderDynamicLODChanged(const LLSD& newvalue)
{
	LLPipeline::sDynamicLOD = newvalue.asBoolean();
	return true;
}

static bool handleRenderLocalLightsChanged(const LLSD& newvalue)
{
	gPipeline.setLightingDetail(-1);
	return true;
}

static bool handleRenderUseFBOChanged(const LLSD& newvalue)
{
	LLRenderTarget::sUseFBO = newvalue.asBoolean();
	if (gPipeline.isInit())
	{
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
		if (LLPipeline::sRenderDeferred && LLRenderTarget::sUseFBO)
		{
			LLViewerShaderMgr::instance()->setShaders();
		}
	}
	return true;
}

static bool handleRenderUseImpostorsChanged(const LLSD& newvalue)
{
	LLVOAvatar::sUseImpostors = newvalue.asBoolean();
	return true;
}

static bool handleAuditTextureChanged(const LLSD& newvalue)
{
	gAuditTexture = newvalue.asBoolean();
	return true;
}

static bool handleRenderDebugGLChanged(const LLSD& newvalue)
{
	gDebugGL = newvalue.asBoolean();
	gGL.clearErrors();
	return true;
}

static bool handleRenderDebugPipelineChanged(const LLSD& newvalue)
{
	gDebugPipeline = newvalue.asBoolean();
	return true;
}

static bool handleRenderResolutionDivisorChanged(const LLSD&)
{
	gResizeScreenTexture = TRUE;
	return true;
}

static bool handleDebugViewsChanged(const LLSD& newvalue)
{
	LLView::sDebugRects = newvalue.asBoolean();
	return true;
}

static bool handleLogFileChanged(const LLSD& newvalue)
{
	std::string log_filename = newvalue.asString();
	LLFile::remove(log_filename);
	LLError::logToFile(log_filename);
	return true;
}

bool handleHideGroupTitleChanged(const LLSD& newvalue)
{
	gAgent.setHideGroupTitle(newvalue);
	return true;
}

bool handleEffectColorChanged(const LLSD& newvalue)
{
	gAgent.setEffectColor(LLColor4(newvalue));
	return true;
}

bool handleVectorizeChanged(const LLSD& newvalue)
{
	LLViewerJointMesh::updateVectorize();
	return true;
}

bool handleVoiceClientPrefsChanged(const LLSD& newvalue)
{
	if(gVoiceClient)
	{
		gVoiceClient->updateSettings();
	}
	return true;
}

bool handleTranslateChatPrefsChanged(const LLSD& newvalue)
{
	LLFloaterChat* floaterp = LLFloaterChat::getInstance();

	if(floaterp)
	{
		// update "translate chat" pref in "Local Chat" floater
		floaterp->updateSettings();
	}
	return true;
}

bool handleCloudSettingsChanged(const LLSD& newvalue)
{
	bool bCloudsEnabled = gSavedSettings.getBOOL("CloudsEnabled");
	if((bool)LLPipeline::hasRenderTypeControl((void*)LLPipeline::RENDER_TYPE_WL_CLOUDS)!=bCloudsEnabled)
		LLPipeline::toggleRenderTypeControl((void*)LLPipeline::RENDER_TYPE_WL_CLOUDS);

	if( !gSavedSettings.getBOOL("SkyUseClassicClouds") ) bCloudsEnabled = false;

	if((bool)LLPipeline::hasRenderTypeControl((void*)LLPipeline::RENDER_TYPE_CLASSIC_CLOUDS)!=bCloudsEnabled )
		LLPipeline::toggleRenderTypeControl((void*)LLPipeline::RENDER_TYPE_CLASSIC_CLOUDS);
	return true;
}

bool handleAscentSelfTag(const LLSD& newvalue)
{
	if(gAgent.getAvatarObject())
		gAgent.getAvatarObject()->mClientTag = "";
	return true;
}

bool handleAscentGlobalTag(const LLSD& newvalue)
{
	S32 object_count = gObjectList.getNumObjects();
	for (S32 i = 0; i < object_count; i++)
	{
		LLViewerObject *objectp = gObjectList.getObject(i);
		if (objectp && objectp->isAvatar())
			((LLVOAvatar*)objectp)->mClientTag = "";
	}
	return true;
}

bool handleAscentAvatarModifier(const LLSD& newvalue)
{
	llinfos << "Calling gAgent.sendAgentSetAppearance() because AscentAvatar*Modifier changed." << llendl;
	gAgent.sendAgentSetAppearance();
	return true;
}

// [Ansariel: Display name support]
static bool handlePhoenixNameSystemChanged(const LLSD& newvalue)
{
	S32 dnval = (S32)newvalue.asInteger();
	if (dnval <= 0 || dnval > 2) LLAvatarNameCache::setUseDisplayNames(false);
	else LLAvatarNameCache::setUseDisplayNames(true);
	LLVOAvatar::invalidateNameTags();
	return true;
}
// [/Ansariel: Display name support]

////////////////////////////////////////////////////////////////////////////
void settings_setup_listeners()
{
	gSavedSettings.getControl("FirstPersonAvatarVisible")->getSignal()->connect(boost::bind(&handleRenderAvatarMouselookChanged, _1));
	gSavedSettings.getControl("RenderFarClip")->getSignal()->connect(boost::bind(&handleRenderFarClipChanged, _1));
	gSavedSettings.getControl("RenderTerrainDetail")->getSignal()->connect(boost::bind(&handleTerrainDetailChanged, _1));
	gSavedSettings.getControl("RenderAnimateTrees")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _1));
	gSavedSettings.getControl("RenderAvatarVP")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("VertexShaderEnable")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("RenderSpecularResX")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _1));
	gSavedSettings.getControl("RenderSpecularResY")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _1));
	gSavedSettings.getControl("RenderSpecularExponent")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _1));
	gSavedSettings.getControl("RenderFSAASamples")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _1));
	gSavedSettings.getControl("RenderAnisotropic")->getSignal()->connect(boost::bind(&handleAnisotropicChanged, _1));
	gSavedSettings.getControl("RenderShadowResolutionScale")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _1));
	gSavedSettings.getControl("RenderGlow")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _1));
	gSavedSettings.getControl("RenderGlow")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("EnableRippleWater")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("RenderGlowResolutionPow")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _1));
	gSavedSettings.getControl("RenderAvatarCloth")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("WindLightUseAtmosShaders")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("RenderGammaFull")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("RenderAvatarMaxVisible")->getSignal()->connect(boost::bind(&handleAvatarMaxVisibleChanged, _1));
	gSavedSettings.getControl("RenderAvatarInvisible")->getSignal()->connect(boost::bind(&handleSetSelfInvisible, _1));
	gSavedSettings.getControl("RenderVolumeLODFactor")->getSignal()->connect(boost::bind(&handleVolumeLODChanged, _1));
	gSavedSettings.getControl("RenderAvatarLODFactor")->getSignal()->connect(boost::bind(&handleAvatarLODChanged, _1));
	gSavedSettings.getControl("RenderAvatarPhysicsLODFactor")->getSignal()->connect(boost::bind(&handleAvatarPhysicsLODChanged, _1));
	gSavedSettings.getControl("RenderTerrainLODFactor")->getSignal()->connect(boost::bind(&handleTerrainLODChanged, _1));
	gSavedSettings.getControl("RenderTreeLODFactor")->getSignal()->connect(boost::bind(&handleTreeLODChanged, _1));
	gSavedSettings.getControl("RenderFlexTimeFactor")->getSignal()->connect(boost::bind(&handleFlexLODChanged, _1));
	gSavedSettings.getControl("ThrottleBandwidthKBPS")->getSignal()->connect(boost::bind(&handleBandwidthChanged, _1));
	gSavedSettings.getControl("RenderGamma")->getSignal()->connect(boost::bind(&handleGammaChanged, _1));
	gSavedSettings.getControl("RenderFogRatio")->getSignal()->connect(boost::bind(&handleFogRatioChanged, _1));
	gSavedSettings.getControl("RenderMaxPartCount")->getSignal()->connect(boost::bind(&handleMaxPartCountChanged, _1));
	gSavedSettings.getControl("RenderDynamicLOD")->getSignal()->connect(boost::bind(&handleRenderDynamicLODChanged, _1));
	gSavedSettings.getControl("RenderLocalLights")->getSignal()->connect(boost::bind(&handleRenderLocalLightsChanged, _1));
	gSavedSettings.getControl("RenderDebugTextureBind")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _1));
	gSavedSettings.getControl("RenderFastAlpha")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _1));
	gSavedSettings.getControl("RenderObjectBump")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _1));
	gSavedSettings.getControl("RenderMaxVBOSize")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _1));
	//See LL jira VWR-3258 comment section. Implemented by LL in 2.1 -Shyotl
	gSavedSettings.getControl("ShyotlRenderUseStreamVBO")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _1));
	gSavedSettings.getControl("SianaRenderOmitBlankVBO")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _1));
	gSavedSettings.getControl("RenderUseFBO")->getSignal()->connect(boost::bind(&handleRenderUseFBOChanged, _1));
	gSavedSettings.getControl("RenderDeferredNoise")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _1));
	gSavedSettings.getControl("RenderUseImpostors")->getSignal()->connect(boost::bind(&handleRenderUseImpostorsChanged, _1));
	gSavedSettings.getControl("RenderDebugGL")->getSignal()->connect(boost::bind(&handleRenderDebugGLChanged, _1));
	gSavedSettings.getControl("RenderDebugPipeline")->getSignal()->connect(boost::bind(&handleRenderDebugPipelineChanged, _1));
	gSavedSettings.getControl("RenderResolutionDivisor")->getSignal()->connect(boost::bind(&handleRenderResolutionDivisorChanged, _1));
	gSavedSettings.getControl("RenderDeferred")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("RenderShadowDetail")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("RenderDeferredSSAO")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("RenderDeferredGI")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _1));
	gSavedSettings.getControl("TextureMemory")->getSignal()->connect(boost::bind(&handleVideoMemoryChanged, _1));
	gSavedSettings.getControl("AuditTexture")->getSignal()->connect(boost::bind(&handleAuditTextureChanged, _1));
	gSavedSettings.getControl("ChatFontSize")->getSignal()->connect(boost::bind(&handleChatFontSizeChanged, _1));
	gSavedSettings.getControl("ChatPersistTime")->getSignal()->connect(boost::bind(&handleChatPersistTimeChanged, _1));
	gSavedSettings.getControl("UploadBakedTexOld")->getSignal()->connect(boost::bind(&handleUploadBakedTexOldChanged, _1));
	gSavedSettings.getControl("UseOcclusion")->getSignal()->connect(boost::bind(&handleUseOcclusionChanged, _1));
	gSavedSettings.getControl("AudioLevelMaster")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("AudioLevelSFX")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("AudioLevelUI")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("AudioLevelAmbient")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("AudioLevelMusic")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("AudioLevelMedia")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("AudioLevelVoice")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("AudioLevelDoppler")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("AudioLevelRolloff")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("AudioStreamingMusic")->getSignal()->connect(boost::bind(&handleAudioStreamMusicChanged, _1));
	gSavedSettings.getControl("MuteAudio")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("MuteMusic")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("MuteMedia")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("MuteVoice")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("MuteAmbient")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("MuteUI")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _1));
	gSavedSettings.getControl("RenderVBOEnable")->getSignal()->connect(boost::bind(&handleRenderUseVBOChanged, _1));
	gSavedSettings.getControl("RenderVBOMappingDisable")->getSignal()->connect(boost::bind(&handleRenderUseVBOMappingChanged, _1));
	gSavedSettings.getControl("RenderPreferStreamDraw")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _1));
	gSavedSettings.getControl("WLSkyDetail")->getSignal()->connect(boost::bind(&handleWLSkyDetailChanged, _1));
	gSavedSettings.getControl("NumpadControl")->getSignal()->connect(boost::bind(&handleNumpadControlChanged, _1));
	gSavedSettings.getControl("JoystickAxis0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("JoystickAxis1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("JoystickAxis2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("JoystickAxis3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("JoystickAxis4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("JoystickAxis5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("JoystickAxis6")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisScale0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisScale1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisScale2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisScale3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisScale4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisScale5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisScale6")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisDeadZone0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisDeadZone1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisDeadZone2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisDeadZone3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisDeadZone4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisDeadZone5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("FlycamAxisDeadZone6")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisScale0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisScale1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisScale2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisScale3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisScale4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisScale5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisDeadZone0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisDeadZone1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisDeadZone2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisDeadZone3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisDeadZone4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("AvatarAxisDeadZone5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisScale0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisScale1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisScale2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisScale3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisScale4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisScale5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisDeadZone0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisDeadZone1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisDeadZone2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisDeadZone3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisDeadZone4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("BuildAxisDeadZone5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _1));
	gSavedSettings.getControl("DebugViews")->getSignal()->connect(boost::bind(&handleDebugViewsChanged, _1));
	gSavedSettings.getControl("UserLogFile")->getSignal()->connect(boost::bind(&handleLogFileChanged, _1));
	gSavedSettings.getControl("RenderHideGroupTitle")->getSignal()->connect(boost::bind(handleHideGroupTitleChanged, _1));
	gSavedSettings.getControl("EffectColor")->getSignal()->connect(boost::bind(handleEffectColorChanged, _1));
	gSavedSettings.getControl("VectorizePerfTest")->getSignal()->connect(boost::bind(&handleVectorizeChanged, _1));
	gSavedSettings.getControl("VectorizeEnable")->getSignal()->connect(boost::bind(&handleVectorizeChanged, _1));
	gSavedSettings.getControl("VectorizeProcessor")->getSignal()->connect(boost::bind(&handleVectorizeChanged, _1));
	gSavedSettings.getControl("VectorizeSkin")->getSignal()->connect(boost::bind(&handleVectorizeChanged, _1));
	gSavedSettings.getControl("EnableVoiceChat")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _1));
	gSavedSettings.getControl("PTTCurrentlyEnabled")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _1));
	gSavedSettings.getControl("PushToTalkButton")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _1));
	gSavedSettings.getControl("PushToTalkToggle")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _1));
	gSavedSettings.getControl("VoiceEarLocation")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _1));
	gSavedSettings.getControl("VoiceInputAudioDevice")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _1));
	gSavedSettings.getControl("VoiceOutputAudioDevice")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _1));
	gSavedSettings.getControl("AudioLevelMic")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _1));
	gSavedSettings.getControl("LipSyncEnabled")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _1));	
	gSavedSettings.getControl("TranslateChat")->getSignal()->connect(boost::bind(&handleTranslateChatPrefsChanged, _1));
	gSavedSettings.getControl("StateMachineMaxTime")->getSignal()->connect(boost::bind(&handleStateMachineMaxTimeChanged, _1));

	gSavedSettings.getControl("CloudsEnabled")->getSignal()->connect(boost::bind(&handleCloudSettingsChanged, _1));
	gSavedSettings.getControl("SkyUseClassicClouds")->getSignal()->connect(boost::bind(&handleCloudSettingsChanged, _1));
	gSavedSettings.getControl("RenderTransparentWater")->getSignal()->connect(boost::bind(&handleRenderTransparentWaterChanged, _1));

	gSavedSettings.getControl("EmeraldBoobMass")->getSignal()->connect(boost::bind(&handleAvatarBoobMassChanged, _1));
	gSavedSettings.getControl("EmeraldBoobHardness")->getSignal()->connect(boost::bind(&handleAvatarBoobHardnessChanged, _1));
	gSavedSettings.getControl("EmeraldBoobVelMax")->getSignal()->connect(boost::bind(&handleAvatarBoobVelMaxChanged, _1));
	gSavedSettings.getControl("EmeraldBoobFriction")->getSignal()->connect(boost::bind(&handleAvatarBoobFrictionChanged, _1));
	gSavedSettings.getControl("EmeraldBoobVelMin")->getSignal()->connect(boost::bind(&handleAvatarBoobVelMinChanged, _1));
	gSavedSettings.getControl("EmeraldBreastPhysicsToggle")->getSignal()->connect(boost::bind(&handleAvatarBoobToggleChanged, _1));
	gSavedSettings.getControl("EmeraldBoobXYInfluence")->getSignal()->connect(boost::bind(&handleAvatarBoobXYInfluence, _1));
	
	gSavedSettings.getControl("AscentUseTag")->getSignal()->connect(boost::bind(&handleAscentSelfTag,_1));
	gSavedSettings.getControl("AscentUseCustomTag")->getSignal()->connect(boost::bind(&handleAscentSelfTag,_1));
	gSavedSettings.getControl("AscentCustomTagColor")->getSignal()->connect(boost::bind(&handleAscentSelfTag,_1));
	gSavedSettings.getControl("AscentCustomTagLabel")->getSignal()->connect(boost::bind(&handleAscentSelfTag,_1));
	gSavedSettings.getControl("AscentReportClientUUID")->getSignal()->connect(boost::bind(&handleAscentSelfTag,_1));
	gSavedSettings.getControl("AscentShowFriendsTag")->getSignal()->connect(boost::bind(&handleAscentGlobalTag,_1));
	gSavedSettings.getControl("AscentUseStatusColors")->getSignal()->connect(boost::bind(&handleAscentGlobalTag,_1));
	gSavedSettings.getControl("AscentAvatarXModifier")->getSignal()->connect(boost::bind(&handleAscentAvatarModifier, _1));
	gSavedSettings.getControl("AscentAvatarYModifier")->getSignal()->connect(boost::bind(&handleAscentAvatarModifier, _1));
	gSavedSettings.getControl("AscentAvatarZModifier")->getSignal()->connect(boost::bind(&handleAscentAvatarModifier, _1));

    // [Ansariel: Display name support]
	gSavedSettings.getControl("PhoenixNameSystem")->getSignal()->connect(boost::bind(&handlePhoenixNameSystemChanged, _1));
    // [/Ansariel: Display name support]
}

template <> eControlType get_control_type<U32>(const U32& in, LLSD& out) 
{ 
	out = (LLSD::Integer)in; 
	return TYPE_U32; 
}

template <> eControlType get_control_type<S32>(const S32& in, LLSD& out) 
{ 
	out = in; 
	return TYPE_S32; 
}

template <> eControlType get_control_type<F32>(const F32& in, LLSD& out) 
{ 
	out = in; 
	return TYPE_F32; 
}

template <> eControlType get_control_type<bool> (const bool& in, LLSD& out) 
{ 
	out = in; 
	return TYPE_BOOLEAN; 
}
/*
// Yay BOOL, its really an S32.
template <> eControlType get_control_type<BOOL> (const BOOL& in, LLSD& out) 
{ 
	out = in; 
	return TYPE_BOOLEAN; 
}
*/
template <> eControlType get_control_type<std::string>(const std::string& in, LLSD& out) 
{ 
	out = in; 
	return TYPE_STRING; 
}

template <> eControlType get_control_type<LLVector3>(const LLVector3& in, LLSD& out) 
{ 
	out = in.getValue(); 
	return TYPE_VEC3; 
}

template <> eControlType get_control_type<LLVector3d>(const LLVector3d& in, LLSD& out) 
{ 
	out = in.getValue(); 
	return TYPE_VEC3D; 
}

template <> eControlType get_control_type<LLRect>(const LLRect& in, LLSD& out) 
{ 
	out = in.getValue(); 
	return TYPE_RECT; 
}

template <> eControlType get_control_type<LLColor4>(const LLColor4& in, LLSD& out) 
{ 
	out = in.getValue(); 
	return TYPE_COL4; 
}

template <> eControlType get_control_type<LLColor3>(const LLColor3& in, LLSD& out) 
{ 
	out = in.getValue(); 
	return TYPE_COL3; 
}

template <> eControlType get_control_type<LLColor4U>(const LLColor4U& in, LLSD& out) 
{ 
	out = in.getValue();
	return TYPE_COL4U; 
}

template <> eControlType get_control_type<LLSD>(const LLSD& in, LLSD& out) 
{ 
	out = in;
	return TYPE_LLSD; 
}

void onCommitControlSetting_gSavedSettings(LLUICtrl* ctrl, void* name)
{
	gSavedSettings.setValue((const char*)name,ctrl->getValue());
}

void onCommitControlSetting_gSavedPerAccountSettings(LLUICtrl* ctrl, void* name)
{
	gSavedPerAccountSettings.setValue((const char*)name,ctrl->getValue());
}

#if TEST_CACHED_CONTROL

#define DECL_LLCC(T, V) static LLCachedControl<T> mySetting_##T("TestCachedControl"#T, V)
DECL_LLCC(U32, (U32)666);
DECL_LLCC(S32, (S32)-666);
DECL_LLCC(F32, (F32)-666.666);
DECL_LLCC(bool, true);
DECL_LLCC(BOOL, FALSE);
static LLCachedControl<std::string> mySetting_string("TestCachedControlstring", "Default String Value");
DECL_LLCC(LLVector3, LLVector3(1.0f, 2.0f, 3.0f));
DECL_LLCC(LLVector3d, LLVector3d(6.0f, 5.0f, 4.0f));
DECL_LLCC(LLRect, LLRect(0, 0, 100, 500));
DECL_LLCC(LLColor4, LLColor4(0.0f, 0.5f, 1.0f));
DECL_LLCC(LLColor3, LLColor3(1.0f, 0.f, 0.5f));
DECL_LLCC(LLColor4U, LLColor4U(255, 200, 100, 255));

LLSD test_llsd = LLSD()["testing1"] = LLSD()["testing2"];
DECL_LLCC(LLSD, test_llsd);

static LLCachedControl<std::string> test_BrowserHomePage("BrowserHomePage", "hahahahahha", "Not the real comment");

void test_cached_control()
{
#define do { TEST_LLCC(T, V) if((T)mySetting_##T != V) llerrs << "Fail "#T << llendl; } while(0)
	TEST_LLCC(U32, 666);
	TEST_LLCC(S32, (S32)-666);
	TEST_LLCC(F32, (F32)-666.666);
	TEST_LLCC(bool, true);
	TEST_LLCC(BOOL, FALSE);
	if((std::string)mySetting_string != "Default String Value") llerrs << "Fail string" << llendl;
	TEST_LLCC(LLVector3, LLVector3(1.0f, 2.0f, 3.0f));
	TEST_LLCC(LLVector3d, LLVector3d(6.0f, 5.0f, 4.0f));
	TEST_LLCC(LLRect, LLRect(0, 0, 100, 500));
	TEST_LLCC(LLColor4, LLColor4(0.0f, 0.5f, 1.0f));
	TEST_LLCC(LLColor3, LLColor3(1.0f, 0.f, 0.5f));
	TEST_LLCC(LLColor4U, LLColor4U(255, 200, 100, 255));
//There's no LLSD comparsion for LLCC yet. TEST_LLCC(LLSD, test_llsd); 

	if((std::string)test_BrowserHomePage != "http://www.secondlife.com") llerrs << "Fail BrowserHomePage" << llendl;
}
#endif // TEST_CACHED_CONTROL

