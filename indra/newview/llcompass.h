/** 
 * @file llcompass.h
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

#ifndef LL_LLCOMPASS_H
#define LL_LLCOMPASS_H

#include "llmath.h"
#include "llview.h"
#include "llviewertexture.h"
#ifndef LL_V4COLOR_H
#include "v4color.h"
#endif

class LLCompass : public LLView
{
protected:
	LLPointer<LLViewerTexture>	mTexture;
	LLPointer<LLViewerTexture>	mBkgndTexture;
public:
	LLCompass( const std::string& name, const LLRect& rect);
	
	void			setTexture(LLUUID image_id);
	void			setBkgndTexture(LLUUID image_id);
	virtual void	draw();

	inline friend std::ostream& operator<<(std::ostream& s, LLCompass &compass)
	{
#if 0
						s << "X,Y" << TAB;
		s.width(10);	s << "In (new/total)";
		s.width(0);		s << DIVIDER;
		s.width(10);		s << "Lost (new/total)";
		s.width(0);		s << DIVIDER;
		s.width(5);		s << "Ping";
		s.width(0);		s << DIVIDER;
		s.width(6);		s << "Bandwidth";
		s.width(0);		s << TAB << "IP:port" << endl;
		for (LLCompassInfo* p = compass.mCompassList.getFirstData(); p != NULL; p = compass.mCompassList.getNextData())
		{
			s << *p;
		}
#endif
		return s;
	}
};

class LLHorizontalCompass : public LLView
{
protected:
	LLColor4 mFocusColor;
	LLPointer<LLViewerTexture>	mTexture;

public:
	LLHorizontalCompass( const std::string& name, const LLRect& rect,
						 const LLColor4& focus_color,
						 const LLUUID& image_id );
	virtual ~LLHorizontalCompass( void ) {}

	void setTexture( const LLUUID& image_id );
	virtual void draw();
};

#endif
