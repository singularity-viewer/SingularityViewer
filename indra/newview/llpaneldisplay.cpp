/** 
 * @file llpaneldisplay.cpp
 * @brief Display preferences for the preferences floater
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

// file include
#include "llpaneldisplay.h"

// linden library includes
#include "llerror.h"
#include "llfontgl.h"
#include "llrect.h"
#include "llstring.h"

// project includes
#include "llagent.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llflexibleobject.h"
#include "lllineeditor.h"
#include "llradiogroup.h"
#include "llresmgr.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llstartup.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llviewermessage.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvovolume.h"
#include "llvotree.h"
#include "llvosky.h"
#include "llwindow.h"
#include "llworld.h"
#include "pipeline.h"
#include "lluictrlfactory.h"
#include "llfeaturemanager.h"
#include "llviewershadermgr.h"
#include "llfloaterhardwaresettings.h"
#include "llboost.h"

//RN temporary includes for resolution switching
#include "llglheaders.h"
#include "llviewercontrol.h"
#include "llsky.h"

// parent
#include "llfloaterpreference.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

#if LL_MSVC
#pragma warning( disable       : 4265 )	// "class has virtual functions, but destructor is not virtual"
#endif


#include <boost/regex.hpp>

const F32 MAX_USER_FAR_CLIP = 512.f;
const F32 MIN_USER_FAR_CLIP = 64.f;

const S32 ASPECT_RATIO_STR_LEN = 100;

LLPanelDisplay::LLPanelDisplay()
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_graphics1.xml");
}

BOOL LLPanelDisplay::postBuild()
{
	// return to default values
	childSetAction("Defaults", setHardwareDefaults, NULL);
	
	// Help button
	childSetAction("GraphicsPreferencesHelpButton", onOpenHelp, this);

	// Hardware settings button
	childSetAction("GraphicsHardwareButton", onOpenHardwareSettings, NULL);

	//============================================================================
	// Resolution
	
	// radio set for fullscreen size
	
	mCtrlWindowed = getChild<LLCheckBoxCtrl>( "windowed mode");
	mCtrlWindowed->setCommitCallback(&LLPanelDisplay::onCommitWindowedMode);
	mCtrlWindowed->setCallbackUserData(this);

	mAspectRatioLabel1 = getChild<LLTextBox>("AspectRatioLabel1");
	mFullScreenInfo = getChild<LLTextEditor>("FullScreenInfo");
	mDisplayResLabel = getChild<LLTextBox>("DisplayResLabel");

	S32 num_resolutions = 0;
	LLWindow::LLWindowResolution* supported_resolutions = gViewerWindow->getWindow()->getSupportedResolutions(num_resolutions);

	S32 fullscreen_mode = num_resolutions - 1;

	mCtrlFullScreen = getChild<LLComboBox>( "fullscreen combo");
	
	LLUIString resolution_label = getString("resolution_format");

	for (S32 i = 0; i < num_resolutions; i++)
	{
		resolution_label.setArg("[RES_X]", llformat("%d", supported_resolutions[i].mWidth));
		resolution_label.setArg("[RES_Y]", llformat("%d", supported_resolutions[i].mHeight));
		mCtrlFullScreen->add( resolution_label, ADD_BOTTOM );
	}

	{
		BOOL targetFullscreen;
		S32 targetWidth;
		S32 targetHeight;
		
		gViewerWindow->getTargetWindow(targetFullscreen, targetWidth, targetHeight);
		
		if (targetFullscreen)
		{
			fullscreen_mode = 0; // default to 800x600
			for (S32 i = 0; i < num_resolutions; i++)
			{
				if (targetWidth == supported_resolutions[i].mWidth
				&&  targetHeight == supported_resolutions[i].mHeight)
				{
					fullscreen_mode = i;
				}
			}
			mCtrlFullScreen->setCurrentByIndex(fullscreen_mode);
			mCtrlWindowed->set(FALSE);
			mCtrlFullScreen->setVisible(TRUE);
		}
		else
		{
			// set to windowed mode
			//fullscreen_mode = mCtrlFullScreen->getItemCount() - 1;
			mCtrlWindowed->set(TRUE);
			mCtrlFullScreen->setCurrentByIndex(0);
			mCtrlFullScreen->setVisible(FALSE);
		}
	}

	initWindowSizeControls();
	
	if (gSavedSettings.getBOOL("FullScreenAutoDetectAspectRatio"))
	{
		mAspectRatio = gViewerWindow->getDisplayAspectRatio();
	}
	else
	{
		mAspectRatio = gSavedSettings.getF32("FullScreenAspectRatio");
	}

	S32 numerator = 0;
	S32 denominator = 0;
	fractionFromDecimal(mAspectRatio, numerator, denominator);

	LLUIString aspect_ratio_text = getString("aspect_ratio_text");
	if (numerator != 0)
	{
		aspect_ratio_text.setArg("[NUM]", llformat("%d",  numerator));
		aspect_ratio_text.setArg("[DEN]", llformat("%d",  denominator));
	}
	else
	{
		aspect_ratio_text = llformat("%.3f", mAspectRatio);
	}

	mCtrlAspectRatio = getChild<LLComboBox>( "aspect_ratio");
	mCtrlAspectRatio->setTextEntryCallback(onKeystrokeAspectRatio);
	mCtrlAspectRatio->setCommitCallback(onSelectAspectRatio);
	mCtrlAspectRatio->setCallbackUserData(this);
	// add default aspect ratios
	mCtrlAspectRatio->add(aspect_ratio_text, &mAspectRatio, ADD_TOP);
	mCtrlAspectRatio->setCurrentByIndex(0);

	mCtrlAutoDetectAspect = getChild<LLCheckBoxCtrl>( "aspect_auto_detect");
	mCtrlAutoDetectAspect->setCommitCallback(onCommitAutoDetectAspect);
	mCtrlAutoDetectAspect->setCallbackUserData(this);

	// radio performance box
	mCtrlSliderQuality = getChild<LLSliderCtrl>("QualityPerformanceSelection");
	mCtrlSliderQuality->setSliderMouseUpCallback(boost::bind(&LLPanelDisplay::onChangeQuality,mCtrlSliderQuality,this));
	mCtrlSliderQuality->setCallbackUserData(this);

	mCtrlCustomSettings = getChild<LLCheckBoxCtrl>("CustomSettings");
	mCtrlCustomSettings->setCommitCallback(onChangeCustom);
	mCtrlCustomSettings->setCallbackUserData(this);

	mGraphicsBorder = getChild<LLViewBorder>("GraphicsBorder");

	//----------------------------------------------------------------------------
	// Enable Bump/Shiny
	mCtrlBumpShiny = getChild<LLCheckBoxCtrl>("BumpShiny");
	
	//----------------------------------------------------------------------------
	// Enable Reflections
	mCtrlReflectionDetail = getChild<LLComboBox>("ReflectionDetailCombo");
	mCtrlReflectionDetail->setCommitCallback(&LLPanelDisplay::onVertexShaderEnable);
	mCtrlReflectionDetail->setCallbackUserData(this);

	// WindLight
	mCtrlWindLight = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
	mCtrlWindLight->setCommitCallback(&LLPanelDisplay::onVertexShaderEnable);
	mCtrlWindLight->setCallbackUserData(this);

	// Deferred
	mCtrlDeferred = getChild<LLCheckBoxCtrl>("RenderDeferred");
	mCtrlDeferred->setCommitCallback(&LLPanelDisplay::onVertexShaderEnable);
	mCtrlDeferred->setCallbackUserData(this);
	mCtrlDeferredGI = getChild<LLCheckBoxCtrl>("RenderDeferredGI");
	mCtrlDeferredGI->setCommitCallback(&LLPanelDisplay::onVertexShaderEnable);
	mCtrlDeferredGI->setCallbackUserData(this);
	mCtrlShadowDetail = getChild<LLComboBox>("ShadowDetailCombo");
	mCtrlShadowDetail->setCommitCallback(&LLPanelDisplay::onVertexShaderEnable);
	mCtrlShadowDetail->setCallbackUserData(this);

	//----------------------------------------------------------------------------
	// Enable Avatar Shaders
	mCtrlAvatarVP = getChild<LLCheckBoxCtrl>("AvatarVertexProgram");
	mCtrlAvatarVP->setCommitCallback(&LLPanelDisplay::onVertexShaderEnable);
	mCtrlAvatarVP->setCallbackUserData(this);

	//----------------------------------------------------------------------------
	// Avatar Render Mode
	mCtrlAvatarCloth = getChild<LLCheckBoxCtrl>("AvatarCloth");
	mCtrlAvatarImpostors = getChild<LLCheckBoxCtrl>("AvatarImpostors");
	mCtrlAvatarImpostors->setCommitCallback(&LLPanelDisplay::onVertexShaderEnable);
	mCtrlAvatarImpostors->setCallbackUserData(this);
	mCtrlNonImpostors = getChild<LLSliderCtrl>("AvatarMaxVisible");

	//----------------------------------------------------------------------------
	// radio set for lighting detail
	mRadioLightingDetail2 = getChild<LLRadioGroup>("LightingDetailRadio");

	//----------------------------------------------------------------------------
	// radio set for terrain detail mode
	mRadioTerrainDetail = getChild<LLRadioGroup>("TerrainDetailRadio");

	//----------------------------------------------------------------------------
	// Global Shader Enable
	mCtrlShaderEnable = getChild<LLCheckBoxCtrl>("BasicShaders");
	mCtrlShaderEnable->setCommitCallback(&LLPanelDisplay::onVertexShaderEnable);
	mCtrlShaderEnable->setCallbackUserData(this);
	
	//============================================================================

	// Object detail slider
	mCtrlDrawDistance = getChild<LLSliderCtrl>("DrawDistance");
	mDrawDistanceMeterText1 = getChild<LLTextBox>("DrawDistanceMeterText1");
	mDrawDistanceMeterText2 = getChild<LLTextBox>("DrawDistanceMeterText2");
	mCtrlDrawDistance->setCommitCallback(&LLPanelDisplay::updateMeterText);
	mCtrlDrawDistance->setCallbackUserData(this);

	// Object detail slider
	mCtrlLODFactor = getChild<LLSliderCtrl>("ObjectMeshDetail");
	mLODFactorText = getChild<LLTextBox>("ObjectMeshDetailText");
	mCtrlLODFactor->setCommitCallback(&LLPanelDisplay::updateSliderText);
	mCtrlLODFactor->setCallbackUserData(mLODFactorText);

	// Flex object detail slider
	mCtrlFlexFactor = getChild<LLSliderCtrl>("FlexibleMeshDetail");
	mFlexFactorText = getChild<LLTextBox>("FlexibleMeshDetailText");
	mCtrlFlexFactor->setCommitCallback(&LLPanelDisplay::updateSliderText);
	mCtrlFlexFactor->setCallbackUserData(mFlexFactorText);

	// Tree detail slider
	mCtrlTreeFactor = getChild<LLSliderCtrl>("TreeMeshDetail");
	mTreeFactorText = getChild<LLTextBox>("TreeMeshDetailText");
	mCtrlTreeFactor->setCommitCallback(&LLPanelDisplay::updateSliderText);
	mCtrlTreeFactor->setCallbackUserData(mTreeFactorText);

	// Avatar detail slider
	mCtrlAvatarFactor = getChild<LLSliderCtrl>("AvatarMeshDetail");
	mAvatarFactorText = getChild<LLTextBox>("AvatarMeshDetailText");
	mCtrlAvatarFactor->setCommitCallback(&LLPanelDisplay::updateSliderText);
	mCtrlAvatarFactor->setCallbackUserData(mAvatarFactorText);

	// Avatar physics detail slider
	mCtrlAvatarPhysicsFactor = getChild<LLSliderCtrl>("AvatarPhysicsDetail");
	mAvatarPhysicsFactorText = getChild<LLTextBox>("AvatarPhysicsDetailText");
	mCtrlAvatarPhysicsFactor->setCommitCallback(&LLPanelDisplay::updateSliderText);
	mCtrlAvatarPhysicsFactor->setCallbackUserData(mAvatarPhysicsFactorText);

	// Terrain detail slider
	mCtrlTerrainFactor = getChild<LLSliderCtrl>("TerrainMeshDetail");
	mTerrainFactorText = getChild<LLTextBox>("TerrainMeshDetailText");
	mCtrlTerrainFactor->setCommitCallback(&LLPanelDisplay::updateSliderText);
	mCtrlTerrainFactor->setCallbackUserData(mTerrainFactorText);

	// Terrain detail slider
	mCtrlSkyFactor = getChild<LLSliderCtrl>("SkyMeshDetail");
	mSkyFactorText = getChild<LLTextBox>("SkyMeshDetailText");
	mCtrlSkyFactor->setCommitCallback(&LLPanelDisplay::updateSliderText);
	mCtrlSkyFactor->setCallbackUserData(mSkyFactorText);

	// Particle detail slider
	mCtrlMaxParticle = getChild<LLSliderCtrl>("MaxParticleCount");

	// Glow detail slider
	mCtrlPostProcess = getChild<LLSliderCtrl>("RenderPostProcess");
	mPostProcessText = getChild<LLTextBox>("PostProcessText");
	mCtrlPostProcess->setCommitCallback(&LLPanelDisplay::updateSliderText);
	mCtrlPostProcess->setCallbackUserData(mPostProcessText);

	// Text boxes (for enabling/disabling)
	mShaderText = getChild<LLTextBox>("ShadersText");
	mReflectionText = getChild<LLTextBox>("ReflectionDetailText");
	mAvatarText = getChild<LLTextBox>("AvatarRenderingText");
	mTerrainText = getChild<LLTextBox>("TerrainDetailText");
	mLightingText = getChild<LLTextBox>("LightingDetailText");
	mMeshDetailText = getChild<LLTextBox>("MeshDetailText");
	mShadowDetailText = getChild<LLTextBox>("ShadowDetailText");

	refresh();

	return TRUE;
}

void LLPanelDisplay::initWindowSizeControls()
{
	// Window size
	mWindowSizeLabel = getChild<LLTextBox>("WindowSizeLabel");
	mCtrlWindowSize = getChild<LLComboBox>("windowsize combo");

	// Look to see if current window size matches existing window sizes, if so then
	// just set the selection value...
	const U32 height = gViewerWindow->getWindowDisplayHeight();
	const U32 width = gViewerWindow->getWindowDisplayWidth();
	for (S32 i=0; i < mCtrlWindowSize->getItemCount(); i++)
	{
		U32 height_test = 0;
		U32 width_test = 0;
		mCtrlWindowSize->setCurrentByIndex(i);
		if (extractWindowSizeFromString(mCtrlWindowSize->getValue().asString(), width_test, height_test))
		{
			if ((height_test == height) && (width_test == width))
			{
				return;
			}
		}
	}
	// ...otherwise, add a new entry with the current window height/width.
	LLUIString resolution_label = getString("resolution_format");
	resolution_label.setArg("[RES_X]", llformat("%d", width));
	resolution_label.setArg("[RES_Y]", llformat("%d", height));
	mCtrlWindowSize->add(resolution_label, ADD_TOP);
	mCtrlWindowSize->setCurrentByIndex(0);
}

LLPanelDisplay::~LLPanelDisplay()
{
	// clean up user data
	for (S32 i = 0; i < mCtrlAspectRatio->getItemCount(); i++)
	{
		mCtrlAspectRatio->setCurrentByIndex(i);
	}
	for (S32 i = 0; i < mCtrlWindowSize->getItemCount(); i++)
	{
		mCtrlWindowSize->setCurrentByIndex(i);
	}
}

void LLPanelDisplay::refresh()
{
	LLPanel::refresh();
	
	mFSAutoDetectAspect = gSavedSettings.getBOOL("FullScreenAutoDetectAspectRatio");

	mQualityPerformance = gSavedSettings.getU32("RenderQualityPerformance");
	mCustomSettings = gSavedSettings.getBOOL("RenderCustomSettings");

	// shader settings
	mBumpShiny = gSavedSettings.getBOOL("RenderObjectBump");
	mShaderEnable = gSavedSettings.getBOOL("VertexShaderEnable");
	mWindLight = gSavedSettings.getBOOL("WindLightUseAtmosShaders");
	mAvatarVP = gSavedSettings.getBOOL("RenderAvatarVP");
	mDeferred = gSavedSettings.getBOOL("RenderDeferred");
	mDeferredGI = gSavedSettings.getBOOL("RenderDeferredGI");

	// reflection radio
	mReflectionDetail = gSavedSettings.getS32("RenderReflectionDetail");

	mShadowDetail = gSavedSettings.getS32("RenderShadowDetail");

	// avatar settings
	mAvatarImpostors = gSavedSettings.getBOOL("RenderUseImpostors");
	mNonImpostors = gSavedSettings.getS32("RenderAvatarMaxVisible");
	mAvatarCloth = gSavedSettings.getBOOL("RenderAvatarCloth");

	// Draw distance
	mRenderFarClip = gSavedSettings.getF32("RenderFarClip");

	// sliders and their text boxes
	mPrimLOD = gSavedSettings.getF32("RenderVolumeLODFactor");
	mFlexLOD = gSavedSettings.getF32("RenderFlexTimeFactor");
	mTreeLOD = gSavedSettings.getF32("RenderTreeLODFactor");
	mAvatarLOD = gSavedSettings.getF32("RenderAvatarLODFactor");
	mTerrainLOD = gSavedSettings.getF32("RenderTerrainLODFactor");
	mSkyLOD = gSavedSettings.getU32("WLSkyDetail");
	mParticleCount = gSavedSettings.getS32("RenderMaxPartCount");
	mPostProcess = gSavedSettings.getS32("RenderGlowResolutionPow");
	
	// lighting and terrain radios
	mLocalLights = gSavedSettings.getBOOL("RenderLocalLights");
	mTerrainDetail =  gSavedSettings.getS32("RenderTerrainDetail");

	// slider text boxes
	updateSliderText(mCtrlLODFactor, mLODFactorText);
	updateSliderText(mCtrlFlexFactor, mFlexFactorText);
	updateSliderText(mCtrlTreeFactor, mTreeFactorText);
	updateSliderText(mCtrlAvatarFactor, mAvatarFactorText);
	updateSliderText(mCtrlAvatarPhysicsFactor, mAvatarPhysicsFactorText);
	updateSliderText(mCtrlTerrainFactor, mTerrainFactorText);
	updateSliderText(mCtrlPostProcess, mPostProcessText);
	updateSliderText(mCtrlSkyFactor, mSkyFactorText);

	refreshEnabledState();
}

void LLPanelDisplay::refreshEnabledState()
{
	// if in windowed mode, disable full screen options
	bool isFullScreen = !mCtrlWindowed->get();

	mDisplayResLabel->setVisible(isFullScreen);
	mCtrlFullScreen->setVisible(isFullScreen);
	mCtrlAspectRatio->setVisible(isFullScreen);
	mAspectRatioLabel1->setVisible(isFullScreen);
	mCtrlAutoDetectAspect->setVisible(isFullScreen);
	mFullScreenInfo->setVisible(!isFullScreen);
	mWindowSizeLabel->setVisible(!isFullScreen);

	// disable graphics settings and exit if it's not set to custom
	if(!gSavedSettings.getBOOL("RenderCustomSettings"))
	{
		setHiddenGraphicsState(true);
		return;
	}

	// otherwise turn them all on and selectively turn off others
	else
	{
		setHiddenGraphicsState(false);
	}

	// Reflections
	BOOL reflections = gSavedSettings.getBOOL("VertexShaderEnable") 
		&& gGLManager.mHasCubeMap 
		&& LLCubeMap::sUseCubeMaps;
	mCtrlReflectionDetail->setEnabled(reflections);
	
	// Bump & Shiny
	bool bumpshiny = gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps && LLFeatureManager::getInstance()->isFeatureAvailable("RenderObjectBump");
	mCtrlBumpShiny->setEnabled(bumpshiny ? TRUE : FALSE);

	// Avatar Mode
	S32 max_avatar_shader = LLViewerShaderMgr::instance()->mMaxAvatarShaderLevel;
	mCtrlAvatarVP->setEnabled((max_avatar_shader > 0) ? TRUE : FALSE);
	
	if (gSavedSettings.getBOOL("VertexShaderEnable") == FALSE || 
		gSavedSettings.getBOOL("RenderAvatarVP") == FALSE)
	{
		mCtrlAvatarCloth->setEnabled(false);
	} 
	else
	{
		mCtrlAvatarCloth->setEnabled(true);
	}

	//I actually recommend RenderUseFBO:FALSE for ati users when not using deferred, so RenderUseFBO shouldn't control visibility of the element.
	// Instead, gGLManager.mHasFramebufferObject seems better as it is determined by hardware and not current user settings. -Shyotl
	//Enabling deferred will force RenderUseFBO to TRUE.
	BOOL can_defer = gGLManager.mHasFramebufferObject &&
		LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") && //Ensure it's enabled in the gpu feature table
		LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP") && //Hardware Skinning. Deferred forces RenderAvatarVP to true
		LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable") && gSavedSettings.getBOOL("VertexShaderEnable") && //Basic Shaders
		LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders") && gSavedSettings.getBOOL("WindLightUseAtmosShaders"); //Atmospheric Shaders


	mCtrlDeferred->setEnabled(can_defer);
	mCtrlShadowDetail->setEnabled(can_defer && gSavedSettings.getBOOL("RenderDeferred"));
	//GI won't do anything with shadows off, but disabling it here is less than intuitive. Ignore shadow setting for now.
	mCtrlDeferredGI->setEnabled(mCtrlShadowDetail->getEnabled()/* && gSavedSettings.getS32("RenderShadowDetail") > 0*/); 

	// Disable max non-impostors slider if avatar impostors are off
	mCtrlNonImpostors->setEnabled(gSavedSettings.getBOOL("RenderUseImpostors"));

	// Vertex Shaders
//	mCtrlShaderEnable->setEnabled(LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable"));
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g) | Modified: RLVa-0.2.0a
	// "Basic Shaders" can't be disabled - but can be enabled - under @setenv=n
	bool fCtrlShaderEnable = LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable");
	mCtrlShaderEnable->setEnabled(fCtrlShaderEnable && (!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV) || !mShaderEnable));
// [/RLVa:KB]

	BOOL shaders = mCtrlShaderEnable->get();
	if (shaders)
	{
		mRadioTerrainDetail->setValue(1);
		mRadioTerrainDetail->setEnabled(FALSE);
	}
	else
	{
		mRadioTerrainDetail->setEnabled(TRUE);
	}

	// *HACK just checks to see if we can use shaders... 
	// maybe some cards that use shaders, but don't support windlight
//	mCtrlWindLight->setEnabled(mCtrlShaderEnable->getEnabled() && shaders);
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g) | Modified: RLVa-0.2.0a
	// "Atmospheric Shaders" can't be disabled - but can be enabled - under @setenv=n
	bool fCtrlWindLightEnable = fCtrlShaderEnable && shaders;
	mCtrlWindLight->setEnabled(fCtrlWindLightEnable && (!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV) || !mWindLight));
// [/RLVa:KB]

	// turn off sky detail if atmostpherics isn't on
	mCtrlSkyFactor->setEnabled(gSavedSettings.getBOOL("WindLightUseAtmosShaders"));
	mSkyFactorText->setEnabled(gSavedSettings.getBOOL("WindLightUseAtmosShaders"));

	// now turn off any features that are unavailable
	disableUnavailableSettings();
}

void LLPanelDisplay::disableUnavailableSettings()
{	
	// if vertex shaders off, disable all shader related products
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable"))
	{
		mCtrlShaderEnable->setEnabled(FALSE);
		mCtrlShaderEnable->setValue(FALSE);

		mCtrlWindLight->setEnabled(FALSE);
		mCtrlWindLight->setValue(FALSE);

		mCtrlReflectionDetail->setEnabled(FALSE);
		mCtrlReflectionDetail->setValue(FALSE);

		mCtrlAvatarVP->setEnabled(FALSE);
		mCtrlAvatarVP->setValue(FALSE);

		mCtrlAvatarCloth->setEnabled(FALSE);
		mCtrlAvatarCloth->setValue(FALSE);

		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
		mCtrlDeferredGI->setEnabled(FALSE);
		mCtrlDeferredGI->setValue(FALSE);
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(FALSE);
	}

	// disabled windlight
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders"))
	{
		mCtrlWindLight->setEnabled(FALSE);
		mCtrlWindLight->setValue(FALSE);
	}

	// disabled reflections
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderReflectionDetail"))
	{
		mCtrlReflectionDetail->setEnabled(FALSE);
		mCtrlReflectionDetail->setValue(FALSE);
	}

	// disabled av
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP"))
	{
		mCtrlAvatarVP->setEnabled(FALSE);
		mCtrlAvatarVP->setValue(FALSE);

		mCtrlAvatarCloth->setEnabled(FALSE);
		mCtrlAvatarCloth->setValue(FALSE);
	}
	// disabled cloth
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarCloth"))
	{
		mCtrlAvatarCloth->setEnabled(FALSE);
		mCtrlAvatarCloth->setValue(FALSE);
	}
	// disabled impostors
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderUseImpostors"))
	{
		mCtrlAvatarImpostors->setEnabled(FALSE);
		mCtrlAvatarImpostors->setValue(FALSE);
		mCtrlNonImpostors->setEnabled(FALSE);
	}
	// disabled deferred
	if(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred"))
	{
		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
		mCtrlDeferredGI->setEnabled(FALSE);
		mCtrlDeferredGI->setValue(FALSE);
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(FALSE);
	}

}

void LLPanelDisplay::setHiddenGraphicsState(bool isHidden)
{
	// quick check
	llassert(mGraphicsBorder != NULL);

	llassert(mCtrlDrawDistance != NULL);
	llassert(mCtrlLODFactor != NULL);
	llassert(mCtrlFlexFactor != NULL);
	llassert(mCtrlTreeFactor != NULL);
	llassert(mCtrlAvatarFactor != NULL);
	llassert(mCtrlTerrainFactor != NULL);
	llassert(mCtrlSkyFactor != NULL);
	llassert(mCtrlMaxParticle != NULL);
	llassert(mCtrlPostProcess != NULL);

	llassert(mLODFactorText != NULL);
	llassert(mFlexFactorText != NULL);
	llassert(mTreeFactorText != NULL);
	llassert(mAvatarFactorText != NULL);
	llassert(mTerrainFactorText != NULL);
	llassert(mSkyFactorText != NULL);
	llassert(mPostProcessText != NULL);

	llassert(mCtrlBumpShiny != NULL);
	llassert(mCtrlWindLight != NULL);
	llassert(mCtrlAvatarVP != NULL);
	llassert(mCtrlShaderEnable != NULL);
	llassert(mCtrlAvatarImpostors != NULL);
	llassert(mCtrlNonImpostors != NULL);
	llassert(mCtrlAvatarCloth != NULL);
	llassert(mRadioLightingDetail2 != NULL);

	llassert(mRadioTerrainDetail != NULL);
	llassert(mCtrlReflectionDetail != NULL);

	llassert(mMeshDetailText != NULL);
	llassert(mShaderText != NULL);
	llassert(mReflectionText != NULL);
	llassert(mAvatarText != NULL);
	llassert(mLightingText != NULL);
	llassert(mTerrainText != NULL);
	llassert(mDrawDistanceMeterText1 != NULL);
	llassert(mDrawDistanceMeterText2 != NULL);

	// enable/disable the states
	mGraphicsBorder->setVisible(!isHidden);
	/* 
	LLColor4 light(.45098f, .51765f, .6078f, 1.0f);
	LLColor4 dark(.10196f, .10196f, .10196f, 1.0f);
	b ? mGraphicsBorder->setColors(dark, light) : mGraphicsBorder->setColors(dark, dark);
	*/

	mCtrlDrawDistance->setVisible(!isHidden);
	mCtrlLODFactor->setVisible(!isHidden);	
	mCtrlFlexFactor->setVisible(!isHidden);	
	mCtrlTreeFactor->setVisible(!isHidden);	
	mCtrlAvatarFactor->setVisible(!isHidden);
	mCtrlAvatarPhysicsFactor->setVisible(!isHidden);
	mCtrlTerrainFactor->setVisible(!isHidden);
	mCtrlSkyFactor->setVisible(!isHidden);
	mCtrlMaxParticle->setVisible(!isHidden);
	mCtrlPostProcess->setVisible(!isHidden);

	mLODFactorText->setVisible(!isHidden);	
	mFlexFactorText->setVisible(!isHidden);	
	mTreeFactorText->setVisible(!isHidden);	
	mAvatarFactorText->setVisible(!isHidden);	
	mAvatarPhysicsFactorText->setVisible(!isHidden);
	mTerrainFactorText->setVisible(!isHidden);
	mSkyFactorText->setVisible(!isHidden);
	mPostProcessText->setVisible(!isHidden);

	mCtrlBumpShiny->setVisible(!isHidden);
	mCtrlWindLight->setVisible(!isHidden);
	mCtrlAvatarVP->setVisible(!isHidden);
	mCtrlShaderEnable->setVisible(!isHidden);
	mCtrlAvatarImpostors->setVisible(!isHidden);
	mCtrlNonImpostors->setVisible(!isHidden);
	mCtrlAvatarCloth->setVisible(!isHidden);
	mRadioLightingDetail2->setVisible(!isHidden);

	mRadioTerrainDetail->setVisible(!isHidden);
	mCtrlReflectionDetail->setVisible(!isHidden);

	mCtrlDeferred->setVisible(!isHidden);
	mCtrlDeferredGI->setVisible(!isHidden);
	mCtrlShadowDetail->setVisible(!isHidden);

	// text boxes
	mShaderText->setVisible(!isHidden);
	mReflectionText->setVisible(!isHidden);
	mAvatarText->setVisible(!isHidden);
	mLightingText->setVisible(!isHidden);
	mTerrainText->setVisible(!isHidden);
	mDrawDistanceMeterText1->setVisible(!isHidden);
	mDrawDistanceMeterText2->setVisible(!isHidden);
	mShadowDetailText->setVisible(!isHidden);

	// hide one meter text if we're making things visible
	if(!isHidden)
	{
		updateMeterText(mCtrlDrawDistance, this);
	}

	mMeshDetailText->setVisible(!isHidden);
}

void LLPanelDisplay::cancel()
{
	gSavedSettings.setBOOL("FullScreenAutoDetectAspectRatio", mFSAutoDetectAspect);
	gSavedSettings.setF32("FullScreenAspectRatio", mAspectRatio);

	gSavedSettings.setU32("RenderQualityPerformance", mQualityPerformance);

	gSavedSettings.setBOOL("RenderCustomSettings", mCustomSettings);

	gSavedSettings.setBOOL("RenderObjectBump", mBumpShiny);
	gSavedSettings.setBOOL("VertexShaderEnable", mShaderEnable);
	gSavedSettings.setBOOL("WindLightUseAtmosShaders", mWindLight);

	gSavedSettings.setBOOL("RenderAvatarVP", mAvatarVP);
	gSavedSettings.setBOOL("RenderDeferred", mDeferred);
	gSavedSettings.setBOOL("RenderDeferredGI", mDeferredGI);

	gSavedSettings.setS32("RenderReflectionDetail", mReflectionDetail);
	gSavedSettings.setS32("RenderShadowDetail", mShadowDetail);

	gSavedSettings.setBOOL("RenderUseImpostors", mAvatarImpostors);
	gSavedSettings.setS32("RenderAvatarMaxVisible", mNonImpostors);
	gSavedSettings.setBOOL("RenderAvatarCloth", mAvatarCloth);

	gSavedSettings.setBOOL("RenderLocalLights", mLocalLights);
	gSavedSettings.setS32("RenderTerrainDetail", mTerrainDetail);

	gSavedSettings.setF32("RenderFarClip", mRenderFarClip);
	gSavedSettings.setF32("RenderVolumeLODFactor", mPrimLOD);
	gSavedSettings.setF32("RenderFlexTimeFactor", mFlexLOD);
	gSavedSettings.setF32("RenderTreeLODFactor", mTreeLOD);
	gSavedSettings.setF32("RenderAvatarLODFactor", mAvatarLOD);
	gSavedSettings.setF32("RenderTerrainLODFactor", mTerrainLOD);
	gSavedSettings.setU32("WLSkyDetail", mSkyLOD);
	gSavedSettings.setS32("RenderMaxPartCount", mParticleCount);
	gSavedSettings.setS32("RenderGlowResolutionPow", mPostProcess);
}

void LLPanelDisplay::apply()
{
	applyResolution();
	
	// Only set window size if we're not in fullscreen mode
	if (mCtrlWindowed->get())
	{
		applyWindowSize();
	}
}

void LLPanelDisplay::onChangeQuality(LLUICtrl *ctrl, void *data)
{
	LLSliderCtrl* sldr = static_cast<LLSliderCtrl*>(ctrl);
	LLPanelDisplay* cur_panel = static_cast<LLPanelDisplay*>(data);

	if(sldr == NULL || cur_panel == NULL)
	{
		return;
	}

	U32 set = (U32)sldr->getValueF32();
	LLFeatureManager::getInstance()->setGraphicsLevel(set, true);
	
	LLFloaterPreference::refreshEnabledGraphics();
	cur_panel->refresh();
}

void LLPanelDisplay::onChangeCustom(LLUICtrl *ctrl, void *data)
{
	LLFloaterPreference::refreshEnabledGraphics();
}

void LLPanelDisplay::onOpenHelp(void* user_data)
{
	LLPanelDisplay* self = static_cast<LLPanelDisplay*>(user_data);

	const char* xml_alert = "GraphicsPreferencesHelp";
	LLFloater* parent_floater = gFloaterView->getParentFloater(self);
	LLNotifications::instance().add(parent_floater->contextualNotification(xml_alert));
}

void LLPanelDisplay::onOpenHardwareSettings(void* user_data)
{
	LLFloaterHardwareSettings::show();
}

void LLPanelDisplay::onApplyResolution(LLUICtrl* src, void* user_data)
{
	((LLPanelDisplay*) src)->applyResolution();
}

void LLPanelDisplay::applyResolution()
{

	gGL.flush();
	char aspect_ratio_text[ASPECT_RATIO_STR_LEN];		/*Flawfinder: ignore*/
	if (mCtrlAspectRatio->getCurrentIndex() == -1)
	{
		// *Can't pass const char* from c_str() into strtok
		strncpy(aspect_ratio_text, mCtrlAspectRatio->getSimple().c_str(), sizeof(aspect_ratio_text) -1);	/*Flawfinder: ignore*/
		aspect_ratio_text[sizeof(aspect_ratio_text) -1] = '\0';
		char *element = strtok(aspect_ratio_text, ":/\\");
		if (!element)
		{
			mAspectRatio = 0.f; // will be clamped later
		}
		else
		{
			LLLocale locale(LLLocale::USER_LOCALE);
			mAspectRatio = (F32)atof(element);
		}

		// look for denominator
		element = strtok(NULL, ":/\\");
		if (element)
		{
			LLLocale locale(LLLocale::USER_LOCALE);

			F32 denominator = (F32)atof(element);
			if (denominator != 0.f)
			{
				mAspectRatio /= denominator;
			}
		}
	}
	else
	{
		mAspectRatio = (F32)mCtrlAspectRatio->getValue().asReal();
	}
	
	// presumably, user entered a non-numeric value if aspect_ratio == 0.f
	if (mAspectRatio != 0.f)
	{
		mAspectRatio = llclamp(mAspectRatio, 0.2f, 5.f);
		gSavedSettings.setF32("FullScreenAspectRatio", mAspectRatio);
	}
	
	// Screen resolution
	S32 num_resolutions;
	LLWindow::LLWindowResolution* supported_resolutions = 
		gViewerWindow->getWindow()->getSupportedResolutions(num_resolutions);
	U32 resIndex = mCtrlFullScreen->getCurrentIndex();
	gSavedSettings.setS32("FullScreenWidth", supported_resolutions[resIndex].mWidth);
	gSavedSettings.setS32("FullScreenHeight", supported_resolutions[resIndex].mHeight);

	gViewerWindow->requestResolutionUpdate(!mCtrlWindowed->get());

	send_agent_update(TRUE);

	// Update enable/disable
	refresh();
}

// Extract from strings of the form "<width> x <height>", e.g. "640 x 480".
bool LLPanelDisplay::extractWindowSizeFromString(const std::string& instr, U32 &width, U32 &height)
{
	using namespace boost;
	cmatch what;
	const regex expression("([0-9]+) x ([0-9]+)");
	if (regex_match(instr.c_str(), what, expression))
	{
		width = atoi(what[1].first);
		height = atoi(what[2].first);
		return true;
	}
	width = height = 0;
	return false;
}

void LLPanelDisplay::applyWindowSize()
{
	if (mCtrlWindowSize->getVisible() && (mCtrlWindowSize->getCurrentIndex() != -1))
	{
		U32 width = 0;
		U32 height = 0;
		if (extractWindowSizeFromString(mCtrlWindowSize->getValue().asString().c_str(), width,height))
		{
			LLViewerWindow::movieSize(width, height);
		}
	}
}

//static 
void LLPanelDisplay::onCommitWindowedMode(LLUICtrl* ctrl, void *data)
{
	LLPanelDisplay *panel = (LLPanelDisplay*)data;

	panel->refresh();
}

//static
void LLPanelDisplay::onCommitAutoDetectAspect(LLUICtrl *ctrl, void *data)
{
	LLPanelDisplay *panel = (LLPanelDisplay*)data;

	BOOL auto_detect = ((LLCheckBoxCtrl*)ctrl)->get();
	F32 ratio;

	if (auto_detect)
	{
		S32 numerator = 0;
		S32 denominator = 0;
		
		// clear any aspect ratio override
		gViewerWindow->mWindow->setNativeAspectRatio(0.f);
		fractionFromDecimal(gViewerWindow->mWindow->getNativeAspectRatio(), numerator, denominator);

		std::string aspect;
		if (numerator != 0)
		{
			aspect = llformat("%d:%d", numerator, denominator);
		}
		else
		{
			aspect = llformat("%.3f", gViewerWindow->mWindow->getNativeAspectRatio());
		}

		panel->mCtrlAspectRatio->setLabel(aspect);

		ratio = gViewerWindow->mWindow->getNativeAspectRatio();
		gSavedSettings.setF32("FullScreenAspectRatio", ratio);
	}
}

//static 
void LLPanelDisplay::onKeystrokeAspectRatio(LLLineEditor* caller, void* user_data)
{
	LLPanelDisplay* panel = (LLPanelDisplay*)user_data;

	panel->mCtrlAutoDetectAspect->set(FALSE);
}

//static
void LLPanelDisplay::onSelectAspectRatio(LLUICtrl*, void* user_data)
{
	LLPanelDisplay* panel = (LLPanelDisplay*)user_data;

	panel->mCtrlAutoDetectAspect->set(FALSE);
}

//static
void LLPanelDisplay::fractionFromDecimal(F32 decimal_val, S32& numerator, S32& denominator)
{
	numerator = 0;
	denominator = 0;
	for (F32 test_denominator = 1.f; test_denominator < 30.f; test_denominator += 1.f)
	{
		if (fmodf((decimal_val * test_denominator) + 0.01f, 1.f) < 0.02f)
		{
			numerator = llround(decimal_val * test_denominator);
			denominator = llround(test_denominator);
			break;
		}
	}
}

//static
void LLPanelDisplay::onVertexShaderEnable(LLUICtrl* self, void* data)
{
	LLFloaterPreference::refreshEnabledGraphics();
}

void LLPanelDisplay::setHardwareDefaults(void* user_data)
{
	LLFeatureManager::getInstance()->applyRecommendedSettings();
	LLControlVariable* controlp = gSavedSettings.getControl("RenderAvatarMaxVisible");
	if (controlp)
	{
		controlp->resetToDefault(true);
	}
	LLFloaterPreference::refreshEnabledGraphics();
}

void LLPanelDisplay::updateSliderText(LLUICtrl* ctrl, void* user_data)
{
	// get our UI widgets
	LLTextBox* text_box = (LLTextBox*)user_data;
	LLSliderCtrl* slider = (LLSliderCtrl*) ctrl;
	if(text_box == NULL || slider == NULL)
	{
		return;
	}

	//Hack to display 'Off' for avatar physics slider.
	if(slider->getName() == "AvatarPhysicsDetail" && !slider->getValueF32())
	{
		text_box->setText(std::string("Off"));
		return;
	}

	// get range and points when text should change
	F32 range = slider->getMaxValue() - slider->getMinValue();
	llassert(range > 0);
	F32 midPoint = slider->getMinValue() + range / 3.0f;
	F32 highPoint = slider->getMinValue() + (2.0f * range / 3.0f);

	// choose the right text
	if(slider->getValueF32() < midPoint)
	{
		text_box->setText(std::string("Low"));
	} 
	else if (slider->getValueF32() < highPoint)
	{
		text_box->setText(std::string("Mid"));
	}
	else if(slider->getValueF32() < slider->getMaxValue())
	{
		text_box->setText(std::string("High"));
	}
	else
	{
		text_box->setText(std::string("Max"));
	}
}

void LLPanelDisplay::updateMeterText(LLUICtrl* ctrl, void* user_data)
{
	// get our UI widgets
	LLPanelDisplay* panel = (LLPanelDisplay*)user_data;
	LLSliderCtrl* slider = (LLSliderCtrl*) ctrl;

	LLTextBox* m1 = panel->getChild<LLTextBox>("DrawDistanceMeterText1");
	LLTextBox* m2 = panel->getChild<LLTextBox>("DrawDistanceMeterText2");

	// toggle the two text boxes based on whether we have 1 or two digits
	F32 val = slider->getValueF32();
	bool two_digits = val < 100;
	m1->setVisible(two_digits);
	m2->setVisible(!two_digits);
}



