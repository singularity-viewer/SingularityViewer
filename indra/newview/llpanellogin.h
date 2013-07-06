/** 
 * @file llpanellogin.h
 * @brief Login username entry fields.
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

#ifndef LL_LLPANELLOGIN_H
#define LL_LLPANELLOGIN_H

#include "llpanel.h"
#include "llmemory.h"			// LLPointer<>
#include "llmediactrl.h"	// LLMediaCtrlObserver
#include "llsavedlogins.h"
#include "llslurl.h"

class LLUIImage;
class LLComboBox;
class HippoGridInfo;

class LLPanelLogin:	
	public LLPanel,
	public LLViewerMediaObserver
{
	LOG_CLASS(LLPanelLogin);
public:
	LLPanelLogin(const LLRect &rect,
				void (*callback)(S32 option, void* user_data),
				void *callback_data);
	~LLPanelLogin();

	virtual BOOL handleKeyHere(KEY key, MASK mask);
	virtual void draw();
	virtual void setFocus( BOOL b );

	static void show(const LLRect &rect, 
		void (*callback)(S32 option, void* user_data), 
		void* callback_data);

	// Remember password checkbox is set via gSavedSettings "RememberPassword"

	/**
	 * @brief	Set the values of the displayed fields.
	 * @param	firstname	First name value.
	 * @param	lastname	Last name value.
	 * @param	password	Password, as plaintext or munged.
	 * @param	is_secondlife True if First/Last refer to a SecondLife(tm) account.
	 */
	static void setFields(const std::string& firstname, const std::string& lastname,
						  const std::string& password);

	/**
	 * @brief	Set the values of the displayed fields from a populated history entry.
	 * @param	entry	History entry containing all necessary fields.
	 */
	static void setFields(const LLSavedLoginEntry& entry, bool takeFocus = false);

	static void getFields(std::string *firstname, std::string *lastname, std::string *password);

	static void setLocation(const LLSLURL& slurl);
	
	/// Call when preferences that control visibility may have changed
	static void updateLocationSelectorsVisibility();

	static void close();

	void setSiteIsAlive( bool alive );

	void updateGridCombo();

	void onCurGridChange(HippoGridInfo* new_grid, HippoGridInfo* old_grid);
	static void loadLoginPage();	
	static void refreshLoginPage();
	static void giveFocus();
	static void setAlwaysRefresh(bool refresh); 
	static void mungePassword(LLUICtrl* caller, void* user_data);
	
	// inherited from LLViewerMediaObserver
	/*virtual*/ void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event);

	/// to be called from LLStartUp::setStartSLURL
	static void onUpdateStartSLURL(const LLSLURL& new_start_slurl);

private:
	void reshapeBrowser();
	void onLocationSLURL();

	static void onClickConnect(void*);
	static void onClickNewAccount();
	static bool newAccountAlertCallback(const LLSD& notification, const LLSD& response);
	static void onClickGrids(void*);
	static void onSelectGrid(LLUICtrl *ctrl);
	static void onClickVersion(void*);
	static void onClickForgotPassword();
	static void onPassKey(LLLineEditor* caller);
	static void onSelectLoginEntry(LLUICtrl*, void*);
	void onLoginComboLostFocus(LLComboBox* combo_box);
	static void onNameCheckChanged(LLUICtrl* ctrl, void* data);
	static void clearPassword();

public:
	/**
	 * @brief	Set the login history data.
	 */
	static void setLoginHistory(LLSavedLogins const& login_history);

	/**
	 * @brief	Returns the login history data.
	 * @return	History data. It will be empty if the instance does not exist.
	 */
	static LLSavedLogins getLoginHistory()
	{
		return (sInstance ? sInstance->mLoginHistoryData : LLSavedLogins());
	}

	/**
	 * @brief	Returns the state of the "remember resident name" checkbox if it exists.
	 * @return	Checkbox state, or false if the instance is not instantiated.
	 */
	static bool getRememberLogin();

	//static void selectFirstElement(void);

private:
	LLPointer<LLUIImage> mLogoImage;

	void			(*mCallback)(S32 option, void *userdata);
	void*			mCallbackData;

	std::string mIncomingPassword;
	std::string mMungedPassword;

	static LLPanelLogin* sInstance;
	static BOOL		sCapslockDidNotification;

	LLSavedLogins	mLoginHistoryData;
};

std::string load_password_from_disk(void);
void save_password_to_disk(const char* hashed_password);

#endif
