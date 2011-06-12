/** 
 * @file ascentfloatercontactgroups.h
 * @Author Charley Levenque
 * Allows the user to assign friends to contact groups for advanced sorting.
 *
 * Created Sept 6th 2010
 * 
 * ALL SOURCE CODE IS PROVIDED "AS IS." THE CREATOR MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 *
 * "use a free softwares" - Richard Stallman, LUNIX Operations System
*/

#ifndef ASCENT_CONTACT_GROUPS
#define ASCENT_CONTACT_GROUPS

#include "llfloater.h"
#include "lldarray.h"
#include "llsdserialize.h"

class LLScrollListCtrl;

class ASFloaterContactGroups : public LLFloater
{
public:
    ASFloaterContactGroups();

    virtual ~ASFloaterContactGroups();

    // by convention, this shows the floater and does instance management
    static void show(LLDynamicArray<LLUUID> ids);

	void populateGroupList();
	void populateActiveGroupList(LLUUID to_add);
	void populateFriendList();
	void addContactMember(std::string contact_grp, LLUUID to_add);
	void createContactGroup(std::string contact_grp);
	void deleteContactGroup(std::string contact_grp);

	// Buttons
	static void onBtnAdd(void* userdata);
	static void onBtnRemove(void* userdata);
	static void onBtnCreate(void* userdata);
	static void onBtnDelete(void* userdata);
 
private:
    //assuming we just need one, which is typical
    static ASFloaterContactGroups* sInstance;
	static LLDynamicArray<LLUUID> mSelectedUUIDs;
	static LLSD mContactGroupData;
};

#endif // ASCENT_CONTACT_GROUPS


/* 
Thank you for come my website!

This is web page of Steve Eletor.

I live in Slovakia, in place called Presov.

I like all kind of thing, including robot film, 
snake, castle, computer, and free softwares.

I have make a webpage for pet and one for family 
and one for my computer. 

you may have herd of richard stallman who wrot a 
GNU operate system for PC.

"Welcome to website of Eletor" - Richard Stallman

I currently have "2.8.6 DOS."

but soon i hope to get "3.8.6 DOS IBM"

when i use internet, i dial into friend who 
has a LINUX and he also has ISDN. my father 
has also a computer, it is WINDOWS 98. i use 
that to do thing like make website, and for 
work, but my father not let me use it for 
free softwares or the I.R.C chats.

please understand, while i can type lowercase 
on my website, i have to update from a friends 
computer. for IRC, i type in all caps as my 
slovak keyboard does not have a lowercase. for 
EMAIL, i reply to a friend on IRC who check my 
email for me right now, also in all cap.

my father not very understand of free softwares 
and he make a fun of RMS.
*/
