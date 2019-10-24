/** 
 * @file llfloatersettingsdebug.cpp
 * @brief floater for debugging internal viewer settings
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

#include "llfloatersettingsdebug.h"

#include "llcolorswatch.h"
#include "llfloater.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llsdserialize.h"
#include "llspinctrl.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llwindow.h"

LLFloaterSettingsDebug::LLFloaterSettingsDebug()
:	LLFloater(std::string("Configuration Editor"))
,	mOldSearchTerm("---")
,	mCurrentControlVariable(nullptr)
,	mSettingsScrollList(nullptr)
,	mValSpinner1(nullptr)
,	mValSpinner2(nullptr)
,	mValSpinner3(nullptr)
,	mValSpinner4(nullptr)
,	mValColor(nullptr)
,	mValBool(nullptr)
,	mValText(nullptr)
,	mComment(nullptr)
,	mBtnCopy(nullptr)
,	mBtnDefault(nullptr)
{
	mCommitCallbackRegistrar.add("SettingSelect",	boost::bind(&LLFloaterSettingsDebug::onSettingSelect, this));
	mCommitCallbackRegistrar.add("CommitSettings",	boost::bind(&LLFloaterSettingsDebug::onCommitSettings, this));
	mCommitCallbackRegistrar.add("ClickDefault",	boost::bind(&LLFloaterSettingsDebug::onClickDefault, this));
	mCommitCallbackRegistrar.add("UpdateFilter",	boost::bind(&LLFloaterSettingsDebug::onUpdateFilter, this, _2));
	mCommitCallbackRegistrar.add("ClickCopy",		boost::bind(&LLFloaterSettingsDebug::onCopyToClipboard, this));

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_settings_debug.xml");
}

LLFloaterSettingsDebug::~LLFloaterSettingsDebug()
{
	if (mCurrentControlVariable)
		mCurrentControlVariable->getCommitSignal()->disconnect(boost::bind(&LLFloaterSettingsDebug::updateControl, this));
}

BOOL LLFloaterSettingsDebug::postBuild()
{
	mSettingsScrollList = getChild<LLScrollListCtrl>("settings_scroll_list");

	struct f final : public LLControlGroup::ApplyFunctor
	{
		settings_map_t* map;
		f(settings_map_t* m) : map(m) {}

		void apply(const std::string& name, LLControlVariable* control) override final
		{
			if (!control->isHiddenFromSettingsEditor())
			{
				(*map)[name]=control;
			}
		}
	} func(&mSettingsMap);

	gSavedSettings.applyToAll(&func);
	gSavedPerAccountSettings.applyToAll(&func);
	gColors.applyToAll(&func);

	// Populate the list
	for(const auto& pair : mSettingsMap)
		addRow(pair.second);

	mSettingsScrollList->sortByColumnIndex(0, true);
	mComment = getChild<LLTextEditor>("comment_text");
	mValSpinner1 = getChild<LLSpinCtrl>("val_spinner_1");
	mValSpinner2 = getChild<LLSpinCtrl>("val_spinner_2");
	mValSpinner3 = getChild<LLSpinCtrl>("val_spinner_3");
	mValSpinner4 = getChild<LLSpinCtrl>("val_spinner_4");
	mValColor = getChild<LLColorSwatchCtrl>("val_color_swatch");
	mValBool = getChild<LLUICtrl>("boolean_combo");
	mValText = getChild<LLUICtrl>("val_text");
	mBtnCopy = getChildView("copy_btn");
	mBtnDefault = getChildView("default_btn");

	LL_INFOS() << mSettingsScrollList->getItemCount() << " total debug settings displayed." << LL_ENDL;

	return TRUE;
}

void LLFloaterSettingsDebug::draw()
{
	// check for changes in control visibility, like RLVa does
	if (mCurrentControlVariable && mCurrentControlVariable->isHiddenFromSettingsEditor() != mOldVisibility)
		updateControl();

	LLFloater::draw();
}

LLControlVariable* LLFloaterSettingsDebug::getControlVariable()
{
	LLScrollListItem* item = mSettingsScrollList->getFirstSelected();
	if (!item) return nullptr;

	LLControlVariable* controlp = static_cast<LLControlVariable*>(item->getUserdata());

	return controlp ? controlp->getCOAActive() : nullptr;
}

void LLFloaterSettingsDebug::addRow(LLControlVariable* control, const std::string& searchTerm)
{
	const auto& name = control->getName();
	const auto& comment = control->getComment();

	if (!searchTerm.empty())
	{
		std::string itemValue = control->getName();
		LLStringUtil::toLower(itemValue);
		if (itemValue.find(searchTerm, 0) == std::string::npos)
		{
			std::string itemComment = control->getComment();
			LLStringUtil::toLower(itemComment);
			if (itemComment.find(searchTerm, 0) == std::string::npos)
				return;
		}
	}

	auto params = LLScrollListItem::Params();
	params.userdata(control)
		.columns.add(LLScrollListCell::Params()
			.value(name)
			.tool_tip(comment));
	mSettingsScrollList->addRow(params);
}

void LLFloaterSettingsDebug::onSettingSelect()
{
	auto new_control = getControlVariable();

	if (new_control == mCurrentControlVariable) return;

	// unbind change control signal from previously selected control
	if (mCurrentControlVariable)
		mCurrentControlVariable->getCommitSignal()->disconnect(boost::bind(&LLFloaterSettingsDebug::updateControl, this));

	mCurrentControlVariable = new_control;

	// bind change control signal, so we can see updates to the current control in realtime
	if (mCurrentControlVariable)
		mCurrentControlVariable->getCommitSignal()->connect(boost::bind(&LLFloaterSettingsDebug::updateControl, this));

	updateControl();
}

void LLFloaterSettingsDebug::onCommitSettings()
{
	LLControlVariable* controlp = mCurrentControlVariable;

	if (!controlp)
	{
		return;
	}

	LLVector3 vector;
	LLVector3d vectord;
	LLRect rect;
	LLColor4 col4;
	LLColor3 col3;
	LLColor4U col4U;
	LLColor4 color_with_alpha;

	switch(controlp->type())
	{		
	  case TYPE_U32:
		controlp->set(mValSpinner1->getValue());
		break;
	  case TYPE_S32:
		controlp->set(mValSpinner1->getValue());
		break;
	  case TYPE_F32:
		controlp->set(LLSD(mValSpinner1->getValue().asReal()));
		break;
	  case TYPE_BOOLEAN:
		controlp->set(mValBool->getValue());
		break;
	  case TYPE_STRING:
		controlp->set(mValText->getValue());
		break;
	  case TYPE_VEC3:
		vector.mV[VX] = (F32)mValSpinner1->getValue().asReal();
		vector.mV[VY] = (F32)mValSpinner2->getValue().asReal();
		vector.mV[VZ] = (F32)mValSpinner3->getValue().asReal();
		controlp->set(vector.getValue());
		break;
	  case TYPE_VEC3D:
		vectord.mdV[VX] = mValSpinner1->getValue().asReal();
		vectord.mdV[VY] = mValSpinner2->getValue().asReal();
		vectord.mdV[VZ] = mValSpinner3->getValue().asReal();
		controlp->set(vectord.getValue());
		break;
	  case TYPE_RECT:
		rect.mLeft = mValSpinner1->getValue().asInteger();
		rect.mRight = mValSpinner2->getValue().asInteger();
		rect.mBottom = mValSpinner3->getValue().asInteger();
		rect.mTop = mValSpinner4->getValue().asInteger();
		controlp->set(rect.getValue());
		break;
	  case TYPE_COL4:
		col3.setValue(mValColor->getValue());
		col4 = LLColor4(col3, (F32)mValSpinner4->getValue().asReal());
		controlp->set(col4.getValue());
		break;
	  case TYPE_COL3:
		controlp->set(mValColor->getValue());
		//col3.mV[VRED] = (F32)mValSpinner1->getValue().asReal();
		//col3.mV[VGREEN] = (F32)mValSpinner2->getValue().asReal();
		//col3.mV[VBLUE] = (F32)mValSpinner3->getValue().asReal();
		//mCurrentControlVariable->set(col3.getValue());
		break;
	  case TYPE_COL4U:
		col3.setValue(mValColor->getValue());
		col4U.setVecScaleClamp(col3);
		col4U.mV[VALPHA] = mValSpinner4->getValue().asInteger();
		mCurrentControlVariable->set(col4U.getValue());
		break;
	  case TYPE_LLSD:
	  {
		const auto& val = mValText->getValue().asString();
		LLSD sd;
		if (!val.empty())
		{
			std::istringstream str(val);
			if (LLSDSerialize::fromXML(sd, str) == LLSDParser::PARSE_FAILURE)
				break;
		}
		mCurrentControlVariable->set(sd);
		break;
	  }
	  default:
		break;
	}
}

void LLFloaterSettingsDebug::onClickDefault()
{
	if (mCurrentControlVariable)
	{
		mCurrentControlVariable->resetToDefault(true);
		updateControl();
	}
}

void LLFloaterSettingsDebug::onCopyToClipboard()
{
	if (mCurrentControlVariable)
	{
		getWindow()->copyTextToClipboard(utf8str_to_wstring(mCurrentControlVariable->getName()));
	}
}

// we've switched controls, or doing per-frame update, so update spinners, etc.
void LLFloaterSettingsDebug::updateControl()
{
	if (!mValSpinner1 || !mValSpinner2 || !mValSpinner3 || !mValSpinner4 || !mValColor)
	{
		LL_WARNS() << "Could not find all desired controls by name"
			<< LL_ENDL;
		return;
	}

	mValSpinner1->setVisible(FALSE);
	mValSpinner2->setVisible(FALSE);
	mValSpinner3->setVisible(FALSE);
	mValSpinner4->setVisible(FALSE);
	mValColor->setVisible(FALSE);
	mValText->setVisible(FALSE);
	if (!mComment->hasFocus()) // <alchemy/>
	{
		mComment->setText(LLStringUtil::null);
	}
	mValBool->setVisible(false);
	mBtnCopy->setEnabled(false);
	mBtnDefault->setEnabled(false);

	if (mCurrentControlVariable)
	{
// [RLVa:KB] - Checked: 2011-05-28 (RLVa-1.4.0a) | Modified: RLVa-1.4.0a
		// If "HideFromEditor" was toggled while the floater is open then we need to manually disable access to the control
		mOldVisibility = mCurrentControlVariable->isHiddenFromSettingsEditor();
		mValSpinner1->setEnabled(!mOldVisibility);
		mValSpinner2->setEnabled(!mOldVisibility);
		mValSpinner3->setEnabled(!mOldVisibility);
		mValSpinner4->setEnabled(!mOldVisibility);
		mValColor->setEnabled(!mOldVisibility);
		mValText->setEnabled(!mOldVisibility);
		mValBool->setEnabled(!mOldVisibility);
		mBtnDefault->setEnabled(!mOldVisibility);
// [/RLVa:KB]

		mBtnCopy->setEnabled(true);

		eControlType type = mCurrentControlVariable->type();
		if (!mComment->hasFocus()) // <alchemy/>
		{
			mComment->setText(mCurrentControlVariable->getName() + std::string(": ") + mCurrentControlVariable->getComment());
		}
		mValSpinner1->setMaxValue(F32_MAX);
		mValSpinner2->setMaxValue(F32_MAX);
		mValSpinner3->setMaxValue(F32_MAX);
		mValSpinner4->setMaxValue(F32_MAX);
		mValSpinner1->setMinValue(F32_MIN);
		mValSpinner2->setMinValue(F32_MIN);
		mValSpinner3->setMinValue(F32_MIN);
		mValSpinner4->setMinValue(F32_MIN);
		if (!mValSpinner1->hasFocus())
		{
			mValSpinner1->setIncrement(0.1f);
		}
		if (!mValSpinner2->hasFocus())
		{
			mValSpinner2->setIncrement(0.1f);
		}
		if (!mValSpinner3->hasFocus())
		{
			mValSpinner3->setIncrement(0.1f);
		}
		if (!mValSpinner4->hasFocus())
		{
			mValSpinner4->setIncrement(0.1f);
		}

		LLSD sd = mCurrentControlVariable->get();
		switch(type)
		{
		  case TYPE_U32:
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("value")); // Debug, don't translate
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setValue(sd);
				mValSpinner1->setMinValue((F32)U32_MIN);
				mValSpinner1->setMaxValue((F32)U32_MAX);
				mValSpinner1->setIncrement(1.f);
				mValSpinner1->setPrecision(0);
			}
			break;
		  case TYPE_S32:
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("value")); // Debug, don't translate
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setValue(sd);
				mValSpinner1->setMinValue((F32)S32_MIN);
				mValSpinner1->setMaxValue((F32)S32_MAX);
				mValSpinner1->setIncrement(1.f);
				mValSpinner1->setPrecision(0);
			}
			break;
		  case TYPE_F32:
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("value")); // Debug, don't translate
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setPrecision(3);
				mValSpinner1->setValue(sd);
			}
			break;
		  case TYPE_BOOLEAN:
			mValBool->setVisible(true);
			if (!mValBool->hasFocus())
			{
				mValBool->setValue(LLSD(sd.asBoolean() ? "TRUE" : "FALSE"));
			}
			break;
		  case TYPE_STRING:
			mValText->setVisible(TRUE);
			if (!mValText->hasFocus())
			{
				mValText->setValue(sd);
			}
			break;
		  case TYPE_VEC3:
		  {
			LLVector3 v;
			v.setValue(sd);
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("X"));
			mValSpinner2->setVisible(TRUE);
			mValSpinner2->setLabel(LLStringExplicit("Y"));
			mValSpinner3->setVisible(TRUE);
			mValSpinner3->setLabel(LLStringExplicit("Z"));
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setPrecision(3);
				mValSpinner1->setValue(v[VX]);
			}
			if (!mValSpinner2->hasFocus())
			{
				mValSpinner2->setPrecision(3);
				mValSpinner2->setValue(v[VY]);
			}
			if (!mValSpinner3->hasFocus())
			{
				mValSpinner3->setPrecision(3);
				mValSpinner3->setValue(v[VZ]);
			}
			break;
		  }
		  case TYPE_VEC3D:
		  {
			LLVector3d v;
			v.setValue(sd);
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("X"));
			mValSpinner2->setVisible(TRUE);
			mValSpinner2->setLabel(LLStringExplicit("Y"));
			mValSpinner3->setVisible(TRUE);
			mValSpinner3->setLabel(LLStringExplicit("Z"));
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setPrecision(3);
				mValSpinner1->setValue(v[VX]);
			}
			if (!mValSpinner2->hasFocus())
			{
				mValSpinner2->setPrecision(3);
				mValSpinner2->setValue(v[VY]);
			}
			if (!mValSpinner3->hasFocus())
			{
				mValSpinner3->setPrecision(3);
				mValSpinner3->setValue(v[VZ]);
			}
			break;
		  }
		  case TYPE_RECT:
		  {
			LLRect r;
			r.setValue(sd);
			mValSpinner1->setVisible(TRUE);
			mValSpinner1->setLabel(LLStringExplicit("Left"));
			mValSpinner2->setVisible(TRUE);
			mValSpinner2->setLabel(LLStringExplicit("Right"));
			mValSpinner3->setVisible(TRUE);
			mValSpinner3->setLabel(LLStringExplicit("Bottom"));
			mValSpinner4->setVisible(TRUE);
			mValSpinner4->setLabel(LLStringExplicit("Top"));
			if (!mValSpinner1->hasFocus())
			{
				mValSpinner1->setPrecision(0);
				mValSpinner1->setValue(r.mLeft);
			}
			if (!mValSpinner2->hasFocus())
			{
				mValSpinner2->setPrecision(0);
				mValSpinner2->setValue(r.mRight);
			}
			if (!mValSpinner3->hasFocus())
			{
				mValSpinner3->setPrecision(0);
				mValSpinner3->setValue(r.mBottom);
			}
			if (!mValSpinner4->hasFocus())
			{
				mValSpinner4->setPrecision(0);
				mValSpinner4->setValue(r.mTop);
			}

			mValSpinner1->setMinValue((F32)S32_MIN);
			mValSpinner1->setMaxValue((F32)S32_MAX);
			mValSpinner1->setIncrement(1.f);

			mValSpinner2->setMinValue((F32)S32_MIN);
			mValSpinner2->setMaxValue((F32)S32_MAX);
			mValSpinner2->setIncrement(1.f);

			mValSpinner3->setMinValue((F32)S32_MIN);
			mValSpinner3->setMaxValue((F32)S32_MAX);
			mValSpinner3->setIncrement(1.f);

			mValSpinner4->setMinValue((F32)S32_MIN);
			mValSpinner4->setMaxValue((F32)S32_MAX);
			mValSpinner4->setIncrement(1.f);
			break;
		  }
		  case TYPE_COL4:
		  {
			LLColor4 clr;
			clr.setValue(sd);
			mValColor->setVisible(TRUE);
			// only set if changed so color picker doesn't update
			if(clr != LLColor4(mValColor->getValue()))
			{
				mValColor->set(LLColor4(sd), TRUE, FALSE);
			}
			mValSpinner4->setVisible(TRUE);
			mValSpinner4->setLabel(LLStringExplicit("Alpha"));
			if (!mValSpinner4->hasFocus())
			{
				mValSpinner4->setPrecision(3);
				mValSpinner4->setMinValue(0.0);
				mValSpinner4->setMaxValue(1.f);
				mValSpinner4->setValue(clr.mV[VALPHA]);
			}
			break;
		  }
		  case TYPE_COL3:
		  {
			LLColor3 clr;
			clr.setValue(sd);
			mValColor->setVisible(TRUE);
			mValColor->setValue(sd);
			break;
		  }
		  case TYPE_COL4U:
		  {
			LLColor4U clr(sd);
			mValColor->setVisible(TRUE);
			if (LLColor4(clr) != LLColor4(mValColor->getValue()))
			{
				mValColor->set(LLColor4(clr), TRUE, FALSE);
			}
			mValSpinner4->setVisible(TRUE);
			mValSpinner4->setLabel(std::string("Alpha"));
			if(!mValSpinner4->hasFocus())
			{
				mValSpinner4->setPrecision(0);
				mValSpinner4->setValue(clr.mV[VALPHA]);
			}

			mValSpinner4->setMinValue(0);
			mValSpinner4->setMaxValue(255);
			mValSpinner4->setIncrement(1.f);

			break;
		  }
		  case TYPE_LLSD:
			mValText->setVisible(true);
			{
				std::ostringstream str;
				LLSDSerialize::toPrettyXML(sd, str);
				mValText->setValue(str.str());
			}
			break;
		  default:
			mComment->setText(LLStringExplicit("unknown"));
			break;
		}
	}

}

void LLFloaterSettingsDebug::onUpdateFilter(const LLSD& value)
{
	updateFilter(value.asString());
}

void LLFloaterSettingsDebug::updateFilter(std::string searchTerm)
{
	LLStringUtil::toLower(searchTerm);

	// make sure not to reselect the first item in the list on focus restore
	if (searchTerm == mOldSearchTerm) return;

	mOldSearchTerm = searchTerm;

	mSettingsScrollList->deleteAllItems();

	for(const auto& pair : mSettingsMap)
		addRow(pair.second, searchTerm);

	mSettingsScrollList->sortByColumnIndex(0, true);

	// if at least one match was found, highlight and select the topmost entry in the list
	// but only if actually a search term was given
	if (!searchTerm.empty() && mSettingsScrollList->getItemCount())
		mSettingsScrollList->selectFirstItem();

	onSettingSelect();
}
