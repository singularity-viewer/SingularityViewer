/** 
 * @file demo_plugin.cpp
 * @brief Test plugin to be loaded by the llplugin testbed.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llgl.h"

#include "llplugininstance.h"
#include "llpluginmessage.h"
#include "llpluginmessageclasses.h"

// TODO: Make sure that the only symbol exported from this library is LLPluginInitEntryPoint

class DemoMediaPlugin
{
public:

	static int init(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data, LLPluginInstance::sendMessageFunction *plugin_send_func, void **plugin_user_data);

private:
	DemoMediaPlugin(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data);
	~DemoMediaPlugin();

	static void staticReceiveMessage(const char *message_string, void **user_data);
	void receiveMessage(const char *message_string);
	void sendMessage(const LLPluginMessage &message);
	void setDirty(int left, int top, int right, int bottom);
	
	LLPluginInstance::sendMessageFunction mHostSendFunction;
	void *mHostUserData;
	bool mDeleteMe;
	
	class SharedSegmentInfo
	{
	public:
		void *mAddress;
		size_t mSize;
	};
	
	typedef std::map<std::string, SharedSegmentInfo> SharedSegmentMap;
	
	SharedSegmentMap mSharedSegments;

public:
	
	void clear(void)
	{		
//		return;

		if(mPixels != NULL)
		{
			for( int y = 0; y < mHeight; ++y )
			{
				unsigned char *row = mPixels + ( y * mTextureWidth * mDepth);
				for(int x = 0; x < mWidth; ++x)
				{
					unsigned char *pixel = row + (x * mDepth);
					if(0)
					{
						pixel[0] = 0x20;
						pixel[1] = 0x20;
						pixel[2] = 0x20;
					}
					else
					{
						pixel[0] = rand() % 0x40;
						pixel[1] = rand() % 0x40;
						pixel[2] = rand() % 0x40;
					}
				}
			}
			
			setDirty(0, 0, mWidth, mHeight);
		}
	};

	////////////////////////////////////////////////////////////////////////////////
	//
	void update()
	{
		const time_t interval = 5;
		static time_t last_time = time( NULL );
		time_t cur_time = time( NULL );

		if ( cur_time - last_time > interval )
		{
			clear();

			last_time = cur_time;
		};
	};

	////////////////////////////////////////////////////////////////////////////////
	//
	void write_pixel( int x, int y, unsigned char r, unsigned char g, unsigned char b )
	{
		// make sure we don't write outside the buffer
		if((x < 0) || (x >= mWidth) || (y < 0) || (y >= mHeight))
			return;
			
		if(mPixels != NULL)
		{
			unsigned char *pixel = mPixels;
			pixel += y * mTextureWidth * mDepth;	// row offset
			pixel += (x * mDepth);								// columm offset
			pixel[0] = b;
			pixel[1] = g;
			pixel[2] = r;

			setDirty(x, y, x+1, y+1);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	//
	void mouseDown( int x, int y )
	{
		write_pixel( x, y, 0xff, 0x00, 0x00 );
	};

	////////////////////////////////////////////////////////////////////////////////
	//
	void mouseUp( int x, int y )
	{
		write_pixel( x, y, 0xff, 0xff, 0x00 );
	};

	////////////////////////////////////////////////////////////////////////////////
	//
	void mouseMove( int x, int y )
	{
		write_pixel( x  , y  , 0xff, 0x00, 0xff );
	};

	////////////////////////////////////////////////////////////////////////////////
	//
	void keyPress( unsigned char key )
	{
	};

private:
	unsigned char* mPixels;
	int mWidth;
	int mHeight;
	int mTextureWidth;
	int mTextureHeight;
	int mDepth;
};

int DemoMediaPlugin::init(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data, LLPluginInstance::sendMessageFunction *plugin_send_func, void **plugin_user_data)
{
	DemoMediaPlugin *self = new DemoMediaPlugin(host_send_func, host_user_data);
	*plugin_send_func = staticReceiveMessage;
	*plugin_user_data = (void*)self;

	return 0;
}


DemoMediaPlugin::DemoMediaPlugin(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data)
{
	std::cerr << "DemoMediaPlugin constructor" << std::endl;

	mHostSendFunction = host_send_func;
	mHostUserData = host_user_data;
	mDeleteMe = false;
}

DemoMediaPlugin::~DemoMediaPlugin()
{
	std::cerr << "DemoMediaPlugin destructor" << std::endl;
}

void DemoMediaPlugin::staticReceiveMessage(const char *message_string, void **user_data)
{
	DemoMediaPlugin *self = (DemoMediaPlugin*)*user_data;
	
	if(self != NULL)
	{
		self->receiveMessage(message_string);
	
		// If the plugin has processed the delete message, delete it.
		if(self->mDeleteMe)
		{
			delete self;
			*user_data = NULL;
		}
	}
}

void DemoMediaPlugin::receiveMessage(const char *message_string)
{
//	std::cerr << "DemoMediaPlugin::receiveMessage: received message: \"" << message_string << "\"" << std::endl;
	LLPluginMessage message_in;
	
	if(message_in.parse(message_string) >= 0)
	{
		std::string message_class = message_in.getClass();
		std::string message_name = message_in.getName();
		if(message_class == LLPLUGIN_MESSAGE_CLASS_BASE)
		{
			if(message_name == "init")
			{
				LLPluginMessage message("base", "init_response");
				LLSD versions = LLSD::emptyMap();
				versions[LLPLUGIN_MESSAGE_CLASS_BASE] = LLPLUGIN_MESSAGE_CLASS_BASE_VERSION;
				versions[LLPLUGIN_MESSAGE_CLASS_MEDIA] = LLPLUGIN_MESSAGE_CLASS_MEDIA_VERSION;
				// Normally a plugin would only specify one of these two subclasses, but this is a demo...
				versions[LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER] = LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER_VERSION;
				versions[LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME] = LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME_VERSION;
				message.setValueLLSD("versions", versions);
				sendMessage(message);
				
				// Plugin gets to decide the texture parameters to use.
				mDepth = 3;
				
				message.setMessage(LLPLUGIN_MESSAGE_CLASS_MEDIA, "texture_params");
				message.setValueS32("depth", mDepth);
				message.setValueU32("internalformat", GL_RGB);
				message.setValueU32("format", GL_RGB);
				message.setValueU32("type", GL_UNSIGNED_BYTE);
				message.setValueBoolean("coords_opengl", false);	// true == use OpenGL-style coordinates, false == (0,0) is upper left.
				sendMessage(message);
			}
			else if(message_name == "idle")
			{
				// no response is necessary here.
				update();
			}
			else if(message_name == "shutdown")
			{
				sendMessage(LLPluginMessage("base", "shutdown_response"));
				
				mDeleteMe = true;
			}
			else if(message_name == "shm_added")
			{
				SharedSegmentInfo info;
				info.mAddress =  (void*)message_in.getValueU32("address");
				info.mSize = (size_t)message_in.getValueS32("size");
				std::string name = message_in.getValue("name");
				
				
				std::cerr << "DemoMediaPlugin::receiveMessage: shared memory added, name: " << name 
					<< ", size: " << info.mSize 
					<< ", address: " << info.mAddress 
					<< std::endl;

				mSharedSegments.insert(SharedSegmentMap::value_type(name, info));
			
			}
			else if(message_name == "shm_remove")
			{
				std::string name = message_in.getValue("name");
				
				std::cerr << "DemoMediaPlugin::receiveMessage: shared memory remove, name = " << name << std::endl;

				SharedSegmentMap::iterator iter = mSharedSegments.find(name);
				if(iter != mSharedSegments.end())
				{
					if(mPixels == iter->second.mAddress)
					{
						// This is the currently active pixel buffer.  Make sure we stop drawing to it.
						mPixels = NULL;
					}
					mSharedSegments.erase(iter);
				}
				else
				{
					std::cerr << "DemoMediaPlugin::receiveMessage: unknown shared memory region!" << std::endl;
				}

				// Send the response so it can be cleaned up.
				LLPluginMessage message("base", "shm_remove_response");
				message.setValue("name", name);
				sendMessage(message);
			}
			else
			{
				std::cerr << "DemoMediaPlugin::receiveMessage: unknown base message: " << message_name << std::endl;
			}
		}
		else if(message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA)
		{
			if(message_name == "size_change")
			{
				std::string name = message_in.getValue("name");
				S32 width = message_in.getValueS32("width");
				S32 height = message_in.getValueS32("height");
				S32 texture_width = message_in.getValueS32("texture_width");
				S32 texture_height = message_in.getValueS32("texture_height");
				
				LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "size_change_response");
				message.setValue("name", name);
				message.setValueS32("width", width);
				message.setValueS32("height", height);
				message.setValueS32("texture_width", texture_width);
				message.setValueS32("texture_height", texture_height);
				sendMessage(message);

				if(!name.empty())
				{
					// Find the shared memory region with this name
					SharedSegmentMap::iterator iter = mSharedSegments.find(name);
					if(iter != mSharedSegments.end())
					{
						std::cerr << "Got size change, new size is " << width << " by " << height << std::endl;
						std::cerr << "    texture size is " << texture_width << " by " << texture_height << std::endl;
						
						mPixels = (unsigned char*)iter->second.mAddress;
						mWidth = width;
						mHeight = height;
						mTextureWidth = texture_width;
						mTextureHeight = texture_height;
						
						clear();
					}
				}
			}
			else if(message_name == "mouse_event")
			{
				std::string event = message_in.getValue("event");
				S32 x = message_in.getValueS32("x");
				S32 y = message_in.getValueS32("y");
// 				std::string modifiers = message.getValue("modifiers");

//				std::cerr << "DemoMediaPlugin::receiveMessage: mouse event \"" << event 
//					<< "\", coords " << x << ", " << y
//					<< std::endl;
				
				if(event == "down")
				{
					mouseDown(x, y);
				}
				else if(event == "up")
				{
					mouseUp(x, y);
				}
				else if(event == "move")
				{
					mouseMove(x, y);
				}
			}
			else
			{
				std::cerr << "DemoMediaPlugin::receiveMessage: unknown media message: " << message_string << std::endl;
			}
		}
		else if(message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER)
		{
			if(message_name == "focus")
			{
				// foo = message_in.getValueBoolean("focused");
			}
			else if(message_name == "clear_cache")
			{
			}
			else if(message_name == "clear_cookies")
			{
			}
			else if(message_name == "enable_cookies")
			{
				// foo = message_in.getValueBoolean("enable");
			}
			else if(message_name == "proxy_setup")
			{
				// foo = message_in.getValueBoolean("enable");
				// bar = message_in.getValue("host");
				// baz = message_in.getValueS32("port");
			}
			else if(message_name == "browse_stop")
			{
			}
			else if(message_name == "browse_reload")
			{
				// foo = message_in.getValueBoolean("ignore_cache");
			}
			else if(message_name == "browse_forward")
			{
			}
			else if(message_name == "browse_back")
			{
			}
			else if(message_name == "set_status_redirect")
			{
				// foo = message_in.getValueS32("code");
				// bar = message_in.getValue("url");
			}
			else
			{
				std::cerr << "DemoMediaPlugin::receiveMessage: unknown media_browser message: " << message_string << std::endl;
			}
		}
		else
		{
			std::cerr << "DemoMediaPlugin::receiveMessage: unknown message class: " << message_class << std::endl;
		}

	}
}

void DemoMediaPlugin::sendMessage(const LLPluginMessage &message)
{
	std::string output = message.generate();
	mHostSendFunction(output.c_str(), &mHostUserData);
}

void DemoMediaPlugin::setDirty(int left, int top, int right, int bottom)
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "updated");
	
	message.setValueS32("left", left);
	message.setValueS32("top", top);
	message.setValueS32("right", right);
	message.setValueS32("bottom", bottom);
	
	sendMessage(message);
}


extern "C"
{
#ifdef WIN32
	__declspec(dllexport)
#endif
		int LLPluginInitEntryPoint(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data, LLPluginInstance::sendMessageFunction *plugin_send_func, void **plugin_user_data);
}

int 
#ifdef WIN32
	__declspec(dllexport)
#endif
	LLPluginInitEntryPoint(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data, LLPluginInstance::sendMessageFunction *plugin_send_func, void **plugin_user_data)
{
	return DemoMediaPlugin::init(host_send_func, host_user_data, plugin_send_func, plugin_user_data);
}

#ifdef WIN32
int WINAPI DllEntryPoint( HINSTANCE hInstance, unsigned long reason, void* params )
{
	return 1;
}
#endif
