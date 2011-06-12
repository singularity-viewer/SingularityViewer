// <edit>

#ifndef LL_LLFLOATEREXPLORESOUNDS_H
#define LL_LLFLOATEREXPLORESOUNDS_H

#include "llfloater.h"
#include "llaudioengine.h"
#include "lleventtimer.h"

class LLFloaterExploreSounds
: public LLFloater, public LLEventTimer
{
public:
	LLFloaterExploreSounds();
	BOOL postBuild(void);
	void close(bool app_quitting);

	BOOL tick();

	LLSoundHistoryItem getItem(LLUUID itemID);

	static void handle_play_locally(void* user_data);
	static void handle_play_in_world(void* user_data);
	static void handle_look_at(void* user_data);
	static void handle_open(void* user_data);
	static void handle_copy_uuid(void* user_data);
	static void handle_stop(void* user_data);
	static void blacklistSound(void* user_data);

private:
	virtual ~LLFloaterExploreSounds();
	std::list<LLSoundHistoryItem> mLastHistory;

// static stuff!
public:
	static LLFloaterExploreSounds* sInstance;

	static void toggle();
};

#endif
// </edit>
