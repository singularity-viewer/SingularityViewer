/** 
 * @file llnotify.h
 * @brief Non-blocking notification that doesn't take keyboard focus.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLNOTIFY_H
#define LL_LLNOTIFY_H

#include "llfontgl.h"
#include "llpanel.h"
#include "lleventtimer.h"
#include "llnotifications.h"
#include <vector>

class LLButton;
class LLNotifyBoxTemplate;
class LLTextEditor;

// NotifyBox - for notifications that require a response from the user.  
class LLNotifyBox : 
	public LLPanel, 
	public LLEventTimer,
	public LLInitClass<LLNotifyBox>,
	public LLInstanceTracker<LLNotifyBox, LLUUID>
{
public:
	typedef void (*notify_callback_t)(S32 option, void* data);
	typedef std::vector<std::string> option_list_t;

	static void initClass();
	static void destroyClass();

	BOOL isTip() const { return mIsTip; }
	BOOL isCaution() const { return mIsCaution; }
	/*virtual*/ void setVisible(BOOL visible);
	void stopAnimation() { mAnimating = FALSE; }

	void close();

	LLNotificationPtr getNotification() const { return mNotification; }

	static void format(std::string& msg, const LLStringUtil::format_map_t& args);

protected:
	LLNotifyBox(LLNotificationPtr notification, BOOL layout_script_dialog);

	/*virtual*/ ~LLNotifyBox();

	LLButton* addButton(std::string const &name, const std::string& label, BOOL is_option, BOOL is_default);
	
	/*virtual*/ BOOL handleMouseUp(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);

	// Animate as sliding onto the screen.
	/*virtual*/ void draw();
	/*virtual*/ BOOL tick();

	void moveToBack(bool getfocus = false);

	// Returns the rect, relative to gNotifyView, where this
	// notify box should be placed.
	static LLRect getNotifyRect(S32 num_options, BOOL layout_script_dialog, BOOL is_caution);
	static LLRect getNotifyTipRect(const std::string &message);

	// internal handler for button being clicked
	static void onClickButton(void* data);

	// for "next" button
	static void onClickNext(void* data);

	//static LLNotifyBox* findExistingNotify(LLPointer<LLNotifyBoxTemplate> notify_template, const LLString::format_map_t& args);

private:
	static bool onNotification(const LLSD& notify);
	void drawBackground() const;

protected:
	std::string mMessage;

	LLTextEditor *mUserInputBox;

	LLNotificationPtr mNotification;
	BOOL mIsTip;
	BOOL mIsCaution; // is this a caution notification?
	BOOL mAnimating; // Are we sliding onscreen?

	// Time since this notification was displayed.
	// This is an LLTimer not a frame timer because I am concerned
	// that I could be out-of-sync by one frame in the animation.
	LLTimer mAnimateTimer;

	LLButton* mNextBtn;

	S32 mNumOptions;
	S32 mNumButtons;
	BOOL mAddedDefaultBtn;

	BOOL mLayoutScriptDialog;

	// Used for callbacks
	struct InstanceAndS32
	{
		LLNotifyBox* mSelf;
		std::string	mButtonName;
	};
	std::vector<InstanceAndS32*> mBtnCallbackData;

	static S32 sNotifyBoxCount;
	static const LLFontGL* sFont;
	static const LLFontGL* sFontSmall;

	typedef std::map<std::string, LLNotifyBox*> unique_map_t;
	static unique_map_t sOpenUniqueNotifyBoxes;
};

class LLNotifyBoxView : public LLUICtrl
{
public:
	LLNotifyBoxView(const std::string& name, const LLRect& rect, BOOL mouse_opaque, U32 follows=FOLLOWS_NONE);
	void showOnly(LLView * ctrl);
	LLNotifyBox * getFirstNontipBox() const;

	class Matcher
	{
	public: 
		Matcher(){}
		virtual ~Matcher() {}
		virtual BOOL matches(const LLNotificationPtr) const = 0;
	};
	// Walks the list and removes any stacked messages for which the given matcher returns TRUE.
	// Useful when muting people and things in order to clear out any similar previously queued messages.
	void purgeMessagesMatching(const Matcher& matcher);

private:
	bool isGroupNotifyBox(const LLView* view) const ;
};

// This view contains the stack of notification windows.
extern LLNotifyBoxView* gNotifyBoxView;

#endif
