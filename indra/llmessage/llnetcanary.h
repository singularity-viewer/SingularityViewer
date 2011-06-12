// <edit>
#ifndef LL_LLNETCANARY_H
#define LL_LLNETCANARY_H
#include "stdtypes.h"
#include <string>
class LLNetCanary
{
public:
	LLNetCanary(int requested_port);
	~LLNetCanary();
	BOOL mGood;
	S32 mSocket;
	U16 mPort;
	U8 mBuffer[8192];
	typedef struct
	{
		F64 time; // LLTimer::getElapsedSeconds();
		//U8 message[4];
		U32 message;
		U32 points;
		std::string name;
	} entry;
};
#endif
// </edit>
