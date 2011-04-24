/** 
 * @file llfloaterao.cpp
 * @brief clientside animation overrider
 * by Skills Hak
 */

#include "llviewerprecompiledheaders.h"

#include "floaterao.h"

#include "llagent.h"
#include "llvoavatar.h"
#include "llanimationstates.h"
#include "lluictrlfactory.h"
#include "llinventoryview.h"
#include "llstartup.h"
#include "llpreviewnotecard.h"
#include "llviewertexteditor.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llspinctrl.h"
#include "chatbar_as_cmdline.h"
//#include "llfloaterchat.h"
#include "llfirstuse.h"

#include "llinventory.h"
#include "llinventoryview.h"
#include "roles_constants.h"
#include "llviewerregion.h"

#include "llpanelinventory.h"
#include "llinventorybridge.h"

#include "llboost.h"
#include <boost/regex.hpp>

// Uncomment and use instead if we ever add the chatbar as a command line - MC
void cmdline_printchat(std::string message);

class AONotecardCallback : public LLInventoryCallback
{
public:
	AONotecardCallback(std::string &filename)
	{
		mFileName = filename;
	}

	void fire(const LLUUID &inv_item)
	{
		if (!mFileName.empty())
		{ 
			LLPreviewNotecard* nc;
			nc = (LLPreviewNotecard*)LLPreview::find(inv_item);
			if(nc)
			{
				nc->open();
				LLTextEditor *text = nc->getEditor();
				if (text)
				{
					text->clear();
					text->makePristine();

					std::ifstream file(mFileName.c_str());
				
					std::string line;
					while (!file.eof())
					{ 
						getline(file, line);
						line = line + "\n";
						text->insertText(line);
					}
					file.close();
			
					nc->saveIfNeeded();
				}
			}
		}
	}

private:
	std::string mFileName;
};

AOInvTimer* gAOInvTimer = NULL;


// -------------------------------------------------------

AOStandTimer* mAOStandTimer;

AOStandTimer::AOStandTimer() : LLEventTimer( gSavedSettings.getF32("AOStandInterval") )
{
	AOStandTimer::tick();
}
AOStandTimer::~AOStandTimer()
{
//	llinfos << "dead" << llendl;
}
void AOStandTimer::reset()
{
	mPeriod = gSavedSettings.getF32("AOStandInterval");
	mEventTimer.reset();
//	llinfos << "reset" << llendl;
}
BOOL AOStandTimer::tick()
{
	LLFloaterAO::stand_iterator++;
//	llinfos << "tick" << llendl;
	LLFloaterAO::ChangeStand();
	return FALSE;
//	return LLFloaterAO::ChangeStand(); //timer is always active now ..
}

// -------------------------------------------------------

AOInvTimer::AOInvTimer() : LLEventTimer( (F32)1.0 )
{
}
AOInvTimer::~AOInvTimer()
{
}
BOOL AOInvTimer::tick()
{
	if (!(gSavedSettings.getBOOL("AOEnabled"))) return TRUE;
	if(LLStartUp::getStartupState() >= STATE_INVENTORY_SEND)
	{
		if(gInventory.isEverythingFetched())
		{
//			cmdline_printchat("Inventory fetched, loading AO.");
			LLFloaterAO::init();
			return TRUE;
		}
	}
	return FALSE;
}
// NC DROP -------------------------------------------------------

class AONoteCardDropTarget : public LLView
{
public:
	AONoteCardDropTarget(const std::string& name, const LLRect& rect, void (*callback)(LLViewerInventoryItem*));
	~AONoteCardDropTarget();

	void doDrop(EDragAndDropType cargo_type, void* cargo_data);

	//
	// LLView functionality
	virtual BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);
protected:
	void	(*mDownCallback)(LLViewerInventoryItem*);
};


AONoteCardDropTarget::AONoteCardDropTarget(const std::string& name, const LLRect& rect,
						  void (*callback)(LLViewerInventoryItem*)) :
	LLView(name, rect, NOT_MOUSE_OPAQUE, FOLLOWS_ALL),
	mDownCallback(callback)
{
}

AONoteCardDropTarget::~AONoteCardDropTarget()
{
}

void AONoteCardDropTarget::doDrop(EDragAndDropType cargo_type, void* cargo_data)
{
//	llinfos << "AONoteCardDropTarget::doDrop()" << llendl;
}

BOOL AONoteCardDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data,
									 EAcceptance* accept,
									 std::string& tooltip_msg)
{
	BOOL handled = FALSE;
	if(getParent())
	{
		handled = TRUE;
		LLViewerInventoryItem* inv_item = (LLViewerInventoryItem*)cargo_data;
		if(gInventory.getItem(inv_item->getUUID()))
		{
			*accept = ACCEPT_YES_COPY_SINGLE;
			if(drop)
			{
				mDownCallback(inv_item);
			}
		}
		else
		{
			*accept = ACCEPT_NO;
		}
	}
	return handled;
}

AONoteCardDropTarget * LLFloaterAO::mAOItemDropTarget;


// STUFF -------------------------------------------------------

int LLFloaterAO::mAnimationState = 0;
int LLFloaterAO::stand_iterator = 0;

LLUUID LLFloaterAO::invfolderid = LLUUID::null;
LLUUID LLFloaterAO::mCurrentStandId = LLUUID::null;

LLComboBox* mcomboBox_stands;
LLComboBox* mcomboBox_walks;
LLComboBox* mcomboBox_runs;
LLComboBox* mcomboBox_jumps;
LLComboBox* mcomboBox_sits;
LLComboBox* mcomboBox_gsits;
LLComboBox* mcomboBox_crouchs;
LLComboBox* mcomboBox_cwalks;
LLComboBox* mcomboBox_falls;
LLComboBox* mcomboBox_hovers;
LLComboBox* mcomboBox_flys;
LLComboBox* mcomboBox_flyslows;
LLComboBox* mcomboBox_flyups;
LLComboBox* mcomboBox_flydowns;
LLComboBox* mcomboBox_lands;
LLComboBox* mcomboBox_standups;
LLComboBox* mcomboBox_prejumps;

struct struct_overrides
{
	LLUUID orig_id;
	LLUUID ao_id;
	int state;
};
std::vector<struct_overrides> mAOOverrides;
std::map<LLUUID,struct_overrides> mQuickAOOverrides;

struct struct_stands
{
	LLUUID ao_id;
	std::string anim_name;
};
std::vector<struct_stands> mAOStands;

struct struct_tokens
{
	std::string token;
	int state;
};
std::vector<struct_tokens> mAOTokens;

LLFloaterAO* LLFloaterAO::sInstance = NULL;

LLFloaterAO::LLFloaterAO()
:LLFloater(std::string("floater_ao"))
{
//	init();
	llassert_always(sInstance == NULL);
    LLUICtrlFactory::getInstance()->buildFloater(this, "floater_ao.xml");
	sInstance = this;
}

LLFloaterAO::~LLFloaterAO()
{
    sInstance=NULL;
	mcomboBox_stands = 0;
	mcomboBox_walks = 0;
	mcomboBox_runs = 0;
	mcomboBox_jumps = 0;
	mcomboBox_sits = 0;
	mcomboBox_gsits = 0;
	mcomboBox_crouchs = 0;
	mcomboBox_cwalks = 0;
	mcomboBox_falls = 0;
	mcomboBox_hovers = 0;
	mcomboBox_flys = 0;
	mcomboBox_flyslows = 0;
	mcomboBox_flyups = 0;
	mcomboBox_flydowns = 0;
	mcomboBox_lands = 0;
	mcomboBox_standups = 0;
	mcomboBox_prejumps = 0;
	delete mAOItemDropTarget;
	mAOItemDropTarget = NULL;
//	llinfos << "floater destroyed" << llendl;
}

void LLFloaterAO::show(void*)
{
    if (!sInstance)
	{
		sInstance = new LLFloaterAO();
		updateLayout(sInstance);
		init();

		sInstance->open();
	}
	else
	{
		sInstance->close();
	}
	LLFirstUse::useAO();
}

bool LLFloaterAO::getInstance()
{
	if (sInstance) 
		return true;
	else 
		return false;
}

BOOL LLFloaterAO::postBuild()
{
	LLView *target_view = getChild<LLView>("ao_notecard");
	if(target_view)
	{
		if (mAOItemDropTarget)
		{
			delete mAOItemDropTarget;
		}
		mAOItemDropTarget = new AONoteCardDropTarget("drop target", target_view->getRect(), AOItemDrop);//, mAvatarID);
		addChild(mAOItemDropTarget);
	}
	if(LLStartUp::getStartupState() == STATE_STARTED)
	{
		LLUUID itemidimport = (LLUUID)gSavedPerAccountSettings.getString("AOConfigNotecardID");
		LLViewerInventoryItem* itemimport = gInventory.getItem(itemidimport);
		if(itemimport)
		{
			childSetValue("ao_nc_text","Currently set to: "+itemimport->getName());
		}
		else if(itemidimport.isNull())
		{
			childSetValue("ao_nc_text","Currently not set");
		}
		else
		{
			childSetValue("ao_nc_text","Currently set to a item not on this account");
		}
	}
	else
	{
		childSetValue("ao_nc_text","Not logged in");
	}
	childSetAction("more_btn", onClickMore, this);
	childSetAction("less_btn", onClickLess, this);

	childSetAction("reloadcard",onClickReloadCard,this);
	childSetAction("opencard",onClickOpenCard,this);
	childSetAction("newcard",onClickNewCard,this);
	childSetAction("prevstand",onClickPrevStand,this);
	childSetAction("nextstand",onClickNextStand,this);
	childSetCommitCallback("AOEnabled",onClickToggleAO);
	childSetCommitCallback("AOSitsEnabled",onClickToggleSits);
	childSetCommitCallback("standtime",onSpinnerCommit);
	mcomboBox_stands = getChild<LLComboBox>("stands");
	mcomboBox_walks = getChild<LLComboBox>("walks");
	mcomboBox_runs = getChild<LLComboBox>("runs");
	mcomboBox_jumps = getChild<LLComboBox>("jumps");
	mcomboBox_sits = getChild<LLComboBox>("sits");
	mcomboBox_gsits = getChild<LLComboBox>("gsits");
	mcomboBox_crouchs = getChild<LLComboBox>("crouchs");
	mcomboBox_cwalks = getChild<LLComboBox>("cwalks");
	mcomboBox_falls = getChild<LLComboBox>("falls");
	mcomboBox_hovers = getChild<LLComboBox>("hovers");
	mcomboBox_flys = getChild<LLComboBox>("flys");
	mcomboBox_flyslows = getChild<LLComboBox>("flyslows");
	mcomboBox_flyups = getChild<LLComboBox>("flyups");
	mcomboBox_flydowns = getChild<LLComboBox>("flydowns");
	mcomboBox_lands = getChild<LLComboBox>("lands");
	mcomboBox_standups = getChild<LLComboBox>("standups");
	mcomboBox_prejumps = getChild<LLComboBox>("prejumps");
	getChild<LLComboBox>("stands")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("walks")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("runs")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("jumps")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("sits")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("gsits")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("crouchs")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("cwalks")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("falls")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("hovers")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("flys")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("flyslows")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("flyups")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("flydowns")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("lands")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("standups")->setCommitCallback(onComboBoxCommit);
	getChild<LLComboBox>("prejumps")->setCommitCallback(onComboBoxCommit);

	return TRUE;
}

void LLFloaterAO::onSpinnerCommit(LLUICtrl* ctrl, void* userdata)
{
	LLSpinCtrl* spin = (LLSpinCtrl*) ctrl;
	if(spin)
	{
		if (spin->getName() == "standtime")
		{
			if (mAOStandTimer) mAOStandTimer->reset();
		}
	}
}

void LLFloaterAO::onComboBoxCommit(LLUICtrl* ctrl, void* userdata)
{
	LLComboBox* box = (LLComboBox*)ctrl;
	if(box)
	{
		if (box->getName() == "stands")
		{
			stand_iterator = box->getCurrentIndex();
			cmdline_printchat(llformat("Changing stand to %s.",mAOStands[stand_iterator].anim_name.c_str()));
			ChangeStand();
		}
		else
		{
			int state = STATE_AGENT_IDLE;
			std::string stranim = box->getValue().asString();
//			llinfos << "state " << (gAgent.getAvatarObject()->mIsSitting) << " - " << getAnimationState() << llendl;
			if (box->getName() == "walks")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_WALK), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultWalk",stranim);
				state = STATE_AGENT_WALK;
			}
			else if (box->getName() == "runs")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_RUN), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultRun",stranim);
				state = STATE_AGENT_RUN;
			}
			else if (box->getName() == "jumps")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_JUMP), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultJump",stranim);
				state = STATE_AGENT_JUMP;
			}
			else if (box->getName() == "sits")
			{
				if (gAgent.getAvatarObject() && (gSavedSettings.getBOOL("AOEnabled")) && (gSavedSettings.getBOOL("AOSitsEnabled")))
				{
					if ((gAgent.getAvatarObject()->mIsSitting) && (getAnimationState() == STATE_AGENT_SIT))
					{
//						llinfos << "sitting " << GetAnimID(ANIM_AGENT_SIT) << " " << getAssetIDByName(stranim) << llendl;
						gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_SIT), ANIM_REQUEST_STOP);
						gAgent.sendAnimationRequest(getAssetIDByName(stranim), ANIM_REQUEST_START);
					}
				}
				gSavedPerAccountSettings.setString("AODefaultSit",stranim);
				state = STATE_AGENT_SIT;
			}
			else if (box->getName() == "gsits")
			{
//				llinfos << "gsitting " << GetAnimID(ANIM_AGENT_SIT_GROUND) << " " << getAssetIDByName(stranim) << llendl;
				if (gAgent.getAvatarObject())
				{
					if ((gAgent.getAvatarObject()->mIsSitting) && (getAnimationState() == STATE_AGENT_GROUNDSIT))
					{
//						llinfos << "gsitting " << GetAnimID(ANIM_AGENT_SIT_GROUND) << " " << getAssetIDByName(stranim) << llendl;
						gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_SIT_GROUND), ANIM_REQUEST_STOP);
						gAgent.sendAnimationRequest(getAssetIDByName(stranim), ANIM_REQUEST_START);
					}
				}
				gSavedPerAccountSettings.setString("AODefaultGroundSit",stranim);
				state = STATE_AGENT_GROUNDSIT;
			}
			else if (box->getName() == "crouchs")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_CROUCH), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultCrouch",stranim);
				state = STATE_AGENT_CROUCH;
			}
			else if (box->getName() == "cwalks")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_CROUCHWALK), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultCrouchWalk",stranim);
				state = STATE_AGENT_CROUCHWALK;
			}
			else if (box->getName() == "falls")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_FALLDOWN), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultFall",stranim);
				state = STATE_AGENT_FALLDOWN;
			}
			else if (box->getName() == "hovers")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_HOVER), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultHover",stranim);
				state = STATE_AGENT_HOVER;
			}
			else if (box->getName() == "flys")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_FLY), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultFly",stranim);
				state = STATE_AGENT_FLY;
			}
			else if (box->getName() == "flyslows")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_FLYSLOW), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultFlySlow",stranim);
				state = STATE_AGENT_FLYSLOW;
			}
			else if (box->getName() == "flyups")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_HOVER_UP), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultFlyUp",stranim);
				state = STATE_AGENT_HOVER_UP;
			}
			else if (box->getName() == "flydowns")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_HOVER_DOWN), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultFlyDown",stranim);
				state = STATE_AGENT_HOVER_DOWN;
			}
			else if (box->getName() == "lands")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_LAND), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultLand",stranim);
				state = STATE_AGENT_LAND;
			}
			else if (box->getName() == "standups")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_STAND), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultStandUp",stranim);
				state = STATE_AGENT_STAND;
			}
			else if (box->getName() == "prejumps")
			{
				gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_PRE_JUMP), ANIM_REQUEST_STOP);
				gSavedPerAccountSettings.setString("AODefaultPreJump",stranim);
				state = STATE_AGENT_PRE_JUMP;
			}
			for (std::vector<struct_overrides>::iterator iter = mAOOverrides.begin(); iter != mAOOverrides.end(); ++iter)
			{
				if (state == iter->state)
				{
					LLUUID anim_uuid = getAssetIDByName(stranim);
					iter->ao_id = anim_uuid;
					mQuickAOOverrides[iter->orig_id].ao_id = anim_uuid;
				}
			}
		}
	}
}

void LLFloaterAO::updateLayout(LLFloaterAO* floater)
{
	if (floater)
	{
		BOOL advanced = gSavedSettings.getBOOL( "AOAdvanced");
		if (advanced)
		{
			floater->reshape(610,380); //view->getRect().getWidth(), view->getUIWinHeightLong());
		}
		else
		{
			floater->reshape(200,380); //view->getRect().getWidth(), view->getUIWinHeightShort());
		}
		
		floater->childSetVisible("more_btn", !advanced);
		floater->childSetVisible("less_btn", advanced);

		floater->childSetVisible("tabcontainer", advanced);
		floater->childSetVisible("tabdefaultanims", advanced);

		floater->childSetVisible("textdefaultwalk", advanced);
		floater->childSetVisible("textdefaultrun", advanced);
		floater->childSetVisible("textdefaultjump", advanced);
		floater->childSetVisible("textdefaultsit", advanced);
		floater->childSetVisible("textdefaultgsit", advanced);
		floater->childSetVisible("textdefaultcrouch", advanced);
		floater->childSetVisible("textdefaultcrouchwalk", advanced);
		floater->childSetVisible("textdefaultfall", advanced);
		floater->childSetVisible("textdefaulthover", advanced);
		floater->childSetVisible("textdefaultfly", advanced);
		floater->childSetVisible("textdefaultflyslow", advanced);
		floater->childSetVisible("textdefaultflyup", advanced);
		floater->childSetVisible("textdefaultflydown", advanced);
		floater->childSetVisible("textdefaultland", advanced);
		floater->childSetVisible("textdefaultstandup", advanced);
		floater->childSetVisible("textdefaultprejump", advanced);


		floater->childSetVisible("walks", advanced);
		floater->childSetVisible("runs", advanced);
		floater->childSetVisible("jumps", advanced);
		floater->childSetVisible("sits", advanced);
		floater->childSetVisible("gsits", advanced);
		floater->childSetVisible("crouchs", advanced);
		floater->childSetVisible("crouchwalks", advanced);
		floater->childSetVisible("falls", advanced);
		floater->childSetVisible("hovers", advanced);
		floater->childSetVisible("flys", advanced);
		floater->childSetVisible("flyslows", advanced);
		floater->childSetVisible("flyups", advanced);
		floater->childSetVisible("flydowns", advanced);
		floater->childSetVisible("lands", advanced);
		floater->childSetVisible("standups", advanced);
		floater->childSetVisible("prejumps", advanced);
	}
}

void LLFloaterAO::init()
{
	mAOStands.clear();
	mAOTokens.clear();
	mAOOverrides.clear();
	mQuickAOOverrides.clear();

	struct_tokens tokenloader;
	tokenloader.token = 
	tokenloader.token = "[ Sitting On Ground ]";	tokenloader.state = STATE_AGENT_GROUNDSIT; mAOTokens.push_back(tokenloader);    // 0
	tokenloader.token = "[ Sitting ]";				tokenloader.state = STATE_AGENT_SIT; mAOTokens.push_back(tokenloader);              // 1
	tokenloader.token = "[ Crouching ]";			tokenloader.state = STATE_AGENT_CROUCH; mAOTokens.push_back(tokenloader);            // 3
	tokenloader.token = "[ Crouch Walking ]";		tokenloader.state = STATE_AGENT_CROUCHWALK; mAOTokens.push_back(tokenloader);       // 4
	tokenloader.token = "[ Standing Up ]";			tokenloader.state = STATE_AGENT_STANDUP; mAOTokens.push_back(tokenloader);          // 6
	tokenloader.token = "[ Falling ]";				tokenloader.state = STATE_AGENT_FALLDOWN; mAOTokens.push_back(tokenloader);              // 7
	tokenloader.token = "[ Flying Down ]";			tokenloader.state = STATE_AGENT_HOVER_DOWN; mAOTokens.push_back(tokenloader);          // 8
	tokenloader.token = "[ Flying Up ]";			tokenloader.state = STATE_AGENT_HOVER_UP; mAOTokens.push_back(tokenloader);            // 9
	tokenloader.token = "[ Flying Slow ]";			tokenloader.state = STATE_AGENT_FLYSLOW; mAOTokens.push_back(tokenloader);          // 10
	tokenloader.token = "[ Flying ]";				tokenloader.state = STATE_AGENT_FLY; mAOTokens.push_back(tokenloader);               // 11
	tokenloader.token = "[ Hovering ]";				tokenloader.state = STATE_AGENT_HOVER; mAOTokens.push_back(tokenloader);             // 12
	tokenloader.token = "[ Jumping ]";				tokenloader.state = STATE_AGENT_JUMP; mAOTokens.push_back(tokenloader);              // 13
	tokenloader.token = "[ Pre Jumping ]";			tokenloader.state = STATE_AGENT_PRE_JUMP; mAOTokens.push_back(tokenloader);          // 14
	tokenloader.token = "[ Running ]";				tokenloader.state = STATE_AGENT_RUN; mAOTokens.push_back(tokenloader);              // 15
	tokenloader.token = "[ Turning Right ]";		tokenloader.state = STATE_AGENT_TURNRIGHT; mAOTokens.push_back(tokenloader);        // 16
	tokenloader.token = "[ Turning Left ]";			tokenloader.state = STATE_AGENT_TURNLEFT; mAOTokens.push_back(tokenloader);         // 17
	tokenloader.token = "[ Walking ]";				tokenloader.state = STATE_AGENT_WALK; mAOTokens.push_back(tokenloader);              // 18
	tokenloader.token = "[ Landing ]";				tokenloader.state = STATE_AGENT_LAND; mAOTokens.push_back(tokenloader);              // 19
	tokenloader.token = "[ Standing ]";				tokenloader.state = STATE_AGENT_STAND; mAOTokens.push_back(tokenloader);             // 20
	tokenloader.token = "[ Swimming Down ]";		tokenloader.state = 999; mAOTokens.push_back(tokenloader);        // 21
	tokenloader.token = "[ Swimming Up ]";			tokenloader.state = 999; mAOTokens.push_back(tokenloader);          // 22
	tokenloader.token = "[ Swimming Forward ]";		tokenloader.state = 999; mAOTokens.push_back(tokenloader);     // 23
	tokenloader.token = "[ Floating ]";				tokenloader.state = 999; mAOTokens.push_back(tokenloader);             // 24

	struct_overrides overrideloader;
	overrideloader.orig_id = ANIM_AGENT_WALK;					overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_WALK;			mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_RUN;					overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_RUN;			mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_PRE_JUMP;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_PRE_JUMP;		mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_JUMP;					overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_JUMP;			mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_TURNLEFT;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_TURNLEFT;		mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_TURNRIGHT;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_TURNRIGHT;		mAOOverrides.push_back(overrideloader);

	overrideloader.orig_id = ANIM_AGENT_SIT;					overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_SIT;			mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_SIT_FEMALE;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_SIT;			mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_SIT_GENERIC;			overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_SIT;			mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_SIT_GROUND;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_GROUNDSIT;		mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_SIT_GROUND_CONSTRAINED;	overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_GROUNDSIT;		mAOOverrides.push_back(overrideloader);

	overrideloader.orig_id = ANIM_AGENT_HOVER;					overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_HOVER;			mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_HOVER_DOWN;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_HOVER_DOWN;		mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_HOVER_UP;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_HOVER_UP;		mAOOverrides.push_back(overrideloader);

	overrideloader.orig_id = ANIM_AGENT_CROUCH;					overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_CROUCH;			mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_CROUCHWALK;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_CROUCHWALK;		mAOOverrides.push_back(overrideloader);

	overrideloader.orig_id = ANIM_AGENT_FALLDOWN;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_FALLDOWN;		mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_STANDUP;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_STANDUP;		mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_LAND;					overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_LAND;			mAOOverrides.push_back(overrideloader);

	overrideloader.orig_id = ANIM_AGENT_FLY;					overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_FLY;			mAOOverrides.push_back(overrideloader);
	overrideloader.orig_id = ANIM_AGENT_FLYSLOW;				overrideloader.ao_id = LLUUID::null; overrideloader.state = STATE_AGENT_FLYSLOW;		mAOOverrides.push_back(overrideloader);

	for(std::vector<struct_overrides>::iterator iter = mAOOverrides.begin(); iter != mAOOverrides.end(); iter++)
	{
		mQuickAOOverrides[iter->orig_id] = (*iter);
	}

	BOOL success = TRUE;

	if(LLStartUp::getStartupState() >= STATE_INVENTORY_SEND)
	{
		if(gInventory.isEverythingFetched())
		{
			LLUUID configncitem = (LLUUID)gSavedPerAccountSettings.getString("AOConfigNotecardID");
			if (configncitem.notNull())
			{
				success = FALSE;
				const LLInventoryItem* item = gInventory.getItem(configncitem);
				if(item)
				{
					if (gAgent.allowOperation(PERM_COPY, item->getPermissions(),GP_OBJECT_MANIPULATE) || gAgent.isGodlike())
					{
						if(!item->getAssetUUID().isNull())
						{
							LLUUID* new_uuid = new LLUUID(configncitem);
							LLHost source_sim = LLHost::invalid;
							invfolderid = item->getParentUUID();
							gAssetStorage->getInvItemAsset(source_sim,
															gAgent.getID(),
															gAgent.getSessionID(),
															item->getPermissions().getOwner(),
															LLUUID::null,
															item->getUUID(),
															item->getAssetUUID(),
															item->getType(),
															&onNotecardLoadComplete,
															(void*)new_uuid,
															TRUE);
							success = TRUE;
						}
					}
				}
			}
		}
	}

	if (!success)
	{
		cmdline_printchat("Could not read the specified Config Notecard");
	}

//	mAnimationState = 0;
//	mCurrentStandId = LLUUID::null;
//	setAnimationState(STATE_AGENT_IDLE);

}

void LLFloaterAO::onClickMore(void* data)
{
	gSavedSettings.setBOOL( "AOAdvanced", TRUE );
	updateLayout(sInstance);
}
void LLFloaterAO::onClickLess(void* data)
{
	gSavedSettings.setBOOL( "AOAdvanced", FALSE );
	updateLayout(sInstance);
}

void LLFloaterAO::onClickToggleAO(LLUICtrl *, void*)
{
	run();
}

void LLFloaterAO::onClickToggleSits(LLUICtrl *, void*)
{
	run();
}


void LLFloaterAO::run()
{
	setAnimationState(STATE_AGENT_IDLE); // reset state
	int state = getAnimationState(); // check if sitting or hovering
	if ((state == STATE_AGENT_IDLE) || (state == STATE_AGENT_STAND))
	{
		if (gSavedSettings.getBOOL("AOEnabled"))
		{
			if (mAOStandTimer)
			{
				mAOStandTimer->reset();
				ChangeStand();
			}
			else
			{
				mAOStandTimer =	new AOStandTimer();
			}
		}
		else
		{
			stopMotion(getCurrentStandId(), FALSE, TRUE); //stop stand first then set state
			setAnimationState(STATE_AGENT_IDLE);
		}
	}
	else
	{
		if (state == STATE_AGENT_SIT) gAgent.sendAnimationRequest(GetAnimIDFromState(state), (gSavedSettings.getBOOL("AOEnabled") && gSavedSettings.getBOOL("AOSitsEnabled")) ? ANIM_REQUEST_START : ANIM_REQUEST_STOP);
		else gAgent.sendAnimationRequest(GetAnimIDFromState(state), gSavedSettings.getBOOL("AOEnabled") ? ANIM_REQUEST_START : ANIM_REQUEST_STOP);
	}
}

int LLFloaterAO::getAnimationState()
{
	if (gAgent.getAvatarObject())
	{
		if (gAgent.getAvatarObject()->mIsSitting) setAnimationState(STATE_AGENT_SIT);
		else if (gAgent.getFlying()) setAnimationState(STATE_AGENT_HOVER);
	}
	return mAnimationState;
}

void LLFloaterAO::setAnimationState(const int state)
{
	mAnimationState = state;
}

LLUUID LLFloaterAO::getCurrentStandId()
{
	return mCurrentStandId;
}

void LLFloaterAO::setCurrentStandId(const LLUUID& id)
{
	mCurrentStandId = id;
}

void LLFloaterAO::AOItemDrop(LLViewerInventoryItem* item)
{
	gSavedPerAccountSettings.setString("AOConfigNotecardID", item->getUUID().asString());
	sInstance->childSetValue("ao_nc_text","Currently set to: "+item->getName());
}

LLUUID LLFloaterAO::GetAnimID(const LLUUID& id)
{
	std::map<LLUUID,struct_overrides>::iterator iter = mQuickAOOverrides.find(id);
	if(iter != mQuickAOOverrides.end())
	{
		return iter->second.ao_id;
	}
	return LLUUID::null;
}

int LLFloaterAO::GetStateFromAnimID(const LLUUID& id)
{
	std::map<LLUUID,struct_overrides>::iterator iter = mQuickAOOverrides.find(id);
	if(iter != mQuickAOOverrides.end())
	{
		return iter->second.state;
	}
	return STATE_AGENT_IDLE;
}

LLUUID LLFloaterAO::GetAnimIDFromState(const int state)
{
	for (std::vector<struct_overrides>::iterator iter = mAOOverrides.begin(); iter != mAOOverrides.end(); ++iter)
	{
		if (iter->state == state) return iter->ao_id;
	}
	return LLUUID::null;
}

int LLFloaterAO::GetStateFromToken(std::string strtoken)
{
	for (std::vector<struct_tokens>::iterator iter = mAOTokens.begin(); iter != mAOTokens.end(); ++iter)
	{
		if (iter->token == strtoken) return iter->state;
	}
	return STATE_AGENT_IDLE;
}

void LLFloaterAO::onClickPrevStand(void* user_data)
{
	if (!(mAOStands.size() > 0)) return;
	stand_iterator=stand_iterator-1;
	if (stand_iterator < 0) stand_iterator = int( mAOStands.size()-stand_iterator);
	if (stand_iterator > int( mAOStands.size()-1)) stand_iterator = 0;
	cmdline_printchat(llformat("Changing stand to %s.",mAOStands[stand_iterator].anim_name.c_str()));
	ChangeStand();
}

void LLFloaterAO::onClickNextStand(void* user_data)
{
	if (!(mAOStands.size() > 0)) return;
	stand_iterator=stand_iterator+1;
	if (stand_iterator < 0) stand_iterator = int( mAOStands.size()-stand_iterator);
	if (stand_iterator > int( mAOStands.size()-1)) stand_iterator = 0;
	cmdline_printchat(llformat("Changing stand to %s.",mAOStands[stand_iterator].anim_name.c_str()));
	ChangeStand();
}

BOOL LLFloaterAO::ChangeStand()
{
	if (gSavedSettings.getBOOL("AOEnabled"))
	{
		if (gAgent.getAvatarObject())
		{
			if (gSavedSettings.getBOOL("AONoStandsInMouselook") && gAgent.cameraMouselook()) return FALSE;

			if (gAgent.getAvatarObject()->mIsSitting)
			{
//				stopMotion(getCurrentStandId(), FALSE, TRUE); //stop stand first then set state
//				if (getAnimationState() != STATE_AGENT_GROUNDSIT) setAnimationState(STATE_AGENT_SIT);
//				setCurrentStandId(LLUUID::null);
				return FALSE;
			}
		}
		if ((getAnimationState() == STATE_AGENT_IDLE) || (getAnimationState() == STATE_AGENT_STAND))// stands have lowest priority
		{
			if (!(mAOStands.size() > 0)) return TRUE;
			if (gSavedSettings.getBOOL("AOStandRandomize"))
			{
				stand_iterator = ll_rand(mAOStands.size()-1);
			}
			if (stand_iterator < 0) stand_iterator = int( mAOStands.size()-stand_iterator);
			if (stand_iterator > int( mAOStands.size()-1)) stand_iterator = 0;

			int stand_iterator_previous = stand_iterator -1;

			if (stand_iterator_previous < 0) stand_iterator_previous = int( mAOStands.size()-1);
			
			if (mAOStands[stand_iterator].ao_id.notNull())
			{
				stopMotion(getCurrentStandId(), FALSE, TRUE); //stop stand first then set state
				startMotion(mAOStands[stand_iterator].ao_id, 0, TRUE);

				setAnimationState(STATE_AGENT_STAND);
				setCurrentStandId(mAOStands[stand_iterator].ao_id);
				if ((sInstance)&&(mcomboBox_stands)) mcomboBox_stands->selectNthItem(stand_iterator);
//				llinfos << "changing stand to " << mAOStands[stand_iterator].anim_name << llendl;
				return FALSE;
			}
		}
	} 
	else
	{
		stopMotion(getCurrentStandId(), FALSE, TRUE);
		return TRUE; //stop if ao is off
	}
	return TRUE;
}


BOOL LLFloaterAO::startMotion(const LLUUID& id, F32 time_offset, BOOL stand)
{
	if (stand)
	{
		if (id.notNull())
		{
			BOOL sitting = FALSE;
			if (gAgent.getAvatarObject())
			{
				sitting = gAgent.getAvatarObject()->mIsSitting;
			}
			if (sitting) return FALSE;
			gAgent.sendAnimationRequest(id, ANIM_REQUEST_START);
			return TRUE;
		}
	}
	else
	{
		if (GetAnimID(id).notNull() && gSavedSettings.getBOOL("AOEnabled"))
		{
			stopMotion(getCurrentStandId(), FALSE, TRUE); //stop stand first then set state 
			setAnimationState(GetStateFromAnimID(id));
		
//			llinfos << " state " << getAnimationState() << " start anim " << id << " overriding with " << GetAnimID(id) << llendl;
			if ((GetStateFromAnimID(id) == STATE_AGENT_SIT) && !(gSavedSettings.getBOOL("AOSitsEnabled"))) return TRUE;
			gAgent.sendAnimationRequest(GetAnimID(id), ANIM_REQUEST_START);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL LLFloaterAO::stopMotion(const LLUUID& id, BOOL stop_immediate, BOOL stand)
{	
	if (stand)
	{
		setAnimationState(STATE_AGENT_IDLE);
		gAgent.sendAnimationRequest(id, ANIM_REQUEST_STOP);
		return TRUE;
	}
	else
	{
		if (GetAnimID(id).notNull() && gSavedSettings.getBOOL("AOEnabled"))
		{
//			llinfos << "  state " << getAnimationState() << "/" << GetStateFromAnimID(id) << "(now 0)  stop anim " << id << " overriding with " << GetAnimID(id) << llendl;
			if (getAnimationState() == GetStateFromAnimID(id))
			{
				setAnimationState(STATE_AGENT_IDLE);
			}
			ChangeStand(); // startMotion(getCurrentStandId(), 0, TRUE);
			gAgent.sendAnimationRequest(GetAnimID(id), ANIM_REQUEST_STOP);
			return TRUE;
		}
	}
	return FALSE;
}

void LLFloaterAO::onClickReloadCard(void* user_data)
{
	if(gInventory.isEverythingFetched())
	{
		LLFloaterAO::init();
	}
}

void LLFloaterAO::onClickOpenCard(void* user_data)
{
	if(gInventory.isEverythingFetched())
	{
		LLUUID configncitem = (LLUUID)gSavedPerAccountSettings.getString("AOConfigNotecardID");
		if (configncitem.notNull())
		{
			const LLInventoryItem* item = gInventory.getItem(configncitem);
			if(item)
			{
				if (gAgent.allowOperation(PERM_COPY, item->getPermissions(),GP_OBJECT_MANIPULATE) || gAgent.isGodlike())
				{
					if(!item->getAssetUUID().isNull())
					open_notecard((LLViewerInventoryItem*)item, std::string("Note: ") + item->getName(), LLUUID::null, FALSE);
	//				open_notecard((LLViewerInventoryItem*)item, std::string("Note: ") + item->getName(), LLUUID::null, FALSE, LLUUID::null, FALSE);
				}
			}
		}
	}
}

void LLFloaterAO::onClickNewCard(void* user_data)
{
	// load the template file from app_settings/ao_template.ini then
	// create a new properly-formatted notecard in the user's inventory
	std::string ao_template = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "ao_template.ini");
	if (!ao_template.empty())
	{
		LLPointer<LLInventoryCallback> cb = new AONotecardCallback(ao_template);
		create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
							LLUUID::null, LLTransactionID::tnull, "New AO Notecard", 
							"Drop this notecard in your AO window to use", LLAssetType::AT_NOTECARD,
							LLInventoryType::IT_NOTECARD, NOT_WEARABLE, PERM_ALL, cb);
	}
	else
	{
		llwarns << "Can't find ao_template.ini in app_settings!" << llendl;
	}	
}

struct AOAssetInfo
{
	std::string path;
	std::string name;
};

void LLFloaterAO::onNotecardLoadComplete(LLVFS *vfs,const LLUUID& asset_uuid,LLAssetType::EType type,void* user_data, S32 status, LLExtStat ext_status)
{
	if(status == LL_ERR_NOERR)
	{
		S32 size = vfs->getSize(asset_uuid, type);
		U8* buffer = new U8[size];
		vfs->getData(asset_uuid, type, buffer, 0, size);

		if(type == LLAssetType::AT_NOTECARD)
		{
			LLViewerTextEditor* edit = new LLViewerTextEditor("",LLRect(0,0,0,0),S32_MAX,"");
			if(edit->importBuffer((char*)buffer, (S32)size))
			{
				llinfos << "ao nc decode success" << llendl;
				std::string card = edit->getText();
				edit->die();

				if (mcomboBox_stands)
				{
					mcomboBox_stands->clear();
					mcomboBox_stands->removeall();
				}
				if (mcomboBox_walks) mcomboBox_walks->clear();
				if (mcomboBox_runs) mcomboBox_runs->clear();
				if (mcomboBox_jumps) mcomboBox_jumps->clear();
				if (mcomboBox_sits) mcomboBox_sits->clear();
				if (mcomboBox_gsits) mcomboBox_gsits->clear();
				if (mcomboBox_crouchs) mcomboBox_cwalks->clear();
				if (mcomboBox_cwalks) mcomboBox_cwalks->clear();
				if (mcomboBox_falls) mcomboBox_falls->clear();
				if (mcomboBox_hovers) mcomboBox_hovers->clear();
				if (mcomboBox_flys) mcomboBox_flys->clear();
				if (mcomboBox_flyslows) mcomboBox_flyslows->clear();
				if (mcomboBox_flyups) mcomboBox_flyups->clear();
				if (mcomboBox_flydowns) mcomboBox_flydowns->clear();
				if (mcomboBox_lands) mcomboBox_lands->clear();
				if (mcomboBox_standups) mcomboBox_standups->clear();
				if (mcomboBox_prejumps) mcomboBox_prejumps->clear();


				struct_stands loader;

				typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
				boost::char_separator<char> sep("\n");
				tokenizer tokline(card, sep);

				for (tokenizer::iterator line = tokline.begin(); line != tokline.end(); ++line)
				{
//					llinfos << *line << llendl;
					std::string strline(*line);
//					llinfos << "uncommented line: " << strline << llendl;

					boost::regex type("^(\\s*)(\\[ )(.*)( \\])");
					boost::smatch what; 
					if (boost::regex_search(strline, what, type)) 
					{
//						llinfos << "type: " << what[0] << llendl;
//						llinfos << "anims in type: " << boost::regex_replace(strline, type, "") << llendl;

						boost::char_separator<char> sep("|,");
						std::string stranimnames(boost::regex_replace(strline, type, ""));
						tokenizer tokanimnames(stranimnames, sep);
						for (tokenizer::iterator anim = tokanimnames.begin(); anim != tokanimnames.end(); ++anim)
						{
							std::string strtoken(what[0]);
							std::string stranim(*anim);
							LLUUID animid(getAssetIDByName(stranim));

//							llinfos << invfolderid.asString().c_str() << llendl;
//							llinfos << "anim: " << stranim.c_str() << " assetid: " << animid << llendl;
							if (!(animid.notNull()))
							{
								cmdline_printchat(llformat("Warning: animation '%s' could not be found (Section: %s).",stranim.c_str(),strtoken.c_str()));
							}
							else
							{
								switch(GetStateFromToken(strtoken.c_str()))
								{
								case STATE_AGENT_STAND:
										loader.ao_id = animid; loader.anim_name = stranim.c_str(); mAOStands.push_back(loader);
										if(mcomboBox_stands != NULL) mcomboBox_stands->add(stranim.c_str(), ADD_BOTTOM, TRUE);
										break;
								case STATE_AGENT_WALK:
										{
											if (sInstance && (mcomboBox_walks != NULL))
											{
												//llinfos << "1 anim: " << stranim.c_str() << " assetid: " << animid << llendl;
												if (!(mcomboBox_walks->selectByValue(stranim.c_str()))) mcomboBox_walks->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										}
										break;
								case STATE_AGENT_RUN:
										{
											if (sInstance && (mcomboBox_runs != NULL))
											{
												if (!(mcomboBox_runs->selectByValue(stranim.c_str()))) mcomboBox_runs->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										}
										break;
								case STATE_AGENT_JUMP:
										 {
											if (sInstance && (mcomboBox_jumps != NULL))
											{
												if (!(mcomboBox_jumps->selectByValue(stranim.c_str()))) mcomboBox_jumps->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_SIT:
										 {
											if (sInstance && (mcomboBox_sits != NULL))
											{
												if (!(mcomboBox_sits->selectByValue(stranim.c_str()))) mcomboBox_sits->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_GROUNDSIT:
										 {
											if (sInstance && (mcomboBox_gsits != NULL))
											{
												if (!(mcomboBox_gsits->selectByValue(stranim.c_str()))) mcomboBox_gsits->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_CROUCH:
										 {
											if (sInstance && (mcomboBox_crouchs != NULL))
											{
												if (!(mcomboBox_crouchs->selectByValue(stranim.c_str()))) mcomboBox_crouchs->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_CROUCHWALK:
										 {
											if (sInstance && (mcomboBox_cwalks != NULL))
											{
												if (!(mcomboBox_cwalks->selectByValue(stranim.c_str()))) mcomboBox_cwalks->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_FALLDOWN:
										 {
											if (sInstance && (mcomboBox_falls != NULL))
											{
												if (!(mcomboBox_falls->selectByValue(stranim.c_str()))) mcomboBox_falls->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_HOVER:
										 {
											if (sInstance && (mcomboBox_hovers != NULL))
											{
												if (!(mcomboBox_hovers->selectByValue(stranim.c_str()))) mcomboBox_hovers->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_FLY:
										 {
											if (sInstance && (mcomboBox_flys != NULL))
											{
												if (!(mcomboBox_flys->selectByValue(stranim.c_str()))) mcomboBox_flys->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_FLYSLOW:
										 {
											if (sInstance && (mcomboBox_flyslows != NULL))
											{
												if (!(mcomboBox_flyslows->selectByValue(stranim.c_str()))) mcomboBox_flyslows->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_HOVER_UP:
										 {
											if (sInstance && (mcomboBox_flyups != NULL))
											{
												if (!(mcomboBox_flyups->selectByValue(stranim.c_str()))) mcomboBox_flyups->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										break;
								case STATE_AGENT_HOVER_DOWN:
										 {
											if (sInstance && (mcomboBox_flydowns != NULL))
											{
												if (!(mcomboBox_flydowns->selectByValue(stranim.c_str()))) mcomboBox_flydowns->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_LAND:
										 {
											if (sInstance && (mcomboBox_lands != NULL))
											{
												if (!(mcomboBox_lands->selectByValue(stranim.c_str()))) mcomboBox_lands->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_STANDUP:
										 {
											if (sInstance && (mcomboBox_standups != NULL))
											{
												if (!(mcomboBox_standups->selectByValue(stranim.c_str()))) mcomboBox_standups->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								case STATE_AGENT_PRE_JUMP:
										 {
											if (sInstance && (mcomboBox_prejumps != NULL))
											{
												if (!(mcomboBox_prejumps->selectByValue(stranim.c_str()))) mcomboBox_prejumps->add(stranim.c_str(), ADD_BOTTOM, TRUE); //check if exist
											}
										 }
										 break;
								}
								for (std::vector<struct_overrides>::iterator iter = mAOOverrides.begin(); iter != mAOOverrides.end(); ++iter)
								{
									if (GetStateFromToken(strtoken.c_str()) == iter->state)
									{
										iter->ao_id = animid;
										mQuickAOOverrides[iter->orig_id].ao_id = animid;
									}
								}
							}
						}
					} 
				}
				llinfos << "ao nc read sucess" << llendl;

				for (std::vector<struct_overrides>::iterator iter = mAOOverrides.begin(); iter != mAOOverrides.end(); ++iter)
				{
					std::string defaultanim = "";
					switch(iter->state)
					{

					case STATE_AGENT_WALK:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultWalk");
							SetDefault(mcomboBox_walks,iter->ao_id,defaultanim);
							
						}
						 break;
					case STATE_AGENT_RUN:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultRun");
							SetDefault(mcomboBox_runs,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_JUMP:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultJump");
							SetDefault(mcomboBox_jumps,iter->ao_id,defaultanim);
						}
						break;
					case STATE_AGENT_SIT:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultSit");
							SetDefault(mcomboBox_sits,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_CROUCH:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultCrouch");
							SetDefault(mcomboBox_crouchs,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_GROUNDSIT:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultGroundSit");
							SetDefault(mcomboBox_gsits,iter->ao_id,defaultanim);;
						}
						 break;
					case STATE_AGENT_CROUCHWALK:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultCrouchWalk");
							SetDefault(mcomboBox_cwalks,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_FALLDOWN:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultFall");
							SetDefault(mcomboBox_falls,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_HOVER:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultHover");
							SetDefault(mcomboBox_hovers,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_FLY:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultFly");
							SetDefault(mcomboBox_flys,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_HOVER_UP:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultFlyUp");
							SetDefault(mcomboBox_flyups,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_FLYSLOW:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultFlySlow");
							SetDefault(mcomboBox_flyslows,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_HOVER_DOWN:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultFlyDown");
							SetDefault(mcomboBox_flydowns,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_LAND:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultLand");
							SetDefault(mcomboBox_lands,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_STANDUP:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultStandUp");
							SetDefault(mcomboBox_standups,iter->ao_id,defaultanim);
						}
						 break;
					case STATE_AGENT_PRE_JUMP:
						{
							defaultanim = gSavedPerAccountSettings.getString("AODefaultPreJump");
							SetDefault(mcomboBox_prejumps,iter->ao_id,defaultanim);
						}
						 break;
					}
					if (getAssetIDByName(defaultanim) != LLUUID::null)
					{
						iter->ao_id = getAssetIDByName(defaultanim);
						mQuickAOOverrides[iter->orig_id].ao_id = iter->ao_id;
					}
				}
				run();
			}
			else
			{
				llinfos << "ao nc decode error" << llendl;
			}
		}
	}
	else
	{
		llinfos << "ao nc read error" << llendl;
	}
}

BOOL LLFloaterAO::SetDefault(void* userdata, LLUUID ao_id, std::string defaultanim)
{
	if (sInstance && (userdata))
	{
		LLComboBox *box = (LLComboBox *) userdata;
		if (LLUUID::null == ao_id)
		{
			box->clear();
			box->removeall();
		}
		else
		{
			box->selectByValue(defaultanim);
		}
	}
	return TRUE;
}

class ObjectNameMatches : public LLInventoryCollectFunctor
{
public:
	ObjectNameMatches(std::string name)
	{
		sName = name;
	}
	virtual ~ObjectNameMatches() {}
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item)
	{
		if(item)
		{
			if (item->getParentUUID() == LLFloaterAO::invfolderid)
			{
				return (item->getName() == sName);
			}
			return false;
		}
		return false;
	}
private:
	std::string sName;
};

const LLUUID LLFloaterAO::getAssetIDByName(const std::string& name)
{
	if (name.empty() || !(gInventory.isEverythingFetched())) return LLUUID::null;

	if (name.length() == 32)
	{
		LLUUID test = LLUUID(name);
		if(test.asString().compare(name) == 0)
			return test;
	}

	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	ObjectNameMatches objectnamematches(name);
	gInventory.collectDescendentsIf(LLUUID::null,cats,items,FALSE,objectnamematches);

	if (items.count())
	{
		return items[0]->getAssetUUID();
	}
	return LLUUID::null;
};
