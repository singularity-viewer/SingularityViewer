/**
 * @file llfloaterteleport.cpp
 * @brief floater code for agentd teleports.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Linden Research, Inc.
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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
//Teleport floater used for agent domain TP. URI text floater.
//Copyright International Business Machines Corporation 2008-9
//Contributed to Linden Research, Inc. under the Second Life Viewer Contribution
//Agreement and licensed as above.
#include "llviewerprecompiledheaders.h" // must be first include

#include "llfloaterteleport.h"

#include "llagent.h" //for hack in teleport start
#include "llchat.h"
#include "llcombobox.h"
#include "llfloaterchat.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llsdutil_math.h"
#include "lluictrlfactory.h" // builds floaters from XML
#include "llurlhistory.h"
#include "lluserauth.h"      // for saving placeavatarresponder result
#include "llviewercontrol.h" // for gSavedSettings
#include "llviewerdisplay.h" // for gTeleportDisplay 
#include "llviewermessage.h" // for send_agent_movement_complete attempt
#include "llviewerregion.h"
#include "llviewerwindow.h"  // for hack in teleport start
#include "llvoavatar.h"
#include "llworld.h"
#include "pipeline.h" // for gPipeline


// OGPX HTTP responder for PlaceAvatar cap used for Teleport
// very similar to the responder in Login, but not as many fields are returned in the TP version
// OGPX TODO: should this be combined with the Login responder for rez_avatar/place?
// OGPX TODO: mResult should not get replaced in result(), instead 
//            should replace individual LLSD fields in mResult.
class LLPlaceAvatarTeleportResponder :
	public LLHTTPClient::Responder
{
public:
	LLPlaceAvatarTeleportResponder()
	{
	}

	~LLPlaceAvatarTeleportResponder()
	{
	}
	
	void error(U32 statusNum, const std::string& reason)
	{		
		LL_INFOS("OGPX") << "LLPlaceAvatarTeleportResponder error in TP "
				<< statusNum << " " << reason << LL_ENDL;
				
		LLSD args;
		args["REASON"] = reason;
	
		
		LLNotificationsUtil::add("CouldNotTeleportReason", args);
		
		gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
	
	}

	void result(const LLSD& content)
	{
		
		LLSD result;
		result["agent_id"] = content["agent_id"];  // need this for send_complete_agent_movement
		result["region_x"] = content["region_x"];  // need these for making the first region
		result["region_y"] = content["region_y"];	
		result["login"]    = "true"; // this gets checked in idle_startup()
		result["session_id"] = content["session_id"];
		result["secure_session_id"] = content["secure_session_id"];
		result["circuit_code"] = content["circuit_code"];
		result["sim_port"] = content["sim_port"];
		result["sim_host"] = content["sim_host"];
		result["look_at"] = content["look_at"];
		// maintaining result seed_capability name for compatibility with legacy login
		result["seed_capability"] = content["region_seed_capability"];
		result["position"] = content["position"]; // save this for agentmovementcomplete type processing
		
		// Even though we have the pretty print of the complete content returned, we still find it handy
		// when scanning SecondLife.log to have these laid out in this way. So they are still here.
		LL_DEBUGS("OGPX") << " Teleport placeAvatar responder " << LL_ENDL;
		LL_DEBUGS("OGPX") << "agent_id: " << content["agent_id"] << LL_ENDL;
		LL_DEBUGS("OGPX") << "region_x: " << content["region_x"] << LL_ENDL;
		LL_DEBUGS("OGPX") << "session_id: " << content["session_id"] << LL_ENDL;
		LL_DEBUGS("OGPX") << "sim_port: " << content["sim_port"] << LL_ENDL;
		LL_DEBUGS("OGPX") << "sim_host: " << content["sim_host"] << LL_ENDL;
		LL_DEBUGS("OGPX") << "look_at: " << content["look_at"] << LL_ENDL;
		LL_DEBUGS("OGPX") << "position: " << content["position"] << LL_ENDL;
		LL_DEBUGS("OGPX") << "seed_capability: " << content["region_seed_capability"] << LL_ENDL;

		LL_INFOS("OGPX") << " All the LLSD PlaceAvatarTeleportResponder content: \n " << ll_pretty_print_sd(content) << LL_ENDL; // OGPX

		
		// check "connect" to make sure place_avatar fully successful
		if (!content["connect"].asBoolean()) 
		{
			// place_avatar failed somewhere
			LL_INFOS("OGPX") << "TP failed, connect false in TP PlaceAvatarResponder " << LL_ENDL;
			
			LLSD args;
			args["REASON"] = "Place Avatar Failed";
	
			//gViewerWindow->alertXml("CouldNotTeleportReason", args);
			LLNotificationsUtil::add("CouldNotTeleportReason",args);
			
			gAgent.setTeleportState( LLAgent::TELEPORT_NONE );

			return;
		}
		
		 
		U64 region_handle;
		region_handle = to_region_handle_global(content["region_x"].asInteger(), content["region_y"].asInteger()); 

		LLHost sim_host;
		U32 sim_port = strtoul(result["sim_port"].asString().c_str(), NULL, 10);
        sim_host.setHostByName(result["sim_host"].asString().c_str());
        sim_host.setPort(sim_port);
		
		if (sim_host.isOk())
		{
			LLMessageSystem* msg = gMessageSystem;
			gMessageSystem->enableCircuit(sim_host, TRUE);
			msg->newMessageFast(_PREHASH_UseCircuitCode);
			msg->nextBlockFast(_PREHASH_CircuitCode);
			msg->addU32Fast(_PREHASH_Code, msg->getOurCircuitCode());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->addUUIDFast(_PREHASH_ID, gAgent.getID());
			msg->sendReliable(sim_host);
		}
		else
		{
		    LL_INFOS("OGPX") << "TP failed, could not resolve hostname for UDP messages." << LL_ENDL;
			LLSD args;
			args["REASON"] = "Failed to resolve host.";
			//gViewerWindow->alertXml("CouldNotTeleportReason", args);
			LLNotificationsUtil::add("CouldNotTeleportReason", args);
			gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
			return;		    
		}


		

		// Viewer trusts the simulator.
		LLViewerRegion* regionp =  LLWorld::getInstance()->addRegion(region_handle, sim_host);
		regionp->setSeedCapability(content["seed_capability"].asString().c_str());
		// process_agent_movement_complete needs the region to still be the old region gAgent.setRegion(regionp);

		// placing these in result so they can be set properly in LLUserAuth result
		// ...they are only passed in on login, and not on TP
		result["session_id"] = gAgent.getSessionID();
		result["agent_id"] = gAgent.getID(); 
		result["circuit_code"].asString() = gMessageSystem->mOurCircuitCode; // this is what startup sets, is this proper to do?

		// grab the skeleton and root. 
		result["inventory-skeleton"] = LLUserAuth::getInstance()->mResult["inventory-skeleton"];
		result["inventory-root"] = LLUserAuth::getInstance()->mResult["inventory-root"];
		
		LL_DEBUGS("OGPX") << "session_id: " << result["session_id"] << LL_ENDL;

		

		// results need to be stored so process_agent_movement_complete() can pull them
		LLUserAuth::getInstance()->mAuthResponse = LLUserAuth::E_OK;

		// OGPX TODO: This just reeks of causing problems, because we are using 
		// ... mResult to store things that we get from other caps....So slamming a 
		// ... completely new result in on teleport is going to cause issues. 
		// ... It makes changing the things we save in mResult error prone. 
		// ... Question is, how should we really be storing the seemingly random things
		// ... that we get back from (now) various different caps that used to all come back
		// ... in the result of XMLRPC authenticate? 
		LLUserAuth::getInstance()->mResult = result;



		// ... new sim not sending me much without sending it CompleteAgentMovement msg.
		//gAgent.setTeleportState( LLAgent::TELEPORT_MOVING ); // process_agent_mv_complete looks for TELEPORT_MOVING
		LLVector3 position = ll_vector3_from_sd(result["position"]);
		gAgent.setHomePosRegion(region_handle, position); // taken from teleport_finish (not sure regular code path gets this)

		send_complete_agent_movement(sim_host);

		// Turn off progress msg (also need to do this in all the possible failure places)
		// I think we leave this, as the scene is still changing during the 
		// processing of agentmovementcomeplete message. TELEPORT_NONE resets it anyway
		// gViewerWindow->setShowProgress(FALSE);
		
	}
};

// Statics
LLFloaterTeleport* LLFloaterTeleport::sInstance = NULL;

LLFloaterTeleport::LLFloaterTeleport()
:   LLFloater("floater_teleport")
{
	if(!sInstance)
	{
		LLUICtrlFactory::getInstance()->buildFloater(this, "floater_teleport.xml");
	
		LLComboBox* regioncombo = getChild<LLComboBox>("teleport_edit"); 
		regioncombo->setAllowTextEntry(TRUE, 256, FALSE); // URL bar needs to allow user text input
	
		// iterate on uri list adding to combobox (couldn't figure out how to add them all in one call)
		LLSD regionuri_history = LLURLHistory::getURLHistory("regionuri");
		LLSD::array_iterator iter_history = regionuri_history.beginArray();
		LLSD::array_iterator iter_end = regionuri_history.endArray();
		for(; iter_history != iter_end; ++iter_history)
		{
			regioncombo->addSimpleElement((*iter_history).asString());
		}

		// select which is displayed if we have a current URL.
		regioncombo->setSelectedByValue(LLSD(gSavedSettings.getString("CmdLineRegionURI")),TRUE);

		// TODO : decide if 'enter' when selecting something from the combox box should *not* be sent 
		// to the floater (p.s. and figure out how to change it)
	
		childSetAction("teleport_btn", onClickTeleport, this);
		childSetAction("cancel_btn", onClickCancel, this);

		setDefaultBtn("teleport_btn");
	}
	else
	{
		sInstance->show(NULL);
	}
}

// static
void LLFloaterTeleport::show(void*)
{
    if (!sInstance)
	{
		sInstance = new LLFloaterTeleport();
	}

    sInstance->open();
}

LLFloaterTeleport::~LLFloaterTeleport()
{
    sInstance=NULL;
}



// static
void LLFloaterTeleport::onClickTeleport(void* userdata)
{
	std::string placeAvatarCap = LLAppViewer::instance()->getPlaceAvatarCap();
	LLSD args;
	
    LLFloaterTeleport* self = (LLFloaterTeleport*)userdata;
	std::string text = self->childGetText("teleport_edit");
	if (text.find("://",0) == std::string::npos)
	{
		// if there is no uri, prepend it with http://
		text = "http://"+text;
		LL_DEBUGS("OGPX") << "Teleport URI was prepended, now " << text << LL_ENDL;
	}
	
    LL_DEBUGS("OGPX") << "onClickTeleport! from using place_avatar cap "<< placeAvatarCap << " contains "<< text << LL_ENDL;
	LLStringUtil::trim(text); // trim extra spacing
	gAgent.setTeleportSourceURL(gSavedSettings.getString("CmdLineRegionURI")); // grab src region name
	gSavedSettings.setString("CmdLineRegionURI",text); // save the dst region
	args["public_region_seed_capability"] = text; 
	args["position"] = ll_sd_from_vector3(LLVector3(128, 128, 50)); // default to middle of region above base terrain
	LL_INFOS("OGPX") << " args to placeavatar cap " << placeAvatarCap << " on teleport: " << LLSDOStreamer<LLSDXMLFormatter>(args) << LL_ENDL;
	LLHTTPClient::post(placeAvatarCap, args, new LLPlaceAvatarTeleportResponder());
	gAgent.setTeleportMessage(
				LLAgent::sTeleportProgressMessages["requesting"]);
	gViewerWindow->setShowProgress(TRUE);
	gAgent.teleportCore(); 
	gAgent.setTeleportState( LLAgent::TELEPORT_PLACE_AVATAR ); // teleportcore() sets tp state to legacy path, so reset. ick!
	gTeleportDisplayTimer.reset();

	

	self->setVisible(FALSE);
	if ( LLURLHistory::appendToURLCollection("regionuri",text)) 
	{
		// since URL history only populated on create of sInstance, add to combo list directly
		LLComboBox* regioncombo = self->getChild<LLComboBox>("teleport_edit");
		// BUG : this should add the new item to the combo box, but doesn't
		regioncombo->addSimpleElement(text);
	}
	
}

void LLFloaterTeleport::onClickCancel(void *userdata)
{
	LLFloaterTeleport* self = (LLFloaterTeleport*)userdata;
	LL_INFOS("OGPX") << "Teleport Cancel " << self->getName() << LL_ENDL;
	self->setVisible(FALSE);
}
