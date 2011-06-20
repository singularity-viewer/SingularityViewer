// <edit>

#include "llviewerprecompiledheaders.h"
#include "llfloaterinterceptor.h"
#include "lluictrlfactory.h"
#include "llsliderctrl.h"
#include "llviewercontrol.h"
#include "llagent.h"
#include "llviewerobject.h"

bool LLFloaterInterceptor::gInterceptorActive = false;
LLFloaterInterceptor* LLFloaterInterceptor::sInstance;
std::list<LLViewerObject*> LLFloaterInterceptor::affected;

LLFloaterInterceptor::LLFloaterInterceptor()
:	LLFloater()
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_interceptor.xml");
	setIsChrome(TRUE);
}

void LLFloaterInterceptor::close(bool app_quitting)
{
	gInterceptorActive = false;
	changeRange(0.f); // Let go of everything!
	LLFloater::close(app_quitting);
}

LLFloaterInterceptor::~LLFloaterInterceptor()
{
	sInstance = NULL;
}

BOOL LLFloaterInterceptor::postBuild(void)
{
	childSetCommitCallback("range", onChangeStuff, NULL);
	childSetCommitCallback("affect_yours", onChangeStuff, NULL);
	return TRUE;
}

void LLFloaterInterceptor::updateNumberAffected()
{
	childSetText("number_affected", llformat("Affected: %d", LLFloaterInterceptor::affected.size()));
}

// static
void LLFloaterInterceptor::show()
{
	if(!sInstance)
	{
		affected.clear();
		gInterceptorActive = true;
		sInstance = new LLFloaterInterceptor();
	}
	sInstance->open();
	//changeRange(gSavedSettings.getF32("InterceptorRange"));
}

void LLFloaterInterceptor::affect(LLViewerObject* object)
{
	// Objects that move are passed in here

	if(!object->isRoot() || object->isAvatar())// || !object->isActive())
	{
		letGo(object);
		return;
	}
	if(object->permYouOwner() && !gSavedSettings.getBOOL("InterceptorAffectYours"))
	{
		letGo(object);
		return;
	}
	LLVector3 difference = object->getPositionAgent() - gAgent.getPositionAgent();
	F32 distance = llabs(difference.magVec());
	if(distance < gSavedSettings.getF32("InterceptorRange"))
		grab(object);
	else
		letGo(object);
}

void LLFloaterInterceptor::changeRange(F32 range)
{
	// User changed range, may have to let go of some objects
	// Currently doesn't grab objects, that only happens if they move

	std::list<LLViewerObject*> remove;

	LLVector3 agentPos = gAgent.getPositionAgent();
	std::list<LLViewerObject*>::iterator iter = affected.begin();
	std::list<LLViewerObject*>::iterator end = affected.end();
	for( ; iter != end; ++iter)
	{
		LLViewerObject* object = (*iter);
		if(object)
		{
			if(object->isDead())
			{
				remove.push_back(object);
			}
			else if(object->permYouOwner() && !gSavedSettings.getBOOL("InterceptorAffectYours"))
			{
				remove.push_back(object);
			}
			else
			{
				LLVector3 difference = object->getPositionAgent() - agentPos;
				F32 distance = llabs(difference.magVec());
				if(distance >= range)
					remove.push_back(object);
			}
		}
		else remove.push_back(object);
	}
	iter = remove.begin();
	end = remove.end();
	for( ; iter != end; ++iter)
	{
		letGo(*iter);
	}
}

void LLFloaterInterceptor::grab(LLViewerObject* object)
{
	//std::list<LLViewerObject*>::iterator pos = std::find(affected.begin(), affected.end(), object);
	//if(pos == affected.end())
	if(!LLFloaterInterceptor::has(object))
	{
		affected.push_back(object);
		// Select
		gMessageSystem->newMessageFast(_PREHASH_ObjectSelect);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
		gMessageSystem->sendReliable(gAgent.getRegionHost());

		if(LLFloaterInterceptor::sInstance)
			LLFloaterInterceptor::sInstance->updateNumberAffected();
	}
}

void LLFloaterInterceptor::letGo(LLViewerObject* object)
{
	//std::list<LLViewerObject*>::iterator pos = std::find(affected.begin(), affected.end(), object);
	//if(pos != affected.end())
	if(LLFloaterInterceptor::has(object))
	{
		affected.remove(object);
		// Deselect
		gMessageSystem->newMessageFast(_PREHASH_ObjectDeselect);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
		gMessageSystem->sendReliable(gAgent.getRegionHost());

		if(LLFloaterInterceptor::sInstance)
			LLFloaterInterceptor::sInstance->updateNumberAffected();
	}
}

// static
void LLFloaterInterceptor::onChangeStuff(LLUICtrl* ctrl, void* userData)
{
	F32 range = LLFloaterInterceptor::sInstance->getChild<LLSliderCtrl>("range")->getValueF32();
	changeRange(range);
	LLFloaterInterceptor::sInstance->setIsChrome(TRUE);
}

// static
bool LLFloaterInterceptor::has(LLViewerObject* vobj)
{
	std::list<LLViewerObject*>::iterator pos = std::find(affected.begin(), affected.end(), vobj);
	return (pos != affected.end());
}
// </edit>