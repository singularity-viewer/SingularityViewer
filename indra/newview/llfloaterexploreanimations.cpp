// <edit>

#include "llviewerprecompiledheaders.h"
#include "llfloaterexploreanimations.h"
#include "lluictrlfactory.h"
#include "llscrolllistctrl.h"
#include "llfloateranimpreview.h"
#include "llvoavatar.h"
#include "lllocalinventory.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"
#include "lltoolmgr.h"
// <stuff for jelly roll>
#include "llfloatertools.h"
#include "llselectmgr.h"
// </stuff for jelly roll>

std::map< LLUUID, std::list< LLAnimHistoryItem* > > LLFloaterExploreAnimations::animHistory;
LLFloaterExploreAnimations* LLFloaterExploreAnimations::sInstance;


LLAnimHistoryItem::LLAnimHistoryItem(LLUUID assetid)
{
	mAssetID = assetid;
}

LLFloaterExploreAnimations::LLFloaterExploreAnimations(LLUUID avatarid)
:	LLFloater()
{
	mLastMouseX = 0;
	mLastMouseY = 0;

	LLFloaterExploreAnimations::sInstance = this;
	mAvatarID = avatarid;
	mAnimPreview = new LLPreviewAnimation(256, 256);
	mAnimPreview->setZoom(2.0f);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_explore_animations.xml");
}

void LLFloaterExploreAnimations::close(bool app_quitting)
{
	LLFloater::close(app_quitting);
}

LLFloaterExploreAnimations::~LLFloaterExploreAnimations()
{
	mAnimPreview = NULL;
	LLFloaterExploreAnimations::sInstance = NULL;
}

BOOL LLFloaterExploreAnimations::postBuild(void)
{
	childSetCommitCallback("anim_list", onSelectAnimation, this);
	LLRect r = getRect();
	mPreviewRect.set(r.getWidth() - 266, r.getHeight() - 25, r.getWidth() - 10, r.getHeight() - 256);
	update();
	return TRUE;
}

void LLFloaterExploreAnimations::update()
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("anim_list");
	LLUUID selection = list->getSelectedValue().asUUID();
	list->clearRows(); // do this differently probably

	std::list<LLAnimHistoryItem*> history = animHistory[mAvatarID];
	std::list<LLAnimHistoryItem*>::iterator iter = history.begin();
	std::list<LLAnimHistoryItem*>::iterator end = history.end();
	for( ; iter != end; ++iter)
	{
		LLAnimHistoryItem* item = (*iter);

		LLSD element;
		element["id"] = item->mAssetID;

		LLSD& name_column = element["columns"][0];
		name_column["column"] = "name";
		name_column["value"] = item->mAssetID.asString();

		LLSD& info_column = element["columns"][1];
		info_column["column"] = "info";
		if(item->mPlaying)
			info_column["value"] = "Playing";
		else
			info_column["value"] = llformat("%.1f min ago", (LLTimer::getElapsedSeconds() - item->mTimeStopped) / 60.f);

		list->addElement(element, ADD_BOTTOM);
	}

	list->selectByID(selection);
}

void LLFloaterExploreAnimations::draw()
{
	LLFloater::draw();

	LLRect r = getRect();
	mPreviewRect.set(r.getWidth() - 266, r.getHeight() - 25, r.getWidth() - 10, r.getHeight() - 256);

	gGL.color3f(1.f, 1.f, 1.f);

	gGL.getTexUnit(0)->bind(mAnimPreview);

	gGL.begin( LLRender::QUADS );
	{
		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex2i(mPreviewRect.mLeft, mPreviewRect.mTop);
		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex2i(mPreviewRect.mLeft, mPreviewRect.mBottom);
		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex2i(mPreviewRect.mRight, mPreviewRect.mBottom);
		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex2i(mPreviewRect.mRight, mPreviewRect.mTop);
	}
	gGL.end();

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	//LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
	//if (!avatarp->areAnimationsPaused())
	//{
	//	mAnimPreview->requestUpdate();
	//}
}





// static
void LLFloaterExploreAnimations::startAnim(LLUUID avatarid, LLUUID assetid)
{
	std::string asset_str = assetid.asString();
	if(asset_str.find("17132261-c061") != std::string::npos) return; // dog1
	else if(asset_str.find("fea558cb-8b9b") != std::string::npos) return; // dog2
	else if(asset_str.find("50cb5750-0743") != std::string::npos) return; // dog3
	else if(asset_str.find("-dead-") != std::string::npos) return; // emo

	LLAnimHistoryItem* item = NULL;

	std::list<LLAnimHistoryItem*> history = animHistory[avatarid];
	std::list<LLAnimHistoryItem*>::iterator iter = history.begin();
	std::list<LLAnimHistoryItem*>::iterator end = history.end();
	for( ; iter != end; ++iter)
	{
		if((*iter)->mAssetID == assetid)
		{
			item = (*iter);
			break;
		}
	}
	if(!item)
	{
		item = new LLAnimHistoryItem(assetid);
		item->mAvatarID = avatarid;
		item->mTimeStarted = LLTimer::getElapsedSeconds();
	}
	item->mPlaying = true;
	history.push_back(item);
	animHistory[avatarid] = history; // is this really necessary?
	handleHistoryChange();
}

// static
void LLFloaterExploreAnimations::stopAnim(LLUUID avatarid, LLUUID assetid)
{
	std::string asset_str = assetid.asString();
	if(asset_str.find("17132261-c061") != std::string::npos) return; // dog1
	else if(asset_str.find("fea558cb-8b9b") != std::string::npos) return; // dog2
	else if(asset_str.find("50cb5750-0743") != std::string::npos) return; // dog3
	else if(asset_str.find("-dead-") != std::string::npos) return; // emo

	LLAnimHistoryItem* item = NULL;

	std::list<LLAnimHistoryItem*> history = animHistory[avatarid];
	std::list<LLAnimHistoryItem*>::iterator iter = history.begin();
	std::list<LLAnimHistoryItem*>::iterator end = history.end();
	for( ; iter != end; ++iter)
	{
		if((*iter)->mAssetID == assetid)
		{
			item = (*iter);
			break;
		}
	}
	if(!item)
	{
		item = new LLAnimHistoryItem(assetid);
		item->mAvatarID = avatarid;
		item->mTimeStarted = LLTimer::getElapsedSeconds();
		history.push_back(item);
	}
	item->mPlaying = false;
	item->mTimeStopped = LLTimer::getElapsedSeconds();
	handleHistoryChange();
}

class LLAnimHistoryItemCompare
{
public:
	bool operator() (LLAnimHistoryItem* first, LLAnimHistoryItem* second)
	{
		if(first->mPlaying)
		{
			if(second->mPlaying)
			{
				return (first->mTimeStarted > second->mTimeStarted);
			}
			else
			{
				return true;
			}
		}
		else if(second->mPlaying)
		{
			return false;
		}
		else
		{
			return (first->mTimeStopped > second->mTimeStopped);
		}
	}
};

// static
void LLFloaterExploreAnimations::handleHistoryChange()
{
	std::map< LLUUID, std::list< LLAnimHistoryItem* > >::iterator av_iter = animHistory.begin();
	std::map< LLUUID, std::list< LLAnimHistoryItem* > >::iterator av_end = animHistory.end();
	for( ; av_iter != av_end; ++av_iter)
	{
		std::list<LLAnimHistoryItem*> history = (*av_iter).second;

		// Sort it
		LLAnimHistoryItemCompare c;
		history.sort(c);

		// Remove dupes
		history.unique();

		// Trim it
		if(history.size() > 32)
		{
			history.resize(32);
		}

		animHistory[(*av_iter).first] = history;
	}

	// Update floater
	if(LLFloaterExploreAnimations::sInstance)
		LLFloaterExploreAnimations::sInstance->update();
}





// static
void LLFloaterExploreAnimations::onSelectAnimation(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterExploreAnimations* floater = (LLFloaterExploreAnimations*)user_data;
	LLPreviewAnimation* preview = (LLPreviewAnimation*)floater->mAnimPreview;
	LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("anim_list");
	LLUUID selection = list->getSelectedValue().asUUID();
	
	preview->getDummyAvatar()->deactivateAllMotions();
	preview->getDummyAvatar()->startMotion(selection, 0.f);
	preview->setZoom(2.0f);
}

//-----------------------------------------------------------------------------
// handleMouseDown()
//-----------------------------------------------------------------------------
BOOL LLFloaterExploreAnimations::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mPreviewRect.pointInRect(x, y))
	{
		//llinfos << "lolwut" << llendl;
		bringToFront( x, y );
		gFocusMgr.setMouseCapture(this);
		gViewerWindow->hideCursor();
		mLastMouseX = x;
		mLastMouseY = y;
		return TRUE;
	}

	return LLFloater::handleMouseDown(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleMouseUp()
//-----------------------------------------------------------------------------
BOOL LLFloaterExploreAnimations::handleMouseUp(S32 x, S32 y, MASK mask)
{
	//llinfos << "lolwut pear" << llendl;
	gFocusMgr.setMouseCapture(FALSE);
	gViewerWindow->showCursor();
	return LLFloater::handleMouseUp(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleHover()
//-----------------------------------------------------------------------------
BOOL LLFloaterExploreAnimations::handleHover(S32 x, S32 y, MASK mask)
{
	MASK local_mask = mask & ~MASK_ALT;

	if (mAnimPreview && hasMouseCapture())
	{
		if (local_mask == MASK_PAN)
		{
			// pan here
			mAnimPreview->pan((F32)(x - mLastMouseX) * -0.005f, (F32)(y - mLastMouseY) * -0.005f);
		}
		else if (local_mask == MASK_ORBIT)
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;
			
			mAnimPreview->rotate(yaw_radians, pitch_radians);
		}
		else 
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;
			
			mAnimPreview->rotate(yaw_radians, 0.f);
			mAnimPreview->zoom(zoom_amt);
		}

		mAnimPreview->requestUpdate();

		LLUI::setCursorPositionLocal(this, mLastMouseX, mLastMouseY);
	}

	if (!mPreviewRect.pointInRect(x, y) || !mAnimPreview)
	{
		return LLFloater::handleHover(x, y, mask);
	}
	else if (local_mask == MASK_ORBIT)
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLCAMERA);
	}
	else if (local_mask == MASK_PAN)
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLPAN);
	}
	else
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLZOOMIN);
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// handleScrollWheel()
//-----------------------------------------------------------------------------
BOOL LLFloaterExploreAnimations::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	mAnimPreview->zoom((F32)clicks * -0.2f);
	mAnimPreview->requestUpdate();
	return TRUE;
}

//-----------------------------------------------------------------------------
// onMouseCaptureLost()
//-----------------------------------------------------------------------------
void LLFloaterExploreAnimations::onMouseCaptureLost()
{
	gViewerWindow->showCursor();
}
// </edit>
