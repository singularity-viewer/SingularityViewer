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
#include "indra_constants.h"

#include "lltimer.h" // for ms_sleep()
#include "llpumpio.h"
#include "llapr.h"
#include "llerrorcontrol.h"
#include "llpluginclassmedia.h"

#include <string>
#include <iostream>
#include <stdlib.h>
#include <time.h>

#include "llmediaplugintest.h" // for GLUT headers


////////////////////////////////////////////////////////////////////////////////
//
class mediaPluginTest : public LLPluginClassMediaOwner
{
	private:
		int mAppWindowWidth;
		int mAppWindowHeight;
		LLPluginClassMedia* mMediaSource;
		int mAppTextureWidth;
		int mAppTextureHeight;
		bool mAppTextureCoordsOpenGL;
		GLuint mAppTexture;
		std::string mAppWindowName;
		std::string mHomeUrl;
		std::string mLauncherFilename;
		std::string mPluginFilename;

		LLPluginClassMedia *mPlugin;

	public:
		mediaPluginTest(const std::string &launcher_filename, const std::string &plugin_filename) :
			mAppWindowWidth( 800 ),
			mAppWindowHeight( 600 ),
			mAppTextureWidth( 0 ),
			mAppTextureHeight( 0 ),
			mAppTextureCoordsOpenGL(false),
			mAppTexture( 0 ),
			mAppWindowName( "Media Simple Test" ),
			mHomeUrl( "" )
		{
			mLauncherFilename = launcher_filename;
			mMediaSource = new LLPluginClassMedia(this);
			mMediaSource->init(launcher_filename, plugin_filename);
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		virtual ~mediaPluginTest()
		{
			delete mMediaSource;
		};
		
		////////////////////////////////////////////////////////////////////////////////
		//
		void createTexture()
		{
			// create the texture used to display the browser data
			if(mMediaSource->textureValid())
			{
				mAppTextureWidth = mMediaSource->getTextureWidth();
				mAppTextureHeight = mMediaSource->getTextureHeight();
				mAppTextureCoordsOpenGL = mMediaSource->getTextureCoordsOpenGL();
				
				if(mAppTexture != 0)
				{
					glDeleteTextures( 1, &mAppTexture );
					mAppTexture = 0;
				}
				
				glGenTextures( 1, &mAppTexture );
				glBindTexture( GL_TEXTURE_2D, mAppTexture );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
				glTexImage2D( GL_TEXTURE_2D, 0,
						mMediaSource->getTextureFormatInternal(),
						mAppTextureWidth, 
						mAppTextureHeight,
						0, 
						mMediaSource->getTextureFormatPrimary(), 
						mMediaSource->getTextureFormatType(), 
						NULL );
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		//
		void initGL()
		{
			// OpenGL initialization
			glClearColor( 0.0f, 0.0f, 0.0f, 0.5f);
			glEnable( GL_COLOR_MATERIAL );
			glColorMaterial( GL_FRONT, GL_AMBIENT_AND_DIFFUSE );
			glEnable( GL_TEXTURE_2D );
			glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
			glEnable( GL_CULL_FACE );
		}

		////////////////////////////////////////////////////////////////////////////////
		//
		void reshape( int width, int height )
		{
			if ( height == 0 )
				height = 1;

			glMatrixMode( GL_PROJECTION );
			glLoadIdentity();

			glViewport( 0, 0, width, height );
			glOrtho( 0.0f, width, height, 0.0f, -1.0f, 1.0f );

			// we use these values elsewhere so save
			mAppWindowWidth = width;
			mAppWindowHeight = height;
			
			// Request a media size change
			mMediaSource->setSize(width, height);
			
			glMatrixMode( GL_MODELVIEW );
			glLoadIdentity();

			glutPostRedisplay();
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		void idle()
		{
			// lots of updates for smooth motion
			glutPostRedisplay();
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		void display()
		{
			mMediaSource->idle();

			// Check whether the texture needs to be recreated.
			if(mMediaSource->textureValid())
			{
				if(
					(mAppTextureWidth != mMediaSource->getTextureWidth() ||	mAppTextureHeight != mMediaSource->getTextureHeight()) &&
					(mAppWindowWidth == mMediaSource->getWidth() && mAppWindowHeight == mMediaSource->getHeight())
				)
				{
					// Attempt to (re)create the texture
					createTexture();
				}
			}
						
			// clear screen
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			glLoadIdentity();
			
			if(mAppTexture != 0)
			{
				// use the browser texture
				glBindTexture( GL_TEXTURE_2D, mAppTexture );

				// If dirty, update the texture.
				LLRect dirtyRect;
				if(!mMediaSource->textureValid())
				{
//					LL_DEBUGS("media_plugin_test") << "Resize in progress, skipping update..." << LL_ENDL;
				}
				else if(mAppWindowWidth != mMediaSource->getWidth() || mAppWindowHeight != mMediaSource->getHeight())
				{
					// A resize is in progress.  Just wait for it...
				}
				else if(mMediaSource->getDirty(&dirtyRect))
				{
					// grab the page
					const unsigned char* pixels = mMediaSource->getBitsData();
					if ( pixels )
					{
						// write them into the texture
						
						// Paranoia: intersect dirtyRect with (0, 0, mAppTextureWidth, mAppTextureHeight)?

						int x_offset = dirtyRect.mLeft;
						int y_offset = dirtyRect.mBottom;
						int width = dirtyRect.mRight - dirtyRect.mLeft;
						int height = dirtyRect.mTop - dirtyRect.mBottom;

						LL_DEBUGS("media_plugin_test") << "Updating, dirty rect is (" 
							<< dirtyRect.mLeft << ", " 
							<< dirtyRect.mTop << ", " 
							<< dirtyRect.mRight << ", " 
							<< dirtyRect.mBottom << "), update params are: (" 
							<< x_offset << ", " 
							<< y_offset << ", " 
							<< width << ", " 
							<< height << ")" 
							<< LL_ENDL; 
						
						// Offset the pixels pointer properly
						pixels += (y_offset * mMediaSource->getTextureDepth() * mMediaSource->getTextureWidth());
						pixels += (x_offset * mMediaSource->getTextureDepth());
						
						glPixelStorei(GL_UNPACK_ROW_LENGTH, mMediaSource->getTextureWidth());
						
						glTexSubImage2D( GL_TEXTURE_2D, 0, 
							x_offset, 
							y_offset,
							width, 
							height,
							mMediaSource->getTextureFormatPrimary(), 
							mMediaSource->getTextureFormatType(), 
							pixels );
						
						mMediaSource->resetDirty();
					}
				}
				
				// scale the texture so that it fits the screen
				GLdouble media_texture_x = mAppWindowWidth / (double)mAppTextureWidth;
				GLdouble media_texture_y = mAppWindowHeight / (double)mAppTextureHeight;

				// draw the single quad full screen (orthographic)

				glEnable( GL_TEXTURE_2D );
				glColor3f( 1.0f, 1.0f, 1.0f );
				glBegin( GL_QUADS );
				if(mAppTextureCoordsOpenGL)
				{
					// Render the texture as per opengl coords (where 0,0 is at the lower left)
					glTexCoord2d( 0, 0 );
					glVertex2d( 0, 0 );

					glTexCoord2d( 0 , media_texture_y );
					glVertex2d( 0, mAppWindowHeight);

					glTexCoord2d( media_texture_x, media_texture_y );
					glVertex2d( mAppWindowWidth , mAppWindowHeight);

					glTexCoord2d( media_texture_x, 0 );
					glVertex2d( mAppWindowWidth, 0 );
				}
				else
				{
					// Render the texture the "other way round" (where 0,0 is at the upper left)
					glTexCoord2d( 0, media_texture_y );
					glVertex2d( 0, 0 );

					glTexCoord2d( 0 , 0 );
					glVertex2d( 0, mAppWindowHeight);

					glTexCoord2d( media_texture_x, 0 );
					glVertex2d( mAppWindowWidth , mAppWindowHeight);

					glTexCoord2d( media_texture_x, media_texture_y );
					glVertex2d( mAppWindowWidth, 0 );
				}
				glEnd();

			}
			
			glutSwapBuffers();
		};
		
		
		////////////////////////////////////////////////////////////////////////////////
		//
		MASK getModifiers(void)
		{
			MASK result = 0;
			
			int modifiers = glutGetModifiers();
			
			if(modifiers & GLUT_ACTIVE_SHIFT)
				result |= MASK_SHIFT;
			if(modifiers & GLUT_ACTIVE_CTRL)
				result |= MASK_CONTROL;
			if(modifiers & GLUT_ACTIVE_ALT)
				result |= MASK_ALT;

			return result;
		}
		
		////////////////////////////////////////////////////////////////////////////////
		//
		void mouseButton( int button, int state, int x, int y )
		{
			// Texture has been scaled so it's 1:1 with screen pixels, so no need to scale mouse coords here.
//			x = ( x * mAppTextureWidth ) / mAppWindowWidth;
//			y = ( y * mAppTextureHeight ) / mAppWindowHeight;

			if ( button == GLUT_LEFT_BUTTON )
			{
				if ( state == GLUT_DOWN )
					mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOWN, x, y, getModifiers());
				else if ( state == GLUT_UP )
					mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, x, y, getModifiers());
			}

			// force a GLUT update
			glutPostRedisplay();
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		void mouseMove( int x , int y )
		{
			// Texture has been scaled so it's 1:1 with screen pixels, so no need to scale mouse coords here.
//			x = ( x * mAppTextureWidth ) / mAppWindowWidth;
//			y = ( y * mAppTextureHeight ) / mAppWindowHeight;
			
			// GLUT complains if I get the keyboard modifiers here, so just pretend there aren't any.
			mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_MOVE, x, y, 0);

			// force a GLUT update
			glutPostRedisplay();
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		void keyboard( unsigned char key )
		{
			mMediaSource->keyEvent( LLPluginClassMedia::KEY_EVENT_DOWN, key, getModifiers());
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		int getAppWindowWidth()
		{
			return mAppWindowWidth;
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		int getAppWindowHeight()
		{
			return mAppWindowHeight;
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		std::string getAppWindowName()
		{
			return mAppWindowName;
		};

};

mediaPluginTest* gApplication;

////////////////////////////////////////////////////////////////////////////////
//
void glutReshape( int width, int height )
{
	if ( gApplication )
		gApplication->reshape( width, height );
};

////////////////////////////////////////////////////////////////////////////////
//
void glutDisplay()
{
	if ( gApplication )
		gApplication->display();
};

////////////////////////////////////////////////////////////////////////////////
//
void glutIdle()
{
	if ( gApplication )
		gApplication->idle();
};

////////////////////////////////////////////////////////////////////////////////
//
void glutKeyboard( unsigned char key, int x, int y )
{
	if ( key == 27 )
	{
		delete gApplication;
		exit( 0 );
	};

	if ( gApplication )
		gApplication->keyboard( key );
};

////////////////////////////////////////////////////////////////////////////////
//
void glutMouseMove( int x, int y )
{
	if ( gApplication )
		gApplication->mouseMove( x, y );
}

////////////////////////////////////////////////////////////////////////////////
//
void glutMouseButton( int button, int state, int x, int y )
{
	if ( gApplication )
		gApplication->mouseButton( button, state, x, y );
}

////////////////////////////////////////////////////////////////////////////////
//
int main( int argc, char* argv[] )
{

	ll_init_apr();

	// Set up llerror logging 
	{
		LLError::initForApplication(".");
		LLError::setDefaultLevel(LLError::LEVEL_INFO);
	}
	
	std::string launcher_name;
	std::string plugin_name;

	if(argc >= 3)
	{
		launcher_name = argv[1];
		plugin_name = argv[2];
	}
	else
	{
#if LL_DARWIN
		// hardcoding the testbed arguments by default
		launcher_name = "plugin_process_host";
		plugin_name = "libdemo_media_plugin_quicktime.dylib";
#elif LL_WINDOWS
		// hardcoding the testbed arguments by default
		launcher_name = "plugin_process_host.exe";
		plugin_name = "demo_media_plugin_quicktime.dll";
#else
		LL_ERRS("plugin_process_launcher") << "usage: " << argv[0] << " launcher_filename plugin_filename" << LL_ENDL;
#endif
	}

	gApplication = new mediaPluginTest(launcher_name, plugin_name);

	if ( gApplication )
	{
		glutInit( &argc, argv );
		glutInitDisplayMode( GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGB );

		glutInitWindowPosition( 80, 0 );
		glutInitWindowSize( gApplication->getAppWindowWidth(), gApplication->getAppWindowHeight() );

		glutCreateWindow( gApplication->getAppWindowName().c_str() );

		glutKeyboardFunc( glutKeyboard );

		glutMouseFunc( glutMouseButton );
		glutPassiveMotionFunc( glutMouseMove );
		glutMotionFunc( glutMouseMove );

		glutDisplayFunc( glutDisplay );
		glutReshapeFunc( glutReshape );

		glutIdleFunc( glutIdle );

		gApplication->initGL();

		glutMainLoop();

		delete gApplication;
	};

	ll_cleanup_apr();

	return 0;
}

