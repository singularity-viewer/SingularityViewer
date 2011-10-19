/** 
 * @file llfloatertopobjects.cpp
 * @brief Shows top colliders, top scripts, etc.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfloatertopobjects.h"

#include "message.h"
#include "llfontgl.h"

#include "llagent.h"
#include "llbutton.h"
#include "llfloatergodtools.h"
#include "llfloateravatarinfo.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "llscrolllistctrl.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "lltracker.h"
#include "llviewermessage.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llagentcamera.h"
#include "llviewerobjectlist.h"

void cmdline_printchat(std::string message);

LLFloaterTopObjects* LLFloaterTopObjects::sInstance = NULL;

// Globals
// const U32 TIME_STR_LENGTH = 30;

// static
void LLFloaterTopObjects::show()
{
	if (sInstance)
	{
		sInstance->setVisibleAndFrontmost();
		return;
	}

	sInstance = new LLFloaterTopObjects();
	LLUICtrlFactory::getInstance()->buildFloater(sInstance, "floater_top_objects.xml");
	sInstance->center();
}

LLFloaterTopObjects::LLFloaterTopObjects()
:	LLFloater(std::string("top_objects")),
	mInitialized(FALSE),
	mtotalScore(0.f)
{
	sInstance = this;
}

LLFloaterTopObjects::~LLFloaterTopObjects()
{
	sInstance = NULL;
}

// virtual
BOOL LLFloaterTopObjects::postBuild()
{
	childSetCommitCallback("objects_list", onCommitObjectsList, this);
	childSetDoubleClickCallback("objects_list", onDoubleClickObjectsList);
	childSetFocus("objects_list");
	LLScrollListCtrl *objects_list = getChild<LLScrollListCtrl>("objects_list");
	if (objects_list)
	{
		objects_list->setCommitOnSelectionChange(TRUE);
	}

	childSetAction("show_beacon_btn", onClickShowBeacon, this);
	setDefaultBtn("show_beacon_btn");

	childSetAction("return_selected_btn", onReturnSelected, this);
	childSetAction("return_all_btn", onReturnAll, this);
	childSetAction("disable_selected_btn", onDisableSelected, this);
	childSetAction("disable_all_btn", onDisableAll, this);
	childSetAction("refresh_btn", onRefresh, this);	

	childSetAction("lagwarning", onLagWarningBtn, this);	
	childSetAction("profile", onProfileBtn, this);	
	childSetAction("kick", onKickBtn, this);	
	childSetAction("tpto", onTPBtn, this);	


	childSetAction("filter_object_btn", onGetByObjectNameClicked, this);
	childSetAction("filter_owner_btn", onGetByOwnerNameClicked, this);	


	/*
	LLLineEditor* line_editor = getChild<LLLineEditor>("owner_name_editor");
	if (line_editor)
	{
		line_editor->setCommitOnFocusLost(FALSE);
		line_editor->setCommitCallback(onGetByOwnerName);
		line_editor->setCallbackUserData(this);
	}

	line_editor = getChild<LLLineEditor>("object_name_editor");
	if (line_editor)
	{
		line_editor->setCommitOnFocusLost(FALSE);
		line_editor->setCommitCallback(onGetByObjectName);
		line_editor->setCallbackUserData(this);
	}*/

	mCurrentMode = STAT_REPORT_TOP_SCRIPTS;
	mFlags = 0;
	mFilter.clear();

	return TRUE;
}

void LLFloaterTopObjects::handle_land_reply(LLMessageSystem* msg, void** data)
{
	// Make sure dialog is on screen
	show();
	sInstance->handleReply(msg, data);

	//HACK: for some reason sometimes top scripts originally comes back
	//with no results even though they're there
	if (!sInstance->mObjectListIDs.size() && !sInstance->mInitialized)
	{
		sInstance->onRefresh(NULL);
		sInstance->mInitialized = TRUE;
	}

}

void LLFloaterTopObjects::handleReply(LLMessageSystem *msg, void** data)
{
	U32 request_flags;
	U32 total_count;

	msg->getU32Fast(_PREHASH_RequestData, _PREHASH_RequestFlags, request_flags);
	msg->getU32Fast(_PREHASH_RequestData, _PREHASH_TotalObjectCount, total_count);
	msg->getU32Fast(_PREHASH_RequestData, _PREHASH_ReportType, mCurrentMode);

	LLScrollListCtrl *list = getChild<LLScrollListCtrl>("objects_list");
	
	S32 block_count = msg->getNumberOfBlocks("ReportData");
	for (S32 block = 0; block < block_count; ++block)
	{
		U32 task_local_id;
		U32 time_stamp = 0;
		LLUUID task_id;
		F32 location_x, location_y, location_z;
		F32 score;
		std::string name_buf;
		std::string owner_buf;
		F32 mono_score = 0.f;
		bool have_extended_data = false;
		S32 public_urls = 0;

		msg->getU32Fast(_PREHASH_ReportData, _PREHASH_TaskLocalID, task_local_id, block);
		msg->getUUIDFast(_PREHASH_ReportData, _PREHASH_TaskID, task_id, block);
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_LocationX, location_x, block);
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_LocationY, location_y, block);
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_LocationZ, location_z, block);
		msg->getF32Fast(_PREHASH_ReportData, _PREHASH_Score, score, block);
		msg->getStringFast(_PREHASH_ReportData, _PREHASH_TaskName, name_buf, block);
		msg->getStringFast(_PREHASH_ReportData, _PREHASH_OwnerName, owner_buf, block);
		if(msg->has("DataExtended"))
		{
			have_extended_data = true;
			msg->getU32("DataExtended", "TimeStamp", time_stamp, block);
			msg->getF32("DataExtended", "MonoScore", mono_score, block);
			msg->getS32(_PREHASH_ReportData,"PublicURLs",public_urls,block);
		}

		LLSD element;

		element["id"] = task_id;
		element["object_name"] = name_buf;
		element["owner_name"] = owner_buf;
		element["columns"][0]["column"] = "score";
		element["columns"][0]["value"] = llformat("%0.3f", score);
		element["columns"][0]["font"] = "SANSSERIF";
		
		element["columns"][1]["column"] = "name";
		element["columns"][1]["value"] = name_buf;
		element["columns"][1]["font"] = "SANSSERIF";
		if (name_buf == owner_buf) 
		{
			element["columns"][1]["color"] = LLColor4::red.getValue();
		}
		element["columns"][2]["column"] = "owner";
		element["columns"][2]["value"] = owner_buf;
		element["columns"][2]["font"] = "SANSSERIF";
		element["columns"][3]["column"] = "location";
		element["columns"][3]["value"] = llformat("<%0.1f,%0.1f,%0.1f>", location_x, location_y, location_z);
		element["columns"][3]["font"] = "SANSSERIF";
		element["columns"][4]["column"] = "time";
		element["columns"][4]["value"] = formatted_time((time_t)time_stamp);
		element["columns"][4]["font"] = "SANSSERIF";

		if (mCurrentMode == STAT_REPORT_TOP_SCRIPTS
			&& have_extended_data)
		{
			element["columns"][5]["column"] = "mono_time";
			element["columns"][5]["value"] = llformat("%0.3f", mono_score);
			element["columns"][5]["font"] = "SANSSERIF";

			element["columns"][6]["column"] = "URLs";
			element["columns"][6]["value"] = llformat("%d", public_urls);
			element["columns"][6]["font"] = "SANSSERIF";
		}
		
		list->addElement(element);
		
		mObjectListData.append(element);
		mObjectListIDs.push_back(task_id);

		mtotalScore += score;
	}

	if (total_count == 0 && list->getItemCount() == 0)
	{
		list->addCommentText(getString("none_descriptor"));
	}
	else
	{
		list->selectFirstItem();
	}

	if (mCurrentMode == STAT_REPORT_TOP_SCRIPTS)
	{
		setTitle(getString("top_scripts_title"));
		list->setColumnLabel("score", getString("scripts_score_label"));
		list->setColumnLabel("mono_time", getString("scripts_mono_time_label"));
		
		LLUIString format = getString("top_scripts_text");
		format.setArg("[COUNT]", llformat("%d", total_count));
		format.setArg("[TIME]", llformat("%0.1f", mtotalScore));
		childSetValue("title_text", LLSD(format));
	}
	else
	{
		setTitle(getString("top_colliders_title"));
		list->setColumnLabel("score", getString("colliders_score_label"));
		list->setColumnLabel("mono_time", "");
		LLUIString format = getString("top_colliders_text");
		format.setArg("[COUNT]", llformat("%d", total_count));
		childSetValue("title_text", LLSD(format));
	}
}

// static
void LLFloaterTopObjects::onCommitObjectsList(LLUICtrl* ctrl, void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;

	self->updateSelectionInfo();
}

void LLFloaterTopObjects::updateSelectionInfo()
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");

	if (!list) return;

	LLUUID object_id = list->getCurrentID();
	if (object_id.isNull()) return;

	std::string object_id_string = object_id.asString();

	childSetValue("id_editor", LLSD(object_id_string));
	childSetValue("object_name_editor", list->getFirstSelected()->getColumn(1)->getValue().asString());
	childSetValue("owner_name_editor", list->getFirstSelected()->getColumn(2)->getValue().asString());
}

// static
void LLFloaterTopObjects::onDoubleClickObjectsList(void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	self->showBeacon();
	self->lookAtAvatar();
}

void LLFloaterTopObjects::lookAtAvatar()
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");
	if (!list) return;
	LLScrollListItem* first_selected = list->getFirstSelected();
	if (!first_selected) return;
	LLUUID taskid = first_selected->getUUID();

    LLVOAvatar* voavatar = gObjectList.findAvatar(taskid);
    if(voavatar)
    {
        gAgentCamera.setFocusOnAvatar(FALSE, FALSE);
        gAgentCamera.changeCameraToThirdPerson();
        gAgentCamera.setFocusGlobal(voavatar->getPositionGlobal(),taskid);
        gAgentCamera.setCameraPosAndFocusGlobal(voavatar->getPositionGlobal() 
                + LLVector3d(3.5,1.35,0.75) * voavatar->getRotation(), 
                                                voavatar->getPositionGlobal(), 
                                                taskid );
    }
}

// static
void LLFloaterTopObjects::onClickShowBeacon(void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	if (!self) return;
	self->showBeacon();
}

void LLFloaterTopObjects::doToObjects(int action, bool all)
{
	LLMessageSystem *msg = gMessageSystem;

	LLViewerRegion* region = gAgent.getRegion();
	if (!region) return;

	LLCtrlListInterface *list = childGetListInterface("objects_list");
	if (!list || list->getItemCount() == 0) return;

	std::vector<LLUUID>::iterator id_itor;

	bool start_message = true;

	for (id_itor = mObjectListIDs.begin(); id_itor != mObjectListIDs.end(); ++id_itor)
	{
		LLUUID task_id = *id_itor;
		if (!all && !list->isSelected(task_id))
		{
			// Selected only
			continue;
		}
		if (start_message)
		{
			if (action == ACTION_RETURN)
			{
				msg->newMessageFast(_PREHASH_ParcelReturnObjects);
			}
			else
			{
				msg->newMessageFast(_PREHASH_ParcelDisableObjects);
			}
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
			msg->nextBlockFast(_PREHASH_ParcelData);
			msg->addS32Fast(_PREHASH_LocalID, -1); // Whole region
			msg->addS32Fast(_PREHASH_ReturnType, RT_NONE);
			start_message = false;
		}

		msg->nextBlockFast(_PREHASH_TaskIDs);
		msg->addUUIDFast(_PREHASH_TaskID, task_id);

		if (msg->isSendFullFast(_PREHASH_TaskIDs))
		{
			msg->sendReliable(region->getHost());
			start_message = true;
		}
	}

	if (!start_message)
	{
		msg->sendReliable(region->getHost());
	}
}

//static
bool LLFloaterTopObjects::callbackReturnAll(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		sInstance->doToObjects(ACTION_RETURN, true);
	}
	return false;
}

void LLFloaterTopObjects::onReturnAll(void* data)
{	
	LLNotificationsUtil::add("ReturnAllTopObjects", LLSD(), LLSD(), &callbackReturnAll);
}


void LLFloaterTopObjects::onReturnSelected(void* data)
{
	sInstance->doToObjects(ACTION_RETURN, false);
}

void LLFloaterTopObjects::onLagWarningBtn(void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;

	self->onLagWarning(data);
}

void LLFloaterTopObjects::onLagWarning(void* data)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");
		if (!list) return;
	LLScrollListItem* first_selected = list->getFirstSelected();
	if (!first_selected) return;
	LLUUID taskid = first_selected->getUUID();
	
	std::string name = first_selected->getColumn(1)->getValue().asString();
	std::string score = first_selected->getColumn(0)->getValue().asString();

	std::istringstream stm;
	stm.str(score);
	F32 f_score;
	stm >> f_score;
	F32 percentage = 100.f * (f_score / 22);

	std::string message = llformat(
		"Hello %s, you are receiving this automated message because you are wearing heavily scripted attachments/HUDs, "
		"causing excessive script lag (%5.2f ms, that's ca. %5.2f%% of the region's resources.)\n\n"
		"Please remove resizer scripts or attachments to reduce your script time, thank you.",
		name.c_str(),
		(F32)f_score,
		(F32)percentage
		);

	std::string my_name;
	gAgent.buildFullname(my_name);

	cmdline_printchat(llformat("Script time warning sent to %s: (%5.2f ms)",
		name.c_str(),(F32)f_score));

	send_improved_im(LLUUID(taskid),
				 my_name,
				 message,
				 IM_ONLINE,
				 IM_NOTHING_SPECIAL,
				 LLUUID::null,
				 NO_TIMESTAMP,
				 (U8*)EMPTY_BINARY_BUCKET,
				 EMPTY_BINARY_BUCKET_SIZE);
}

void LLFloaterTopObjects::onProfileBtn(void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	self->onProfile(data);
}

void LLFloaterTopObjects::onProfile(void* data)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");
		if (!list) return;
	LLScrollListItem* first_selected = list->getFirstSelected();
	if (!first_selected) return;
	LLUUID taskid = first_selected->getUUID();
	LLFloaterAvatarInfo::showFromDirectory(taskid);
}

void LLFloaterTopObjects::onKickBtn(void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	self->onKick(data);
}

void LLFloaterTopObjects::onKick(void* data)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");
		if (!list) return;
	LLScrollListItem* first_selected = list->getFirstSelected();
	if (!first_selected) return;
	LLUUID taskid = first_selected->getUUID();

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("EstateOwnerMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock("MethodData");
	msg->addString("Method", "kickestate");
	msg->addUUID("Invoice", LLUUID::null);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", taskid.asString().c_str());
	msg->sendReliable(gAgent.getRegionHost());
}
void LLFloaterTopObjects::onTPBtn(void* data)
{
	LLFloaterTopObjects* self = (LLFloaterTopObjects*)data;
	self->onTP(data);
}

void LLFloaterTopObjects::onTP(void* data)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");
		if (!list) return;
	LLScrollListItem* first_selected = list->getFirstSelected();
	if (!first_selected) return;

	std::string name = first_selected->getColumn(1)->getValue().asString();
	std::string pos_string =  first_selected->getColumn(3)->getValue().asString();

	F32 x, y, z;
	S32 matched = sscanf(pos_string.c_str(), "<%g,%g,%g>", &x, &y, &z);
	if (matched != 3) return;

	LLVector3 pos_agent(x, y, z);
	LLVector3d pos_global = gAgent.getPosGlobalFromAgent(pos_agent);

	gAgent.teleportViaLocation( pos_global );
}



//static
bool LLFloaterTopObjects::callbackDisableAll(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		sInstance->doToObjects(ACTION_DISABLE, true);
	}
	return false;
}

void LLFloaterTopObjects::onDisableAll(void* data)
{
	LLNotificationsUtil::add("DisableAllTopObjects", LLSD(), LLSD(), callbackDisableAll);
}

void LLFloaterTopObjects::onDisableSelected(void* data)
{
	sInstance->doToObjects(ACTION_DISABLE, false);
}

//static
void LLFloaterTopObjects::clearList()
{
	LLCtrlListInterface *list = sInstance->childGetListInterface("objects_list");
	
	if (list) 
	{
		list->operateOnAll(LLCtrlListInterface::OP_DELETE);
	}

	sInstance->mObjectListData.clear();
	sInstance->mObjectListIDs.clear();
	sInstance->mtotalScore = 0.f;
}

//static
void LLFloaterTopObjects::onRefresh(void* data)
{
	U32 mode = STAT_REPORT_TOP_SCRIPTS;
	U32 flags = 0;
	std::string filter = "";

	if (sInstance)
	{
		mode = sInstance->mCurrentMode;
		flags = sInstance->mFlags;
		filter = sInstance->mFilter;
		sInstance->clearList();
	}

	LLMessageSystem *msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_LandStatRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID() );
	msg->nextBlockFast(_PREHASH_RequestData);
	msg->addU32Fast(_PREHASH_ReportType, mode);
	msg->addU32Fast(_PREHASH_RequestFlags, flags);
	msg->addStringFast(_PREHASH_Filter, filter);
	msg->addS32Fast(_PREHASH_ParcelLocalID, 0);

	msg->sendReliable(gAgent.getRegionHost());

	if (sInstance)
	{
		sInstance->mFilter.clear();
		sInstance->mFlags = 0;
	}
}

void LLFloaterTopObjects::onGetByObjectName(LLUICtrl* ctrl, void* data)
{
	if (sInstance)
	{
		sInstance->mFlags = STAT_FILTER_BY_OBJECT;
		sInstance->mFilter = sInstance->childGetText("object_name_editor");
		onRefresh(NULL);
	}
}

void LLFloaterTopObjects::onGetByOwnerName(LLUICtrl* ctrl, void* data)
{
	if (sInstance)
	{
		sInstance->mFlags = STAT_FILTER_BY_OWNER;
		sInstance->mFilter = sInstance->childGetText("owner_name_editor");
		onRefresh(NULL);
	}
}

void LLFloaterTopObjects::showBeacon()
{	
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("objects_list");
	if (!list) return;

	LLScrollListItem* first_selected = list->getFirstSelected();
	if (!first_selected) return;

	std::string name = first_selected->getColumn(1)->getValue().asString();
	std::string pos_string =  first_selected->getColumn(3)->getValue().asString();

	F32 x, y, z;
	S32 matched = sscanf(pos_string.c_str(), "<%g,%g,%g>", &x, &y, &z);
	if (matched != 3) return;

	LLVector3 pos_agent(x, y, z);
	LLVector3d pos_global = gAgent.getPosGlobalFromAgent(pos_agent);
	std::string tooltip("");
	LLTracker::trackLocation(pos_global, name, tooltip, LLTracker::LOCATION_ITEM);
}
