/** 
 * @file llpanellogin.cpp
 * @brief Login dialog and logo display
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

#include "llpanellogin.h"

#include "llpanelgeneral.h"

#include "hippogridmanager.h"

#include "indra_constants.h"		// for key and mask constants
#include "llfontgl.h"
#include "llmd5.h"
#include "llsecondlifeurls.h"
#include "sgversion.h"
#include "v4color.h"

#include "llappviewer.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcommandhandler.h"		// for secondlife:///app/login/
#include "llcombobox.h"
#include "llviewercontrol.h"
#include "llfloaterabout.h"
#include "llfloatertest.h"
#include "llfloaterpreference.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llstartup.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluiconstants.h"
#include "llurlhistory.h" // OGPX : regionuri text box has a history of region uris (if FN/LN are loaded at startup)
#include "llviewerbuild.h"
#include "llviewertexturelist.h"
#include "llviewermenu.h"			// for handle_preferences()
#include "llviewernetwork.h"
#include "llviewerwindow.h"			// to link into child list
#include "llnotify.h"
#include "lluictrlfactory.h"
#include "llhttpclient.h"
#include "llweb.h"
#include "llmediactrl.h"

#include "llfloatertos.h"

#include "llglheaders.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

// <edit>
#include "llspinctrl.h"
#include "llviewermessage.h"
#include <boost/lexical_cast.hpp>
// </edit>
#include <boost/algorithm/string.hpp>
#include "llstring.h"
#include <cctype>

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy iamHereLogin_timeout;

const S32 BLACK_BORDER_HEIGHT = 160;
const S32 MAX_PASSWORD = 16;

LLPanelLogin *LLPanelLogin::sInstance = NULL;
BOOL LLPanelLogin::sCapslockDidNotification = FALSE;


static bool nameSplit(const std::string& full, std::string& first, std::string& last) {
	std::vector<std::string> fragments;
	boost::algorithm::split(fragments, full, boost::is_any_of(" ."));
	if (!fragments.size() || !fragments[0].length())
		return false;
	first = fragments[0];
	if (fragments.size() == 1)
	{
		if (gHippoGridManager->getCurrentGrid()->isAurora())
			last = "";
		else
			last = "Resident";
	}
	else
		last = fragments[1];
	return (fragments.size() <= 2);
}

static std::string nameJoin(const std::string& first,const std::string& last, bool strip_resident) {
	if (last.empty() || (strip_resident && boost::algorithm::iequals(last, "Resident")))
		return first;
	else {
		if(std::islower(last[0]))
			return first + "." + last;
		else
			return first + " " + last;
	}
}

static std::string getDisplayString(const std::string& first, const std::string& last, const std::string& grid, bool is_secondlife) {
	//grid comes via LLSavedLoginEntry, which uses full grid names, not nicks
	if(grid == gHippoGridManager->getDefaultGridName())	
		return nameJoin(first, last, is_secondlife);
	else
		return nameJoin(first, last, is_secondlife) + " (" + grid + ")";
}

static std::string getDisplayString(const LLSavedLoginEntry& entry) {
	return getDisplayString(entry.getFirstName(), entry.getLastName(), entry.getGrid(), entry.isSecondLife());
}

class LLLoginRefreshHandler : public LLCommandHandler
{
public:
	// don't allow from external browsers
	LLLoginRefreshHandler() : LLCommandHandler("login_refresh", UNTRUSTED_BLOCK) { }
	bool handle(const LLSD& tokens, const LLSD& query_map, LLMediaCtrl* web)
	{	
		if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
		{
			LLPanelLogin::loadLoginPage();
		}	
		return true;
	}
};

LLLoginRefreshHandler gLoginRefreshHandler;

// helper class that trys to download a URL from a web site and calls a method 
// on parent class indicating if the web server is working or not
class LLIamHereLogin : public LLHTTPClient::ResponderHeadersOnly
{
	private:
		LLIamHereLogin( LLPanelLogin* parent ) :
		   mParent( parent )
		{}

		LLPanelLogin* mParent;

	public:
		static boost::intrusive_ptr< LLIamHereLogin > build( LLPanelLogin* parent )
		{
			return boost::intrusive_ptr< LLIamHereLogin >( new LLIamHereLogin( parent ) );
		};

		virtual void  setParent( LLPanelLogin* parentIn )
		{
			mParent = parentIn;
		};

		/*virtual*/ void completedHeaders(U32 status, std::string const& reason, AIHTTPReceivedHeaders const& headers)
		{
			if (mParent)
			{
				if(200 <= status && status < 300)
					llinfos << "Found site" << llendl;
				mParent->setSiteIsAlive(200 <= status && status < 300);
			}
		}

		/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return iamHereLogin_timeout; }

		/*virtual*/ char const* getName(void) const { return "LLIamHereLogin"; }
};

// this is global and not a class member to keep crud out of the header file
namespace {
	boost::intrusive_ptr< LLIamHereLogin > gResponsePtr = 0;
};

//---------------------------------------------------------------------------
// Public methods
//---------------------------------------------------------------------------
LLPanelLogin::LLPanelLogin(const LLRect &rect,
						 void (*callback)(S32 option, void* user_data),
						 void *cb_data)
:	LLPanel(std::string("panel_login"), LLRect(0,600,800,0), FALSE),		// not bordered
	mLogoImage(),
	mCallback(callback),
	mCallbackData(cb_data)
{
	setFocusRoot(TRUE);

	setBackgroundVisible(FALSE);
	setBackgroundOpaque(TRUE);

	gViewerWindow->abortShowProgress();	//Kill previous instance. It might still be alive, and if so, its probably pending
										//deletion via the progressviews idle callback. Kill it now and unregister said idle callback.

	LLPanelLogin::sInstance = this;

	// add to front so we are the bottom-most child
	gViewerWindow->getRootView()->addChildInBack(this);

	// Logo
	mLogoImage = LLUI::getUIImage("startup_logo.j2c");

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_login.xml");
	
	reshape(rect.getWidth(), rect.getHeight());

	LLComboBox* name_combo = sInstance->getChild<LLComboBox>("name_combo");
	name_combo->setCommitCallback(boost::bind(LLPanelLogin::onSelectLoginEntry, _1, this));
	name_combo->setFocusLostCallback(boost::bind(&LLPanelLogin::onLoginComboLostFocus, this, name_combo));
	name_combo->setPrevalidate(LLLineEditor::prevalidatePrintableNotPipe);
	name_combo->setSuppressTentative(true);
	name_combo->setSuppressAutoComplete(true);

	childSetCommitCallback("remember_name_check", onNameCheckChanged);

	LLLineEditor* password_edit(getChild<LLLineEditor>("password_edit"));
	password_edit->setKeystrokeCallback(onPassKey);
	// STEAM-14: When user presses Enter with this field in focus, initiate login
	password_edit->setCommitCallback(mungePassword, this);
	password_edit->setDrawAsterixes(TRUE);

	// change z sort of clickable text to be behind buttons
	sendChildToBack(getChildView("channel_text"));
	sendChildToBack(getChildView("forgot_password_text"));

	//llinfos << " url history: " << LLSDOStreamer<LLSDXMLFormatter>(LLURLHistory::getURLHistory("regionuri")) << llendl;

	LLComboBox* location_combo = getChild<LLComboBox>("start_location_combo");
	updateLocationSelectorsVisibility(); // separate so that it can be called from preferences
	location_combo->setAllowTextEntry(TRUE, 128, FALSE);
	location_combo->setFocusLostCallback( boost::bind(&LLPanelLogin::onLocationSLURL, this) );
	
	LLComboBox *server_choice_combo = getChild<LLComboBox>("grids_combo");
	server_choice_combo->setCommitCallback(boost::bind(&LLPanelLogin::onSelectGrid, _1));
	
	// Load all of the grids, sorted, and then add a bar and the current grid at the top
	updateGridCombo();

	LLSLURL start_slurl(LLStartUp::getStartSLURL());
	if ( !start_slurl.isSpatial() ) // has a start been established by the command line or NextLoginLocation ? 
	{
		// no, so get the preference setting
		std::string defaultStartLocation = gSavedSettings.getString("LoginLocation");
		LL_INFOS("AppInit")<<"default LoginLocation '"<<defaultStartLocation<<"'"<<LL_ENDL;
		LLSLURL defaultStart(defaultStartLocation);
		if ( defaultStart.isSpatial() )
		{
			LLStartUp::setStartSLURL(defaultStart);	// calls onUpdateStartSLURL
		}
		else
		{
			LL_INFOS("AppInit")<<"no valid LoginLocation, using home"<<LL_ENDL;
			LLSLURL homeStart(LLSLURL::SIM_LOCATION_HOME);
			LLStartUp::setStartSLURL(homeStart); // calls onUpdateStartSLURL
		}
	}
	else
	{
		LLPanelLogin::onUpdateStartSLURL(start_slurl); // updates grid if needed
	}

	childSetAction("connect_btn", onClickConnect, this);

	setDefaultBtn("connect_btn");

	childSetAction("grids_btn", onClickGrids, this);

	std::string channel = gVersionChannel;

	std::string version = llformat("%d.%d.%d (%d)",
		gVersionMajor,
		gVersionMinor,
		gVersionPatch,
		LL_VIEWER_BUILD );
	LLTextBox* channel_text = getChild<LLTextBox>("channel_text");
	channel_text->setTextArg("[CHANNEL]", channel); // though not displayed
	channel_text->setTextArg("[VERSION]", version);
	channel_text->setClickedCallback(boost::bind(&LLPanelLogin::onClickVersion,(void*)NULL));
	
	LLTextBox* forgot_password_text = getChild<LLTextBox>("forgot_password_text");
	forgot_password_text->setClickedCallback(boost::bind(&onClickForgotPassword));

	
	LLTextBox* create_new_account_text = getChild<LLTextBox>("create_new_account_text");
	create_new_account_text->setClickedCallback(boost::bind(&onClickNewAccount));

	// get the web browser control
	LLMediaCtrl* web_browser = getChild<LLMediaCtrl>("login_html");
	web_browser->addObserver(this);
	web_browser->setBackgroundColor(LLColor4::black);

	reshapeBrowser();

	refreshLoginPage();

	gHippoGridManager->setCurrentGridChangeCallback(boost::bind(&LLPanelLogin::onCurGridChange,this,_1,_2));
}

void LLPanelLogin::setSiteIsAlive( bool alive )
{
	LLMediaCtrl* web_browser = getChild<LLMediaCtrl>("login_html");
	// if the contents of the site was retrieved
	if ( alive )
	{
		if ( web_browser )
		{
			loadLoginPage();
						
			web_browser->setVisible(true);
		}
	}
	else
	// the site is not available (missing page, server down, other badness)
	{
		if ( web_browser )
		{
			// hide browser control (revealing default one)
			web_browser->setVisible( FALSE );
			web_browser->navigateTo( "data:text/html,%3Chtml%3E%3Cbody%20bgcolor=%22#000000%22%3E%3C/body%3E%3C/html%3E", "text/html" );
		}
	}
}

void LLPanelLogin::mungePassword(LLUICtrl* caller, void* user_data)
{
	LLPanelLogin* self = (LLPanelLogin*)user_data;
	LLLineEditor* editor = (LLLineEditor*)caller;
	std::string password = editor->getText();

	// Re-md5 if we've changed at all
	if (password != self->mIncomingPassword)
	{
		LLMD5 pass((unsigned char *)password.c_str());
		char munged_password[MD5HEX_STR_SIZE];
		pass.hex_digest(munged_password);
		self->mMungedPassword = munged_password;
	}
}

// force the size to be correct (XML doesn't seem to be sufficient to do this)
// (with some padding so the other login screen doesn't show through)
void LLPanelLogin::reshapeBrowser()
{
	LLMediaCtrl* web_browser = getChild<LLMediaCtrl>("login_html");
	LLRect rect = gViewerWindow->getWindowRectScaled();
	LLRect html_rect;
	html_rect.setCenterAndSize(
	rect.getCenterX() /*- 2*/, rect.getCenterY() + 40,
	rect.getWidth() /*+ 6*/, rect.getHeight() - 78 );
	web_browser->setRect( html_rect );
	web_browser->reshape( html_rect.getWidth(), html_rect.getHeight(), TRUE );
	reshape( rect.getWidth(), rect.getHeight(), 1 );
}

LLPanelLogin::~LLPanelLogin()
{
	LLPanelLogin::sInstance = NULL;

	// tell the responder we're not here anymore
	if ( gResponsePtr )
		gResponsePtr->setParent( 0 );

	if ( gFocusMgr.getDefaultKeyboardFocus() == this )
	{
		gFocusMgr.setDefaultKeyboardFocus(NULL);
	}
}

void LLPanelLogin::setLoginHistory(LLSavedLogins const& login_history)
{
	sInstance->mLoginHistoryData = login_history;

	LLComboBox* login_combo = sInstance->getChild<LLComboBox>("name_combo");
	llassert(login_combo);
	login_combo->clear();

	LLSavedLoginsList const& saved_login_entries(login_history.getEntries());
	for (LLSavedLoginsList::const_reverse_iterator i = saved_login_entries.rbegin();
	     i != saved_login_entries.rend(); ++i)
	{
		LLSD e = i->asLLSD();
		if (e.isMap() && gHippoGridManager->getGrid(i->getGrid()))
			login_combo->add(getDisplayString(*i), e);
	}
}

// virtual
void LLPanelLogin::draw()
{
	gGL.pushMatrix();
	{
		F32 image_aspect = 1.333333f;
		F32 view_aspect = (F32)getRect().getWidth() / (F32)getRect().getHeight();
		// stretch image to maintain aspect ratio
		if (image_aspect > view_aspect)
		{
			gGL.translatef(-0.5f * (image_aspect / view_aspect - 1.f) * getRect().getWidth(), 0.f, 0.f);
			gGL.scalef(image_aspect / view_aspect, 1.f, 1.f);
		}

		S32 width = getRect().getWidth();
		S32 height = getRect().getHeight();

		if ( getChild<LLView>("login_html")->getVisible())
		{
			// draw a background box in black
			gl_rect_2d( 0, height - 264, width, 264, LLColor4::black );
			// draw the bottom part of the background image
			// just the blue background to the native client UI
			mLogoImage->draw(0, -264, width + 8, mLogoImage->getHeight());
		}
		else
		{
			// the HTML login page is not available so default to the original screen
			S32 offscreen_part = height / 3;
			mLogoImage->draw(0, -offscreen_part, width, height+offscreen_part);
		};
	}
	gGL.popMatrix();

	LLPanel::draw();
}

// virtual
BOOL LLPanelLogin::handleKeyHere(KEY key, MASK mask)
{
	if (( KEY_RETURN == key ) && (MASK_ALT == mask))
	{
		gViewerWindow->toggleFullscreen(FALSE);
		return TRUE;
	}

	if (('P' == key) && (MASK_CONTROL == mask))
	{
		LLFloaterPreference::show(NULL);
		return TRUE;
	}

	if (('T' == key) && (MASK_CONTROL == mask))
	{
		new LLFloaterSimple("floater_test.xml");
		return TRUE;
	}
	
	//Singu TODO: Re-implement f1 help.
	/*if ( KEY_F1 == key )
	{
		llinfos << "Spawning HTML help window" << llendl;
		gViewerHtmlHelp.show();
		return TRUE;
	}*/

# if !LL_RELEASE_FOR_DOWNLOAD
	if ( KEY_F2 == key )
	{
		llinfos << "Spawning floater TOS window" << llendl;
		LLFloaterTOS* tos_dialog = LLFloaterTOS::show(LLFloaterTOS::TOS_TOS,"");
		tos_dialog->startModal();
		return TRUE;
	}
#endif

	if (KEY_RETURN == key && MASK_NONE == mask)
	{
		// let the panel handle UICtrl processing: calls onClickConnect()
		return LLPanel::handleKeyHere(key, mask);
	}

	return LLPanel::handleKeyHere(key, mask);
}

// virtual 
void LLPanelLogin::setFocus(BOOL b)
{
	if(b != hasFocus())
	{
		if(b)
		{
			LLPanelLogin::giveFocus();
		}
		else
		{
			LLPanel::setFocus(b);
		}
	}
}

// static
void LLPanelLogin::giveFocus()
{
	if( sInstance )
	{
		// Grab focus and move cursor to first blank input field
		std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();
		std::string pass = sInstance->getChild<LLUICtrl>("password_edit")->getValue().asString();

		BOOL have_username = !username.empty();
		BOOL have_pass = !pass.empty();

		LLLineEditor* edit = NULL;
		LLComboBox* combo = NULL;
		if (have_username && !have_pass)
		{
			// User saved his name but not his password.  Move
			// focus to password field.
			edit = sInstance->getChild<LLLineEditor>("password_edit");
		}
		else
		{
			// User doesn't have a name, so start there.
			combo = sInstance->getChild<LLComboBox>("username_combo");
		}

		if (edit)
		{
			edit->setFocus(TRUE);
			edit->selectAll();
		}
		else if (combo)
		{
			combo->setFocus(TRUE);
		}
	}
}


// static
void LLPanelLogin::show(const LLRect &rect,
						void (*callback)(S32 option, void* user_data),
						void* callback_data)
{
	new LLPanelLogin(rect, callback, callback_data);

	if( !gFocusMgr.getKeyboardFocus() )
	{
		// Grab focus and move cursor to first enabled control
		sInstance->setFocus(TRUE);
	}

	// Make sure that focus always goes here (and use the latest sInstance that was just created)
	gFocusMgr.setDefaultKeyboardFocus(sInstance);
}

// static
void LLPanelLogin::setFields(const std::string& firstname,
			     const std::string& lastname,
			     const std::string& password)
{
	if (!sInstance)
	{
		llwarns << "Attempted fillFields with no login view shown" << llendl;
		return;
	}

	LLComboBox* login_combo = sInstance->getChild<LLComboBox>("name_combo");

	llassert_always(firstname.find(' ') == std::string::npos);
	login_combo->setLabel(nameJoin(firstname, lastname, false));

	// Max "actual" password length is 16 characters.
	// Hex digests are always 32 characters.
	if (password.length() == 32)
	{
		// This is a MD5 hex digest of a password.
		// We don't actually use the password input field, 
		// fill it with MAX_PASSWORD characters so we get a 
		// nice row of asterixes.
		const std::string filler("123456789!123456");
		sInstance->childSetText("password_edit", filler);
		sInstance->mIncomingPassword = filler;
		sInstance->mMungedPassword = password;
	}
	else
	{
		// this is a normal text password
		sInstance->childSetText("password_edit", password);
		sInstance->mIncomingPassword = password;
		LLMD5 pass((unsigned char *)password.c_str());
		char munged_password[MD5HEX_STR_SIZE];
		pass.hex_digest(munged_password);
		sInstance->mMungedPassword = munged_password;
	}
}

// static
void LLPanelLogin::setFields(const LLSavedLoginEntry& entry, bool takeFocus)
{
	if (!sInstance)
	{
		llwarns << "Attempted setFields with no login view shown" << llendl;
		return;
	}

	LLCheckBoxCtrl* remember_pass_check = sInstance->getChild<LLCheckBoxCtrl>("remember_check");
	std::string fullname = nameJoin(entry.getFirstName(), entry.getLastName(), entry.isSecondLife()); 
	LLComboBox* login_combo = sInstance->getChild<LLComboBox>("name_combo");
	login_combo->setTextEntry(fullname);
	login_combo->resetTextDirty();
	//sInstance->childSetText("name_combo", fullname);

	std::string grid = entry.getGrid();
	//grid comes via LLSavedLoginEntry, which uses full grid names, not nicks
	if(!grid.empty() && gHippoGridManager->getGrid(grid) && grid != gHippoGridManager->getCurrentGridName())
	{
		gHippoGridManager->setCurrentGrid(grid);
	}
	
	if (entry.getPassword().empty())
	{
		sInstance->childSetText("password_edit", std::string(""));
		remember_pass_check->setValue(LLSD(false));
	}
	else
	{
		const std::string filler("123456789!123456");
		sInstance->childSetText("password_edit", filler);
		sInstance->mIncomingPassword = filler;
		sInstance->mMungedPassword = entry.getPassword();
		remember_pass_check->setValue(LLSD(true));
	}

	if (takeFocus)
	{
		giveFocus();
	}
}

// static
void LLPanelLogin::getFields(std::string *firstname,
			     std::string *lastname,
			     std::string *password)
{
	if (!sInstance)
	{
		llwarns << "Attempted getFields with no login view shown" << llendl;
		return;
	}
	
	nameSplit(sInstance->getChild<LLComboBox>("name_combo")->getTextEntry(), *firstname, *lastname);
	LLStringUtil::trim(*firstname);
	LLStringUtil::trim(*lastname);
	
	*password = sInstance->mMungedPassword;
}

// static
/*void LLPanelLogin::getLocation(std::string &location)
{
	if (!sInstance)
	{
		llwarns << "Attempted getLocation with no login view shown" << llendl;
		return;
	}
	
	LLComboBox* combo = sInstance->getChild<LLComboBox>("start_location_combo");
	location = combo->getValue().asString();
}*/

// static
void LLPanelLogin::updateLocationSelectorsVisibility()
{
	if (sInstance) 
	{
		BOOL show_start = gSavedSettings.getBOOL("ShowStartLocation");

// [RLVa:KB] - Alternate: Snowglobe-1.2.4 | Checked: 2009-07-08 (RLVa-1.0.0e)
	// TODO-RLVa: figure out some way to make this work with RLV_EXTENSION_STARTLOCATION
	#ifndef RLV_EXTENSION_STARTLOCATION
		if (rlv_handler_t::isEnabled())
		{
			show_start = FALSE;
		}
	#endif // RLV_EXTENSION_STARTLOCATION
// [/RLVa:KB]

		sInstance->getChildView("location_panel")->setVisible(show_start);
	
		bool show_server = true;
		sInstance->getChildView("grids_panel")->setVisible(show_server);
	}
	
}

// static
void LLPanelLogin::onUpdateStartSLURL(const LLSLURL& new_start_slurl)
{
	if (!sInstance) return;

	LL_DEBUGS("AppInit")<<new_start_slurl.asString()<<LL_ENDL;

	LLComboBox* location_combo = sInstance->getChild<LLComboBox>("start_location_combo");
	/*
	 * Determine whether or not the new_start_slurl modifies the grid.
	 *
	 * Note that some forms that could be in the slurl are grid-agnostic.,
	 * such as "home".  Other forms, such as
	 * https://grid.example.com/region/Party%20Town/20/30/5 
	 * specify a particular grid; in those cases we want to change the grid
	 * and the grid selector to match the new value.
	 */
	enum LLSLURL::SLURL_TYPE new_slurl_type = new_start_slurl.getType();
	switch ( new_slurl_type )
	{
	case LLSLURL::LOCATION:
	{
		location_combo->setCurrentByIndex( 2 );
		location_combo->setTextEntry(new_start_slurl.getLocationString());
	}
	break;
	case LLSLURL::HOME_LOCATION:
		location_combo->setCurrentByIndex( 0 );	// home location
		break;
	case LLSLURL::LAST_LOCATION:
		location_combo->setCurrentByIndex( 1 ); // last location
		break;
	default:
		LL_WARNS("AppInit")<<"invalid login slurl, using home"<<LL_ENDL;
		location_combo->setCurrentByIndex(1); // home location
		break;
	}

	updateLocationSelectorsVisibility();
}

void LLPanelLogin::setLocation(const LLSLURL& slurl)
{
	LL_DEBUGS("AppInit")<<"setting Location "<<slurl.asString()<<LL_ENDL;
	LLStartUp::setStartSLURL(slurl); // calls onUpdateStartSLURL, above
}

// static
void LLPanelLogin::close()
{
	if (sInstance)
	{
		LLPanelLogin::sInstance->getParent()->removeChild( LLPanelLogin::sInstance );

		delete sInstance;
		sInstance = NULL;
	}
}

// static
void LLPanelLogin::setAlwaysRefresh(bool refresh)
{
	if (sInstance && LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
	{
		LLMediaCtrl* web_browser = sInstance->getChild<LLMediaCtrl>("login_html");

		if (web_browser)
		{
			web_browser->setAlwaysRefresh(refresh);
		}
	}
}

void LLPanelLogin::updateGridCombo()
{
	const std::string &defaultGrid = gHippoGridManager->getDefaultGridName();

	LLComboBox *grids = getChild<LLComboBox>("grids_combo");
	std::string top_entry;

	grids->removeall();

	const HippoGridInfo *curGrid = gHippoGridManager->getCurrentGrid();
	const HippoGridInfo *defGrid = gHippoGridManager->getGrid(defaultGrid);

	HippoGridManager::GridIterator it, end = gHippoGridManager->endGrid();
	for (it = gHippoGridManager->beginGrid(); it != end; ++it)
	{
		std::string grid = it->second->getGridName();
		if(grid.empty() || it->second == defGrid || it->second == curGrid)
			continue;
		grids->add(grid);
	}
	if(curGrid || defGrid)
	{
		if(defGrid)
			grids->add(defGrid->getGridName(),ADD_TOP);
		if(curGrid && defGrid != curGrid)
			grids->add(curGrid->getGridName(),ADD_TOP);
		grids->setCurrentByIndex(0);
	}
	else
	{
		grids->setLabel(LLStringExplicit(""));  // LLComboBox::removeall() does not clear the label
	}
}

void LLPanelLogin::loadLoginPage()
{
	if (!sInstance) return;

 	sInstance->updateGridCombo();

	std::string login_page_str = gHippoGridManager->getCurrentGrid()->getLoginPage();
	if (login_page_str.empty())
	{
		sInstance->setSiteIsAlive(false);
		return;
	}
  
	// Use the right delimeter depending on how LLURI parses the URL
	LLURI login_page = LLURI(login_page_str);
	LLSD params(login_page.queryMap());
 
	LL_DEBUGS("AppInit") << "login_page: " << login_page << LL_ENDL;

 	// Language
	params["lang"] = LLUI::getLanguage();
 
 	// First Login?
 	if (gSavedSettings.getBOOL("FirstLoginThisInstall"))
	{
		params["firstlogin"] = "TRUE"; // not bool: server expects string TRUE
 	}
 
 	if(login_page_str.find("secondlife.com") == -1)
	{
		params["version"]= llformat("%d.%d.%d (%d)",
 						gVersionMajor, gVersionMinor, gVersionPatch, gVersionBuild);
		params["channel"] = gVersionChannel;
	}

	// Grid

	if (gHippoGridManager->getCurrentGrid()->isSecondLife()) {
		// find second life grid from login URI
		// yes, this is heuristic, but hey, it is just to get the right login page...
		std::string tmp = gHippoGridManager->getCurrentGrid()->getLoginUri();
		int i = tmp.find(".lindenlab.com");
		if (i != std::string::npos) {
			tmp = tmp.substr(0, i);
			i = tmp.rfind('.');
			if (i == std::string::npos)
				i = tmp.rfind('/');
			if (i != std::string::npos) {
				tmp = tmp.substr(i+1);
				params["grid"] = tmp;
			}
		}
	}
	else if (gHippoGridManager->getCurrentGrid()->isOpenSimulator())
	{
		params["grid"] = gHippoGridManager->getCurrentGrid()->getGridNick();
	}
	else if (gHippoGridManager->getCurrentGrid()->getPlatform() == HippoGridInfo::PLATFORM_AURORA)
	{
		params["grid"] = LLViewerLogin::getInstance()->getGridLabel();
	}
	
	// add OS info
	params["os"] = LLAppViewer::instance()->getOSInfo().getOSStringSimple();
		
	// Make an LLURI with this augmented info
	LLURI login_uri(LLURI::buildHTTP(login_page.authority(),
									 login_page.path(),
									 params));
	
	gViewerWindow->setMenuBackgroundColor(false, !LLViewerLogin::getInstance()->isInProductionGrid());
	gLoginMenuBarView->setBackgroundColor(gMenuBarView->getBackgroundColor());

	LLMediaCtrl* web_browser = sInstance->getChild<LLMediaCtrl>("login_html");
	if (web_browser->getCurrentNavUrl() != login_uri.asString())
	{
		LL_DEBUGS("AppInit") << "loading:    " << login_uri << LL_ENDL;
		web_browser->navigateTo( login_uri.asString(), "text/html" );
	}
}

void LLPanelLogin::handleMediaEvent(LLPluginClassMedia* /*self*/, EMediaEvent event)
{
}


bool LLPanelLogin::getRememberLogin()
{
	bool remember = false;

	if (sInstance)
	{
		LLCheckBoxCtrl* remember_login = sInstance->getChild<LLCheckBoxCtrl>("remember_name_check");
		if (remember_login)
		{
			remember = remember_login->getValue().asBoolean();
		}
	}
	else
	{
		llwarns << "Attempted to query rememberLogin with no login view shown" << llendl;
	}

	return remember;
}

//---------------------------------------------------------------------------
// Protected methods
//---------------------------------------------------------------------------

// static
void LLPanelLogin::onClickConnect(void *)
{
	if (sInstance && sInstance->mCallback)
	{

		// tell the responder we're not here anymore
		if ( gResponsePtr )
			gResponsePtr->setParent( 0 );

		// JC - Make sure the fields all get committed.
		sInstance->setFocus(FALSE);

		std::string first, last, password;
		if (nameSplit(sInstance->getChild<LLComboBox>("name_combo")->getTextEntry(), first, last))
		{
			// has both first and last name typed
			sInstance->mCallback(0, sInstance->mCallbackData);
		}
		else
		{
			if (gHippoGridManager->getCurrentGrid()->getRegisterUrl().empty()) {
				LLNotificationsUtil::add("MustHaveAccountToLogInNoLinks");
			} else {
				LLNotificationsUtil::add("MustHaveAccountToLogIn", LLSD(), LLSD(),
												LLPanelLogin::newAccountAlertCallback);
			}
		}
	}
}


// static
bool LLPanelLogin::newAccountAlertCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (0 == option)
	{
		llinfos << "Going to account creation URL" << llendl;
		LLWeb::loadURLExternal( CREATE_ACCOUNT_URL );
	}
	else
	{
		sInstance->setFocus(TRUE);
	}
	return false;
}


// static
void LLPanelLogin::onClickNewAccount()
{
	const std::string &url = gHippoGridManager->getCurrentGrid()->getRegisterUrl();
	if (!url.empty()) {
		llinfos << "Going to account creation URL." << llendl;
		LLWeb::loadURLExternal(url);
	} else {
		llinfos << "Account creation URL is empty." << llendl;
		sInstance->setFocus(TRUE);
	}
}

// static
void LLPanelLogin::onClickGrids(void*)
{
	//LLFloaterPreference::overrideLastTab(LLPreferenceCore::TAB_GRIDS);
	LLFloaterPreference::show(NULL);
	LLFloaterPreference::switchTab(LLPreferenceCore::TAB_GRIDS);
}

// static
void LLPanelLogin::onClickVersion(void*)
{
	LLFloaterAbout::show(NULL);
}

//static
void LLPanelLogin::onClickForgotPassword()
{
	if (sInstance )
	{
		const std::string &url = gHippoGridManager->getCurrentGrid()->getPasswordUrl();
		if (!url.empty()) {
			LLWeb::loadURLExternal(url);
		} else {
			llwarns << "Link for 'forgotton password' not set." << llendl;
		}
	}
}

// static
void LLPanelLogin::onPassKey(LLLineEditor* caller)
{
	if (gKeyboard->getKeyDown(KEY_CAPSLOCK) && sCapslockDidNotification == FALSE)
	{
		LLNotificationsUtil::add("CapsKeyOn");
		sCapslockDidNotification = TRUE;
	}
}

void LLPanelLogin::onCurGridChange(HippoGridInfo* new_grid, HippoGridInfo* old_grid)
{
	refreshLoginPage();
	if(old_grid != new_grid)	//Changed grid? Reset the location combobox
	{
		std::string defaultStartLocation = gSavedSettings.getString("LoginLocation");
		LLSLURL defaultStart(defaultStartLocation);
		LLStartUp::setStartSLURL(defaultStart.isSpatial() ? defaultStart : LLSLURL(LLSLURL::SIM_LOCATION_HOME));	// calls onUpdateStartSLURL
	}
}

// static
//void LLPanelLogin::updateServer()
void LLPanelLogin::refreshLoginPage()
{
	if (!sInstance || (LLStartUp::getStartupState() >= STATE_LOGIN_CLEANUP))
		 return;

	sInstance->updateGridCombo();

	sInstance->childSetVisible("create_new_account_text",
		!gHippoGridManager->getCurrentGrid()->getRegisterUrl().empty());
	sInstance->childSetVisible("forgot_password_text",
		!gHippoGridManager->getCurrentGrid()->getPasswordUrl().empty());

	std::string login_page = gHippoGridManager->getCurrentGrid()->getLoginPage();
	if (!login_page.empty())
	{
		LLMediaCtrl* web_browser = sInstance->getChild<LLMediaCtrl>("login_html");
		if (web_browser->getCurrentNavUrl() != login_page)
		{
			if(gResponsePtr)
				gResponsePtr->setParent(0);	//Tell our previous responder that we no longer require its result.
			gResponsePtr.reset();			//Deref previous responder

			llinfos << "Firing off lookup for " << login_page << llendl;
			// kick off a request to grab the url manually
			gResponsePtr = LLIamHereLogin::build(sInstance);
			LLHTTPClient::head(login_page, gResponsePtr.get());
		}
	}
	else
	{
		if(gResponsePtr)
			gResponsePtr->setParent(0);	//Tell our previous responder that we no longer require its result.
		gResponsePtr.reset();			//Deref previous responder
		sInstance->setSiteIsAlive(false);
	}
}

// static
//void LLPanelLogin::onSelectServer()
void LLPanelLogin::onSelectGrid(LLUICtrl *ctrl)
{
	gHippoGridManager->setCurrentGrid(ctrl->getValue());
}

void LLPanelLogin::onLocationSLURL()
{
	LLComboBox* location_combo = getChild<LLComboBox>("start_location_combo");
	std::string location = location_combo->getValue().asString();
	LL_DEBUGS("AppInit")<<location<<LL_ENDL;

	LLStartUp::setStartSLURL(location); // calls onUpdateStartSLURL, above 
}

//Special handling of name combobox. Facilitates grid-changing by account selection.
// static
void LLPanelLogin::onSelectLoginEntry(LLUICtrl* ctrl, void* data)
{
	if (sInstance)
	{
		LLComboBox* combo = sInstance->getChild<LLComboBox>("name_combo");
		if (ctrl == combo)
		{
			LLSD selected_entry = combo->getSelectedValue();
			if (!selected_entry.isUndefined())
			{
				LLSavedLoginEntry entry(selected_entry);
				setFields(entry);
			}
			// This stops the automatic matching of the first name to a selected grid.
			LLViewerLogin::getInstance()->setNameEditted(true);
		}
	}
}

void LLPanelLogin::onLoginComboLostFocus(LLComboBox* combo_box)
{
	if(combo_box->isTextDirty())
	{
		clearPassword();
		combo_box->resetTextDirty();
	}
}

// static
void LLPanelLogin::onNameCheckChanged(LLUICtrl* ctrl, void* data)
{
	if (sInstance)
	{
		LLCheckBoxCtrl* remember_login_check = sInstance->getChild<LLCheckBoxCtrl>("remember_name_check");
		LLCheckBoxCtrl* remember_pass_check = sInstance->getChild<LLCheckBoxCtrl>("remember_check");
		if (remember_login_check && remember_pass_check)
		{
			if (remember_login_check->getValue().asBoolean())
			{
				remember_pass_check->setEnabled(true);
			}
			else
			{
				remember_pass_check->setValue(LLSD(false));
				remember_pass_check->setEnabled(false);
			}
		}
	}
}

// static
void LLPanelLogin::clearPassword()
{
	std::string blank;
	sInstance->childSetText("password_edit", blank);
	sInstance->mIncomingPassword = blank;
	sInstance->mMungedPassword = blank;
}
