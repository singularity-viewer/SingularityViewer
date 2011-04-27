/**
 * @file LLMediaCtrl.cpp
 * @brief Web browser UI control
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


#include "llmediactrl.h"

// viewer includes
#include "llfloaterhtml.h"
#include "llfloaterworldmap.h"
#include "lluictrlfactory.h"
#include "llurldispatcher.h"
#include "llurlsimstring.h"
#include "llviewborder.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerwindow.h"
#include "llnotifications.h"
#include "llweb.h"
#include "llrender.h"
#include "llpluginclassmedia.h"

// linden library includes
#include "llfocusmgr.h"

extern BOOL gRestoreGL;

// Setting the mozilla buffer width to 2048 exactly doesn't work, since it pads its rowbytes a bit, pushing the texture width over 2048.
// 2000 should give enough headroom for any amount of padding it cares to add.
const S32 MAX_DIMENSION = 2000;
const S32 MAX_TEXTURE_DIMENSION = 2048;

static LLRegisterWidget<LLMediaCtrl> r("web_browser");

LLMediaCtrl::LLMediaCtrl( const std::string& name, const LLRect& rect ) :
	LLUICtrl( name, rect, FALSE, NULL, NULL ),
	mTextureDepthBytes( 4 ),
	mWebBrowserImage( 0 ),
	mBorder(NULL),
	mFrequentUpdates( true ),
	mForceUpdate( false ),
	mOpenLinksInExternalBrowser( false ),
	mOpenLinksInInternalBrowser( false ),
	mTrusted( false ),
	mHomePageUrl( "" ),
	mIgnoreUIScale( true ),
	mAlwaysRefresh( false ),
	mExternalUrl( "" ),
	mMediaSource( 0 ),
	mTakeFocusOnClick( true ),
	mCurrentNavUrl( "about:blank" ),
	mLastSetCursor( UI_CURSOR_ARROW ),
	mStretchToFill( true ),
	mMaintainAspectRatio ( true ),
	mHideLoading (false)
{
	S32 screen_width = mIgnoreUIScale ? 
		llround((F32)getRect().getWidth() * LLUI::sGLScaleFactor.mV[VX]) : getRect().getWidth();
	S32 screen_height = mIgnoreUIScale ? 
		llround((F32)getRect().getHeight() * LLUI::sGLScaleFactor.mV[VY]) : getRect().getHeight();

	mMediaSource = LLViewerMedia::newMediaImpl(mHomePageUrl, LLUUID::null, screen_width, screen_height, false, false, "text/html");
	if ( !mMediaSource )
	{
		llwarns << "media source create failed " << llendl;
		return;
	}
	else
	{
		// create a new texture (based on LLDynamic texture) that will be used to display the output
		mWebBrowserImage = new LLWebBrowserTexture( screen_width, screen_height, this, mMediaSource );
	}

	mMediaSource->setVisible( getVisible() );

	mMediaSource->addObserver( this );

	LLRect border_rect( 0, getRect().getHeight() + 2, getRect().getWidth() + 2, 0 );
	mBorder = new LLViewBorder( std::string("web control border"), border_rect, LLViewBorder::BEVEL_IN );
	addChild( mBorder );
}

////////////////////////////////////////////////////////////////////////////////
// note: this is now a singleton and destruction happens via initClass() now
LLMediaCtrl::~LLMediaCtrl()
{

	if (mMediaSource)
	{
		mMediaSource->remObserver( this );
		mMediaSource = NULL;
	}

	if ( mWebBrowserImage )
	{
		mWebBrowserImage = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::setBorderVisible( BOOL border_visible )
{
	if ( mBorder )
	{
		mBorder->setVisible( border_visible );
	};
};

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::setTakeFocusOnClick( bool take_focus )
{
	mTakeFocusOnClick = take_focus;
}

////////////////////////////////////////////////////////////////////////////////
// set flag that forces the embedded browser to open links in the external system browser
void LLMediaCtrl::setOpenInExternalBrowser( bool valIn )
{
	mOpenLinksInExternalBrowser = valIn;
};

////////////////////////////////////////////////////////////////////////////////
// set flag that forces the embedded browser to open links in the internal browser floater
void LLMediaCtrl::setOpenInInternalBrowser( bool valIn )
{
	mOpenLinksInInternalBrowser = valIn;
};

////////////////////////////////////////////////////////////////////////////////
void LLMediaCtrl::setTrusted( bool valIn )
{
	mTrusted = valIn;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleHover( S32 x, S32 y, MASK mask )
{
	convertInputCoords(x, y);

	if (mMediaSource)
		mMediaSource->mouseMove(x, y);

	gViewerWindow->setCursor(mLastSetCursor);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleScrollWheel( S32 x, S32 y, S32 clicks )
{
	if (mMediaSource && mMediaSource->hasMedia())
		mMediaSource->getMediaPlugin()->scrollEvent(0, clicks, MASK_NONE);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleMouseUp( S32 x, S32 y, MASK mask )
{
	convertInputCoords(x, y);

	if (mMediaSource)
	{
		mMediaSource->mouseUp(x, y);

		// *HACK: LLMediaImplLLMozLib automatically takes focus on mouseup,
		// in addition to the onFocusReceived() call below.  Undo this. JC
		if (!mTakeFocusOnClick)
		{
			mMediaSource->focus(false);
			gViewerWindow->focusClient();
		}
	}
	
	gFocusMgr.setMouseCapture( NULL );

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleMouseDown( S32 x, S32 y, MASK mask )
{
	convertInputCoords(x, y);

	if (mMediaSource)
		mMediaSource->mouseDown(x, y);
	
	gFocusMgr.setMouseCapture( this );

	if (mTakeFocusOnClick)
	{
		setFocus( TRUE );
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleDoubleClick( S32 x, S32 y, MASK mask )
{
	convertInputCoords(x, y);

	if (mMediaSource)
		mMediaSource->mouseLeftDoubleClick( x, y );

	gFocusMgr.setMouseCapture( this );

	if (mTakeFocusOnClick)
	{
		setFocus( TRUE );
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::onFocusReceived()
{
	if (mMediaSource)
	{
		mMediaSource->focus(true);
		
		// Set focus for edit menu items
		LLEditMenuHandler::gEditMenuHandler = mMediaSource;
	}
	
	LLUICtrl::onFocusReceived();
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::onFocusLost()
{
	if (mMediaSource)
	{
		mMediaSource->focus(false);

		if( LLEditMenuHandler::gEditMenuHandler == mMediaSource )
		{
			// Clear focus for edit menu items
			LLEditMenuHandler::gEditMenuHandler = NULL;
		}
	}

	gViewerWindow->focusClient();

	LLUICtrl::onFocusLost();
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleKeyHere( KEY key, MASK mask )
{
	BOOL result = FALSE;
	
	// FIXME: THIS IS SO WRONG.
	// Menu keys should be handled by the menu system and not passed to UI elements, but this is how LLTextEditor and LLLineEditor do it...
	
	if (mMediaSource)
	{
		if( MASK_CONTROL & mask )
		{
			if( 'C' == key )
			{
				mMediaSource->copy();
				result = TRUE;
			}
			else
			if( 'V' == key )
			{
				mMediaSource->paste();
				result = TRUE;
			}
			else
			if( 'X' == key )
			{
				mMediaSource->cut();
				result = TRUE;
			}
		}
		
		if(!result)
		{
			result = mMediaSource->handleKeyHere(key, mask);
		}
	}
		
	return result;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::handleVisibilityChange ( BOOL new_visibility )
{
	llinfos << "visibility changed to " << (new_visibility?"true":"false") << llendl;
	if(mMediaSource)
	{
		mMediaSource->setVisible( new_visibility );
	}
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLMediaCtrl::handleUnicodeCharHere(llwchar uni_char)
{
	BOOL result = FALSE;
	
	// only accept 'printable' characters, sigh...
	if (uni_char >= 32 // discard 'control' characters
	    && uni_char != 127) // SDL thinks this is 'delete' - yuck.
	{
		if (mMediaSource)
			result = mMediaSource->handleUnicodeCharHere(uni_char);
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::onVisibilityChange ( BOOL new_visibility )
{
	// set state of frequent updates automatically if visibility changes
	if ( new_visibility )
	{
		mFrequentUpdates = true;
	}
	else
	{
		mFrequentUpdates = false;
	}
	LLUICtrl::onVisibilityChange(new_visibility);
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::reshape( S32 width, S32 height, BOOL called_from_parent )
{
	S32 screen_width = mIgnoreUIScale ? llround((F32)width * LLUI::sGLScaleFactor.mV[VX]) : width;
	S32 screen_height = mIgnoreUIScale ? llround((F32)height * LLUI::sGLScaleFactor.mV[VY]) : height;
	
//	llinfos << "reshape called with width = " << width << ", height = " << height << llendl;
	
	// when floater is minimized, these sizes are negative
	if ( mWebBrowserImage && screen_height > 0 && screen_width > 0 )
	{
		mWebBrowserImage->resize( screen_width, screen_height );
		mForceUpdate = true;
	}

	LLUICtrl::reshape( width, height, called_from_parent );
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateBack()
{
	if (mMediaSource && mMediaSource->hasMedia())
	{
		mMediaSource->getMediaPlugin()->browse_back();
	}
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateForward()
{
	if (mMediaSource && mMediaSource->hasMedia())
	{
		mMediaSource->getMediaPlugin()->browse_forward();
	}
}

////////////////////////////////////////////////////////////////////////////////
//
bool LLMediaCtrl::canNavigateBack()
{
	if (mMediaSource)
		return mMediaSource->canNavigateBack();
	else
		return false;
}

////////////////////////////////////////////////////////////////////////////////
//
bool LLMediaCtrl::canNavigateForward()
{
	if (mMediaSource)
		return mMediaSource->canNavigateForward();
	else
		return false;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::set404RedirectUrl( std::string redirect_url )
{
	if(mMediaSource && mMediaSource->hasMedia())
		mMediaSource->getMediaPlugin()->set_status_redirect( 404, redirect_url );
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::clr404RedirectUrl()
{
	if(mMediaSource && mMediaSource->hasMedia())
		mMediaSource->getMediaPlugin()->set_status_redirect(404, "");
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateTo( std::string url_in, std::string mime_type)
{
	// don't browse to anything that starts with secondlife:// or sl://
	const std::string protocol1 = "secondlife://";
	const std::string protocol2 = "sl://";
	if ((LLStringUtil::compareInsensitive(url_in.substr(0, protocol1.length()), protocol1) == 0) ||
	    (LLStringUtil::compareInsensitive(url_in.substr(0, protocol2.length()), protocol2) == 0))
	{
		// TODO: Print out/log this attempt?
		// llinfos << "Rejecting attempt to load restricted website :" << urlIn << llendl;
		return;
	}
	
	if (mMediaSource)
	{
		mCurrentNavUrl = url_in;
		mMediaSource->navigateTo(url_in, mime_type, mime_type.empty());
	}
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateToLocalPage( const std::string& subdir, const std::string& filename_in )
{
	std::string language = LLUI::getLanguage();
	std::string delim = gDirUtilp->getDirDelimiter();
	std::string filename;

	filename += subdir;
	filename += delim;
	filename += filename_in;

	std::string expanded_filename = gDirUtilp->findSkinnedFilename("html", language, filename);

	if (! gDirUtilp->fileExists(expanded_filename))
	{
		if (language != "en-us")
		{
			expanded_filename = gDirUtilp->findSkinnedFilename("html", "en-us", filename);
			if (! gDirUtilp->fileExists(expanded_filename))
			{
				llwarns << "File " << subdir << delim << filename_in << "not found" << llendl;
				return;
			}
		}
		else
		{
			llwarns << "File " << subdir << delim << filename_in << "not found" << llendl;
			return;
		}
	}
	if (mMediaSource)
	{
		mCurrentNavUrl = expanded_filename;
		mMediaSource->navigateTo(expanded_filename, "text/html", false);
	}

}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::navigateHome()
{
	if( mHomePageUrl.length() )
	{
		if (mMediaSource)
			mMediaSource->navigateTo(mHomePageUrl);
	};
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::setHomePageUrl( const std::string urlIn )
{
	mHomePageUrl = urlIn;
}

////////////////////////////////////////////////////////////////////////////////
//
bool LLMediaCtrl::setCaretColor(unsigned int red, unsigned int green, unsigned int blue)
{
	//NOOP
	return false;
}
////////////////////////////////////////////////////////////////////////////////
//
std::string LLMediaCtrl::getHomePageUrl()
{
	return 	mHomePageUrl;
}

////////////////////////////////////////////////////////////////////////////////
//
LLPluginClassMedia* LLMediaCtrl::getMediaPlugin()
{ 
	return mMediaSource.isNull() ? NULL : mMediaSource->getMediaPlugin(); 
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::draw()
{
	if ( ! mWebBrowserImage || mWebBrowserImage->getNeedsUpdate())
		return;

	if ( gRestoreGL == 1 )
	{
		LLRect r = getRect();
		reshape( r.getWidth(), r.getHeight(), FALSE );
		return;
	};

	// NOTE: optimization needed here - probably only need to do this once
	// unless tearoffs change the parent which they probably do.
	const LLUICtrl* ptr = findRootMostFocusRoot();
	if ( ptr && ptr->hasFocus() )
	{
		setFrequentUpdates( true );
	}
	else
	{
		setFrequentUpdates( false );
	};

	// alpha off for this
	LLGLSUIDefault gls_ui;
	LLGLDisable gls_alphaTest( GL_ALPHA_TEST );

	gGL.pushMatrix();
	{
		if (mIgnoreUIScale)
		{
			glLoadIdentity();
			// font system stores true screen origin, need to scale this by UI scale factor
			// to get render origin for this view (with unit scale)
			gGL.translatef(floorf(LLFontGL::sCurOrigin.mX * LLUI::sGLScaleFactor.mV[VX]), 
						floorf(LLFontGL::sCurOrigin.mY * LLUI::sGLScaleFactor.mV[VY]), 
						LLFontGL::sCurOrigin.mZ);
		}

		// scale texture to fit the space using texture coords
		gGL.getTexUnit(0)->bind(mWebBrowserImage);
		gGL.color4fv( LLColor4::white.mV );
		F32 max_u = ( F32 )mWebBrowserImage->getMediaWidth() / ( F32 )mWebBrowserImage->getWidth();
		F32 max_v = ( F32 )mWebBrowserImage->getMediaHeight() / ( F32 )mWebBrowserImage->getHeight();

		LLRect r = getRect();
		S32 width, height;
		S32 x_offset = 0;
		S32 y_offset = 0;
		
		if(mStretchToFill)
		{
			if(mMaintainAspectRatio)
			{
				F32 media_aspect = (F32)(mWebBrowserImage->getMediaWidth()) / (F32)(mWebBrowserImage->getMediaHeight());
				F32 view_aspect = (F32)(r.getWidth()) / (F32)(r.getHeight());
				if(media_aspect > view_aspect)
				{
					// max width, adjusted height
					width = r.getWidth();
					height = llmin(llmax(S32(width / media_aspect), 0), r.getHeight());
				}
				else
				{
					// max height, adjusted width
					height = r.getHeight();
					width = llmin(llmax(S32(height * media_aspect), 0), r.getWidth());
				}
			}
			else
			{
				width = r.getWidth();
				height = r.getHeight();
			}
		}
		else
		{
			width = llmin(mWebBrowserImage->getMediaWidth(), r.getWidth());
			height = llmin(mWebBrowserImage->getMediaHeight(), r.getHeight());
		}

		x_offset = (r.getWidth() - width) / 2;
		y_offset = (r.getHeight() - height) / 2;		

		if (mIgnoreUIScale)
		{
			width = llround((F32)width * LLUI::sGLScaleFactor.mV[VX]);
			height = llround((F32)height * LLUI::sGLScaleFactor.mV[VY]);
			x_offset = llround((F32)x_offset * LLUI::sGLScaleFactor.mV[VX]);
			y_offset = llround((F32)y_offset * LLUI::sGLScaleFactor.mV[VY]);
		}

		// draw the browser
		gGL.setSceneBlendType(LLRender::BT_REPLACE);
		gGL.begin( LLRender::QUADS );
		if (! mWebBrowserImage->getTextureCoordsOpenGL())
		{
			// render using web browser reported width and height, instead of trying to invert GL scale
			gGL.texCoord2f( max_u, 0.f );
			gGL.vertex2i( x_offset + width, y_offset + height );

			gGL.texCoord2f( 0.f, 0.f );
			gGL.vertex2i( x_offset, y_offset + height );

			gGL.texCoord2f( 0.f, max_v );
			gGL.vertex2i( x_offset, y_offset );

			gGL.texCoord2f( max_u, max_v );
			gGL.vertex2i( x_offset + width, y_offset );
		}
		else
		{
			// render using web browser reported width and height, instead of trying to invert GL scale
			gGL.texCoord2f( max_u, max_v );
			gGL.vertex2i( x_offset + width, y_offset + height );

			gGL.texCoord2f( 0.f, max_v );
			gGL.vertex2i( x_offset, y_offset + height );

			gGL.texCoord2f( 0.f, 0.f );
			gGL.vertex2i( x_offset, y_offset );

			gGL.texCoord2f( max_u, 0.f );
			gGL.vertex2i( x_offset + width, y_offset );
		}
		gGL.end();
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
	}
	gGL.popMatrix();

	// highlight if keyboard focus here. (TODO: this needs some work)
	if ( mBorder->getVisible() )
		mBorder->setKeyboardFocusHighlight( gFocusMgr.childHasKeyboardFocus( this ) );

	
	LLUICtrl::draw();
}

////////////////////////////////////////////////////////////////////////////////
//
void LLMediaCtrl::convertInputCoords(S32& x, S32& y)
{
	x = mIgnoreUIScale ? llround((F32)x * LLUI::sGLScaleFactor.mV[VX]) : x;
	if ( ! mWebBrowserImage->getTextureCoordsOpenGL() )
	{
		y = mIgnoreUIScale ? llround((F32)(y) * LLUI::sGLScaleFactor.mV[VY]) : y;
	}
	else
	{
		y = mIgnoreUIScale ? llround((F32)(getRect().getHeight() - y) * LLUI::sGLScaleFactor.mV[VY]) : getRect().getHeight() - y;
	};
}

////////////////////////////////////////////////////////////////////////////////
// static 
bool LLMediaCtrl::onClickLinkExternalTarget(const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if ( 0 == option )
	{
		// open in external browser because we don't support 
		// creation of our own secondary browser windows
		LLWeb::loadURLExternal( notification["payload"]["external_url"].asString() );
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////
// inherited from LLViewerMediaObserver
//virtual 
void LLMediaCtrl::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_CONTENT_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CONTENT_UPDATED " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_TIME_DURATION_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_TIME_DURATION_UPDATED, time is " << self->getCurrentTime() << " of " << self->getDuration() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_SIZE_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_SIZE_CHANGED " << LL_ENDL;
			LLRect r = getRect();
			reshape( r.getWidth(), r.getHeight(), FALSE );
		};
		break;
		
		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			LL_INFOS("Media") <<  "Media event:  MEDIA_EVENT_CURSOR_CHANGED, new cursor is " << self->getCursorName() << LL_ENDL;

			std::string cursor = self->getCursorName();
			
			if(cursor == "arrow")
				mLastSetCursor = UI_CURSOR_ARROW;
			else if(cursor == "ibeam")
				mLastSetCursor = UI_CURSOR_IBEAM;
			else if(cursor == "splith")
				mLastSetCursor = UI_CURSOR_SIZEWE;
			else if(cursor == "splitv")
				mLastSetCursor = UI_CURSOR_SIZENS;
			else if(cursor == "hand")
				mLastSetCursor = UI_CURSOR_HAND;
			else // for anything else, default to the arrow
				mLastSetCursor = UI_CURSOR_ARROW;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_INFOS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_BEGIN, url is " << self->getNavigateURI() << LL_ENDL;
			if(mMediaSource && mHideLoading)
			{
				mMediaSource->suspendUpdates(true);
			}
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_INFOS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_COMPLETE, result string is: " << self->getNavigateResultString() << LL_ENDL;
			if(mMediaSource && mHideLoading)
			{
				mMediaSource->suspendUpdates(false);
			}
		};
		break;

		case MEDIA_EVENT_PROGRESS_UPDATED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PROGRESS_UPDATED, loading at " << self->getProgressPercent() << "%" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_STATUS_TEXT_CHANGED, new status text is: " << self->getStatusText() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_LOCATION_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_LOCATION_CHANGED, new uri is: " << self->getLocation() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CLICK_LINK_HREF:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_HREF, target is \"" << self->getClickTarget() << "\", uri is " << self->getClickURL() << LL_ENDL;
			onClickLinkHref(self);
		};
		break;
		
		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri is " << self->getClickURL() << LL_ENDL;
			onClickLinkNoFollow(self);
		};
		break;

		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED_LAUNCH" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAME_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAME_CHANGED" << LL_ENDL;
		};
		break;
	};

	// chain all events to any potential observers of this object.
	emitEvent(self, event);
}

////////////////////////////////////////////////////////////////////////////////
// 
void LLMediaCtrl::onClickLinkHref( LLPluginClassMedia* self )
{
	// retrieve the event parameters
	std::string target = self->getClickTarget();
	std::string url = self->getClickURL();
	
	// if there is a value for the target
	if ( !target.empty() )
	{
		if ( target == "_external" )		
		{
			mExternalUrl = url;
			LLSD payload;
			payload["external_url"] = mExternalUrl;
			LLNotifications::instance().add( "WebLaunchExternalTarget", LLSD(), payload, onClickLinkExternalTarget);
			return;
		}
	}

	const std::string protocol1( "http://" );
	const std::string protocol2( "https://" );
	if( mOpenLinksInExternalBrowser )
	{
		if ( !url.empty() )
		{
			if ( LLStringUtil::compareInsensitive( url.substr( 0, protocol1.length() ), protocol1 ) == 0 ||
				 LLStringUtil::compareInsensitive( url.substr( 0, protocol2.length() ), protocol2 ) == 0 )
			{
				LLWeb::loadURLExternal( url );
			}
		}
	}
	else
	if( mOpenLinksInInternalBrowser )
	{
		if ( !url.empty() )
		{
			if ( LLStringUtil::compareInsensitive( url.substr( 0, protocol1.length() ), protocol1 ) == 0 ||
				 LLStringUtil::compareInsensitive( url.substr( 0, protocol2.length() ), protocol2 ) == 0 )
			{
				// If we spawn a new LLFloaterHTML, assume we want it to
				// follow this LLMediaCtrl's trust for whether or
				// not to open secondlife:///app/ links. JC.
//				const bool open_links_externally = false;
//				LLFloaterHtml::getInstance()->show( 
//					event_in.mStringPayload, 
//						"Second Life Browser",
//							open_links_externally,
//								mTrusted);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// 
void LLMediaCtrl::onClickLinkNoFollow( LLPluginClassMedia* self )
{
	std::string url = self->getClickURL();
	if (LLURLDispatcher::isSLURLCommand(url)
		&& !mTrusted)
	{
		// block handling of this secondlife:///app/ URL
		LLNotifications::instance().add("UnableToOpenCommandURL");
		return;
	}

	LLURLDispatcher::dispatch(url, this, mTrusted);
}

////////////////////////////////////////////////////////////////////////////////
//
LLWebBrowserTexture::LLWebBrowserTexture( S32 width, S32 height, LLMediaCtrl* browserCtrl, viewer_media_t media_source ) :
	LLViewerDynamicTexture( 512, 512, 4, ORDER_FIRST, TRUE ),
	mNeedsUpdate( true ),
	mNeedsResize( false ),
	mTextureCoordsOpenGL( true ),
	mWebBrowserCtrl( browserCtrl ),
	mMediaSource(media_source)
{
	mElapsedTime.start();

	resize( width, height );
}

////////////////////////////////////////////////////////////////////////////////
//
LLWebBrowserTexture::~LLWebBrowserTexture()
{
	mElapsedTime.stop();
	mMediaSource = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLWebBrowserTexture::needsRender()
{
	bool texture_dirty = false;
	
	if ( mWebBrowserCtrl->getFrequentUpdates() || 
		mWebBrowserCtrl->getAlwaysRefresh() ||
		mWebBrowserCtrl->getForceUpdate() )
	{
		// All of these force an update
		return TRUE;
	}
	
	// If the texture needs updating, render needs to be called.
	if (mMediaSource && mMediaSource->hasMedia())
	{
		LLPluginClassMedia* media = mMediaSource->getMediaPlugin();

		if(media->textureValid() && media->getDirty())
		{
			texture_dirty = true;
		}
	}


	return texture_dirty;
}

////////////////////////////////////////////////////////////////////////////////
//
BOOL LLWebBrowserTexture::render()
{	
	if(updateBrowserTexture())
	{
		// updateBrowserTexture already verified that the media plugin is there and the texture is valid.
		LLPluginClassMedia* media_plugin = mMediaSource->getMediaPlugin();
		LLRect dirty_rect;
		
		if(mNeedsUpdate)
		{
			// If we need an update, use the whole rect instead of the dirty rect.
			dirty_rect.mLeft = 0;
			dirty_rect.mBottom = 0;
			dirty_rect.mRight = media_plugin->getWidth();
			dirty_rect.mTop = media_plugin->getHeight();
		}
		else
		{
			mNeedsUpdate = media_plugin->getDirty(&dirty_rect);
		}
		
		if ( mNeedsUpdate )
		{			
			mNeedsUpdate = false;
			mWebBrowserCtrl->setForceUpdate(false);

			// Constrain the dirty rect to be inside the texture
			S32 x_pos = llmax(dirty_rect.mLeft, 0);
			S32 y_pos = llmax(dirty_rect.mBottom, 0);
			S32 width = llmin(dirty_rect.mRight, getWidth()) - x_pos;
			S32 height = llmin(dirty_rect.mTop, getHeight()) - y_pos;
			
			if(width > 0 && height > 0)
			{
				U8* data = media_plugin->getBitsData();

				// Offset the pixels pointer to match x_pos and y_pos
				data += ( x_pos * media_plugin->getTextureDepth() * media_plugin->getBitsWidth() );
				data += ( y_pos * media_plugin->getTextureDepth() );
				
				setSubImage(
						data, 
						media_plugin->getBitsWidth(), 
						media_plugin->getBitsHeight(),
						x_pos, 
						y_pos, 
						width, 
						height,
						TRUE);	// force a fast update (i.e. don't call analyzeAlpha, etc.)
			}
		
			media_plugin->resetDirty();

			return TRUE;
		};
	};
	
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
//
S32 LLWebBrowserTexture::getMediaWidth()
{
	return mMediaWidth;
}

////////////////////////////////////////////////////////////////////////////////
//
S32 LLWebBrowserTexture::getMediaHeight()
{
	return mMediaHeight;
}

////////////////////////////////////////////////////////////////////////////////
//
void LLWebBrowserTexture::setNeedsUpdate()
{
	mNeedsUpdate = true;
}

////////////////////////////////////////////////////////////////////////////////
//
bool LLWebBrowserTexture::getNeedsUpdate()
{
	return mNeedsUpdate;
}

////////////////////////////////////////////////////////////////////////////////
//
bool LLWebBrowserTexture::getTextureCoordsOpenGL()
{
	return mTextureCoordsOpenGL;
}


////////////////////////////////////////////////////////////////////////////////
//
void LLWebBrowserTexture::resize( S32 new_width, S32 new_height )
{
	F32 scale_ratio = 1.f;
	if (new_width > MAX_DIMENSION)
	{
		scale_ratio = (F32)MAX_DIMENSION / (F32)new_width;
	}
	if (new_height > MAX_DIMENSION)
	{
		scale_ratio = llmin(scale_ratio, (F32)MAX_DIMENSION / (F32)new_height);
	}

	mMediaWidth = llround(scale_ratio * (F32)new_width);
	mMediaHeight = llround(scale_ratio * (F32)new_height);
	
	adjustSize();
}

bool LLWebBrowserTexture::adjustSize()
{
	if (mMediaSource && mMediaSource->hasMedia())
	{
		int natural_width = mMediaSource->getMediaPlugin()->getNaturalWidth();
		int natural_height = mMediaSource->getMediaPlugin()->getNaturalHeight();
		
		if(natural_width != 0)
		{
			// If the media has a "natural size", use it.
			mMediaWidth = natural_width;
			mMediaHeight = natural_height;
		}

		mMediaSource->setSize(mMediaWidth, mMediaHeight);
		mNeedsResize = false;

		return true;
	}
	else
	{
		// The media isn't fully initialized yet, delay the resize until later.
		mNeedsResize = true;
	}
	
	return false;
}

bool LLWebBrowserTexture::updateBrowserTexture()
{
	if (!adjustSize())
		return false;
		
	LLPluginClassMedia* media = mMediaSource->getMediaPlugin();
	
	if(!media->textureValid())
		return false;
	
	if(mMediaSource->mNeedsNewTexture
		|| media->getTextureWidth() != getFullWidth()
		|| media->getTextureHeight() != getFullHeight() )
	{
		//releaseGLTexture();
		
		mFullWidth = media->getTextureWidth();
		mFullHeight = media->getTextureHeight();
		mTextureCoordsOpenGL = media->getTextureCoordsOpenGL();

		const LLColor4U fill_color(0,0,0,255);
		// will create mWidth * mHeight sized texture, using the texture params specified by the media.
		generateGLTexture(
				media->getTextureFormatInternal(), 
				media->getTextureFormatPrimary(), 
				media->getTextureFormatType(), 
				media->getTextureFormatSwapBytes(),
				&fill_color); //Initialize the texture to black.


		mMediaSource->mNeedsNewTexture = false;
	}
	
	return true;
}
// virtual
LLXMLNodePtr LLMediaCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_WEB_BROWSER_CTRL_TAG);

	return node;
}

LLView* LLMediaCtrl::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string name("web_browser");
	node->getAttributeString("name", name);

	std::string start_url("");
	node->getAttributeString("start_url", start_url );

	BOOL border_visible = true;
	node->getAttributeBOOL("border_visible", border_visible);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	LLMediaCtrl* web_browser = new LLMediaCtrl( name, rect );

	if(node->hasAttribute("caret_color"))
	{
		LLColor4 color;
		LLUICtrlFactory::getAttributeColor(node, "caret_color", color);
		LLColor4U colorU = LLColor4U(color);
		web_browser->setCaretColor( colorU.mV[0], colorU.mV[1], colorU.mV[2] );
	}

	BOOL ignore_ui_scale = web_browser->getIgnoreUIScale();
	node->getAttributeBOOL("ignore_ui_scale", ignore_ui_scale);
	web_browser->setIgnoreUIScale((bool)ignore_ui_scale);

	web_browser->initFromXML(node, parent);

	web_browser->setHomePageUrl( start_url );

	web_browser->setBorderVisible( border_visible );

	if(! start_url.empty())
	{
		web_browser->navigateHome();
	}

	return web_browser;
}

std::string LLMediaCtrl::getCurrentNavUrl()
{
	return mCurrentNavUrl;
}

