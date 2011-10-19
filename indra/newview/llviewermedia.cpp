/**
 * @file llviewermedia.cpp
 * @brief Client interface to the media engine
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llviewermedia.h"
#include "llviewermediafocus.h"
#include "llhoverview.h"
#include "llmimetypes.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llviewertexturelist.h"
//#include "viewerversion.h"

#include "llpluginclassmedia.h"

#include "llnotifications.h"
#include "llevent.h"		// LLSimpleListener
#include "llnotificationsutil.h"
#include "lluuid.h"
#include "llkeyboard.h"

// Merov: Temporary definitions while porting the new viewer media code to Snowglobe
const int LEFT_BUTTON  = 0;
const int RIGHT_BUTTON = 1;

///////////////////////////////////////////////////////////////////////////////
// Helper class that tries to download a URL from a web site and calls a method
// on the Panel Land Media and to discover the MIME type
class LLMimeDiscoveryResponder : public LLHTTPClient::Responder
{
LOG_CLASS(LLMimeDiscoveryResponder);
public:
	LLMimeDiscoveryResponder( viewer_media_t media_impl)
		: mMediaImpl(media_impl),
		  mInitialized(false)
	{}



	virtual void completedHeader(U32 status, const std::string& reason, const LLSD& content)
	{
		std::string media_type = content["content-type"].asString();
		std::string::size_type idx1 = media_type.find_first_of(";");
		std::string mime_type = media_type.substr(0, idx1);
		completeAny(status, mime_type);
	}

	virtual void error( U32 status, const std::string& reason )
	{
		// completeAny(status, "none/none");
	}

	void completeAny(U32 status, const std::string& mime_type)
	{
		if(!mInitialized && ! mime_type.empty())
		{
			if (mMediaImpl->initializeMedia(mime_type))
			{
				mInitialized = true;
				mMediaImpl->play();
			}
		}
	}

	public:
		viewer_media_t mMediaImpl;
		bool mInitialized;
};
typedef std::list<LLViewerMediaImpl*> impl_list;
static impl_list sViewerMediaImplList;

//////////////////////////////////////////////////////////////////////////////////////////
// LLViewerMedia

//////////////////////////////////////////////////////////////////////////////////////////
// static
viewer_media_t LLViewerMedia::newMediaImpl(const std::string& media_url,
														 const LLUUID& texture_id,
														 S32 media_width, S32 media_height, U8 media_auto_scale,
														 U8 media_loop,
														 std::string mime_type)
{
	LLViewerMediaImpl* media_impl = getMediaImplFromTextureID(texture_id);
	if(media_impl == NULL || texture_id.isNull())
	{
		// Create the media impl
		media_impl = new LLViewerMediaImpl(media_url, texture_id, media_width, media_height, media_auto_scale, media_loop, mime_type);
		sViewerMediaImplList.push_back(media_impl);
	}
	else
	{
		media_impl->stop();
		media_impl->mTextureId = texture_id;
		media_impl->mMediaURL = media_url;
		media_impl->mMediaWidth = media_width;
		media_impl->mMediaHeight = media_height;
		media_impl->mMediaAutoScale = media_auto_scale;
		media_impl->mMediaLoop = media_loop;
		if(! media_url.empty())
			media_impl->navigateTo(media_url, mime_type, true);
	}
	return media_impl;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::removeMedia(LLViewerMediaImpl* media)
{
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();
	
	for(; iter != end; iter++)
	{
		if(media == *iter)
		{
			sViewerMediaImplList.erase(iter);
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
LLViewerMediaImpl* LLViewerMedia::getMediaImplFromTextureID(const LLUUID& texture_id)
{
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* media_impl = *iter;
		if(media_impl->getMediaTextureID() == texture_id)
		{
			return media_impl;
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
std::string LLViewerMedia::getCurrentUserAgent()
{
	// Don't include version, channel, or skin -- MC

	// Don't use user-visible string to avoid 
	// punctuation and strange characters.
	//std::string skin_name = gSavedSettings.getString("SkinCurrent");

	// Just in case we need to check browser differences in A/B test
	// builds.
	//std::string channel = gSavedSettings.getString("VersionChannelName");

	// append our magic version number string to the browser user agent id
	// See the HTTP 1.0 and 1.1 specifications for allowed formats:
	// http://www.ietf.org/rfc/rfc1945.txt section 10.15
	// http://www.ietf.org/rfc/rfc2068.txt section 3.8
	// This was also helpful:
	// http://www.mozilla.org/build/revised-user-agent-strings.html
	std::ostringstream codec;
	codec << "SecondLife/";
	codec << "C64 Basic V2";
	//codec << ViewerVersion::getImpMajorVersion() << "." << ViewerVersion::getImpMinorVersion() << "." << ViewerVersion::getImpPatchVersion() << " " << ViewerVersion::getImpTestVersion();
 	//codec << " (" << channel << "; " << skin_name << " skin)";
// 	llinfos << codec.str() << llendl;
	
	return codec.str();
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::updateBrowserUserAgent()
{
	std::string user_agent = getCurrentUserAgent();
	
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		LLPluginClassMedia* plugin = pimpl->getMediaPlugin();
		if(plugin && plugin->pluginSupportsMediaBrowser())
		{
			plugin->setBrowserUserAgent(user_agent);
		}
	}

}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::handleSkinCurrentChanged(const LLSD& /*newvalue*/)
{
	// gSavedSettings is already updated when this function is called.
	updateBrowserUserAgent();
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::textureHasMedia(const LLUUID& texture_id)
{
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if(pimpl->getMediaTextureID() == texture_id)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::setVolume(F32 volume)
{
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		pimpl->setVolume(volume);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::updateMedia()
{
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		pimpl->update();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::cleanupClass()
{
	// This is no longer necessary, since the list is no longer smart pointers.
#if 0
	while(!sViewerMediaImplList.empty())
	{
		sViewerMediaImplList.pop_back();
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////
// LLViewerMediaImpl
//////////////////////////////////////////////////////////////////////////////////////////
LLViewerMediaImpl::LLViewerMediaImpl(const std::string& media_url, 
										  const LLUUID& texture_id, 
										  S32 media_width, 
										  S32 media_height, 
										  U8 media_auto_scale, 
										  U8 media_loop,
										  const std::string& mime_type)
:	
	mMovieImageHasMips(false),
	mTextureId(texture_id),
	mMediaWidth(media_width),
	mMediaHeight(media_height),
	mMediaAutoScale(media_auto_scale),
	mMediaLoop(media_loop),
	mMediaURL(media_url),
	mMimeType(mime_type),
	mNeedsNewTexture(true),
	mTextureUsedWidth(0),
	mTextureUsedHeight(0),
	mSuspendUpdates(false),
	mVisible(true)
{ 
	createMediaSource();
}

//////////////////////////////////////////////////////////////////////////////////////////
LLViewerMediaImpl::~LLViewerMediaImpl()
{
	if( gEditMenuHandler == this )
	{
		gEditMenuHandler = NULL;
	}
	
	destroyMediaSource();
	LLViewerMedia::removeMedia(this);
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::initializeMedia(const std::string& mime_type)
{
	if((mPluginBase == NULL) || (mMimeType != mime_type))
	{
		if(! initializePlugin(mime_type))
		{
			LL_WARNS("Plugin") << "plugin intialization failed for mime type: " << mime_type << LL_ENDL;
			LLSD args;
			args["MIME_TYPE"] = mime_type;
			LLNotificationsUtil::add("NoPlugin", args);

			return false;
		}
	}

	// play();
	return (mPluginBase != NULL);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::createMediaSource()
{
	if(! mMediaURL.empty())
	{
		navigateTo(mMediaURL, mMimeType, true);
	}
	else if(! mMimeType.empty())
	{
		initializeMedia(mMimeType);
	}
	
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::destroyMediaSource()
{
	mNeedsNewTexture = true;
	if (!mPluginBase)
	{
		return;
	}
	// Restore the texture
	updateMovieImage(LLUUID::null, false);
	destroyPlugin();
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setMediaType(const std::string& media_type)
{
	mMimeType = media_type;
}

//////////////////////////////////////////////////////////////////////////////////////////
/*static*/
LLPluginClassMedia* LLViewerMediaImpl::newSourceFromMediaType(std::string media_type, LLPluginClassMediaOwner *owner /* may be NULL */, S32 default_width, S32 default_height)
{
	std::string plugin_basename = LLMIMETypes::implType(media_type);

	if(plugin_basename.empty())
	{
		LL_WARNS("Media") << "Couldn't find plugin for media type " << media_type << LL_ENDL;
	}
	else
	{
		std::string launcher_name = gDirUtilp->getLLPluginLauncher();
		std::string plugin_name = gDirUtilp->getLLPluginFilename(plugin_basename);
		std::string user_data_path = gDirUtilp->getOSUserAppDir();
		user_data_path += gDirUtilp->getDirDelimiter();

		// See if the plugin executable exists
		llstat s;
		if(LLFile::stat(launcher_name, &s))
		{
			LL_WARNS("Media") << "Couldn't find launcher at " << launcher_name << LL_ENDL;
			llassert(false);	// Fail in debugging mode.
		}
		else if(LLFile::stat(plugin_name, &s))
		{
			LL_WARNS("Media") << "Couldn't find plugin at " << plugin_name << LL_ENDL;
			llassert(false);	// Fail in debugging mode.
		}
		else
		{
			LLPluginClassMedia* media_source = new LLPluginClassMedia(owner);
			media_source->setSize(default_width, default_height);
			media_source->setUserDataPath(user_data_path);
			media_source->setLanguageCode(LLUI::getLanguage());

			// collect 'cookies enabled' setting from prefs and send to embedded browser
			bool cookies_enabled = gSavedSettings.getBOOL( "BrowserCookiesEnabled" );
			media_source->enable_cookies( cookies_enabled );

			// collect 'plugins enabled' setting from prefs and send to embedded browser
			bool plugins_enabled = gSavedSettings.getBOOL( "BrowserPluginsEnabled" );
			media_source->setPluginsEnabled( plugins_enabled );

			// collect 'javascript enabled' setting from prefs and send to embedded browser
			bool javascript_enabled = gSavedSettings.getBOOL( "BrowserJavascriptEnabled" );
			media_source->setJavascriptEnabled( javascript_enabled );

			if (media_source->init(launcher_name, plugin_name, gSavedSettings.getBOOL("PluginAttachDebuggerToPlugins")))
			{
				return media_source;
			}
			else
			{
				LL_WARNS("Media") << "Failed to init plugin.  Destroying." << LL_ENDL;
				delete media_source;
			}
		}
	}

	LL_WARNS("Plugin") << "plugin intialization failed for mime type: " << media_type << LL_ENDL;
	LLSD args;
	args["MIME_TYPE"] = media_type;
	LLNotificationsUtil::add("NoPlugin", args);

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::initializePlugin(const std::string& media_type)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		// Save the previous media source's last set size before destroying it.
		mMediaWidth = plugin->getSetWidth();
		mMediaHeight = plugin->getSetHeight();
	}
	
	// Always delete the old media impl first.
	destroyMediaSource();
	
	// and unconditionally set the mime type
	mMimeType = media_type;

	LLPluginClassMedia* media_source = newSourceFromMediaType(media_type, this, mMediaWidth, mMediaHeight);
	
	if (media_source)
	{
		media_source->setDisableTimeout(gSavedSettings.getBOOL("DebugPluginDisableTimeout"));
		media_source->setLoop(mMediaLoop);
		media_source->setAutoScale(mMediaAutoScale);
		media_source->setBrowserUserAgent(LLViewerMedia::getCurrentUserAgent());
		
		mPluginBase = media_source;
		return true;
	}

	return false;
}

void LLViewerMediaImpl::setSize(int width, int height)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	mMediaWidth = width;
	mMediaHeight = height;
	if (plugin)
	{
		plugin->setSize(width, height);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::play()
{
	LLPluginClassMedia* plugin = getMediaPlugin();

	// first stop any previously playing media
	// stop();

	// plugin->addObserver( this );
	if (!plugin)
	{
	 	if(!initializePlugin(mMimeType))
		{
			// Plugin failed initialization... should assert or something
			return;
		}
		plugin = getMediaPlugin();
	}
	
	// updateMovieImage(mTextureId, true);

	plugin->loadURI( mMediaURL );
	if(/*plugin->pluginSupportsMediaTime()*/ true)
	{
		start();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::stop()
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		plugin->stop();
		// destroyMediaSource();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::pause()
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		plugin->pause();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::start()
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		plugin->start();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::seek(F32 time)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		plugin->seek(time);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setVolume(F32 volume)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		plugin->setVolume(volume);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::focus(bool focus)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		// call focus just for the hell of it, even though this apopears to be a nop
		plugin->focus(focus);
		if (focus)
		{
			// spoof a mouse click to *actually* pass focus
			// Don't do this anymore -- it actually clicks through now.
//			plugin->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOWN, 1, 1, 0);
//			plugin->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, 1, 1, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseDown(S32 x, S32 y)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (plugin)
	{
		plugin->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOWN, LEFT_BUTTON, x, y, 0);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseUp(S32 x, S32 y)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (plugin)
	{
		plugin->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, LEFT_BUTTON, x, y, 0);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseMove(S32 x, S32 y)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (plugin)
	{
		plugin->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_MOVE, LEFT_BUTTON, x, y, 0);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseLeftDoubleClick(S32 x, S32 y)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (plugin)
	{
		plugin->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOUBLE_CLICK, LEFT_BUTTON, x, y, 0);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::onMouseCaptureLost()
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		plugin->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, LEFT_BUTTON, mLastMouseX, mLastMouseY, 0);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
BOOL LLViewerMediaImpl::handleMouseUp(S32 x, S32 y, MASK mask) 
{ 
	// NOTE: this is called when the mouse is released when we have capture.
	// Due to the way mouse coordinates are mapped to the object, we can't use the x and y coordinates that come in with the event.
	
	if(hasMouseCapture())
	{
		// Release the mouse -- this will also send a mouseup to the media
		gFocusMgr.setMouseCapture( FALSE );
	}

	return TRUE; 
}
//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateHome()
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		plugin->loadURI( mHomeURL );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateTo(const std::string& url, const std::string& mime_type,  bool rediscover_type)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if(rediscover_type)
	{

		LLURI uri(url);
		std::string scheme = uri.scheme();

		if(scheme.empty() || ("http" == scheme || "https" == scheme))
		{
			if(mime_type.empty())
			{
				LLHTTPClient::getHeaderOnly( url, new LLMimeDiscoveryResponder(this));
			}
			else if(initializeMedia(mime_type) && (plugin = getMediaPlugin()))
			{
				plugin->loadURI( url );
			}
		}
		else if("data" == scheme || "file" == scheme || "about" == scheme)
		{
			// FIXME: figure out how to really discover the type for these schemes
			// We use "data" internally for a text/html url for loading the login screen
			if(initializeMedia("text/html") && (plugin = getMediaPlugin()))
			{
				plugin->loadURI( url );
			}
		}
		else
		{
			// This catches 'rtsp://' urls
			if(initializeMedia(scheme) && (plugin = getMediaPlugin()))
			{
				plugin->loadURI( url );
			}
		}
	}
	else if (plugin)
	{
		plugin->loadURI( url );
	}
	else if(initializeMedia(mime_type) && (plugin = getMediaPlugin()))
	{
		plugin->loadURI( url );
	}
	else
	{
		LL_WARNS("Media") << "Couldn't navigate to: " << url << " as there is no media type for: " << mime_type << LL_ENDL;
		return;
	}
	mMediaURL = url;

}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateStop()
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		plugin->browse_stop();
	}

}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::handleKeyHere(KEY key, MASK mask)
{
	bool result = false;
	LLPluginClassMedia* plugin = getMediaPlugin();
	
	if (plugin)
	{
		// FIXME: THIS IS SO WRONG.
		// Menu keys should be handled by the menu system and not passed to UI elements, but this is how LLTextEditor and LLLineEditor do it...
		if( MASK_CONTROL & mask )
		{
			if( 'C' == key )
			{
				plugin->copy();
				result = true;
			}
			else
			if( 'V' == key )
			{
				plugin->paste();
				result = true;
			}
			else
			if( 'X' == key )
			{
				plugin->cut();
				result = true;
			}
		}
		
		if(!result)
		{
			
			LLSD native_key_data = LLSD::emptyMap(); 
			
			result = plugin->keyEvent(LLPluginClassMedia::KEY_EVENT_DOWN ,key, mask, native_key_data);
			// Since the viewer internal event dispatching doesn't give us key-up events, simulate one here.
			(void)plugin->keyEvent(LLPluginClassMedia::KEY_EVENT_UP ,key, mask, native_key_data);
		}
	}
	
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::handleUnicodeCharHere(llwchar uni_char)
{
	bool result = false;
	LLPluginClassMedia* plugin = getMediaPlugin();
	
	if (plugin)
	{
		// only accept 'printable' characters, sigh...
		if (uni_char >= 32 // discard 'control' characters
			&& uni_char != 127) // SDL thinks this is 'delete' - yuck.
		{
			LLSD native_key_data = LLSD::emptyMap(); 
			
			plugin->textInput(wstring_to_utf8str(LLWString(1, uni_char)), gKeyboard->currentMask(FALSE), native_key_data);
		}
	}
	
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::canNavigateForward()
{
	bool result = false;
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		result = plugin->getHistoryForwardAvailable();
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::canNavigateBack()
{
	bool result = false;
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
	{
		result = plugin->getHistoryBackAvailable();
	}
	return result;
}


//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::updateMovieImage(const LLUUID& uuid, BOOL active)
{
	// IF the media image hasn't changed, do nothing
	if (mTextureId == uuid)
	{
		return;
	}
	// If we have changed media uuid, restore the old one
	if (!mTextureId.isNull())
	{
		LLViewerTexture* oldImage = LLViewerTextureManager::findTexture( mTextureId );
		if (oldImage) 
		{
			// Casting to LLViewerMediaTexture is a huge hack. Implement LLViewerMediaTexture some time later.
			((LLViewerMediaTexture*)oldImage)->reinit(mMovieImageHasMips);
			oldImage->mIsMediaTexture = FALSE;
		}
	}
	// If the movie is playing, set the new media image
	if (active && !uuid.isNull())
	{
		LLViewerTexture* viewerImage = LLViewerTextureManager::findTexture( uuid );
		if( viewerImage )
		{
			mTextureId = uuid;
			// Can't use mipmaps for movies because they don't update the full image
			// Casting to LLViewerMediaTexture is a huge hack. Implement LLViewerMediaTexture some time later.
			mMovieImageHasMips = ((LLViewerMediaTexture*)viewerImage)->getUseMipMaps();
			((LLViewerMediaTexture*)viewerImage)->reinit(FALSE);
			viewerImage->mIsMediaTexture = TRUE;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::update()
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (!plugin)
	{
		return;
	}
	
	plugin->idle();
	
	if (plugin->isPluginExited())
	{
		destroyMediaSource();
		return;
	}

	if (!plugin->textureValid())
	{
		return;
	}
	
	if(mSuspendUpdates || !mVisible)
	{
		return;
	}
	
	LLViewerTexture* placeholder_image = updatePlaceholderImage();
		
	if(placeholder_image)
	{
		LLRect dirty_rect;
		if (plugin->getDirty(&dirty_rect))
		{
			// Constrain the dirty rect to be inside the texture
			S32 x_pos = llmax(dirty_rect.mLeft, 0);
			S32 y_pos = llmax(dirty_rect.mBottom, 0);
			S32 width = llmin(dirty_rect.mRight, placeholder_image->getWidth()) - x_pos;
			S32 height = llmin(dirty_rect.mTop, placeholder_image->getHeight()) - y_pos;
			
			if(width > 0 && height > 0)
			{

				U8* data = plugin->getBitsData();

				// Offset the pixels pointer to match x_pos and y_pos
				data += ( x_pos * plugin->getTextureDepth() * plugin->getBitsWidth() );
				data += ( y_pos * plugin->getTextureDepth() );
				
				placeholder_image->setSubImage(
						data, 
						plugin->getBitsWidth(), 
						plugin->getBitsHeight(),
						x_pos, 
						y_pos, 
						width, 
						height,
						TRUE);		// force a fast update (i.e. don't call analyzeAlpha, etc.)

			}
			
			plugin->resetDirty();
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::updateImagesMediaStreams()
{
}


//////////////////////////////////////////////////////////////////////////////////////////
/*LLViewerMediaTexture*/LLViewerTexture* LLViewerMediaImpl::updatePlaceholderImage()
{
	if(mTextureId.isNull())
	{
		// The code that created this instance will read from the plugin's bits.
		return NULL;
	}
	
	LLViewerMediaTexture* placeholder_image = (LLViewerMediaTexture*)LLViewerTextureManager::getFetchedTexture( mTextureId );
	LLPluginClassMedia* plugin = getMediaPlugin();

	placeholder_image->getLastReferencedTimer()->reset();

	if (mNeedsNewTexture 
		|| placeholder_image->getUseMipMaps()
		|| ! placeholder_image->mIsMediaTexture
		|| (placeholder_image->getWidth() != plugin->getTextureWidth())
		|| (placeholder_image->getHeight() != plugin->getTextureHeight())
		|| (mTextureUsedWidth != plugin->getWidth())
		|| (mTextureUsedHeight != plugin->getHeight())
		)
	{
		llinfos << "initializing media placeholder" << llendl;
		llinfos << "movie image id " << mTextureId << llendl;

		int texture_width = plugin->getTextureWidth();
		int texture_height = plugin->getTextureHeight();
		int texture_depth = plugin->getTextureDepth();
		
		// MEDIAOPT: check to see if size actually changed before doing work
		placeholder_image->destroyGLTexture();
		// MEDIAOPT: apparently just calling setUseMipMaps(FALSE) doesn't work?
		placeholder_image->reinit(FALSE);	// probably not needed

		// MEDIAOPT: seems insane that we actually have to make an imageraw then
		// immediately discard it
		LLPointer<LLImageRaw> raw = new LLImageRaw(texture_width, texture_height, texture_depth);
		raw->clear(0x0f, 0x0f, 0x0f, 0xff);
		int discard_level = 0;

		// ask media source for correct GL image format constants
		placeholder_image->setExplicitFormat(plugin->getTextureFormatInternal(),
											 plugin->getTextureFormatPrimary(),
											 plugin->getTextureFormatType(),
											 plugin->getTextureFormatSwapBytes());

		placeholder_image->createGLTexture(discard_level, raw);

		// placeholder_image->setExplicitFormat()
		placeholder_image->setUseMipMaps(FALSE);

		// MEDIAOPT: set this dynamically on play/stop
		placeholder_image->mIsMediaTexture = true;
		mNeedsNewTexture = false;
				
		// If the amount of the texture being drawn by the media goes down in either width or height, 
		// recreate the texture to avoid leaving parts of the old image behind.
		mTextureUsedWidth = plugin->getWidth();
		mTextureUsedHeight = plugin->getHeight();
	}
	
	return placeholder_image;
}


//////////////////////////////////////////////////////////////////////////////////////////
LLUUID LLViewerMediaImpl::getMediaTextureID()
{
	return mTextureId;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setVisible(bool visible)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	mVisible = visible;
	
	if(mVisible)
	{
		if(plugin && plugin->isPluginExited())
		{
			destroyMediaSource();
		}
		
		if(!plugin)
		{
			createMediaSource();
		}
	}
	
	if(plugin)
	{
		plugin->setPriority(mVisible?LLPluginClassBasic::PRIORITY_NORMAL:LLPluginClassBasic::PRIORITY_SLEEP);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseCapture()
{
	gFocusMgr.setMouseCapture(this);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::getTextureSize(S32 *texture_width, S32 *texture_height)
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if(plugin && plugin->textureValid())
	{
		S32 real_texture_width = plugin->getBitsWidth();
		S32 real_texture_height = plugin->getBitsHeight();

		{
			// The "texture width" coming back from the plugin may not be a power of two (thanks to webkit).
			// It will be the correct "data width" to pass to setSubImage
			int i;
			
			for(i = 1; i < real_texture_width; i <<= 1)
				;
			*texture_width = i;

			for(i = 1; i < real_texture_height; i <<= 1)
				;
			*texture_height = i;
		}
			
	}
	else
	{
		*texture_width = 0;
		*texture_height = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::scaleMouse(S32 *mouse_x, S32 *mouse_y)
{
#if 0
	S32 media_width, media_height;
	S32 texture_width, texture_height;
	getMediaSize( &media_width, &media_height );
	getTextureSize( &texture_width, &texture_height );
	S32 y_delta = texture_height - media_height;

	*mouse_y -= y_delta;
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::isMediaPlaying()
{
	bool result = false;
	LLPluginClassMedia* plugin = getMediaPlugin();
	
	if(plugin)
	{
		EMediaStatus status = plugin->getStatus();
		if(status == MEDIA_PLAYING || status == MEDIA_LOADING)
			result = true;
	}
	
	return result;
}
//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::isMediaPaused()
{
	bool result = false;
	LLPluginClassMedia* plugin = getMediaPlugin();

	if(plugin)
	{
		if(plugin->getStatus() == MEDIA_PAUSED)
			result = true;
	}
	
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::hasMedia()
{
	return mPluginBase != NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::handleMediaEvent(LLPluginClassMedia* self, LLPluginClassMediaOwner::EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			LLSD args;
			args["PLUGIN"] = LLMIMETypes::implType(mMimeType);
			LLNotificationsUtil::add("MediaPluginFailed", args);
		}
		break;
		default:
		break;
	}
	// Just chain the event to observers.
	emitEvent(self, event);
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::cut()
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
		plugin->cut();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canCut() const
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
		return plugin->canCut();
	else
		return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::copy()
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
		plugin->copy();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canCopy() const
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
		return plugin->canCopy();
	else
		return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::paste()
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
		plugin->paste();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canPaste() const
{
	LLPluginClassMedia* plugin = getMediaPlugin();
	if (plugin)
		return plugin->canPaste();
	else
		return FALSE;
}

