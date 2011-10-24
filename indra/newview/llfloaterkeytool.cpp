// <edit>

#include "llviewerprecompiledheaders.h"

#include "llkeytool.h"
#include "llfloaterkeytool.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llcommandhandler.h"
// for "other assets"
#include "lllocalinventory.h"

class LLKeyToolHandler : public LLCommandHandler
{
public:
	// don't allow from external browsers
	LLKeyToolHandler() : LLCommandHandler("keytool", true) { }
	bool handle(const LLSD& tokens, const LLSD& queryMap, LLMediaCtrl* web)
	{
		int tokencount = tokens.size();
		if (tokencount < 1)
		{
			return false;
		}
		LLUUID key = tokens[0].asUUID();
		if(key.notNull())
		{
			LLFloaterKeyTool::show(key);
		}
		
		return true;
	}
};
LLKeyToolHandler gKeyToolHandler;

std::list<LLFloaterKeyTool*> LLFloaterKeyTool::sInstances;

LLFloaterKeyTool::LLFloaterKeyTool(LLUUID id)
:	LLFloater()
{
	sInstances.push_back(this);
	mKey = id;
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_keytool.xml");
}

LLFloaterKeyTool::~LLFloaterKeyTool()
{
	sInstances.remove(this);
	delete mKeyTool;
}

void LLFloaterKeyTool::show(LLUUID id)
{
	S32 left, top;
	gFloaterView->getNewFloaterPosition(&left, &top);
	LLRect rect = gSavedSettings.getRect("FloaterKeyToolRect");
	rect.translate(left - rect.mLeft, top - rect.mTop);
	LLFloaterKeyTool* floaterp = new LLFloaterKeyTool(id);
	floaterp->setRect(rect);
	gFloaterView->adjustToFitScreen(floaterp, FALSE);
}

void LLFloaterKeyTool::close(bool app_quitting)
{
	LLFloater::close(app_quitting);
}

BOOL LLFloaterKeyTool::postBuild(void)
{
	setIsChrome(TRUE);
	setTitle(std::string("KeyTool"));

	mListBottom = getRect().getHeight() - 45;
	showType(LLKeyTool::KT_AGENT, LLAssetType::AT_NONE, MAYBE);
	showType(LLKeyTool::KT_TASK, LLAssetType::AT_NONE, MAYBE);
	showType(LLKeyTool::KT_GROUP, LLAssetType::AT_NONE, MAYBE);
	showType(LLKeyTool::KT_REGION, LLAssetType::AT_NONE, MAYBE);
	showType(LLKeyTool::KT_PARCEL, LLAssetType::AT_NONE, MAYBE);
	showType(LLKeyTool::KT_ITEM, LLAssetType::AT_NONE, MAYBE);
	showType(LLKeyTool::KT_ASSET, LLAssetType::AT_TEXTURE, MAYBE);
	showType(LLKeyTool::KT_ASSET, LLAssetType::AT_SOUND, MAYBE);
	showType(LLKeyTool::KT_ASSET, LLAssetType::AT_ANIMATION, MAYBE);
	showType(LLKeyTool::KT_ASSET, LLAssetType::AT_LANDMARK, MAYBE);
	showType(LLKeyTool::KT_ASSET, LLAssetType::AT_GESTURE, MAYBE);
	showType(LLKeyTool::KT_ASSET, LLAssetType::AT_CLOTHING, MAYBE);
	showType(LLKeyTool::KT_ASSET, LLAssetType::AT_BODYPART, MAYBE);
	//showType(LLKeyTool::KT_ASSET, LLAssetType::AT_COUNT, MAYBE);

	mKeyTool = new LLKeyTool(mKey, keyToolCallback, this);
	return TRUE;
}

void LLFloaterKeyTool::showType(LLKeyTool::LLKeyType key_type, LLAssetType::EType asset_type, isness result)
{
	std::string name = LLKeyTool::aWhat(key_type, asset_type);
	if((key_type == LLKeyTool::KT_ASSET) && (asset_type == LLAssetType::AT_COUNT))
		name = "other assets";
	LLTextBox* text = getChild<LLTextBox>(name, FALSE, FALSE);
	if(!text)
	{
		text = new LLTextBox(name, LLRect(10, mListBottom + 20, getRect().mRight, mListBottom));
		text->setFollowsTop();
		text->setColor(LLColor4::white);
		text->setHoverColor(LLColor4::white);
		mListBottom -= 20;
		addChild(text);

		clickData* data = new clickData;
		data->floater = this;
		data->key_type = key_type;
		data->asset_type = asset_type;
		text->setCallbackUserData((void*)data);
		//text->setClickedCallback(onClickType);
	}
	switch(result)
	{
	case YES:
		if(gSavedSettings.getBOOL("KeyToolAutomaticOpen"))
		{
			LLKeyTool::openKey(mKey, key_type, asset_type);
		}
		if(gSavedSettings.getBOOL("KeyToolAutomaticClose"))
		{
			this->close(FALSE);
		}
		text->setColor(LLColor4::green);
		break;
	case NO:
		text->setColor(LLColor4::grey);
		break;
	default:
		text->setColor(LLColor4::white);
		break;
	}
}

// static
void LLFloaterKeyTool::keyToolCallback(LLUUID id, LLKeyTool::LLKeyType key_type, LLAssetType::EType asset_type, BOOL is, void* user_data)
{
	LLFloaterKeyTool* floater = (LLFloaterKeyTool*)user_data;
	if(std::find(sInstances.begin(), sInstances.end(), floater) == sInstances.end())
		return;
	floater->showType(key_type, asset_type, is ? YES : NO);
}

// static
void LLFloaterKeyTool::onClickType(void* user_data)
{
	clickData data = *((clickData*)user_data);
	if(data.floater)
	{
		if(std::find(sInstances.begin(), sInstances.end(), data.floater) == sInstances.end())
			return;
		if(!((data.key_type == LLKeyTool::KT_ASSET) && (data.asset_type == LLAssetType::AT_COUNT)))
			LLKeyTool::openKey(data.floater->mKey, data.key_type, data.asset_type);
		/*
		 //notecard magic bullcrap?
		else // notecards, objects, and scripts
		{
			std::set<LLUUID> new_items;
			new_items.insert(LLLocalInventory::addItem("New Note", int(LLAssetType::AT_NOTECARD), data.floater->mKey));
			new_items.insert(LLLocalInventory::addItem("Object", int(LLAssetType::AT_OBJECT), data.floater->mKey));
			new_items.insert(LLLocalInventory::addItem("New Script", int(LLAssetType::AT_SCRIPT), data.floater->mKey));
			LLNotecardMagic::acquire(new_items);
		}
		*/
	}
}

// </edit>
