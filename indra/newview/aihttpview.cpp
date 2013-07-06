/**
 * @file aihttpview.cpp
 * @brief Definition of class AIHTTPView.
 *
 * Copyright (c) 2013, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   28/05/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "llviewerprecompiledheaders.h"
#include "aihttpview.h"
#include "llrect.h"
#include "llerror.h"
#include "aicurlperservice.h"
#include "llviewerstats.h"
#include "llfontgl.h"
#include "aihttptimeout.h"

AIHTTPView* gHttpView = NULL;
static S32 sLineHeight;

// Forward declaration.
namespace AICurlInterface {
  size_t getHTTPBandwidth(void);
  U32 getNumHTTPAdded(void);
  U32 getMaxHTTPAdded(void);
} // namespace AICurlInterface

//=============================================================================

	//PerService_crat per_service_r(*service.second);
class AIServiceBar : public LLView
{
  private:
	AIHTTPView* mHTTPView;
	std::string mName;
	AIPerServicePtr mPerService;

  public:
	AIServiceBar(AIHTTPView* httpview, AIPerService::instance_map_type::value_type const& service)
		: LLView("aiservice bar", LLRect(), FALSE), mHTTPView(httpview), mName(service.first), mPerService(service.second) { }

	/*virtual*/ void draw(void);
	/*virtual*/ LLRect getRequiredRect(void);
};

int const mc_col = number_of_capability_types;				// Maximum connections column.
int const bw_col = number_of_capability_types + 1;			// Bandwidth column.

void AIServiceBar::draw()
{
  LLColor4 text_color = LLColor4::white;
  F32 height = getRect().getHeight();
  U32 start = 4;
  LLFontGL::getFontMonospace()->renderUTF8(mName, 0, start, height, text_color, LLFontGL::LEFT, LLFontGL::TOP);
  start += LLFontGL::getFontMonospace()->getWidth(mName);
  std::string text;
  AIPerService::CapabilityType* cts;
  U32 is_used;
  U32 is_inuse;
  int total_added;
  int established_connections;
  int concurrent_connections;
  size_t bandwidth;
  {
	PerService_rat per_service_r(*mPerService);
	is_used = per_service_r->is_used();
	is_inuse = per_service_r->is_inuse();
	total_added = per_service_r->mTotalAdded;
	established_connections = per_service_r->mEstablishedConnections;
	concurrent_connections = per_service_r->mConcurrentConnections;
	bandwidth = per_service_r->bandwidth().truncateData(AIHTTPView::getTime_40ms());
	cts = per_service_r->mCapabilityType;	// Not thread-safe, but we're only reading from it and only using the results to show in a debug console.
  }
  for (int col = 0; col < number_of_capability_types; ++col)
  {
	AICapabilityType capability_type = static_cast<AICapabilityType>(col);
	AIPerService::CapabilityType& ct(cts[capability_type]);
	start = mHTTPView->updateColumn(col, start);
	U32 mask = AIPerService::CT2mask(capability_type);
	if (!(is_used & mask))
	{
	  text = " |              ";
	}
	else
	{
	  if (col < 2)
	  {
		text = llformat(" | %hu-%hu-%lu,{%hu/%hu,%u}/%u",
			ct.mApprovedRequests, ct.mQueuedCommands, ct.mQueuedRequests.size(),
			ct.mAdded, ct.mConcurrentConnections, ct.mDownloading,
			ct.mMaxPipelinedRequests);
	  }
	  else
	  {
		text = llformat(" | --%hu-%lu,{%hu/%hu,%u}",
			ct.mQueuedCommands, ct.mQueuedRequests.size(),
			ct.mAdded, ct.mConcurrentConnections, ct.mDownloading);
	  }
	  if (capability_type == cap_texture || capability_type == cap_mesh)
	  {
		if (!(is_inuse & mask))
		{
		  ct.mFlags |= AIPerService::ctf_grey;
		}
		else
		{
		  bool show = true;
		  int progress_counter = (ct.mFlags & AIPerService::ctf_progress_mask) >> AIPerService::ctf_progress_shift;
		  if ((ct.mFlags & AIPerService::ctf_success))
		  {
			show = !(ct.mFlags & AIPerService::ctf_grey);
			ct.mFlags &= ~(AIPerService::ctf_success|AIPerService::ctf_grey|AIPerService::ctf_progress_mask);
			progress_counter = (progress_counter + 1) % 8;
			ct.mFlags |= progress_counter << AIPerService::ctf_progress_shift;
		  }
		  if (show)
		  {
			static char const* progress_utf8[8] = { " \xe2\xac\x93", " \xe2\xac\x95", " \xe2\x97\xa7", " \xe2\x97\xa9", " \xe2\xac\x92", " \xe2\xac\x94", " \xe2\x97\xa8", " \xe2\x97\xaa" };
			text += progress_utf8[progress_counter];
		  }
		}
	  }
	}
	LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, ((is_inuse & mask) == 0) ? LLColor4::grey2 : text_color, LLFontGL::LEFT, LLFontGL::TOP);
	start += LLFontGL::getFontMonospace()->getWidth(text);
  }
  start = mHTTPView->updateColumn(mc_col, start);
#if defined(CWDEBUG) || defined(DEBUG_CURLIO)
  text = llformat(" | %d,%d/%d", total_added, established_connections, concurrent_connections);
#else
  text = llformat(" | %d/%d", total_added, concurrent_connections);
#endif
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, text_color, LLFontGL::LEFT, LLFontGL::TOP);
  start += LLFontGL::getFontMonospace()->getWidth(text);
  start = mHTTPView->updateColumn(bw_col, start);
  size_t max_bandwidth = mHTTPView->mMaxBandwidthPerService;
  text = " | ";
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, text_color, LLFontGL::LEFT, LLFontGL::TOP);
  start += LLFontGL::getFontMonospace()->getWidth(text);
  text = llformat("%lu", bandwidth / 125);
  if (bandwidth == 0)
  {
	text_color = LLColor4::grey2;
  }
  LLColor4 color = (bandwidth > max_bandwidth) ? LLColor4::red : ((bandwidth > max_bandwidth * .75f) ? LLColor4::yellow : text_color);
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, color, LLFontGL::LEFT, LLFontGL::TOP);
  start += LLFontGL::getFontMonospace()->getWidth(text);
  text = llformat("/%lu", max_bandwidth / 125);
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, text_color, LLFontGL::LEFT, LLFontGL::TOP);
}

LLRect AIServiceBar::getRequiredRect(void)
{
  LLRect rect;
  rect.mTop = sLineHeight;
  return rect;
}

//=============================================================================

static int const number_of_header_lines = 2;

class AIGLHTTPHeaderBar : public LLView
{
  public:
	AIGLHTTPHeaderBar(std::string const& name, AIHTTPView* httpview) :
		LLView(name, FALSE), mHTTPView(httpview)
	{
	  sLineHeight = llround(LLFontGL::getFontMonospace()->getLineHeight());
	  setRect(LLRect(0, 0, 200, sLineHeight * number_of_header_lines));
	}

	/*virtual*/ void draw(void);	
	/*virtual*/ BOOL handleMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ LLRect getRequiredRect(void);

  private:
	AIHTTPView* mHTTPView;
};

void AIGLHTTPHeaderBar::draw(void)
{
  S32 const v_offset = -1;		// Offset from the bottom. Move header one pixel down.
  S32 const h_offset = 4;

  LLGLSUIDefault gls_ui;

  LLColor4 text_color(1.f, 1.f, 1.f, 0.75f);
  std::string text;

  // First header line.
  F32 height = v_offset + sLineHeight * number_of_header_lines;
  text = "HTTP console -- [approved]-commandQ-curlQ,{added/max,downloading}[/max][ completed]";
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, h_offset, height, text_color, LLFontGL::LEFT, LLFontGL::TOP);
  text = " | Added/Max";
  U32 start = mHTTPView->updateColumn(mc_col, 100);
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, LLColor4::green, LLFontGL::LEFT, LLFontGL::TOP);
  start += LLFontGL::getFontMonospace()->getWidth(text);
  text = " | Tot/Max BW (kbit/s)";
  start = mHTTPView->updateColumn(bw_col, start);
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, LLColor4::green, LLFontGL::LEFT, LLFontGL::TOP);
  mHTTPView->setWidth(start + LLFontGL::getFontMonospace()->getWidth(text) + h_offset);

  // Second header line.
  height -= sLineHeight;
  start = h_offset;
  text = "Service (host:port)";
  static char const* caption[number_of_capability_types] = {
	" | Textures", " | Inventory", " | Mesh", " | Other"
  };
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, LLColor4::green, LLFontGL::LEFT, LLFontGL::TOP);
  start += LLFontGL::getFontMonospace()->getWidth(text);
  for (int col = 0; col < number_of_capability_types; ++col)
  {
	start = mHTTPView->updateColumn(col, start);
	text = caption[col];
	LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, LLColor4::green, LLFontGL::LEFT, LLFontGL::TOP);
	start += LLFontGL::getFontMonospace()->getWidth(text);
  }
  start = mHTTPView->updateColumn(mc_col, start);
  text = llformat(" | %u/%u", AICurlInterface::getNumHTTPAdded(), AICurlInterface::getMaxHTTPAdded());
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, text_color, LLFontGL::LEFT, LLFontGL::TOP);
  start += LLFontGL::getFontMonospace()->getWidth(text);

  // This bandwidth is averaged over 1 seconds (in bytes/s).
  size_t const bandwidth = AICurlInterface::getHTTPBandwidth();
  size_t const max_bandwidth = AIPerService::getHTTPThrottleBandwidth125();
  mHTTPView->mMaxBandwidthPerService = max_bandwidth * AIPerService::throttleFraction();
  LLColor4 color = (bandwidth > max_bandwidth) ? LLColor4::red : ((bandwidth > max_bandwidth * .75f) ? LLColor4::yellow : text_color);
  color[VALPHA] = text_color[VALPHA];
  start = mHTTPView->updateColumn(bw_col, start);
  text = " | ";
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, LLColor4::green, LLFontGL::LEFT, LLFontGL::TOP);
  start += LLFontGL::getFontMonospace()->getWidth(text);
  text = llformat("%lu", bandwidth / 125);
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, color, LLFontGL::LEFT, LLFontGL::TOP);
  start += LLFontGL::getFontMonospace()->getWidth(text);
  text = llformat("/%lu", max_bandwidth / 125);
  LLFontGL::getFontMonospace()->renderUTF8(text, 0, start, height, text_color, LLFontGL::LEFT, LLFontGL::TOP);
}

BOOL AIGLHTTPHeaderBar::handleMouseDown(S32 x, S32 y, MASK mask)
{
  return FALSE;
}

LLRect AIGLHTTPHeaderBar::getRequiredRect()
{
  LLRect rect;
  rect.mTop = sLineHeight * number_of_header_lines;
  return rect;
}

//=============================================================================

AIHTTPView::AIHTTPView(AIHTTPView::Params const& p) :
	LLContainerView(p), mGLHTTPHeaderBar(NULL), mWidth(200)
{
  setVisible(FALSE);
  setRectAlpha(0.5);
}

AIHTTPView::~AIHTTPView()
{
  delete mGLHTTPHeaderBar;
  mGLHTTPHeaderBar = NULL;
}

U32 AIHTTPView::updateColumn(int col, U32 start)
{
  llassert(col <= mStartColumn.size());
  if (col == mStartColumn.size())
  {
	mStartColumn.push_back(start);
  }
  else if (mStartColumn[col] < start)
  {
	mStartColumn[col] = start;
  }
  return mStartColumn[col];
}

//static
void AIHTTPView::toggle_visibility(void* user_data)
{
  LLView* viewp = (LLView*)user_data;
  bool visible = !viewp->getVisible();
  if (visible)
  {
	AIPerService::resetUsed();
  }
  viewp->setVisible(visible);
}

U64 AIHTTPView::sTime_40ms;

struct KillView
{
  void operator()(LLView* viewp)
  {
	viewp->getParent()->removeChild(viewp);
	viewp->die();
  }
};

struct CreateServiceBar
{
  AIHTTPView* mHTTPView;

  CreateServiceBar(AIHTTPView* httpview) : mHTTPView(httpview) { }

  void operator()(AIPerService::instance_map_type::value_type const& service)
  {
	if (!PerService_rat(*service.second)->is_used())
	  return;
	AIServiceBar* service_bar = new AIServiceBar(mHTTPView, service);
	mHTTPView->addChild(service_bar);
	mHTTPView->mServiceBars.push_back(service_bar);
  }
};

void AIHTTPView::draw()
{
  for_each(mServiceBars.begin(), mServiceBars.end(), KillView());
  mServiceBars.clear();
	  
  if (mGLHTTPHeaderBar)
  {
	removeChild(mGLHTTPHeaderBar);
	mGLHTTPHeaderBar->die();
  }

  CreateServiceBar functor(this);
  AIPerService::copy_forEach(functor);

  sTime_40ms = get_clock_count() * AICurlPrivate::curlthread::HTTPTimeout::sClockWidth_40ms;

  mGLHTTPHeaderBar = new AIGLHTTPHeaderBar("gl httpheader bar", this);
  addChild(mGLHTTPHeaderBar);

  reshape(mWidth, getRect().getHeight(), TRUE);

  for (child_list_const_iter_t child_iter = getChildList()->begin(); child_iter != getChildList()->end(); ++child_iter)
  {
	LLView* viewp = *child_iter;
	if (viewp->getRect().mBottom < 0)
	{
	  viewp->setVisible(FALSE);
	}
  }

  LLContainerView::draw();
}

BOOL AIHTTPView::handleMouseUp(S32 x, S32 y, MASK mask)
{
  return FALSE;
}

BOOL AIHTTPView::handleKey(KEY key, MASK mask, BOOL called_from_parent)
{
  return FALSE;
}

