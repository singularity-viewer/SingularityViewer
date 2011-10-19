/**
 * @file llviewerparcelmedia.cpp
 * @brief Handlers for multimedia on a per-parcel basis
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
#include "llviewerparcelmedia.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerregion.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "lluuid.h"
#include "message.h"
#include "llviewermediafocus.h"
#include "llviewerparcelmediaautoplay.h"
#include "llnotificationsutil.h"
#include "llviewerwindow.h"
#include "llfirstuse.h"
#include "llpluginclassmedia.h"
#include "llnotify.h"
#include "llsdserialize.h"
#include "llaudioengine.h"
#include "lloverlaybar.h"
#include "slfloatermediafilter.h"

// Static Variables

S32 LLViewerParcelMedia::sMediaParcelLocalID = 0;
LLUUID LLViewerParcelMedia::sMediaRegionID;
viewer_media_t LLViewerParcelMedia::sMediaImpl;
bool LLViewerParcelMedia::sIsUserAction = false;
bool LLViewerParcelMedia::sMediaFilterListLoaded = false;
LLSD LLViewerParcelMedia::sMediaFilterList;
std::set<std::string> LLViewerParcelMedia::sMediaQueries;
std::set<std::string> LLViewerParcelMedia::sAllowedMedia;
std::set<std::string> LLViewerParcelMedia::sDeniedMedia;

// Local functions
bool callback_play_media(const LLSD& notification, const LLSD& response, LLParcel* parcel);
void callback_media_alert(const LLSD& notification, const LLSD& response, LLParcel* parcel, U32 type, std::string domain);

// static
void LLViewerParcelMedia::initClass()
{
	LLMessageSystem* msg = gMessageSystem;
	msg->setHandlerFunc("ParcelMediaCommandMessage", processParcelMediaCommandMessage );
	msg->setHandlerFunc("ParcelMediaUpdate", processParcelMediaUpdate );
	LLViewerParcelMediaAutoPlay::initClass();
}

//static 
void LLViewerParcelMedia::cleanupClass()
{
	// This needs to be destroyed before global destructor time.
	sMediaImpl = NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::update(LLParcel* parcel)
{
	if (/*LLViewerMedia::hasMedia()*/ true)
	{
		// we have a player
		if (parcel)
		{
			if(!gAgent.getRegion())
			{
				sMediaRegionID = LLUUID() ;
				stop() ;
				LL_DEBUGS("Media") << "no agent region, bailing out." << LL_ENDL;
				return ;				
			}

			// we're in a parcel
			bool new_parcel = false;
			S32 parcelid = parcel->getLocalID();						

			LLUUID regionid = gAgent.getRegion()->getRegionID();
			if (parcelid != sMediaParcelLocalID || regionid != sMediaRegionID)
			{
				LL_DEBUGS("Media") << "New parcel, parcel id = " << parcelid << ", region id = " << regionid << LL_ENDL;
				sMediaParcelLocalID = parcelid;
				sMediaRegionID = regionid;
				new_parcel = true;
			}

			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			std::string mediaCurrentUrl = std::string( parcel->getMediaCurrentURL());

			// First use warning
			if(	! mediaUrl.empty() && gSavedSettings.getWarning("FirstStreamingVideo") )
			{
				LLNotificationsUtil::add("ParcelCanPlayMedia", LLSD(), LLSD(),
					boost::bind(callback_play_media, _1, _2, parcel));
				return;

			}

			// if we have a current (link sharing) url, use it instead
			if (mediaCurrentUrl != "" && parcel->getMediaType() == "text/html")
			{
				mediaUrl = mediaCurrentUrl;
			}
			
			LLStringUtil::trim(mediaUrl);
			
			// If no parcel media is playing, nothing left to do
			if(sMediaImpl.isNull())

			{
				return;
			}

			// Media is playing...has something changed?
			else if (( sMediaImpl->getMediaURL() != mediaUrl )
				|| ( sMediaImpl->getMediaTextureID() != parcel->getMediaID() )
				|| ( sMediaImpl->getMimeType() != parcel->getMediaType() ))
			{
				// Only play if the media types are the same.
				if(sMediaImpl->getMimeType() == parcel->getMediaType())
				{
					play(parcel);
				}

				else
				{
					stop();
				}
			}
		}
		else
		{
			stop();
		}
	}
	/*
	else
	{
		// no audio player, do a first use dialog if there is media here
		if (parcel)
		{
			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			if (!mediaUrl.empty ())
			{
				if (gSavedSettings.getWarning("QuickTimeInstalled"))
				{
					gSavedSettings.setWarning("QuickTimeInstalled", FALSE);

					LLNotificationsUtil::add("NoQuickTime" );
				};
			}
		}
	}
	*/
}

// static
void LLViewerParcelMedia::play(LLParcel* parcel, bool filter)
{
	lldebugs << "LLViewerParcelMedia::play" << llendl;

	if (!parcel) return;

	if (!gSavedSettings.getBOOL("AudioStreamingVideo"))
		return;

	std::string media_url = parcel->getMediaURL();
	LLStringUtil::trim(media_url);

	if (!media_url.empty() && gSavedSettings.getBOOL("MediaEnableFilter") && (filter || !allowedMedia(media_url)))
	{
		// If filtering is needed or in case media_url just changed
		// to something we did not yet approve.
		LLViewerParcelMediaAutoPlay::playStarted();
		filterMedia(parcel, 0);
		return;
	}

	std::string mime_type = parcel->getMediaType();
	LLUUID placeholder_texture_id = parcel->getMediaID();
	U8 media_auto_scale = parcel->getMediaAutoScale();
	U8 media_loop = parcel->getMediaLoop();
	S32 media_width = parcel->getMediaWidth();
	S32 media_height = parcel->getMediaHeight();

	// Debug print
	// LL_DEBUGS("Media") << "Play media type : " << mime_type << ", url : " << media_url << LL_ENDL;

	if (!sMediaImpl || (sMediaImpl &&
						(sMediaImpl->getMediaURL() != media_url ||
						 sMediaImpl->getMimeType() != mime_type ||
						 sMediaImpl->getMediaTextureID() != placeholder_texture_id)))
	{
		if (sMediaImpl)
		{
			// Delete the old media impl first so they don't fight over the texture.
			sMediaImpl->stop();
		}

		LL_DEBUGS("Media") << "new media impl with mime type " << mime_type << ", url " << media_url << LL_ENDL;

		// There is no media impl, or it has just been deprecated, make a new one
			sMediaImpl = LLViewerMedia::newMediaImpl(media_url, placeholder_texture_id,
				media_width, media_height, media_auto_scale,
				media_loop, mime_type);
		}

	// The url, mime type and texture are now the same, call play again
	if (sMediaImpl->getMediaURL() == media_url 
		&& sMediaImpl->getMimeType() == mime_type
		&& sMediaImpl->getMediaTextureID() == placeholder_texture_id)
	{
		LL_DEBUGS("Media") << "playing with existing url " << media_url << LL_ENDL;

		sMediaImpl->play();
	}
	
	LLFirstUse::useMedia();

	LLViewerParcelMediaAutoPlay::playStarted();
}

// static
void LLViewerParcelMedia::stop()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	
	// We need to remove the media HUD if it is up.
	LLViewerMediaFocus::getInstance()->clearFocus();

	// This will kill the media instance.
	sMediaImpl->stop();
	sMediaImpl = NULL;
}

// static
void LLViewerParcelMedia::pause()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->pause();
}

// static
void LLViewerParcelMedia::start()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->start();

	LLFirstUse::useMedia();

	LLViewerParcelMediaAutoPlay::playStarted();
}

// static
void LLViewerParcelMedia::seek(F32 time)
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->seek(time);
}

// static
void LLViewerParcelMedia::focus(bool focus)
{
	sMediaImpl->focus(focus);
}

// static
LLViewerMediaImpl::EMediaStatus LLViewerParcelMedia::getStatus()
{	
	LLViewerMediaImpl::EMediaStatus result = LLViewerMediaImpl::MEDIA_NONE;
	
	if(sMediaImpl.notNull() && sMediaImpl->hasMedia())
	{
		result = sMediaImpl->getMediaPlugin()->getStatus();
	}
	
	return result;
}

// static
std::string LLViewerParcelMedia::getMimeType()
{
	return sMediaImpl.notNull() ? sMediaImpl->getMimeType() : "none/none";
}
viewer_media_t LLViewerParcelMedia::getParcelMedia()
{
	return sMediaImpl;
}
//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::processParcelMediaCommandMessage( LLMessageSystem *msg, void ** )
{
	// extract the agent id
	//	LLUUID agent_id;
	//	msg->getUUID( agent_id );

	U32 flags;
	U32 command;
	F32 time;
	msg->getU32( "CommandBlock", "Flags", flags );
	msg->getU32( "CommandBlock", "Command", command);
	msg->getF32( "CommandBlock", "Time", time );

	if (flags &( (1<<PARCEL_MEDIA_COMMAND_STOP)
				| (1<<PARCEL_MEDIA_COMMAND_PAUSE)
				| (1<<PARCEL_MEDIA_COMMAND_PLAY)
				| (1<<PARCEL_MEDIA_COMMAND_LOOP)
				| (1<<PARCEL_MEDIA_COMMAND_UNLOAD) ))
	{
		// stop
		if( command == PARCEL_MEDIA_COMMAND_STOP )
		{
			stop();
		}
		else
		// pause
		if( command == PARCEL_MEDIA_COMMAND_PAUSE )
		{
			pause();
		}
		else
		// play
		if(( command == PARCEL_MEDIA_COMMAND_PLAY ) ||
		   ( command == PARCEL_MEDIA_COMMAND_LOOP ))
		{
			if (getStatus() == LLViewerMediaImpl::MEDIA_PAUSED)
			{
				start();
			}
			else
			{
				LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
				play(parcel);
			}
		}
		else
		// unload
		if( command == PARCEL_MEDIA_COMMAND_UNLOAD )
		{
			stop();
		}
	}

	if (flags & (1<<PARCEL_MEDIA_COMMAND_TIME))
	{
		if(sMediaImpl.isNull())
		{
			LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
			play(parcel);
		}
		seek(time);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::processParcelMediaUpdate( LLMessageSystem *msg, void ** )
{
	LLUUID media_id;
	std::string media_url;
	std::string media_type;
	S32 media_width = 0;
	S32 media_height = 0;
	U8 media_auto_scale = FALSE;
	U8 media_loop = FALSE;

	msg->getUUID( "DataBlock", "MediaID", media_id );
	char media_url_buffer[257];
	msg->getString( "DataBlock", "MediaURL", 255, media_url_buffer );
	media_url = media_url_buffer;
	msg->getU8("DataBlock", "MediaAutoScale", media_auto_scale);

	if (msg->has("DataBlockExtended")) // do we have the extended data?
	{
		char media_type_buffer[257];
		msg->getString("DataBlockExtended", "MediaType", 255, media_type_buffer);
		media_type = media_type_buffer;
		msg->getU8("DataBlockExtended", "MediaLoop", media_loop);
		msg->getS32("DataBlockExtended", "MediaWidth", media_width);
		msg->getS32("DataBlockExtended", "MediaHeight", media_height);
	}

	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	BOOL same = FALSE;
	if (parcel)
	{
		same = ((parcel->getMediaURL() == media_url) &&
				(parcel->getMediaType() == media_type) &&
				(parcel->getMediaID() == media_id) &&
				(parcel->getMediaWidth() == media_width) &&
				(parcel->getMediaHeight() == media_height) &&
				(parcel->getMediaAutoScale() == media_auto_scale) &&
				(parcel->getMediaLoop() == media_loop));

		if (!same)
		{
			// temporarily store these new values in the parcel
			parcel->setMediaURL(media_url);
			parcel->setMediaType(media_type);
			parcel->setMediaID(media_id);
			parcel->setMediaWidth(media_width);
			parcel->setMediaHeight(media_height);
			parcel->setMediaAutoScale(media_auto_scale);
			parcel->setMediaLoop(media_loop);

			play(parcel);
		}
	}
}
// Static
/////////////////////////////////////////////////////////////////////////////////////////
void LLViewerParcelMedia::sendMediaNavigateMessage(const std::string& url)
{
	std::string region_url = gAgent.getRegion()->getCapability("ParcelNavigateMedia");
	if (!region_url.empty())
	{
		// send navigate event to sim for link sharing
		LLSD body;
		body["agent-id"] = gAgent.getID();
		body["local-id"] = LLViewerParcelMgr::getInstance()->getAgentParcel()->getLocalID();
		body["url"] = url;
		LLHTTPClient::post(region_url, body, new LLHTTPClient::Responder);
	}
	else
	{
		llwarns << "can't get ParcelNavigateMedia capability" << llendl;
	}

}

/////////////////////////////////////////////////////////////////////////////////////////
// inherited from LLViewerMediaObserver
// virtual 
void LLViewerParcelMedia::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
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
		};
		break;
		
		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CURSOR_CHANGED, new cursor is " << self->getCursorName() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_BEGIN " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_COMPLETE, result string is: " << self->getNavigateResultString() << LL_ENDL;
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
		};
		break;
		
		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri is " << self->getClickURL() << LL_ENDL;
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

		case MEDIA_EVENT_NAVIGATE_ERROR_PAGE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_ERROR_PAGE" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CLOSE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLOSE_REQUEST" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PICK_FILE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PICK_FILE_REQUEST" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_GEOMETRY_CHANGE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_GEOMETRY_CHANGE" << LL_ENDL;
		};
		break;
			
		case MEDIA_EVENT_AUTH_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_AUTH_REQUEST" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_LINK_HOVERED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_LINK_HOVERED" << LL_ENDL;
		};
		break;
		
		default:
		{
			LL_WARNS("Media") <<  "Media event:  unknown event type" << LL_ENDL;
		};
	};
}

bool callback_play_media(const LLSD& notification, const LLSD& response, LLParcel* parcel)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", TRUE);
		LLViewerParcelMedia::play(parcel);
	}
	else
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", FALSE);
	}
	gSavedSettings.setWarning("FirstStreamingVideo", FALSE);
	return false;
}

// TODO: observer
/*
void LLViewerParcelMediaNavigationObserver::onNavigateComplete( const EventType& event_in )
{
	std::string url = event_in.getStringValue();

	if (mCurrentURL != url && ! mFromMessage)
	{
		LLViewerParcelMedia::sendMediaNavigateMessage(url);
	}

	mCurrentURL = url;
	mFromMessage = false;

}
*/

void LLViewerParcelMedia::playStreamingMusic(LLParcel* parcel, bool filter)
{
	std::string music_url = parcel->getMusicURL();
	LLStringUtil::trim(music_url);
	if (!music_url.empty() && gSavedSettings.getBOOL("MediaEnableFilter") && (filter || !allowedMedia(music_url)))
	{
		// If filtering is needed or in case music_url just changed
		// to something we did not yet approve.
		filterMedia(parcel, 1);
	}
	else if (gAudiop)
	{
		LLStringUtil::trim(music_url);
		gAudiop->startInternetStream(music_url);
		if (music_url.empty())
		{
			LLOverlayBar::audioFilterStop();
		}
		else
		{
			LLOverlayBar::audioFilterPlay();
		}
	}
}

void LLViewerParcelMedia::stopStreamingMusic()
{
	if (gAudiop)
	{
		gAudiop->stopInternetStream();
		LLOverlayBar::audioFilterStop();
	}
}

bool LLViewerParcelMedia::allowedMedia(std::string media_url)
{
	LLStringUtil::trim(media_url);
	std::string domain = extractDomain(media_url);
	LLHost host;
	host.setHostByName(domain);
	std::string ip = host.getIPString();
	if (sAllowedMedia.count(domain) || sAllowedMedia.count(ip))
	{
		return true;
	}
	std::string server;
	for (S32 i = 0; i < (S32)sMediaFilterList.size(); i++)
	{
		server = sMediaFilterList[i]["domain"].asString();
		if (server == domain || server == ip)
		{
			if (sMediaFilterList[i]["action"].asString() == "allow")
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

void LLViewerParcelMedia::filterMedia(LLParcel* parcel, U32 type)
{
	std::string media_action;
	std::string media_url;
	std::string domain;
	std::string ip;

	if (parcel != LLViewerParcelMgr::getInstance()->getAgentParcel())
	{
		// The parcel just changed (may occur right out after a TP)
		sIsUserAction = false;
		return;
	}		

	if (type == 0)
	{
		media_url = parcel->getMediaURL();
	}
	else
	{
		media_url = parcel->getMusicURL();
	}
	LLStringUtil::trim(media_url);

	domain = extractDomain(media_url);

	if (sMediaQueries.count(domain) > 0)
	{
		sIsUserAction = false;
		return;
	}

	LLHost host;
	host.setHostByName(domain);
	ip = host.getIPString();

	if (sIsUserAction)
	{
		// This was a user manual request to play this media, so give
		// it another chance...
		sIsUserAction = false;
		bool dirty = false;
		if (sDeniedMedia.count(domain))
		{
			sDeniedMedia.erase(domain);
			dirty = true;
		}
		if (sDeniedMedia.count(ip))
		{
			sDeniedMedia.erase(ip);
			dirty = true;
		}
		if (dirty)
		{
			SLFloaterMediaFilter::setDirty();
		}
	}

	if (media_url.empty())
	{
		media_action == "allow";
	}
	else if (!sMediaFilterListLoaded || sDeniedMedia.count(domain) || sDeniedMedia.count(ip))
	{
		media_action = "ignore";
	}
	else if (sAllowedMedia.count(domain) || sAllowedMedia.count(ip))
	{
		media_action = "allow";
	}
	else
	{
		std::string server;
		for (S32 i = 0; i < (S32)sMediaFilterList.size(); i++)
		{
			server = sMediaFilterList[i]["domain"].asString();
			if (server == domain || server == ip)
			{
				media_action = sMediaFilterList[i]["action"].asString();
				break;
			}
		}
	}

	if (media_action == "allow")
	{
		if (type == 0)
		{
			play(parcel, false);
		}
		else
		{
			playStreamingMusic(parcel, false);
		}
		return;
	}
	if (media_action == "ignore")
	{
		if (type == 1)
		{
			LLViewerParcelMedia::stopStreamingMusic();
		}
		return;
	}

	LLSD args;
	if (ip != domain && domain.find('/') == std::string::npos)
	{
		args["DOMAIN"] = domain + " (" + ip + ")";
	}
	else
	{
		args["DOMAIN"] = domain;
	}

	if (media_action == "deny")
	{
		LLNotificationsUtil::add("MediaBlocked", args);
		if (type == 1)
		{
			LLViewerParcelMedia::stopStreamingMusic();
		}
		// So to avoid other "blocked" messages later in the session
		// for this url should it be requested again by a script.
		// We don't add the IP, on purpose (want to show different
		// blocks for different domains pointing to the same IP).
		sDeniedMedia.insert(domain);
	}
	else
	{
		sMediaQueries.insert(domain);
		args["URL"] = media_url;
		if (type == 0)
		{
			args["TYPE"] = "media";
		}
		else
		{
			args["TYPE"] = "audio";
		}
		LLNotificationsUtil::add("MediaAlert", args, LLSD(), boost::bind(callback_media_alert, _1, _2, parcel, type, domain));
	}
}

void callback_media_alert(const LLSD &notification, const LLSD &response, LLParcel* parcel, U32 type, std::string domain)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	LLHost host;
	host.setHostByName(domain);
	std::string ip = host.getIPString();

	LLSD args;
	if (ip != domain && domain.find('/') == std::string::npos)
	{
		args["DOMAIN"] = domain + " (" + ip + ")";
	}
	else
	{
		args["DOMAIN"] = domain;
	}

	if (option == 0 || option == 3) // Allow or Whitelist
	{
		LLViewerParcelMedia::sAllowedMedia.insert(domain);
		if (option == 3) // Whitelist
		{
			LLSD newmedia;
			newmedia["domain"] = domain;
			newmedia["action"] = "allow";
			LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			if (ip != domain && domain.find('/') == std::string::npos)
			{
				newmedia["domain"] = ip;
				LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			}
			LLViewerParcelMedia::saveDomainFilterList();
			args["LISTED"] = "whitelisted";
			LLNotificationsUtil::add("MediaListed", args);
		}
		if (type == 0)
		{
			LLViewerParcelMedia::play(parcel, false);
		}
		else
		{
			LLViewerParcelMedia::playStreamingMusic(parcel, false);
		}
	}
	else if (option == 1 || option == 2) // Deny or Blacklist
	{
		LLViewerParcelMedia::sDeniedMedia.insert(domain);
		if (ip != domain && domain.find('/') == std::string::npos)
		{
			LLViewerParcelMedia::sDeniedMedia.insert(ip);
		}
		if (type == 1)
		{
			LLViewerParcelMedia::stopStreamingMusic();
		}
		if (option == 1) // Deny
		{
			LLNotificationsUtil::add("MediaBlocked", args);
		}
		else // Blacklist
		{
			LLSD newmedia;
			newmedia["domain"] = domain;
			newmedia["action"] = "deny";
			LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			if (ip != domain && domain.find('/') == std::string::npos)
			{
				newmedia["domain"] = ip;
				LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			}
			LLViewerParcelMedia::saveDomainFilterList();
			args["LISTED"] = "blacklisted";
			LLNotificationsUtil::add("MediaListed", args);
		}
	}

	LLViewerParcelMedia::sMediaQueries.erase(domain);
	SLFloaterMediaFilter::setDirty();
}

void LLViewerParcelMedia::saveDomainFilterList()
{
	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "media_filter.xml");

	llofstream medialistFile(medialist_filename);
	LLSDSerialize::toPrettyXML(sMediaFilterList, medialistFile);
	medialistFile.close();
}

bool LLViewerParcelMedia::loadDomainFilterList()
{
	sMediaFilterListLoaded = true;

	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "media_filter.xml");

	if (!LLFile::isfile(medialist_filename))
	{
		LLSD emptyllsd;
		llofstream medialistFile(medialist_filename);
		LLSDSerialize::toPrettyXML(emptyllsd, medialistFile);
		medialistFile.close();
	}

	if (LLFile::isfile(medialist_filename))
	{
		llifstream medialistFile(medialist_filename);
		LLSDSerialize::fromXML(sMediaFilterList, medialistFile);
		medialistFile.close();
		SLFloaterMediaFilter::setDirty();
		return true;
	}
	else
	{
		return false;
	}
}

void LLViewerParcelMedia::clearDomainFilterList()
{
	sMediaFilterList.clear();
	sAllowedMedia.clear();
	sDeniedMedia.clear();
	saveDomainFilterList();
	LLNotificationsUtil::add("MediaFiltersCleared");
	SLFloaterMediaFilter::setDirty();
}

std::string LLViewerParcelMedia::extractDomain(std::string url)
{
	static std::string last_region = "@";

	if (url.empty())
	{
		return url;
	}

	LLStringUtil::toLower(url);

	size_t pos = url.find("//");

	if (pos != std::string::npos)
	{
		size_t count = url.size() - pos + 2;
		url = url.substr(pos + 2, count);
	}

	// Check that there is at least one slash in the URL and add a trailing
	// one if not (for media/audio URLs such as http://mydomain.net)
	if (url.find('/') == std::string::npos)
	{
		url += '/';
	}

	// If there's a user:password@ part, remove it
	pos = url.find('@');
	if (pos != std::string::npos && pos < url.find('/'))	// if '@' is not before the first '/', then it's not a user:password
	{
		size_t count = url.size() - pos + 1;
		url = url.substr(pos + 1, count);
	}

	if (url.find(gAgent.getRegion()->getHost().getHostName()) == 0 || url.find(last_region) == 0)
	{
		// This must be a scripted object rezzed in the region:
		// extend the concept of "domain" to encompass the
		// scripted object server id and avoid blocking all other
		// objects at once in this region...

		// Get rid of any port number
		pos = url.find('/');		// We earlier made sure that there's one
		url = gAgent.getRegion()->getHost().getHostName() + url.substr(pos);

		pos = url.find('?');
		if (pos != std::string::npos)
		{
			// Get rid of any parameter
			url = url.substr(0, pos);
		}

		pos = url.rfind('/');
		if (pos != std::string::npos)
		{
			// Get rid of the filename, if any, keeping only the server + path
			url = url.substr(0, pos);
		}
	}
	else
	{
		pos = url.find(':');  
		if (pos != std::string::npos && pos < url.find('/'))
		{
			// Keep anything before the port number and strip the rest off
			url = url.substr(0, pos);
		}
		else
		{
			pos = url.find('/');	// We earlier made sure that there's one
			url = url.substr(0, pos);
		}
	}

		
	// Remember this region, so to cope with requests occuring just after a
	// TP out of it.
	last_region = gAgent.getRegion()->getHost().getHostName();

	return url;
}
