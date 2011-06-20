// <edit>
#ifndef LL_LLFLOATERINTERCEPTOR_H
#define LL_LLFLOATERINTERCEPTOR_H

#include "llfloater.h"
#include "llviewerobject.h"

class LLFloaterInterceptor
: public LLFloater
{
public:
	LLFloaterInterceptor();
	BOOL postBuild(void);
	void close(bool app_quitting);
	void updateNumberAffected();
	
private:
	virtual ~LLFloaterInterceptor();

// static stuff
public:
	static bool gInterceptorActive;
	static LLFloaterInterceptor* sInstance;
	static std::list<LLViewerObject*> affected;
	static void show();
	static void LLFloaterInterceptor::onChangeStuff(LLUICtrl* ctrl, void* userData);
	static void LLFloaterInterceptor::changeRange(F32 range);
	static void affect(LLViewerObject* object);
	static void LLFloaterInterceptor::grab(LLViewerObject* object);
	static void LLFloaterInterceptor::letGo(LLViewerObject* object);
	static bool LLFloaterInterceptor::has(LLViewerObject* vobj);
};

#endif
// </edit>