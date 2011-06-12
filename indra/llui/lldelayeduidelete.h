// <edit>
#ifndef LL_LLDELAYEDUIDELETE_H
#define LL_LLDELAYEDUIDELETE_H
#include "lleventtimer.h"
#include "llview.h"
class LLDeleteJob
{
public:
	virtual BOOL work(U32& completed);
};
class LLViewDeleteJob : public LLDeleteJob
{
public:
	LLViewDeleteJob(std::list<LLView*> views);
	virtual ~LLViewDeleteJob();
	virtual BOOL work(U32& completed);
private:
	std::list<LLView*> mList;
};
class LLDeleteScheduler : public LLEventTimer
{
public:
	LLDeleteScheduler();
	void addViewDeleteJob(std::list<LLView*> views);
	BOOL tick();
private:
	static std::list<LLDeleteJob*> sJobs;
};
extern LLDeleteScheduler* gDeleteScheduler;
#endif
// </edit>
