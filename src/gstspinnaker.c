/* GStreamer
 * Copyright (C) 2019 FIXME <fixme@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstspinnaker
 *
 * The spinnaker element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! spinnaker ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstspinnaker.h"

GST_DEBUG_CATEGORY_STATIC (gst_spinnaker_debug_category);
#define GST_CAT_DEFAULT gst_spinnaker_debug_category
#define OVERRIDE_CREATE
/* prototypes */


static void gst_spinnaker_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_spinnaker_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_spinnaker_dispose (GObject * object);
static void gst_spinnaker_finalize (GObject * object);

static GstCaps *gst_spinnaker_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_spinnaker_negotiate (GstBaseSrc * src);
static GstCaps *gst_spinnaker_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_spinnaker_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_spinnaker_decide_allocation (GstBaseSrc * src,
    GstQuery * query);
static gboolean gst_spinnaker_start (GstBaseSrc * src);
static gboolean gst_spinnaker_stop (GstBaseSrc * src);
//static void gst_spinnaker_get_times (GstBaseSrc * src, GstBuffer * buffer,
  //  GstClockTime * start, GstClockTime * end);
static gboolean gst_spinnaker_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_spinnaker_is_seekable (GstBaseSrc * src);
//static gboolean gst_spinnaker_prepare_seek_segment (GstBaseSrc * src,
  //  GstEvent * seek, GstSegment * segment);
//static gboolean gst_spinnaker_do_seek (GstBaseSrc * src, GstSegment * segment);
static gboolean gst_spinnaker_unlock (GstBaseSrc * src);
static gboolean gst_spinnaker_unlock_stop (GstBaseSrc * src);
//static gboolean gst_spinnaker_query (GstBaseSrc * src, GstQuery * query);
static gboolean gst_spinnaker_event (GstBaseSrc * src, GstEvent * event);
static GstFlowReturn gst_spinnaker_create (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** buf);
//static GstFlowReturn gst_spinnaker_alloc (GstBaseSrc * src, guint64 offset,
//    guint size, GstBuffer ** buf);
//static GstFlowReturn gst_spinnaker_fill (GstBaseSrc * src, guint64 offset,
//    guint size, GstBuffer * buf);

enum
{
  PROP_0
};

#define EXEANDCHECK(function) \
{\
	spinError Ret = function;\
	if (SPINNAKER_ERR_SUCCESS != Ret){\
		GST_ERROR_OBJECT(src, "Spinnaker call failed: %d", Ret);\
		goto fail;\
	}\
}

/* pad templates */

static GstStaticPadTemplate gst_spinnaker_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
				GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
						("{ RGBA }"))
    );
/*
static GstStaticPadTemplate gst_spinnaker_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
				GST_STATIC_CAPS ("video/x-raw, format=(string){ RGB }, width=(int){ 4000 } , height=(int){ 3000 }, framerate=(fraction){ 31/1 }")
    );
*/
/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstSpinnaker, gst_spinnaker, GST_TYPE_BASE_SRC,
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "spinnaker", 0,
        "debug category for spinnaker element"));

static void
gst_spinnaker_class_init (GstSpinnakerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_spinnaker_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "FIXME Long name", "Generic", "FIXME Description",
      "FIXME <fixme@example.com>");


  gobject_class->set_property = gst_spinnaker_set_property;
  gobject_class->get_property = gst_spinnaker_get_property;
  gobject_class->dispose = gst_spinnaker_dispose;
  gobject_class->finalize = gst_spinnaker_finalize;
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_spinnaker_get_caps);
  base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_spinnaker_negotiate);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_spinnaker_fixate);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_spinnaker_set_caps);
  base_src_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_spinnaker_decide_allocation);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_spinnaker_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_spinnaker_stop);
  //base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_spinnaker_get_times);
  base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_spinnaker_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_spinnaker_is_seekable);
  //base_src_class->prepare_seek_segment =
   //   GST_DEBUG_FUNCPTR (gst_spinnaker_prepare_seek_segment);
 // base_src_class->do_seek = GST_DEBUG_FUNCPTR (gst_spinnaker_do_seek);
  base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_spinnaker_unlock);
  base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_spinnaker_unlock_stop);
  //base_src_class->query = GST_DEBUG_FUNCPTR (gst_spinnaker_query);
  base_src_class->event = GST_DEBUG_FUNCPTR (gst_spinnaker_event);
  base_src_class->create = GST_DEBUG_FUNCPTR (gst_spinnaker_create);
 // base_src_class->alloc = GST_DEBUG_FUNCPTR (gst_spinnaker_alloc);
 // base_src_class->fill = GST_DEBUG_FUNCPTR (gst_spinnaker_fill);

}

static void
gst_spinnaker_init (GstSpinnaker * spinnaker)
{
    //Hardcoded hot-garbage **************************
  spinnaker->nWidth = 4000;
	spinnaker->nHeight = 3000;
	spinnaker->nRawWidth = 4000;
	spinnaker->nRawHeight = 3000;
  spinnaker->nBytesPerPixel = 4;
  spinnaker->binning = 1;
  spinnaker->n_frames = 0;
  spinnaker->framerate = 31;
  spinnaker->last_frame_time = 0;
  spinnaker->nPitch = spinnaker->nWidth * spinnaker->nBytesPerPixel;
	spinnaker->nRawPitch = spinnaker->nRawWidth * spinnaker->nBytesPerPixel;
}

void
gst_spinnaker_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (object);

  GST_DEBUG_OBJECT (spinnaker, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_spinnaker_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (object);

  GST_DEBUG_OBJECT (spinnaker, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_spinnaker_dispose (GObject * object)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (object);

  GST_DEBUG_OBJECT (spinnaker, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_spinnaker_parent_class)->dispose (object);
}

void
gst_spinnaker_finalize (GObject * object)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (object);

  GST_DEBUG_OBJECT (spinnaker, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_spinnaker_parent_class)->finalize (object);
}

/* get caps from subclass */
static GstCaps *
gst_spinnaker_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);
  GstCaps *caps;
  GST_DEBUG_OBJECT (spinnaker, "get_caps");
  caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  GST_DEBUG_OBJECT (src, "The caps are %" GST_PTR_FORMAT, caps);
  return caps;
}

/* decide on caps */
static gboolean
gst_spinnaker_negotiate (GstBaseSrc * src)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "negotiate");

  return TRUE;
}

/* called if, in negotiation, caps need fixating */
static GstCaps *
gst_spinnaker_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "fixate");

  return NULL;
}

/* notify the subclass of new caps */
static gboolean
gst_spinnaker_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);
  GstVideoInfo vinfo;

  GST_DEBUG_OBJECT (spinnaker, "set_caps");

  GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);

	gst_video_info_from_caps (&vinfo, caps);

  if (GST_VIDEO_INFO_FORMAT (&vinfo) != GST_VIDEO_FORMAT_UNKNOWN) {
      g_assert (spinnaker->hCamera != NULL);
      //  src->vrm_stride = get_pitch (src->device);  // wait for image to arrive for this
      spinnaker->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);
      spinnaker->nHeight = vinfo.height;
    } else {
      goto unsupported_caps;
    }

  EXEANDCHECK(spinCameraBeginAcquisition(spinnaker->hCamera));
  GST_DEBUG_OBJECT (spinnaker, "camera opened");

  return TRUE;

	unsupported_caps:
	GST_ERROR_OBJECT (src, "Unsupported caps: %" GST_PTR_FORMAT, caps);
	return FALSE;

	fail:
	return FALSE;

}

/* setup allocation query */
static gboolean
gst_spinnaker_decide_allocation (GstBaseSrc * src, GstQuery * query)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "decide_allocation");

  return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_spinnaker_start (GstBaseSrc * src)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "start");

  spinError errReturn = SPINNAKER_ERR_SUCCESS;
  spinError err = SPINNAKER_ERR_SUCCESS;

  // Retrieve singleton reference to system object
  spinSystem hSystem = NULL;

  EXEANDCHECK(spinSystemGetInstance(&hSystem));

  // Retrieve list of cameras from the system
  //spinCameraList hCameraList = NULL;

  EXEANDCHECK(spinCameraListCreateEmpty(&spinnaker->hCameraList));

  EXEANDCHECK(spinSystemGetCameras(hSystem, spinnaker->hCameraList));

  // Retrieve number of cameras
  size_t numCameras = 0;

  EXEANDCHECK(spinCameraListGetSize(spinnaker->hCameraList, &numCameras));

	// display error when no camera has been found
	if (numCameras==0){
		GST_ERROR_OBJECT(src, "No Flycapture device found.");
		
    // Clear and destroy camera list before releasing system
    EXEANDCHECK(spinCameraListClear(spinnaker->hCameraList));

    EXEANDCHECK(spinCameraListDestroy(spinnaker->hCameraList));

    // Release system
    EXEANDCHECK(spinSystemReleaseInstance(hSystem));

    return FALSE;
	}

// Select camera
spinnaker->hCamera = NULL;

EXEANDCHECK(spinCameraListGet(spinnaker->hCameraList, 0, &spinnaker->hCamera));
EXEANDCHECK(spinCameraInit(spinnaker->hCamera));
  //GstVideoInfo vinfo;
  //GstCaps * filter;
  //GstCaps *caps = gst_spinnaker_get_caps (src, filter);
  //GstCaps * caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  //GstCaps * caps = GST_STATIC_CAPS ("video/x-raw, format=(string)RGB, height=(int)3000, width=(int)4000");
  //GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);

	//gst_video_info_from_caps (&vinfo, caps);

  //if (GST_VIDEO_INFO_FORMAT (&vinfo) != GST_VIDEO_FORMAT_UNKNOWN) {
    //  g_assert (spinnaker->hCamera != NULL);
      //  src->vrm_stride = get_pitch (src->device);  // wait for image to arrive for this
      //spinnaker->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);
      //spinnaker->nHeight = vinfo.height;
    //} else {
     // return FALSE;
    //}
spinnaker->gst_stride = 4000 * 4;
spinnaker->nHeight = 3000;
EXEANDCHECK(spinCameraBeginAcquisition(spinnaker->hCamera));

  return TRUE;

  fail:

    // Clear and destroy camera list before releasing system
    spinCameraListClear(spinnaker->hCameraList);

    spinCameraListDestroy(spinnaker->hCameraList);

    // Release system
    spinSystemReleaseInstance(hSystem);

  return FALSE;
}

static gboolean
gst_spinnaker_stop (GstBaseSrc * src)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "stop");

  EXEANDCHECK(spinCameraEndAcquisition(spinnaker->hCamera));

    // Retrieve singleton reference to system object
  spinSystem hSystem = NULL;
  //spinCameraList hCameraList = NULL;

  EXEANDCHECK(spinSystemGetInstance(&hSystem));
  EXEANDCHECK(spinSystemGetCameras(hSystem, spinnaker->hCameraList));
  EXEANDCHECK(spinCameraListClear(spinnaker->hCameraList));
  EXEANDCHECK(spinCameraListDestroy(spinnaker->hCameraList));
  EXEANDCHECK(spinSystemReleaseInstance(hSystem));
  return TRUE;

  fail:
  return FALSE;
}

/* given a buffer, return start and stop time when it should be pushed
 * out. The base class will sync on the clock using these times. */
/*
static void
gst_spinnaker_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "get_times");

}*/

/* get the total size of the resource in bytes */
static gboolean
gst_spinnaker_get_size (GstBaseSrc * src, guint64 * size)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "get_size");

  return TRUE;
}

/* check if the resource is seekable */
static gboolean
gst_spinnaker_is_seekable (GstBaseSrc * src)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "is_seekable");

  return FALSE;
}


/* Prepare the segment on which to perform do_seek(), converting to the
 * current basesrc format. */
/*
static gboolean
gst_spinnaker_prepare_seek_segment (GstBaseSrc * src, GstEvent * seek,
    GstSegment * segment)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "prepare_seek_segment");

  return TRUE;
}
*/
/* notify subclasses of a seek *//*
static gboolean
gst_spinnaker_do_seek (GstBaseSrc * src, GstSegment * segment)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "do_seek");

  return TRUE;
}*/

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
static gboolean
gst_spinnaker_unlock (GstBaseSrc * src)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "unlock");

  return TRUE;
}

/* Clear any pending unlock request, as we succeeded in unlocking */
static gboolean
gst_spinnaker_unlock_stop (GstBaseSrc * src)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "unlock_stop");

  return TRUE;
}
/*
/* notify subclasses of a query /
static gboolean
gst_spinnaker_query (GstBaseSrc * src, GstQuery * query)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);
  gboolean ret = TRUE;
  GST_DEBUG_OBJECT (spinnaker, "query");

    switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
      /* we should report the current position 
      [...]
      break;
    case GST_QUERY_DURATION:
      /* we should report the duration here 
      [...]
      break;
    case GST_QUERY_CAPS:
      /* we should report the supported caps here 
      [...]
      break;
    default:
      /* just call the default handler 
      ret = gst_pad_query_default (src->srcpad, query);
      break;
  }

  return TRUE;
}*/

/* notify subclasses of an event */
static gboolean
gst_spinnaker_event (GstBaseSrc * src, GstEvent * event)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "event");

  return TRUE;
}


/* Copy and duplicate data from the possibly binned image
 *  into the full size image
 */
void
copy_duplicate_data(GstBaseSrc * src, GstMapInfo *minfo)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);
	guint i, j, ii;

	// From the grabber source we get 1 progressive frame
	// We expect src->nPitch = src->gst_stride but use separate vars for safety

//	GST_DEBUG_OBJECT (src, "copy_duplicate_data: binning %d src->nRawWidth %d src->nRawHeight %d src->nRawPitch %d", src->binning, src->nRawWidth, src->nRawHeight, src->nRawPitch);

	if (spinnaker->binning == 1){   // just copy the data into the buffer
    size_t imageSize;
    EXEANDCHECK(spinImageGetBufferSize(spinnaker->convertedImage, &imageSize));

    void **data;
    data = (void**)malloc(imageSize * sizeof(void*));
    EXEANDCHECK(spinImageGetData(spinnaker->convertedImage, data));
    GST_DEBUG_OBJECT(spinnaker, "image is %d bytes", imageSize);

    spinnaker->nPitch = 4 * 4000;
    spinnaker->gst_stride = 4 * 4000;
    spinnaker->nHeight = 3000;
		for (i = 0; i < spinnaker->nHeight; i++) {
			memcpy (minfo->data + i * spinnaker->gst_stride, *data + i * spinnaker->nPitch, spinnaker->nPitch);
		}
    

    //memcpy(minfo->data, ppData, spinnaker->nHeight * spinnaker->nWidth * 3);
	}
  fail:
  return;
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
#ifdef OVERRIDE_CREATE
static GstFlowReturn
gst_spinnaker_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);
  GstMapInfo minfo;
  GST_DEBUG_OBJECT (spinnaker, "create");
  GstClock *clock;
  GstClockTime abs_time, base_time, timestamp, duration;

  spinImage hResultImage = NULL;
  EXEANDCHECK(spinImageCreateEmpty(&spinnaker->convertedImage));
  EXEANDCHECK(spinCameraGetNextImage(spinnaker->hCamera, &hResultImage));

  bool8_t isIncomplete = False;
  bool8_t hasFailed = False;

  EXEANDCHECK(spinImageIsIncomplete(hResultImage, &isIncomplete));

  
  //convert image to RGB
  EXEANDCHECK(spinImageConvert(hResultImage, PixelFormat_RGBa8, spinnaker->convertedImage));
  //spinImageSave(spinnaker->convertedImage, "demo.jpg", JPEG);
  EXEANDCHECK(spinImageRelease(hResultImage));

  // Create a new buffer for the image
  //*buf = gst_buffer_new_and_alloc (spinnaker->nHeight * spinnaker->gst_stride);
  *buf = gst_buffer_new_and_alloc (3000 * 4000 * 4);

  gst_buffer_map (*buf, &minfo, GST_MAP_WRITE);

  //GST_DEBUG_OBJECT(spinnaker, "copying data....");
  copy_duplicate_data(spinnaker, &minfo);
  //GST_DEBUG_OBJECT(spinnaker, "data copied!");
  //EXEANDCHECK(spinImageRelease(&spinnaker->convertedImage));
  //copy_interpolate_data(src, &minfo);  // NOT WORKING, SEE ABOVE

  // Normally this is commented out, useful for timing investigation
  //overlay_param_changed(src, &minfo);
  EXEANDCHECK(spinImageDestroy(spinnaker->convertedImage));
  gst_buffer_unmap (*buf, &minfo);

  //src->framerate = 1000.0/(src->exposure); // set a suitable frame rate for the exposure, if too fast for usb camera it will slow down.
	spinnaker->duration = 1000000000.0/spinnaker->framerate;  // frame duration in ns
  // If we do not use gst_base_src_set_do_timestamp() we need to add timestamps manually
  spinnaker->last_frame_time += spinnaker->duration;   // Get the timestamp for this frame
  if(!gst_base_src_get_do_timestamp(GST_BASE_SRC(src))){
    GST_BUFFER_PTS(*buf) = spinnaker->last_frame_time;  // convert ms to ns
    GST_BUFFER_DTS(*buf) = spinnaker->last_frame_time;  // convert ms to ns
  }
  GST_BUFFER_DURATION(*buf) = spinnaker->duration;
  GST_DEBUG_OBJECT(src, "pts, dts: %" GST_TIME_FORMAT ", duration: %d ms", GST_TIME_ARGS (spinnaker->last_frame_time), GST_TIME_AS_MSECONDS(spinnaker->duration));

  // count frames, and send EOS when required frame number is reached
  GST_BUFFER_OFFSET(*buf) = spinnaker->n_frames;  // from videotestsrc
  spinnaker->n_frames++;
  GST_BUFFER_OFFSET_END(*buf) = spinnaker->n_frames;  // from videotestsrc
  if (src->num_buffers>0)  // If we were asked for a specific number of buffers, stop when complete
    if (G_UNLIKELY(spinnaker->n_frames >= src->num_buffers))
      return GST_FLOW_EOS;


 timestamp = GST_BUFFER_TIMESTAMP (*buf);
  //duration = obj->duration;

  /* timestamps, LOCK to get clock and base time. */
  /* FIXME: element clock and base_time is rarely changing */
  GST_OBJECT_LOCK (spinnaker);
  if ((clock = GST_ELEMENT_CLOCK (spinnaker))) {
    /* we have a clock, get base time and ref clock */
    base_time = GST_ELEMENT (spinnaker)->base_time;
    gst_object_ref (clock);
  } else {
    /* no clock, can't set timestamps */
    GST_DEBUG(spinnaker,"no clock");
    base_time = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK (spinnaker);

  /* sample pipeline clock */
  if (clock) {
    abs_time = gst_clock_get_time (clock);
    gst_object_unref (clock);
  } else {
    GST_DEBUG(spinnaker,"no abs time");
    abs_time = GST_CLOCK_TIME_NONE;
  }

  //GST_DEBUG_OBJECT(spinnaker, "done!");
  return GST_FLOW_OK;

  fail:

  return GST_FLOW_ERROR;
}
#endif

#ifndef OVERRIDE_CREATE
/* ask the subclass to allocate an output buffer. The default implementation
 * will use the negotiated allocator. */
static GstFlowReturn
gst_spinnaker_alloc (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "alloc");

  return GST_FLOW_OK;
}

/* ask the subclass to fill the buffer with data from offset and size */
static GstFlowReturn
gst_spinnaker_fill (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer * buf)
{
  GstSpinnaker *spinnaker = GST_SPINNAKER (src);

  GST_DEBUG_OBJECT (spinnaker, "fill");

  return GST_FLOW_OK;
}
#endif

static gboolean
plugin_init (GstPlugin * plugin)
{

  /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register (plugin, "gstspinnakersrc", GST_RANK_NONE,
      GST_TYPE_SPINNAKER);
}

/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "GST Spinnaker"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "spinnaker src"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "github.com/thompd27"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    spinnaker,
    "plugin for interfacing with point grey cameras based on the spinnaker API",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN);
