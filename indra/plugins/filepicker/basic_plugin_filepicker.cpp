/**
 * @file plugin_filepicker.cpp
 * @brief Plugin that runs a file picker.
 *
 * @cond
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 * 
 * Copyright (c) 2010, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 * 
 * @endcond
 */

#include "linden_common.h"
#include "basic_plugin_base.h"
#include "llfilepicker.h"
#include "lldirpicker.h"

class FilepickerPlugin : public BasicPluginBase
{
	public:
		FilepickerPlugin(LLPluginInstance::sendMessageFunction send_message_function, LLPluginInstance* plugin_instance);
		~FilepickerPlugin();

		/*virtual*/ void receiveMessage(char const* message_string);

	private:
		bool init();
};

FilepickerPlugin::FilepickerPlugin(LLPluginInstance::sendMessageFunction send_message_function, LLPluginInstance* plugin_instance) :
	BasicPluginBase(send_message_function, plugin_instance)
{
}

FilepickerPlugin::~FilepickerPlugin()
{
}

static LLFilePicker::ESaveFilter str2savefilter(std::string const& filter)
{
  // Complement of AIFilePicker::open(std::string const& filename, ESaveFilter filter, std::string const& folder)
  if (filter == "wav")
	return LLFilePicker::FFSAVE_WAV;
  else if (filter == "tga")
	return LLFilePicker::FFSAVE_TGA;
  else if (filter == "bmp")
	return LLFilePicker::FFSAVE_BMP;
  else if (filter == "avi")
	return LLFilePicker::FFSAVE_AVI;
  else if (filter == "anim")
	return LLFilePicker::FFSAVE_ANIM;
#ifdef _CORY_TESTING
  else if (filter == "geometry")
	return LLFilePicker::FFSAVE_GEOMETRY;
#endif
  else if (filter == "xml")
	return LLFilePicker::FFSAVE_XML;
  else if (filter == "collada")
	return LLFilePicker::FFSAVE_COLLADA;
  else if (filter == "raw")
	return LLFilePicker::FFSAVE_RAW;
  else if (filter == "j2c")
	return LLFilePicker::FFSAVE_J2C;
  else if (filter == "png")
	return LLFilePicker::FFSAVE_PNG;
  else if (filter == "jpeg")
	return LLFilePicker::FFSAVE_JPEG;
  else if (filter == "animatn")
	return LLFilePicker::FFSAVE_ANIMATN;
  else if (filter == "ogg")
	return LLFilePicker::FFSAVE_OGG;
  else if (filter == "notecard")
	return LLFilePicker::FFSAVE_NOTECARD;
  else if (filter == "gesture")
	return LLFilePicker::FFSAVE_GESTURE;
  else if (filter == "lsl")
	return LLFilePicker::FFSAVE_LSL;
  else if (filter == "shape")
	return LLFilePicker::FFSAVE_SHAPE;
  else if (filter == "skin")
	return LLFilePicker::FFSAVE_SKIN;
  else if (filter == "hair")
	return LLFilePicker::FFSAVE_HAIR;
  else if (filter == "eyes")
	return LLFilePicker::FFSAVE_EYES;
  else if (filter == "shirt")
	return LLFilePicker::FFSAVE_SHIRT;
  else if (filter == "pants")
	return LLFilePicker::FFSAVE_PANTS;
  else if (filter == "shoes")
	return LLFilePicker::FFSAVE_SHOES;
  else if (filter == "socks")
	return LLFilePicker::FFSAVE_SOCKS;
  else if (filter == "jacket")
	return LLFilePicker::FFSAVE_JACKET;
  else if (filter == "gloves")
	return LLFilePicker::FFSAVE_GLOVES;
  else if (filter == "undershirt")
	return LLFilePicker::FFSAVE_UNDERSHIRT;
  else if (filter == "underpants")
	return LLFilePicker::FFSAVE_UNDERPANTS;
  else if (filter == "skirt")
	return LLFilePicker::FFSAVE_SKIRT;
  else if (filter == "invgz")
	return LLFilePicker::FFSAVE_INVGZ;
  else if (filter == "landmark")
	return LLFilePicker::FFSAVE_LANDMARK;
  else if (filter == "ao")
	return LLFilePicker::FFSAVE_AO;
  else if (filter == "blacklist")
	return LLFilePicker::FFSAVE_BLACKLIST;
  else if (filter == "physics")
	return LLFilePicker::FFSAVE_PHYSICS;
  else
	return LLFilePicker::FFSAVE_ALL;
}

static LLFilePicker::ELoadFilter str2loadfilter(std::string const& filter)
{
  // Complement of AIFilePicker::open(ELoadFilter filter, std::string const& folder)
  if (filter == "wav")
	return LLFilePicker::FFLOAD_WAV;
  else if (filter == "image")
	return LLFilePicker::FFLOAD_IMAGE;
  else if (filter == "anim")
	return LLFilePicker::FFLOAD_ANIM;
#ifdef _CORY_TESTING
  else if (filter == "geometry")
	return LLFilePicker::FFLOAD_GEOMETRY;
#endif
  else if (filter == "xml")
	return LLFilePicker::FFLOAD_XML;
  else if (filter == "slobject")
	return LLFilePicker::FFLOAD_SLOBJECT;
  else if (filter == "raw")
	return LLFilePicker::FFLOAD_RAW;
  else if (filter == "invgz")
	return LLFilePicker::FFLOAD_INVGZ;
  else if (filter == "ao")
	return LLFilePicker::FFLOAD_AO;
  else if (filter == "blacklist")
	return LLFilePicker::FFLOAD_BLACKLIST;
  else
	return LLFilePicker::FFLOAD_ALL;
}

// This is the SLPlugin process.
// This is part of the loaded DSO.
//
// This function is called from LLPluginInstance::sendMessage
// for messages received from the viewer (that are not 'internal').
void FilepickerPlugin::receiveMessage(char const* message_string)
{
	LLPluginMessage message_in;

	if (message_in.parse(message_string) >= 0)
	{
		std::string message_class = message_in.getClass();
		std::string message_name = message_in.getName();

		if (message_class == LLPLUGIN_MESSAGE_CLASS_BASE)
		{
			if (message_name == "init")
			{
				LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_BASE, "init_response");
				LLSD versions = LLSD::emptyMap();
				versions[LLPLUGIN_MESSAGE_CLASS_BASE] = LLPLUGIN_MESSAGE_CLASS_BASE_VERSION;
				versions[LLPLUGIN_MESSAGE_CLASS_BASIC] = LLPLUGIN_MESSAGE_CLASS_BASIC_VERSION;
				message.setValueLLSD("versions", versions);

				std::string plugin_version = "Filepicker Plugin, version 1.0.0.0";
				message.setValue("plugin_version", plugin_version);
				sendMessage(message);
			}
			else if (message_name == "cleanup")
			{
				// We have no resources that need care. Just do nothing.
			}
			else if (message_name == "idle")
			{
				// This whole message should not have existed imho -- Aleric
			}
			else
			{
				std::cerr << "FilepickerPlugin::receiveMessage: unknown base message: " << message_name << std::endl;
			}
		}
		else if (message_class == LLPLUGIN_MESSAGE_CLASS_BASIC)
		{
			// This message should be sent at most once per SLPlugin invokation.
			if (message_name == "initialization")
			{
				LLSD dictionary = message_in.getValueLLSD("dictionary");
				for (LLSD::map_iterator iter = dictionary.beginMap(); iter != dictionary.endMap(); ++iter)
				{
					translation::add(iter->first, iter->second.asString());
				}
				if (message_in.hasValue("window_id"))
				{
					unsigned long window_id = strtoul(message_in.getValue("window_id").c_str(), NULL, 16);
					LLFilePicker::instance().setWindowID(window_id);
				}
			}
			// This message may theoretically be repeated (though currently the plugin is terminated after returning).
			else if (message_name == "open")
			{
				std::string type = message_in.getValue("type");
				std::string filter = message_in.getValue("filter");
				std::string folder = message_in.getValue("folder");
				bool get_directory = (filter == "directory");

				// We about to completely block on running the modal File/Dir picker window, so flush any pending messages to the viewer.
				flushMessages();

				bool canceled;
				if (get_directory)
				{
					canceled = !LLDirPicker::instance().getDir(folder);
				}
				else if (type == "save")
				{
					canceled = !LLFilePicker::instance().getSaveFile(str2savefilter(filter), message_in.getValue("default"), folder);
				}
				else if (type == "load")
				{
					canceled = !LLFilePicker::instance().getLoadFile(str2loadfilter(filter), folder);
				}
				else // type == "load_multiple"
				{
					canceled = !LLFilePicker::instance().getMultipleLoadFiles(str2loadfilter(filter), folder);
				}
				if (canceled)
				{
					LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_BASIC, "canceled");
					message.setValue("perseus", "unblock");
					sendMessage(message);
				}
				else
				{
					LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_BASIC, "done");
					message.setValue("perseus", "unblock");
					LLSD filenames;
					if (get_directory)
					{
						filenames.append(LLDirPicker::instance().getDirName());
					}
					else
					{
						for (std::string filename = LLFilePicker::instance().getFirstFile(); !filename.empty(); filename = LLFilePicker::instance().getNextFile())
						{
							filenames.append(filename);
						}
					}
					message.setValueLLSD("filenames", filenames);
					sendMessage(message);
				}
				// We're done. Exit the whole application.
				// This first flushes any messages before terminating the plugin.
				sendShutdownMessage();
			}
			else
			{
				std::cerr << "FilepickerPlugin::receiveMessage: unknown basic message: " << message_name << std::endl;
			}
		}
		else
		{
			std::cerr << "FilepickerPlugin::receiveMessage: unknown message class: " << message_class << std::endl;
		}
	}
}

bool FilepickerPlugin::init(void)
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_BASIC, "name_text");
	message.setValue("name", "Filepicker Plugin");
	sendMessage(message);

	return true;
}

int create_plugin(LLPluginInstance::sendMessageFunction send_message_function, LLPluginInstance* plugin_instance, BasicPluginBase** plugin_object)
{
	*plugin_object = new FilepickerPlugin(send_message_function, plugin_instance);
	return 0;
}

