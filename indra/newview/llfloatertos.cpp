/** 
 * @file llfloatertos.cpp
 * @brief Terms of Service Agreement dialog
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

#include "llviewerprecompiledheaders.h"

#include "llfloatertos.h"

// viewer includes
#include "llagent.h"
#include "llappviewer.h"
#include "llstartup.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"

// linden library includes
#include "llbutton.h"
#include "llhttpclient.h"
#include "llhttpstatuscodes.h"	// for HTTP_FOUND
#include "llnotificationsutil.h"
#include "llradiogroup.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluictrlfactory.h"
#include "llvfile.h"
#include "message.h"

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy iamHere_timeout;

// static 
LLFloaterTOS* LLFloaterTOS::sInstance = NULL;

// static
LLFloaterTOS* LLFloaterTOS::show(ETOSType type, const std::string & message)
{
	if (sInstance)
	{
		delete sInstance;
	}
	return sInstance = new LLFloaterTOS(type, message);
}


LLFloaterTOS::LLFloaterTOS(ETOSType type, const std::string& message)
:	LLModalDialog( std::string(" "), 100, 100 ),
	mType(type),
	mLoadCompleteCount( 0 )
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
		mType == TOS_CRITICAL_MESSAGE ? "floater_critical.xml"
			: mType == TOS_TOS ? "floater_tos.xml"
			: "floater_voice_license.xml");

	if (mType == TOS_CRITICAL_MESSAGE)
	{
		// this displays the critical message
		LLTextEditor *editor = getChild<LLTextEditor>("tos_text");
		editor->setHandleEditKeysDirectly( TRUE );
		editor->setEnabled( FALSE );
		editor->setWordWrap(TRUE);
		editor->setFocus(TRUE);
		editor->setValue(LLSD(message));
	}
}

// helper class that trys to download a URL from a web site and calls a method 
// on parent class indicating if the web server is working or not
class LLIamHere : public LLHTTPClient::ResponderWithResult
{
	private:
		LLIamHere( LLFloaterTOS* parent ) :
		   mParent( parent )
		{}

		LLFloaterTOS* mParent;

	public:

		static boost::intrusive_ptr< LLIamHere > build( LLFloaterTOS* parent )
		{
			return boost::intrusive_ptr< LLIamHere >( new LLIamHere( parent ) );
		};
		
		virtual void  setParent( LLFloaterTOS* parentIn )
		{
			mParent = parentIn;
		};
		
		/*virtual*/ void httpSuccess(void)
		{
			if ( mParent )
				mParent->setSiteIsAlive( true );
		};

		/*virtual*/ void httpFailure(void)
		{
			if ( mParent )
			{
				// *HACK: For purposes of this alive check, 302 Found
				// (aka Moved Temporarily) is considered alive.  The web site
				// redirects this link to a "cache busting" temporary URL. JC
				bool alive = (mStatus == HTTP_FOUND);
				mParent->setSiteIsAlive( alive );
			}
		};

		/*virtual*/  AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return iamHere_timeout; }
		/*virtual*/ bool pass_redirect_status(void) const { return true; }
		/*virtual*/ char const* getName(void) const { return "LLIamHere"; }
};

// this is global and not a class member to keep crud out of the header file
namespace {
	boost::intrusive_ptr< LLIamHere > gResponsePtr = 0;
};

BOOL LLFloaterTOS::postBuild()
{	
	childSetAction("Continue", onContinue, this);
	childSetAction("Cancel", onCancel, this);
	childSetCommitCallback("agree_chk", updateAgree, this);

	if ( mType == TOS_CRITICAL_MESSAGE )
	{
		return TRUE;
	}

	// disable Agree to TOS radio button until the page has fully loaded
	LLCheckBoxCtrl* tos_agreement = getChild<LLCheckBoxCtrl>("agree_chk");
	tos_agreement->setEnabled( false );

	// hide the SL text widget if we're displaying TOS with using a browser widget.
	LLTextEditor *editor = getChild<LLTextEditor>(mType == TOS_VOICE ? "license_text" : "tos_text");
	editor->setVisible( FALSE );

	LLMediaCtrl* web_browser = getChild<LLMediaCtrl>(mType == TOS_VOICE ? "license_html" : "tos_html");
	if ( web_browser )
	{
		web_browser->addObserver(this);
		std::string url = getString( "real_url" );

		if (mType != TOS_VOICE || url.substr(0,4) == "http") {
			gResponsePtr = LLIamHere::build( this );
			LLHTTPClient::get(url, gResponsePtr);
		} else {
			setSiteIsAlive(false);
		}
	}

	return TRUE;
}

void LLFloaterTOS::setSiteIsAlive( bool alive )
{
	// only do this for TOS pages
	if ( mType != TOS_CRITICAL_MESSAGE )
	{
		LLMediaCtrl* web_browser = getChild<LLMediaCtrl>(mType == TOS_VOICE ? "license_html" : "tos_html");
		// if the contents of the site was retrieved
		if ( alive )
		{
			if ( web_browser )
			{
				// navigate to the "real" page 
				web_browser->navigateTo( getString( "real_url" ) );
			}
		}
		else
		{
			if (mType == TOS_VOICE) web_browser->navigateToLocalPage("license", getString("fallback_html"));
			// normally this is set when navigation to TOS page navigation completes (so you can't accept before TOS loads)
			// but if the page is unavailable, we need to do this now
			LLCheckBoxCtrl* tos_agreement = getChild<LLCheckBoxCtrl>("agree_chk");
			tos_agreement->setEnabled( true );
		}
	}
}

LLFloaterTOS::~LLFloaterTOS()
{
	// tell the responder we're not here anymore
	if ( gResponsePtr )
		gResponsePtr->setParent( 0 );

	LLFloaterTOS::sInstance = NULL;
}

// virtual
void LLFloaterTOS::draw()
{
	// draw children
	LLModalDialog::draw();
}

// static
void LLFloaterTOS::updateAgree(LLUICtrl*, void* userdata )
{
	LLFloaterTOS* self = (LLFloaterTOS*) userdata;
	bool agree = self->childGetValue("agree_chk").asBoolean(); 
	self->childSetEnabled("Continue", agree);
}

// static
void LLFloaterTOS::onContinue( void* userdata )
{
	LLFloaterTOS* self = (LLFloaterTOS*) userdata;
	bool voice = self->mType == TOS_VOICE;
	LL_INFOS() << (voice ? "User agreed to the Vivox personal license" : "User agrees with TOS.") << LL_ENDL;
	if (voice)
	{
		// enabling voice by default here seems like the best behavior
		gSavedSettings.setBOOL("EnableVoiceChat", TRUE);
		gSavedSettings.setBOOL("VivoxLicenseAccepted", TRUE);

		// save these settings in case something bad happens later
		gSavedSettings.saveToFile(gSavedSettings.getString("ClientSettingsFile"), TRUE);
	}
	else if (self->mType == TOS_TOS)
	{
		gAcceptTOS = TRUE;
	}
	else
	{
		gAcceptCriticalMessage = TRUE;
	}

	auto state = LLStartUp::getStartupState();
	// Testing TOS dialog
	#if ! LL_RELEASE_FOR_DOWNLOAD
	if (!voice && state == STATE_LOGIN_WAIT)
	{
		LLStartUp::setStartupState( STATE_LOGIN_SHOW );
	}
	else 
	#endif
	if (!voice || state == STATE_LOGIN_VOICE_LICENSE)
	{
		LLStartUp::setStartupState( STATE_LOGIN_AUTH_INIT );			// Go back and finish authentication
	}
	self->close(); // destroys this object
}

// static
void LLFloaterTOS::onCancel( void* userdata )
{
	LLFloaterTOS* self = (LLFloaterTOS*) userdata;
	if (self->mType == TOS_VOICE)
	{
		LL_INFOS() << "User disagreed with the vivox personal license" << LL_ENDL;
		gSavedSettings.setBOOL("EnableVoiceChat", FALSE);
		gSavedSettings.setBOOL("VivoxLicenseAccepted", FALSE);

		if (LLStartUp::getStartupState() == STATE_LOGIN_VOICE_LICENSE)
		{
			LLStartUp::setStartupState( STATE_LOGIN_AUTH_INIT );			// Go back and finish authentication
		}
	}
	else
	{
		LL_INFOS() << "User disagrees with TOS." << LL_ENDL;
		LLNotificationsUtil::add("MustAgreeToLogIn", LLSD(), LLSD(), login_alert_done);
		LLStartUp::setStartupState( STATE_LOGIN_SHOW );
	}
	self->mLoadCompleteCount = 0;  // reset counter for next time we come to TOS
	self->close(); // destroys this object
}

//virtual 
void LLFloaterTOS::handleMediaEvent(LLPluginClassMedia* /*self*/, EMediaEvent event)
{
	if(event == MEDIA_EVENT_NAVIGATE_COMPLETE)
	{
		// skip past the loading screen navigate complete
		if ( ++mLoadCompleteCount == 2 )
		{
			LL_INFOS() << "NAVIGATE COMPLETE" << LL_ENDL;
			// enable Agree to TOS radio button now that page has loaded
			LLCheckBoxCtrl * tos_agreement = getChild<LLCheckBoxCtrl>("agree_chk");
			tos_agreement->setEnabled( true );
		}
	}
}
