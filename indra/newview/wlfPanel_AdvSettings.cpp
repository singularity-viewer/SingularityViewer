/* Copyright (c) 2009
 *
 * Modular Systems All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS “AS IS”
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "llviewerprecompiledheaders.h"

#include "wlfPanel_AdvSettings.h"

#include "llbutton.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "lliconctrl.h"
#include "lloverlaybar.h"
#include "lltextbox.h"
#include "llcombobox.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llstartup.h"


BOOL firstBuildDone;
void* fixPointer;
std::string ButtonState;
std::string current_preset = "Default";

wlfPanel_AdvSettings::wlfPanel_AdvSettings()
{
	setIsChrome(TRUE);
	setFocusRoot(TRUE);
	build();
}
void wlfPanel_AdvSettings::build()
{
	deleteAllChildren();
	if (!gSavedSettings.getBOOL("wlfAdvSettingsPopup"))
	{
		LLUICtrlFactory::getInstance()->buildPanel(this, "wlfPanel_AdvSettings_expanded.xml", &getFactoryMap());
		ButtonState = "arrow_up.tga";
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildPanel(this, "wlfPanel_AdvSettings.xml", &getFactoryMap());
		ButtonState = "arrow_down.tga";
	}
}

void wlfPanel_AdvSettings::refresh()
{
	if (gSavedSettings.getBOOL("wlfAdvSettingsPopup"))
	{
		childSetEnabled("WLSkyPresetsCombo", true);
		childSetEnabled("WLWaterPresetsCombo", true);
	}
}

void wlfPanel_AdvSettings::fixPanel()
{
	if(!firstBuildDone)
	{
		llinfos << "firstbuild done" << llendl;
		firstBuildDone = TRUE;
		onClickExpandBtn(fixPointer);
	}
}
BOOL wlfPanel_AdvSettings::postBuild()
{
	childSetAction("expand", onClickExpandBtn, this);
	
	LLComboBox* comboBoxSky = getChild<LLComboBox>("WLSkyPresetsCombo");
	if(comboBoxSky != NULL) 
	{
		std::map<std::string, LLWLParamSet>::const_iterator mIt = LLWLParamManager::getInstance()->getPresets().begin();
		for(; mIt != LLWLParamManager::getInstance()->getPresets().end(); mIt++) 
		{
			if (mIt->first.length() > 0)
				comboBoxSky->add(mIt->first);
		}
		comboBoxSky->add(LLStringUtil::null);
		comboBoxSky->selectByValue(LLSD(current_preset));
	}
	comboBoxSky->setCommitCallback(onChangePresetName);

	LLComboBox* comboBoxWater = getChild<LLComboBox>("WLWaterPresetsCombo");
	if(comboBoxWater != NULL) 
	{
		std::map<std::string, LLWaterParamSet>::const_iterator mIt = LLWaterParamManager::getInstance()->getPresets().begin();
		for(; mIt != LLWaterParamManager::getInstance()->getPresets().end(); mIt++) 
		{
			if (mIt->first.length() > 0)
				comboBoxWater->add(mIt->first);
		}
		comboBoxWater->add(LLStringUtil::null);
		comboBoxWater->selectByValue(LLSD(current_preset));
	}
	comboBoxWater->setCommitCallback(onChangePresetName);
	fixPointer = this;
	/*onClickExpandBtn(fixPointer);
	onClickExpandBtn(fixPointer);*/
	return TRUE;
}
void wlfPanel_AdvSettings::draw()
{
	LLButton* expand_button = getChild<LLButton>("expand");
	/*if (expand_button)
	{
		if (expand_button->getToggleState())
		{
			expand_button->setImageOverlay("arrow_down.tga");
		}
		else
		{
			expand_button->setImageOverlay("arrow_up.tga");
		}
	}*/
	expand_button->setImageOverlay(ButtonState);
	refresh();
	LLPanel::draw();
}
wlfPanel_AdvSettings::~wlfPanel_AdvSettings ()
{
}
void wlfPanel_AdvSettings::onClickExpandBtn(void* user_data)
{
	gSavedSettings.setBOOL("wlfAdvSettingsPopup",!gSavedSettings.getBOOL("wlfAdvSettingsPopup"));
	wlfPanel_AdvSettings* remotep = (wlfPanel_AdvSettings*)user_data;
	remotep->build();
	gOverlayBar->layoutButtons();
}
void wlfPanel_AdvSettings::onChangePresetName(LLUICtrl* ctrl, void * userData)
{
	LLWLParamManager::getInstance()->mAnimator.mIsRunning = false;
	LLWLParamManager::getInstance()->mAnimator.mUseLindenTime = false;

	LLComboBox * combo_box = static_cast<LLComboBox*>(ctrl);
	llinfos << "Combobox is " << combo_box->getControlName() << " aka " << combo_box->getName() << llendl;
	if (combo_box->getName() == "WLSkyPresetsCombo")
	{
		if(combo_box->getSimple() == "")
		{
			return;
		}
		current_preset = combo_box->getSelectedValue().asString();
		LLWLParamManager::getInstance()->loadPreset(current_preset);
	}
	else if (combo_box->getName() == "WLWaterPresetsCombo")
	{
		if(combo_box->getSimple() == "")
		{
			return;
		}
		current_preset = combo_box->getSelectedValue().asString();
		LLWaterParamManager::getInstance()->loadPreset(current_preset);
	}
}
