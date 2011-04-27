/** 
 * @file llcompass.cpp
 * @brief A little map of the world with network information
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llmath.h"		// clampf()
#include "llmath.h"
#include "llgl.h"

#include "llui.h"

#include "llcircuit.h"
#include "message.h"
#include "llagent.h"
#include "llcompass.h"

#include "llviewertexturelist.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"

//#include "llglheaders.h"

//
// Imported globals
//
extern LLMessageSystem* gMessageSystem;

//
// Constants
//

///----------------------------------------------------------------------------
/// LLCompass
///----------------------------------------------------------------------------

LLCompass::LLCompass( const std::string& name, const LLRect& rect)
		:	LLView(name, rect, FALSE)
{
	mTexture = NULL;
	mBkgndTexture = NULL;

	BOOL state = gSavedSettings.getBOOL( "ShowCompass" );

	setVisible( state );
}

void LLCompass::setTexture(LLUUID image_id)
{
   	mTexture = LLViewerTextureManager::getFetchedTexture(image_id, FALSE, LLViewerTexture::BOOST_UI);
}

void LLCompass::setBkgndTexture(LLUUID image_id)
{
   	mBkgndTexture = LLViewerTextureManager::getFetchedTexture(image_id, FALSE, LLViewerTexture::BOOST_UI);
}

//
// Functions
//

///----------------------------------------------------------------------------
/// LLHorizontalCompass
///----------------------------------------------------------------------------

LLHorizontalCompass::LLHorizontalCompass( const std::string& name, const LLRect& rect,
										  const LLColor4& focus_color,
										  const LLUUID& image_id ) :
	LLView( name, rect, TRUE ),
	mFocusColor( focus_color ),
	mTexture( NULL )
{
	setTexture( image_id );

	BOOL state = gSavedSettings.getBOOL( "ShowCompass" );
	setVisible( state );
}

void LLHorizontalCompass::setTexture( const LLUUID& image_id )
{
   	mTexture = LLViewerTextureManager::getFetchedTexture(image_id, FALSE, LLViewerTexture::BOOST_UI);
}
