// <edit>
#ifndef LL_LLFLOATEREXPLOREANIMATIONS_H
#define LL_LLFLOATEREXPLOREANIMATIONS_H

#include "llfloater.h"
#include "llfloateranimpreview.h"
#include "llviewerwindow.h" // gViewerWindow

class LLAnimHistoryItem
{
public:
	LLAnimHistoryItem(LLUUID assetid);

	LLUUID mAvatarID;
	LLUUID mAssetID;
	bool mPlaying;
	F64 mTimeStarted;
	F64 mTimeStopped;
};

class LLFloaterExploreAnimations
: public LLFloater
{
public:
	LLFloaterExploreAnimations(LLUUID avatarid);
	BOOL postBuild(void);
	void close(bool app_quitting);

	void update();

	LLUUID mAvatarID;
	LLPointer<LLPreviewAnimation> mAnimPreview;

private:
	virtual ~LLFloaterExploreAnimations();


// static stuff!
public:
	static void onSelectAnimation(LLUICtrl* ctrl, void* user_data);

	BOOL handleMouseDown(S32 x, S32 y, MASK mask);
	BOOL handleMouseUp(S32 x, S32 y, MASK mask);
	BOOL handleHover(S32 x, S32 y, MASK mask);
	BOOL handleScrollWheel(S32 x, S32 y, S32 clicks); 
	void onMouseCaptureLost();

	static void startAnim(LLUUID avatarid, LLUUID assetid);
	static void stopAnim(LLUUID avatarid, LLUUID assetid);

	static std::map< LLUUID, std::list< LLAnimHistoryItem* > > animHistory;
	static LLFloaterExploreAnimations* sInstance;
private:
	static void handleHistoryChange();

protected:
	void			draw();
	LLRect				mPreviewRect;
	S32					mLastMouseX;
	S32					mLastMouseY;
};

#endif
// </edit>
