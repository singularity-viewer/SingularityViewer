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

#include <string>
#include <iostream>
#include <stdlib.h>
#include <time.h>

#if LL_DARWIN
	#include <GLUT/glut.h>
#elif LL_LINUX
	#include <GL/glut.h>
#else
	#include "glut.h"
#endif

////////////////////////////////////////////////////////////////////////////////
//
class mediaSource
{
	public:
		////////////////////////////////////////////////////////////////////////////////
		//
		mediaSource() :
			mPixels( 0 ),
			mIsDirty( false ),
			mWidth( 200 ),
			mHeight( 100 ),
			mDepth( 3 ),
			mPixelFormat( GL_BGR_EXT )
		{
			mPixels = new unsigned char [ mWidth * mHeight * mDepth ];

			for( int i = 0; i < mWidth * mHeight * mDepth; ++i )
				*( mPixels + i ) = rand() % 0x40;
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		void update()
		{
			const time_t interval = 1;
			static time_t last_time = time( NULL );
			time_t cur_time = time( NULL );

			if ( cur_time - last_time > interval )
			{
				for( int i = 0; i < mWidth * mHeight * mDepth; ++i )
					*( mPixels + i ) = rand() % 0x40;

				mIsDirty = true;

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
				
			*( mPixels + ( mHeight - y ) * mWidth * mDepth + x * mDepth + 0 ) = b;
			*( mPixels + ( mHeight - y ) * mWidth * mDepth + x * mDepth + 1 ) = g;
			*( mPixels + ( mHeight - y ) * mWidth * mDepth + x * mDepth + 2 ) = r;

			mIsDirty = true;
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
			write_pixel( x, y, 0xff, 0x00, 0xff );
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		void keyPress( unsigned char key )
		{
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		int getWidth() { return mWidth; };
		int getHeight() { return mHeight; };
		int getDepth() { return mDepth; };
		int getPixelFormat() { return mPixelFormat; };
		bool isDirty() { return mIsDirty; };
		void ackDirty() { mIsDirty = false; };
		unsigned char* getPixels() { return mPixels; };

	private:
		unsigned char* mPixels;
		bool mIsDirty;
		int mWidth;
		int mHeight;
		int mDepth;
		int mPixelFormat;
};

////////////////////////////////////////////////////////////////////////////////
//
class mediaSimpleTest
{
	public:
		mediaSimpleTest() :
			mAppWindowWidth( 800 ),
			mAppWindowHeight( 600 ),
			mAppTextureWidth( 0 ),
			mAppTextureHeight( 0 ),
			mAppTexture( 0 ),
			mAppWindowName( "Media Simple Test" ),
			mHomeUrl( "" )
		{
			mMediaSource = new mediaSource;

			// calculate texture size required (next power of two above browser window size
			for ( mAppTextureWidth = 1; mAppTextureWidth < mMediaSource->getWidth(); mAppTextureWidth <<= 1 ) {};
			for ( mAppTextureHeight = 1; mAppTextureHeight < mMediaSource->getHeight(); mAppTextureHeight <<= 1 ) {};
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		~mediaSimpleTest()
		{
			delete mMediaSource;
		};

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

			// create the texture used to display the browser data
			glGenTextures( 1, &mAppTexture );
			glBindTexture( GL_TEXTURE_2D, mAppTexture );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexImage2D( GL_TEXTURE_2D, 0,
				GL_RGB,
					mAppTextureWidth, mAppTextureHeight,
						0, GL_RGB, GL_UNSIGNED_BYTE, 0 );
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
			// clear screen
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			glLoadIdentity();

			// use the browser texture
			glBindTexture( GL_TEXTURE_2D, mAppTexture );

			// needs to be updated?
			mMediaSource->update();
			if ( mMediaSource->isDirty() )
			{
				// grab the page
				const unsigned char* pixels = mMediaSource->getPixels();
				if ( pixels )
				{
					// write them into the texture
					glTexSubImage2D( GL_TEXTURE_2D, 0,
						0, 0,
							mMediaSource->getWidth(),
								mMediaSource->getHeight(),
										mMediaSource->getPixelFormat(),
											GL_UNSIGNED_BYTE,
												pixels );

					// acknowledge we saw media was dirty and updated
					mMediaSource->ackDirty();
				};
			};

			// scale the texture so that it fits the screen
			GLfloat texture_scale_x = ( GLfloat )mMediaSource->getWidth() / ( GLfloat )mAppTextureWidth;
			GLfloat texture_scale_y = ( GLfloat )mMediaSource->getHeight() / ( GLfloat )mAppTextureHeight;

			// draw the single quad full screen (orthographic)
			glMatrixMode( GL_TEXTURE );
			glPushMatrix();
			glScalef( texture_scale_x, texture_scale_y, 1.0f );

			glEnable( GL_TEXTURE_2D );
			glColor3f( 1.0f, 1.0f, 1.0f );
			glBegin( GL_QUADS );
				glTexCoord2f( 1.0f, 1.0f );
				glVertex2d( mAppWindowWidth, 0 );

				glTexCoord2f( 0.0f, 1.0f );
				glVertex2d( 0, 0 );

				glTexCoord2f( 0.0f, 0.0f );
				glVertex2d( 0, mAppWindowHeight );

				glTexCoord2f( 1.0f, .0f );
				glVertex2d( mAppWindowWidth, mAppWindowHeight );
			glEnd();

			glMatrixMode( GL_TEXTURE );
			glPopMatrix();

			glutSwapBuffers();
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		void mouseButton( int button, int state, int x, int y )
		{
			// texture is scaled to fit the screen so we scale mouse coords in the same way
			x = ( x * mMediaSource->getWidth() ) / mAppWindowWidth;
			y = ( y * mMediaSource->getHeight() ) / mAppWindowHeight;

			if ( button == GLUT_LEFT_BUTTON )
			{
				if ( state == GLUT_DOWN )
					mMediaSource->mouseDown( x, y );
				else
				if ( state == GLUT_UP )
					mMediaSource->mouseUp( x, y );
			};

			// force a GLUT update
			glutPostRedisplay();
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		void mouseMove( int x , int y )
		{
			// texture is scaled to fit the screen so we scale mouse coords in the same way
			x = ( x * mMediaSource->getWidth() ) / mAppWindowWidth;
			y = ( y * mMediaSource->getHeight() ) / mAppWindowHeight;

			mMediaSource->mouseMove( x, y );

			// force a GLUT update
			glutPostRedisplay();
		};

		////////////////////////////////////////////////////////////////////////////////
		//
		void keyboard( unsigned char key )
		{
			mMediaSource->keyPress( key );
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

	private:
		int mAppWindowWidth;
		int mAppWindowHeight;
		mediaSource* mMediaSource;
		int mAppTextureWidth;
		int mAppTextureHeight;
		GLuint mAppTexture;
		std::string mAppWindowName;
		std::string mHomeUrl;
};

mediaSimpleTest* gApplication;

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
	gApplication = new mediaSimpleTest;

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

	return 0;
}

