/** 
 * @file llfloateractivespeakers.cpp
 * @brief Management interface for muting and controlling volume of residents currently speaking
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfloateractivespeakers.h"

#include "llagent.h"
#include "llvoavatar.h"
#include "llfloateravatarinfo.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llscrolllistctrl.h"
#include "llbutton.h"
#include "lltextbox.h"
#include "llmutelist.h"
#include "llviewerobjectlist.h"
#include "llimpanel.h" // LLVoiceChannel
#include "llsdutil.h"
#include "llimview.h"
#include "llviewerwindow.h"
#include "llworld.h"
#include "llappviewer.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

#include "llavatarname.h"

const F32 SPEAKER_TIMEOUT = 10.f; // seconds of not being on voice channel before removed from list of active speakers
const F32 RESORT_TIMEOUT = 5.f; // seconds of mouse inactivity before it's ok to sort regardless of mouse-in-view.
const LLColor4 INACTIVE_COLOR(0.3f, 0.3f, 0.3f, 0.5f);
const LLColor4 ACTIVE_COLOR(0.5f, 0.5f, 0.5f, 1.f);
const F32 TYPING_ANIMATION_FPS = 2.5f;

LLSpeaker::LLSpeaker(const LLUUID& id, const std::string& name, const ESpeakerType type) : 
	mStatus(LLSpeaker::STATUS_TEXT_ONLY),
	mLastSpokeTime(0.f), 
	mSpeechVolume(0.f), 
	mHasSpoken(FALSE),
	mDotColor(LLColor4::white),
	mID(id),
	mTyping(FALSE),
	mSortIndex(0),
	mType(type),
	mIsModerator(FALSE),
	mModeratorMutedVoice(FALSE),
	mModeratorMutedText(FALSE)
{
	// Make sure we also get the display name if SLIM or some other external
	// voice client is used and not whatever is provided.
	if ((name.empty() && type == SPEAKER_AGENT) || type == SPEAKER_EXTERNAL)
	{
		lookupName();
	}
	else
	{
		mDisplayName = name;
		mLegacyName = name;
	}

	gVoiceClient->setUserVolume(id, LLMuteList::getInstance()->getSavedResidentVolume(id));

	mActivityTimer.reset(SPEAKER_TIMEOUT);
}


void LLSpeaker::lookupName()
{
    // [Ansariel: Display name support]
	LLAvatarNameCache::get(mID, boost::bind(&LLSpeaker::onAvatarNameLookup, _1, _2, new LLHandle<LLSpeaker>(getHandle())));
    // [/Ansariel: Display name support]
}

//static 
// [Ansariel: Display name support]
void LLSpeaker::onAvatarNameLookup(const LLUUID& id, const LLAvatarName& avatar_name, void* user_data)
// [/Ansariel: Display name support]
{
	LLSpeaker* speaker_ptr = ((LLHandle<LLSpeaker>*)user_data)->get();
	delete (LLHandle<LLSpeaker>*)user_data;

	if (speaker_ptr)
	{
        // [Ansariel: Display name support]
		switch (gSavedSettings.getS32("PhoenixNameSystem"))
		{
			case 0 : speaker_ptr->mDisplayName = avatar_name.getLegacyName(); break;
			case 1 : speaker_ptr->mDisplayName = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
			case 2 : speaker_ptr->mDisplayName = avatar_name.mDisplayName; break;
			default : speaker_ptr->mDisplayName = avatar_name.getLegacyName(); break;
		}
		
		// Also set the legacy name. We will need it to initiate a new
		// IM session.
		speaker_ptr->mLegacyName = avatar_name.getLegacyName();
	    // [/Ansariel: Display name support]
		
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g) | Added: RLVa-1.0.0g
		// TODO-RLVa: this seems to get called per frame which is very likely an LL bug that will eventuall get fixed
		if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
			speaker_ptr->mDisplayName = RlvStrings::getAnonym(speaker_ptr->mDisplayName);
// [/RLVa:KB]
	}
}

LLSpeakerTextModerationEvent::LLSpeakerTextModerationEvent(LLSpeaker* source)
: LLEvent(source, "Speaker text moderation event")
{
}

LLSD LLSpeakerTextModerationEvent::getValue()
{
	return std::string("text");
}


LLSpeakerVoiceModerationEvent::LLSpeakerVoiceModerationEvent(LLSpeaker* source)
: LLEvent(source, "Speaker voice moderation event")
{
}

LLSD LLSpeakerVoiceModerationEvent::getValue()
{
	return std::string("voice");
}

LLSpeakerListChangeEvent::LLSpeakerListChangeEvent(LLSpeakerMgr* source, const LLUUID& speaker_id)
: LLEvent(source, "Speaker added/removed from speaker mgr"),
  mSpeakerID(speaker_id)
{
}

LLSD LLSpeakerListChangeEvent::getValue()
{
	return mSpeakerID;
}

// helper sort class
struct LLSortRecentSpeakers
{
	bool operator()(const LLPointer<LLSpeaker> lhs, const LLPointer<LLSpeaker> rhs) const;
};

bool LLSortRecentSpeakers::operator()(const LLPointer<LLSpeaker> lhs, const LLPointer<LLSpeaker> rhs) const
{
	// Sort first on status
	if (lhs->mStatus != rhs->mStatus) 
	{
		return (lhs->mStatus < rhs->mStatus);
	}

	// and then on last speaking time
	if(lhs->mLastSpokeTime != rhs->mLastSpokeTime)
	{
		return (lhs->mLastSpokeTime > rhs->mLastSpokeTime);
	}
	
	// and finally (only if those are both equal), on name.
	return(	lhs->mDisplayName.compare(rhs->mDisplayName) < 0 );
}

//
// LLFloaterActiveSpeakers
//

LLFloaterActiveSpeakers::LLFloaterActiveSpeakers(const LLSD& seed) : mPanel(NULL)
{
	mFactoryMap["active_speakers_panel"] = LLCallbackMap(createSpeakersPanel, NULL);
	// do not automatically open singleton floaters (as result of getInstance())
	BOOL no_open = FALSE;
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_active_speakers.xml", &getFactoryMap(), no_open);	
	//RN: for now, we poll voice client every frame to get voice amplitude feedback
	//gVoiceClient->addObserver(this);
	mPanel->refreshSpeakers();
}

LLFloaterActiveSpeakers::~LLFloaterActiveSpeakers()
{
}

void LLFloaterActiveSpeakers::onOpen()
{
	gSavedSettings.setBOOL("ShowActiveSpeakers", TRUE);
}

void LLFloaterActiveSpeakers::onClose(bool app_quitting)
{
	if (!app_quitting)
	{
		gSavedSettings.setBOOL("ShowActiveSpeakers", FALSE);
	}
	setVisible(FALSE);
}

void LLFloaterActiveSpeakers::draw()
{
	// update state every frame to get live amplitude feedback
	mPanel->refreshSpeakers();
	LLFloater::draw();
}

BOOL LLFloaterActiveSpeakers::postBuild()
{
	mPanel = getChild<LLPanelActiveSpeakers>("active_speakers_panel");
	return TRUE;
}

void LLFloaterActiveSpeakers::onChange()
{
	//refresh();
}

//static
void* LLFloaterActiveSpeakers::createSpeakersPanel(void* data)
{
	// don't show text only speakers
	return new LLPanelActiveSpeakers(LLActiveSpeakerMgr::getInstance(), FALSE);
}

//
// LLPanelActiveSpeakers::SpeakerMuteListener
//
bool LLPanelActiveSpeakers::SpeakerMuteListener::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	LLPointer<LLSpeaker> speakerp = (LLSpeaker*)event->getSource();
	if (speakerp.isNull()) return false;

	// update UI on confirmation of moderator mutes
	if (event->getValue().asString() == "voice")
	{
		mPanel->childSetValue("moderator_allow_voice", !speakerp->mModeratorMutedVoice);
	}
	if (event->getValue().asString() == "text")
	{
		mPanel->childSetValue("moderator_allow_text", !speakerp->mModeratorMutedText);
	}
	return true;
}


//
// LLPanelActiveSpeakers::SpeakerAddListener
//
bool LLPanelActiveSpeakers::SpeakerAddListener::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	mPanel->addSpeaker(event->getValue().asUUID());
	return true;
}


//
// LLPanelActiveSpeakers::SpeakerRemoveListener
//
bool LLPanelActiveSpeakers::SpeakerRemoveListener::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	mPanel->removeSpeaker(event->getValue().asUUID());
	return true;
}

//
// LLPanelActiveSpeakers::SpeakerClearListener
//
bool LLPanelActiveSpeakers::SpeakerClearListener::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	mPanel->mSpeakerList->clearRows();
	return true;
}


//
// LLPanelActiveSpeakers
//
LLPanelActiveSpeakers::LLPanelActiveSpeakers(LLSpeakerMgr* data_source, BOOL show_text_chatters) : 
	mSpeakerList(NULL),
	mMuteVoiceCtrl(NULL),
	mMuteTextCtrl(NULL),
	mNameText(NULL),
	mProfileBtn(NULL),
	mShowTextChatters(show_text_chatters),
	mSpeakerMgr(data_source)
{
	setMouseOpaque(FALSE);
	mSpeakerMuteListener = new SpeakerMuteListener(this);
	mSpeakerAddListener = new SpeakerAddListener(this);
	mSpeakerRemoveListener = new SpeakerRemoveListener(this);
	mSpeakerClearListener = new SpeakerClearListener(this);

	mSpeakerMgr->addListener(mSpeakerAddListener, "add");
	mSpeakerMgr->addListener(mSpeakerRemoveListener, "remove");
	mSpeakerMgr->addListener(mSpeakerClearListener, "clear");
}

BOOL LLPanelActiveSpeakers::postBuild()
{
	std::string sort_column = gSavedSettings.getString(std::string("FloaterActiveSpeakersSortColumn"));
	BOOL sort_ascending     = gSavedSettings.getBOOL(  std::string("FloaterActiveSpeakersSortAscending"));

	mSpeakerList = getChild<LLScrollListCtrl>("speakers_list");
	mSpeakerList->sortByColumn(sort_column, sort_ascending);
	mSpeakerList->setDoubleClickCallback(onDoubleClickSpeaker);
	mSpeakerList->setCommitOnSelectionChange(TRUE);
	mSpeakerList->setCommitCallback(onSelectSpeaker);
	mSpeakerList->setSortChangedCallback(onSortChanged);
	mSpeakerList->setCallbackUserData(this);

	mMuteTextCtrl = getChild<LLUICtrl>("mute_text_btn");
	childSetCommitCallback("mute_text_btn", onClickMuteTextCommit, this);

	mMuteVoiceCtrl = getChild<LLUICtrl>("mute_btn");
	childSetCommitCallback("mute_btn", onClickMuteVoiceCommit, this);
	childSetAction("mute_btn", onClickMuteVoice, this);

	childSetCommitCallback("speaker_volume", onVolumeChange, this);

	mNameText = getChild<LLTextBox>("resident_name");
	
	mProfileBtn = getChild<LLButton>("profile_btn");
	childSetAction("profile_btn", onClickProfile, this);

	childSetCommitCallback("moderator_allow_voice", onModeratorMuteVoice, this);
	childSetCommitCallback("moderator_allow_text", onModeratorMuteText, this);
	childSetCommitCallback("moderation_mode", onChangeModerationMode, this);

	// update speaker UI
	handleSpeakerSelect();

	return TRUE;
}

void LLPanelActiveSpeakers::addSpeaker(const LLUUID& speaker_id)
{
	if (mSpeakerList->getItemIndex(speaker_id) >= 0)
	{
		// already have this speaker
		return;
	}

	LLPointer<LLSpeaker> speakerp = mSpeakerMgr->findSpeaker(speaker_id);
	if (speakerp)
	{
		// since we are forced to sort by text, encode sort order as string
		std::string speaking_order_sort_string = llformat("%010d", speakerp->mSortIndex);

		LLSD row;
		row["id"] = speaker_id;

		LLSD& columns = row["columns"];

		columns[0]["column"] = "icon_speaking_status";
		columns[0]["type"] = "icon";
		columns[0]["value"] = "icn_active-speakers-dot-lvl0.tga";

		std::string speaker_name;
		if (speakerp->mDisplayName.empty())
		{
			speaker_name = LLCacheName::getDefaultName();
		}
		else
		{
			speaker_name = speakerp->mDisplayName;
		}
		columns[1]["column"] = "speaker_name";
		columns[1]["type"] = "text";
		columns[1]["value"] = speaker_name;

		columns[2]["column"] = "speaking_status";
		columns[2]["type"] = "text";
		
		// print speaking ordinal in a text-sorting friendly manner
		columns[2]["value"] = speaking_order_sort_string;

		mSpeakerList->addElement(row);
	}
}

void LLPanelActiveSpeakers::removeSpeaker(const LLUUID& speaker_id)
{
	mSpeakerList->deleteSingleItem(mSpeakerList->getItemIndex(speaker_id));
}

void LLPanelActiveSpeakers::handleSpeakerSelect()
{
	LLUUID speaker_id = mSpeakerList->getValue().asUUID();
	LLPointer<LLSpeaker> selected_speakerp = mSpeakerMgr->findSpeaker(speaker_id);

	if (selected_speakerp.notNull())
	{
		// since setting these values is delayed by a round trip to the Vivox servers
		// update them only when selecting a new speaker or
		// asynchronously when an update arrives
		childSetValue("moderator_allow_voice", selected_speakerp ? !selected_speakerp->mModeratorMutedVoice : TRUE);
		childSetValue("moderator_allow_text", selected_speakerp ? !selected_speakerp->mModeratorMutedText : TRUE);

		mSpeakerMuteListener->clearDispatchers();
		selected_speakerp->addListener(mSpeakerMuteListener);
	}
}

void LLPanelActiveSpeakers::refreshSpeakers()
{
	// store off current selection and scroll state to preserve across list rebuilds
	LLUUID selected_id = mSpeakerList->getSelectedValue().asUUID();
	S32 scroll_pos = mSpeakerList->getScrollInterface()->getScrollPos();

	// decide whether it's ok to resort the list then update the speaker manager appropriately.
	// rapid resorting by activity makes it hard to interact with speakers in the list
	// so we freeze the sorting while the user appears to be interacting with the control.
	// we assume this is the case whenever the mouse pointer is within the active speaker
	// panel and hasn't been motionless for more than a few seconds. see DEV-6655 -MG
	LLRect screen_rect;
	localRectToScreen(getLocalRect(), &screen_rect);
	BOOL mouse_in_view = screen_rect.pointInRect(gViewerWindow->getCurrentMouseX(), gViewerWindow->getCurrentMouseY());
	F32 mouses_last_movement = gMouseIdleTimer.getElapsedTimeF32();
	BOOL sort_ok = ! (mouse_in_view && mouses_last_movement<RESORT_TIMEOUT);
	mSpeakerMgr->update(sort_ok);

	const std::string icon_image_0 = "icn_active-speakers-dot-lvl0.tga";
	const std::string icon_image_1 = "icn_active-speakers-dot-lvl1.tga";
	const std::string icon_image_2 = "icn_active-speakers-dot-lvl2.tga";

	std::vector<LLScrollListItem*> items = mSpeakerList->getAllData();

	std::string mute_icon_image = "mute_icon.tga";

	LLSpeakerMgr::speaker_list_t speaker_list;
	mSpeakerMgr->getSpeakerList(&speaker_list, mShowTextChatters);
	for (std::vector<LLScrollListItem*>::iterator item_it = items.begin();
		item_it != items.end();
		++item_it)
	{
		LLScrollListItem* itemp = (*item_it);
		LLUUID speaker_id = itemp->getUUID();

		LLPointer<LLSpeaker> speakerp = mSpeakerMgr->findSpeaker(speaker_id);
		if (!speakerp)
		{
			continue;
		}

		// since we are forced to sort by text, encode sort order as string
		std::string speaking_order_sort_string = llformat("%010d", speakerp->mSortIndex);

		LLScrollListCell* icon_cell = itemp->getColumn(0);
		if (icon_cell)
		{

			std::string icon_image_id;

			S32 icon_image_idx = llmin(2, llfloor((speakerp->mSpeechVolume / LLVoiceClient::OVERDRIVEN_POWER_LEVEL) * 3.f));
			switch(icon_image_idx)
			{
			case 0:
				icon_image_id = icon_image_0;
				break;
			case 1:
				icon_image_id = icon_image_1;
				break;
			case 2:
				icon_image_id = icon_image_2;
				break;
			}

			LLColor4 icon_color;
			
			if (speakerp->mStatus == LLSpeaker::STATUS_MUTED)
			{
				icon_cell->setValue(mute_icon_image);
				if(speakerp->mModeratorMutedVoice)
				{
					icon_color.setVec(0.5f, 0.5f, 0.5f, 1.f);
				}
				else
				{
					icon_color.setVec(1.f, 71.f / 255.f, 71.f / 255.f, 1.f);
				}
			}
			else
			{
				icon_cell->setValue(icon_image_id);
				icon_color = speakerp->mDotColor;

				if (speakerp->mStatus > LLSpeaker::STATUS_VOICE_ACTIVE) // if voice is disabled for this speaker
				{
					// non voice speakers have hidden icons, render as transparent
					icon_color.setVec(0.f, 0.f, 0.f, 0.f);
				}
			}

			icon_cell->setColor(icon_color);

			if (speakerp->mStatus > LLSpeaker::STATUS_VOICE_ACTIVE && speakerp->mStatus != LLSpeaker::STATUS_MUTED) // if voice is disabled for this speaker
			{
				// non voice speakers have hidden icons, render as transparent
				icon_cell->setColor(LLColor4::transparent);
			}
		}

		// update name column
		LLScrollListCell* name_cell = itemp->getColumn(1);
		if (name_cell)
		{
			//FIXME: remove hard coding of font colors
			if (speakerp->mStatus == LLSpeaker::STATUS_NOT_IN_CHANNEL)	
			{
				// draw inactive speakers in gray
				name_cell->setColor(LLColor4::grey4);
			}
			else
			{
				name_cell->setColor(LLColor4::black);
			}
			// <edit>
			if(!mShowTextChatters && !(speakerp->mStatus == LLSpeaker::STATUS_NOT_IN_CHANNEL) && speakerp->mID != gAgent.getID())
			{
				bool found = false;
				for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
						iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
				{
					LLViewerRegion* regionp = *iter;
					// let us check to see if they are actually in the sim
					if(regionp)
					{
						if(regionp->mMapAvatarIDs.find(speakerp->mID) != -1)
						{
							found = true;
							break;
						}
					}
				}
				if(!found)
					name_cell->setColor(LLColor4::red);
			}
			// </edit>

			std::string speaker_name;
			if (speakerp->mDisplayName.empty())
			{
				speaker_name = LLCacheName::getDefaultName();
			}
			else
			{
				speaker_name = speakerp->mDisplayName;
			}

			if (speakerp->mIsModerator)
			{
				speaker_name += std::string(" ") + getString("moderator_label");
			}
			
			name_cell->setValue(speaker_name);
			((LLScrollListText*)name_cell)->setFontStyle(speakerp->mIsModerator ? LLFontGL::BOLD : LLFontGL::NORMAL);
		}

		// update speaking order column
		LLScrollListCell* speaking_status_cell = itemp->getColumn(2);
		if (speaking_status_cell)
		{
			// print speaking ordinal in a text-sorting friendly manner
			speaking_status_cell->setValue(speaking_order_sort_string);
		}
	}
	
	// we potentially modified the sort order by touching the list items
	mSpeakerList->setSorted(FALSE);

	LLPointer<LLSpeaker> selected_speakerp = mSpeakerMgr->findSpeaker(selected_id);
	// update UI for selected participant
	if (mMuteVoiceCtrl)
	{
		mMuteVoiceCtrl->setValue(LLMuteList::getInstance()->isMuted(selected_id, LLMute::flagVoiceChat));
		mMuteVoiceCtrl->setEnabled(LLVoiceClient::voiceEnabled()
									&& gVoiceClient->getVoiceEnabled(selected_id)
									&& selected_id.notNull() 
									&& selected_id != gAgent.getID() 
									&& (selected_speakerp.notNull() && (selected_speakerp->mType == LLSpeaker::SPEAKER_AGENT || selected_speakerp->mType == LLSpeaker::SPEAKER_EXTERNAL)));

	}
	if (mMuteTextCtrl)
	{
		mMuteTextCtrl->setValue(LLMuteList::getInstance()->isMuted(selected_id, LLMute::flagTextChat));
		mMuteTextCtrl->setEnabled(selected_id.notNull() 
								&& selected_id != gAgent.getID() 
								&& selected_speakerp.notNull() 
								&& selected_speakerp->mType != LLSpeaker::SPEAKER_EXTERNAL
								// Ansariel: No, we don't want to mute Lindens with display names
								//&& !LLMuteList::getInstance()->isLinden(selected_speakerp->mDisplayName));
								&& !LLMuteList::getInstance()->isLinden(selected_speakerp->mLegacyName));
	}
	childSetValue("speaker_volume", gVoiceClient->getUserVolume(selected_id));
	childSetEnabled("speaker_volume", LLVoiceClient::voiceEnabled()
					&& gVoiceClient->getVoiceEnabled(selected_id)
					&& selected_id.notNull() 
					&& selected_id != gAgent.getID() 
					&& (selected_speakerp.notNull() && (selected_speakerp->mType == LLSpeaker::SPEAKER_AGENT || selected_speakerp->mType == LLSpeaker::SPEAKER_EXTERNAL)));

	childSetEnabled(
		"moderator_controls_label",
		selected_id.notNull());

	childSetEnabled(
		"moderator_allow_voice", 
		selected_id.notNull() 
		&& mSpeakerMgr->isVoiceActive()
		&& gVoiceClient->getVoiceEnabled(selected_id));

	childSetEnabled(
		"moderator_allow_text", 
		selected_id.notNull());

	if (mProfileBtn)
	{
		mProfileBtn->setEnabled(selected_id.notNull() && (selected_speakerp.notNull() && selected_speakerp->mType != LLSpeaker::SPEAKER_EXTERNAL) );
	}

	// show selected user name in large font
	if (mNameText)
	{
		if (selected_speakerp)
		{
			mNameText->setValue(selected_speakerp->mDisplayName);
		}
		else
		{
			mNameText->setValue(LLStringUtil::null);
		}
	}

	//update moderator capabilities
	LLPointer<LLSpeaker> self_speakerp = mSpeakerMgr->findSpeaker(gAgent.getID());
	if(self_speakerp)
	{
		childSetVisible("moderation_mode_panel", self_speakerp->mIsModerator && mSpeakerMgr->isVoiceActive());
		childSetVisible("moderator_controls", self_speakerp->mIsModerator);
	}

	// keep scroll value stable
	mSpeakerList->getScrollInterface()->setScrollPos(scroll_pos);
}

void LLPanelActiveSpeakers::setSpeaker(const LLUUID& id, const std::string& name, LLSpeaker::ESpeakerStatus status, LLSpeaker::ESpeakerType type)
{
	mSpeakerMgr->setSpeaker(id, name, status, type);
}

void LLPanelActiveSpeakers::setVoiceModerationCtrlMode(
	const BOOL& moderated_voice)
{
	LLUICtrl* voice_moderation_ctrl = getChild<LLUICtrl>("moderation_mode");

	if ( voice_moderation_ctrl )
	{
		std::string value;

		value = moderated_voice ? "moderated" : "unmoderated";
		voice_moderation_ctrl->setValue(value);
	}
}

//static
void LLPanelActiveSpeakers::onClickMuteTextCommit(LLUICtrl* ctrl, void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	LLUUID speaker_id = panelp->mSpeakerList->getValue().asUUID();
	BOOL is_muted = LLMuteList::getInstance()->isMuted(speaker_id, LLMute::flagTextChat);
	std::string name;

	//fill in name using voice client's copy of name cache
	LLPointer<LLSpeaker> speakerp = panelp->mSpeakerMgr->findSpeaker(speaker_id);
	if (speakerp.isNull())
	{
		return;
	}
	
	name = speakerp->mDisplayName;

	LLMute mute(speaker_id, name, speakerp->mType == LLSpeaker::SPEAKER_AGENT ? LLMute::AGENT : LLMute::OBJECT);

	if (!is_muted)
	{
		LLMuteList::getInstance()->add(mute, LLMute::flagTextChat);
	}
	else
	{
		LLMuteList::getInstance()->remove(mute, LLMute::flagTextChat);
	}
}

//static
void LLPanelActiveSpeakers::onClickMuteVoice(void* user_data)
{
	onClickMuteVoiceCommit(NULL, user_data);
}

//static
void LLPanelActiveSpeakers::onClickMuteVoiceCommit(LLUICtrl* ctrl, void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	LLUUID speaker_id = panelp->mSpeakerList->getValue().asUUID();
	BOOL is_muted = LLMuteList::getInstance()->isMuted(speaker_id, LLMute::flagVoiceChat);
	std::string name;

	LLPointer<LLSpeaker> speakerp = panelp->mSpeakerMgr->findSpeaker(speaker_id);
	if (speakerp.isNull())
	{
		return;
	}

	name = speakerp->mDisplayName;

	// muting voice means we're dealing with an agent
	LLMute mute(speaker_id, name, LLMute::AGENT);

	if (!is_muted)
	{
		LLMuteList::getInstance()->add(mute, LLMute::flagVoiceChat);
	}
	else
	{
		LLMuteList::getInstance()->remove(mute, LLMute::flagVoiceChat);
	}
}


//static
void LLPanelActiveSpeakers::onVolumeChange(LLUICtrl* source, void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	LLUUID speaker_id = panelp->mSpeakerList->getValue().asUUID();

	F32 new_volume = (F32)panelp->childGetValue("speaker_volume").asReal();
	gVoiceClient->setUserVolume(speaker_id, new_volume); 

	// store this volume setting for future sessions
	LLMuteList::getInstance()->setSavedResidentVolume(speaker_id, new_volume);
}

//static 
void LLPanelActiveSpeakers::onClickProfile(void* user_data)
{
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g) | Added: RLVa-1.0.0g
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		return;
	}
// [/RLVa:KB]

	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	LLUUID speaker_id = panelp->mSpeakerList->getValue().asUUID();

	LLFloaterAvatarInfo::showFromDirectory(speaker_id);
}

//static
void LLPanelActiveSpeakers::onDoubleClickSpeaker(void* user_data)
{
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g) | Added: RLVa-1.0.0g
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		return;
	}
// [/RLVa:KB]

	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	LLUUID speaker_id = panelp->mSpeakerList->getValue().asUUID();

	LLPointer<LLSpeaker> speakerp = panelp->mSpeakerMgr->findSpeaker(speaker_id);

	if (speaker_id != gAgent.getID() && speakerp.notNull())
	{
		// Changed for display name support
		//gIMMgr->addSession(speakerp->mDisplayName, IM_NOTHING_SPECIAL, speaker_id);
		gIMMgr->addSession(speakerp->mLegacyName, IM_NOTHING_SPECIAL, speaker_id);
	}
}

//static
void LLPanelActiveSpeakers::onSelectSpeaker(LLUICtrl* source, void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	panelp->handleSpeakerSelect();
}


//static
void LLPanelActiveSpeakers::onSortChanged(void* user_data)
{
	LLPanelActiveSpeakers* panelp = (LLPanelActiveSpeakers*)user_data;
	std::string sort_column = panelp->mSpeakerList->getSortColumnName();
	BOOL sort_ascending = panelp->mSpeakerList->getSortAscending();
	gSavedSettings.setString(std::string("FloaterActiveSpeakersSortColumn"), sort_column);
	gSavedSettings.setBOOL(  std::string("FloaterActiveSpeakersSortAscending"), sort_ascending);
}


//static 
void LLPanelActiveSpeakers::onModeratorMuteVoice(LLUICtrl* ctrl, void* user_data)
{
	LLPanelActiveSpeakers* self = (LLPanelActiveSpeakers*)user_data;
	LLUICtrl* speakers_list = self->getChild<LLUICtrl>("speakers_list");
	if (!speakers_list || !gAgent.getRegion()) return;

	std::string url = gAgent.getRegion()->getCapability("ChatSessionRequest");
	LLSD data;
	data["method"] = "mute update";
	data["session-id"] = self->mSpeakerMgr->getSessionID();
	data["params"] = LLSD::emptyMap();
	data["params"]["agent_id"] = speakers_list->getValue();
	data["params"]["mute_info"] = LLSD::emptyMap();
	// ctrl value represents ability to type, so invert
	data["params"]["mute_info"]["voice"] = !ctrl->getValue();

	class MuteVoiceResponder : public LLHTTPClient::Responder
	{
	public:
		MuteVoiceResponder(const LLUUID& session_id)
		{
			mSessionID = session_id;
		}

		virtual void error(U32 status, const std::string& reason)
		{
			llwarns << status << ": " << reason << llendl;

			if ( gIMMgr )
			{
				//403 == you're not a mod
				//should be disabled if you're not a moderator
				LLFloaterIMPanel* floaterp;

				floaterp = gIMMgr->findFloaterBySession(mSessionID);

				if ( floaterp )
				{
					if ( 403 == status )
					{
						floaterp->showSessionEventError(
							"mute",
							"not_a_moderator");
					}
					else
					{
						floaterp->showSessionEventError(
							"mute",
							"generic");
					}
				}
			}
		}

	private:
		LLUUID mSessionID;
	};

	LLHTTPClient::post(
		url,
		data,
		new MuteVoiceResponder(self->mSpeakerMgr->getSessionID()));
}

//static 
void LLPanelActiveSpeakers::onModeratorMuteText(LLUICtrl* ctrl, void* user_data)
{
	LLPanelActiveSpeakers* self = (LLPanelActiveSpeakers*)user_data;
	LLUICtrl* speakers_list = self->getChild<LLUICtrl>("speakers_list");
	if (!speakers_list || !gAgent.getRegion()) return;

	std::string url = gAgent.getRegion()->getCapability("ChatSessionRequest");
	LLSD data;
	data["method"] = "mute update";
	data["session-id"] = self->mSpeakerMgr->getSessionID();
	data["params"] = LLSD::emptyMap();
	data["params"]["agent_id"] = speakers_list->getValue();
	data["params"]["mute_info"] = LLSD::emptyMap();
	// ctrl value represents ability to type, so invert
	data["params"]["mute_info"]["text"] = !ctrl->getValue();

	class MuteTextResponder : public LLHTTPClient::Responder
	{
	public:
		MuteTextResponder(const LLUUID& session_id)
		{
			mSessionID = session_id;
		}

		virtual void error(U32 status, const std::string& reason)
		{
			llwarns << status << ": " << reason << llendl;

			if ( gIMMgr )
			{
				//403 == you're not a mod
				//should be disabled if you're not a moderator
				LLFloaterIMPanel* floaterp;

				floaterp = gIMMgr->findFloaterBySession(mSessionID);

				if ( floaterp )
				{
					if ( 403 == status )
					{
						floaterp->showSessionEventError(
							"mute",
							"not_a_moderator");
					}
					else
					{
						floaterp->showSessionEventError(
							"mute",
							"generic");
					}
				}
			}
		}

	private:
		LLUUID mSessionID;
	};

	LLHTTPClient::post(
		url,
		data,
		new MuteTextResponder(self->mSpeakerMgr->getSessionID()));
}

//static
void LLPanelActiveSpeakers::onChangeModerationMode(LLUICtrl* ctrl, void* user_data)
{
	LLPanelActiveSpeakers* self = (LLPanelActiveSpeakers*)user_data;
	if (!gAgent.getRegion()) return;

	std::string url = gAgent.getRegion()->getCapability("ChatSessionRequest");
	LLSD data;
	data["method"] = "session update";
	data["session-id"] = self->mSpeakerMgr->getSessionID();
	data["params"] = LLSD::emptyMap();

	data["params"]["update_info"] = LLSD::emptyMap();

	data["params"]["update_info"]["moderated_mode"] = LLSD::emptyMap();
	if (ctrl->getValue().asString() == "unmoderated")
	{
		data["params"]["update_info"]["moderated_mode"]["voice"] = false;
	}
	else if (ctrl->getValue().asString() == "moderated")
	{
		data["params"]["update_info"]["moderated_mode"]["voice"] = true;
	}

	struct ModerationModeResponder : public LLHTTPClient::Responder
	{
		virtual void error(U32 status, const std::string& reason)
		{
			llwarns << status << ": " << reason << llendl;
		}
	};

	LLHTTPClient::post(url, data, new ModerationModeResponder());
}

//
// LLSpeakerMgr
//

LLSpeakerMgr::LLSpeakerMgr(LLVoiceChannel* channelp) : 
	mVoiceChannel(channelp)
{
}

LLSpeakerMgr::~LLSpeakerMgr()
{
}

LLPointer<LLSpeaker> LLSpeakerMgr::setSpeaker(const LLUUID& id, const std::string& name, LLSpeaker::ESpeakerStatus status, LLSpeaker::ESpeakerType type)
{
	if (id.isNull()) return NULL;

	LLPointer<LLSpeaker> speakerp;
	if (mSpeakers.find(id) == mSpeakers.end())
	{
		speakerp = new LLSpeaker(id, name, type);
		speakerp->mStatus = status;
		mSpeakers.insert(std::make_pair(speakerp->mID, speakerp));
		mSpeakersSorted.push_back(speakerp);
		fireEvent(new LLSpeakerListChangeEvent(this, speakerp->mID), "add");
	}
	else
	{
		speakerp = findSpeaker(id);
		if (speakerp.notNull())
		{
			// keep highest priority status (lowest value) instead of overriding current value
			speakerp->mStatus = llmin(speakerp->mStatus, status);
			speakerp->mActivityTimer.reset(SPEAKER_TIMEOUT);
			// RN: due to a weird behavior where IMs from attached objects come from the wearer's agent_id
			// we need to override speakers that we think are objects when we find out they are really
			// residents
			if (type == LLSpeaker::SPEAKER_AGENT)
			{
				speakerp->mType = LLSpeaker::SPEAKER_AGENT;
				speakerp->lookupName();
			}
		}
	}

	return speakerp;
}

void LLSpeakerMgr::update(BOOL resort_ok)
{
	if (!gVoiceClient)
	{
		return;
	}
	
	LLColor4 speaking_color = gSavedSettings.getColor4("SpeakingColor");
	LLColor4 overdriven_color = gSavedSettings.getColor4("OverdrivenColor");

	if(resort_ok) // only allow list changes when user is not interacting with it
	{
		updateSpeakerList();
	}

	// update status of all current speakers
	BOOL voice_channel_active = (!mVoiceChannel && gVoiceClient->inProximalChannel()) || (mVoiceChannel && mVoiceChannel->isActive());
	for (speaker_map_t::iterator speaker_it = mSpeakers.begin(); speaker_it != mSpeakers.end();)
	{
		LLUUID speaker_id = speaker_it->first;
		LLSpeaker* speakerp = speaker_it->second;
		
		speaker_map_t::iterator  cur_speaker_it = speaker_it++;

		if (voice_channel_active && gVoiceClient->getVoiceEnabled(speaker_id))
		{
			speakerp->mSpeechVolume = gVoiceClient->getCurrentPower(speaker_id);
			BOOL moderator_muted_voice = gVoiceClient->getIsModeratorMuted(speaker_id);
			if (moderator_muted_voice != speakerp->mModeratorMutedVoice)
			{
				speakerp->mModeratorMutedVoice = moderator_muted_voice;
				speakerp->fireEvent(new LLSpeakerVoiceModerationEvent(speakerp));
			}

			if (gVoiceClient->getOnMuteList(speaker_id) || speakerp->mModeratorMutedVoice)
			{
				speakerp->mStatus = LLSpeaker::STATUS_MUTED;
			}
			else if (gVoiceClient->getIsSpeaking(speaker_id))
			{
				// reset inactivity expiration
				if (speakerp->mStatus != LLSpeaker::STATUS_SPEAKING)
				{
					speakerp->mLastSpokeTime = mSpeechTimer.getElapsedTimeF32();
					speakerp->mHasSpoken = TRUE;
				}
				speakerp->mStatus = LLSpeaker::STATUS_SPEAKING;
				// interpolate between active color and full speaking color based on power of speech output
				speakerp->mDotColor = speaking_color;
				if (speakerp->mSpeechVolume > LLVoiceClient::OVERDRIVEN_POWER_LEVEL)
				{
					speakerp->mDotColor = overdriven_color;
				}
			}
			else
			{
				speakerp->mSpeechVolume = 0.f;
				speakerp->mDotColor = ACTIVE_COLOR;

				if (speakerp->mHasSpoken)
				{
					// have spoken once, not currently speaking
					speakerp->mStatus = LLSpeaker::STATUS_HAS_SPOKEN;
				}
				else
				{
					// default state for being in voice channel
					speakerp->mStatus = LLSpeaker::STATUS_VOICE_ACTIVE;
				}
			}
		}
		// speaker no longer registered in voice channel, demote to text only
		else if (speakerp->mStatus != LLSpeaker::STATUS_NOT_IN_CHANNEL)
		{
			if(speakerp->mType == LLSpeaker::SPEAKER_EXTERNAL)
			{
				// external speakers should be timed out when they leave the voice channel (since they only exist via SLVoice)
				speakerp->mStatus = LLSpeaker::STATUS_NOT_IN_CHANNEL;
			}
			else
			{
				speakerp->mStatus = LLSpeaker::STATUS_TEXT_ONLY;
				speakerp->mSpeechVolume = 0.f;
				speakerp->mDotColor = ACTIVE_COLOR;
			}
		}
	}

	if(resort_ok)  // only allow list changes when user is not interacting with it
	{
		// sort by status then time last spoken
		std::sort(mSpeakersSorted.begin(), mSpeakersSorted.end(), LLSortRecentSpeakers());
	}

	// for recent speakers who are not currently speaking, show "recent" color dot for most recent
	// fading to "active" color

	S32 recent_speaker_count = 0;
	S32 sort_index = 0;
	speaker_list_t::iterator sorted_speaker_it;
	for(sorted_speaker_it = mSpeakersSorted.begin(); 
		sorted_speaker_it != mSpeakersSorted.end(); )
	{
		LLPointer<LLSpeaker> speakerp = *sorted_speaker_it;
		
		// color code recent speakers who are not currently speaking
		if (speakerp->mStatus == LLSpeaker::STATUS_HAS_SPOKEN)
		{
			speakerp->mDotColor = lerp(speaking_color, ACTIVE_COLOR, clamp_rescale((F32)recent_speaker_count, -2.f, 3.f, 0.f, 1.f));
			recent_speaker_count++;
		}

		// stuff sort ordinal into speaker so the ui can sort by this value
		speakerp->mSortIndex = sort_index++;

		// remove speakers that have been gone too long
		if (speakerp->mStatus == LLSpeaker::STATUS_NOT_IN_CHANNEL && speakerp->mActivityTimer.hasExpired())
		{
			fireEvent(new LLSpeakerListChangeEvent(this, speakerp->mID), "remove");

			mSpeakers.erase(speakerp->mID);
			sorted_speaker_it = mSpeakersSorted.erase(sorted_speaker_it);
		}
		else
		{
			++sorted_speaker_it;
		}
	}
}

void LLSpeakerMgr::updateSpeakerList()
{
	// are we bound to the currently active voice channel?
	if ((!mVoiceChannel && gVoiceClient->inProximalChannel()) || (mVoiceChannel && mVoiceChannel->isActive()))
	{
		LLVoiceClient::participantMap* participants = gVoiceClient->getParticipantList();
		if(participants)
		{
			LLVoiceClient::participantMap::iterator participant_it;

			// add new participants to our list of known speakers
			for (participant_it = participants->begin(); participant_it != participants->end(); ++participant_it)
			{
				LLVoiceClient::participantState* participantp = participant_it->second;
				setSpeaker(participantp->mAvatarID, participantp->mDisplayName, LLSpeaker::STATUS_VOICE_ACTIVE, (participantp->isAvatar()?LLSpeaker::SPEAKER_AGENT:LLSpeaker::SPEAKER_EXTERNAL));
			}
		}
	}
}

const LLPointer<LLSpeaker> LLSpeakerMgr::findSpeaker(const LLUUID& speaker_id)
{
	speaker_map_t::iterator found_it = mSpeakers.find(speaker_id);
	if (found_it == mSpeakers.end())
	{
		return NULL;
	}
	return found_it->second;
}

void LLSpeakerMgr::getSpeakerList(speaker_list_t* speaker_list, BOOL include_text)
{
	speaker_list->clear();
	for (speaker_map_t::iterator speaker_it = mSpeakers.begin(); speaker_it != mSpeakers.end(); ++speaker_it)
	{
		LLPointer<LLSpeaker> speakerp = speaker_it->second;
		// what about text only muted or inactive?
		if (include_text || speakerp->mStatus != LLSpeaker::STATUS_TEXT_ONLY)
		{
			speaker_list->push_back(speakerp);
		}
	}
}

const LLUUID LLSpeakerMgr::getSessionID() 
{ 
	return mVoiceChannel->getSessionID(); 
}


void LLSpeakerMgr::setSpeakerTyping(const LLUUID& speaker_id, BOOL typing)
{
	LLPointer<LLSpeaker> speakerp = findSpeaker(speaker_id);
	if (speakerp.notNull())
	{
		speakerp->mTyping = typing;
	}
}

// speaker has chatted via either text or voice
void LLSpeakerMgr::speakerChatted(const LLUUID& speaker_id)
{
	LLPointer<LLSpeaker> speakerp = findSpeaker(speaker_id);
	if (speakerp.notNull())
	{
		speakerp->mLastSpokeTime = mSpeechTimer.getElapsedTimeF32();
		speakerp->mHasSpoken = TRUE;
	}
}

BOOL LLSpeakerMgr::isVoiceActive()
{
	// mVoiceChannel = NULL means current voice channel, whatever it is
	return LLVoiceClient::voiceEnabled() && mVoiceChannel && mVoiceChannel->isActive();
}


//
// LLIMSpeakerMgr
//
LLIMSpeakerMgr::LLIMSpeakerMgr(LLVoiceChannel* channel) : LLSpeakerMgr(channel)
{
}

void LLIMSpeakerMgr::updateSpeakerList()
{
	// don't do normal updates which are pulled from voice channel
	// rely on user list reported by sim
	
	// We need to do this to allow PSTN callers into group chats to show in the list.
	LLSpeakerMgr::updateSpeakerList();
	
	return;
}

void LLIMSpeakerMgr::setSpeakers(const LLSD& speakers)
{
	if ( !speakers.isMap() ) return;

	if ( speakers.has("agent_info") && speakers["agent_info"].isMap() )
	{
		LLSD::map_const_iterator speaker_it;
		for(speaker_it = speakers["agent_info"].beginMap();
			speaker_it != speakers["agent_info"].endMap();
			++speaker_it)
		{
			LLUUID agent_id(speaker_it->first);

			LLPointer<LLSpeaker> speakerp = setSpeaker(
				agent_id,
				LLStringUtil::null,
				LLSpeaker::STATUS_TEXT_ONLY);

			if ( speaker_it->second.isMap() )
			{
				speakerp->mIsModerator = speaker_it->second["is_moderator"];
				speakerp->mModeratorMutedText =
					speaker_it->second["mutes"]["text"];
			}
		}
	}
	else if ( speakers.has("agents" ) && speakers["agents"].isArray() )
	{
		//older, more decprecated way.  Need here for
		//using older version of servers
		LLSD::array_const_iterator speaker_it;
		for(speaker_it = speakers["agents"].beginArray();
			speaker_it != speakers["agents"].endArray();
			++speaker_it)
		{
			const LLUUID agent_id = (*speaker_it).asUUID();

			LLPointer<LLSpeaker> speakerp = setSpeaker(
				agent_id,
				LLStringUtil::null,
				LLSpeaker::STATUS_TEXT_ONLY);
		}
	}
}

void LLIMSpeakerMgr::updateSpeakers(const LLSD& update)
{
	if ( !update.isMap() ) return;

	if ( update.has("agent_updates") && update["agent_updates"].isMap() )
	{
		LLSD::map_const_iterator update_it;
		for(
			update_it = update["agent_updates"].beginMap();
			update_it != update["agent_updates"].endMap();
			++update_it)
		{
			LLUUID agent_id(update_it->first);
			LLPointer<LLSpeaker> speakerp = findSpeaker(agent_id);

			LLSD agent_data = update_it->second;

			if (agent_data.isMap() && agent_data.has("transition"))
			{
				if (agent_data["transition"].asString() == "LEAVE" && speakerp.notNull())
				{
					speakerp->mStatus = LLSpeaker::STATUS_NOT_IN_CHANNEL;
					speakerp->mDotColor = INACTIVE_COLOR;
					speakerp->mActivityTimer.reset(SPEAKER_TIMEOUT);
				}
				else if (agent_data["transition"].asString() == "ENTER")
				{
					// add or update speaker
					speakerp = setSpeaker(agent_id);
				}
				else
				{
					llwarns << "bad membership list update " << ll_print_sd(agent_data["transition"]) << llendl;
				}
			}

			if (speakerp.isNull()) continue;

			// should have a valid speaker from this point on
			if (agent_data.isMap() && agent_data.has("info"))
			{
				LLSD agent_info = agent_data["info"];

				if (agent_info.has("is_moderator"))
				{
					speakerp->mIsModerator = agent_info["is_moderator"];
				}

				if (agent_info.has("mutes"))
				{
					speakerp->mModeratorMutedText = agent_info["mutes"]["text"];
				}
			}
		}
	}
	else if ( update.has("updates") && update["updates"].isMap() )
	{
		LLSD::map_const_iterator update_it;
		for (
			update_it = update["updates"].beginMap();
			update_it != update["updates"].endMap();
			++update_it)
		{
			LLUUID agent_id(update_it->first);
			LLPointer<LLSpeaker> speakerp = findSpeaker(agent_id);

			std::string agent_transition = update_it->second.asString();
			if (agent_transition == "LEAVE" && speakerp.notNull())
			{
				speakerp->mStatus = LLSpeaker::STATUS_NOT_IN_CHANNEL;
				speakerp->mDotColor = INACTIVE_COLOR;
				speakerp->mActivityTimer.reset(SPEAKER_TIMEOUT);
			}
			else if ( agent_transition == "ENTER")
			{
				// add or update speaker
				speakerp = setSpeaker(agent_id);
			}
			else
			{
				llwarns << "bad membership list update "
						<< agent_transition << llendl;
			}
		}
	}
}


//
// LLActiveSpeakerMgr
//

LLActiveSpeakerMgr::LLActiveSpeakerMgr() : LLSpeakerMgr(NULL)
{
}

void LLActiveSpeakerMgr::updateSpeakerList()
{
	// point to whatever the current voice channel is
	mVoiceChannel = LLVoiceChannel::getCurrentVoiceChannel();

	// always populate from active voice channel
	if (LLVoiceChannel::getCurrentVoiceChannel() != mVoiceChannel)
	{
		fireEvent(new LLSpeakerListChangeEvent(this, LLUUID::null), "clear");
		mSpeakers.clear();
		mSpeakersSorted.clear();
		mVoiceChannel = LLVoiceChannel::getCurrentVoiceChannel();
	}
	LLSpeakerMgr::updateSpeakerList();

	// clean up text only speakers
	for (speaker_map_t::iterator speaker_it = mSpeakers.begin(); speaker_it != mSpeakers.end(); ++speaker_it)
	{
		LLUUID speaker_id = speaker_it->first;
		LLSpeaker* speakerp = speaker_it->second;
		if (speakerp->mStatus == LLSpeaker::STATUS_TEXT_ONLY)
		{
			// automatically flag text only speakers for removal
			speakerp->mStatus = LLSpeaker::STATUS_NOT_IN_CHANNEL;
		}
	}

}



//
// LLLocalSpeakerMgr
//

LLLocalSpeakerMgr::LLLocalSpeakerMgr() : LLSpeakerMgr(LLVoiceChannelProximal::getInstance())
{
}

LLLocalSpeakerMgr::~LLLocalSpeakerMgr ()
{
}

void LLLocalSpeakerMgr::updateSpeakerList()
{
	// pull speakers from voice channel
	LLSpeakerMgr::updateSpeakerList();

	if (gDisconnected)//the world is cleared.
	{
		return ;
	}

	// pick up non-voice speakers in chat range
	std::vector<LLUUID> avatar_ids;
	std::vector<LLVector3d> positions;
	LLWorld::getInstance()->getAvatars(&avatar_ids, &positions, gAgent.getPositionGlobal(), CHAT_NORMAL_RADIUS);
	for(U32 i=0; i<avatar_ids.size(); i++)
	{
		setSpeaker(avatar_ids[i]);
	}

	// check if text only speakers have moved out of chat range
	for (speaker_map_t::iterator speaker_it = mSpeakers.begin(); speaker_it != mSpeakers.end(); ++speaker_it)
	{
		LLUUID speaker_id = speaker_it->first;
		LLSpeaker* speakerp = speaker_it->second;
		if (speakerp->mStatus == LLSpeaker::STATUS_TEXT_ONLY)
		{
			LLVOAvatar* avatarp = gObjectList.findAvatar(speaker_id);
			if (!avatarp || dist_vec(avatarp->getPositionAgent(), gAgent.getPositionAgent()) > CHAT_NORMAL_RADIUS)
			{
				speakerp->mStatus = LLSpeaker::STATUS_NOT_IN_CHANNEL;
				speakerp->mDotColor = INACTIVE_COLOR;
				speakerp->mActivityTimer.reset(SPEAKER_TIMEOUT);
			}
		}
	}
}
