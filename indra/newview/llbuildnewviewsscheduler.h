// <edit>
#include "llinventoryview.h"
#include "llinventory.h"
#include "lleventtimer.h"
class LLBuildNewViewsScheduler : public LLEventTimer
{
	typedef struct
	{
		LLInventoryPanel* mInventoryPanel;
		LLInventoryObject* mInventoryObject;
	} job;
public:
	LLBuildNewViewsScheduler();
	void addJob(LLInventoryPanel* inventory_panel, LLInventoryObject* inventory_object);
	void cancel(LLInventoryPanel* inventory_panel);
	BOOL tick();
private:
	static std::list<job> sJobs;
	void buildNewViews(LLInventoryPanel* panelp, LLInventoryObject* objectp);
};
extern LLBuildNewViewsScheduler* gBuildNewViewsScheduler;
// </edit>
