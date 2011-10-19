#ifndef LL_LLFLOATERSEARCHREPLACE_H
#define LL_LLFLOATERSEARCHREPLACE_H

#include "llfloater.h"
#include "lltexteditor.h"

class LLFloaterSearchReplace : public LLFloater
{
private:
	LLFloaterSearchReplace();
	virtual ~LLFloaterSearchReplace();

public:
	virtual void open();
	virtual	BOOL postBuild();

public:
	static void show(LLTextEditor* editor);

	static void onBtnSearch(void* userdata);
	static void onBtnReplace(void* userdata);
	static void onBtnReplaceAll(void* userdata);

	static LLFloaterSearchReplace* getInstance() { return sInstance; }

private:
	LLTextEditor* mEditor;

	static LLFloaterSearchReplace* sInstance;
};

#endif // LL_LLFLOATERSEARCHREPLACE_H
