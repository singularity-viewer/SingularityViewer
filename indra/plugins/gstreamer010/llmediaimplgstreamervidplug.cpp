/**
 * @file llmediaimplgstreamervidplug.cpp
 * @brief Video-consuming static GStreamer plugin for gst-to-LLMediaImpl
 *
 * @cond
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2010, Linden Research, Inc.
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

#if LL_GSTREAMER010_ENABLED

#include "linden_common.h"

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>

#include "llmediaimplgstreamertriviallogging.h"
//  #include "llthread.h"

#include "llmediaimplgstreamervidplug.h"

GST_DEBUG_CATEGORY_STATIC (gst_slvideo_debug);
#define GST_CAT_DEFAULT gst_slvideo_debug

/* Filter signals and args *//*
enum
{
	*//* FILL ME *//*
	LAST_SIGNAL
};

enum
{
	ARG_0
};

#define SLV_SIZECAPS ", width=(int){1,2,4,8,16,32,64,128,256,512,1024}, height=(int){1,2,4,8,16,32,64,128,256,512,1024} "
#define SLV_ALLCAPS GST_VIDEO_CAPS_RGBx SLV_SIZECAPS ";" GST_VIDEO_CAPS_BGRx SLV_SIZECAPS
*/

#define SLV_SIZECAPS ", width=(int)[1,2048], height=(int)[1,2048] "
#define SLV_ALLCAPS GST_VIDEO_CAPS_RGBx SLV_SIZECAPS

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE (
    (gchar*)"sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SLV_ALLCAPS)
    );

GST_BOILERPLATE (GstSLVideo, gst_slvideo, GstVideoSink,
    GST_TYPE_VIDEO_SINK);

static void gst_slvideo_set_property (GObject * object, guint prop_id,
				      const GValue * value,
				      GParamSpec * pspec);
static void gst_slvideo_get_property (GObject * object, guint prop_id,
				      GValue * value, GParamSpec * pspec);

static void
gst_slvideo_base_init (gpointer gclass)
{
	static GstElementDetails element_details = {
		(gchar*)"PluginTemplate",
		(gchar*)"Generic/PluginTemplate",
		(gchar*)"Generic Template Element",
		(gchar*)"Linden Lab"
	};
	GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);
	
	gst_element_class_add_pad_template (element_class,
			      gst_static_pad_template_get (&sink_factory));
	gst_element_class_set_details (element_class, &element_details);
}


static void
gst_slvideo_finalize (GObject * object)
{
	GstSLVideo *slvideo;
	slvideo = GST_SLVIDEO (object);
	if (slvideo->caps)
	{
		gst_caps_unref(slvideo->caps);
	}

	G_OBJECT_CLASS(parent_class)->finalize (object);
}


static GstFlowReturn
gst_slvideo_show_frame (GstBaseSink * bsink, GstBuffer * buf)
{
	GstSLVideo *slvideo;
	g_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);
	
	slvideo = GST_SLVIDEO(bsink);
	
	DEBUGMSG("transferring a frame of %dx%d <- %p (%d)",
		 slvideo->width, slvideo->height, GST_BUFFER_DATA(buf),
		 slvideo->format);

	if (GST_BUFFER_DATA(buf))
	{
		// copy frame and frame info into neutral territory
		GST_OBJECT_LOCK(slvideo);
		slvideo->retained_frame_ready = TRUE;
		slvideo->retained_frame_width = slvideo->width;
		slvideo->retained_frame_height = slvideo->height;
		slvideo->retained_frame_format = slvideo->format;
		int rowbytes = 
			SLVPixelFormatBytes[slvideo->retained_frame_format] *
			slvideo->retained_frame_width;
		int needbytes = rowbytes * slvideo->retained_frame_width;
		// resize retained frame hunk only if necessary
		if (needbytes != slvideo->retained_frame_allocbytes)
		{
			delete[] slvideo->retained_frame_data;
			slvideo->retained_frame_data = new unsigned char[needbytes];
			slvideo->retained_frame_allocbytes = needbytes;
			
		}
		// copy the actual frame data to neutral territory -
		// flipped, for GL reasons
		for (int ypos=0; ypos<slvideo->height; ++ypos)
		{
			memcpy(&slvideo->retained_frame_data[(slvideo->height-1-ypos)*rowbytes],
			       &(((unsigned char*)GST_BUFFER_DATA(buf))[ypos*rowbytes]),
			       rowbytes);
		}
		// done with the shared data
		GST_OBJECT_UNLOCK(slvideo);
	}

	return GST_FLOW_OK;
}


static GstStateChangeReturn
gst_slvideo_change_state(GstElement * element, GstStateChange transition)
{
	GstSLVideo *slvideo;
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	
	slvideo = GST_SLVIDEO (element);

	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
	if (ret == GST_STATE_CHANGE_FAILURE)
		return ret;

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		slvideo->fps_n = 0;
		slvideo->fps_d = 1;
		GST_VIDEO_SINK_WIDTH(slvideo) = 0;
		GST_VIDEO_SINK_HEIGHT(slvideo) = 0;
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		break;
	default:
		break;
	}

	return ret;
}


static GstCaps *
gst_slvideo_get_caps (GstBaseSink * bsink)
{
	GstSLVideo *slvideo;
	slvideo = GST_SLVIDEO(bsink);
	
	return gst_caps_ref (slvideo->caps);
}


/* this function handles the link with other elements */
static gboolean
gst_slvideo_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
	GstSLVideo *filter;
	GstStructure *structure;
//	GstCaps *intersection;
	
	GST_DEBUG ("set caps with %" GST_PTR_FORMAT, caps);
	
	filter = GST_SLVIDEO(bsink);

/*	
	intersection = gst_caps_intersect (filter->caps, caps);
	if (gst_caps_is_empty (intersection))
	{
		// no overlap between our caps and requested caps
		return FALSE;
	}
	gst_caps_unref(intersection);
*/	
	int width = 0;
	int height = 0;
	gboolean ret;
	const GValue *fps;
	const GValue *par;
	structure = gst_caps_get_structure (caps, 0);
	ret = gst_structure_get_int (structure, "width", &width);
	ret = ret && gst_structure_get_int (structure, "height", &height);
	fps = gst_structure_get_value (structure, "framerate");
	ret = ret && (fps != NULL);
	par = gst_structure_get_value (structure, "pixel-aspect-ratio");
	if (!ret)
		return FALSE;

	INFOMSG("** filter caps set with width=%d, height=%d", width, height);

	GST_OBJECT_LOCK(filter);

	filter->width = width;
	filter->height = height;
	filter->fps_n = gst_value_get_fraction_numerator(fps);
	filter->fps_d = gst_value_get_fraction_denominator(fps);
	if (par)
	{
		filter->par_n = gst_value_get_fraction_numerator(par);
		filter->par_d = gst_value_get_fraction_denominator(par);
	}
	else
	{
		filter->par_n = 1;
		filter->par_d = 1;
	}

	GST_VIDEO_SINK_WIDTH(filter) = width;
	GST_VIDEO_SINK_HEIGHT(filter) = height;

	// crufty lump - we *always* accept *only* RGBX now.
	/*
	
	filter->format = SLV_PF_UNKNOWN;
	if (0 == strcmp(gst_structure_get_name(structure),
			"video/x-raw-rgb"))
	{
		int red_mask;
		int green_mask;
		int blue_mask;
		gst_structure_get_int(structure, "red_mask", &red_mask);
		gst_structure_get_int(structure, "green_mask", &green_mask);
		gst_structure_get_int(structure, "blue_mask", &blue_mask);
		if ((unsigned int)red_mask   == 0xFF000000 &&
		    (unsigned int)green_mask == 0x00FF0000 &&
		    (unsigned int)blue_mask  == 0x0000FF00)
		{
			filter->format = SLV_PF_RGBX;
			//fprintf(stderr, "\n\nPIXEL FORMAT RGB\n\n");
		} else if ((unsigned int)red_mask   == 0x0000FF00 &&
			   (unsigned int)green_mask == 0x00FF0000 &&
			   (unsigned int)blue_mask  == 0xFF000000)
		{
			filter->format = SLV_PF_BGRX;
			//fprintf(stderr, "\n\nPIXEL FORMAT BGR\n\n");
		}
		
	}*/

	filter->format = SLV_PF_RGBX;

	GST_OBJECT_UNLOCK(filter);
	
	return TRUE;
}


static gboolean
gst_slvideo_start (GstBaseSink * bsink)
{
	GstSLVideo *slvideo;
	gboolean ret = TRUE;
	
	slvideo = GST_SLVIDEO(bsink);

	return ret;
}

static gboolean
gst_slvideo_stop (GstBaseSink * bsink)
{
	GstSLVideo *slvideo;
	slvideo = GST_SLVIDEO(bsink);

	// free-up retained frame buffer
	GST_OBJECT_LOCK(slvideo);
	slvideo->retained_frame_ready = FALSE;
	delete[] slvideo->retained_frame_data;
	slvideo->retained_frame_data = NULL;
	slvideo->retained_frame_allocbytes = 0;
	GST_OBJECT_UNLOCK(slvideo);

	return TRUE;
}


static GstFlowReturn
gst_slvideo_buffer_alloc (GstBaseSink * bsink, guint64 offset, guint size,
			  GstCaps * caps, GstBuffer ** buf)
{
	gint width, height;
	GstStructure *structure = NULL;
	GstSLVideo *slvideo;
	slvideo = GST_SLVIDEO(bsink);

	// caps == requested caps
	// we can ignore these and reverse-negotiate our preferred dimensions with
	// the peer if we like - we need to do this to obey dynamic resize requests
	// flowing in from the app.
	structure = gst_caps_get_structure (caps, 0);
	if (!gst_structure_get_int(structure, "width", &width) ||
	    !gst_structure_get_int(structure, "height", &height))
	{
		GST_WARNING_OBJECT (slvideo, "no width/height in caps %" GST_PTR_FORMAT, caps);
		return GST_FLOW_NOT_NEGOTIATED;
	}

	GstBuffer *newbuf = gst_buffer_new();
	bool made_bufferdata_ptr = false;
#define MAXDEPTHHACK 4
	
	GST_OBJECT_LOCK(slvideo);
	if (slvideo->resize_forced_always) // app is giving us a fixed size to work with
	{
		gint slwantwidth, slwantheight;
		slwantwidth = slvideo->resize_try_width;
		slwantheight = slvideo->resize_try_height;
	
		if (slwantwidth != width ||
		    slwantheight != height)
		{
			// don't like requested caps, we will issue our own suggestion - copy
			// the requested caps but substitute our own width and height and see
			// if our peer is happy with that.
		
			GstCaps *desired_caps;
			GstStructure *desired_struct;
			desired_caps = gst_caps_copy (caps);
			desired_struct = gst_caps_get_structure (desired_caps, 0);
			
			GValue value = {0};
			g_value_init(&value, G_TYPE_INT);
			g_value_set_int(&value, slwantwidth);
			gst_structure_set_value (desired_struct, "width", &value);
			g_value_unset(&value);
			g_value_init(&value, G_TYPE_INT);
			g_value_set_int(&value, slwantheight);
			gst_structure_set_value (desired_struct, "height", &value);
			
			if (gst_pad_peer_accept_caps (GST_VIDEO_SINK_PAD (slvideo),
							desired_caps))
			{
				// todo: re-use buffers from a pool?
				// todo: set MALLOCDATA to null, set DATA to point straight to shm?
				
				// peer likes our cap suggestion
				DEBUGMSG("peer loves us :)");
				GST_BUFFER_SIZE(newbuf) = slwantwidth * slwantheight * MAXDEPTHHACK;
				GST_BUFFER_MALLOCDATA(newbuf) = (guint8*)g_malloc(GST_BUFFER_SIZE(newbuf));
				GST_BUFFER_DATA(newbuf) = GST_BUFFER_MALLOCDATA(newbuf);
				gst_buffer_set_caps (GST_BUFFER_CAST(newbuf), desired_caps);

				made_bufferdata_ptr = true;
			} else {
				// peer hates our cap suggestion
				INFOMSG("peer hates us :(");
				gst_caps_unref(desired_caps);
			}
		}
	}

	GST_OBJECT_UNLOCK(slvideo);

	if (!made_bufferdata_ptr) // need to fallback to malloc at original size
	{
		GST_BUFFER_SIZE(newbuf) = width * height * MAXDEPTHHACK;
		GST_BUFFER_MALLOCDATA(newbuf) = (guint8*)g_malloc(GST_BUFFER_SIZE(newbuf));
		GST_BUFFER_DATA(newbuf) = GST_BUFFER_MALLOCDATA(newbuf);
		gst_buffer_set_caps (GST_BUFFER_CAST(newbuf), caps);
	}

	*buf = GST_BUFFER_CAST(newbuf);

	return GST_FLOW_OK;
}


/* initialize the plugin's class */
static void
gst_slvideo_class_init (GstSLVideoClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseSinkClass *gstbasesink_class;
	
	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	gstbasesink_class = (GstBaseSinkClass *) klass;
	
	gobject_class->finalize = gst_slvideo_finalize;
	gobject_class->set_property = gst_slvideo_set_property;
	gobject_class->get_property = gst_slvideo_get_property;
	
	gstelement_class->change_state = gst_slvideo_change_state;
	
#define LLGST_DEBUG_FUNCPTR(p) (p)
	gstbasesink_class->get_caps = LLGST_DEBUG_FUNCPTR (gst_slvideo_get_caps);
	gstbasesink_class->set_caps = LLGST_DEBUG_FUNCPTR( gst_slvideo_set_caps);
	gstbasesink_class->buffer_alloc=LLGST_DEBUG_FUNCPTR(gst_slvideo_buffer_alloc);
	//gstbasesink_class->get_times = LLGST_DEBUG_FUNCPTR (gst_slvideo_get_times);
	gstbasesink_class->preroll = LLGST_DEBUG_FUNCPTR (gst_slvideo_show_frame);
	gstbasesink_class->render = LLGST_DEBUG_FUNCPTR (gst_slvideo_show_frame);
	
	gstbasesink_class->start = LLGST_DEBUG_FUNCPTR (gst_slvideo_start);
	gstbasesink_class->stop = LLGST_DEBUG_FUNCPTR (gst_slvideo_stop);
	
	//	gstbasesink_class->unlock = LLGST_DEBUG_FUNCPTR (gst_slvideo_unlock);
#undef LLGST_DEBUG_FUNCPTR
}

/*
static void
gst_slvideo_update_caps (GstSLVideo * slvideo)
{
	GstCaps *caps;

	// GStreamer will automatically convert colourspace if necessary.
	// GStreamer will automatically resize media to one of these enumerated
	// powers-of-two that we ask for (yay GStreamer!)
	caps = gst_caps_from_string (SLV_ALLCAPS);
	
	gst_caps_replace (&slvideo->caps, caps);
}
*/

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_slvideo_init (GstSLVideo * filter,
		  GstSLVideoClass * gclass)
{
	filter->caps = NULL;
	filter->width = -1;
	filter->height = -1;

	// this is the info we share with the client app
	GST_OBJECT_LOCK(filter);
	filter->retained_frame_ready = FALSE;
	filter->retained_frame_data = NULL;
	filter->retained_frame_allocbytes = 0;
	filter->retained_frame_width = filter->width;
	filter->retained_frame_height = filter->height;
	filter->retained_frame_format = SLV_PF_UNKNOWN;
	GstCaps *caps = gst_caps_from_string (SLV_ALLCAPS);
	gst_caps_replace (&filter->caps, caps);
	filter->resize_forced_always = false;
	filter->resize_try_width = -1;
	filter->resize_try_height = -1;
	GST_OBJECT_UNLOCK(filter);
	
	//gst_slvideo_update_caps(filter);
}

static void
gst_slvideo_set_property (GObject * object, guint prop_id,
			  const GValue * value, GParamSpec * pspec)
{
	g_return_if_fail (GST_IS_SLVIDEO (object));
	
	if (prop_id) {
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gst_slvideo_get_property (GObject * object, guint prop_id,
			  GValue * value, GParamSpec * pspec)
{
	g_return_if_fail (GST_IS_SLVIDEO (object));

	if (prop_id) {
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
	DEBUGMSG("PLUGIN INIT");

	GST_DEBUG_CATEGORY_INIT (gst_slvideo_debug, (gchar*)"private-slvideo-plugin",
				 0, (gchar*)"Second Life Video Sink");

	return gst_element_register (plugin, "private-slvideo",
				       GST_RANK_NONE, GST_TYPE_SLVIDEO);
}

/* this is the structure that gstreamer looks for to register plugins
 */
/* NOTE: Can't rely upon GST_PLUGIN_DEFINE_STATIC to self-register, since
   some g++ versions buggily avoid __attribute__((constructor)) functions -
   so we provide an explicit plugin init function.
 */

void gst_slvideo_init_class (void)
{
	gst_plugin_register_static( GST_VERSION_MAJOR,
	                            GST_VERSION_MINOR,
	                            (const gchar *)"private-slvideoplugin", 
	                            (gchar *)"SL Video sink plugin",
	                            plugin_init,
	                            (const gchar *)"0.1",
	                            GST_LICENSE_UNKNOWN,
	                            (const gchar *)"Second Life",
	                            (const gchar *)"Second Life",
	                            (const gchar *)"http://www.secondlife.com/" );
}

#endif // LL_GSTREAMER010_ENABLED
