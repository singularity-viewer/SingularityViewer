/* Copyright (c) 2009
 *
 * Modular Systems All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS “AS IS”
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "llagent.h"
#include "llvoinventorylistener.h"

class ScriptCounter : public LLVOInventoryListener
{
public:
	ScriptCounter();
	~ScriptCounter();

private:
	static ScriptCounter* sInstance;
	static void init();
	static LLSD* getprim(LLUUID id);
	static void completechk();
public:
	static void processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data);
	static void processObjectProperties(LLMessageSystem* msg, void** user_data);
	void inventoryChanged(LLViewerObject* obj,
								 LLInventoryObject::object_list_t* inv,
								 S32 serial_num,
								 void* data);

	static ScriptCounter* getInstance(){ init(); return sInstance; }

	static void serialize(LLDynamicArray<LLViewerObject*> objects);
	static void serializeSelection(bool delScript);
	static void finalize(LLSD data);
	static void showResult(std::string output);

private:
	static void subserialize(LLViewerObject* linkset);

	enum ExportState { IDLE, COUNTING };

	static U32 status;
	static U32 invqueries;
	static U32 scriptcount;
	static LLUUID reqObjectID;
	static std::set<std::string> objIDS;
	static LLDynamicArray<LLUUID> delUUIDS;
	static int objectCount;
	static LLViewerObject* foo;
	static bool doDelete;
	static std::stringstream sstr;
	static int countingDone;
};
