
/** 
 * @file llviewermessage.cpp
 * @brief Dumping ground for viewer-side message system callbacks.
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

#include "llviewermessage.h"

#include <deque>

#include "llimagejpeg.h"
#include "llaudioengine.h" 
#include "llavatarnamecache.h"
#include "indra_constants.h"
#include "lscript_byteformat.h"
#include "mean_collision_data.h"
#include "llfloaterbump.h"
#include "llassetstorage.h"
#include "llcachename.h"
#include "llchat.h"
#include "lldbstrings.h"
#include "lleconomy.h"
#include "llfilepicker.h"
#include "llfocusmgr.h"
#include "llfollowcamparams.h"
#include "llinstantmessage.h"
#include "llquantize.h"
#include "llregionflags.h"
#include "llregionhandle.h"
#include "llsdserialize.h"
#include "llstring.h"
#include "llteleportflags.h"
#include "lltracker.h"
#include "lltransactionflags.h"
#include "llxfermanager.h"
#include "message.h"
#include "sound_ids.h"
#include "lltimer.h"
#include "llmd5.h"

#include "llagent.h"
#include "llcallingcard.h"
#include "llconsole.h"
#include "llvieweraudio.h"
#include "llviewercontrol.h"
#include "lldrawpool.h"
#include "llfirstuse.h"
#include "llfloateractivespeakers.h"
#include "llfloateranimpreview.h"
#include "llfloaterbuycurrency.h"
#include "llfloaterbuyland.h"
#include "llfloaterchat.h"
#include "llfloatergroupinfo.h"
#include "llfloaterimagepreview.h"
#include "llfloaterland.h"
#include "llfloaterregioninfo.h"
#include "llfloaterlandholdings.h"
#include "llurldispatcher.h"
#include "llfloatermute.h"
#include "llfloaterpostcard.h"
#include "llfloaterpreference.h"
#include "llfloaterteleporthistory.h"
#include "llfollowcam.h"
#include "llgroupnotify.h"
#include "llhudeffect.h"
#include "llhudeffecttrail.h"
#include "llhudmanager.h"
#include "llimpanel.h"
#include "llinventorymodel.h"
#include "llinventoryview.h"
#include "llmenugl.h"
#include "llmutelist.h"
#include "llnotifications.h"
#include "llnotify.h"
#include "llpanelgrouplandmoney.h"
#include "llselectmgr.h"
#include "llstartup.h"
#include "llsky.h"
#include "llstatenums.h"
#include "llstatusbar.h"
#include "llimview.h"
#include "lltexturestats.h"
#include "lltool.h"
#include "lltoolbar.h"
#include "lltoolmgr.h"
#include "lltrans.h"
#include "llui.h"			// for make_ui_sound
#include "lluploaddialog.h"
#include "llviewercamera.h"
#include "llviewerdisplay.h"
#include "llviewergenericmessage.h"
#include "llviewerinventory.h"
#include "llviewerjoystick.h"
#include "llviewermenu.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerpartsource.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvlmanager.h"
#include "llvoavatar.h"
#include "llvotextbubble.h"
#include "llweb.h"
#include "llworld.h"
#include "pipeline.h"
#include "llappviewer.h"
#include "llfloaterworldmap.h"
#include "llviewerdisplay.h"
#include "llkeythrottle.h"
#include "lltranslate.h"
// <edit>
#include "llviewernetwork.h"
// </edit>

// [RLVa:KB]
#include "rlvhandler.h"
#include "rlvinventory.h"
// [/RLVa:KB]

#if SHY_MOD //Group Title script access
# include "shcommandhandler.h"
#endif //shy_mod

#include "hippogridmanager.h"
#include "hippolimits.h"
#include "hipporestrequest.h"
#include "hippofloaterxml.h"
#include "llversionviewer.h"

#include "llwlparammanager.h"
#include "llwaterparammanager.h"

#include <boost/tokenizer.hpp>

#if LL_WINDOWS // For Windows specific error handler
#include "llwindebug.h"	// For the invalid message handler
#endif

// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e)
#include "llfloateravatarinfo.h"
extern LLMap< const LLUUID, LLFloaterAvatarInfo* > gAvatarInfoInstances; // Only defined in llfloateravatarinfo.cpp
// [/RLVa:KB]

//
// Constants
//
const F32 BIRD_AUDIBLE_RADIUS = 32.0f;
const F32 SIT_DISTANCE_FROM_TARGET = 0.25f;
static const F32 LOGOUT_REPLY_TIME = 3.f;	// Wait this long after LogoutReply before quitting.

// Determine how quickly residents' scripts can issue question dialogs
// Allow bursts of up to 5 dialogs in 10 seconds. 10*2=20 seconds recovery if throttle kicks in
static const U32 LLREQUEST_PERMISSION_THROTTLE_LIMIT	= 5;     // requests
static const F32 LLREQUEST_PERMISSION_THROTTLE_INTERVAL	= 10.0f; // seconds

extern BOOL gDebugClicks;

// function prototypes
void open_offer(const std::vector<LLUUID>& items, const std::string& from_name);
bool highlight_offered_object(const LLUUID& obj_id);
bool check_offer_throttle(const std::string& from_name, bool check_only);
void callbackCacheEstateOwnerName(const LLUUID& id,
								  const std::string& first, const std::string& last,
								  BOOL is_group, void*);

//inventory offer throttle globals
LLFrameTimer gThrottleTimer;
const U32 OFFER_THROTTLE_MAX_COUNT=5; //number of items per time period
const F32 OFFER_THROTTLE_TIME=10.f; //time period in seconds

//script permissions
const std::string SCRIPT_QUESTIONS[SCRIPT_PERMISSION_EOF] = 
	{ 
		"ScriptTakeMoney",
		"ActOnControlInputs",
		"RemapControlInputs",
		"AnimateYourAvatar",
		"AttachToYourAvatar",
		"ReleaseOwnership",
		"LinkAndDelink",
		"AddAndRemoveJoints",
		"ChangePermissions",
		"TrackYourCamera",
		"ControlYourCamera"
	};

const BOOL SCRIPT_QUESTION_IS_CAUTION[SCRIPT_PERMISSION_EOF] = 
{
	TRUE,	// ScriptTakeMoney,
	FALSE,	// ActOnControlInputs
	FALSE,	// RemapControlInputs
	FALSE,	// AnimateYourAvatar
	FALSE,	// AttachToYourAvatar
	FALSE,	// ReleaseOwnership,
	FALSE,	// LinkAndDelink,
	FALSE,	// AddAndRemoveJoints
	FALSE,	// ChangePermissions
	FALSE,	// TrackYourCamera,
	FALSE	// ControlYourCamera
};

bool friendship_offer_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLUUID fid;
	LLMessageSystem* msg = gMessageSystem;
	const LLSD& payload = notification["payload"];
	switch(option)
	{
	case 0:
		// accept
		LLAvatarTracker::formFriendship(payload["from_id"]);

		fid = gInventory.findCategoryUUIDForType(LLAssetType::AT_CALLINGCARD);

		// This will also trigger an onlinenotification if the user is online
		msg->newMessageFast(_PREHASH_AcceptFriendship);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_TransactionBlock);
		msg->addUUIDFast(_PREHASH_TransactionID, payload["session_id"]);
		msg->nextBlockFast(_PREHASH_FolderData);
		msg->addUUIDFast(_PREHASH_FolderID, fid);
		msg->sendReliable(LLHost(payload["sender"].asString()));
		break;
	case 1:
		// decline
		// We no longer notify other viewers, but we DO still send
		// the rejection to the simulator to delete the pending userop.
		msg->newMessageFast(_PREHASH_DeclineFriendship);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_TransactionBlock);
		msg->addUUIDFast(_PREHASH_TransactionID, payload["session_id"]);
		msg->sendReliable(LLHost(payload["sender"].asString()));
		break;
	default:
		// close button probably, possibly timed out
		break;
	}

	return false;
}
static LLNotificationFunctorRegistration friendship_offer_callback_reg("OfferFriendship", friendship_offer_callback);
static LLNotificationFunctorRegistration friendship_offer_callback_reg_nm("OfferFriendshipNoMessage", friendship_offer_callback);

//const char BUSY_AUTO_RESPONSE[] =	"The Resident you messaged is in 'busy mode' which means they have "
//									"requested not to be disturbed. Your message will still be shown in their IM "
//									"panel for later viewing.";

//
// Functions
//

void give_money(const LLUUID& uuid, LLViewerRegion* region, S32 amount, BOOL is_group,
				S32 trx_type, const std::string& desc)
{
	// <edit>
	//if(0 == amount || !region) return;
	if(!region) return;
	// </edit>
	amount = abs(amount);
	LL_INFOS("Messaging") << "give_money(" << uuid << "," << amount << ")"<< LL_ENDL;
	if(can_afford_transaction(amount))
	{
//		gStatusBar->debitBalance(amount);
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_MoneyTransferRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
        msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_MoneyData);
		msg->addUUIDFast(_PREHASH_SourceID, gAgent.getID() );
		msg->addUUIDFast(_PREHASH_DestID, uuid);
		msg->addU8Fast(_PREHASH_Flags, pack_transaction_flags(FALSE, is_group));
		msg->addS32Fast(_PREHASH_Amount, amount);
		msg->addU8Fast(_PREHASH_AggregatePermNextOwner, (U8)LLAggregatePermissions::AP_EMPTY);
		msg->addU8Fast(_PREHASH_AggregatePermInventory, (U8)LLAggregatePermissions::AP_EMPTY);
		msg->addS32Fast(_PREHASH_TransactionType, trx_type );
		msg->addStringFast(_PREHASH_Description, desc);
		msg->sendReliable(region->getHost());
	}
	else
	{
		LLFloaterBuyCurrency::buyCurrency("Giving", amount);
	}
}

void send_complete_agent_movement(const LLHost& sim_host)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_CompleteAgentMovement);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addU32Fast(_PREHASH_CircuitCode, msg->mOurCircuitCode);
	msg->sendReliable(sim_host);
}

void process_logout_reply(LLMessageSystem* msg, void**)
{
	// The server has told us it's ok to quit.
	LL_DEBUGS("Messaging") << "process_logout_reply" << LL_ENDL;

	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	LLUUID session_id;
	msg->getUUID("AgentData", "SessionID", session_id);
	if((agent_id != gAgent.getID()) || (session_id != gAgent.getSessionID()))
	{
		LL_WARNS("Messaging") << "Bogus Logout Reply" << LL_ENDL;
	}

	LLInventoryModel::update_map_t parents;
	S32 count = msg->getNumberOfBlocksFast( _PREHASH_InventoryData );
	for(S32 i = 0; i < count; ++i)
	{
		LLUUID item_id;
		msg->getUUIDFast(_PREHASH_InventoryData, _PREHASH_ItemID, item_id, i);

		if( (1 == count) && item_id.isNull() )
		{
			// Detect dummy item.  Indicates an empty list.
			break;
		}

		// We do not need to track the asset ids, just account for an
		// updated inventory version.
		LL_INFOS("Messaging") << "process_logout_reply itemID=" << item_id << LL_ENDL;
		LLInventoryItem* item = gInventory.getItem( item_id );
		if( item )
		{
			parents[item->getParentUUID()] = 0;
			gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
		}
		else
		{
			LL_INFOS("Messaging") << "process_logout_reply item not found: " << item_id << LL_ENDL;
		}
	}
    LLAppViewer::instance()->forceQuit();
}

void process_layer_data(LLMessageSystem *mesgsys, void **user_data)
{
	LLViewerRegion *regionp = LLWorld::getInstance()->getRegion(mesgsys->getSender());

	if (!regionp || gNoRender)
	{
		return;
	}


	S32 size;
	S8 type;

	mesgsys->getS8Fast(_PREHASH_LayerID, _PREHASH_Type, type);
	size = mesgsys->getSizeFast(_PREHASH_LayerData, _PREHASH_Data);
	if (0 == size)
	{
		LL_WARNS("Messaging") << "Layer data has zero size." << LL_ENDL;
		return;
	}
	if (size < 0)
	{
		// getSizeFast() is probably trying to tell us about an error
		LL_WARNS("Messaging") << "getSizeFast() returned negative result: "
			<< size
			<< LL_ENDL;
		return;
	}
	U8 *datap = new U8[size];
	mesgsys->getBinaryDataFast(_PREHASH_LayerData, _PREHASH_Data, datap, size);
	LLVLData *vl_datap = new LLVLData(regionp, type, datap, size);
	if (mesgsys->getReceiveCompressedSize())
	{
		gVLManager.addLayerData(vl_datap, mesgsys->getReceiveCompressedSize());
	}
	else
	{
		gVLManager.addLayerData(vl_datap, mesgsys->getReceiveSize());
	}
}

// S32 exported_object_count = 0;
// S32 exported_image_count = 0;
// S32 current_object_count = 0;
// S32 current_image_count = 0;

// extern LLNotifyBox *gExporterNotify;
// extern LLUUID gExporterRequestID;
// extern std::string gExportDirectory;

// extern LLUploadDialog *gExportDialog;

// std::string gExportedFile;

// std::map<LLUUID, std::string> gImageChecksums;

// void export_complete()
// {
// 		LLUploadDialog::modalUploadFinished();
// 		gExporterRequestID.setNull();
// 		gExportDirectory = "";

// 		LLFILE* fXML = LLFile::fopen(gExportedFile, "rb");		/* Flawfinder: ignore */
// 		fseek(fXML, 0, SEEK_END);
// 		long length = ftell(fXML);
// 		fseek(fXML, 0, SEEK_SET);
// 		U8 *buffer = new U8[length + 1];
// 		size_t nread = fread(buffer, 1, length, fXML);
// 		if (nread < (size_t) length)
// 		{
// 			LL_WARNS("Messaging") << "Short read" << LL_ENDL;
// 		}
// 		buffer[nread] = '\0';
// 		fclose(fXML);

// 		char *pos = (char *)buffer;
// 		while ((pos = strstr(pos+1, "<sl:image ")) != 0)
// 		{
// 			char *pos_check = strstr(pos, "checksum=\"");

// 			if (pos_check)
// 			{
// 				char *pos_uuid = strstr(pos_check, "\">");

// 				if (pos_uuid)
// 				{
// 					char image_uuid_str[UUID_STR_SIZE];		/* Flawfinder: ignore */
// 					memcpy(image_uuid_str, pos_uuid+2, UUID_STR_SIZE-1);		/* Flawfinder: ignore */
// 					image_uuid_str[UUID_STR_SIZE-1] = 0;
					
// 					LLUUID image_uuid(image_uuid_str);

// 					LL_INFOS("Messaging") << "Found UUID: " << image_uuid << LL_ENDL;

// 					std::map<LLUUID, std::string>::iterator itor = gImageChecksums.find(image_uuid);
// 					if (itor != gImageChecksums.end())
// 					{
// 						LL_INFOS("Messaging") << "Replacing with checksum: " << itor->second << LL_ENDL;
// 						if (!itor->second.empty())
// 						{
// 							memcpy(&pos_check[10], itor->second.c_str(), 32);		/* Flawfinder: ignore */
// 						}
// 					}
// 				}
// 			}
// 		}

// 		LLFILE* fXMLOut = LLFile::fopen(gExportedFile, "wb");		/* Flawfinder: ignore */
// 		if (fwrite(buffer, 1, length, fXMLOut) != length)
// 		{
// 			LL_WARNS("Messaging") << "Short write" << LL_ENDL;
// 		}
// 		fclose(fXMLOut);

// 		delete [] buffer;
// }


// void exported_item_complete(const LLTSCode status, void *user_data)
// {
// 	//std::string *filename = (std::string *)user_data;

// 	if (status < LLTS_OK)
// 	{
// 		LL_WARNS("Messaging") << "Export failed!" << LL_ENDL;
// 	}
// 	else
// 	{
// 		++current_object_count;
// 		if (current_image_count == exported_image_count && current_object_count == exported_object_count)
// 		{
// 			LL_INFOS("Messaging") << "*** Export complete ***" << LL_ENDL;

// 			export_complete();
// 		}
// 		else
// 		{
// 			gExportDialog->setMessage(llformat("Exported %d/%d object files, %d/%d textures.", current_object_count, exported_object_count, current_image_count, exported_image_count));
// 		}
// 	}
// }

// struct exported_image_info
// {
// 	LLUUID image_id;
// 	std::string filename;
// 	U32 image_num;
// };

// void exported_j2c_complete(const LLTSCode status, void *user_data)
// {
// 	exported_image_info *info = (exported_image_info *)user_data;
// 	LLUUID image_id = info->image_id;
// 	U32 image_num = info->image_num;
// 	std::string filename = info->filename;
// 	delete info;

// 	if (status < LLTS_OK)
// 	{
// 		LL_WARNS("Messaging") << "Image download failed!" << LL_ENDL;
// 	}
// 	else
// 	{
// 		LLFILE* fIn = LLFile::fopen(filename, "rb");		/* Flawfinder: ignore */
// 		if (fIn) 
// 		{
// 			LLPointer<LLImageJ2C> ImageUtility = new LLImageJ2C;
// 			LLPointer<LLImageTGA> TargaUtility = new LLImageTGA;

// 			fseek(fIn, 0, SEEK_END);
// 			S32 length = ftell(fIn);
// 			fseek(fIn, 0, SEEK_SET);
// 			U8 *buffer = ImageUtility->allocateData(length);
// 			if (fread(buffer, 1, length, fIn) != length)
// 			{
// 				LL_WARNS("Messaging") << "Short read" << LL_ENDL;
// 			}
// 			fclose(fIn);
// 			LLFile::remove(filename);

// 			// Convert to TGA
// 			LLPointer<LLImageRaw> image = new LLImageRaw();

// 			ImageUtility->updateData();
// 			ImageUtility->decode(image, 100000.0f);
			
// 			TargaUtility->encode(image);
// 			U8 *data = TargaUtility->getData();
// 			S32 data_size = TargaUtility->getDataSize();

// 			std::string file_path = gDirUtilp->getDirName(filename);
			
// 			std::string output_file = llformat("%s/image-%03d.tga", file_path.c_str(), image_num);//filename;
// 			//S32 name_len = output_file.length();
// 			//strcpy(&output_file[name_len-3], "tga");
// 			LLFILE* fOut = LLFile::fopen(output_file, "wb");		/* Flawfinder: ignore */
// 			char md5_hash_string[33];		/* Flawfinder: ignore */
// 			strcpy(md5_hash_string, "00000000000000000000000000000000");		/* Flawfinder: ignore */
// 			if (fOut)
// 			{
// 				if (fwrite(data, 1, data_size, fOut) != data_size)
// 				{
// 					LL_WARNS("Messaging") << "Short write" << LL_ENDL;
// 				}
// 				fseek(fOut, 0, SEEK_SET);
// 				fclose(fOut);
// 				fOut = LLFile::fopen(output_file, "rb");		/* Flawfinder: ignore */
// 				LLMD5 my_md5_hash(fOut);
// 				my_md5_hash.hex_digest(md5_hash_string);
// 			}

// 			gImageChecksums.insert(std::pair<LLUUID, std::string>(image_id, md5_hash_string));
// 		}
// 	}

// 	++current_image_count;
// 	if (current_image_count == exported_image_count && current_object_count == exported_object_count)
// 	{
// 		LL_INFOS("Messaging") << "*** Export textures complete ***" << LL_ENDL;
// 			export_complete();
// 	}
// 	else
// 	{
// 		gExportDialog->setMessage(llformat("Exported %d/%d object files, %d/%d textures.", current_object_count, exported_object_count, current_image_count, exported_image_count));
// 	}
//}

void process_derez_ack(LLMessageSystem*, void**)
{
	if(gViewerWindow) gViewerWindow->getWindow()->decBusyCount();
}

void process_places_reply(LLMessageSystem* msg, void** data)
{
	LLUUID query_id;

	msg->getUUID("AgentData", "QueryID", query_id);
	if (query_id.isNull())
	{
		LLFloaterLandHoldings::processPlacesReply(msg, data);
	}
	else if(gAgent.isInGroup(query_id))
	{
		LLPanelGroupLandMoney::processPlacesReply(msg, data);
	}
	else
	{
		LL_WARNS("Messaging") << "Got invalid PlacesReply message" << LL_ENDL;
	}
}

void send_sound_trigger(const LLUUID& sound_id, F32 gain)
{
	if (sound_id.isNull() || gAgent.getRegion() == NULL)
	{
		// disconnected agent or zero guids don't get sent (no sound)
		return;
	}

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_SoundTrigger);
	msg->nextBlockFast(_PREHASH_SoundData);
	msg->addUUIDFast(_PREHASH_SoundID, sound_id);
	// Client untrusted, ids set on sim
	msg->addUUIDFast(_PREHASH_OwnerID, LLUUID::null );
	msg->addUUIDFast(_PREHASH_ObjectID, LLUUID::null );
	msg->addUUIDFast(_PREHASH_ParentID, LLUUID::null );

	msg->addU64Fast(_PREHASH_Handle, gAgent.getRegion()->getHandle());

	LLVector3 position = gAgent.getPositionAgent();
	msg->addVector3Fast(_PREHASH_Position, position);
	msg->addF32Fast(_PREHASH_Gain, gain);

	gAgent.sendMessage();
}

bool join_group_response(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	BOOL delete_context_data = TRUE;
	bool accept_invite = false;

	LLUUID group_id = notification["payload"]["group_id"].asUUID();
	LLUUID transaction_id = notification["payload"]["transaction_id"].asUUID();
	std::string name = notification["payload"]["name"].asString();
	std::string message = notification["payload"]["message"].asString();
	S32 fee = notification["payload"]["fee"].asInteger();

	if (option == 2 && !group_id.isNull())
	{
		LLFloaterGroupInfo::showFromUUID(group_id);
		LLSD args;
		args["MESSAGE"] = message;
		LLNotifications::instance().add("JoinGroup", args, notification["payload"]);
		return false;
	}
	if(option == 0 && !group_id.isNull())
	{
		// check for promotion or demotion.
		S32 max_groups = gHippoLimits->getMaxAgentGroups();
		if(gAgent.isInGroup(group_id)) ++max_groups;

		if(gAgent.mGroups.count() < max_groups)
		{
			accept_invite = true;
		}
		else
		{
			delete_context_data = FALSE;
			LLSD args;
			args["NAME"] = name;
			args["INVITE"] = message;
			LLNotifications::instance().add("JoinedTooManyGroupsMember", args, notification["payload"]);
		}
	}

	if (accept_invite)
	{
		// If there is a fee to join this group, make
		// sure the user is sure they want to join.
		if (fee > 0)
		{
			delete_context_data = FALSE;
			LLSD args;
			args["COST"] = llformat("%d", fee);
			args["CURRENCY"] = gHippoGridManager->getConnectedGrid()->getCurrencySymbol();
			// Set the fee for next time to 0, so that we don't keep
			// asking about a fee.
			LLSD next_payload = notification["payload"];
			next_payload["fee"] = 0;
			LLNotifications::instance().add("JoinGroupCanAfford",
									args,
									next_payload);
		}
		else
		{
			send_improved_im(group_id,
							 std::string("name"),
							 std::string("message"),
							IM_ONLINE,
							IM_GROUP_INVITATION_ACCEPT,
							transaction_id);
		}
	}
	else
	{
		send_improved_im(group_id,
						 std::string("name"),
						 std::string("message"),
						IM_ONLINE,
						IM_GROUP_INVITATION_DECLINE,
						transaction_id);
	}

	return false;
}
static LLNotificationFunctorRegistration jgr_1("JoinGroup", join_group_response);
static LLNotificationFunctorRegistration jgr_2("JoinedTooManyGroupsMember", join_group_response);
static LLNotificationFunctorRegistration jgr_3("JoinGroupCanAfford", join_group_response);


//-----------------------------------------------------------------------------
// Instant Message
//-----------------------------------------------------------------------------
class LLOpenAgentOffer : public LLInventoryFetchObserver
{
public:
	LLOpenAgentOffer(const std::string& from_name) : mFromName(from_name) {}
	/*virtual*/ void done()
	{
		open_offer(mComplete, mFromName);
		gInventory.removeObserver(this);
		delete this;
	}
private:
	std::string mFromName;
};

//unlike the FetchObserver for AgentOffer, we only make one 
//instance of the AddedObserver for TaskOffers
//and it never dies.  We do this because we don't know the UUID of 
//task offers until they are accepted, so we don't wouldn't 
//know what to watch for, so instead we just watch for all additions.
class LLOpenTaskOffer : public LLInventoryAddedObserver
{
protected:
	/*virtual*/ void done()
	{
		open_offer(mAdded, "");
		mAdded.clear();
	}
 };

//one global instance to bind them
LLOpenTaskOffer* gNewInventoryObserver=NULL;

void start_new_inventory_observer()
{
	if (!gNewInventoryObserver) //task offer observer 
	{
		// Observer is deleted by gInventory
		gNewInventoryObserver = new LLOpenTaskOffer;
		gInventory.addObserver(gNewInventoryObserver);
	}
}

class LLDiscardAgentOffer : public LLInventoryFetchComboObserver
{
public:
	LLDiscardAgentOffer(const LLUUID& folder_id, const LLUUID& object_id) :
		mFolderID(folder_id),
		mObjectID(object_id) {}
	virtual ~LLDiscardAgentOffer() {}
	virtual void done()
	{
		LL_DEBUGS("Messaging") << "LLDiscardAgentOffer::done()" << LL_ENDL;
		LLUUID trash_id;
		trash_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_TRASH);
		bool notify = false;
		if(trash_id.notNull() && mObjectID.notNull())
		{
			LLInventoryModel::update_list_t update;
			LLInventoryModel::LLCategoryUpdate old_folder(mFolderID, -1);
			update.push_back(old_folder);
			LLInventoryModel::LLCategoryUpdate new_folder(trash_id, 1);
			update.push_back(new_folder);
			gInventory.accountForUpdate(update);
			gInventory.moveObject(mObjectID, trash_id);
			LLInventoryObject* obj = gInventory.getObject(mObjectID);
			if(obj)
			{
				// no need to restamp since this is already a freshly
				// stamped item.
				obj->updateParentOnServer(FALSE);
				notify = true;
			}
		}
		else
		{
			LL_WARNS("Messaging") << "DiscardAgentOffer unable to find: "
					<< (trash_id.isNull() ? "trash " : "")
					<< (mObjectID.isNull() ? "object" : "") << LL_ENDL;
		}
		gInventory.removeObserver(this);
		if(notify)
		{
			gInventory.notifyObservers();
		}
		delete this;
	}
protected:
	LLUUID mFolderID;
	LLUUID mObjectID;
};


//Returns TRUE if we are OK, FALSE if we are throttled
//Set check_only true if you want to know the throttle status 
//without registering a hit
bool check_offer_throttle(const std::string& from_name, bool check_only)
{
	static U32 throttle_count;
	static bool throttle_logged;
	LLChat chat;
	std::string log_message;

	if (!gSavedSettings.getBOOL("ShowNewInventory"))
		return false;

	if (check_only)
	{
		return gThrottleTimer.hasExpired();
	}
	
	if(gThrottleTimer.checkExpirationAndReset(OFFER_THROTTLE_TIME))
	{
		LL_DEBUGS("Messaging") << "Throttle Expired" << LL_ENDL;
		throttle_count=1;
		throttle_logged=false;
		return true;
	}
	else //has not expired
	{
		LL_DEBUGS("Messaging") << "Throttle Not Expired, Count: " << throttle_count << LL_ENDL;
		// When downloading the initial inventory we get a lot of new items
		// coming in and can't tell that from spam.
		if (LLStartUp::getStartupState() >= STATE_STARTED
			&& throttle_count >= OFFER_THROTTLE_MAX_COUNT)
		{
			if (!throttle_logged)
			{
				// Use the name of the last item giver, who is probably the person
				// spamming you.
				std::ostringstream message;
				message << LLAppViewer::instance()->getSecondLifeTitle();
				if (!from_name.empty())
				{
					message << ": Items coming in too fast from " << from_name;
				}
				else
				{
					message << ": Items coming in too fast";
				}
				message << ", automatic preview disabled for "
					<< OFFER_THROTTLE_TIME << " seconds.";
				chat.mText = message.str();
				//this is kinda important, so actually put it on screen
				LLFloaterChat::addChat(chat, FALSE, FALSE);
				throttle_logged=true;
			}
			return false;
		}
		else
		{
			throttle_count++;
			return true;
		}
	}
}
 
void open_offer(const std::vector<LLUUID>& items, const std::string& from_name)
{
	std::vector<LLUUID>::const_iterator it = items.begin();
	std::vector<LLUUID>::const_iterator end = items.end();
	LLInventoryItem* item;
	for(; it != end; ++it)
	{
		item = gInventory.getItem(*it);
		if(!item)
		{
			LL_WARNS("Messaging") << "Unable to show inventory item: " << *it << LL_ENDL;
			continue;
		}
		if(!highlight_offered_object(item->getUUID()))
		{
			continue;
		}
		LLAssetType::EType asset_type = item->getType();

		//if we are throttled, don't display them
		if (check_offer_throttle(from_name, false))
		{
			// I'm not sure this is a good idea - Definitely a bad idea. HB
			//bool show_keep_discard = item->getPermissions().getCreator() != gAgent.getID();
			bool show_keep_discard = true;
			switch(asset_type)
			{
			case LLAssetType::AT_NOTECARD:
				open_notecard((LLViewerInventoryItem*)item, std::string("Note: ") + item->getName(), LLUUID::null, show_keep_discard, LLUUID::null, FALSE);
				break;
			case LLAssetType::AT_LANDMARK:
				open_landmark((LLViewerInventoryItem*)item, std::string("Landmark: ") + item->getName(), show_keep_discard, LLUUID::null, FALSE);
				break;
			case LLAssetType::AT_TEXTURE:
				open_texture(*it, std::string("Texture: ") + item->getName(), show_keep_discard, LLUUID::null, FALSE);
				break;
			default:
				break;
			}
		}
		//highlight item, if it's not in the trash or lost+found
		
		// Don't auto-open the inventory floater
		LLInventoryView* view = LLInventoryView::getActiveInventory();
		if(!view)
		{
			return;
		}

		if(gSavedSettings.getBOOL("ShowInInventory") &&
		   asset_type != LLAssetType::AT_CALLINGCARD &&
		   item->getInventoryType() != LLInventoryType::IT_ATTACHMENT &&
		   !from_name.empty())
		{
			LLInventoryView::showAgentInventory(TRUE);
		}
		//Trash Check
		LLUUID trash_id;
		trash_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_TRASH);
		if(gInventory.isObjectDescendentOf(item->getUUID(), trash_id))
		{
			return;
		}
		LLUUID lost_and_found_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_LOST_AND_FOUND);
		//BOOL inventory_has_focus = gFocusMgr.childHasKeyboardFocus(view);
		BOOL user_is_away = gAwayTimer.getStarted();

		// don't select lost and found items if the user is active
		if (gInventory.isObjectDescendentOf(item->getUUID(), lost_and_found_id)
			&& !user_is_away)
		{
			return;
		}

		//Not sure about this check.  Could make it easy to miss incoming items.
		//don't dick with highlight while the user is working
		//if(inventory_has_focus && !user_is_away)
		//	break;
		LL_DEBUGS("Messaging") << "Highlighting" << item->getUUID()  << LL_ENDL;
		//highlight item

		LLFocusableElement* focus_ctrl = gFocusMgr.getKeyboardFocus();
		view->getPanel()->setSelection(item->getUUID(), TAKE_FOCUS_NO);
		gFocusMgr.setKeyboardFocus(focus_ctrl);
	}
}

bool highlight_offered_object(const LLUUID& obj_id)
{
	const LLInventoryObject* obj = gInventory.getObject(obj_id);
	if(!obj)
	{
		LL_WARNS("Messaging") << "Unable to show inventory item: " << obj_id << LL_ENDL;
		return false;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Don't highlight if it's in certain "quiet" folders which don't need UI
	// notification (e.g. trash, cof, lost-and-found).
	if(!gAgent.getAFK())
	{
		const LLViewerInventoryCategory *parent = gInventory.getFirstNondefaultParent(obj_id);
		if (parent)
		{
			const LLAssetType::EType parent_type = parent->getPreferredType();
			switch (parent_type)
			{
				case LLAssetType::AT_TRASH:
				case LLAssetType::AT_LOST_AND_FOUND:
				case LLAssetType::AT_CURRENT_OUTFIT:
				case LLAssetType::AT_OUTFIT:
				case LLAssetType::AT_MY_OUTFITS:
					return false;
				default:
					break;
			}
		}
	}

	return true;
}

void inventory_offer_mute_callback(const LLUUID& blocked_id,
								   const std::string& first_name,
								   const std::string& last_name,
								   BOOL is_group,
								   void* user_data)
{
	std::string from_name;
	LLMute::EType type;

	if (is_group)
	{
		type = LLMute::GROUP;
		from_name = first_name;
	}
	else
	{
		type = LLMute::AGENT;
		from_name = first_name + " " + last_name;
	}

	LLMute mute(blocked_id, from_name, type);
	if (LLMuteList::getInstance()->add(mute))
	{
		LLFloaterMute::showInstance();
		LLFloaterMute::getInstance()->selectMute(blocked_id);
	}

	// purge the message queue of any previously queued inventory offers from the same source.
	class OfferMatcher : public LLNotifyBoxView::Matcher
	{
	public:
		OfferMatcher(const LLUUID& to_block) : blocked_id(to_block) {}
		BOOL matches(const LLNotificationPtr notification) const
		{
			if(notification->getName() == "ObjectGiveItem" 
				|| notification->getName() == "ObjectGiveItemUnknownUser"
				|| notification->getName() == "UserGiveItem")
			{
				return (notification->getPayload()["from_id"].asUUID() == blocked_id);
			}
			return FALSE;
		}
	private:
		const LLUUID& blocked_id;
	};
	gNotifyBoxView->purgeMessagesMatching(OfferMatcher(blocked_id));
}

LLOfferInfo::LLOfferInfo(const LLSD& sd)
{
	mIM = (EInstantMessage)sd["im_type"].asInteger();
	mFromID = sd["from_id"].asUUID();
	mFromGroup = sd["from_group"].asBoolean();
	mFromObject = sd["from_object"].asBoolean();
	mTransactionID = sd["transaction_id"].asUUID();
	mFolderID = sd["folder_id"].asUUID();
	mObjectID = sd["object_id"].asUUID();
	mType = LLAssetType::lookup(sd["type"].asString().c_str());
	mFromName = sd["from_name"].asString();
	mDesc = sd["description"].asString();
	mHost = LLHost(sd["sender"].asString());
}

LLSD LLOfferInfo::asLLSD()
{
	LLSD sd;
	sd["im_type"] = mIM;
	sd["from_id"] = mFromID;
	sd["from_group"] = mFromGroup;
	sd["from_object"] = mFromObject;
	sd["transaction_id"] = mTransactionID;
	sd["folder_id"] = mFolderID;
	sd["object_id"] = mObjectID;
	sd["type"] = LLAssetType::lookup(mType);
	sd["from_name"] = mFromName;
	sd["description"] = mDesc;
	sd["sender"] = mHost.getIPandPort();
	return sd;
}

bool LLOfferInfo::inventory_offer_callback(const LLSD& notification, const LLSD& response)
 {
	LLChat chat;
	std::string log_message;
	S32 button = LLNotification::getSelectedOption(notification, response);

	// For muting, we need to add the mute, then decline the offer.
	// This must be done here because:
	// * callback may be called immediately,
	// * adding the mute sends a message,
	// * we can't build two messages at once.
	if (2 == button)
	{
		gCacheName->get(mFromID, mFromGroup, inventory_offer_mute_callback, this);
	}

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MessageBlock);
	msg->addBOOLFast(_PREHASH_FromGroup, FALSE);
	msg->addUUIDFast(_PREHASH_ToAgentID, mFromID);
	msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
	msg->addUUIDFast(_PREHASH_ID, mTransactionID);
	msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP); // no timestamp necessary
	std::string name;
	gAgent.buildFullname(name);
	msg->addStringFast(_PREHASH_FromAgentName, name);
	msg->addStringFast(_PREHASH_Message, ""); 
	msg->addU32Fast(_PREHASH_ParentEstateID, 0);
	msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
	msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());
	LLInventoryObserver* opener = NULL;
	LLViewerInventoryCategory* catp = NULL;
	catp = (LLViewerInventoryCategory*)gInventory.getCategory(mObjectID);
	LLViewerInventoryItem* itemp = NULL;
	if(!catp)
	{
		itemp = (LLViewerInventoryItem*)gInventory.getItem(mObjectID);
	}

	// *TODO:translate
	std::string from_string; // Used in the pop-up.
	std::string chatHistory_string;  // Used in chat history.
	if (mFromObject == TRUE)
	{
		if (mFromGroup)
		{
			std::string group_name;
			if (gCacheName->getGroupName(mFromID, group_name))
			{
				from_string = std::string("An object named '") + mFromName + "' owned by the group '" + group_name + "'";
				chatHistory_string = mFromName + " owned by the group '" + group_name + "'";
			}
			else
			{
				from_string = std::string("An object named '") + mFromName + "' owned by an unknown group";
				chatHistory_string = mFromName + " owned by an unknown group";
			}
		}
		else
		{
			std::string first_name, last_name;
			if (gCacheName->getName(mFromID, first_name, last_name))
			{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e)
				std::string full_name = first_name + " " + last_name;
				if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(mFromID)) )
				{
					full_name = RlvStrings::getAnonym(full_name);
				}
				from_string = std::string("An object named '") + mFromName + "' owned by " + full_name;
				chatHistory_string = mFromName + " owned by " + full_name;
// [/RLVa:KB]
				//from_string = std::string("An object named '") + mFromName + "' owned by " + first_name + " " + last_name;
				//chatHistory_string = mFromName + " owned by " + first_name + " " + last_name;
			}
			else
			{
				from_string = std::string("An object named '") + mFromName + "' owned by an unknown user";
				chatHistory_string = mFromName + " owned by an unknown user";
			}
		}
	}
	else
	{
		from_string = chatHistory_string = mFromName;
	}
	
	bool busy=FALSE;
	
// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1e) | Added: RLVa-1.2.1e
	bool fRlvNotifyAccepted = false;
// [/RLVa:KB]
	switch(button)
	{
	case IOR_ACCEPT:
		// ACCEPT. The math for the dialog works, because the accept
		// for inventory_offered, task_inventory_offer or
		// group_notice_inventory is 1 greater than the offer integer value.

// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1e) | Modified: RLVa-1.2.1e
		// Only change the inventory offer's destination folder to the shared root if:
		//   - the user has enabled the feature
		//   - the inventory offer came from a script (and specifies a folder)
		//   - the name starts with the prefix - mDesc format: '[OBJECTNAME]'  ( http://slurl.com/... )
		if ( (rlv_handler_t::isEnabled()) && 
			 (IM_TASK_INVENTORY_OFFERED == mIM) && (LLAssetType::AT_CATEGORY == mType) && (mDesc.find(RLV_PUTINV_PREFIX) == 1) )
		{
			fRlvNotifyAccepted = true;
			if (!RlvSettings::getForbidGiveToRLV())
			{
				const LLViewerInventoryCategory* pRlvRoot = RlvInventory::instance().getSharedRoot();
				if (pRlvRoot)
				{
					fRlvNotifyAccepted = false;		// "accepted_in_rlv" is sent from RlvGiveToRLVTaskOffer *after* we have the folder
					mFolderID = pRlvRoot->getUUID();

					RlvGiveToRLVTaskOffer* pOfferObserver = new RlvGiveToRLVTaskOffer(mTransactionID);
					gInventory.addObserver(pOfferObserver);
				}
			}
		}
// [/RLVa:KB]

		// Generates IM_INVENTORY_ACCEPTED, IM_TASK_INVENTORY_ACCEPTED, 
		// or IM_GROUP_NOTICE_INVENTORY_ACCEPTED
		msg->addU8Fast(_PREHASH_Dialog, (U8)(mIM + 1));
		msg->addBinaryDataFast(_PREHASH_BinaryBucket, &(mFolderID.mData),
					 sizeof(mFolderID.mData));
		// send the message
		msg->sendReliable(mHost);

		//don't spam them if they are getting flooded
		if (check_offer_throttle(mFromName, true))
		{
			log_message = chatHistory_string + " gave you " + mDesc + ".";
 			chat.mText = log_message;
 			LLFloaterChat::addChatHistory(chat);
		}

		// we will want to open this item when it comes back.
		LL_DEBUGS("Messaging") << "Initializing an opener for tid: " << mTransactionID
				 << LL_ENDL;
		switch (mIM)
		{
		case IM_INVENTORY_OFFERED:
		{
			// This is an offer from an agent. In this case, the back
			// end has already copied the items into your inventory,
			// so we can fetch it out of our inventory.
// [RLVa:KB] - Checked: 2010-04-18 (RLVa-1.2.0e) | Modified: RLVa-1.2.0e
#ifdef RLV_EXTENSION_GIVETORLV_A2A
			if ( (rlv_handler_t::isEnabled()) && (!RlvSettings::getForbidGiveToRLV()) && (LLAssetType::AT_CATEGORY == mType) && 
				 (RlvInventory::instance().getSharedRoot()) && (mDesc.find(RLV_PUTINV_PREFIX) == 0) )
			{
				RlvGiveToRLVAgentOffer* pOffer = new RlvGiveToRLVAgentOffer();
				LLInventoryFetchComboObserver::folder_ref_t folders;
				folders.push_back(mObjectID);
				pOffer->fetchDescendents(folders);
				if (pOffer->isEverythingComplete())
					pOffer->done();
				else
					gInventory.addObserver(pOffer);
			}
#endif // RLV_EXTENSION_GIVETORLV_A2A
// [/RLVa:KB]

			LLInventoryFetchObserver::item_ref_t items;
			items.push_back(mObjectID);
			LLOpenAgentOffer* open_agent_offer = new LLOpenAgentOffer(from_string);
			open_agent_offer->fetchItems(items);
			if(catp || (itemp && itemp->isComplete()))
			{
				open_agent_offer->done();
			}
			else
			{
				opener = open_agent_offer;
			}
		}
			break;
		case IM_TASK_INVENTORY_OFFERED:
		case IM_GROUP_NOTICE:
		case IM_GROUP_NOTICE_REQUESTED:
		{
			// This is an offer from a task or group.
			// We don't use a new instance of an opener
			// We instead use the singular observer gOpenTaskOffer
			// Since it already exists, we don't need to actually do anything
		}
		break;
		default:
			LL_WARNS("Messaging") << "inventory_offer_callback: unknown offer type" << LL_ENDL;
			break;
		}	// end switch (mIM)
		break;

	case IOR_BUSY:
		//Busy falls through to decline.  Says to make busy message.
		busy=TRUE;
	case IOR_MUTE:
		// MUTE falls through to decline
	case IOR_DECLINE:
		// DECLINE. The math for the dialog works, because the decline
		// for inventory_offered, task_inventory_offer or
		// group_notice_inventory is 2 greater than the offer integer value.
		// Generates IM_INVENTORY_DECLINED, IM_TASK_INVENTORY_DECLINED,
		// or IM_GROUP_NOTICE_INVENTORY_DECLINED
	default:
		// close button probably (or any of the fall-throughs from above)
		msg->addU8Fast(_PREHASH_Dialog, (U8)(mIM + 2));
		msg->addBinaryDataFast(_PREHASH_BinaryBucket, EMPTY_BINARY_BUCKET, EMPTY_BINARY_BUCKET_SIZE);
		// send the message
		msg->sendReliable(mHost);

// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1e) | Added: RLVa-1.2.1e
		if ( (rlv_handler_t::isEnabled()) && 
			 (IM_TASK_INVENTORY_OFFERED == mIM) && (LLAssetType::AT_CATEGORY == mType) && (mDesc.find(RLV_PUTINV_PREFIX) == 1) )
		{
			std::string::size_type idxToken = mDesc.find("'  ( http://");
			if (std::string::npos != idxToken)
				RlvBehaviourNotifyHandler::instance().sendNotification("declined inv_offer " + mDesc.substr(1, idxToken - 1));
		}
// [/RLVa:KB]

		log_message = "You decline " + mDesc + " from " + mFromName + ".";
		chat.mText = log_message;
		if( LLMuteList::getInstance()->isMuted(mFromID ) && ! LLMuteList::getInstance()->isLinden(mFromName) )  // muting for SL-42269
		{
			chat.mMuted = TRUE;
		}
		LLFloaterChat::addChatHistory(chat);

		// If it's from an agent, we have to fetch the item to throw
		// it away. If it's from a task or group, just denying the 
		// request will suffice to discard the item.
		if(IM_INVENTORY_OFFERED == mIM)
		{
			LLInventoryFetchComboObserver::folder_ref_t folders;
			LLInventoryFetchComboObserver::item_ref_t items;
			items.push_back(mObjectID);
			LLDiscardAgentOffer* discard_agent_offer;
			discard_agent_offer = new LLDiscardAgentOffer(mFolderID, mObjectID);
			discard_agent_offer->fetch(folders, items);
			if(catp || (itemp && itemp->isComplete()))
			{
				discard_agent_offer->done();
			}
			else
			{
				opener = discard_agent_offer;
			}
			
		}
		if (busy &&	(!mFromGroup && !mFromObject))
		{
			busy_message(msg,mFromID);
		}
		break;
	}

	if(opener)
	{
		gInventory.addObserver(opener);
	}

	// Allow these to stack up, but once you deal with one, reset the
	// position.
	gFloaterView->resetStartingFloaterPosition();

	delete this;
	return false;
}


void inventory_offer_handler(LLOfferInfo* info, BOOL from_task)
{
	//Until throttling is implmented, busy mode should reject inventory instead of silently
	//accepting it.  SEE SL-39554
	if (gAgent.getBusy())
	{
		info->forceResponse(IOR_BUSY);
		return;
	}
	
	//If muted, don't even go through the messaging stuff.  Just curtail the offer here.
	if (LLMuteList::getInstance()->isMuted(info->mFromID, info->mFromName))
	{
		info->forceResponse(IOR_MUTE);
		return;
	}

	// Avoid the Accept/Discard dialog if the user so desires. JC
	if (gSavedSettings.getBOOL("AutoAcceptNewInventory")
		&& (info->mType == LLAssetType::AT_NOTECARD
			|| info->mType == LLAssetType::AT_LANDMARK
			|| info->mType == LLAssetType::AT_TEXTURE))
	{
		// For certain types, just accept the items into the inventory,
		// and possibly open them on receipt depending upon "ShowNewInventory".
		info->forceResponse(IOR_ACCEPT);
		return;
	}

	// Strip any SLURL from the message display. (DEV-2754)
	std::string msg = info->mDesc;
	int indx = msg.find(" ( http://slurl.com/secondlife/");
	if(indx == std::string::npos)
	{
		// try to find new slurl host
		indx = msg.find(" ( http://maps.secondlife.com/secondlife/");
	}
	if(indx >= 0)
	{
		LLStringUtil::truncate(msg, indx);
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-04 (RLVa-1.0.0a) | Added: RLVa-1.0.0a
		// TODO-RLVa: needs revisiting when LL saves open notifications to disk to accept them on the next relog
		if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
		{
			RlvUtil::filterLocation(info->mDesc);
		}
// [/RLVa:KB]
	}
	
	LLSD args;
	args["[OBJECTNAME]"] = msg;

	LLSD payload;

	// must protect against a NULL return from lookupHumanReadable()
	std::string typestr = ll_safe_string(LLAssetType::lookupHumanReadable(info->mType));
	if (!typestr.empty())
	{
		args["OBJECTTYPE"] = typestr;
	}
	else
	{
		LL_WARNS("Messaging") << "LLAssetType::lookupHumanReadable() returned NULL - probably bad asset type: " << info->mType << LL_ENDL;
		args["OBJECTTYPE"] = "";

		// This seems safest, rather than propagating bogosity
		LL_WARNS("Messaging") << "Forcing an inventory-decline for probably-bad asset type." << LL_ENDL;
		info->forceResponse(IOR_DECLINE);
		return;
	}

	// Name cache callbacks don't store userdata, so can't save
	// off the LLOfferInfo.  Argh.
	BOOL name_found = FALSE;
	if (info->mFromGroup)
	{
		std::string group_name;
		if (gCacheName->getGroupName(info->mFromID, group_name))
		{
			args["FIRST"] = group_name;
			args["LAST"] = "";
			name_found = TRUE;
		}
	}
	else
	{
		std::string first_name, last_name;
		if (gCacheName->getName(info->mFromID, first_name, last_name))
		{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e)
			if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(info->mFromID)) )
			{
				first_name = RlvStrings::getAnonym(first_name.append(" ").append(last_name));
				last_name.clear();
			}
// [/RLVa:KB]
			args["FIRST"] = first_name;
			args["LAST"] = last_name;
			name_found = TRUE;
		}
	}

	payload["from_id"] = info->mFromID;
	args["OBJECTFROMNAME"] = info->mFromName;
	args["NAME"] = info->mFromName;

	LLNotification::Params p("ObjectGiveItem");
	p.substitutions(args).payload(payload).functor(boost::bind(&LLOfferInfo::inventory_offer_callback, info, _1, _2));

	if (from_task)
	{
		p.name = name_found ? "ObjectGiveItem" : "ObjectGiveItemUnknownUser";
	}
	else
	{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e)
		if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(info->mFromID)) )
		{
			args["NAME"] = RlvStrings::getAnonym(info->mFromName);
		}
// [/RLVa:KB]
		p.name = "UserGiveItem";
	}

	LLNotifications::instance().add(p);
}


bool group_vote_callback(const LLSD& notification, const LLSD& response)
{
	LLUUID group_id = notification["payload"]["group_id"].asUUID();
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch(option)
	{
	case 0:
		// Vote Now
		// Open up the voting tab
		LLFloaterGroupInfo::showFromUUID(group_id, "voting_tab");
		break;
	default:
		// Vote Later or
		// close button
		break;
	}
	return false;
}
static LLNotificationFunctorRegistration group_vote_callback_reg("GroupVote", group_vote_callback);

bool lure_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = 0;
	if (response.isInteger()) 
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotification::getSelectedOption(notification, response);
	}
	
	LLUUID from_id = notification["payload"]["from_id"].asUUID();
	LLUUID lure_id = notification["payload"]["lure_id"].asUUID();
	BOOL godlike = notification["payload"]["godlike"].asBoolean();

	switch(option)
	{
	case 0:
		{
			// accept
			gAgent.teleportViaLure(lure_id, godlike);
		}
		break;
	case 1:
	default:
		// decline
		send_simple_im(from_id,
					   LLStringUtil::null,
					   IM_LURE_DECLINED,
					   lure_id);
		break;
	}
	return false;
}
static LLNotificationFunctorRegistration lure_callback_reg("TeleportOffered", lure_callback);

bool goto_url_callback(const LLSD& notification, const LLSD& response)
{
	std::string url = notification["payload"]["url"].asString();
	S32 option = LLNotification::getSelectedOption(notification, response);
	if(1 == option)
	{
		LLWeb::loadURL(url);
	}
	return false;
}
static LLNotificationFunctorRegistration goto_url_callback_reg("GotoURL", goto_url_callback);


void process_improved_im(LLMessageSystem *msg, void **user_data)
{
	if (gNoRender)
	{
		return;
	}
	LLUUID from_id;
	BOOL from_group;
	LLUUID to_id;
	U8 offline;
	U8 d = 0;
	LLUUID session_id;
	U32 timestamp;
	std::string name;
	std::string message;
	U32 parent_estate_id = 0;
	LLUUID region_id;
	LLVector3 position;
	U8 binary_bucket[MTUBYTES];
	S32 binary_bucket_size;
	LLChat chat;
	std::string buffer;
	
	// *TODO:translate - need to fix the full name to first/last (maybe)
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, from_id);
	msg->getBOOLFast(_PREHASH_MessageBlock, _PREHASH_FromGroup, from_group);
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_ToAgentID, to_id);
	msg->getU8Fast(  _PREHASH_MessageBlock, _PREHASH_Offline, offline);
	msg->getU8Fast(  _PREHASH_MessageBlock, _PREHASH_Dialog, d);
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_ID, session_id);
	msg->getU32Fast( _PREHASH_MessageBlock, _PREHASH_Timestamp, timestamp);
	//msg->getData("MessageBlock", "Count",		&count);
	msg->getStringFast(_PREHASH_MessageBlock, _PREHASH_FromAgentName, name);
	msg->getStringFast(_PREHASH_MessageBlock, _PREHASH_Message,		message);
	msg->getU32Fast(_PREHASH_MessageBlock, _PREHASH_ParentEstateID, parent_estate_id);
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_RegionID, region_id);
	msg->getVector3Fast(_PREHASH_MessageBlock, _PREHASH_Position, position);
	msg->getBinaryDataFast(  _PREHASH_MessageBlock, _PREHASH_BinaryBucket, binary_bucket, 0, 0, MTUBYTES);
	binary_bucket_size = msg->getSizeFast(_PREHASH_MessageBlock, _PREHASH_BinaryBucket);
	EInstantMessage dialog = (EInstantMessage)d;

	// <edit>
	llinfos << "RegionID: " << region_id.asString() << llendl;
	// </edit>

	BOOL is_busy = gAgent.getBusy();
	BOOL is_muted = LLMuteList::getInstance()->isMuted(from_id, name, LLMute::flagTextChat);
	BOOL is_linden = LLMuteList::getInstance()->isLinden(name);
	BOOL is_owned_by_me = FALSE;
	
	LLUUID computed_session_id = LLIMMgr::computeSessionID(dialog,from_id);
	
	chat.mMuted = is_muted && !is_linden;
	chat.mFromID = from_id;
	chat.mFromName = name;
	chat.mSourceType = (from_id.isNull() || (name == std::string(SYSTEM_FROM))) ? CHAT_SOURCE_SYSTEM : CHAT_SOURCE_AGENT;

	LLViewerObject *source = gObjectList.findObject(session_id); //Session ID is probably the wrong thing.
	if (source)
	{
		is_owned_by_me = source->permYouOwner();
	}

	std::string separator_string(": ");
	int message_offset = 0;

		//Handle IRC styled /me messages.
	std::string prefix = message.substr(0, 4);
	if (prefix == "/me " || prefix == "/me'")
	{
		separator_string = "";
		message_offset = 3;
	}

	if(  dialog == IM_TYPING_START
	  || dialog == IM_NOTHING_SPECIAL
	  || dialog == IM_TYPING_STOP
	  || dialog == IM_BUSY_AUTO_RESPONSE)
	{
		if(session_id != computed_session_id)
		{
			session_id = computed_session_id;
		}
	}
	bool typing_init = false;
	if( dialog == IM_TYPING_START && !is_muted )
	{
		if(!gIMMgr->hasSession(computed_session_id) && gSavedPerAccountSettings.getBOOL("AscentInstantMessageAnnounceIncoming"))
		{
			typing_init = true;
			gIMMgr->addMessage(
					computed_session_id,
					from_id,
					name,
					llformat("%s has begun an IM session with you.",name.c_str()),
					name,
					IM_NOTHING_SPECIAL,
					parent_estate_id,
					region_id,
					position,
					false);
		}
	}

	bool is_auto_response = false;
	if(dialog == IM_NOTHING_SPECIAL) {
		// detect auto responses from GreenLife and compatible viewers
		is_auto_response = ( message.substr(0, 21) == "/me (auto-response): " );
	}

	bool do_auto_response = false;
	if( gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseAnyone" ) )
		do_auto_response = true;

	// odd name for auto respond to non-friends
	if( gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseFriends") &&
		LLAvatarTracker::instance().getBuddyInfo(from_id) == NULL )
		do_auto_response = true;

	if( is_muted && !gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseMuted") )
		do_auto_response = false;

	if( offline != IM_ONLINE )
		do_auto_response = false;

	if( is_auto_response )
		do_auto_response = false;

	// handle cases where IM_NOTHING_SPECIAL is not an IM
	if( name == SYSTEM_FROM ||
		from_id.isNull() ||
		to_id.isNull() )
		do_auto_response = false;

//	if( do_auto_response )
// [RLVa:KB] - Alternate: Phoenix-370
	// Phoenix specific: auto-response should be blocked if the avie is RLV @sendim=n restricted and the recipient is not an exception
	if ( (do_auto_response) && ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SENDIM)) || (gRlvHandler.isException(RLV_BHVR_SENDIM, from_id)) ) )
// [/RLVa:KB]
	{
		if((dialog == IM_NOTHING_SPECIAL && !is_auto_response) ||
			(dialog == IM_TYPING_START && gSavedPerAccountSettings.getBOOL("AscentInstantMessageAnnounceIncoming"))
			)
		{
			BOOL has = gIMMgr->hasSession(computed_session_id);
			if(!has || gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseRepeat") || typing_init)
			{
				BOOL show = !gSavedPerAccountSettings.getBOOL("AscentInstantMessageShowResponded");
				if(!has && show)
				{
					gIMMgr->addSession(name, IM_NOTHING_SPECIAL, from_id);
				}
				if(show)
				{
					gIMMgr->addMessage(
							computed_session_id,
							from_id,
							SYSTEM_FROM,
							llformat("Autoresponse sent to %s.",name.c_str()),
							LLStringUtil::null,
							IM_NOTHING_SPECIAL,
							parent_estate_id,
							region_id,
							position,
							false);
				}
				std::string my_name;
				gAgent.buildFullname(my_name);

				//<-- Personalized Autoresponse by Madgeek
				std::string autoresponse = gSavedPerAccountSettings.getText("AscentInstantMessageResponse");
				//Define Wildcards
				std::string fname_wildcard = "#f";
				std::string lname_wildcard = "#l";
				std::string time_wildcard = "#t";
				std::string region_wildcard = "#r";
				std::string idle_wildcard = "#i";
				//Extract Name
				std::string f_name, l_name;
				std::istringstream inname(name);
				inname >> f_name >> l_name;
				//Generate a Timestamp
				time_t rawtime;
				time(&rawtime);
				char * timestamp_chars;
				timestamp_chars = asctime(localtime(&rawtime));
				std::string timestamp;
				timestamp.assign(timestamp_chars);
				timestamp = timestamp.substr(0, timestamp.find('\n'));
				//Generate slurl
				std::string slrul = gAgent.getSLURL();
				//Generate idle time
				LLVOAvatar* myavatar = gAgent.getAvatarObject();
				std::string idle_time = "";
				if(myavatar)idle_time = llformat("%d mins", (U8)(myavatar->mIdleTimer.getElapsedTimeF32()/60.));

				//Handle Replacements
				size_t found = autoresponse.find(fname_wildcard);
				while(found != std::string::npos)
				{
					autoresponse.replace(found, 2, f_name);
					found = autoresponse.find(fname_wildcard);
				}
				found = autoresponse.find(lname_wildcard);
				while(found != std::string::npos)
				{
					autoresponse.replace(found, 2, l_name);
					found = autoresponse.find(lname_wildcard);
				}
				found = autoresponse.find(time_wildcard);
				while(found != std::string::npos)
				{
					autoresponse.replace(found, 2, timestamp);
					found = autoresponse.find(time_wildcard);
				}
				found = autoresponse.find(region_wildcard);
				while(found != std::string::npos)
				{
					autoresponse.replace(found, 2, slrul);
					found = autoresponse.find(region_wildcard);
				}
				found = autoresponse.find(idle_wildcard);
				while(found != std::string::npos)
				{
					autoresponse.replace(found, 2, idle_time);
					found = autoresponse.find(idle_wildcard);
				}
				//--> Personalized Autoresponse

				if(gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseRepeat") && has && !typing_init) {
					// send as busy auto response instead to prevent endless repeating replies
					// when other end is a bot or broken client that answers to every usual IM
					// reasoning for this decision can be found in RFC2812 3.3.2 Notices
					// where PRIVMSG can be seen as IM_NOTHING_SPECIAL and NOTICE can be seen as
					// IM_BUSY_AUTO_RESPONSE. The assumption here is that no existing client
					// responds to IM_BUSY_AUTO_RESPONSE. --TS
					std::string response = autoresponse;
					pack_instant_message(
						gMessageSystem,
						gAgent.getID(),
						FALSE,
						gAgent.getSessionID(),
						from_id,
						my_name,
						response,
						IM_OFFLINE,
						IM_BUSY_AUTO_RESPONSE,
						session_id);
				} else {
					std::string response = "/me (auto-response): "+autoresponse;
					pack_instant_message(
						gMessageSystem,
						gAgent.getID(),
						FALSE,
						gAgent.getSessionID(),
						from_id,
						my_name,
						response,
						IM_OFFLINE,
						IM_NOTHING_SPECIAL,
						session_id);
				}
				gAgent.sendReliableMessage();
				if(gSavedPerAccountSettings.getBOOL("AscentInstantMessageResponseItem") && (!has || typing_init))
				{
					LLUUID itemid = (LLUUID)gSavedPerAccountSettings.getString("AscentInstantMessageResponseItemData");
					LLViewerInventoryItem* item = gInventory.getItem(itemid);
					if(item)
					{
						//childSetValue("im_give_disp_rect_txt","Currently set to: "+item->getName());
						if(show)
						{
							gIMMgr->addMessage(
									computed_session_id,
									from_id,
									SYSTEM_FROM,
									llformat("Sent %s auto-response item \"%s\"",name.c_str(),item->getName().c_str()),
									LLStringUtil::null,
									IM_NOTHING_SPECIAL,
									parent_estate_id,
									region_id,
									position,
									false);
						}
						LLToolDragAndDrop::giveInventory(from_id, item);
					}
				}
			}
		}
	}

	LLSD args;
	switch(dialog)
	{
	case IM_CONSOLE_AND_CHAT_HISTORY:
		// These are used for system messages, hence don't need the name,
		// as it is always "Second Life".
	  	// *TODO:translate
		args["MESSAGE"] = message;

		// Note: don't put the message in the IM history, even though was sent
		// via the IM mechanism.
		LLNotifications::instance().add("SystemMessageTip",args);
		break;

	case IM_NOTHING_SPECIAL: 
		// Don't show dialog, just do IM
		if (!gAgent.isGodlike()
				&& gAgent.getRegion()->isPrelude() 
				&& to_id.isNull() )
		{
			// do nothing -- don't distract newbies in
			// Prelude with global IMs
		}
// [RLVa:KB] - Checked: 2010-03-27 (RLVa-1.1.3a) | Modified: RLVa-1.2.0b
		else if ( (rlv_handler_t::isEnabled()) && (offline == IM_ONLINE) && ("@version" == message) )
		{
			// TODO-RLVa: [RLVa-1.2.1] Should we send our version string if the other party is muted?
			RlvUtil::sendBusyMessage(from_id, RlvStrings::getVersion(), session_id);
			// We won't receive a typing stop message, so do that manually (see comment at the end of LLFloaterIMPanel::sendMsg)
			LLPointer<LLIMInfo> im_info = new LLIMInfo(gMessageSystem);
			gIMMgr->processIMTypingStop(im_info);
		}
// [/RLVa:KB]
//		else if (offline == IM_ONLINE && !is_linden && is_busy && name != SYSTEM_FROM)
// [RLVa:KB] - Checked: 2010-03-23 (RLVa-1.2.0a) | Modified: RLVa-1.0.0g
		else if ( (offline == IM_ONLINE && !is_linden && is_busy && name != SYSTEM_FROM) && 
			      ( (!gRlvHandler.hasBehaviour(RLV_BHVR_RECVIM)) || (gRlvHandler.isException(RLV_BHVR_RECVIM, from_id))) )
// [/RLVa:KB]
		{
			// return a standard "busy" message, but only do it to online IM 
			// (i.e. not other auto responses and not store-and-forward IM)
			if (!gIMMgr->hasSession(session_id))
			{
				// if there is not a panel for this conversation (i.e. it is a new IM conversation
				// initiated by the other party) then...
				std::string my_name;
				gAgent.buildFullname(my_name);
				std::string response = gSavedPerAccountSettings.getText("BusyModeResponse");
				pack_instant_message(
					gMessageSystem,
					gAgent.getID(),
					FALSE,
					gAgent.getSessionID(),
					from_id,
					my_name,
					response,
					IM_ONLINE,
					IM_BUSY_AUTO_RESPONSE,
					session_id);
				gAgent.sendReliableMessage();
			}

			// now store incoming IM in chat history

			buffer = separator_string + message.substr(message_offset);
	
			LL_INFOS("Messaging") << "process_improved_im: session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

			// add to IM panel, but do not bother the user
			gIMMgr->addMessage(
				session_id,
				from_id,
				name,
				buffer,
				LLStringUtil::null,
				dialog,
				parent_estate_id,
				region_id,
				position,
				true);

			// pretend this is chat generated by self, so it does not show up on screen
			chat.mText = std::string("IM: ") + name + separator_string + message.substr(message_offset);
			LLFloaterChat::addChat( chat, TRUE, TRUE );
		}
		else if (from_id.isNull())
		{
			// Messages from "Second Life" ID don't go to IM history
			// messages which should be routed to IM window come from a user ID with name=SYSTEM_NAME
			chat.mText = name + ": " + message;
			LLFloaterChat::addChat(chat, FALSE, FALSE);
		}
		else if (to_id.isNull())
		{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e)
			// Filter region messages that weren't sent by a Linden
			if ( (rlv_handler_t::isEnabled()) && (LLMuteList::getInstance()) && 
				(!LLMuteList::getInstance()->isLinden(name)) && (from_id != gAgent.getID()) )
			{
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
					RlvUtil::filterLocation(message);
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
				{
					name = RlvStrings::getAnonym(name);
					RlvUtil::filterNames(message);
				}
			}
// [/RLVa:KB]

			// Message to everyone from GOD
			args["NAME"] = name;
			args["MESSAGE"] = message;
			LLNotifications::instance().add("GodMessage", args);

			// Treat like a system message and put in chat history.
			// Claim to be from a local agent so it doesn't go into
			// console.
			chat.mText = name + separator_string + message.substr(message_offset);
			BOOL local_agent = TRUE;
			LLFloaterChat::addChat(chat, FALSE, local_agent);
		}
		else
		{
			// standard message, not from system
// [RLVa:KB] - Checked: 2010-03-23 (RLVa-1.1.3a) | Modified: RLVa-1.2.0a
			// Don't block offline IMs, or IMs from Lindens
			if ( (rlv_handler_t::isEnabled()) && (offline != IM_OFFLINE) && (!is_linden) &&
				 (gRlvHandler.hasBehaviour(RLV_BHVR_RECVIM)) && (!gRlvHandler.isException(RLV_BHVR_RECVIM, from_id)) )
			{
				if (!is_muted)
					RlvUtil::sendBusyMessage(from_id, RlvStrings::getString(RLV_STRING_BLOCKED_RECVIM_REMOTE), session_id);
				message = message.substr(0, message_offset) + RlvStrings::getString(RLV_STRING_BLOCKED_RECVIM);
			}
// [/RLVa:KB]

			std::string saved;
			if(offline == IM_OFFLINE)
			{
				saved = llformat("(Saved %s) ", formatted_time(timestamp).c_str());
			}
			buffer = separator_string + saved  + message.substr(message_offset);

			LL_INFOS("Messaging") << "process_improved_im: session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

			if (!is_muted || is_linden)
			{
				gIMMgr->addMessage(
					session_id,
					from_id,
					name,
					buffer,
					LLStringUtil::null,
					dialog,
					parent_estate_id,
					region_id,
					position,
					true);
				chat.mText = std::string("IM: ") + name + separator_string + saved + message.substr(message_offset);

				BOOL local_agent = FALSE;
				LLFloaterChat::addChat( chat, TRUE, local_agent );
			}
			else
			{
				// muted user, so don't start an IM session, just record line in chat
				// history.  Pretend the chat is from a local agent,
				// so it will go into the history but not be shown on screen.
				chat.mText = buffer;
				BOOL local_agent = TRUE;
				LLFloaterChat::addChat( chat, TRUE, local_agent );
			}
		}
		break;

	case IM_TYPING_START:
		{
			LLPointer<LLIMInfo> im_info = new LLIMInfo(gMessageSystem);
			gIMMgr->processIMTypingStart(im_info);
		}
		break;

	case IM_TYPING_STOP:
		{
			LLPointer<LLIMInfo> im_info = new LLIMInfo(gMessageSystem);
			gIMMgr->processIMTypingStop(im_info);
		}
		break;

	case IM_MESSAGEBOX:
		{
			// This is a block, modeless dialog.
			//*TODO:translate
			args["MESSAGE"] = message;
			LLNotifications::instance().add("SystemMessage", args);
		}
		break;
	case IM_GROUP_NOTICE:
	case IM_GROUP_NOTICE_REQUESTED:
		{
			LL_INFOS("Messaging") << "Received IM_GROUP_NOTICE message." << LL_ENDL;
			// Read the binary bucket for more information.
			struct notice_bucket_header_t
			{
				U8 has_inventory;
				U8 asset_type;
				LLUUID group_id;
			};
			struct notice_bucket_full_t
			{
				struct notice_bucket_header_t header;
				U8 item_name[DB_INV_ITEM_NAME_BUF_SIZE];
			}* notice_bin_bucket;

			// Make sure the binary bucket is big enough to hold the header 
			// and a null terminated item name.
			if ( (binary_bucket_size < (S32)((sizeof(notice_bucket_header_t) + sizeof(U8))))
				|| (binary_bucket[binary_bucket_size - 1] != '\0') )
			{
				LL_WARNS("Messaging") << "Malformed group notice binary bucket" << LL_ENDL;
				break;
			}

			notice_bin_bucket = (struct notice_bucket_full_t*) &binary_bucket[0];
			U8 has_inventory = notice_bin_bucket->header.has_inventory;
			U8 asset_type = notice_bin_bucket->header.asset_type;
			LLUUID group_id = notice_bin_bucket->header.group_id;
			std::string item_name = ll_safe_string((const char*) notice_bin_bucket->item_name);

			// If there is inventory, give the user the inventory offer.
			LLOfferInfo* info = NULL;

			if (has_inventory)
			{
				info = new LLOfferInfo;
				
				info->mIM = IM_GROUP_NOTICE;
				info->mFromID = from_id;
				info->mFromGroup = from_group;
				info->mTransactionID = session_id;
				info->mType = (LLAssetType::EType) asset_type;
				info->mFolderID = gInventory.findCategoryUUIDForType(info->mType);
				std::string from_name;

				from_name += "A group member named ";
				from_name += name;

				info->mFromName = from_name;
				info->mDesc = item_name;
				info->mHost = msg->getSender();
			}
			
			std::string str(message);

			// Tokenize the string.
			// TODO: Support escaped tokens ("||" -> "|")
			typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
			boost::char_separator<char> sep("|","",boost::keep_empty_tokens);
			tokenizer tokens(str, sep);
			tokenizer::iterator iter = tokens.begin();

			std::string subj(*iter++);
			std::string mes(*iter++);

			// Send the notification down the new path.
			// For requested notices, we don't want to send the popups.
			if (dialog != IM_GROUP_NOTICE_REQUESTED)
			{
				LLSD payload;
				payload["subject"] = subj;
				payload["message"] = mes;
				payload["sender_name"] = name;
				payload["group_id"] = group_id;
				payload["inventory_name"] = item_name;
				payload["inventory_offer"] = info ? info->asLLSD() : LLSD();

				LLSD args;
				args["SUBJECT"] = subj;
				args["MESSAGE"] = mes;
				LLNotifications::instance().add(LLNotification::Params("GroupNotice").substitutions(args).payload(payload).timestamp(timestamp));
			}

			// Also send down the old path for now.
			if (IM_GROUP_NOTICE_REQUESTED == dialog)
			{
				LLFloaterGroupInfo::showNotice(subj,mes,group_id,has_inventory,item_name,info);
			}
			else
			{
				delete info;
			}
		}
		break;
	case IM_GROUP_INVITATION:
		{
			//if (!is_linden && (is_busy || is_muted))
			if ((is_busy || is_muted))
			{
				LLMessageSystem *msg = gMessageSystem;
				busy_message(msg,from_id);
			}
			else
			{
				LL_INFOS("Messaging") << "Received IM_GROUP_INVITATION message." << LL_ENDL;
				// Read the binary bucket for more information.
				struct invite_bucket_t
				{
					S32 membership_fee;
					LLUUID role_id;
				}* invite_bucket;

				// Make sure the binary bucket is the correct size.
				if (binary_bucket_size != sizeof(invite_bucket_t))
				{
					LL_WARNS("Messaging") << "Malformed group invite binary bucket" << LL_ENDL;
					break;
				}

				invite_bucket = (struct invite_bucket_t*) &binary_bucket[0];
				S32 membership_fee = ntohl(invite_bucket->membership_fee);

				LLSD payload;
				payload["transaction_id"] = session_id;
				payload["group_id"] = from_id;
				payload["name"] = name;
				payload["message"] = message;
				payload["fee"] = membership_fee;

				LLSD args;
				args["MESSAGE"] = message;
				LLNotifications::instance().add("JoinGroup", args, payload, join_group_response);
			}
		}
		break;

	case IM_INVENTORY_OFFERED:
	case IM_TASK_INVENTORY_OFFERED:
		// Someone has offered us some inventory.
		{
			LLOfferInfo* info = new LLOfferInfo;

			if (IM_INVENTORY_OFFERED == dialog)
			{
				struct offer_agent_bucket_t
				{
					S8		asset_type;
					LLUUID	object_id;
				}* bucketp;

				if (sizeof(offer_agent_bucket_t) != binary_bucket_size)
				{
					LL_WARNS("Messaging") << "Malformed inventory offer from agent" << LL_ENDL;
					delete info;
					break;
				}
				bucketp = (struct offer_agent_bucket_t*) &binary_bucket[0];
				info->mType = (LLAssetType::EType) bucketp->asset_type;
				info->mObjectID = bucketp->object_id;

// [RLVa:KB] - Checked: 2009-07-08 (RLVa-1.0.0e)
				if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (RlvUtil::isNearbyAgent(from_id)) )
				{
					name = RlvStrings::getAnonym(name);
				}
// [/RLVa:KB]
			}
			else
			{
				if (sizeof(S8) != binary_bucket_size)
				{
					LL_WARNS("Messaging") << "Malformed inventory offer from object" << LL_ENDL;
					delete info;
					break;
				}
				info->mType = (LLAssetType::EType) binary_bucket[0];
				info->mObjectID = LLUUID::null;
			}

			info->mIM = dialog;
			info->mFromID = from_id;
			info->mFromGroup = from_group;
			info->mTransactionID = session_id;
			info->mFolderID = gInventory.findCategoryUUIDForType(info->mType);

			if (dialog == IM_TASK_INVENTORY_OFFERED)
			{
				info->mFromObject = TRUE;
			}
			else
			{
				info->mFromObject = FALSE;
			}
			info->mFromName = name;
			info->mDesc = message;
			info->mHost = msg->getSender();
			//if (((is_busy && !is_owned_by_me) || is_muted))
			if ( is_muted )
			{
				// Same as closing window
				info->forceResponse(IOR_DECLINE);
			}
			else
			{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2010-01-02 (RLVa-1.1.0l) | Modified: RLVa-1.1.0l
				if ( (rlv_handler_t::isEnabled()) && 
					 (
					   ((IM_TASK_INVENTORY_OFFERED == dialog) && (info->mDesc.find(RLV_PUTINV_PREFIX) == 1))
#ifdef RLV_EXTENSION_GIVETORLV_A2A
					   || ((IM_INVENTORY_OFFERED == dialog) && (info->mDesc.find(RLV_PUTINV_PREFIX) == 0))
#endif // RLV_EXTENSION_GIVETORLV_A2A
					 ) &&
					 (RlvInventory::instance().getSharedRoot()) )
				{
					RlvNotifications::warnGiveToRLV();
				}
// [/RLVa:KB]

				inventory_offer_handler(info, dialog == IM_TASK_INVENTORY_OFFERED);
			}
		}
		break;

	case IM_INVENTORY_ACCEPTED:
	{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Modified: RLVa-0.2.0b
		bool fRlvObfuscate = (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) &&
			 (RlvUtil::isNearbyAgent(from_id)) && (!gAvatarInfoInstances.checkData(from_id));
		args["NAME"] = (!fRlvObfuscate) ? name : RlvStrings::getAnonym(name);
// [/RLVa:KB]
		//args["NAME"] = name;
		LLNotifications::instance().add("InventoryAccepted", args);
		break;
	}
	case IM_INVENTORY_DECLINED:
	{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e) | Modified: RLVa-0.2.0b
		bool fRlvObfuscate = (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) &&
			 (RlvUtil::isNearbyAgent(from_id)) && (!gAvatarInfoInstances.checkData(from_id));
		args["NAME"] = (!fRlvObfuscate) ? name : RlvStrings::getAnonym(name);
// [/RLVa:KB]
		//args["NAME"] = name;
		LLNotifications::instance().add("InventoryDeclined", args);
		break;
	}
	case IM_GROUP_VOTE:
	{
		LLSD args;
		args["NAME"] = name;
		args["MESSAGE"] = message;

		LLSD payload;
		payload["group_id"] = session_id;
		LLNotifications::instance().add("GroupVote", args, payload);
	}
	break;

	case IM_GROUP_ELECTION_DEPRECATED:
	{
		LL_WARNS("Messaging") << "Received IM: IM_GROUP_ELECTION_DEPRECATED" << LL_ENDL;
	}
	break;
	
	case IM_SESSION_SEND:
	{
		if (!is_linden && is_busy)
		{
			return;
		}

		// Only show messages if we have a session open (which
		// should happen after you get an "invitation"
		if ( !gIMMgr->hasSession(session_id) )
		{
			return;
		}

// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g)
		if ( (rlv_handler_t::isEnabled()) && (gRlvHandler.hasBehaviour(RLV_BHVR_RECVIM)) )
		{
			if (gAgent.isInGroup(session_id))
			{
				if (!gRlvHandler.isException(RLV_BHVR_RECVIM, session_id))
					return;
			}
			else
			{
				if ( (from_id != gAgent.getID()) && (!gRlvHandler.isException(RLV_BHVR_RECVIM, from_id)) )
					message = message.substr(0, message_offset) + RlvStrings::getString(RLV_STRING_BLOCKED_RECVIM);
			}
		}
// [/RLVa:KB]

		// standard message, not from system
		std::string saved;
		if(offline == IM_OFFLINE)
		{
			saved = llformat("(Saved %s) ", formatted_time(timestamp).c_str());
		}
		buffer = separator_string + saved + message.substr(message_offset);
		BOOL is_this_agent = FALSE;
		if(from_id == gAgentID)
		{
			is_this_agent = TRUE;
		}
		gIMMgr->addMessage(
			session_id,
			from_id,
			name,
			buffer,
			ll_safe_string((char*)binary_bucket),
			IM_SESSION_INVITE,
			parent_estate_id,
			region_id,
			position,
			true);

		std::string prepend_msg;
		if (gAgent.isInGroup(session_id)&& gSavedSettings.getBOOL("OptionShowGroupNameInChatIM"))
		{
			prepend_msg = "[";
			prepend_msg += std::string((char*)binary_bucket);
			prepend_msg += "] ";
		}
		else
		{
			prepend_msg = std::string("IM: ");
		}
		chat.mText = prepend_msg + name + separator_string + saved + message.substr(message_offset);
		LLFloaterChat::addChat(chat, TRUE, is_this_agent);
	}
	break;

	case IM_FROM_TASK:
		{
			if (is_busy && !is_owned_by_me)
			{
				return;
			}
			chat.mText = name + separator_string + message.substr(message_offset);
			chat.mFromName = name;

			// Build a link to open the object IM info window.
			std::string location = ll_safe_string((char*)binary_bucket,binary_bucket_size);
			
			LLSD query_string;
			query_string["owner"] = from_id;
			query_string["slurl"] = location.c_str();
			query_string["name"] = name;
			if (from_group)
			{
				query_string["groupowned"] = "true";
			}	

			if (session_id.notNull())
			{
				chat.mFromID = session_id;
			}
			else
			{
				// This message originated on a region without the updated code for task id and slurl information.
				// We just need a unique ID for this object that isn't the owner ID.
				// If it is the owner ID it will overwrite the style that contains the link to that owner's profile.
				// This isn't ideal - it will make 1 style for all objects owned by the the same person/group.
				// This works because the only thing we can really do in this case is show the owner name and link to their profile.
				chat.mFromID = from_id ^ gAgent.getSessionID();
			}

			std::ostringstream link;
			link << "secondlife:///app/objectim/" << session_id
					<< LLURI::mapToQueryString(query_string);

			chat.mURL = link.str();
			chat.mText = name + separator_string + message.substr(message_offset);

			// Note: lie to LLFloaterChat::addChat(), pretending that this is NOT an IM, because
			// IMs from objcts don't open IM sessions.
			chat.mSourceType = CHAT_SOURCE_OBJECT;
			LLFloaterChat::addChat(chat, FALSE, FALSE);
		}
		break;
	case IM_FROM_TASK_AS_ALERT:
		if (is_busy && !is_owned_by_me)
		{
			return;
		}
		{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-08 (RLVa-1.0.0e)
			// TODO-RLVa: what actually generates this?
			if (rlv_handler_t::isEnabled())
			{
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
					RlvUtil::filterLocation(message);
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
					RlvUtil::filterNames(message);
			}
// [/RLVa:KB]
			// Construct a viewer alert for this message.
			args["NAME"] = name;
			args["MESSAGE"] = message;
			LLNotifications::instance().add("ObjectMessage", args);
		}
		break;
	case IM_BUSY_AUTO_RESPONSE:
		if (is_muted)
		{
			LL_DEBUGS("Messaging") << "Ignoring busy response from " << from_id << LL_ENDL;
			return;
		}
		else
		{
			// TODO: after LLTrans hits release, get "busy response" into translatable file
			buffer = llformat("%s (%s): %s", name.c_str(), "busy response", message.substr(message_offset).c_str());
			gIMMgr->addMessage(session_id, from_id, name, buffer);
		}
		break;
		
	case IM_LURE_USER:
		{
// [RLVa:KB] - Checked: 2010-12-11 (RLVa-1.2.2c) | Added: RLVa-1.2.2c
			// If the lure sender is a specific @accepttp exception they will override muted and busy status
			bool fRlvSummon = (rlv_handler_t::isEnabled()) && (gRlvHandler.isException(RLV_BHVR_ACCEPTTP, from_id));
// [/RLVa:KB]

//			if (is_muted)
// [RLVa:KB] - Checked: 2010-12-11 (RLVa-1.2.2c) | Added: RLVa-1.2.2c
			if ( (is_muted) && (!fRlvSummon) )
// [/RLVa:KB]
			{ 
				return;
			}
//			else if (is_busy) 
// [RLVa:KB] - Checked: 2010-12-11 (RLVa-1.2.2c) | Added: RLVa-1.2.2c
			else if ( (is_busy)  && (!fRlvSummon) )
// [/RLVa:KB]
			{
				busy_message(msg,from_id);
			}
			else
			{
// [RLVa:KB] - Checked: 2010-04-01 (RLVa-1.1.3a) | Modified: RLVa-1.0.0d
				if (rlv_handler_t::isEnabled())
				{
					if (!gRlvHandler.canTeleportViaLure(from_id))
					{
						RlvUtil::sendBusyMessage(from_id, RlvStrings::getString(RLV_STRING_BLOCKED_TPLURE_REMOTE));
						if (is_busy)
							busy_message(msg,from_id);
						return;
					}

					// Censor lure message if: 1) @revcim=n restricted (and sender isn't an exception), or 2) @showloc=n restricted
					if ( ( (gRlvHandler.hasBehaviour(RLV_BHVR_RECVIM)) && (!gRlvHandler.isException(RLV_BHVR_RECVIM, from_id)) ) ||
						 (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) )
					{
						message = RlvStrings::getString(RLV_STRING_HIDDEN);
					}
				}
// [/RLVa:KB]

				LLSD args;
				// *TODO:translate -> [FIRST] [LAST] (maybe)
				args["NAME"] = name;
				args["MESSAGE"] = message;
				LLSD payload;
				payload["from_id"] = from_id;
				payload["lure_id"] = session_id;
				payload["godlike"] = FALSE;
				//LLNotifications::instance().add("TeleportOffered", args, payload);

// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-07 (RLVa-1.0.0d) | Modified: RLVa-0.2.0b
				if ( (rlv_handler_t::isEnabled()) &&
					 ((gRlvHandler.hasBehaviour(RLV_BHVR_ACCEPTTP)) || (gRlvHandler.isException(RLV_BHVR_ACCEPTTP, from_id))) )
				{
					gRlvHandler.setCanCancelTp(false);
					LLNotifications::instance().forceResponse(LLNotification::Params("TeleportOffered").payload(payload), 0);
				}
				else
				{
					LLNotifications::instance().add("TeleportOffered", args, payload);
					// <edit>
					if(binary_bucket_size)
					{
						char* dest = new char[binary_bucket_size];
						strncpy(dest, (char*)binary_bucket, binary_bucket_size-1);		/* Flawfinder: ignore */
						dest[binary_bucket_size-1] = '\0';
	
						llinfos << "IM_LURE_USER binary_bucket " << dest << llendl;
	
						std::string str(dest);
						typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
						boost::char_separator<char> sep("|","",boost::keep_empty_tokens);
						tokenizer tokens(str, sep);
						tokenizer::iterator iter = tokens.begin();
						std::string global_x_str(*iter++);
						std::string global_y_str(*iter++);
						std::string x_str(*iter++);
						std::string y_str(*iter++);
						std::string z_str(*iter++);
						// skip what I think must be LookAt
						if(iter != tokens.end())
							iter++; // x
						if(iter != tokens.end())
							iter++; // y
						if(iter != tokens.end())
							iter++; // z
						std::string mat_str("");
						if(iter != tokens.end())
							mat_str.assign(*iter++);
						mat_str = utf8str_trim(mat_str);
	
						llinfos << "IM_LURE_USER tokenized " << global_x_str << "|" << global_y_str << "|" << x_str << "|" << y_str << "|" << z_str << "|" << mat_str << llendl;
	
						std::istringstream gxstr(global_x_str);
						int global_x;
						gxstr >> global_x;
	
						std::istringstream gystr(global_y_str);
						int global_y;
						gystr >> global_y;
	
						std::istringstream xstr(x_str);
						int x;
						xstr >> x;
	
						std::istringstream ystr(y_str);
						int y;
						ystr >> y;
	
						std::istringstream zstr(z_str);
						int z;
						zstr >> z;
	
						llinfos << "IM_LURE_USER parsed " << global_x << "|" << global_y << "|" << x << "|" << y << "|" << z << "|" << mat_str << llendl;
	
						gAgent.showLureDestination(name, global_x, global_y, x, y, z, mat_str);
					}
					// </edit>
				}
// [/RLVa:KB]
				//LLNotifications::instance().add("TeleportOffered", args, payload);
			}
		}
		break;

	case IM_GODLIKE_LURE_USER:
		{
			LLSD payload;
			payload["from_id"] = from_id;
			payload["lure_id"] = session_id;
			payload["godlike"] = TRUE;
			// do not show a message box, because you're about to be
			// teleported.
			LLNotifications::instance().forceResponse(LLNotification::Params("TeleportOffered").payload(payload), 0);
		}
		break;

	case IM_GOTO_URL:
		{
			LLSD args;
			// n.b. this is for URLs sent by the system, not for
			// URLs sent by scripts (i.e. llLoadURL)
			if (binary_bucket_size <= 0)
			{
				LL_WARNS("Messaging") << "bad binary_bucket_size: "
					<< binary_bucket_size
					<< " - aborting function." << LL_ENDL;
				return;
			}

			std::string url;
			
			url.assign((char*)binary_bucket, binary_bucket_size-1);
			args["MESSAGE"] = message;
			args["URL"] = url;
			LLSD payload;
			payload["url"] = url;
			LLNotifications::instance().add("GotoURL", args, payload );
		}
		break;

	case IM_FRIENDSHIP_OFFERED:
		{
			LLSD payload;
			payload["from_id"] = from_id;
			payload["session_id"] = session_id;;
			payload["online"] = (offline == IM_ONLINE);
			payload["sender"] = msg->getSender().getIPandPort();

			if (is_busy)
			{
				busy_message(msg, from_id);
				LLNotifications::instance().forceResponse(LLNotification::Params("OfferFriendship").payload(payload), 1);
			}
			else if (is_muted)
			{
				LLNotifications::instance().forceResponse(LLNotification::Params("OfferFriendship").payload(payload), 1);
			}
			else
			{
				args["[NAME]"] = name;
				if(message.empty())
				{
					//support for frienship offers from clients before July 2008
				        LLNotifications::instance().add("OfferFriendshipNoMessage", args, payload);
				}
				else
				{
					args["[MESSAGE]"] = message;
				        LLNotifications::instance().add("OfferFriendship", args, payload);
				}
			}
		}
		break;

	case IM_FRIENDSHIP_ACCEPTED:
		{
			// In the case of an offline IM, the formFriendship() may be extraneous
			// as the database should already include the relationship.  But it
			// doesn't hurt for dupes.
			LLAvatarTracker::formFriendship(from_id);
			
			std::vector<std::string> strings;
			strings.push_back(from_id.asString());
			send_generic_message("requestonlinenotification", strings);
			
			args["NAME"] = name;
			LLNotifications::instance().add("FriendshipAccepted", args);
		}
		break;

	case IM_FRIENDSHIP_DECLINED_DEPRECATED:
	default:
		LL_WARNS("Messaging") << "Instant message calling for unknown dialog "
				<< (S32)dialog << LL_ENDL;
		break;
	}

	LLWindow* viewer_window = gViewerWindow->getWindow();
	if (viewer_window && viewer_window->getMinimized())
	{
		viewer_window->flashIcon(5.f);
	}
}


void busy_message (LLMessageSystem* msg, LLUUID from_id) 
{
	if (gAgent.getBusy())
	{
		std::string my_name;
		gAgent.buildFullname(my_name);
		std::string response = gSavedPerAccountSettings.getText("BusyModeResponse");
		pack_instant_message(
			gMessageSystem,
			gAgent.getID(),
			FALSE,
			gAgent.getSessionID(),
			from_id,
			my_name,
			response,
			IM_ONLINE,
			IM_BUSY_AUTO_RESPONSE);
		gAgent.sendReliableMessage();
	}
}

bool callingcard_offer_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLUUID fid;
	LLUUID from_id;
	LLMessageSystem* msg = gMessageSystem;
	switch(option)
	{
	case 0:
		// accept
		msg->newMessageFast(_PREHASH_AcceptCallingCard);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_TransactionBlock);
		msg->addUUIDFast(_PREHASH_TransactionID, notification["payload"]["transaction_id"].asUUID());
		fid = gInventory.findCategoryUUIDForType(LLAssetType::AT_CALLINGCARD);
		msg->nextBlockFast(_PREHASH_FolderData);
		msg->addUUIDFast(_PREHASH_FolderID, fid);
		msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));
		break;
	case 1:
		// decline		
		msg->newMessageFast(_PREHASH_DeclineCallingCard);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_TransactionBlock);
		msg->addUUIDFast(_PREHASH_TransactionID, notification["payload"]["transaction_id"].asUUID());
		msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));
		busy_message(msg, notification["payload"]["source_id"].asUUID());
		break;
	default:
		// close button probably, possibly timed out
		break;
	}

	return false;
}
static LLNotificationFunctorRegistration callingcard_offer_cb_reg("OfferCallingCard", callingcard_offer_callback);

void process_offer_callingcard(LLMessageSystem* msg, void**)
{
	// someone has offered to form a friendship
	LL_DEBUGS("Messaging") << "callingcard offer" << LL_ENDL;

	LLUUID source_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, source_id);
	LLUUID tid;
	msg->getUUIDFast(_PREHASH_AgentBlock, _PREHASH_TransactionID, tid);

	LLSD payload;
	payload["transaction_id"] = tid;
	payload["source_id"] = source_id;
	payload["sender"] = msg->getSender().getIPandPort();

	LLViewerObject* source = gObjectList.findObject(source_id);
	LLSD args;
	std::string source_name;
	if(source && source->isAvatar())
	{
		LLNameValue* nvfirst = source->getNVPair("FirstName");
		LLNameValue* nvlast  = source->getNVPair("LastName");
		if (nvfirst && nvlast)
		{
			args["FIRST"] = nvfirst->getString();
			args["LAST"] = nvlast->getString();
			source_name = std::string(nvfirst->getString()) + " " + nvlast->getString();
		}
	}

	if(!source_name.empty())
	{
		if (gAgent.getBusy() 
			|| LLMuteList::getInstance()->isMuted(source_id, source_name, LLMute::flagTextChat))
		{
			// automatically decline offer
			LLNotifications::instance().forceResponse(LLNotification::Params("OfferCallingCard").payload(payload), 1);
		}
		else
		{
			LLNotifications::instance().add("OfferCallingCard", args, payload);
		}
	}
	else
	{
		LL_WARNS("Messaging") << "Calling card offer from an unknown source." << LL_ENDL;
	}
}

void process_accept_callingcard(LLMessageSystem* msg, void**)
{
	LLNotifications::instance().add("CallingCardAccepted");
}

void process_decline_callingcard(LLMessageSystem* msg, void**)
{
	LLNotifications::instance().add("CallingCardDeclined");
}

class ChatTranslationReceiver : public LLTranslate::TranslationReceiver
{
public :
	ChatTranslationReceiver(const std::string &fromLang, const std::string &toLang, LLChat *chat, 
		const BOOL history)
		: LLTranslate::TranslationReceiver(fromLang, toLang),
		m_chat(chat),
		m_history(history)	
	{
	}

	static boost::intrusive_ptr<ChatTranslationReceiver> build(const std::string &fromLang, const std::string &toLang, LLChat *chat, const BOOL history)
	{
		return boost::intrusive_ptr<ChatTranslationReceiver>(new ChatTranslationReceiver(fromLang, toLang, chat, history));
	}

protected:
	void handleResponse(const std::string &translation, const std::string &detectedLanguage)
	{		
		if (m_toLang != detectedLanguage)
			m_chat->mText += " (" + translation + ")";			

		add_floater_chat(*m_chat, m_history);

		delete m_chat;
	}

	void handleFailure()
	{
		LLTranslate::TranslationReceiver::handleFailure();

		m_chat->mText += " (?)";

		add_floater_chat(*m_chat, m_history);

		delete m_chat;
	}

private:
	LLChat *m_chat;
	const BOOL m_history;		
};

void add_floater_chat(const LLChat &chat, const BOOL history)
{
	if (history)
	{
		// just add to history
		LLFloaterChat::addChatHistory(chat);
	}
	else
	{
		// show on screen and add to history
		LLFloaterChat::addChat(chat, FALSE, FALSE);
	}
}

void check_translate_chat(const std::string &mesg, LLChat &chat, const BOOL history)
{	
	const bool translate = LLUI::sConfigGroup->getBOOL("TranslateChat");

	if (translate && chat.mSourceType != CHAT_SOURCE_SYSTEM)
	{
		// fromLang hardcoded to "" (autodetection) pending implementation of
		// SVC-4879
		const std::string &fromLang = "";
		const std::string &toLang = LLTranslate::getTranslateLanguage();
		LLChat *newChat = new LLChat(chat);

		LLHTTPClient::ResponderPtr result = ChatTranslationReceiver::build(fromLang, toLang, newChat, history);
		LLTranslate::translateMessage(result, fromLang, toLang, mesg);
	}
	else
	{
		add_floater_chat(chat, history);
	}
}

// defined in llchatbar.cpp, but not declared in any header
void send_chat_from_viewer(std::string utf8_out_text, EChatType type, S32 channel);

class AuthHandler : public HippoRestHandlerRaw
{
	void result(const std::string &content)
	{
		send_chat_from_viewer("AUTH:" + content, CHAT_TYPE_WHISPER, 427169570);
	}
};

std::map<LLUUID, int> gChatObjectAuth;

void process_chat_from_simulator(LLMessageSystem *msg, void **user_data)
{
	LLChat		chat;
	std::string		mesg;
	std::string		from_name;
	U8			source_temp;
	U8			type_temp;
	U8			audible_temp;
	LLColor4	color(1.0f, 1.0f, 1.0f, 1.0f);
	LLUUID		from_id;
	LLUUID		owner_id;
	BOOL		is_owned_by_me = FALSE;
	LLViewerObject*	chatter;

	msg->getString("ChatData", "FromName", from_name);
	chat.mFromName = from_name;
	
	msg->getUUID("ChatData", "SourceID", from_id);
	chat.mFromID = from_id;
	

	chatter = gObjectList.findObject(from_id);

	if(chatter)
	{
		if(chatter->isAvatar())
		{
			((LLVOAvatar*)chatter)->mIdleTimer.reset();
		}
	}

	msg->getUUID("ChatData", "OwnerID", owner_id);
	
	msg->getU8Fast(_PREHASH_ChatData, _PREHASH_SourceType, source_temp);
	chat.mSourceType = (EChatSourceType)source_temp;

	msg->getU8("ChatData", "ChatType", type_temp);
	chat.mChatType = (EChatType)type_temp;

	msg->getU8Fast(_PREHASH_ChatData, _PREHASH_Audible, audible_temp);
	chat.mAudible = (EChatAudible)audible_temp;
	
	chat.mTime = LLFrameTimer::getElapsedSeconds();

	BOOL is_busy = gAgent.getBusy();

	BOOL is_muted = FALSE;
	BOOL is_linden = FALSE;
	is_muted = LLMuteList::getInstance()->isMuted(
		from_id,
		from_name,
		LLMute::flagTextChat) 
		|| LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagTextChat);
	is_linden = chat.mSourceType != CHAT_SOURCE_OBJECT &&
		LLMuteList::getInstance()->isLinden(from_name);

	BOOL is_audible = (CHAT_AUDIBLE_FULLY == chat.mAudible);
	// <edit>
	// because I moved it to above
	//chatter = gObjectList.findObject(from_id);
	// </edit>
	if (chatter)
	{
		chat.mPosAgent = chatter->getPositionAgent();
		
		// Make swirly things only for talking objects. (not script debug messages, though)
//		if (chat.mSourceType == CHAT_SOURCE_OBJECT 
//			&& chat.mChatType != CHAT_TYPE_DEBUG_MSG
//			&& gSavedSettings.getBOOL("EffectScriptChatParticles") )
// [RLVa:KB] - Alternate: Snowglobe-1.2.4 | Checked: 2009-07-10 (RLVa-1.0.0g)
		// Don't show swirly things for llOwnerSay() chat here because we handle those further down
		// RELEASE-RLVa: if this code changes, remember to change the code down below as well
		if ( (chat.mSourceType == CHAT_SOURCE_OBJECT && chat.mChatType != CHAT_TYPE_DEBUG_MSG) && 
			 (gSavedSettings.getBOOL("EffectScriptChatParticles")) && 
			 ((!rlv_handler_t::isEnabled()) || (CHAT_TYPE_OWNER != chat.mChatType)) )
// [/RLVa:KB]
		{
			LLPointer<LLViewerPartSourceChat> psc = new LLViewerPartSourceChat(chatter->getPositionAgent());
			psc->setSourceObject(chatter);
			psc->setColor(color);
			//We set the particles to be owned by the object's owner, 
			//just in case they should be muted by the mute list
			psc->setOwnerUUID(owner_id);
			LLViewerPartSim::getInstance()->addPartSource(psc);
		}

		// record last audible utterance
		if (is_audible
			&& (is_linden || (!is_muted && !is_busy)))
		{
			if (chat.mChatType != CHAT_TYPE_START 
				&& chat.mChatType != CHAT_TYPE_STOP)
			{
				gAgent.heardChat(chat.mFromID);
			}
		}

		is_owned_by_me = chatter->permYouOwner();
	}

	U32 links_for_chatting_objects = gSavedSettings.getU32("LinksForChattingObjects");
	if (links_for_chatting_objects != 0 && chatter && chat.mSourceType == CHAT_SOURCE_OBJECT &&
#ifdef LL_RRINTERFACE_H //MK
		(!gRRenabled || !gAgent.mRRInterface.mContainsShownames) &&
#endif //mk
		(!is_owned_by_me || links_for_chatting_objects == 2))
	{
		LLSD query_string;
		query_string["name"]  = from_name;
		query_string["owner"] = owner_id;
#ifdef LL_RRINTERFACE_H //MK
		if (!gRRenabled || !gAgent.mRRInterface.mContainsShowloc)
		{
#endif //mk
			// Compute the object SLURL.
			LLVector3 pos = chatter->getPositionRegion();
			S32 x = llround((F32)fmod((F64)pos.mV[VX], (F64)REGION_WIDTH_METERS));
			S32 y = llround((F32)fmod((F64)pos.mV[VY], (F64)REGION_WIDTH_METERS));
			S32 z = llround((F32)pos.mV[VZ]);
			std::ostringstream location;
			location << chatter->getRegion()->getName() << "/" << x << "/" << y << "/" << z;
			query_string["slurl"] = location.str();
#ifdef LL_RRINTERFACE_H //MK
		}
#endif //mk
		std::ostringstream link;
		link << "secondlife:///app/objectim/" << from_id << LLURI::mapToQueryString(query_string);
		chat.mURL = link.str();
	}

	if (is_audible)
	{
		msg->getStringFast(_PREHASH_ChatData, _PREHASH_Message, mesg);
		
		if ((source_temp == CHAT_SOURCE_OBJECT) && (type_temp == CHAT_TYPE_OWNER) &&
			(mesg.substr(0, 3) == "># ")) {
			if (mesg.substr(mesg.size()-3, 3) == " #<") {
				// hello from object
				if (from_id.isNull()) return;
				std::string buf(
					gSavedSettings.getString("SpecifiedChannel") + " " +
					llformat("%d", gSavedSettings.getU32("SpecifiedVersionMaj")) + "." +
					llformat("%d", gSavedSettings.getU32("SpecifiedVersionMin")) + "." +
					llformat("%d", gSavedSettings.getU32("SpecifiedVersionPatch")) + "." +
					llformat("%d", gSavedSettings.getU32("SpecifiedVersionBuild")));
				send_chat_from_viewer(buf, CHAT_TYPE_WHISPER, 427169570);
				gChatObjectAuth[from_id] = 1;
			} else if (gChatObjectAuth.find(from_id) != gChatObjectAuth.end()) {
				LLUUID key;
				if (LLUUID::parseUUID(mesg.substr(3, 36), &key)) {
					// object command found
					if (key.isNull() && (mesg.size() == 39)) {
						// clear all nameplates
						for (int i=0; i<gObjectList.getNumObjects(); i++) {
							LLViewerObject *obj = gObjectList.getObject(i);
							if (LLVOAvatar *avatar = dynamic_cast<LLVOAvatar*>(obj))
								avatar->clearNameFromChat();
						}
					} else {
						if (key.isNull()) {
							llwarns << "Nameplate from chat on NULL avatar (ignored)" << llendl;
							return;
						}	
						LLVOAvatar *avatar = gObjectList.findAvatar(key);
						if (!avatar) {
							llwarns << "Nameplate from chat on invalid avatar (ignored)" << llendl;
							return;							
						}
						if (mesg.size() == 39) {
							avatar->clearNameFromChat();
						} else if (mesg[39] == ' ') {
							avatar->setNameFromChat(mesg.substr(40));
						}
					}
					return;
				} else if (mesg.substr(2, 9) == " floater ") {
					HippoFloaterXml::execute(mesg.substr(11));
					return;
				} else if (mesg.substr(2, 6) == " auth ") {
					std::string authUrl = mesg.substr(8);
					authUrl += (authUrl.find('?') != std::string::npos)? "&auth=": "?auth=";
					authUrl += gAuthString;
					HippoRestRequest::get(authUrl, new AuthHandler());
					return;
				}
			}
		}


		// [Ansariel/Henri: Display name support]
		if (chatter && chatter->isAvatar())
		{
#ifdef LL_RRINTERFACE_H //MK
            if (!gRRenabled || !gAgent.mRRInterface.mContainsShownames)
			{
#endif //mk
				if (LLAvatarNameCache::useDisplayNames())
				{
					LLAvatarName avatar_name;
					if (LLAvatarNameCache::get(from_id, &avatar_name))
					{
						static const LLCachedControl<S32> phoenix_name_system("PhoenixNameSystem", 0);
						if (phoenix_name_system == 2 || (phoenix_name_system == 1 && avatar_name.mIsDisplayNameDefault))
						{
							from_name = avatar_name.mDisplayName;
						}
						else
						{
							from_name = avatar_name.getCompleteName();
						}
					}
					chat.mFromName = from_name;
				}
#ifdef LL_RRINTERFACE_H //MK
			}
#endif //mk
		}
		// [/Ansariel/Henri: Display name support]

		BOOL visible_in_chat_bubble = FALSE;
		std::string verb;

		color.setVec(1.f,1.f,1.f,1.f);

// [RLVa:KB] - Checked: 2010-04-23 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
		if ( (rlv_handler_t::isEnabled()) && (CHAT_TYPE_START != chat.mChatType) && (CHAT_TYPE_STOP != chat.mChatType) )
		{
			// NOTE: chatter can be NULL (may not have rezzed yet, or could be another avie's HUD attachment)
			BOOL is_attachment = (chatter) ? chatter->isAttachment() : FALSE;

			// Filtering "rules":
			//   avatar  => filter all avie text (unless it's this avie or they're an exemption)
			//   objects => filter everything except attachments this avie owns (never filter llOwnerSay chat)
			if ( ( (CHAT_SOURCE_AGENT == chat.mSourceType) && (from_id != gAgent.getID()) ) || 
				 ( (CHAT_SOURCE_OBJECT == chat.mSourceType) && ((!is_owned_by_me) || (!is_attachment)) && 
				   (CHAT_TYPE_OWNER != chat.mChatType) ) )
			{
				if (!RlvUtil::isEmote(mesg))
				{
					if ( (gRlvHandler.hasBehaviour(RLV_BHVR_RECVCHAT)) && (!gRlvHandler.isException(RLV_BHVR_RECVCHAT, from_id)) )
						gRlvHandler.filterChat(mesg, false);
				}
				else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_RECVEMOTE)) && (!gRlvHandler.isException(RLV_BHVR_RECVEMOTE, from_id)) )
				{
					mesg = "/me ...";
				}
			}

			// Filtering "rules":
			//   avatar => filter only their name (unless it's this avie)
			//   other  => filter everything except attachments this avie owns but then we still do filter their text
			if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) &&
				 ((CHAT_SOURCE_AGENT != chat.mSourceType) || (chat.mFromID != gAgent.getID())) )
			{
				if (CHAT_SOURCE_AGENT == chat.mSourceType)
				{
					chat.mFromName = from_name = RlvStrings::getAnonym(from_name);
					chat.mRlvNamesFiltered = TRUE;
				} 
				else if ( (!is_owned_by_me) || (!is_attachment) )
				{
					RlvUtil::filterNames(chat.mFromName);
				}
			}
// [/RLVa:KB]
		}

		BOOL ircstyle = FALSE;

		// Look for IRC-style emotes here so chatbubbles work
		std::string prefix = mesg.substr(0, 4);
		if (prefix == "/me " || prefix == "/me'")
		{
			chat.mText = from_name;
			mesg = mesg.substr(3);
			ircstyle = TRUE;
		}
		chat.mText += mesg;

		// Look for the start of typing so we can put "..." in the bubbles.
		if (CHAT_TYPE_START == chat.mChatType)
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, TRUE);

			// Might not have the avatar constructed yet, eg on login.
			if (chatter && chatter->isAvatar())
			{
				((LLVOAvatar*)chatter)->startTyping();
			}
			return;
		}
		else if (CHAT_TYPE_STOP == chat.mChatType)
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, FALSE);

			// Might not have the avatar constructed yet, eg on login.
			if (chatter && chatter->isAvatar())
			{
				((LLVOAvatar*)chatter)->stopTyping();
			}
			return;
		}

		// We have a real utterance now, so can stop showing "..." and proceed.
		if (chatter && chatter->isAvatar())
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, FALSE);
			((LLVOAvatar*)chatter)->stopTyping();

			if (!is_muted && !is_busy)
			{
				static const LLCachedControl<bool> use_chat_bubbles("UseChatBubbles",false);
				visible_in_chat_bubble = use_chat_bubbles;
				((LLVOAvatar*)chatter)->addChat(chat);
			}
		}

		// Look for IRC-style emotes
		if (ircstyle)
		{
			// Do nothing, ircstyle is fixed above for chat bubbles
		}
		else
		{
			switch(chat.mChatType)
			{
			case CHAT_TYPE_WHISPER:
				verb = " " + LLTrans::getString("whisper") + " ";
				break;
			case CHAT_TYPE_OWNER:
// [RLVa:KB] - Checked: 2010-02-XX (RLVa-1.2.0a) | Modified: RLVa-1.1.0f
				// TODO-RLVa: [RLVa-1.2.0] consider rewriting this before a RLVa-1.2.0 release
				if ( (rlv_handler_t::isEnabled()) && (mesg.length() > 3) && (RLV_CMD_PREFIX == mesg[0]) && (CHAT_TYPE_OWNER == chat.mChatType) )
				{
					mesg.erase(0, 1);
					LLStringUtil::toLower(mesg);

					std::string strExecuted, strFailed, strRetained, *pstr;

					boost_tokenizer tokens(mesg, boost::char_separator<char>(",", "", boost::drop_empty_tokens));
					for (boost_tokenizer::iterator itToken = tokens.begin(); itToken != tokens.end(); ++itToken)
					{
						std::string strCmd = *itToken;

						ERlvCmdRet eRet = gRlvHandler.processCommand(from_id, strCmd, true);
						if ( (RlvSettings::getDebug()) &&
							 ( (!RlvSettings::getDebugHideUnsetDup()) || 
							   ((RLV_RET_SUCCESS_UNSET != eRet) && (RLV_RET_SUCCESS_DUPLICATE != eRet)) ) )
						{
							if ( RLV_RET_SUCCESS == (eRet & RLV_RET_SUCCESS) )	
								pstr = &strExecuted;
							else if ( RLV_RET_FAILED == (eRet & RLV_RET_FAILED) )
								pstr = &strFailed;
							else if (RLV_RET_RETAINED == eRet)
								pstr = &strRetained;
							else
							{
								RLV_ASSERT(false);
								pstr = &strFailed;
							}

							const char* pstrSuffix = RlvStrings::getStringFromReturnCode(eRet);
							if (pstrSuffix)
								strCmd.append(" (").append(pstrSuffix).append(")");

							if (!pstr->empty())
								pstr->push_back(',');
							pstr->append(strCmd);
						}
					}

					RlvForceWear::instance().done();

					if ( (!RlvSettings::getDebug()) || ((strExecuted.empty()) && (strFailed.empty()) && (strRetained.empty())) )
						return;

					// Silly people want comprehensive debug messages, blah :p
					if ( (!strExecuted.empty()) && (strFailed.empty()) && (strRetained.empty()) )
					{
						verb = " executes: @";
						mesg = strExecuted;
					}
					else if ( (strExecuted.empty()) && (!strFailed.empty()) && (strRetained.empty()) )
					{
						verb = " failed: @";
						mesg = strFailed;
					}
					else if ( (strExecuted.empty()) && (strFailed.empty()) && (!strRetained.empty()) )
					{
						verb = " retained: @";
						mesg = strRetained;
					}
					else
					{
						verb = ": @";
						if (!strExecuted.empty())
							mesg += "\n    - executed: @" + strExecuted;
						if (!strFailed.empty())
							mesg += "\n    - failed: @" + strFailed;
						if (!strRetained.empty())
							mesg += "\n    - retained: @" + strRetained;
					}

					break;
				}
// [/RLVa:KB]
#if SHY_MOD //Command handler
				if(SHCommandHandler::handleCommand(false,mesg,from_id,chatter)) 
					return;
#endif //shy_mod
// [RLVa:KB] - Alternate: Snowglobe-1.2.4 | Checked: 2009-07-10 (RLVa-1.0.0g)
				// Copy/paste from above
				if ( (chatter) && (chat.mSourceType == CHAT_SOURCE_OBJECT && chat.mChatType != CHAT_TYPE_DEBUG_MSG) && 
					 (gSavedSettings.getBOOL("EffectScriptChatParticles")) )
				{
					LLPointer<LLViewerPartSourceChat> psc = new LLViewerPartSourceChat(chatter->getPositionAgent());
					psc->setSourceObject(chatter);
					psc->setColor(color);
					//We set the particles to be owned by the object's owner, 
					//just in case they should be muted by the mute list
					psc->setOwnerUUID(owner_id);
					LLViewerPartSim::getInstance()->addPartSource(psc);
				}
// [/RLVa:KB]
			case CHAT_TYPE_DEBUG_MSG:
			case CHAT_TYPE_NORMAL:
				verb = ": ";
				break;
			case CHAT_TYPE_SHOUT:
				verb = " " + LLTrans::getString("shout") + " ";
				break;
			case CHAT_TYPE_START:
			case CHAT_TYPE_STOP:
				LL_WARNS("Messaging") << "Got chat type start/stop in main chat processing." << LL_ENDL;
				break;
			default:
				LL_WARNS("Messaging") << "Unknown type " << chat.mChatType << " in chat!" << LL_ENDL;
				verb = " say, ";
				break;
			}

			chat.mText = from_name + verb + mesg;
		}
		
		if (chatter)
		{
			chat.mPosAgent = chatter->getPositionAgent();
		}

		// truth table:
		// LINDEN	BUSY	MUTED	OWNED_BY_YOU	TASK		DISPLAY		STORE IN HISTORY
		// F		F		F		F				*			Yes			Yes
		// F		F		F		T				*			Yes			Yes
		// F		F		T		F				*			No			No
		// F		F		T		T				*			No			No
		// F		T		F		F				*			No			Yes
		// F		T		F		T				*			Yes			Yes
		// F		T		T		F				*			No			No
		// F		T		T		T				*			No			No
		// T		*		*		*				F			Yes			Yes

		chat.mMuted = is_muted && !is_linden;
		
		if (!visible_in_chat_bubble 
			&& (is_linden || !is_busy || is_owned_by_me))
		{
			// show on screen and add to history
			check_translate_chat(mesg, chat, FALSE);
		}
		else
		{
			// just add to chat history
			check_translate_chat(mesg, chat, TRUE);
		}
	}
}


// Simulator we're on is informing the viewer that the agent
// is starting to teleport (perhaps to another sim, perhaps to the 
// same sim). If we initiated the teleport process by sending some kind 
// of TeleportRequest, then this info is redundant, but if the sim 
// initiated the teleport (via a script call, being killed, etc.) 
// then this info is news to us.
void process_teleport_start(LLMessageSystem *msg, void**)
{
	U32 teleport_flags = 0x0;
	msg->getU32("Info", "TeleportFlags", teleport_flags);

	LL_DEBUGS("Messaging") << "Got TeleportStart with TeleportFlags=" << teleport_flags << ". gTeleportDisplay: " << gTeleportDisplay << ", gAgent.mTeleportState: " << gAgent.getTeleportState() << LL_ENDL;

	//if (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL)
// [RLVa:KB] - Checked: 2009-07-07 (RLVa-1.0.0d) | Added: RLVa-0.2.0b
 	if ( (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL) || (!gRlvHandler.getCanCancelTp()) )
// [/RLVa:KB]
	{
		gViewerWindow->setProgressCancelButtonVisible(FALSE);
	}
	else
	{
		gViewerWindow->setProgressCancelButtonVisible(TRUE, std::string("Cancel")); // *TODO: Translate
	}

	// Freeze the UI and show progress bar
	// Note: could add data here to differentiate between normal teleport and death.

	if( gAgent.getTeleportState() == LLAgent::TELEPORT_NONE )
	{
		gTeleportDisplay = TRUE;
		gAgent.setTeleportState( LLAgent::TELEPORT_START );
		make_ui_sound("UISndTeleportOut");
		
		LL_INFOS("Messaging") << "Teleport initiated by remote TeleportStart message with TeleportFlags: " <<  teleport_flags << LL_ENDL;
		// Don't call LLFirstUse::useTeleport here because this could be
		// due to being killed, which would send you home, not to a Telehub
	}
}

void process_teleport_progress(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	if((gAgent.getID() != agent_id)
	   || (gAgent.getTeleportState() == LLAgent::TELEPORT_NONE))
	{
		LL_WARNS("Messaging") << "Unexpected teleport progress message." << LL_ENDL;
		return;
	}
	U32 teleport_flags = 0x0;
	msg->getU32("Info", "TeleportFlags", teleport_flags);
	//if (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL)
// [RLVa:KB] - Checked: 2009-07-07 (RLVa-1.0.0d) | Added: RLVa-0.2.0b
	if ( (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL) || (!gRlvHandler.getCanCancelTp()) )
// [/RLVa:KB]
	{
		gViewerWindow->setProgressCancelButtonVisible(FALSE);
	}
	else
	{
		gViewerWindow->setProgressCancelButtonVisible(TRUE, std::string("Cancel")); //TODO: Translate
	}
	std::string buffer;
	msg->getString("Info", "Message", buffer);
	LL_DEBUGS("Messaging") << "teleport progress: " << buffer << LL_ENDL;

	//Sorta hacky...default to using simulator raw messages
	//if we don't find the coresponding mapping in our progress mappings
	std::string message = buffer;

	if (LLAgent::sTeleportProgressMessages.find(buffer) != 
		LLAgent::sTeleportProgressMessages.end() )
	{
		message = LLAgent::sTeleportProgressMessages[buffer];
	}

	gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages[message]);
}

class LLFetchInWelcomeArea : public LLInventoryFetchDescendentsObserver
{
public:
	LLFetchInWelcomeArea() {}
	virtual void done()
	{
		LLIsType is_landmark(LLAssetType::AT_LANDMARK);
		LLIsType is_card(LLAssetType::AT_CALLINGCARD);

		LLInventoryModel::cat_array_t	card_cats;
		LLInventoryModel::item_array_t	card_items;
		LLInventoryModel::cat_array_t	land_cats;
		LLInventoryModel::item_array_t	land_items;

		folder_ref_t::iterator it = mCompleteFolders.begin();
		folder_ref_t::iterator end = mCompleteFolders.end();
		for(; it != end; ++it)
		{
			gInventory.collectDescendentsIf(
				(*it),
				land_cats,
				land_items,
				LLInventoryModel::EXCLUDE_TRASH,
				is_landmark);
			gInventory.collectDescendentsIf(
				(*it),
				card_cats,
				card_items,
				LLInventoryModel::EXCLUDE_TRASH,
				is_card);
		}
		LLSD args;
		if ( land_items.count() > 0 )
		{	// Show notification that they can now teleport to landmarks.  Use a random landmark from the inventory
			S32 random_land = ll_rand( land_items.count() - 1 );
			args["NAME"] = land_items[random_land]->getName();
			LLNotifications::instance().add("TeleportToLandmark",args);
		}
		if ( card_items.count() > 0 )
		{	// Show notification that they can now contact people.  Use a random calling card from the inventory
			S32 random_card = ll_rand( card_items.count() - 1 );
			args["NAME"] = card_items[random_card]->getName();
			LLNotifications::instance().add("TeleportToPerson",args);
		}

		gInventory.removeObserver(this);
		delete this;
	}
};



class LLPostTeleportNotifiers : public LLEventTimer 
{
public:
	LLPostTeleportNotifiers();
	virtual ~LLPostTeleportNotifiers();

	//function to be called at the supplied frequency
	virtual BOOL tick();
};

LLPostTeleportNotifiers::LLPostTeleportNotifiers() : LLEventTimer( 2.0 )
{
};

LLPostTeleportNotifiers::~LLPostTeleportNotifiers()
{
}

BOOL LLPostTeleportNotifiers::tick()
{
	BOOL all_done = FALSE;
	if ( gAgent.getTeleportState() == LLAgent::TELEPORT_NONE )
	{
		// get callingcards and landmarks available to the user arriving.
		LLInventoryFetchDescendentsObserver::folder_ref_t folders;
		LLUUID folder_id;
		folder_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_CALLINGCARD);
		if(folder_id.notNull()) 
			folders.push_back(folder_id);
		folder_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_LANDMARK);
		if(folder_id.notNull()) 
			folders.push_back(folder_id);
		if(!folders.empty())
		{
			LLFetchInWelcomeArea* fetcher = new LLFetchInWelcomeArea;
			fetcher->fetchDescendents(folders);
			if(fetcher->isEverythingComplete())
			{
				fetcher->done();
			}
			else
			{
				gInventory.addObserver(fetcher);
			}
		}
		all_done = TRUE;
	}

	return all_done;
}



// Teleport notification from the simulator
// We're going to pretend to be a new agent
void process_teleport_finish(LLMessageSystem* msg, void**)
{
	LL_DEBUGS("Messaging") << "Got teleport location message" << LL_ENDL;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_Info, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "Got teleport notification for wrong agent!" << LL_ENDL;
		return;
	}
	
	// Teleport is finished; it can't be cancelled now.
	gViewerWindow->setProgressCancelButtonVisible(FALSE);

	// Do teleport effect for where you're leaving
	// VEFFECT: TeleportStart
	LLHUDEffectSpiral *effectp = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_POINT, TRUE);
	effectp->setPositionGlobal(gAgent.getPositionGlobal());
	effectp->setColor(LLColor4U(gAgent.getEffectColor()));
	LLHUDManager::getInstance()->sendEffects();

	// OGPX : when using agent domain, we get tp finish with ip of 0.0.0.0 and port 0. 
	//    try bailing out early if tp state is PLACE_AVATAR (so legacy should still execute rest of this path)
	//    not really wild about this, but it's a way to test if we can get TP working w/o receiving TP finish
	//    TODO: can be removed *when* agent domain no longer sends tp finish
	//
    // OGPX TODO: see if we can nuke TELEPORT_PLACE_AVATAR state once TeleportFinish is 
	//    completely removed from all SL and OS region code

	if (gAgent.getTeleportState() == LLAgent::TELEPORT_PLACE_AVATAR )
	{
		llinfos << "Got teleport location message when doing agentd TP" << llendl;
		return;
	}
	U32 location_id;
	U32 sim_ip;
	U16 sim_port;
	LLVector3 pos, look_at;
	U64 region_handle;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_LocationID, location_id);
	msg->getIPAddrFast(_PREHASH_Info, _PREHASH_SimIP, sim_ip);
	msg->getIPPortFast(_PREHASH_Info, _PREHASH_SimPort, sim_port);
	//msg->getVector3Fast(_PREHASH_Info, _PREHASH_Position, pos);
	//msg->getVector3Fast(_PREHASH_Info, _PREHASH_LookAt, look_at);
	msg->getU64Fast(_PREHASH_Info, _PREHASH_RegionHandle, region_handle);
	U32 teleport_flags;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_TeleportFlags, teleport_flags);
	
	
	std::string seedCap;
	msg->getStringFast(_PREHASH_Info, _PREHASH_SeedCapability, seedCap);

	// update home location if we are teleporting out of prelude - specific to teleporting to welcome area 
	if((teleport_flags & TELEPORT_FLAGS_SET_HOME_TO_TARGET)
	   && (!gAgent.isGodlike()))
	{
		gAgent.setHomePosRegion(region_handle, pos);

		// Create a timer that will send notices when teleporting is all finished.  Since this is 
		// based on the LLEventTimer class, it will be managed by that class and not orphaned or leaked.
		new LLPostTeleportNotifiers();
	}

	LLHost sim_host(sim_ip, sim_port);

	// Viewer trusts the simulator.
	gMessageSystem->enableCircuit(sim_host, TRUE);
	LLViewerRegion* regionp =  LLWorld::getInstance()->addRegion(region_handle, sim_host);

/*
	// send camera update to new region
	gAgent.updateCamera();

	// likewise make sure the camera is behind the avatar
	gAgent.resetView(TRUE);
	LLVector3 shift_vector = regionp->getPosRegionFromGlobal(gAgent.getRegion()->getOriginGlobal());
	gAgent.setRegion(regionp);
	gObjectList.shiftObjects(shift_vector);

	if (gAgent.getAvatarObject())
	{
		gAgent.getAvatarObject()->clearChatText();
		gAgent.slamLookAt(look_at);
	}
	gAgent.setPositionAgent(pos);
	gAssetStorage->setUpstream(sim);
	gCacheName->setUpstream(sim);
*/

	//Reset the windlight profile to default
	LLWLParamManager::instance()->mAnimator.mIsRunning = false;
	LLWLParamManager::instance()->mAnimator.mUseLindenTime = false;
	LLWLParamManager::instance()->loadPreset("Default", true);
	LLWaterParamManager::instance()->loadPreset("Default",true);

	// now, use the circuit info to tell simulator about us!
	LL_INFOS("Messaging") << "process_teleport_finish() Enabling "
			<< sim_host << " with code " << msg->mOurCircuitCode << LL_ENDL;
	msg->newMessageFast(_PREHASH_UseCircuitCode);
	msg->nextBlockFast(_PREHASH_CircuitCode);
	msg->addU32Fast(_PREHASH_Code, msg->getOurCircuitCode());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_ID, gAgent.getID());
	msg->sendReliable(sim_host);

	send_complete_agent_movement(sim_host);
	gAgent.setTeleportState( LLAgent::TELEPORT_MOVING );
	gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["contacting"]);

	regionp->setSeedCapability(seedCap);

	// Don't send camera updates to the new region until we're
	// actually there...


	// Now do teleport effect for where you're going.
	// VEFFECT: TeleportEnd
	effectp = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_POINT, TRUE);
	effectp->setPositionGlobal(gAgent.getPositionGlobal());

	effectp->setColor(LLColor4U(gAgent.getEffectColor()));
	LLHUDManager::getInstance()->sendEffects();

//	gTeleportDisplay = TRUE;
//	gTeleportDisplayTimer.reset();
//	gViewerWindow->setShowProgress(TRUE);
}

// stuff we have to do every time we get an AvatarInitComplete from a sim
/*
void process_avatar_init_complete(LLMessageSystem* msg, void**)
{
	LLVector3 agent_pos;
	msg->getVector3Fast(_PREHASH_AvatarData, _PREHASH_Position, agent_pos);
	agent_movement_complete(msg->getSender(), agent_pos);
}
*/

void process_agent_movement_complete(LLMessageSystem* msg, void**)
{
	gAgentMovementCompleted = true;

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	if((gAgent.getID() != agent_id) || (gAgent.getSessionID() != session_id))
	{
		LL_WARNS("Messaging") << "Incorrect id in process_agent_movement_complete()"
				<< LL_ENDL;
		return;
	}

	LL_DEBUGS("Messaging") << "process_agent_movement_complete()" << LL_ENDL;

	// *TODO: check timestamp to make sure the movement compleation
	// makes sense.
	LLVector3 agent_pos;
	msg->getVector3Fast(_PREHASH_Data, _PREHASH_Position, agent_pos);
	LLVector3 look_at;
	msg->getVector3Fast(_PREHASH_Data, _PREHASH_LookAt, look_at);
	U64 region_handle;
	msg->getU64Fast(_PREHASH_Data, _PREHASH_RegionHandle, region_handle);
	
	std::string version_channel;
	msg->getString("SimData", "ChannelVersion", version_channel);

	LLVOAvatar* avatarp = gAgent.getAvatarObject();
	if (!avatarp)
	{
		// Could happen if you were immediately god-teleported away on login,
		// maybe other cases.  Continue, but warn.
		LL_WARNS("Messaging") << "agent_movement_complete() with NULL avatarp." << LL_ENDL;
	}

	F32 x, y;
	from_region_handle(region_handle, &x, &y);
	LLViewerRegion* regionp = LLWorld::getInstance()->getRegionFromHandle(region_handle);
	if (!regionp)
	{
		if (gAgent.getRegion())
		{
			LL_WARNS("Messaging") << "current region " << gAgent.getRegion()->getOriginGlobal() << LL_ENDL;
		}

		LL_WARNS("Messaging") << "Agent being sent to invalid home region: " 
			<< x << ":" << y 
			<< " current pos " << gAgent.getPositionGlobal()
			<< LL_ENDL;
		LLAppViewer::instance()->forceDisconnect("You were sent to an invalid region.");
		return;

	}

	LL_INFOS("Messaging") << "Changing home region to " << x << ":" << y << LL_ENDL;

	// set our upstream host the new simulator and shuffle things as
	// appropriate.
	LLVector3 shift_vector = regionp->getPosRegionFromGlobal(
		gAgent.getRegion()->getOriginGlobal());
	gAgent.setRegion(regionp);
	gObjectList.shiftObjects(shift_vector);
	gAssetStorage->setUpstream(msg->getSender());
	gCacheName->setUpstream(msg->getSender());
	gViewerThrottle.sendToSim();
	gViewerWindow->sendShapeToSim();
	// if this is an AgentMovementComplete message that happened as the result of a teleport,
	// then we need to do things like chat the URL and reset the camera.
	bool is_teleport = (gAgent.getTeleportState() & (LLAgent::TELEPORT_MOVING | LLAgent::TELEPORT_PLACE_AVATAR)); //OGPX
	llinfos << " is_teleport =" << is_teleport << llendl;

	if( is_teleport )
	{
		if (gAgent.getTeleportKeepsLookAt())
		{
			// *NOTE: the LookAt data we get from the sim here doesn't
			// seem to be useful, so get it from the camera instead
			look_at = LLViewerCamera::getInstance()->getAtAxis();
		}
		// Force the camera back onto the agent, don't animate.
		gAgent.setFocusOnAvatar(TRUE, FALSE);
		gAgent.slamLookAt(look_at);
		gAgent.updateCamera();

		gAgent.setTeleportState( LLAgent::TELEPORT_START_ARRIVAL );

		// set the appearance on teleport since the new sim does not
		// know what you look like.
		gAgent.sendAgentSetAppearance();

// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
		if ( (avatarp) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) )
// [/RLVa:KB]
//		if (avatarp)
		{
			// Chat the "back" SLURL. (DEV-4907)
			LLChat chat("Teleport completed from " + gAgent.getTeleportSourceSLURL());
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
 			LLFloaterChat::addChatHistory(chat);

			// Set the new position
			avatarp->setPositionAgent(agent_pos);
			avatarp->clearChat();
			avatarp->slamPosition();
		}
		// OGPX TODO: remove all usage of TELEPORT_PLACE_AVATAR state once Teleport UDP sequence finalized
		if ( gAgent.getTeleportState() == LLAgent::TELEPORT_PLACE_AVATAR ) // unset TP state, agent domain is done. OGPX
		{
			gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
		}

		// add teleport destination to the list of visited places
		gFloaterTeleportHistory->addPendingEntry(regionp->getName(), (S16)agent_pos.mV[VX], (S16)agent_pos.mV[VY], (S16)agent_pos.mV[VZ]);
	}
	else
	{
		// This is likely just the initial logging in phase.
		gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
	}

	if ( LLTracker::isTracking(NULL) )
	{
		// Check distance to beacon, if < 5m, remove beacon
		LLVector3d beacon_pos = LLTracker::getTrackedPositionGlobal();
		LLVector3 beacon_dir(agent_pos.mV[VX] - (F32)fmod(beacon_pos.mdV[VX], 256.0), agent_pos.mV[VY] - (F32)fmod(beacon_pos.mdV[VY], 256.0), 0);
		if (beacon_dir.magVecSquared() < 25.f)
		{
			LLTracker::stopTracking(NULL);
		}
		else if ( is_teleport && !gAgent.getTeleportKeepsLookAt() )
		{
			//look at the beacon
			LLVector3 global_agent_pos = agent_pos;
			global_agent_pos[0] += x;
			global_agent_pos[1] += y;
			look_at = (LLVector3)beacon_pos - global_agent_pos;
			look_at.normVec();
			gAgent.slamLookAt(look_at);
		}
	}

	// TODO: Put back a check for flying status! DK 12/19/05
	// Sim tells us whether the new position is off the ground
	/*
	if (teleport_flags & TELEPORT_FLAGS_IS_FLYING)
	{
		gAgent.setFlying(TRUE);
	}
	else
	{
		gAgent.setFlying(FALSE);
	}
	*/

	send_agent_update(TRUE, TRUE);

	if (gAgent.getRegion()->getBlockFly())
	{
		gAgent.setFlying(gAgent.canFly());
	}

	// force simulator to recognize busy state
	if (gAgent.getBusy())
	{
		gAgent.setBusy();
	}
	else
	{
		gAgent.clearBusy();
	}

	if (avatarp)
	{
		avatarp->mFootPlane.clearVec();
	}
	
	// send walk-vs-run status
	gAgent.sendWalkRun(gAgent.getRunning() || gAgent.getAlwaysRun());

	// If the server version has changed, display an info box and offer
	// to display the release notes, unless this is the initial log in.
	if (gLastVersionChannel == version_channel)
	{
		return;
	}

	if (!gLastVersionChannel.empty())
	{
		LLSD payload;
		payload["message"] = version_channel;
		LLNotifications::instance().add("ServerVersionChanged", LLSD(), payload);
	}

	gLastVersionChannel = version_channel;
}

void process_crossed_region(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	if((gAgent.getID() != agent_id) || (gAgent.getSessionID() != session_id))
	{
		LL_WARNS("Messaging") << "Incorrect id in process_crossed_region()"
				<< LL_ENDL;
		return;
	}
	LL_INFOS("Messaging") << "process_crossed_region()" << LL_ENDL;

	U32 sim_ip;
	msg->getIPAddrFast(_PREHASH_RegionData, _PREHASH_SimIP, sim_ip);
	U16 sim_port;
	msg->getIPPortFast(_PREHASH_RegionData, _PREHASH_SimPort, sim_port);
	LLHost sim_host(sim_ip, sim_port);
	U64 region_handle;
	msg->getU64Fast(_PREHASH_RegionData, _PREHASH_RegionHandle, region_handle);
	
	std::string seedCap;
	msg->getStringFast(_PREHASH_RegionData, _PREHASH_SeedCapability, seedCap);

	send_complete_agent_movement(sim_host);

	LLViewerRegion* regionp = LLWorld::getInstance()->addRegion(region_handle, sim_host);
	regionp->setSeedCapability(seedCap);
}



// Sends avatar and camera information to simulator.
// Sent roughly once per frame, or 20 times per second, whichever is less often

const F32 THRESHOLD_HEAD_ROT_QDOT = 0.9997f;	// ~= 2.5 degrees -- if its less than this we need to update head_rot
const F32 MAX_HEAD_ROT_QDOT = 0.99999f;			// ~= 0.5 degrees -- if its greater than this then no need to update head_rot
												// between these values we delay the updates (but no more than one second)


void send_agent_update(BOOL force_send, BOOL send_reliable)
{
	if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
	{
		// We don't care if they want to send an agent update, they're not allowed to until the simulator
		// that's the target is ready to receive them (after avatar_init_complete is received)
		return;
	}

	if(gAgent.getPhantom()) return; //Don't want to do this while phantom

	// We have already requested to log out.  Don't send agent updates.
	if(LLAppViewer::instance()->logoutRequestSent())
	{
		return;
	}

	// no region to send update to
	if(gAgent.getRegion() == NULL)
	{
		return;
	}

	const F32 TRANSLATE_THRESHOLD = 0.01f;

	// NOTA BENE: This is (intentionally?) using the small angle sine approximation to test for rotation
	//			  Plus, there is an extra 0.5 in the mix since the perpendicular between last_camera_at and getAtAxis() bisects cam_rot_change
	//			  Thus, we're actually testing against 0.2 degrees
	const F32 ROTATION_THRESHOLD = 0.1f * 2.f*F_PI/360.f;			//  Rotation thresh 0.2 deg, see note above

	const U8 DUP_MSGS = 1;				//  HACK!  number of times to repeat data on motionless agent

	//  Store data on last sent update so that if no changes, no send
	static LLVector3 last_camera_pos_agent, 
					 last_camera_at, 
					 last_camera_left,
					 last_camera_up;
	
	static LLVector3 cam_center_chg,
					 cam_rot_chg;

	static LLQuaternion last_head_rot;
	static U32 last_control_flags = 0;
	static U8 last_render_state;
	static U8 duplicate_count = 0;
	static F32 head_rot_chg = 1.0;
	static U8 last_flags;

	LLMessageSystem	*msg = gMessageSystem;
	LLVector3		camera_pos_agent;				// local to avatar's region
	U8				render_state;

	LLQuaternion body_rotation = gAgent.getFrameAgent().getQuaternion();
	LLQuaternion head_rotation = gAgent.getHeadRotation();

	camera_pos_agent = gAgent.getCameraPositionAgent();

	render_state = gAgent.getRenderState();

	U32		control_flag_change = 0;
	U8		flag_change = 0;

	cam_center_chg = last_camera_pos_agent - camera_pos_agent;
	cam_rot_chg = last_camera_at - LLViewerCamera::getInstance()->getAtAxis();

	// If a modifier key is held down, turn off
	// LBUTTON and ML_LBUTTON so that using the camera (alt-key) doesn't
	// trigger a control event.
	U32 control_flags = gAgent.getControlFlags();

	// <edit>
	if(gSavedSettings.getBOOL("Nimble"))
	{
		control_flags |= AGENT_CONTROL_FINISH_ANIM;
	}
	// </edit>

	MASK	key_mask = gKeyboard->currentMask(TRUE);
	if (key_mask & MASK_ALT || key_mask & MASK_CONTROL)
	{
		control_flags &= ~(	AGENT_CONTROL_LBUTTON_DOWN |
							AGENT_CONTROL_ML_LBUTTON_DOWN );
		control_flags |= 	AGENT_CONTROL_LBUTTON_UP |
							AGENT_CONTROL_ML_LBUTTON_UP ;
	}

	control_flag_change = last_control_flags ^ control_flags;

	U8 flags = AU_FLAGS_NONE;
	if (gAgent.isGroupTitleHidden())
	{
		flags |= AU_FLAGS_HIDETITLE;
	}

	flag_change = last_flags ^ flags;

	head_rot_chg = dot(last_head_rot, head_rotation);

	if (force_send || 
		(cam_center_chg.magVec() > TRANSLATE_THRESHOLD) || 
		(head_rot_chg < THRESHOLD_HEAD_ROT_QDOT) ||	
		(last_render_state != render_state) ||
		(cam_rot_chg.magVec() > ROTATION_THRESHOLD) ||
		control_flag_change != 0 ||
		flag_change != 0)  
	{
/*
		if (head_rot_chg < THRESHOLD_HEAD_ROT_QDOT)
		{
			//LL_INFOS("Messaging") << "head rot " << head_rotation << LL_ENDL;
			LL_INFOS("Messaging") << "head_rot_chg = " << head_rot_chg << LL_ENDL;
		}
		if (cam_rot_chg.magVec() > ROTATION_THRESHOLD) 
		{
			LL_INFOS("Messaging") << "cam rot " <<  cam_rot_chg.magVec() << LL_ENDL;
		}
		if (cam_center_chg.magVec() > TRANSLATE_THRESHOLD)
		{
			LL_INFOS("Messaging") << "cam center " << cam_center_chg.magVec() << LL_ENDL;
		}
//		if (drag_delta_chg.magVec() > TRANSLATE_THRESHOLD)
//		{
//			LL_INFOS("Messaging") << "drag delta " << drag_delta_chg.magVec() << LL_ENDL;
//		}
		if (control_flag_change)
		{
			LL_INFOS("Messaging") << "dcf = " << control_flag_change << LL_ENDL;
		}
*/

		duplicate_count = 0;
	}
	else
	{
		duplicate_count++;

		if (head_rot_chg < MAX_HEAD_ROT_QDOT  &&  duplicate_count < AGENT_UPDATES_PER_SECOND)
		{
			// The head_rotation is sent for updating things like attached guns.
			// We only trigger a new update when head_rotation deviates beyond
			// some threshold from the last update, however this can break fine
			// adjustments when trying to aim an attached gun, so what we do here
			// (where we would normally skip sending an update when nothing has changed)
			// is gradually reduce the threshold to allow a better update to 
			// eventually get sent... should update to within 0.5 degrees in less 
			// than a second.
			if (head_rot_chg < THRESHOLD_HEAD_ROT_QDOT + (MAX_HEAD_ROT_QDOT - THRESHOLD_HEAD_ROT_QDOT) * duplicate_count / AGENT_UPDATES_PER_SECOND)
			{
				duplicate_count = 0;
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	if (duplicate_count < DUP_MSGS && !gDisconnected)
	{
		// Build the message
		msg->newMessageFast(_PREHASH_AgentUpdate);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->addQuatFast(_PREHASH_BodyRotation, body_rotation);
		msg->addQuatFast(_PREHASH_HeadRotation, head_rotation);
		msg->addU8Fast(_PREHASH_State, render_state);
		msg->addU8Fast(_PREHASH_Flags, flags);

//		if (camera_pos_agent.mV[VY] > 255.f)
//		{
//			LL_INFOS("Messaging") << "Sending camera center " << camera_pos_agent << LL_ENDL;
//		}
		
		msg->addVector3Fast(_PREHASH_CameraCenter, camera_pos_agent);
		msg->addVector3Fast(_PREHASH_CameraAtAxis, LLViewerCamera::getInstance()->getAtAxis());
		msg->addVector3Fast(_PREHASH_CameraLeftAxis, LLViewerCamera::getInstance()->getLeftAxis());
		msg->addVector3Fast(_PREHASH_CameraUpAxis, LLViewerCamera::getInstance()->getUpAxis());
		msg->addF32Fast(_PREHASH_Far, gAgent.mDrawDistance);
		
		msg->addU32Fast(_PREHASH_ControlFlags, control_flags);

		if (gDebugClicks)
		{
			if (control_flags & AGENT_CONTROL_LBUTTON_DOWN)
			{
				LL_INFOS("Messaging") << "AgentUpdate left button down" << LL_ENDL;
			}

			if (control_flags & AGENT_CONTROL_LBUTTON_UP)
			{
				LL_INFOS("Messaging") << "AgentUpdate left button up" << LL_ENDL;
			}
		}

		gAgent.enableControlFlagReset();

		if (!send_reliable)
		{
			gAgent.sendMessage();
		}
		else
		{
			gAgent.sendReliableMessage();
		}

//		LL_DEBUGS("Messaging") << "agent " << avatar_pos_agent << " cam " << camera_pos_agent << LL_ENDL;

		// Copy the old data 
		last_head_rot = head_rotation;
		last_render_state = render_state;
		last_camera_pos_agent = camera_pos_agent;
		last_camera_at = LLViewerCamera::getInstance()->getAtAxis();
		last_camera_left = LLViewerCamera::getInstance()->getLeftAxis();
		last_camera_up = LLViewerCamera::getInstance()->getUpAxis();
		last_control_flags = control_flags;
		last_flags = flags;
	}
}



// *TODO: Remove this dependency, or figure out a better way to handle
// this hack.
extern U32 gObjectBits;

void process_object_update(LLMessageSystem *mesgsys, void **user_data)
{	
	LLMemType mt(LLMemType::MTYPE_OBJECT);
	// Update the data counters
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectBits += mesgsys->getReceiveCompressedSize() * 8;
	}
	else
	{
		gObjectBits += mesgsys->getReceiveSize() * 8;
	}

	// Update the object...
	gObjectList.processObjectUpdate(mesgsys, user_data, OUT_FULL);
	stop_glerror();
}

void process_compressed_object_update(LLMessageSystem *mesgsys, void **user_data)
{
	LLMemType mt(LLMemType::MTYPE_OBJECT);
	// Update the data counters
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectBits += mesgsys->getReceiveCompressedSize() * 8;
	}
	else
	{
		gObjectBits += mesgsys->getReceiveSize() * 8;
	}

	// Update the object...
	gObjectList.processCompressedObjectUpdate(mesgsys, user_data, OUT_FULL_COMPRESSED);
	stop_glerror();
}

void process_cached_object_update(LLMessageSystem *mesgsys, void **user_data)
{
	LLMemType mt(LLMemType::MTYPE_OBJECT);
	// Update the data counters
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectBits += mesgsys->getReceiveCompressedSize() * 8;
	}
	else
	{
		gObjectBits += mesgsys->getReceiveSize() * 8;
	}

	// Update the object...
	gObjectList.processCachedObjectUpdate(mesgsys, user_data, OUT_FULL_CACHED);
	stop_glerror();
}


void process_terse_object_update_improved(LLMessageSystem *mesgsys, void **user_data)
{
	LLMemType mt(LLMemType::MTYPE_OBJECT);
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectBits += mesgsys->getReceiveCompressedSize() * 8;
	}
	else
	{
		gObjectBits += mesgsys->getReceiveSize() * 8;
	}

	gObjectList.processCompressedObjectUpdate(mesgsys, user_data, OUT_TERSE_IMPROVED);
}



void process_kill_object(LLMessageSystem *mesgsys, void **user_data)
{
	LLFastTimer t(LLFastTimer::FTM_PROCESS_OBJECTS);

	LLUUID		id;
	U32			local_id;
	S32			i;
	S32			num_objects;

	num_objects = mesgsys->getNumberOfBlocksFast(_PREHASH_ObjectData);

	for (i = 0; i < num_objects; i++)
	{
		mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);

		LLViewerObjectList::getUUIDFromLocal(id,
											local_id,
											gMessageSystem->getSenderIP(),
											gMessageSystem->getSenderPort());
		if (id == LLUUID::null)
		{
			LL_DEBUGS("Messaging") << "Unknown kill for local " << local_id << LL_ENDL;
			gObjectList.mNumUnknownKills++;
			continue;
		}
		else
		{
			LL_DEBUGS("Messaging") << "Kill message for local " << local_id << LL_ENDL;
		}

		LLSelectMgr::getInstance()->removeObjectFromSelections(id);

		// ...don't kill the avatar
		if (!(id == gAgentID))
		{
			LLViewerObject *objectp = gObjectList.findObject(id);
			if (objectp)
			{
				// Display green bubble on kill
				if ( gShowObjectUpdates )
				{
					LLViewerObject* newobject;
					newobject = gObjectList.createObjectViewer(LL_PCODE_LEGACY_TEXT_BUBBLE, objectp->getRegion());

					LLVOTextBubble* bubble = (LLVOTextBubble*) newobject;

					bubble->mColor.setVec(0.f, 1.f, 0.f, 1.f);
					bubble->setScale( 2.0f * bubble->getScale() );
					bubble->setPositionGlobal(objectp->getPositionGlobal());
					gPipeline.addObject(bubble);
				}

				// Do the kill
				gObjectList.killObject(objectp);
			}
			else
			{
				LL_WARNS("Messaging") << "Object in UUID lookup, but not on object list in kill!" << LL_ENDL;
				gObjectList.mNumUnknownKills++;
			}
		}
	}
}

void process_time_synch(LLMessageSystem *mesgsys, void **user_data)
{
	LLVector3 sun_direction;
	LLVector3 sun_ang_velocity;
	F32 phase;
	U64	space_time_usec;

    U32 seconds_per_day;
    U32 seconds_per_year;

	// "SimulatorViewerTimeMessage"
	mesgsys->getU64Fast(_PREHASH_TimeInfo, _PREHASH_UsecSinceStart, space_time_usec);
	mesgsys->getU32Fast(_PREHASH_TimeInfo, _PREHASH_SecPerDay, seconds_per_day);
	mesgsys->getU32Fast(_PREHASH_TimeInfo, _PREHASH_SecPerYear, seconds_per_year);

	// This should eventually be moved to an "UpdateHeavenlyBodies" message
	mesgsys->getF32Fast(_PREHASH_TimeInfo, _PREHASH_SunPhase, phase);
	mesgsys->getVector3Fast(_PREHASH_TimeInfo, _PREHASH_SunDirection, sun_direction);
	mesgsys->getVector3Fast(_PREHASH_TimeInfo, _PREHASH_SunAngVelocity, sun_ang_velocity);

	LLWorld::getInstance()->setSpaceTimeUSec(space_time_usec);

	//LL_DEBUGS("Messaging") << "time_synch() - " << sun_direction << ", " << sun_ang_velocity
	//		 << ", " << phase << LL_ENDL;

	gSky.setSunPhase(phase);
	gSky.setSunTargetDirection(sun_direction, sun_ang_velocity);
	if (!gNoRender && !(gSavedSettings.getBOOL("SkyOverrideSimSunPosition") || gSky.getOverrideSun()))
	{
		gSky.setSunDirection(sun_direction, sun_ang_velocity);
	}
}

void process_sound_trigger(LLMessageSystem *msg, void **)
{
	if (!gAudiop) return;

	U64		region_handle = 0;
	F32		gain = 0;
	LLUUID	sound_id;
	LLUUID	owner_id;
	LLUUID	object_id;
	LLUUID	parent_id;
	LLVector3	pos_local;

	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_OwnerID, owner_id);
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_ObjectID, object_id);
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_ParentID, parent_id);
	msg->getU64Fast(_PREHASH_SoundData, _PREHASH_Handle, region_handle);
	msg->getVector3Fast(_PREHASH_SoundData, _PREHASH_Position, pos_local);
	msg->getF32Fast(_PREHASH_SoundData, _PREHASH_Gain, gain);

	// adjust sound location to true global coords
	LLVector3d	pos_global = from_region_handle(region_handle);
	pos_global.mdV[VX] += pos_local.mV[VX];
	pos_global.mdV[VY] += pos_local.mV[VY];
	pos_global.mdV[VZ] += pos_local.mV[VZ];

	// Don't play a trigger sound if you can't hear it due
	// to parcel "local audio only" settings.
	if (!LLViewerParcelMgr::getInstance()->canHearSound(pos_global)) return;

	// Don't play sounds triggered by someone you muted.
	if (LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagObjectSounds)) return;
	
	// Don't play sounds from an object you muted
	if (LLMuteList::getInstance()->isMuted(object_id)) return;

	// Don't play sounds from an object whose parent you muted
	if (parent_id.notNull()
		&& LLMuteList::getInstance()->isMuted(parent_id))
	{
		return;
	}

	// Don't play sounds from a region with maturity above current agent maturity
	if( !gAgent.canAccessMaturityInRegion( region_handle ) )
	{
		return;
	}
		
	// <edit>
	//gAudiop->triggerSound(sound_id, owner_id, gain, LLAudioEngine::AUDIO_TYPE_SFX, pos_global);
	gAudiop->triggerSound(sound_id, owner_id, gain, LLAudioEngine::AUDIO_TYPE_SFX, pos_global, object_id);
	// </edit>
}

void process_preload_sound(LLMessageSystem *msg, void **user_data)
{
	if (!gAudiop)
	{
		return;
	}

	LLUUID sound_id;
	LLUUID object_id;
	LLUUID owner_id;

	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_OwnerID, owner_id);

	LLViewerObject *objectp = gObjectList.findObject(object_id);
	if (!objectp) return;

	if (LLMuteList::getInstance()->isMuted(object_id)) return;
	if (LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagObjectSounds)) return;
	
	LLAudioSource *sourcep = objectp->getAudioSource(owner_id);
	if (!sourcep) return;
	
	LLAudioData *datap = gAudiop->getAudioData(sound_id);

	// Note that I don't actually do any loading of the
	// audio data into a buffer at this point, as it won't actually
	// help us out.

	// Don't play sounds from a region with maturity above current agent maturity
	LLVector3d pos_global = objectp->getPositionGlobal();
	if (gAgent.canAccessMaturityAtGlobal(pos_global))
	{
		// Add audioData starts a transfer internally.
		sourcep->addAudioData(datap, FALSE);
	}
}

void process_attached_sound(LLMessageSystem *msg, void **user_data)
{
	F32 gain = 0;
	LLUUID sound_id;
	LLUUID object_id;
	LLUUID owner_id;
	U8 flags;

	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_OwnerID, owner_id);
	msg->getF32Fast(_PREHASH_DataBlock, _PREHASH_Gain, gain);
	msg->getU8Fast(_PREHASH_DataBlock, _PREHASH_Flags, flags);

	LLViewerObject *objectp = gObjectList.findObject(object_id);
	if (!objectp)
	{
		// we don't know about this object, just bail
		return;
	}
	
	if (LLMuteList::getInstance()->isMuted(object_id)) return;
	
	if (LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagObjectSounds)) return;

	
	// Don't play sounds from a region with maturity above current agent maturity
	LLVector3d pos = objectp->getPositionGlobal();
	if( !gAgent.canAccessMaturityAtGlobal(pos) )
	{
		return;
	}
	
	objectp->setAttachedSound(sound_id, owner_id, gain, flags);
}


void process_attached_sound_gain_change(LLMessageSystem *mesgsys, void **user_data)
{
	F32 gain = 0;
	LLUUID object_guid;
	LLViewerObject *objectp = NULL;

	mesgsys->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_guid);

	if (!((objectp = gObjectList.findObject(object_guid))))
	{
		// we don't know about this object, just bail
		return;
	}

 	mesgsys->getF32Fast(_PREHASH_DataBlock, _PREHASH_Gain, gain);

	objectp->adjustAudioGain(gain);
}


void process_health_message(LLMessageSystem *mesgsys, void **user_data)
{
	F32 health;

	mesgsys->getF32Fast(_PREHASH_HealthData, _PREHASH_Health, health);

	if (gStatusBar)
	{
		gStatusBar->setHealth((S32)health);
	}
}


void process_sim_stats(LLMessageSystem *msg, void **user_data)
{	
	S32 count = msg->getNumberOfBlocks("Stat");
	for (S32 i = 0; i < count; ++i)
	{
		U32 stat_id;
		F32 stat_value;
		msg->getU32("Stat", "StatID", stat_id, i);
		msg->getF32("Stat", "StatValue", stat_value, i);
		switch (stat_id)
		{
		case LL_SIM_STAT_TIME_DILATION:
			LLViewerStats::getInstance()->mSimTimeDilation.addValue(stat_value);
			break;
		case LL_SIM_STAT_FPS:
			LLViewerStats::getInstance()->mSimFPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_PHYSFPS:
			LLViewerStats::getInstance()->mSimPhysicsFPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_AGENTUPS:
			LLViewerStats::getInstance()->mSimAgentUPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_FRAMEMS:
			LLViewerStats::getInstance()->mSimFrameMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_NETMS:
			LLViewerStats::getInstance()->mSimNetMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMOTHERMS:
			LLViewerStats::getInstance()->mSimSimOtherMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMPHYSICSMS:
			LLViewerStats::getInstance()->mSimSimPhysicsMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_AGENTMS:
			LLViewerStats::getInstance()->mSimAgentMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_IMAGESMS:
			LLViewerStats::getInstance()->mSimImagesMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SCRIPTMS:
			LLViewerStats::getInstance()->mSimScriptMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_NUMTASKS:
			LLViewerStats::getInstance()->mSimObjects.addValue(stat_value);
			break;
		case LL_SIM_STAT_NUMTASKSACTIVE:
			LLViewerStats::getInstance()->mSimActiveObjects.addValue(stat_value);
			break;
		case LL_SIM_STAT_NUMAGENTMAIN:
			LLViewerStats::getInstance()->mSimMainAgents.addValue(stat_value);
			break;
		case LL_SIM_STAT_NUMAGENTCHILD:
			LLViewerStats::getInstance()->mSimChildAgents.addValue(stat_value);
			break;
		case LL_SIM_STAT_NUMSCRIPTSACTIVE:
			if (gSavedSettings.getBOOL("AscentDisplayTotalScriptJumps"))
			{
				if(abs(stat_value-gSavedSettings.getF32("Ascentnumscripts"))>gSavedSettings.getF32("Ascentnumscriptdiff"))
				{
					LLChat chat;
					std::stringstream os;
					os << (U32)gSavedSettings.getF32("Ascentnumscripts");
					std::stringstream ns;
					ns << (U32)stat_value;
					std::stringstream diff;
					S32 tdiff = (stat_value-gSavedSettings.getF32("Ascentnumscripts"));
					diff << tdiff;
					std::string change_type = "";
					if (tdiff > 0) change_type = "+";
					chat.mText = "Total scripts jumped from " + os.str() + " to " + ns.str() + " ("+change_type+diff.str()+")";
					LLFloaterChat::addChat(chat, FALSE,FALSE);
				}
				gSavedSettings.setF32("Ascentnumscripts",stat_value);
			}
			LLViewerStats::getInstance()->mSimActiveScripts.addValue(stat_value);
			break;
		case LL_SIM_STAT_SCRIPT_EPS:
			LLViewerStats::getInstance()->mSimScriptEPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_INPPS:
			LLViewerStats::getInstance()->mSimInPPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_OUTPPS:
			LLViewerStats::getInstance()->mSimOutPPS.addValue(stat_value);
			break;
		case LL_SIM_STAT_PENDING_DOWNLOADS:
			LLViewerStats::getInstance()->mSimPendingDownloads.addValue(stat_value);
			break;
		case LL_SIM_STAT_PENDING_UPLOADS:
			LLViewerStats::getInstance()->mSimPendingUploads.addValue(stat_value);
			break;
		case LL_SIM_STAT_PENDING_LOCAL_UPLOADS:
			LLViewerStats::getInstance()->mSimPendingLocalUploads.addValue(stat_value);
			break;
		case LL_SIM_STAT_TOTAL_UNACKED_BYTES:
			LLViewerStats::getInstance()->mSimTotalUnackedBytes.addValue(stat_value / 1024.f);
			break;
		case LL_SIM_STAT_PHYSICS_PINNED_TASKS:
			LLViewerStats::getInstance()->mPhysicsPinnedTasks.addValue(stat_value);
			break;
		case LL_SIM_STAT_PHYSICS_LOD_TASKS:
			LLViewerStats::getInstance()->mPhysicsLODTasks.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMPHYSICSSTEPMS:
			LLViewerStats::getInstance()->mSimSimPhysicsStepMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMPHYSICSSHAPEMS:
			LLViewerStats::getInstance()->mSimSimPhysicsShapeUpdateMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMPHYSICSOTHERMS:
			LLViewerStats::getInstance()->mSimSimPhysicsOtherMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMPHYSICSMEMORY:
			LLViewerStats::getInstance()->mPhysicsMemoryAllocated.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMSPARETIME:
			LLViewerStats::getInstance()->mSimSpareMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_SIMSLEEPTIME:
			LLViewerStats::getInstance()->mSimSleepMsec.addValue(stat_value);
			break;
		case LL_SIM_STAT_IOPUMPTIME:
			LLViewerStats::getInstance()->mSimPumpIOMsec.addValue(stat_value);
			break;
		default:
			// Used to be a commented out warning.
 			LL_DEBUGS("Messaging") << "Unknown stat id" << stat_id << LL_ENDL;
		  break;
		}
	}

	/*
	msg->getF32Fast(_PREHASH_Statistics, _PREHASH_PhysicsTimeDilation, time_dilation);
	LLViewerStats::getInstance()->mSimTDStat.addValue(time_dilation);

	// Process information
	//	{	CpuUsage			F32				}
	//	{	SimMemTotal			F32				}
	//	{	SimMemRSS			F32				}
	//	{	ProcessUptime		F32				}
	F32 cpu_usage;
	F32 sim_mem_total;
	F32 sim_mem_rss;
	F32 process_uptime;
	msg->getF32Fast(_PREHASH_Statistics, _PREHASH_CpuUsage, cpu_usage);
	msg->getF32Fast(_PREHASH_Statistics, _PREHASH_SimMemTotal, sim_mem_total);
	msg->getF32Fast(_PREHASH_Statistics, _PREHASH_SimMemRSS, sim_mem_rss);
	msg->getF32Fast(_PREHASH_Statistics, _PREHASH_ProcessUptime, process_uptime);
	LLViewerStats::getInstance()->mSimCPUUsageStat.addValue(cpu_usage);
	LLViewerStats::getInstance()->mSimMemTotalStat.addValue(sim_mem_total);
	LLViewerStats::getInstance()->mSimMemRSSStat.addValue(sim_mem_rss);
	*/

	//
	// Various hacks that aren't statistics, but are being handled here.
	//
	U32 max_tasks_per_region;
	U32 region_flags;
	msg->getU32("Region", "ObjectCapacity", max_tasks_per_region);
	msg->getU32("Region", "RegionFlags", region_flags);

	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		BOOL was_flying = gAgent.getFlying();
		regionp->setRegionFlags(region_flags);
		regionp->setMaxTasks(max_tasks_per_region);
		// HACK: This makes agents drop from the sky if the region is 
		// set to no fly while people are still in the sim.
		if (was_flying && regionp->getBlockFly())
		{
			gAgent.setFlying(gAgent.canFly());
		}
	}
}



void process_avatar_animation(LLMessageSystem *mesgsys, void **user_data)
{
	LLUUID	animation_id;
	LLUUID	uuid;
	S32		anim_sequence_id;
	
	mesgsys->getUUIDFast(_PREHASH_Sender, _PREHASH_ID, uuid);

	//clear animation flags
	LLVOAvatar* avatarp = gObjectList.findAvatar(uuid);

	if (!avatarp)
	{
		// no agent by this ID...error?
		LL_WARNS("Messaging") << "Received animation state for unknown avatar" << uuid << LL_ENDL;
		return;
	}

	S32 num_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_AnimationList);
	S32 num_source_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_AnimationSourceList);

	avatarp->mSignaledAnimations.clear();
	
	if (avatarp->isSelf())
	{
		LLUUID object_id;

		for( S32 i = 0; i < num_blocks; i++ )
		{
			mesgsys->getUUIDFast(_PREHASH_AnimationList, _PREHASH_AnimID, animation_id, i);
			mesgsys->getS32Fast(_PREHASH_AnimationList, _PREHASH_AnimSequenceID, anim_sequence_id, i);

			LL_DEBUGS("Messaging") << "Anim sequence ID: " << anim_sequence_id << LL_ENDL;

			avatarp->mSignaledAnimations[animation_id] = anim_sequence_id;

			if (i < num_source_blocks)
			{
				mesgsys->getUUIDFast(_PREHASH_AnimationSourceList, _PREHASH_ObjectID, object_id, i);
			
				LLViewerObject* object = gObjectList.findObject(object_id);
				if (object)
				{
					object->mFlags |= FLAGS_ANIM_SOURCE;

					BOOL anim_found = FALSE;
					LLVOAvatar::AnimSourceIterator anim_it = avatarp->mAnimationSources.find(object_id);
					for (;anim_it != avatarp->mAnimationSources.end(); ++anim_it)
					{
						if (anim_it->second == animation_id)
						{
							anim_found = TRUE;
							break;
						}
					}

					if (!anim_found)
					{
						avatarp->mAnimationSources.insert(LLVOAvatar::AnimationSourceMap::value_type(object_id, animation_id));
					}
				}
			}
		}
	}
	else
	{
		for( S32 i = 0; i < num_blocks; i++ )
		{
			mesgsys->getUUIDFast(_PREHASH_AnimationList, _PREHASH_AnimID, animation_id, i);
			mesgsys->getS32Fast(_PREHASH_AnimationList, _PREHASH_AnimSequenceID, anim_sequence_id, i);
			avatarp->mSignaledAnimations[animation_id] = anim_sequence_id;
		}
	}

	if (num_blocks)
	{
		avatarp->processAnimationStateChanges();
	}
}

void process_avatar_appearance(LLMessageSystem *mesgsys, void **user_data)
{
	LLUUID uuid;
	mesgsys->getUUIDFast(_PREHASH_Sender, _PREHASH_ID, uuid);

	LLVOAvatar* avatarp = gObjectList.findAvatar(uuid);
	if( avatarp )
	{
		avatarp->processAvatarAppearance( mesgsys );
	}
	else
	{
		LL_WARNS("Messaging") << "avatar_appearance sent for unknown avatar " << uuid << LL_ENDL;
	}
}

void process_camera_constraint(LLMessageSystem *mesgsys, void **user_data)
{
	LLVector4 cameraCollidePlane;
	mesgsys->getVector4Fast(_PREHASH_CameraCollidePlane, _PREHASH_Plane, cameraCollidePlane);

	gAgent.setCameraCollidePlane(cameraCollidePlane);
}

void near_sit_object(BOOL success, void *data)
{
	if (success)
	{
		// Send message to sit on object
		gMessageSystem->newMessageFast(_PREHASH_AgentSit);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gAgent.sendReliableMessage();
	}
}

void process_avatar_sit_response(LLMessageSystem *mesgsys, void **user_data)
{
	LLVector3 sitPosition;
	LLQuaternion sitRotation;
	LLUUID sitObjectID;
	BOOL use_autopilot;
	mesgsys->getUUIDFast(_PREHASH_SitObject, _PREHASH_ID, sitObjectID);
	mesgsys->getBOOLFast(_PREHASH_SitTransform, _PREHASH_AutoPilot, use_autopilot);
	mesgsys->getVector3Fast(_PREHASH_SitTransform, _PREHASH_SitPosition, sitPosition);
	mesgsys->getQuatFast(_PREHASH_SitTransform, _PREHASH_SitRotation, sitRotation);
	LLVector3 camera_eye;
	mesgsys->getVector3Fast(_PREHASH_SitTransform, _PREHASH_CameraEyeOffset, camera_eye);
	LLVector3 camera_at;
	mesgsys->getVector3Fast(_PREHASH_SitTransform, _PREHASH_CameraAtOffset, camera_at);
	BOOL force_mouselook;
	mesgsys->getBOOLFast(_PREHASH_SitTransform, _PREHASH_ForceMouselook, force_mouselook);

	LLVOAvatar* avatar = gAgent.getAvatarObject();

	if (avatar && dist_vec_squared(camera_eye, camera_at) > 0.0001f)
	{
		gAgent.setSitCamera(sitObjectID, camera_eye, camera_at);
	}
	
	gAgent.mForceMouselook = force_mouselook;

	LLViewerObject* object = gObjectList.findObject(sitObjectID);
	if (object)
	{
		LLVector3 sit_spot = object->getPositionAgent() + (sitPosition * object->getRotation());
		if (!use_autopilot || (avatar && avatar->mIsSitting && avatar->getRoot() == object->getRoot()))
		{
			//we're already sitting on this object, so don't autopilot
		}
		else
		{
			gAgent.startAutoPilotGlobal(gAgent.getPosGlobalFromAgent(sit_spot), "Sit", &sitRotation, near_sit_object, NULL, 0.5f);
		}
	}
	else
	{
		LL_WARNS("Messaging") << "Received sit approval for unknown object " << sitObjectID << LL_ENDL;
	}
}

void process_clear_follow_cam_properties(LLMessageSystem *mesgsys, void **user_data)
{
	LLUUID		source_id;

	mesgsys->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, source_id);

	LLFollowCamMgr::removeFollowCamParams(source_id);
}

void process_set_follow_cam_properties(LLMessageSystem *mesgsys, void **user_data)
{
	S32			type;
	F32			value;
	bool		settingPosition = false;
	bool		settingFocus	= false;
	bool		settingFocusOffset = false;
	LLVector3	position;
	LLVector3	focus;
	LLVector3	focus_offset;

	LLUUID		source_id;

	mesgsys->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, source_id);

	LLViewerObject* objectp = gObjectList.findObject(source_id);
	if (objectp)
	{
		objectp->mFlags |= FLAGS_CAMERA_SOURCE;
	}

	S32 num_objects = mesgsys->getNumberOfBlocks("CameraProperty");
	for (S32 block_index = 0; block_index < num_objects; block_index++)
	{
		mesgsys->getS32("CameraProperty", "Type", type, block_index);
		mesgsys->getF32("CameraProperty", "Value", value, block_index);
		switch(type)
		{
		case FOLLOWCAM_PITCH:
			LLFollowCamMgr::setPitch(source_id, value);
			break;
		case FOLLOWCAM_FOCUS_OFFSET_X:
			focus_offset.mV[VX] = value;
			settingFocusOffset = true;
			break;
		case FOLLOWCAM_FOCUS_OFFSET_Y:
			focus_offset.mV[VY] = value;
			settingFocusOffset = true;
			break;
		case FOLLOWCAM_FOCUS_OFFSET_Z:
			focus_offset.mV[VZ] = value;
			settingFocusOffset = true;
			break;
		case FOLLOWCAM_POSITION_LAG:
			LLFollowCamMgr::setPositionLag(source_id, value);
			break;
		case FOLLOWCAM_FOCUS_LAG:
			LLFollowCamMgr::setFocusLag(source_id, value);
			break;
		case FOLLOWCAM_DISTANCE:
			LLFollowCamMgr::setDistance(source_id, value);
			break;
		case FOLLOWCAM_BEHINDNESS_ANGLE:
			LLFollowCamMgr::setBehindnessAngle(source_id, value);
			break;
		case FOLLOWCAM_BEHINDNESS_LAG:
			LLFollowCamMgr::setBehindnessLag(source_id, value);
			break;
		case FOLLOWCAM_POSITION_THRESHOLD:
			LLFollowCamMgr::setPositionThreshold(source_id, value);
			break;
		case FOLLOWCAM_FOCUS_THRESHOLD:
			LLFollowCamMgr::setFocusThreshold(source_id, value);
			break;
		case FOLLOWCAM_ACTIVE:
			//if 1, set using followcam,. 
			LLFollowCamMgr::setCameraActive(source_id, value != 0.f);
			break;
		case FOLLOWCAM_POSITION_X:
			settingPosition = true;
			position.mV[ 0 ] = value;
			break;
		case FOLLOWCAM_POSITION_Y:
			settingPosition = true;
			position.mV[ 1 ] = value;
			break;
		case FOLLOWCAM_POSITION_Z:
			settingPosition = true;
			position.mV[ 2 ] = value;
			break;
		case FOLLOWCAM_FOCUS_X:
			settingFocus = true;
			focus.mV[ 0 ] = value;
			break;
		case FOLLOWCAM_FOCUS_Y:
			settingFocus = true;
			focus.mV[ 1 ] = value;
			break;
		case FOLLOWCAM_FOCUS_Z:
			settingFocus = true;
			focus.mV[ 2 ] = value;
			break;
		case FOLLOWCAM_POSITION_LOCKED:
			LLFollowCamMgr::setPositionLocked(source_id, value != 0.f);
			break;
		case FOLLOWCAM_FOCUS_LOCKED:
			LLFollowCamMgr::setFocusLocked(source_id, value != 0.f);
			break;

		default:
			break;
		}
	}

	if ( settingPosition )
	{
		LLFollowCamMgr::setPosition(source_id, position);
	}
	if ( settingFocus )
	{
		LLFollowCamMgr::setFocus(source_id, focus);
	}
	if ( settingFocusOffset )
	{
		LLFollowCamMgr::setFocusOffset(source_id, focus_offset);
	}
}
//end Ventrella 


// Culled from newsim lltask.cpp
void process_name_value(LLMessageSystem *mesgsys, void **user_data)
{
	std::string	temp_str;
	LLUUID	id;
	S32		i, num_blocks;

	mesgsys->getUUIDFast(_PREHASH_TaskData, _PREHASH_ID, id);

	LLViewerObject* object = gObjectList.findObject(id);

	if (object)
	{
		num_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_NameValueData);
		for (i = 0; i < num_blocks; i++)
		{
			mesgsys->getStringFast(_PREHASH_NameValueData, _PREHASH_NVPair, temp_str, i);
			LL_INFOS("Messaging") << "Added to object Name Value: " << temp_str << LL_ENDL;
			object->addNVPair(temp_str);
		}
	}
	else
	{
		LL_INFOS("Messaging") << "Can't find object " << id << " to add name value pair" << LL_ENDL;
	}
}

void process_remove_name_value(LLMessageSystem *mesgsys, void **user_data)
{
	std::string	temp_str;
	LLUUID	id;
	S32		i, num_blocks;

	mesgsys->getUUIDFast(_PREHASH_TaskData, _PREHASH_ID, id);

	LLViewerObject* object = gObjectList.findObject(id);

	if (object)
	{
		num_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_NameValueData);
		for (i = 0; i < num_blocks; i++)
		{
			mesgsys->getStringFast(_PREHASH_NameValueData, _PREHASH_NVPair, temp_str, i);
			LL_INFOS("Messaging") << "Removed from object Name Value: " << temp_str << LL_ENDL;
			object->removeNVPair(temp_str);
		}
	}
	else
	{
		LL_INFOS("Messaging") << "Can't find object " << id << " to remove name value pair" << LL_ENDL;
	}
}

void process_kick_user(LLMessageSystem *msg, void** /*user_data*/)
{
	std::string message;

	msg->getStringFast(_PREHASH_UserInfo, _PREHASH_Reason, message);

	LLAppViewer::instance()->forceDisconnect(message);
}


/*
void process_user_list_reply(LLMessageSystem *msg, void **user_data)
{
	LLUserList::processUserListReply(msg, user_data);
	return;
	char	firstname[MAX_STRING+1];
	char	lastname[MAX_STRING+1];
	U8		status;
	S32		user_count;

	user_count = msg->getNumberOfBlocks("UserBlock");

	for (S32 i = 0; i < user_count; i++)
	{
		msg->getData("UserBlock", i, "FirstName", firstname);
		msg->getData("UserBlock", i, "LastName", lastname);
		msg->getData("UserBlock", i, "Status", &status);

		if (status & 0x01)
		{
			dialog_friends_add_friend(buffer, TRUE);
		}
		else
		{
			dialog_friends_add_friend(buffer, FALSE);
		}
	}

	dialog_friends_done_adding();
}
*/

// this is not handled in processUpdateMessage
/*
void process_time_dilation(LLMessageSystem *msg, void **user_data)
{
	// get the time_dilation
	U16 foo;
	msg->getData("TimeDilation", "TimeDilation", &foo);
	F32 time_dilation = ((F32) foo) / 65535.f;

	// get the pointer to the right region
	U32 ip = msg->getSenderIP();
	U32 port = msg->getSenderPort();
	LLViewerRegion *regionp = LLWorld::getInstance()->getRegion(ip, port);
	if (regionp)
	{
		regionp->setTimeDilation(time_dilation);
	}
}
*/



void process_money_balance_reply( LLMessageSystem* msg, void** )
{
	S32 balance = 0;
	S32 credit = 0;
	S32 committed = 0;
	std::string desc;

	msg->getS32("MoneyData", "MoneyBalance", balance);
	msg->getS32("MoneyData", "SquareMetersCredit", credit);
	msg->getS32("MoneyData", "SquareMetersCommitted", committed);
	msg->getStringFast(_PREHASH_MoneyData, _PREHASH_Description, desc);
	LL_INFOS("Messaging") << "L$, credit, committed: " << balance << " " << credit << " "
			<< committed << LL_ENDL;

	if (gStatusBar)
	{
		S32 old_balance = gStatusBar->getBalance();

		// This is an update, not the first transmission of balance
		if (old_balance != 0)
		{
			// this is actually an update
			if (balance > old_balance)
			{
				LLFirstUse::useBalanceIncrease(balance - old_balance);
			}
			else if (balance < old_balance)
			{
				LLFirstUse::useBalanceDecrease(balance - old_balance);
			}
		}

		gStatusBar->setBalance(balance);
		gStatusBar->setLandCredit(credit);
		gStatusBar->setLandCommitted(committed);
	}

	LLUUID tid;
	msg->getUUID("MoneyData", "TransactionID", tid);
	static std::deque<LLUUID> recent;
	if(!desc.empty() && gSavedSettings.getBOOL("NotifyMoneyChange")
	   && (std::find(recent.rbegin(), recent.rend(), tid) == recent.rend()))
	{
		// Make the user confirm the transaction, since they might
		// have missed something during an event.
		// *TODO:translate
		LLSD args;
		args["MESSAGE"] = desc;
		LLNotifications::instance().add("SystemMessage", args);

		// Once the 'recent' container gets large enough, chop some
		// off the beginning.
		const U32 MAX_LOOKBACK = 30;
		const S32 POP_FRONT_SIZE = 12;
		if(recent.size() > MAX_LOOKBACK)
		{
			LL_DEBUGS("Messaging") << "Removing oldest transaction records" << LL_ENDL;
			recent.erase(recent.begin(), recent.begin() + POP_FRONT_SIZE);
		}
		//LL_DEBUGS("Messaging") << "Pushing back transaction " << tid << LL_ENDL;
		recent.push_back(tid);
	}
}

bool handle_special_notification_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	
	if (0 == option)
	{
		// set the preference to the maturity of the region we're calling
		int preferredMaturity = notification["payload"]["_region_access"].asInteger();
		gSavedSettings.setU32("PreferredMaturity", preferredMaturity);
		gAgent.sendMaturityPreferenceToServer(preferredMaturity);

	}
	
	return false;
}

// some of the server notifications need special handling. This is where we do that.
bool handle_special_notification(std::string notificationID, LLSD& llsdBlock)
{
	int regionAccess = llsdBlock["_region_access"].asInteger();
	llsdBlock["REGIONMATURITY"] = LLViewerRegion::accessToString(regionAccess);
	
	// we're going to throw the LLSD in there in case anyone ever wants to use it
	LLNotifications::instance().add(notificationID+"_Notify", llsdBlock);
	
	if (regionAccess == SIM_ACCESS_MATURE)
	{
		if (gAgent.isTeen())
		{
			LLNotifications::instance().add(notificationID+"_KB", llsdBlock);
			return true;
		}
		else if (gAgent.prefersPG())
		{
			LLNotifications::instance().add(notificationID+"_Change", llsdBlock, llsdBlock, handle_special_notification_callback);
			return true;
		}
	}
	else if (regionAccess == SIM_ACCESS_ADULT)
	{
		if (!gAgent.isAdult())
		{
			LLNotifications::instance().add(notificationID+"_KB", llsdBlock);
			return true;
		}
		else if (gAgent.prefersPG() || gAgent.prefersMature())
		{
			LLNotifications::instance().add(notificationID+"_Change", llsdBlock, llsdBlock, handle_special_notification_callback);
			return true;
		}
	}
	return false;
}

bool attempt_standard_notification(LLMessageSystem* msgsystem)
{
	// if we have additional alert data
	if (msgsystem->getNumberOfBlocksFast(_PREHASH_AlertInfo) > 0)
	{
		// notification was specified using the new mechanism, so we can just handle it here
		std::string notificationID;
		std::string llsdRaw;
		LLSD llsdBlock;
		msgsystem->getStringFast(_PREHASH_AlertInfo, _PREHASH_Message, notificationID);
		msgsystem->getStringFast(_PREHASH_AlertInfo, _PREHASH_ExtraParams, llsdRaw);
		if (llsdRaw.length())
		{
			std::istringstream llsdData(llsdRaw);
			if (!LLSDSerialize::deserialize(llsdBlock, llsdData, llsdRaw.length()))
			{
				llwarns << "attempt_standard_notification: Attempted to read notification parameter data into LLSD but failed:" << llsdRaw << llendl;
			}
		}
		
		if (
			(notificationID == "RegionEntryAccessBlocked") ||
			(notificationID == "LandClaimAccessBlocked") ||
			(notificationID == "LandBuyAccessBlocked")
		   )
		{
			/*---------------------------------------------------------------------
			 (Commented so a grep will find the notification strings, since
			 we construct them on the fly; if you add additional notifications,
			 please update the comment.)
			 
			 Could throw any of the following notifications:
			 
				RegionEntryAccessBlocked
				RegionEntryAccessBlocked_Notify
				RegionEntryAccessBlocked_Change
				RegionEntryAccessBlocked_KB
				LandClaimAccessBlocked 
				LandClaimAccessBlocked_Notify 
				LandClaimAccessBlocked_Change 
				LandClaimAccessBlocked_KB 
				LandBuyAccessBlocked
				LandBuyAccessBlocked_Notify
				LandBuyAccessBlocked_Change
				LandBuyAccessBlocked_KB
			 
			-----------------------------------------------------------------------*/ 
			if (handle_special_notification(notificationID, llsdBlock))
			{
				return true;
			}
		}
		
		LLNotifications::instance().add(notificationID, llsdBlock);
		return true;
	}	
	return false;
}


void process_agent_alert_message(LLMessageSystem* msgsystem, void** user_data)
{
	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();
	
	if (!attempt_standard_notification(msgsystem))
	{
		BOOL modal = FALSE;
		msgsystem->getBOOL("AlertData", "Modal", modal);
		std::string buffer;
		msgsystem->getStringFast(_PREHASH_AlertData, _PREHASH_Message, buffer);
		process_alert_core(buffer, modal);
	}
}

// The only difference between this routine and the previous is the fact that
// for this routine, the modal parameter is always false. Sadly, for the message
// handled by this routine, there is no "Modal" parameter on the message, and
// there's no API to tell if a message has the given parameter or not.
// So we can't handle the messages with the same handler.
void process_alert_message(LLMessageSystem *msgsystem, void **user_data)
{
	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();
		
	if (!attempt_standard_notification(msgsystem))
	{
		BOOL modal = FALSE;
		std::string buffer;
		msgsystem->getStringFast(_PREHASH_AlertData, _PREHASH_Message, buffer);
		process_alert_core(buffer, modal);
	}
}

void process_alert_core(const std::string& message, BOOL modal)
{
	// HACK -- handle callbacks for specific alerts
	if ( message == "You died and have been teleported to your home location")
	{
		LLViewerStats::getInstance()->incStat(LLViewerStats::ST_KILLED_COUNT);
	}
	else if( message == "Home position set." )
	{
		// save the home location image to disk
		std::string snap_filename = gDirUtilp->getLindenUserDir();
		snap_filename += gDirUtilp->getDirDelimiter();
		snap_filename += SCREEN_HOME_FILENAME;
		gViewerWindow->saveSnapshot(snap_filename, gViewerWindow->getWindowDisplayWidth(), gViewerWindow->getWindowDisplayHeight(), FALSE, FALSE);
	}

	const std::string ALERT_PREFIX("ALERT: ");
	const std::string NOTIFY_PREFIX("NOTIFY: ");
	if (message.find(ALERT_PREFIX) == 0)
	{
		// Allow the server to spawn a named alert so that server alerts can be
		// translated out of English.
		std::string alert_name(message.substr(ALERT_PREFIX.length()));
		LLNotifications::instance().add(alert_name);
	}
	else if (message.find(NOTIFY_PREFIX) == 0)
	{
		// Allow the server to spawn a named notification so that server notifications can be
		// translated out of English.
		std::string notify_name(message.substr(NOTIFY_PREFIX.length()));
		LLNotifications::instance().add(notify_name);
	}
	else if (message[0] == '/')
	{
		// System message is important, show in upper-right box not tip
		std::string text(message.substr(1));
		LLSD args;
		if (text.substr(0,17) == "RESTART_X_MINUTES")
		{
			S32 mins = 0;
			LLStringUtil::convertToS32(text.substr(18), mins);
			args["MINUTES"] = llformat("%d",mins);
			LLNotifications::instance().add("RegionRestartMinutes", args);
		}
		else if (text.substr(0,17) == "RESTART_X_SECONDS")
		{
			S32 secs = 0;
			LLStringUtil::convertToS32(text.substr(18), secs);
			args["SECONDS"] = llformat("%d",secs);
			LLNotifications::instance().add("RegionRestartSeconds", args);
		}
		else
		{
			// *TODO:translate
			args["MESSAGE"] = text;
			LLNotifications::instance().add("SystemMessage", args);
		}
	}
	else if (modal)
	{
		// *TODO:translate
		LLSD args;
		args["ERROR_MESSAGE"] = message;
		LLNotifications::instance().add("ErrorMessage", args);
	}
	else
	{
		// *TODO:translate
		LLSD args;
		args["MESSAGE"] = message;
		LLNotifications::instance().add("SystemMessageTip", args);
	}
}

mean_collision_list_t				gMeanCollisionList;
time_t								gLastDisplayedTime = 0;

void handle_show_mean_events(void *)
{
	if (gNoRender)
	{
		return;
	}

	LLFloaterBump::show(NULL);
}

void mean_name_callback(const LLUUID &id, const std::string& first, const std::string& last, BOOL always_false, void* data)
{
	if (gNoRender)
	{
		return;
	}

	static const U32 max_collision_list_size = 20;
	if (gMeanCollisionList.size() > max_collision_list_size)
	{
		mean_collision_list_t::iterator iter = gMeanCollisionList.begin();
		for (U32 i=0; i<max_collision_list_size; i++) iter++;
		for_each(iter, gMeanCollisionList.end(), DeletePointer());
		gMeanCollisionList.erase(iter, gMeanCollisionList.end());
	}

	for (mean_collision_list_t::iterator iter = gMeanCollisionList.begin();
		 iter != gMeanCollisionList.end(); ++iter)
	{
		LLMeanCollisionData *mcd = *iter;
		if (mcd->mPerp == id)
		{
			mcd->mFirstName = first;
			mcd->mLastName = last;
		}
	}
}

void process_mean_collision_alert_message(LLMessageSystem *msgsystem, void **user_data)
{
	if (gAgent.inPrelude())
	{
		// In prelude, bumping is OK.  This dialog is rather confusing to 
		// newbies, so we don't show it.  Drop the packet on the floor.
		return;
	}

	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();

	LLUUID perp;
	U32	   time;
	U8	   u8type;
	EMeanCollisionType	   type;
	F32    mag;

	S32 i, num = msgsystem->getNumberOfBlocks(_PREHASH_MeanCollision);

	for (i = 0; i < num; i++)
	{
		msgsystem->getUUIDFast(_PREHASH_MeanCollision, _PREHASH_Perp, perp);
		msgsystem->getU32Fast(_PREHASH_MeanCollision, _PREHASH_Time, time);
		msgsystem->getF32Fast(_PREHASH_MeanCollision, _PREHASH_Mag, mag);
		msgsystem->getU8Fast(_PREHASH_MeanCollision, _PREHASH_Type, u8type);

		type = (EMeanCollisionType)u8type;

		BOOL b_found = FALSE;

		for (mean_collision_list_t::iterator iter = gMeanCollisionList.begin();
			 iter != gMeanCollisionList.end(); ++iter)
		{
			LLMeanCollisionData *mcd = *iter;
			if ((mcd->mPerp == perp) && (mcd->mType == type))
			{
				mcd->mTime = time;
				mcd->mMag = mag;
				b_found = TRUE;
				break;
			}
		}

		if (!b_found)
		{
			LLMeanCollisionData *mcd = new LLMeanCollisionData(gAgentID, perp, time, type, mag);
			gMeanCollisionList.push_front(mcd);
			const BOOL is_group = FALSE;
			gCacheName->get(perp, is_group, mean_name_callback);
		}
	}
}

void process_frozen_message(LLMessageSystem *msgsystem, void **user_data)
{
	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();
	BOOL b_frozen;
	
	msgsystem->getBOOL("FrozenData", "Data", b_frozen);

	// TODO: make being frozen change view
	if (b_frozen)
	{
	}
	else
	{
	}
}

// do some extra stuff once we get our economy data
void process_economy_data(LLMessageSystem *msg, void** /*user_data*/)
{
	LLGlobalEconomy::processEconomyData(msg, LLGlobalEconomy::Singleton::getInstance());

	S32 upload_cost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();

	LL_INFOS_ONCE("Messaging") << "EconomyData message arrived; upload cost is L$" << upload_cost << LL_ENDL;

	LLFloaterImagePreview::setUploadAmount(upload_cost);
	LLFloaterAnimPreview::setUploadAmount(upload_cost);

	std::string fee = gHippoGridManager->getConnectedGrid()->getUploadFee();
	gMenuHolder->childSetLabelArg("Upload Image", "[UPLOADFEE]", fee);
	gMenuHolder->childSetLabelArg("Upload Sound", "[UPLOADFEE]", fee);
	gMenuHolder->childSetLabelArg("Upload Animation", "[UPLOADFEE]", fee);
	gMenuHolder->childSetLabelArg("Bulk Upload", "[UPLOADFEE]", fee);
	gMenuHolder->childSetLabelArg("ImportUpload", "[UPLOADFEE]", fee);
	gMenuHolder->childSetLabelArg("Buy and Sell L$...", "[CURRENCY]",
		gHippoGridManager->getConnectedGrid()->getCurrencySymbol());
}

void notify_cautioned_script_question(const LLSD& notification, const LLSD& response, S32 orig_questions, BOOL granted)
{
	// only continue if at least some permissions were requested
	if (orig_questions)
	{
		// check to see if the person we are asking

		// "'[OBJECTNAME]', an object owned by '[OWNERNAME]', 
		// located in [REGIONNAME] at [REGIONPOS], 
		// has been <granted|denied> permission to: [PERMISSIONS]."

		LLUIString notice(LLFloaterChat::getInstance()->getString(granted ? "ScriptQuestionCautionChatGranted" : "ScriptQuestionCautionChatDenied"));

		// always include the object name and owner name 
		notice.setArg("[OBJECTNAME]", notification["payload"]["object_name"].asString());
		notice.setArg("[OWNERNAME]", notification["payload"]["owner_name"].asString());

		// try to lookup viewerobject that corresponds to the object that
		// requested permissions (here, taskid->requesting object id)
		BOOL foundpos = FALSE;
		LLViewerObject* viewobj = gObjectList.findObject(notification["payload"]["task_id"].asUUID());
		if (viewobj)
		{
			// found the viewerobject, get it's position in its region
			LLVector3 objpos(viewobj->getPosition());
			
			// try to lookup the name of the region the object is in
			LLViewerRegion* viewregion = viewobj->getRegion();
			if (viewregion)
			{
				// got the region, so include the region and 3d coordinates of the object
				notice.setArg("[REGIONNAME]", viewregion->getName());
// [RLVa:KB] - Checked: 2009-07-04 (RLVa-1.0.0a)
				if ( (rlv_handler_t::isEnabled()) && (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) )
				{
					notice.setArg("[REGIONNAME]", RlvStrings::getString(RLV_STRING_HIDDEN_REGION));
				}
// [/RLVa:KB]
				std::string formatpos = llformat("%.1f, %.1f,%.1f", objpos[VX], objpos[VY], objpos[VZ]);
				notice.setArg("[REGIONPOS]", formatpos);

				foundpos = TRUE;
			}
		}

		if (!foundpos)
		{
			// unable to determine location of the object
			notice.setArg("[REGIONNAME]", "(unknown region)");
			notice.setArg("[REGIONPOS]", "(unknown position)");
		}

		// check each permission that was requested, and list each 
		// permission that has been flagged as a caution permission
		BOOL caution = FALSE;
		S32 count = 0;
		std::string perms;
		for (S32 i = 0; i < SCRIPT_PERMISSION_EOF; i++)
		{
			if ((orig_questions & LSCRIPTRunTimePermissionBits[i]) && SCRIPT_QUESTION_IS_CAUTION[i])
			{
				count++;
				caution = TRUE;

				// add a comma before the permission description if it is not the first permission
				// added to the list or the last permission to check
				if ((count > 1) && (i < SCRIPT_PERMISSION_EOF))
				{
					perms.append(", ");
				}

				LLStringUtil::format_map_t args;
				args["[CURRENCY]"] = gHippoGridManager->getConnectedGrid()->getCurrencySymbol();
				perms.append(LLFloaterChat::getInstance()->getString(SCRIPT_QUESTIONS[i], args));
			}
		}

		notice.setArg("[PERMISSIONS]", perms);

		// log a chat message as long as at least one requested permission
		// is a caution permission
		if (caution)
		{
			LLChat chat(notice.getString());
			LLFloaterChat::addChat(chat, FALSE, FALSE);
		}
	}
}

bool script_question_cb(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLMessageSystem *msg = gMessageSystem;
	S32 orig = notification["payload"]["questions"].asInteger();
	S32 new_questions = orig;

	// check whether permissions were granted or denied
	BOOL allowed = TRUE;
	// the "yes/accept" button is the first button in the template, making it button 0
	// if any other button was clicked, the permissions were denied
	if (option != 0)
	{
		new_questions = 0;
		allowed = FALSE;
	}	

	LLUUID task_id = notification["payload"]["task_id"].asUUID();
	LLUUID item_id = notification["payload"]["item_id"].asUUID();

	// reply with the permissions granted or denied
	msg->newMessageFast(_PREHASH_ScriptAnswerYes);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_Data);
	msg->addUUIDFast(_PREHASH_TaskID, task_id);
	msg->addUUIDFast(_PREHASH_ItemID, item_id);
	msg->addS32Fast(_PREHASH_Questions, new_questions);
	msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));

	// only log a chat message if caution prompts are enabled
	if (gSavedSettings.getBOOL("PermissionsCautionEnabled"))
	{
		// log a chat message, if appropriate
		notify_cautioned_script_question(notification, response, orig, allowed);
	}

	if ( response["Mute"] ) // mute
	{
		LLMuteList::getInstance()->add(LLMute(item_id, notification["payload"]["object_name"].asString(), LLMute::OBJECT));

		// purge the message queue of any previously queued requests from the same source. DEV-4879
		class OfferMatcher : public LLNotifyBoxView::Matcher
		{
		public:
			OfferMatcher(const LLUUID& to_block) : blocked_id(to_block) {}
			BOOL matches(const LLNotificationPtr notification) const
			{
				if (notification->getName() == "ScriptQuestionCaution"
					|| notification->getName() == "ScriptQuestion")
				{
					return (notification->getPayload()["item_id"].asUUID() == blocked_id);
				}
				return FALSE;
			}
		private:
			const LLUUID& blocked_id;
		};
		// should do this via the channel
		gNotifyBoxView->purgeMessagesMatching(OfferMatcher(item_id));
	}

	if (response["Details"])
	{
		// respawn notification...
		LLNotifications::instance().add(notification["name"], notification["substitutions"], notification["payload"]);

		// ...with description on top
		LLNotifications::instance().add("DebitPermissionDetails");
	}
	return false;
}
static LLNotificationFunctorRegistration script_question_cb_reg_1("ScriptQuestion", script_question_cb);
static LLNotificationFunctorRegistration script_question_cb_reg_2("ScriptQuestionCaution", script_question_cb);

void process_script_question(LLMessageSystem *msg, void **user_data)
{
	// *TODO:translate owner name -> [FIRST] [LAST]

	LLHost sender = msg->getSender();

	LLUUID taskid;
	LLUUID itemid;
	S32		questions;
	std::string object_name;
	std::string owner_name;

	// taskid -> object key of object requesting permissions
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_TaskID, taskid );
	// itemid -> script asset key of script requesting permissions
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_ItemID, itemid );
	msg->getStringFast(_PREHASH_Data, _PREHASH_ObjectName, object_name);
	msg->getStringFast(_PREHASH_Data, _PREHASH_ObjectOwner, owner_name);
	msg->getS32Fast(_PREHASH_Data, _PREHASH_Questions, questions );

	// Special case. If the objects are owned by this agent, throttle per-object instead
	// of per-owner. It's common for residents to reset a ton of scripts that re-request
	// permissions, as with tier boxes. UUIDs can't be valid agent names and vice-versa,
	// so we'll reuse the same namespace for both throttle types.
	std::string throttle_name = owner_name;
	std::string self_name;
	gAgent.getName( self_name );
	if( owner_name == self_name )
	{
		throttle_name = taskid.getString();
	}
	
	// don't display permission requests if this object is muted
	if (LLMuteList::getInstance()->isMuted(taskid)) return;
	
	// throttle excessive requests from any specific user's scripts
	typedef LLKeyThrottle<std::string> LLStringThrottle;
	static LLStringThrottle question_throttle( LLREQUEST_PERMISSION_THROTTLE_LIMIT, LLREQUEST_PERMISSION_THROTTLE_INTERVAL );

	switch (question_throttle.noteAction(throttle_name))
	{
		case LLStringThrottle::THROTTLE_NEWLY_BLOCKED:
			LL_INFOS("Messaging") << "process_script_question throttled"
					<< " owner_name:" << owner_name
					<< LL_ENDL;
			// Fall through

		case LLStringThrottle::THROTTLE_BLOCKED:
			// Escape altogether until we recover
			return;

		case LLStringThrottle::THROTTLE_OK:
			break;
	}

	std::string script_question;
	if (questions)
	{
		BOOL caution = FALSE;
		S32 count = 0;
		LLSD args;
		args["OBJECTNAME"] = object_name;
		args["NAME"] = owner_name;

		// check the received permission flags against each permission
		for (S32 i = 0; i < SCRIPT_PERMISSION_EOF; i++)
		{
			if (questions & LSCRIPTRunTimePermissionBits[i])
			{
				LLStringUtil::format_map_t args;
				args["[CURRENCY]"] = gHippoGridManager->getConnectedGrid()->getCurrencySymbol();
				
				count++;
				script_question += "    " + LLFloaterChat::getInstance()->getString(SCRIPT_QUESTIONS[i], args) + "\n";

				// check whether permission question should cause special caution dialog
				caution |= (SCRIPT_QUESTION_IS_CAUTION[i]);
			}
		}
		args["QUESTIONS"] = script_question;

		LLSD payload;
		payload["task_id"] = taskid;
		payload["item_id"] = itemid;
		payload["sender"] = sender.getIPandPort();
		payload["questions"] = questions;
		payload["object_name"] = object_name;
		payload["owner_name"] = owner_name;

// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-10 (RLVa-1.0.0g) | Modified: RLVa-0.2.0e
		S32 rlvQuestionsOther = questions;

		if ( (rlv_handler_t::isEnabled()) && (gRlvHandler.hasBehaviour(RLV_BHVR_ACCEPTPERMISSION)) )
		{
			LLViewerObject* pObj = gObjectList.findObject(taskid);
			if (pObj)
			{
//				if (pObj->permYouOwner())
//				{
					// PERMISSION_TAKE_CONTROLS and PERMISSION_ATTACH are only auto-granted to objects this avie owns
					rlvQuestionsOther &= ~(LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_TAKE_CONTROLS] | 
						LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_ATTACH]);
//				}
			}
		}

		if ( (!caution) && (!rlvQuestionsOther) )
		{
			LLNotifications::instance().forceResponse(
				LLNotification::Params("ScriptQuestion").substitutions(args).payload(payload), 0/*YES*/);
		}
		else if (gSavedSettings.getBOOL("PermissionsCautionEnabled"))
// [/RLVa:KB]
		// check whether cautions are even enabled or not
		//if (gSavedSettings.getBOOL("PermissionsCautionEnabled"))
		{
			// display the caution permissions prompt
			LLNotifications::instance().add(caution ? "ScriptQuestionCaution" : "ScriptQuestion", args, payload);
		}
		else
		{
			// fall back to default behavior if cautions are entirely disabled
			LLNotifications::instance().add("ScriptQuestion", args, payload);
		}

	}
}


void process_derez_container(LLMessageSystem *msg, void**)
{
	LL_WARNS("Messaging") << "call to deprecated process_derez_container" << LL_ENDL;
}

void container_inventory_arrived(LLViewerObject* object,
								 InventoryObjectList* inventory,
								 S32 serial_num,
								 void* data)
{
	LL_DEBUGS("Messaging") << "container_inventory_arrived()" << LL_ENDL;
	if( gAgent.cameraMouselook() )
	{
		gAgent.changeCameraToDefault();
	}

	LLInventoryView* view = LLInventoryView::getActiveInventory();

	if (inventory->size() > 2)
	{
		// create a new inventory category to put this in
		LLUUID cat_id;
		cat_id = gInventory.createNewCategory(gAgent.getInventoryRootID(),
											  LLAssetType::AT_NONE,
											  std::string("Acquired Items")); //TODO: Translate

		InventoryObjectList::const_iterator it = inventory->begin();
		InventoryObjectList::const_iterator end = inventory->end();
		for ( ; it != end; ++it)
		{
			if ((*it)->getType() != LLAssetType::AT_CATEGORY &&
				(*it)->getType() != LLAssetType::AT_ROOT_CATEGORY)
			{
				LLInventoryObject* obj = (LLInventoryObject*)(*it);
				LLInventoryItem* item = (LLInventoryItem*)(obj);
				LLUUID item_id;
				item_id.generate();
				time_t creation_date_utc = time_corrected();
				LLPointer<LLViewerInventoryItem> new_item
					= new LLViewerInventoryItem(item_id,
												cat_id,
												item->getPermissions(),
												item->getAssetUUID(),
												item->getType(),
												item->getInventoryType(),
												item->getName(),
												item->getDescription(),
												LLSaleInfo::DEFAULT,
												item->getFlags(),
												creation_date_utc);
				new_item->updateServer(TRUE);
				gInventory.updateItem(new_item);
			}
		}
		gInventory.notifyObservers();
		if(view)
		{
			view->getPanel()->setSelection(cat_id, TAKE_FOCUS_NO);
		}
	}
	else if (inventory->size() == 2)
	{
		// we're going to get one fake root category as well as the
		// one actual object
		InventoryObjectList::iterator it = inventory->begin();

		if ((*it)->getType() == LLAssetType::AT_CATEGORY ||
			(*it)->getType() == LLAssetType::AT_ROOT_CATEGORY)
		{
			++it;
		}

		LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
		LLUUID category = gInventory.findCategoryUUIDForType(item->getType());

		LLUUID item_id;
		item_id.generate();
		time_t creation_date_utc = time_corrected();
		LLPointer<LLViewerInventoryItem> new_item
			= new LLViewerInventoryItem(item_id, category,
										item->getPermissions(),
										item->getAssetUUID(),
										item->getType(),
										item->getInventoryType(),
										item->getName(),
										item->getDescription(),
										LLSaleInfo::DEFAULT,
										item->getFlags(),
										creation_date_utc);
		new_item->updateServer(TRUE);
		gInventory.updateItem(new_item);
		gInventory.notifyObservers();
		if(view)
		{
			view->getPanel()->setSelection(item_id, TAKE_FOCUS_NO);
		}
	}

	// we've got the inventory, now delete this object if this was a take
	BOOL delete_object = (BOOL)(intptr_t)data;
	LLViewerRegion *region = gAgent.getRegion();
	if (delete_object && region)
	{
		gMessageSystem->newMessageFast(_PREHASH_ObjectDelete);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		const U8 NO_FORCE = 0;
		gMessageSystem->addU8Fast(_PREHASH_Force, NO_FORCE);
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
		gMessageSystem->sendReliable(region->getHost());
	}
}

// method to format the time.
std::string formatted_time(const time_t& the_time)
{
	std::string timestr;
	timeToFormattedString(the_time, gSavedSettings.getString("TimestampFormat"), timestr);
	char buffer[30]; /* Flawfinder: ignore */
	LLStringUtil::copy(buffer, timestr.c_str(), 30);
	buffer[24] = '\0';
	return std::string(buffer);
}


void process_teleport_failed(LLMessageSystem *msg, void**)
{
	std::string reason;
	std::string big_reason;
	LLSD args;

	// if we have additional alert data
	if (msg->getNumberOfBlocksFast(_PREHASH_AlertInfo) > 0)
	{
		// Get the message ID
		msg->getStringFast(_PREHASH_AlertInfo, _PREHASH_Message, reason);
		big_reason = LLAgent::sTeleportErrorMessages[reason];
		if ( big_reason.size() > 0 )
		{	// Substitute verbose reason from the local map
			args["REASON"] = big_reason;
		}
		else
		{	// Nothing found in the map - use what the server returned in the original message block
			msg->getStringFast(_PREHASH_Info, _PREHASH_Reason, reason);
			args["REASON"] = reason;
		}

		LLSD llsd_block;
		std::string llsd_raw;
		msg->getStringFast(_PREHASH_AlertInfo, _PREHASH_ExtraParams, llsd_raw);
		if (llsd_raw.length())
		{
			std::istringstream llsd_data(llsd_raw);
			if (!LLSDSerialize::deserialize(llsd_block, llsd_data, llsd_raw.length()))
			{
				llwarns << "process_teleport_failed: Attempted to read alert parameter data into LLSD but failed:" << llsd_raw << llendl;
			}
			else
			{
				// change notification name in this special case
				if (handle_special_notification("RegionEntryAccessBlocked", llsd_block))
				{
					if( gAgent.getTeleportState() != LLAgent::TELEPORT_NONE )
					{
						gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
					}
					return;
				}
			}
		}

	}
	else
	{
		msg->getStringFast(_PREHASH_Info, _PREHASH_Reason, reason);

		big_reason = LLAgent::sTeleportErrorMessages[reason];
		if ( big_reason.size() > 0 )
		{	// Substitute verbose reason from the local map
			args["REASON"] = big_reason;
		}
		else
		{	// Nothing found in the map - use what the server returned
			args["REASON"] = reason;
		}
	}

	LLNotifications::instance().add("CouldNotTeleportReason", args);

	if( gAgent.getTeleportState() != LLAgent::TELEPORT_NONE )
	{
		gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
	}
}

void process_teleport_local(LLMessageSystem *msg,void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_Info, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "Got teleport notification for wrong agent!" << LL_ENDL;
		return;
	}

	U32 location_id;
	LLVector3 pos, look_at;
	U32 teleport_flags;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_LocationID, location_id);
	msg->getVector3Fast(_PREHASH_Info, _PREHASH_Position, pos);
	msg->getVector3Fast(_PREHASH_Info, _PREHASH_LookAt, look_at);
	msg->getU32Fast(_PREHASH_Info, _PREHASH_TeleportFlags, teleport_flags);
	
	if( gAgent.getTeleportState() != LLAgent::TELEPORT_NONE )
	{
		if( gAgent.getTeleportState() == LLAgent::TELEPORT_LOCAL )
		{
			// To prevent TeleportStart messages re-activating the progress screen right
			// after tp, keep the teleport state and let progress screen clear it after a short delay
			// (progress screen is active but not visible)  *TODO: remove when SVC-5290 is fixed
			gTeleportDisplayTimer.reset();
			gTeleportDisplay = TRUE;
		}
		else
		{
			gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
		}
	}

	// Sim tells us whether the new position is off the ground
	if (teleport_flags & TELEPORT_FLAGS_IS_FLYING)
	{
		gAgent.setFlying(TRUE);
	}
	else
	{
		gAgent.setFlying(FALSE);
	}

	gAgent.setPositionAgent(pos);
	gAgent.slamLookAt(look_at);

	if ( !(gAgent.getTeleportKeepsLookAt() && LLViewerJoystick::getInstance()->getOverrideCamera()) && gSavedSettings.getBOOL("OptionRotateCamAfterLocalTP"))
	{
		gAgent.resetView(TRUE, TRUE);
	}

	// send camera update to new region
	//gAgent.updateCamera();

	send_agent_update(TRUE, TRUE);
}

void send_simple_im(const LLUUID& to_id,
					const std::string& message,
					EInstantMessage dialog,
					const LLUUID& id)
{
	std::string my_name;
	gAgent.buildFullname(my_name);
	send_improved_im(to_id,
					 my_name,
					 message,
					 IM_ONLINE,
					 dialog,
					 id,
					 NO_TIMESTAMP,
					 (U8*)EMPTY_BINARY_BUCKET,
					 EMPTY_BINARY_BUCKET_SIZE);
}

void send_group_notice(const LLUUID& group_id,
					   const std::string& subject,
					   const std::string& message,
					   const LLInventoryItem* item)
{
	// Put this notice into an instant message form.
	// This will mean converting the item to a binary bucket,
	// and the subject/message into a single field.
	std::string my_name;
	gAgent.buildFullname(my_name);

	// Combine subject + message into a single string.
	std::ostringstream subject_and_message;
	// TODO: turn all existing |'s into ||'s in subject and message.
	subject_and_message << subject << "|" << message;

	// Create an empty binary bucket.
	U8 bin_bucket[MAX_INVENTORY_BUFFER_SIZE];
	U8* bucket_to_send = bin_bucket;
	bin_bucket[0] = '\0';
	S32 bin_bucket_size = EMPTY_BINARY_BUCKET_SIZE;
	// If there is an item being sent, pack it into the binary bucket.
	if (item)
	{
		LLSD item_def;
		item_def["item_id"] = item->getUUID();
		item_def["owner_id"] = item->getPermissions().getOwner();
		std::ostringstream ostr;
		LLSDSerialize::serialize(item_def, ostr, LLSDSerialize::LLSD_XML);
		bin_bucket_size = ostr.str().copy(
			(char*)bin_bucket, ostr.str().size());
		bin_bucket[bin_bucket_size] = '\0';
	}
	else
	{
		bucket_to_send = (U8*) EMPTY_BINARY_BUCKET;
	}
   

	send_improved_im(
			group_id,
			my_name,
			subject_and_message.str(),
			IM_ONLINE,
			IM_GROUP_NOTICE,
			LLUUID::null,
			NO_TIMESTAMP,
			bucket_to_send,
			bin_bucket_size);
}

bool handle_lure_callback(const LLSD& notification, const LLSD& response)
{
	std::string text = response["message"].asString();
	S32 option = LLNotification::getSelectedOption(notification, response);

	if(0 == option)
	{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-10 (RLVa-1.0.0g) | Modified: RLVa-0.2.0b
		if (gRlvHandler.hasBehaviour(RLV_BHVR_SENDIM))
		{
			for (LLSD::array_const_iterator it = notification["payload"]["ids"].beginArray(); 
					it != notification["payload"]["ids"].endArray(); ++it)
			{
				if (!gRlvHandler.isException(RLV_BHVR_SENDIM, it->asUUID()))
				{
					text = RlvStrings::getString(RLV_STRING_HIDDEN);
					break;
				}
			}
		}
// [/RLVa:KB]

		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_StartLure);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_Info);
		msg->addU8Fast(_PREHASH_LureType, (U8)0); // sim will fill this in.
		msg->addStringFast(_PREHASH_Message, text);
		for(LLSD::array_const_iterator it = notification["payload"]["ids"].beginArray();
			it != notification["payload"]["ids"].endArray();
			++it)
		{
			msg->nextBlockFast(_PREHASH_TargetData);
			msg->addUUIDFast(_PREHASH_TargetID, it->asUUID());
		}
		gAgent.sendReliableMessage();
	}

	return false;
}

void handle_lure(const LLUUID& invitee)
{
	LLDynamicArray<LLUUID> ids;
	ids.push_back(invitee);
	handle_lure(ids);
}

// Prompt for a message to the invited user.
void handle_lure(LLDynamicArray<LLUUID>& ids) 
{
	if (ids.empty()) return;

	if (!gAgent.getRegion()) return;
	LLSD edit_args;
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-04 (RLVa-1.0.0a)
	edit_args["REGION"] = 
		(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) ? gAgent.getRegion()->getName() : RlvStrings::getString(RLV_STRING_HIDDEN);
// [/RLVa:KB]
	//edit_args["REGION"] = gAgent.getRegion()->getName();

	LLSD payload;
	for (LLDynamicArray<LLUUID>::iterator it = ids.begin();
		it != ids.end();
		++it)
	{
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-07 (RLVa-1.0.0d) | Modified: RLVa-1.0.0a
		// Only allow offering teleports if everyone is a @tplure exception or able to map this avie under @showloc=n
		if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
		{
			const LLRelationship* pBuddyInfo = LLAvatarTracker::instance().getBuddyInfo(*it);
			if ( (!gRlvHandler.isException(RLV_BHVR_TPLURE, *it, RLV_CHECK_PERMISSIVE)) &&
				 ((!pBuddyInfo) || (!pBuddyInfo->isOnline()) || (!pBuddyInfo->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION))) )
			{
				return;
			}
		}
// [/RLVa:KB]
		payload["ids"].append(*it);
	}
	if (gAgent.isGodlike())
	{
		LLNotifications::instance().add("OfferTeleportFromGod", edit_args, payload, handle_lure_callback);
	}
	else
	{
		LLNotifications::instance().add("OfferTeleport", edit_args, payload, handle_lure_callback);
	}
}


void send_improved_im(const LLUUID& to_id,
							const std::string& name,
							const std::string& message,
							U8 offline,
							EInstantMessage dialog,
							const LLUUID& id,
							U32 timestamp, 
							const U8* binary_bucket,
							S32 binary_bucket_size)
{
	pack_instant_message(
		gMessageSystem,
		gAgent.getID(),
		FALSE,
		gAgent.getSessionID(),
		to_id,
		name,
		message,
		offline,
		dialog,
		id,
		0,
		LLUUID::null,
		gAgent.getPositionAgent(),
		timestamp,
		binary_bucket,
		binary_bucket_size);
	gAgent.sendReliableMessage();
}


void send_places_query(const LLUUID& query_id,
					   const LLUUID& trans_id,
					   const std::string& query_text,
					   U32 query_flags,
					   S32 category,
					   const std::string& sim_name)
{
	LLMessageSystem* msg = gMessageSystem;

	msg->newMessage("PlacesQuery");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUID("SessionID", gAgent.getSessionID());
	msg->addUUID("QueryID", query_id);
	msg->nextBlock("TransactionData");
	msg->addUUID("TransactionID", trans_id);
	msg->nextBlock("QueryData");
	msg->addString("QueryText", query_text);
	msg->addU32("QueryFlags", query_flags);
	msg->addS8("Category", (S8)category);
	msg->addString("SimName", sim_name);
	gAgent.sendReliableMessage();
}


void process_user_info_reply(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if(agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "process_user_info_reply - "
				<< "wrong agent id." << LL_ENDL;
	}
	
	BOOL im_via_email;
	msg->getBOOLFast(_PREHASH_UserData, _PREHASH_IMViaEMail, im_via_email);
	std::string email;
	msg->getStringFast(_PREHASH_UserData, _PREHASH_EMail, email);
	std::string dir_visibility;
	msg->getString( "UserData", "DirectoryVisibility", dir_visibility);

	LLFloaterPreference::updateUserInfo(dir_visibility, im_via_email, email);
	LLFloaterPostcard::updateUserInfo(email);
}


//---------------------------------------------------------------------------
// Script Dialog
//---------------------------------------------------------------------------

const S32 SCRIPT_DIALOG_MAX_BUTTONS = 12;
const S32 SCRIPT_DIALOG_BUTTON_STR_SIZE = 24;
const S32 SCRIPT_DIALOG_MAX_MESSAGE_SIZE = 512;
const char* SCRIPT_DIALOG_HEADER = "Script Dialog:\n";

bool callback_script_dialog(const LLSD& notification, const LLSD& response)
{
	LLNotificationForm form(notification["form"]);
	std::string button = LLNotification::getSelectedOptionName(response);
	S32 button_idx = LLNotification::getSelectedOption(notification, response);
	// Didn't click "Ignore"
	if (button_idx != -1)
	{
		if (notification["payload"].has("textbox"))
		{
			button = response["message"].asString();
		}
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("ScriptDialogReply");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("ObjectID", notification["payload"]["object_id"].asUUID());
		msg->addS32("ChatChannel", notification["payload"]["chat_channel"].asInteger());
		msg->addS32("ButtonIndex", button_idx);
		msg->addString("ButtonLabel", button);
		msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));
	}

	return false;
}
static LLNotificationFunctorRegistration callback_script_dialog_reg_1("ScriptDialog", callback_script_dialog);
static LLNotificationFunctorRegistration callback_script_dialog_reg_2("ScriptDialogGroup", callback_script_dialog);

void process_script_dialog(LLMessageSystem* msg, void**)
{
	S32 i;

	LLSD payload;

	std::string message; 
	std::string first_name;
	std::string last_name;
	std::string title;

	LLUUID object_id;
	S32 chat_channel;
	msg->getUUID("Data", "ObjectID", object_id);
	msg->getString("Data", "FirstName", first_name);
	msg->getString("Data", "LastName", last_name);
	msg->getString("Data", "ObjectName", title);
	msg->getString("Data", "Message", message);
	msg->getS32("Data", "ChatChannel", chat_channel);

		// unused for now
	LLUUID image_id;
	msg->getUUID("Data", "ImageID", image_id);

	payload["sender"] = msg->getSender().getIPandPort();
	payload["object_id"] = object_id;
	payload["chat_channel"] = chat_channel;

	// build up custom form
	S32 button_count = msg->getNumberOfBlocks("Buttons");
	if (button_count > SCRIPT_DIALOG_MAX_BUTTONS)
	{
		llwarns << "Too many script dialog buttons - omitting some" << llendl;
		button_count = SCRIPT_DIALOG_MAX_BUTTONS;
	}

	LLNotificationForm form;
	std::string firstbutton;
	msg->getString("Buttons", "ButtonLabel", firstbutton, 0);
	form.addElement("button", std::string(firstbutton));
	bool is_text_box = false;
	std::string default_text;
	if (firstbutton == "!!llTextBox!!")
	{
		is_text_box = true;
		for (i = 1; i < button_count; i++)
		{
			std::string tdesc;
			msg->getString("Buttons", "ButtonLabel", tdesc, i);
			default_text += tdesc;
		}
	}
	else
	{
		for (i = 1; i < button_count; i++)
	{
		std::string tdesc;
		msg->getString("Buttons", "ButtonLabel", tdesc, i);
		form.addElement("button", std::string(tdesc));
	}
	}

	LLSD args;
	args["TITLE"] = title;
	args["MESSAGE"] = message;
	// <edit>
	args["CHANNEL"] = chat_channel;
	// </edit>
	LLNotificationPtr notification;
	if (!first_name.empty())
	{
		args["FIRST"] = first_name;
		args["LAST"] = last_name;

		if (is_text_box)
		{
			args["DEFAULT"] = default_text;
			payload["textbox"] = "true";
			LLNotifications::instance().add("ScriptTextBoxDialog", args, payload, callback_script_dialog);
		}
		else
		{
		notification = LLNotifications::instance().add(
			LLNotification::Params("ScriptDialog").substitutions(args).payload(payload).form_elements(form.asLLSD()));
	}
	}
	else
	{
		args["GROUPNAME"] = last_name;
		notification = LLNotifications::instance().add(
			LLNotification::Params("ScriptDialogGroup").substitutions(args).payload(payload).form_elements(form.asLLSD()));
	}
}

//---------------------------------------------------------------------------


std::vector<LLSD> gLoadUrlList;

bool callback_load_url(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	if (0 == option)
	{
		LLWeb::loadURL(notification["payload"]["url"].asString());
	}

	return false;
}
static LLNotificationFunctorRegistration callback_load_url_reg("LoadWebPage", callback_load_url);


// We've got the name of the person who owns the object hurling the url.
// Display confirmation dialog.
void callback_load_url_name(const LLUUID& id, const std::string& first, const std::string& last, BOOL is_group, void* data)
{
	std::vector<LLSD>::iterator it;
	for (it = gLoadUrlList.begin(); it != gLoadUrlList.end(); )
	{
		LLSD load_url_info = *it;
		if (load_url_info["owner_id"].asUUID() == id)
		{
			it = gLoadUrlList.erase(it);

			std::string owner_name;
			if (is_group)
			{
				owner_name = first + " (group)";
			}
			else
			{
				owner_name = first + " " + last;
			}

			// For legacy name-only mutes.
			if (LLMuteList::getInstance()->isMuted(LLUUID::null, owner_name))
			{
				continue;
			}
			LLSD args;
			args["URL"] = load_url_info["url"].asString();
			args["MESSAGE"] = load_url_info["message"].asString();;
			args["OBJECTNAME"] = load_url_info["object_name"].asString();
			args["NAME"] = owner_name;

			LLNotifications::instance().add("LoadWebPage", args, load_url_info);
		}
		else
		{
			++it;
		}
	}
}

void process_load_url(LLMessageSystem* msg, void**)
{
	LLUUID object_id;
	LLUUID owner_id;
	BOOL owner_is_group;
	char object_name[256];		/* Flawfinder: ignore */
	char message[256];		/* Flawfinder: ignore */
	char url[256];		/* Flawfinder: ignore */

	msg->getString("Data", "ObjectName", 256, object_name);
	msg->getUUID(  "Data", "ObjectID", object_id);
	msg->getUUID(  "Data", "OwnerID", owner_id);
	msg->getBOOL(  "Data", "OwnerIsGroup", owner_is_group);
	msg->getString("Data", "Message", 256, message);
	msg->getString("Data", "URL", 256, url);

	LLSD payload;
	payload["object_id"] = object_id;
	payload["owner_id"] = owner_id;
	payload["owner_is_group"] = owner_is_group;
	payload["object_name"] = object_name;
	payload["message"] = message;
	payload["url"] = url;

	// URL is safety checked in load_url above

	// Check if object or owner is muted
	if (LLMuteList::getInstance()->isMuted(object_id, object_name) ||
	    LLMuteList::getInstance()->isMuted(owner_id))
	{
		LL_INFOS("Messaging")<<"Ignoring load_url from muted object/owner."<<LL_ENDL;
		return;
	}

	// Add to list of pending name lookups
	gLoadUrlList.push_back(payload);

	gCacheName->get(owner_id, owner_is_group, callback_load_url_name);
}


void callback_download_complete(void** data, S32 result, LLExtStat ext_status)
{
	std::string* filepath = (std::string*)data;
	LLSD args;
	args["DOWNLOAD_PATH"] = *filepath;
	LLNotifications::instance().add("FinishedRawDownload", args);
	delete filepath;
}


void process_initiate_download(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	if (agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "Initiate download for wrong agent" << LL_ENDL;
		return;
	}

	std::string sim_filename;
	std::string viewer_filename;
	msg->getString("FileData", "SimFilename", sim_filename);
	msg->getString("FileData", "ViewerFilename", viewer_filename);

	if (!gXferManager->validateFileForRequest(viewer_filename))
	{
		llwarns << "SECURITY: Unauthorized download to local file " << viewer_filename << llendl;
		return;
	}
	gXferManager->requestFile(viewer_filename,
		sim_filename,
		LL_PATH_NONE,
		msg->getSender(),
		FALSE,	// don't delete remote
		callback_download_complete,
		(void**)new std::string(viewer_filename));
}


void process_script_teleport_request(LLMessageSystem* msg, void**)
{
	std::string object_name;
	std::string sim_name;
	LLVector3 pos;
	LLVector3 look_at;

	msg->getString("Data", "ObjectName", object_name);
	msg->getString("Data", "SimName", sim_name);
	msg->getVector3("Data", "SimPosition", pos);
	msg->getVector3("Data", "LookAt", look_at);

	gFloaterWorldMap->trackURL(sim_name, (S32)pos.mV[VX], (S32)pos.mV[VY], (S32)pos.mV[VZ]);
	LLFloaterWorldMap::show(NULL, TRUE);

	// remove above two lines and replace with below line
	// to re-enable parcel browser for llMapDestination()
	// LLURLDispatcher::dispatch(LLURLDispatcher::buildSLURL(sim_name, (S32)pos.mV[VX], (S32)pos.mV[VY], (S32)pos.mV[VZ]), FALSE);
	
}

void process_covenant_reply(LLMessageSystem* msg, void**)
{
	LLUUID covenant_id, estate_owner_id;
	std::string estate_name;
	U32 covenant_timestamp;
	msg->getUUID("Data", "CovenantID", covenant_id);
	msg->getU32("Data", "CovenantTimestamp", covenant_timestamp);
	msg->getString("Data", "EstateName", estate_name);
	msg->getUUID("Data", "EstateOwnerID", estate_owner_id);

	LLPanelEstateCovenant::updateEstateName(estate_name);
	LLPanelLandCovenant::updateEstateName(estate_name);
	LLFloaterBuyLand::updateEstateName(estate_name);

	// standard message, not from system
	std::string last_modified;
	if (covenant_timestamp == 0)
	{
		last_modified = LLTrans::getString("covenant_never_modified");
	}
	else
	{
		last_modified = LLTrans::getString("covenant_modified") + " " + formatted_time((time_t)covenant_timestamp);
	}

	LLPanelEstateCovenant::updateLastModified(last_modified);
	LLPanelLandCovenant::updateLastModified(last_modified);
	LLFloaterBuyLand::updateLastModified(last_modified);

	gCacheName->getName(estate_owner_id, callbackCacheEstateOwnerName);
	
	// load the actual covenant asset data
	const BOOL high_priority = TRUE;
	if (covenant_id.notNull())
	{
		gAssetStorage->getEstateAsset(gAgent.getRegionHost(),
									gAgent.getID(),
									gAgent.getSessionID(),
									covenant_id,
                                    LLAssetType::AT_NOTECARD,
									ET_Covenant,
                                    onCovenantLoadComplete,
									NULL,
									high_priority);
	}
	else
	{
		std::string covenant_text;
		if (estate_owner_id.isNull())
		{
			// mainland
			covenant_text = "There is no Covenant provided for this Estate.";
		}
		else
		{
			covenant_text = "There is no Covenant provided for this Estate. The land on this estate is being sold by the Estate owner, not Linden Lab.  Please contact the Estate Owner for sales details.";
		}
		LLPanelEstateCovenant::updateCovenantText(covenant_text, covenant_id);
		LLPanelLandCovenant::updateCovenantText(covenant_text);
		LLFloaterBuyLand::updateCovenantText(covenant_text, covenant_id);
	}
}

void callbackCacheEstateOwnerName(const LLUUID& id,
								  const std::string& first, const std::string& last,
								  BOOL is_group, void*)
{
	std::string name;
	
	if (id.isNull())
	{
		name = "(none)";
	}
	else
	{
		name = first + " " + last;
	}
	LLPanelEstateCovenant::updateEstateOwnerName(name);
	LLPanelLandCovenant::updateEstateOwnerName(name);
	LLFloaterBuyLand::updateEstateOwnerName(name);
}

void onCovenantLoadComplete(LLVFS *vfs,
					const LLUUID& asset_uuid,
					LLAssetType::EType type,
					void* user_data, S32 status, LLExtStat ext_status)
{
	LL_DEBUGS("Messaging") << "onCovenantLoadComplete()" << LL_ENDL;
	std::string covenant_text;
	if(0 == status)
	{
		LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
		
		S32 file_length = file.getSize();
		
		char* buffer = new char[file_length+1];
		if (buffer == NULL)
		{
			LL_ERRS("Messaging") << "Memory Allocation failed" << LL_ENDL;
			return;
		}

		file.read((U8*)buffer, file_length);		/* Flawfinder: ignore */
		
		// put a EOS at the end
		buffer[file_length] = 0;
		
		if( (file_length > 19) && !strncmp( buffer, "Linden text version", 19 ) )
		{
			LLViewerTextEditor* editor =
				new LLViewerTextEditor(std::string("temp"),
						       LLRect(0,0,0,0),
						       file_length+1);
			if( !editor->importBuffer( buffer, file_length+1 ) )
			{
				LL_WARNS("Messaging") << "Problem importing estate covenant." << LL_ENDL;
				covenant_text = "Problem importing estate covenant.";
			}
			else
			{
				// Version 0 (just text, doesn't include version number)
				covenant_text = editor->getText();
			}
			delete editor;
		}
		else
		{
			LL_WARNS("Messaging") << "Problem importing estate covenant: Covenant file format error." << LL_ENDL;
			covenant_text = "Problem importing estate covenant: Covenant file format error.";
		}
		delete[] buffer;
	}
	else
	{
		LLViewerStats::getInstance()->incStat( LLViewerStats::ST_DOWNLOAD_FAILED );
		
		if( LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
		    LL_ERR_FILE_EMPTY == status)
		{
			covenant_text = "Estate covenant notecard is missing from database.";
		}
		else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
		{
			covenant_text = "Insufficient permissions to view estate covenant.";
		}
		else
		{
			covenant_text = "Unable to load estate covenant at this time.";
		}
		
		LL_WARNS("Messaging") << "Problem loading notecard: " << status << LL_ENDL;
	}
	LLPanelEstateCovenant::updateCovenantText(covenant_text, asset_uuid);
	LLPanelLandCovenant::updateCovenantText(covenant_text);
	LLFloaterBuyLand::updateCovenantText(covenant_text, asset_uuid);
}


void process_feature_disabled_message(LLMessageSystem* msg, void**)
{
	// Handle Blacklisted feature simulator response...
	LLUUID	agentID;
	LLUUID	transactionID;
	std::string	messageText;
	msg->getStringFast(_PREHASH_FailureInfo,_PREHASH_ErrorMessage, messageText,0);
	msg->getUUIDFast(_PREHASH_FailureInfo,_PREHASH_AgentID,agentID);
	msg->getUUIDFast(_PREHASH_FailureInfo,_PREHASH_TransactionID,transactionID);
	
	LL_WARNS("Messaging") << "Blacklisted Feature Response:" << messageText << LL_ENDL;
}

// ------------------------------------------------------------
// Message system exception callbacks
// ------------------------------------------------------------

void invalid_message_callback(LLMessageSystem* msg,
								   void*,
								   EMessageException exception)
{
    LLAppViewer::instance()->badNetworkHandler();
}

// Please do not add more message handlers here. This file is huge.
// Put them in a file related to the functionality you are implementing.

void LLOfferInfo::forceResponse(InventoryOfferResponse response)
{
	LLNotification::Params params("UserGiveItem");
	params.functor(boost::bind(&LLOfferInfo::inventory_offer_callback, this, _1, _2));
	LLNotifications::instance().forceResponse(params, response);
}
