/** 
 * @file llgroupnotify.cpp
 * @brief Non-blocking notification that doesn't take keyboard focus.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llgroupnotify.h"

#include "llfocusmgr.h"

#include "llbutton.h"
#include "lliconctrl.h"
#include "llfloaterchat.h"	// for add_chat_history()
#include "llnotify.h"
#include "lltextbox.h"
#include "llviewertexteditor.h"
#include "lluiconstants.h"
#include "llui.h"
#include "llviewercontrol.h"
#include "llfloatergroupinfo.h"
#include "llinventoryicon.h"
#include "llinventory.h"

#include "llglheaders.h"
#include "llagent.h"
// Globals
//LLView* gGroupNotifyBoxView = NULL;

const F32 ANIMATION_TIME = 0.333f;

S32 LLGroupNotifyBox::sGroupNotifyBoxCount = 0;


//---------------------------------------------------------------------------
// LLGroupNotifyBox
//---------------------------------------------------------------------------
bool is_openable(LLAssetType::EType type)
{
	switch(type)
	{
	case LLAssetType::AT_LANDMARK:
	case LLAssetType::AT_NOTECARD:
	case LLAssetType::AT_IMAGE_JPEG:
	case LLAssetType::AT_IMAGE_TGA:
	case LLAssetType::AT_TEXTURE:
	case LLAssetType::AT_TEXTURE_TGA:
		return true;
	default:
		return false;
	}
}

LLGroupNotifyBox::LLGroupNotifyBox(const std::string& subject,
							 const std::string& message,
							 const std::string& from_name,
							 const LLUUID& group_id,
							 const LLUUID& group_insignia,
							 const std::string& group_name,
							 const LLDate& time_stamp,
							 const bool& has_inventory,
							 const std::string& inventory_name,
							 const LLSD& inventory_offer)
:	LLPanel("groupnotify", LLGroupNotifyBox::getGroupNotifyRect(), BORDER_YES),
	mAnimating(TRUE),
	mTimer(),
	mGroupID(group_id),
	mHasInventory(has_inventory),
	mInventoryOffer(NULL)
{
	const S32 VPAD = 2;
	const S32 TOP = getRect().getHeight() - 32; // Get past the top menu bar
	const S32 BOTTOM_PAD = VPAD * 2;
	const S32 BTN_TOP = BOTTOM_PAD + BTN_HEIGHT + VPAD;
	const S32 RIGHT = getRect().getWidth() - HPAD - HPAD;
	const S32 LINE_HEIGHT = 16;

	const S32 LABEL_WIDTH = 64;
	const S32 ICON_WIDTH = 64;

  	time_t timestamp = (time_t)time_stamp.secondsSinceEpoch();
 	if (!timestamp) time(&timestamp);

	std::string time_buf;
	timeToFormattedString(timestamp, gSavedSettings.getString("TimestampFormat"), time_buf);

	if (mHasInventory)
	{
		mInventoryOffer = new LLOfferInfo(inventory_offer);
	}

	setFocusRoot(TRUE);
	setFollows(FOLLOWS_TOP|FOLLOWS_RIGHT);
	setBackgroundVisible(TRUE);
	setBackgroundOpaque(TRUE);
	setBackgroundColor( gColors.getColor("GroupNotifyBoxColor") );

	LLIconCtrl* icon;
	LLTextEditor* text;

	S32 y = TOP;
	S32 x = HPAD + HPAD;

	class NoticeText : public LLTextBox
	{
	public:
		NoticeText(const std::string& name, const LLRect& rect, const std::string& text = LLStringUtil::null, const LLFontGL* font = NULL) 
			: LLTextBox(name, rect, text, font)
		{
			setHAlign(LLFontGL::RIGHT);
			setFontStyle(LLFontGL::DROP_SHADOW_SOFT);
			setBorderVisible(FALSE);
			setColor( gColors.getColor("GroupNotifyTextColor") );
			setBackgroundColor( gColors.getColor("GroupNotifyBoxColor") );
		}
	};

	// Title
	addChild(new NoticeText(std::string("title"),LLRect(x,y,RIGHT - HPAD,y - LINE_HEIGHT),std::string("Group Notice"),LLFontGL::getFontSansSerifHuge()));

	y -= llfloor(1.5f*LINE_HEIGHT);

	x += HPAD + HPAD + ICON_WIDTH;

	std::stringstream from;
	from << "Sent by " << from_name << ", " << group_name;

	addChild(new NoticeText(std::string("group"),LLRect(x,y,RIGHT - HPAD,y - LINE_HEIGHT),from.str(),LLFontGL::getFontSansSerif()));
	
	y -= (LINE_HEIGHT + VPAD);
	x = HPAD + HPAD;

	// TODO: change this to be the group icon.
	if (!group_insignia.isNull())
	{
		icon = new LLIconCtrl(std::string("icon"),
							  LLRect(x, y, x+ICON_WIDTH, y-ICON_WIDTH),
							  group_insignia);
	}
	else
	{
		icon = new LLIconCtrl(std::string("icon"),
							  LLRect(x, y, x+ICON_WIDTH, y-ICON_WIDTH),
							  std::string("notify_box_icon.tga"));
	}

	icon->setMouseOpaque(FALSE);
	addChild(icon);

	x += HPAD + HPAD + ICON_WIDTH;
	// If we have inventory with this message, leave room for the name.
	S32 box_bottom = BTN_TOP + (mHasInventory ? (LINE_HEIGHT + 2*VPAD) : 0);

	text = new LLViewerTextEditor(std::string("box"),
		LLRect(x, y, RIGHT, box_bottom),
		DB_GROUP_NOTICE_MSG_STR_LEN,
		LLStringUtil::null,
		LLFontGL::getFontSansSerif(),
		FALSE);

	static const LLStyleSP headerstyle(new LLStyle(true,LLColor4::black,"SansSerifBig"));
	static const LLStyleSP datestyle(new LLStyle(true,LLColor4::black,"serif"));

	text->appendStyledText(subject + "\n",false,false,headerstyle);

	text->appendStyledText(time_buf,false,false,datestyle);
	// Sadly, our LLTextEditor can't handle both styled and unstyled text
	// at the same time.  Hence this space must be styled. JC
	text->appendColoredText(std::string(" "),false,false,LLColor4::grey4);
	text->setParseHTML(TRUE);
	text->appendColoredText(std::string("\n\n") + message,false,false,LLColor4::grey4);

	LLColor4 semi_transparent(1.0f,1.0f,1.0f,0.8f);
	text->setCursor(0,0);
	text->setEnabled(FALSE);
	text->setWordWrap(TRUE);
	//text->setTabStop(FALSE); // was interfering with copy-and-paste
	text->setTabsToNextField(TRUE);
	text->setMouseOpaque(TRUE);
	text->setBorderVisible(TRUE);
	text->setTakesNonScrollClicks(TRUE);
	text->setHideScrollbarForShortDocs(TRUE);
	text->setReadOnlyBgColor ( semi_transparent );
	text->setWriteableBgColor ( semi_transparent );
	
	addChild(text);

	y = box_bottom - VPAD;

	if (mHasInventory)
	{
			addChild(new NoticeText(std::string("subjecttitle"),LLRect(x,y,x + LABEL_WIDTH,y - LINE_HEIGHT),std::string("Attached: "),LLFontGL::getFontSansSerif()));

			LLUIImagePtr item_icon = LLInventoryIcon::getIcon(mInventoryOffer->mType,
													LLInventoryType::IT_TEXTURE,
													0, FALSE);


			x += LABEL_WIDTH + HPAD;

			std::stringstream ss;
			ss << "        " << inventory_name;
			LLTextBox *line = new LLTextBox(std::string("object_name"),LLRect(x,y,RIGHT - HPAD,y - LINE_HEIGHT),ss.str(),LLFontGL::getFontSansSerif());
			line->setEnabled(FALSE);
			line->setBorderVisible(TRUE);
			line->setDisabledColor(LLColor4::blue4);
			line->setFontStyle(LLFontGL::NORMAL);
			line->setBackgroundVisible(true);
			line->setBackgroundColor( semi_transparent );
			addChild(line);

			icon = new LLIconCtrl(std::string("icon"),
									LLRect(x, y, x+16, y-16),
									item_icon->getName());
			icon->setMouseOpaque(FALSE);
			addChild(icon);
	}

	LLButton* btn;
	btn = new LLButton(std::string("next"),
				LLRect(getRect().getWidth()-26, BOTTOM_PAD + 20, getRect().getWidth()-2, BOTTOM_PAD),
				std::string("notify_next.png"),
				std::string("notify_next.png"),
				LLStringUtil::null,
				onClickNext,
				this,
				LLFontGL::getFontSansSerif());
	btn->setToolTip(std::string("Next")); // *TODO: Translate
	btn->setScaleImage(TRUE);
	addChild(btn);
	mNextBtn = btn;

	S32 btn_width = 80;
	S32 wide_btn_width = 120;
	LLRect btn_rect;
	x = 3 * HPAD;

	btn_rect.setOriginAndSize(x,
								BOTTOM_PAD,
								btn_width,
								BTN_HEIGHT);

	btn = new LLButton(std::string("OK"), btn_rect, LLStringUtil::null, onClickOk, this);
	addChild(btn, -1);
	setDefaultBtn(btn);

	x += btn_width + HPAD;

	
	btn_rect.setOriginAndSize(x,
								BOTTOM_PAD,
								wide_btn_width,
								BTN_HEIGHT);

	btn = new LLButton(std::string("Group Notices"), btn_rect, LLStringUtil::null, onClickGroupInfo, this);
	btn->setToolTip(std::string("View past notices or opt-out of receiving these messages here.")); // TODO: Translate
	addChild(btn, -1);

	if (mHasInventory)
	{
		x += wide_btn_width + HPAD;

		btn_rect.setOriginAndSize(x,
									BOTTOM_PAD,
									wide_btn_width,
									BTN_HEIGHT);

		std::string btn_lbl("");
		if(is_openable(mInventoryOffer->mType))
		{
			btn_lbl = "Open Attachment";
		}
		else
		{
			btn_lbl = "Save Attachment";
		}
		mSaveInventoryBtn = new LLButton(btn_lbl, btn_rect, LLStringUtil::null, onClickSaveInventory, this);
		mSaveInventoryBtn->setVisible(mHasInventory);
		addChild(mSaveInventoryBtn);
	}

	sGroupNotifyBoxCount++;

	// If this is the only notify box, don't show the next button
	if (sGroupNotifyBoxCount == 1)
	{
		mNextBtn->setVisible(FALSE);
	}
}


// virtual
LLGroupNotifyBox::~LLGroupNotifyBox()
{
	sGroupNotifyBoxCount--;
}

// virtual
BOOL LLGroupNotifyBox::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	moveToBack();
	return TRUE;
}


// virtual
void LLGroupNotifyBox::draw()
{
	F32 display_time = mTimer.getElapsedTimeF32();

	if (mAnimating && display_time < ANIMATION_TIME)
	{
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();

		S32 height = getRect().getHeight();
		F32 fraction = display_time / ANIMATION_TIME;
		F32 voffset = (1.f - fraction) * height;

		glTranslatef(0.f, voffset, 0.f);

		LLPanel::draw();

		glPopMatrix();
	}
	else
	{
		mAnimating = FALSE;
		LLPanel::draw();
	}
}


void LLGroupNotifyBox::close()
{
	// The group notice dialog may be an inventory offer.
	// If it has an inventory save button and that button is still enabled
	// Then we need to send the inventory declined message
	if(mHasInventory)
	{
		mInventoryOffer->forceResponse(IOR_DECLINE);
		mInventoryOffer = NULL;
		mHasInventory = FALSE;
	}
	gNotifyBoxView->removeChild(this);

	die();
}

//static 
void LLGroupNotifyBox::initClass()
{
	LLNotificationChannel::buildChannel("Group Notifications", "Visible", LLNotificationFilters::filterBy<std::string>(&LLNotification::getType, "groupnotify"));
	LLNotifications::instance().getChannel("Group Notifications")->connectChanged(&LLGroupNotifyBox::onNewNotification);
}

//static 
bool LLGroupNotifyBox::onNewNotification(const LLSD& notify)
{
	LLNotificationPtr notification = LLNotifications::instance().find(notify["id"].asUUID());

	if (notification)
	{
		const LLSD& payload = notification->getPayload();
		// Get the group data
		LLGroupData group_data;
		if (!gAgent.getGroupData(payload["group_id"].asUUID(),group_data))
		{
			llwarns << "Group notice for unkown group: " << payload["group_id"].asUUID() << llendl;
			return false;
		}

		LLGroupNotifyBox* self;
		self = new LLGroupNotifyBox(payload["subject"].asString(),
									payload["message"].asString(),
									payload["sender_name"].asString(), 
									payload["group_id"].asUUID(), 
									group_data.mInsigniaID, 
									group_data.mName,
									notification->getDate(),
									payload["inventory_offer"].isDefined(),
									payload["inventory_name"].asString(),
									payload["inventory_offer"]);
		gNotifyBoxView->addChild(self);
	}
	return false;
}

void LLGroupNotifyBox::moveToBack()
{
	// Move this dialog to the back.
	gNotifyBoxView->removeChild(this);
	gNotifyBoxView->addChildAtEnd(this);
	mNextBtn->setVisible(FALSE);

	// And enable the next button on the frontmost one, if there is one
	if (sGroupNotifyBoxCount > 1)
	{
		LLView* view = gNotifyBoxView->getFirstChild();
		
		if(view && "groupnotify" == view->getName())
		{
			LLGroupNotifyBox* front = (LLGroupNotifyBox*)view;
		
			if(front->mNextBtn)
			{
				front->mNextBtn->setVisible(TRUE);
			}
		}
	}
}


// static
LLRect LLGroupNotifyBox::getGroupNotifyRect()
{
	S32 notify_height = gSavedSettings.getS32("GroupNotifyBoxHeight");
	const S32 NOTIFY_WIDTH = gSavedSettings.getS32("GroupNotifyBoxWidth");

	const S32 TOP = gNotifyBoxView->getRect().getHeight();
	const S32 RIGHT = gNotifyBoxView->getRect().getWidth();
	const S32 LEFT = RIGHT - NOTIFY_WIDTH;

	return LLRect(LEFT, TOP, RIGHT, TOP-notify_height);
}


// static
void LLGroupNotifyBox::onClickOk(void* data)
{
	LLGroupNotifyBox* self = (LLGroupNotifyBox*)data;
	if (self) self->close();
}

void LLGroupNotifyBox::onClickGroupInfo(void* data)
{
	LLGroupNotifyBox* self = (LLGroupNotifyBox*)data;

	if (self)
	{
		LLFloaterGroupInfo::showFromUUID(self->mGroupID, "notices_tab");
	}

	//Leave notice open until explicitly closed
}

void LLGroupNotifyBox::onClickSaveInventory(void* data)
{
	LLGroupNotifyBox* self = (LLGroupNotifyBox*)data;

	self->mInventoryOffer->forceResponse(IOR_ACCEPT);

	self->mInventoryOffer = NULL;
	self->mHasInventory = FALSE;

	// Each item can only be received once, so disable the button.
	self->mSaveInventoryBtn->setEnabled(FALSE);
}

// static
void LLGroupNotifyBox::onClickNext(void* data)
{
	LLGroupNotifyBox* self = (LLGroupNotifyBox*)data;
	self->moveToBack();
}
