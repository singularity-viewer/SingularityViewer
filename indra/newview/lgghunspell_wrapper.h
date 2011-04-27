/* Copyright (C) 2009 LordGregGreg Back

   This is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   This is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the viewer; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef ASPELL_WRAPPER
#define ASPELL_WRAPPER 1

#if LL_WINDOWS
#include "hunspell/hunspelldll.h"
#else
#include "hunspell/hunspell.hxx"
#endif

class lggHunSpell_Wrapper
{

public:
	static Hunspell* myHunspell;

	static void initSettings();
	void processSettings();

	std::vector<std::string> getAvailDicts();
	std::vector<std::string> getInstalledDicts();
	std::vector<std::string> getDicts();
	std::vector<std::string> getExtraDicts();
	void addDictionary(std::string additionalDictionary);
	void addWordToCustomDictionary(std::string wordToAdd);
	void addButton(std::string selection);
	void removeButton(std::string selection);
	void editCustomButton();
	void newDictSelection(std::string selection);
	void getMoreButton(void * data);
	static std::string dictName2FullName(std::string dictName);
	static std::string fullName2DictName(std::string fullName);
	void setNewDictionary(std::string newDict);
	BOOL isSpelledRight(std::string wordToCheck);
	std::vector<std::string> getSuggestionList(std::string badWord);
	S32 findNextError(std::string haystack, int startAt);

	std::vector<std::string> CSV2VEC(std::string csv);
	std::string VEC2CSV(std::vector<std::string> vec);

	void setSpellCheckHighlight(BOOL highlight);
	BOOL getSpellCheckHighlight();

private:
	void createCustomDic();
	std::string getCorrectPath(std::string file);
	lggHunSpell_Wrapper();
	~lggHunSpell_Wrapper();

	void debugTest(std::string testWord);//prints out debug about testing the word
	std::string currentBaseDic;
	//std::vector<std::string> languageCodes;
	//std::vector<std::string> countryCodes;
//	BOOL* mSpellCheckHighlight;
};

extern lggHunSpell_Wrapper* glggHunSpell; // the singleton hunspell wrapper

#endif 
