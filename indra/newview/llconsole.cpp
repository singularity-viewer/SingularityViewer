/** 
 * @file llconsole.cpp
 * @brief a scrolling console output device
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

#include "llconsole.h"

// linden library includes
#include "llmath.h"
#include "llviewercontrol.h"
#include "llcriticaldamp.h"
#include "llfontgl.h"
#include "llgl.h"
#include "llui.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "lltextparser.h"
#include "llsd.h"
#include "llfontgl.h"
#include "llmath.h"

#include "llstartup.h"

LLConsole* gConsole = NULL;  // Created and destroyed in LLViewerWindow.

const F32 FADE_DURATION = 2.f;
const S32 MIN_CONSOLE_WIDTH = 50;

// Why don't these match?
const S32 CONSOLE_GUTTER_LEFT = 14;
const S32 CONSOLE_GUTTER_RIGHT = 15;


LLConsole::LLConsole(const std::string& name, const LLRect &rect, 
					 S32 font_size_index, F32 persist_time ) 
  : LLLineBuffer(),
	LLView(name, rect, FALSE),
	mLinePersistTime(persist_time),
	mFadeTime(persist_time - FADE_DURATION),
	mFont(LLFontGL::getFontSansSerif()),
	mConsoleWidth(0),
	mConsoleHeight(0),
	mQueueMutex(NULL)
{
	mTimer.reset();

	setFontSize( font_size_index ); 	
}

LLConsole::~LLConsole()
{
	clear();
}

void LLConsole::setLinePersistTime(F32 seconds)
{
	mLinePersistTime = seconds;
	mFadeTime = mLinePersistTime - FADE_DURATION;
}

void LLConsole::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	S32 new_width = llmax(MIN_CONSOLE_WIDTH, llmin(getRect().getWidth(), gViewerWindow->getWindowWidth()));
	S32 line_height = mFont ? llfloor(mFont->getLineHeight()) : 0;
    S32 new_height = llclamp(line_height + 15, getRect().getHeight(), gViewerWindow->getWindowHeight());

	if (   mConsoleWidth == new_width
		&& mConsoleHeight == new_height )
	{
		return;
	}
	
	mConsoleWidth = new_width;
	mConsoleHeight= new_height;
	
	LLView::reshape(new_width, new_height, called_from_parent);

	for(paragraph_t::iterator paragraph_it = mParagraphs.begin(); paragraph_it != mParagraphs.end(); paragraph_it++)
	{
		(*paragraph_it)->updateLines((F32)getRect().getWidth(), mFont, true);
	}
}

void LLConsole::setFontSize(S32 size_index)
{
	if (-1 == size_index)
	{
		mFont = LLFontGL::getFontMonospace();
	}
	else if (0 == size_index)
	{
		mFont = LLFontGL::getFontSansSerif();
	}
	else if (1 == size_index)
	{
		mFont = LLFontGL::getFontSansSerifBig();
	}
	else
	{
		mFont = LLFontGL::getFontSansSerifHuge();
	}
	
	for(paragraph_t::iterator paragraph_it = mParagraphs.begin(); paragraph_it != mParagraphs.end(); paragraph_it++)
	{
		(*paragraph_it)->updateLines((F32)getRect().getWidth(), mFont, true);
	}
}

void LLConsole::draw()
{
	LLGLSUIDefault gls_ui;

	{
		LLMutexLock lock(&mQueueMutex);
		for(paragraph_t::iterator paragraph_it = mNewParagraphs.begin(); paragraph_it != mNewParagraphs.end(); paragraph_it++)
		{
			Paragraph* paragraph = *paragraph_it;
			mParagraphs.push_back(paragraph);
			paragraph->updateLines((F32)getRect().getWidth(), mFont);
		}
		mNewParagraphs.clear();
	}

	if (mParagraphs.empty()) 	//No text to draw.
	{
		return;
	}

	// skip lines added more than mLinePersistTime ago
	F32 cur_time = mTimer.getElapsedTimeF32();
	
	F32 skip_time = cur_time - mLinePersistTime;
	F32 fade_time = cur_time - mFadeTime;

	U32 max_lines = gSavedSettings.getS32("ConsoleMaxLines");
	U32 num_lines=0;

	paragraph_t::reverse_iterator paragraph_it;
	paragraph_it = mParagraphs.rbegin();
	U32 paragraph_num=mParagraphs.size();

	while (!mParagraphs.empty() && paragraph_it != mParagraphs.rend())
	{
		num_lines += (*paragraph_it)->mLines.size();
		if(num_lines > max_lines 
			|| ( (mLinePersistTime > (F32)0.f) && ((*paragraph_it)->mAddTime - skip_time)/(mLinePersistTime - mFadeTime) <= (F32)0.f)) 
		{							//All lines above here are done.  Lose them.
			for (U32 i=0;i<paragraph_num;i++)
			{
				if (!mParagraphs.empty())
				{
					Paragraph* paragraph = mParagraphs.front();
					mParagraphs.pop_front();
					delete paragraph;
				}
			}
			break;
		}
		paragraph_num--;
		paragraph_it++;
	}

	if (mParagraphs.empty())
	{
		return;
	}
	
	// draw remaining lines
	F32 y_pos = 0.f;

	LLUIImagePtr imagep = LLUI::getUIImage("rounded_square.tga");

	F32 console_opacity = llclamp(gSavedSettings.getF32("ConsoleBackgroundOpacity"), 0.f, 1.f);
	LLColor4 color = gColors.getColor("ConsoleBackground");
	color.mV[VALPHA] *= console_opacity;

	F32 line_height = mFont->getLineHeight();

	S32 message_spacing = 0;
	
//080813 Spatters:  This section makes a single huge black box behind all the text.
	S32 bkg_height=8;
	S32 bkg_width=0;

// VWR-8999
// ChatSpacing:   0 -- chat lines are close together, as they were in the 1.20 viewer.
//                4 -- chat lines are farther apart as they are in SnowGlobe 1.4.
	static LLCachedControl<S32> chat_spacing("ChatSpacing", 0);
	// Perform clamping.
	S32 const clamped_chat_spacing = llclamp((S32)chat_spacing, -16, 128);
	if (chat_spacing != clamped_chat_spacing)
	{
		chat_spacing = clamped_chat_spacing;
	}
	// Adjust spacing.
	message_spacing += chat_spacing;
	bkg_height -= chat_spacing;

	for(paragraph_it = mParagraphs.rbegin(); paragraph_it != mParagraphs.rend(); paragraph_it++)
	{
		S32 target_height = llfloor( (*paragraph_it)->mLines.size() * line_height + message_spacing);
		S32 target_width =  llfloor( (*paragraph_it)->mMaxWidth + CONSOLE_GUTTER_RIGHT);
		
		bkg_height+= target_height;
		if (target_width > bkg_width)
		{
			bkg_width=target_width;
		}

		// Why is this not using llfloor as above?
		y_pos += ((*paragraph_it)->mLines.size()) * line_height;
		y_pos += message_spacing;  //Extra spacing between messages.
	}
	imagep->drawSolid(-CONSOLE_GUTTER_LEFT, (S32)(y_pos + line_height - bkg_height - message_spacing), bkg_width, bkg_height, color);
	y_pos = 0.f;
//End screen-eating black void

	for(paragraph_it = mParagraphs.rbegin(); paragraph_it != mParagraphs.rend(); paragraph_it++)
	{
//080813 Spatters:  Dainty per-message block boxes
//		S32 target_height = llfloor( (*paragraph_it)->mLines.size() * line_height + 8);
		S32 target_width =  llfloor( (*paragraph_it)->mMaxWidth + CONSOLE_GUTTER_RIGHT);

		y_pos += ((*paragraph_it)->mLines.size()) * line_height;
//080813 Spatters:  Dainty per-message block boxes
//		imagep->drawSolid(-14, (S32)(y_pos + line_height - target_height), target_width, target_height, color);

		F32 y_off=0;

		F32 alpha;

		if ((mLinePersistTime > 0.f) && ((*paragraph_it)->mAddTime < fade_time))
		{
			alpha = ((*paragraph_it)->mAddTime - skip_time)/(mLinePersistTime - mFadeTime);
		}
		else
		{
			alpha = 1.0f;
		}

		if( alpha > 0.f )
		{
			for (lines_t::iterator line_it=(*paragraph_it)->mLines.begin(); 
					line_it != (*paragraph_it)->mLines.end();
					line_it ++)
			{
				for (line_color_segments_t::iterator seg_it = (*line_it).begin();
						seg_it != (*line_it).end();
						seg_it++)
				{
					mFont->render((*seg_it).mText, 0, (*seg_it).mXPosition - 8, y_pos -  y_off,
						LLColor4(
							(*seg_it).mColor.mV[VRED], 
							(*seg_it).mColor.mV[VGREEN], 
							(*seg_it).mColor.mV[VBLUE], 
							(*seg_it).mColor.mV[VALPHA]*alpha),
						LLFontGL::LEFT, 
						LLFontGL::BASELINE,
						LLFontGL::DROP_SHADOW,
						S32_MAX,
						target_width
						);
				}
				y_off += line_height;
			}
		}
		y_pos  += message_spacing;  //Extra spacing between messages.
	}
}

//virtual
void LLConsole::clear()
{
	mTimer.reset();
	LLMutexLock lock(&mQueueMutex);
	std::for_each(mParagraphs.begin(), mParagraphs.end(), DeletePointer());
	mParagraphs.clear();
	std::for_each(mNewParagraphs.begin(), mNewParagraphs.end(), DeletePointer());
	mNewParagraphs.clear();
}

//virtual
void LLConsole::addLine(const std::string& utf8line)
{
	addConsoleLine(utf8line, LLColor4(1.f, 1.f, 1.f, 1.f));
}

void LLConsole::addConsoleLine(const std::string& utf8line, const LLColor4 &color)
{
	LLWString wline = utf8str_to_wstring(utf8line);
	addConsoleLine(wline, color);
}

void LLConsole::addConsoleLine(const LLWString& wline, const LLColor4 &color)
{	
	Paragraph* paragraph = new Paragraph(wline, color, mTimer.getElapsedTimeF32());
	LLMutexLock lock(&mQueueMutex);
	mNewParagraphs.push_back ( paragraph );
}

//Generate highlight color segments for this paragraph.  Pass in default color of paragraph.
void LLConsole::Paragraph::makeParagraphColorSegments (const LLColor4 &color) 
{
	LLTextParser* highlight = LLTextParser::getInstance();
	LLSD paragraph_color_segments;
	LLColor4 lcolor=color;
	
	highlight->parseFullLineHighlights(wstring_to_utf8str(mParagraphText), &lcolor);
	paragraph_color_segments = highlight->parsePartialLineHighlights(
											wstring_to_utf8str(mParagraphText), 
											lcolor);
	
	for(LLSD::array_const_iterator color_segment_it = paragraph_color_segments.beginArray();
		color_segment_it != paragraph_color_segments.endArray();
		++color_segment_it)
	{			
		LLSD color_llsd = (*color_segment_it)["color"];
		std::string color_str  = (*color_segment_it)["text"].asString();

		ParagraphColorSegment color_segment;
		
		color_segment.mColor.setValue(color_llsd);
		color_segment.mNumChars = color_str.length();
		
		mParagraphColorSegments.push_back(color_segment);
	}
}

//Called when a paragraph is added to the console or window is resized.
void LLConsole::Paragraph::updateLines(F32 screen_width, LLFontGL* font, bool force_resize)
{
	if ( !force_resize )
	{
		if ( mMaxWidth >= 0.0f 
		 &&  mMaxWidth < screen_width )
		{
			return;					//No resize required.
		}
	}
	
	screen_width = screen_width - 30;	//Margin for small windows.
	
	if (	mParagraphText.empty() 
		|| mParagraphColorSegments.empty()
		|| font == NULL)
	{
		return;					//Not enough info to complete.
	}
	
	mLines.clear();				//Chuck everything.
	mMaxWidth = 0.0f;
	
	paragraph_color_segments_t::iterator current_color = mParagraphColorSegments.begin();
	U32 current_color_length = (*current_color).mNumChars;	
	
	S32 paragraph_offset = 0;			//Offset into the paragraph text.

	// Wrap lines that are longer than the view is wide.
	while( paragraph_offset < (S32)mParagraphText.length() )
	{
		bool found_newline = false; // skip '\n'
		// Figure out if a word-wrapped line fits here.
		LLWString::size_type line_end = mParagraphText.find_first_of(llwchar('\n'), paragraph_offset);
		if (line_end != LLWString::npos)
		{
			found_newline = true; // skip '\n'
		}
		else
		{
			line_end = mParagraphText.size();
		}

		U32 drawable = font->maxDrawableChars(mParagraphText.c_str()+paragraph_offset, screen_width, line_end - paragraph_offset, TRUE);

		if (drawable != 0 || found_newline)
		{
			F32 x_position = 0;						//Screen X position of text.
			
			mMaxWidth = llmax( mMaxWidth, (F32)font->getWidth( mParagraphText.substr( paragraph_offset, drawable ).c_str() ) );
			line_color_segments_t line;
			
			U32 left_to_draw = drawable;
			U32 drawn = 0;
			
			while (left_to_draw >= current_color_length 
				&& current_color != mParagraphColorSegments.end() )
			{
				LLWString color_text = mParagraphText.substr( paragraph_offset + drawn, current_color_length );
				line.push_back( LineColorSegment( color_text,			//Append segment to line.
												(*current_color).mColor, 
												x_position ) );
												
				x_position += font->getWidth( color_text.c_str() );	//Set up next screen position.
				
				drawn += current_color_length;
				left_to_draw -= current_color_length;
				
				current_color++;							//Goto next paragraph color record.
				
				if (current_color != mParagraphColorSegments.end())
				{
					current_color_length = (*current_color).mNumChars;
				}
			}
			
			if (left_to_draw > 0 && current_color != mParagraphColorSegments.end() )
			{
				LLWString color_text = mParagraphText.substr( paragraph_offset + drawn, left_to_draw );
				
				line.push_back( LineColorSegment( color_text,		//Append segment to line.
												(*current_color).mColor, 
												x_position ) );
																	
				current_color_length -= left_to_draw;
			}
			mLines.push_back(line);								//Append line to paragraph line list.
		}
		else
		{
			break; // Nothing more to print
		}

		paragraph_offset += (drawable + ( found_newline ? 1 : 0 ) );
	}
}

//Pass in the string and the default color for this block of text.
LLConsole::Paragraph::Paragraph (LLWString str, const LLColor4 &color, F32 add_time) 
						: mParagraphText(str), mAddTime(add_time), mMaxWidth(-1)
{
	makeParagraphColorSegments(color);
}
	
