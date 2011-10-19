#include "llviewerprecompiledheaders.h"

#if SHY_MOD //Command handler

#include "lluuid.h"
#include "llagent.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llhudtext.h"

#include <boost/tokenizer.hpp>
#include <ctype.h>

#include "shcommandhandler.h"

inline std::list<SHCommandHandler*> &SHCommandHandler::getScriptCommandList()
{
	static std::list<SHCommandHandler*> sScriptCommandList;
	return sScriptCommandList;
}

inline std::list<SHCommandHandler*> &SHCommandHandler::getChatCommandList()
{
	static std::list<SHCommandHandler*> sChatCommandList;
	return sChatCommandList;
}

SHCommandHandler::SHCommandHandler(bool from_chat, command_fn_t callback) : mDispatchFn(callback)
{
	if(from_chat)
		SHCommandHandler::getChatCommandList().push_back(this);
	else
		SHCommandHandler::getScriptCommandList().push_back(this);
}
bool SHCommandHandler::handleCommand(bool from_chat, const std::string &full_str, const LLUUID &callerid, LLViewerObject *pCaller)
{
	static const LLCachedControl<bool> sh_allow_script_commands("SHAllowScriptCommands",true);	//Are script commands enabled?
	static const LLCachedControl<bool> sh_allow_chat_commands("AscentCmdLine",true);			//Are chat commands enabled?

	static const LLCachedControl<std::string> sh_script_cmd_prefix("SHScriptCommandPrefix","#@#@!");//Incomming script commands are prefixed with this string.
	static const LLCachedControl<std::string> sh_chat_cmd_prefix("SHChatCommandPrefix",">>");		//Outgoing chat commands are prefixed with this string.
	static const LLCachedControl<std::string> sh_script_cmd_sep("SHScriptCommandSeparator","|");	//Separator for script commands
	static const LLCachedControl<std::string> sh_chat_cmd_sep("SHChatCommandSeparator"," ");		//Separator for chat commands

	const char *szPrefix = from_chat ? sh_chat_cmd_prefix.get().c_str() : sh_script_cmd_prefix.get().c_str();
	const size_t prefix_len = strlen(szPrefix);

	if(full_str.length() <= prefix_len || (prefix_len && full_str.substr(0,prefix_len)!=szPrefix))
		return false; //Not a command
	else if(!isalnum(full_str.c_str()[prefix_len]))
		return false; //Commands must start with a # or letter
	else if((!from_chat && !sh_allow_script_commands) || (from_chat && !sh_allow_chat_commands))
		return !!prefix_len; //These commands are disabled.

	const std::string trimmed_str = full_str.substr(prefix_len,std::string::npos);
	
	typedef boost::char_separator<char> sep_t;
	const sep_t sep(from_chat ? sh_chat_cmd_sep.get().c_str(): sh_script_cmd_sep.get().c_str(),"", boost::keep_empty_tokens);
	const boost::tokenizer<sep_t> tokens(trimmed_str, sep);
	const boost::tokenizer<sep_t>::const_iterator tok_end = tokens.end();
	boost::tokenizer<sep_t>::const_iterator tok_iter = tokens.begin();

	if(tok_iter == tok_end) //Shouldn't ever be true.
		return !from_chat && !!prefix_len; //Don't spam if the prefix was correct yet the string was empty.
	
	LLSD cmd_args;		//Push tokens into LLSD so args can be looked up via args[#]
	bool found = false;	//Return this if found. Also used to determine if cmd_args has been set up.
	
	//Now look for the command.
	std::list<SHCommandHandler*> &CommandList = from_chat ? getChatCommandList() : getScriptCommandList();
	std::list<SHCommandHandler*>::iterator cmd_iter = CommandList.begin();
	const std::list<SHCommandHandler*>::iterator cmd_end = CommandList.end();
	const std::string search_arg = utf8str_tolower(*tok_iter);
	for(cmd_iter;cmd_iter!=cmd_end;++cmd_iter)
	{
		if(search_arg==utf8str_tolower((*cmd_iter)->getName()))
		{
			if(!found)
			{
				found = true;
				int i = -1;
				for(tok_iter;tok_iter!=tok_end;++tok_iter) //Fill the LLSD
					cmd_args[++i] = (*tok_iter);
			}
			(*cmd_iter)->Dispatch(cmd_args,trimmed_str,callerid,pCaller);
		}
	}
	
	return found || !!prefix_len; //Don't spam if the prefix was correct yet the command wasn't found.
}
void SHCommandHandler::send_chat_to_object(const std::string &message, int channel, const LLUUID &target_id/*=gAgentID*/)
{
	if(target_id.isNull())return;
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("ScriptDialogReply");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUID("SessionID", gAgent.getSessionID());
	msg->nextBlock("Data");
	msg->addUUID("ObjectID", target_id);
	msg->addS32("ChatChannel", channel);
	msg->addS32("ButtonIndex", 0);
	msg->addString("ButtonLabel", message);
	gAgent.sendReliableMessage();
}
void SHCommandHandler::send_chat_to_object(const std::string &message, int channel, LLViewerObject *pTargetObject)
{
	if(pTargetObject)
		send_chat_to_object(message,channel,pTargetObject->getID());
	
}
CMD_SCRIPT(preloadanim)
{
	//args[1] = anim uuid
	gAssetStorage->getAssetData(args[1],LLAssetType::AT_ANIMATION,(LLGetAssetCallback)NULL,NULL,TRUE);
}
CMD_SCRIPT(key2name)
{
	struct CCacheNameResponder
	{
		static void Handler(const std::string &fullname, const std::string &ident, const S32 channel, const LLUUID &callerid)
		{
			std::string out_str = "key2namereply|"+ident+"|"+fullname;
			SHCommandHandler::send_chat_to_object(out_str, channel, callerid);
		}
	};
	//args[1] = identifier
	//args[2] = av id
	//args[3] = chan
	//args[4] = group
	gCacheName->get(args[2],args[4],boost::bind(&CCacheNameResponder::Handler,_2,args[1].asString(),args[3].asInteger(),callerid));
	return;
}

CMD_SCRIPT(getsex)
{
	//args[1] = identifier
	//args[2] = target id
	//args[3] = chan
	LLVOAvatar* pAvatar = gObjectList.findAvatar(args[2]);
	if(pAvatar)
		SHCommandHandler::send_chat_to_object(llformat("getsexreply|%s|%d",args[1].asString().c_str(),pAvatar->getVisualParamWeight("male")), args[3], callerid);
	else
		SHCommandHandler::send_chat_to_object(llformat("getsexreply|%s|-1",args[1].asString().c_str()), args[3], callerid);
}

CMD_SCRIPT(getwindowsize)
{
	//args[1] = identifier
	//args[2] = chan
	const std::string out_msg = llformat("getwindowsizereply|%s|%d|%d",
		args[1].asString().c_str(),
		gViewerWindow->getWindowDisplayHeight(),
		gViewerWindow->getWindowDisplayWidth());
	SHCommandHandler::send_chat_to_object(out_msg, args[2], callerid);
}

CMD_SCRIPT(gettext)
{
	//args[1] = target id
	//args[2] = chan
	LLViewerObject *pObject = gObjectList.findObject(args[1]);
	if(pObject)
	{
		std::string out_msg = pObject->getDebugText();
		if(!out_msg.empty())
			SHCommandHandler::send_chat_to_object(out_msg,args[2],callerid);
	}
}

#include "llphysicsmotion.h"

void cmdline_printchat(std::string message);
CMD_CHAT(physparams)
{
	//args[1] = avatar name
	for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();iter != LLCharacter::sInstances.end(); ++iter)
	{
		LLVOAvatar* inst = (LLVOAvatar*) *iter;
		if(!inst)continue;
		if(inst->getFullname().find(args[1].asString())!=std::string::npos)
		{
			LLPhysicsMotionController *motion = (LLPhysicsMotionController*)(inst->findMotion(LLUUID("7360e029-3cb8-ebc4-863e-212df440d987")));
			cmdline_printchat(std::string("Phys param info for ")+inst->getFullname()+":\n");
			if(motion)
			{
				cmdline_printchat(motion->getString());
			}
			else
				cmdline_printchat("Unable to find physics motion controller\n");
		}
	}
}
#endif //shy_mod
