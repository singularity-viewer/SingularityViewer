#include "llviewerprecompiledheaders.h"
#include "llcheckboxctrl.h"
#include "llfocusmgr.h"
#include "lluictrlfactory.h"

#include "llfloatersearchreplace.h"

const S32 SEARCH_REPLACE_WIDTH = 300;
const S32 SEARCH_REPLACE_HEIGHT = 120;
const std::string SEARCH_REPLACE_TITLE = "Search and Replace";

LLFloaterSearchReplace* LLFloaterSearchReplace::sInstance = NULL;

LLFloaterSearchReplace::LLFloaterSearchReplace() : mEditor(NULL),
	LLFloater(std::string("searchreplace"), LLRect(0, 0, SEARCH_REPLACE_WIDTH, SEARCH_REPLACE_HEIGHT), SEARCH_REPLACE_TITLE)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_search_replace.xml");
}

LLFloaterSearchReplace::~LLFloaterSearchReplace()
{
	sInstance = NULL;
}

void LLFloaterSearchReplace::open()
{
	LLFloater::open();

	if (mEditor)
	{
		bool fReadOnly = mEditor->isReadOnly();
		childSetEnabled("replace_label", !fReadOnly);
		childSetEnabled("replace_text", !fReadOnly);
		childSetEnabled("replace_btn", !fReadOnly);
		childSetEnabled("replace_all_btn", !fReadOnly);
	}

	childSetFocus("search_text", TRUE);
}

BOOL LLFloaterSearchReplace::postBuild()
{
	childSetAction("search_btn", onBtnSearch, this);
	childSetAction("replace_btn", onBtnReplace, this);
	childSetAction("replace_all_btn", onBtnReplaceAll, this);

	setDefaultBtn("search_btn");

	return TRUE;
}

void LLFloaterSearchReplace::show(LLTextEditor* editor)
{
	if (!sInstance)
	{
		sInstance = new LLFloaterSearchReplace();
	}

	if ( (sInstance) && (editor) )
	{
		sInstance->mEditor = editor;

		LLFloater* newdependee, *olddependee = sInstance->getDependee();
		LLView* viewp = editor->getParent();
		while (viewp)
		{
			newdependee = dynamic_cast<LLFloater*>(viewp);
			if (newdependee)
			{
				if (newdependee != olddependee)
				{
					if (olddependee)
						olddependee->removeDependentFloater(sInstance);

					if (!newdependee->getHost())
						newdependee->addDependentFloater(sInstance);
					else
						newdependee->getHost()->addDependentFloater(sInstance);
				}
				break;
			}
			viewp = viewp->getParent();
		}

		sInstance->open();
	}
}

void LLFloaterSearchReplace::onBtnSearch(void* userdata)
{
	if ( (!sInstance) || (!sInstance->mEditor) || (!sInstance->getDependee()) )
		return;

	LLCheckBoxCtrl* caseChk = sInstance->getChild<LLCheckBoxCtrl>("case_text");
	sInstance->mEditor->selectNext(sInstance->childGetText("search_text"), caseChk->get());
}

void LLFloaterSearchReplace::onBtnReplace(void* userdata)
{
	if ( (!sInstance) || (!sInstance->mEditor) || (!sInstance->getDependee()) )
		return;

	LLCheckBoxCtrl* caseChk = sInstance->getChild<LLCheckBoxCtrl>("case_text");
	sInstance->mEditor->replaceText(sInstance->childGetText("search_text"), sInstance->childGetText("replace_text"), caseChk->get());
}

void LLFloaterSearchReplace::onBtnReplaceAll(void* userdata)
{
	if ( (!sInstance) || (!sInstance->mEditor) || (!sInstance->getDependee()) )
		return;

	LLCheckBoxCtrl* caseChk = sInstance->getChild<LLCheckBoxCtrl>("case_text");
	sInstance->mEditor->replaceTextAll(sInstance->childGetText("search_text"), sInstance->childGetText("replace_text"), caseChk->get());
}
