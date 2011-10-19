/** 
 * @file llpaneldisplay.h
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

#ifndef LL_LLPANELDISPLAY_H
#define LL_LLPANELDISPLAY_H

#include "llpanel.h"
#include "llwindow.h"
#include "llcontrol.h"

class LLSlider;
class LLSpinCtrl;
class LLCheckBoxCtrl;
class LLRadioGroup;
class LLComboBox;
class LLLineEditor;
class LLSliderCtrl;
class LLTextBox;
class LLTextEditor;

typedef enum
{
	GS_LOW_GRAPHICS,
	GS_MID_GRAPHICS,
	GS_HIGH_GRAPHICS,
	GS_ULTRA_GRAPHICS

} EGraphicsSettings;

class LLPanelDisplay 
: public LLPanel
{
	friend class LLPreferenceCore;
	
public:
	LLPanelDisplay();
	virtual ~LLPanelDisplay();

	virtual BOOL postBuild();

	void refresh();	// Refresh enable/disable
	void refreshEnabledState();
	void disableUnavailableSettings();
	void setHiddenGraphicsState(bool isHidden);
	void apply();	// Apply the changed values.
	void applyResolution();
	void applyWindowSize();
	void cancel();
	
protected:
	void initWindowSizeControls();
	bool extractWindowSizeFromString(const std::string& instr, U32 &width, U32 &height);
	
	// aspect ratio sliders and boxes
	LLComboBox		*mCtrlFullScreen;               // Fullscreen resolution
	LLCheckBoxCtrl	*mCtrlAutoDetectAspect;			// automatically detect aspect ratio
	LLComboBox		*mCtrlAspectRatio;				// user provided aspect ratio
	LLComboBox		*mCtrlReflectionDetail;

	LLCheckBoxCtrl	*mCtrlWindowed;					// windowed mode
	LLComboBox      *mCtrlWindowSize;               // window size for windowed mode

	/// performance radio group
	LLSliderCtrl	*mCtrlSliderQuality;
	LLCheckBoxCtrl	*mCtrlCustomSettings;

	// performance sliders and boxes
	LLViewBorder	*mGraphicsBorder;

	LLSliderCtrl	*mCtrlDrawDistance;				// the draw distance slider
	LLSliderCtrl	*mCtrlLODFactor;				// LOD for volume objects
	LLSliderCtrl	*mCtrlFlexFactor;				// Timeslice for flexible objects
	LLSliderCtrl	*mCtrlTreeFactor;				// Control tree cutoff distance
	LLSliderCtrl	*mCtrlAvatarFactor;				// LOD for avatars
	LLSliderCtrl	*mCtrlAvatarPhysicsFactor;		// Physics LOD for avatars
	LLSliderCtrl	*mCtrlTerrainFactor;			// LOD for terrain
	LLSliderCtrl	*mCtrlSkyFactor;				// LOD for terrain
	LLSliderCtrl	*mCtrlMaxParticle;				// Max Particle
	LLSliderCtrl	*mCtrlPostProcess;				// Max Particle
	LLSliderCtrl	*mCtrlNonImpostors;				// Max non-impostors

	LLCheckBoxCtrl	*mCtrlBumpShiny;
	LLCheckBoxCtrl	*mCtrlWindLight;
	LLCheckBoxCtrl	*mCtrlAvatarVP;
	LLCheckBoxCtrl	*mCtrlShaderEnable;
	LLCheckBoxCtrl	*mCtrlDeferred;
	LLCheckBoxCtrl	*mCtrlDeferredGI;
	LLComboBox		*mCtrlShadowDetail;
	LLCheckBoxCtrl	*mCtrlAvatarImpostors;
	LLCheckBoxCtrl	*mCtrlAvatarCloth;
	LLRadioGroup	*mRadioLightingDetail2;
	LLRadioGroup	*mRadioTerrainDetail;

	LLTextBox		*mAspectRatioLabel1;
	LLTextBox		*mDisplayResLabel;
	LLTextEditor	*mFullScreenInfo;
	LLTextBox       *mWindowSizeLabel;
	
	LLTextBox		*mShaderText;
	LLTextBox		*mReflectionText;
	LLTextBox		*mAvatarText;
	LLTextBox		*mTerrainText;
	LLTextBox		*mLightingText;
	LLTextBox		*mDrawDistanceMeterText1;
	LLTextBox		*mDrawDistanceMeterText2;

	LLTextBox		*mMeshDetailText;
	LLTextBox		*mLODFactorText;
	LLTextBox		*mFlexFactorText;
	LLTextBox		*mTreeFactorText;
	LLTextBox		*mAvatarFactorText;
	LLTextBox		*mAvatarPhysicsFactorText;
	LLTextBox		*mTerrainFactorText;
	LLTextBox		*mSkyFactorText;
	LLTextBox		*mPostProcessText;
	LLTextBox		*mShadowDetailText;

	BOOL mFSAutoDetectAspect;
	F32 mAspectRatio;

	// performance value holders for cancel

	S32 mQualityPerformance;
	BOOL mCustomSettings;

	BOOL mBumpShiny;
	BOOL mShaderEnable;
	BOOL mWindLight;
	BOOL mDeferred;
	BOOL mDeferredGI;
	BOOL mAvatarVP;

	S32 mReflectionDetail;
	S32 mShadowDetail;

	BOOL mAvatarImpostors;
	S32 mNonImpostors;
	BOOL mAvatarCloth;
	S32 mAvatarMode;
	BOOL mLocalLights;
	S32 mTerrainDetail;

	F32 mRenderFarClip;
	F32 mPrimLOD;
	F32 mFlexLOD;
	F32 mTreeLOD;
	F32 mAvatarLOD;
	F32 mTerrainLOD;
	S32 mSkyLOD;
	S32 mParticleCount;
	S32 mPostProcess;

	static void setGraphicsSettings(LLControlGroup& group);
	static void createGroup();

	// if the quality radio buttons are changed
	static void onChangeQuality(LLUICtrl *ctrl, void *data);
	
	// if the custom settings box is clicked
	static void onChangeCustom(LLUICtrl *ctrl, void *data);
	
	static void onOpenHelp(void *data);
	static void onOpenHardwareSettings(void *data);
	static void onCommitAutoDetectAspect(LLUICtrl *ctrl, void *data);
	static void onKeystrokeAspectRatio(LLLineEditor* caller, void* user_data);
	static void onSelectAspectRatio(LLUICtrl*, void*);
	static void onCommitWindowedMode(LLUICtrl* ctrl, void *data);
	static void onApplyResolution(LLUICtrl* ctrl, void* data);
	static void updateSliderText(LLUICtrl* ctrl, void* user_data);
	static void updateMeterText(LLUICtrl* ctrl, void* user_data);

	/// callback for defaults
	static void setHardwareDefaults(void *data);

	// callback for when client turns on shaders
	static void onVertexShaderEnable(LLUICtrl*, void*);

	// helper function
	static void fractionFromDecimal(F32 decimal_val, S32& numerator, S32& denominator);
};

const S32 LL_MAX_VRAM_INDEX = 6;

#endif
