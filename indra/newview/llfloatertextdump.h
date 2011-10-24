/**
 * @file llfloatertextdump.h
 * @brief Simple Floater with a textbox for dumping text somewhere other than the chat window
 * @author Hazim Gazov
 */


#ifndef LL_LLFLOATERTEXTDUMP_H
#define LL_LLFLOATERTEXTDUMP_H

#include "llfloater.h"
#include "lltexteditor.h"
#include "llinventory.h"

class LLFloaterTextDump
: public LLFloater
{
public:
	LLFloaterTextDump(std::string& title, std::string& contents);
	static void show(std::string title, std::string contents);
	BOOL postBuild(void);
	void close(bool app_quitting);
	LLTextEditor* mEditor;
	std::string mContents;
	std::string mNewTitle;
private:
	virtual ~LLFloaterTextDump();
};

#endif
