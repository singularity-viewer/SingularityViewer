/** 
 * @file llprogressview.cpp
 * @brief LLProgressView class implementation
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

#include "llprogressview.h"

#include "indra_constants.h"
#include "llmath.h"
#include "llgl.h"
#include "llrender.h"
#include "llui.h"
#include "llfontgl.h"
#include "llimagegl.h"
#include "lltimer.h"
#include "llglheaders.h"

#include "llagent.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llfocusmgr.h"
#include "llprogressbar.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llappviewer.h"
#include "llweb.h"
#include "lluictrlfactory.h"
#include "llpanellogin.h"

LLProgressView* LLProgressView::sInstance = NULL;

S32 gStartImageWidth = 1;
S32 gStartImageHeight = 1;
const F32 FADE_TO_WORLD_TIME = 1.0f;

// XUI:translate
LLProgressView::LLProgressView(const std::string& name, const LLRect &rect) 
:	LLPanel(name, rect, FALSE),
	mPercentDone( 0.f ),
	mURLInMessage(false),
	mMouseDownInActiveArea( false ),
	mFadeToWorldTimer(),
	mFadeFromLoginTimer(),
	mStartupComplete(false)
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_progress.xml");
	reshape(rect.getWidth(), rect.getHeight());
	mFadeToWorldTimer.stop();
	mFadeFromLoginTimer.stop();
}

BOOL LLProgressView::postBuild()
{
	mProgressBar = getChild<LLProgressBar>("login_progress_bar");

	mCancelBtn = getChild<LLButton>("cancel_btn");
	mCancelBtn->setClickedCallback( boost::bind(&LLProgressView::onCancelButtonClicked) );

	getChild<LLTextBox>("title_text")->setText(LLStringExplicit(LLAppViewer::instance()->getSecondLifeTitle()));

	getChild<LLTextBox>("message_text")->setClickedCallback(boost::bind(&LLProgressView::onClickMessage, this));

	// hidden initially, until we need it
	setVisible(FALSE);
	sInstance = this;
	return TRUE;
}


LLProgressView::~LLProgressView()
{
	// Just in case something went wrong, make sure we deregister our idle callback.
	gIdleCallbacks.deleteFunction(onIdle, this);

	gFocusMgr.releaseFocusIfNeeded( this );

	sInstance = NULL;
}

BOOL LLProgressView::handleHover(S32 x, S32 y, MASK mask)
{
	if( childrenHandleHover( x, y, mask ) == NULL )
	{
		gViewerWindow->setCursor(UI_CURSOR_WAIT);
	}
	return TRUE;
}


BOOL LLProgressView::handleKeyHere(KEY key, MASK mask)
{
	// Suck up all keystokes except CTRL-Q.
	if( ('Q' == key) && (MASK_CONTROL == mask) )
	{
		LLAppViewer::instance()->userQuit();
	}
	return TRUE;
}

void LLProgressView::revealIntroPanel()
{
	getParent()->sendChildToFront(this);
	mFadeFromLoginTimer.start();
	gIdleCallbacks.addFunction(onIdle, this);
}

void LLProgressView::abortShowProgress()
{
	mFadeFromLoginTimer.stop();
	LLPanelLogin::close();
	gIdleCallbacks.deleteFunction(onIdle, this);
}

void LLProgressView::setStartupComplete()
{
	mStartupComplete = true;

	mFadeFromLoginTimer.stop();
	mFadeToWorldTimer.start();
}
void LLProgressView::setVisible(BOOL visible)
{
	// hiding progress view
	if (getVisible() && !visible)
	{
		LLPanel::setVisible(FALSE);
	}
	// showing progress view
	else if (visible && (!getVisible() || mFadeToWorldTimer.getStarted()))
	{
		setFocus(TRUE);
		mFadeToWorldTimer.stop();
		LLPanel::setVisible(TRUE);
	} 
}


void LLProgressView::drawStartTexture(F32 alpha)
{
	gGL.pushMatrix();	
	if (gStartTexture)
	{
		LLGLSUIDefault gls_ui;
		gGL.getTexUnit(0)->bind(gStartTexture.get());
		gGL.color4f(1.f, 1.f, 1.f, alpha);
		F32 image_aspect = (F32)gStartImageWidth / (F32)gStartImageHeight;
		S32 width = getRect().getWidth();
		S32 height = getRect().getHeight();
		F32 view_aspect = (F32)width / (F32)height;
		// stretch image to maintain aspect ratio
		if (image_aspect > view_aspect)
		{
			gGL.translatef(-0.5f * (image_aspect / view_aspect - 1.f) * width, 0.f, 0.f);
			gGL.scalef(image_aspect / view_aspect, 1.f, 1.f);
		}
		else
		{
			gGL.translatef(0.f, -0.5f * (view_aspect / image_aspect - 1.f) * height, 0.f);
			gGL.scalef(1.f, view_aspect / image_aspect, 1.f);
		}
		gl_rect_2d_simple_tex( getRect().getWidth(), getRect().getHeight() );
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	}
	else
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f(0.f, 0.f, 0.f, 1.f);
		gl_rect_2d(getRect());
	}
	gGL.popMatrix();
}


void LLProgressView::draw()
{
	static LLTimer timer;

	if (mFadeFromLoginTimer.getStarted())
	{
		F32 alpha = clamp_rescale(mFadeFromLoginTimer.getElapsedTimeF32(), 0.f, FADE_TO_WORLD_TIME, 0.f, 1.f);
		LLViewDrawContext context(alpha);

		drawStartTexture(alpha);
		
		LLPanel::draw();
		return;
	}

	// handle fade out to world view when we're asked to
	if (mFadeToWorldTimer.getStarted())
	{
		// draw fading panel
		F32 alpha = clamp_rescale(mFadeToWorldTimer.getElapsedTimeF32(), 0.f, FADE_TO_WORLD_TIME, 1.f, 0.f);
		LLViewDrawContext context(alpha);
				
		drawStartTexture(alpha);
		LLPanel::draw();

		// faded out completely - remove panel and reveal world
		if (mFadeToWorldTimer.getElapsedTimeF32() > FADE_TO_WORLD_TIME )
		{
			mFadeToWorldTimer.stop();

			// Fade is complete, release focus
			gFocusMgr.releaseFocusIfNeeded( this );

			// turn off panel that hosts intro so we see the world
			setVisible(FALSE);

			gStartTexture = NULL;
		}
		return;
	}

	drawStartTexture(1.0f);
	// draw children
	LLPanel::draw();
}

void LLProgressView::setText(const std::string& text)
{
	getChild<LLTextBox>("progress_text")->setWrappedText(LLStringExplicit(text));
}

void LLProgressView::setPercent(const F32 percent)
{
	mProgressBar->setPercent(percent);
}

void LLProgressView::setMessage(const std::string& msg)
{
	mMessage = msg;
	mURLInMessage = (mMessage.find( "https://" ) != std::string::npos ||
			 mMessage.find( "http://" ) != std::string::npos ||
			 mMessage.find( "ftp://" ) != std::string::npos);

	getChild<LLTextBox>("message_text")->setWrappedText(LLStringExplicit(mMessage));
	getChild<LLTextBox>("message_text")->setHoverActive(mURLInMessage);
}

void LLProgressView::setCancelButtonVisible(BOOL b, const std::string& label)
{
	mCancelBtn->setVisible( b );
	mCancelBtn->setEnabled( b );
	mCancelBtn->setLabelSelected(label);
	mCancelBtn->setLabelUnselected(label);
}

// static
void LLProgressView::onCancelButtonClicked()
{
	// Quitting viewer here should happen only when "Quit" button is pressed while starting up.
	// Check for startup state is used here instead of teleport state to avoid quitting when
	// cancel is pressed while teleporting inside region (EXT-4911)
	if (LLStartUp::getStartupState() < STATE_STARTED)
	{
		LLAppViewer::instance()->requestQuit();
	}
	else
	{
		gAgent.teleportCancel();
		sInstance->mCancelBtn->setEnabled(FALSE);
		sInstance->setVisible(FALSE);
	}
}

// static
void LLProgressView::onClickMessage(void* data)
{
	LLProgressView* viewp = (LLProgressView*)data;
	if ( viewp != NULL && ! viewp->mMessage.empty() )
	{
		std::string url_to_open( "" );

		size_t start_pos;
		start_pos = viewp->mMessage.find( "https://" );
		if (start_pos == std::string::npos)
			start_pos = viewp->mMessage.find( "http://" );
		if (start_pos == std::string::npos)
			start_pos = viewp->mMessage.find( "ftp://" );
			
		if ( start_pos != std::string::npos )
		{
			size_t end_pos = viewp->mMessage.find_first_of( " \n\r\t", start_pos );
			if ( end_pos != std::string::npos )
				url_to_open = viewp->mMessage.substr( start_pos, end_pos - start_pos );
			else
				url_to_open = viewp->mMessage.substr( start_pos );

			LLWeb::loadURLExternal( url_to_open );
		}
	}
}


// static
void LLProgressView::onIdle(void* user_data)
{
	LLProgressView* self = (LLProgressView*) user_data;

	// Close login panel on mFadeToWorldTimer expiration.
	if (self->mFadeFromLoginTimer.getStarted() &&
		self->mFadeFromLoginTimer.getElapsedTimeF32() > FADE_TO_WORLD_TIME)
	{
		self->mFadeFromLoginTimer.stop();
		LLPanelLogin::close();

		// Nothing to do anymore.
		gIdleCallbacks.deleteFunction(onIdle, user_data);
	}
}
