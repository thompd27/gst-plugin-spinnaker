/* GStreamer Flycap Plugin
 * Copyright (C) 2015-2016 Gray Cancer Institute
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
 *
 * Author: P Barber
 *
 */
/**
 * SECTION:element-GstSpinnaker_src
 *
 * The flycapsrc element is a source for a USB 3 camera supported by the Point Grey Fly Capture SDK.
 * A live source, operating in push mode.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 flycapsrc ! videoconvert ! autovideosink
 * ]|
 * </refsect2>
 */

// Which functions of the base class to override. Create must alloc and fill the buffer. Fill just needs to fill it
//#define OVERRIDE_FILL  !!! NOT IMPLEMENTED !!!
#define OVERRIDE_CREATE

#include <unistd.h> // for usleep
#include <string.h> // for memcpy
#include <math.h>  // for pow
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

//#include "FlyCapture2_C.h"

#include "gstspinnaker.h"

GST_DEBUG_CATEGORY_STATIC (gst_spinnaker_src_debug);
#define GST_CAT_DEFAULT gst_spinnaker_src_debug

/* prototypes */
static void gst_spinnaker_src_set_property (GObject * object,
		guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_spinnaker_src_get_property (GObject * object,
		guint property_id, GValue * value, GParamSpec * pspec);
static void gst_spinnaker_src_dispose (GObject * object);
static void gst_spinnaker_src_finalize (GObject * object);

static gboolean gst_spinnaker_src_start (GstBaseSrc * src);
static gboolean gst_spinnaker_src_stop (GstBaseSrc * src);
static GstCaps *gst_spinnaker_src_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_spinnaker_src_set_caps (GstBaseSrc * src, GstCaps * caps);

#ifdef OVERRIDE_CREATE
	static GstFlowReturn gst_spinnaker_src_create (GstPushSrc * src, GstBuffer ** buf);
#endif
#ifdef OVERRIDE_FILL
	static GstFlowReturn gst_spinnaker_src_fill (GstPushSrc * src, GstBuffer * buf);
#endif

//static GstCaps *gst_spinnaker_src_create_caps (GstSpinnakerSrc * src);
static void gst_spinnaker_src_reset (GstSpinnakerSrc * src);
enum
{
	PROP_0,
	PROP_CAMERAPRESENT,
	PROP_EXPOSURE,
	PROP_PIXELCLOCK,
	PROP_GAIN,
	PROP_BLACKLEVEL,
	PROP_RGAIN,
	PROP_GGAIN,
	PROP_BGAIN,
	PROP_BINNING,
	PROP_SHARPNESS,
	PROP_SATURATION,
	PROP_WHITEBALANCE,
	PROP_WB_ONEPUSHINPROGRESS,
	PROP_LUT,
	PROP_LUT1_OFFSET_R,
	PROP_LUT1_OFFSET_G,
	PROP_LUT1_OFFSET_B,
	PROP_LUT1_GAMMA,
	PROP_LUT1_GAIN,
	PROP_LUT2_OFFSET_R,
	PROP_LUT2_OFFSET_G,
	PROP_LUT2_OFFSET_B,
	PROP_LUT2_GAMMA,
	PROP_LUT2_GAIN,
	PROP_MAXFRAMERATE
};

#define	FLYCAP_UPDATE_LOCAL  FALSE
#define	FLYCAP_UPDATE_CAMERA TRUE

#define DEFAULT_PROP_EXPOSURE           40.0
#define DEFAULT_PROP_GAIN               1
#define DEFAULT_PROP_BLACKLEVEL         15
#define DEFAULT_PROP_RGAIN              425
#define DEFAULT_PROP_BGAIN              727
#define DEFAULT_PROP_BINNING            1
#define DEFAULT_PROP_SHARPNESS			2    // this is 'normal'
#define DEFAULT_PROP_SATURATION			25   // this is 100 on the camera scale 0-400
#define DEFAULT_PROP_HORIZ_FLIP         0
#define DEFAULT_PROP_VERT_FLIP          0
#define DEFAULT_PROP_WHITEBALANCE       GST_WB_MANUAL
#define DEFAULT_PROP_LUT		        GST_LUT_1
#define DEFAULT_PROP_LUT1_OFFSET		0    
#define DEFAULT_PROP_LUT1_GAMMA		    0.45
#define DEFAULT_PROP_LUT1_GAIN		    1.099
#define DEFAULT_PROP_LUT2_OFFSET		10    
#define DEFAULT_PROP_LUT2_GAMMA		    0.45
#define DEFAULT_PROP_LUT2_GAIN		    1.501   
#define DEFAULT_PROP_MAXFRAMERATE       25
#define DEFAULT_PROP_GAMMA			    1.5

#define DEFAULT_GST_VIDEO_FORMAT GST_VIDEO_FORMAT_RGB
#define DEFAULT_FLYCAP_VIDEO_FORMAT FC2_PIXEL_FORMAT_RGB8
// Put matching type text in the pad template below

// pad template
static GstStaticPadTemplate gst_spinnaker_src_template =
		GST_STATIC_PAD_TEMPLATE ("src",
				GST_PAD_SRC,
				GST_PAD_ALWAYS,
				GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
						("{ BGR }"))
		);

#define EXEANDCHECK(function) \
{\
	spinError Ret = function;\
	if (SPINNAKER_ERR_SUCCESS != Ret){\
		GST_ERROR_OBJECT(src, "Spinnaker call failed: %d", Ret);\
		goto fail;\
	}\
}

G_DEFINE_TYPE_WITH_CODE (GstSpinnakerSrc, gst_spinnaker_src, GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "spinnaker", 0,
        "debug category for spinnaker element"));

/* class initialisation */

//G_DEFINE_TYPE (GstSpinnakerSrc, gst_spinnaker_src, GST_TYPE_PUSH_SRC);

static void
gst_spinnaker_src_class_init (GstSpinnakerSrcClass * klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
	GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
	GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

	GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "gstspinnakersrc", 0,
			"Spinnaker Camera source");

	gobject_class->set_property = gst_spinnaker_src_set_property;
	gobject_class->get_property = gst_spinnaker_src_get_property;
	gobject_class->dispose = gst_spinnaker_src_dispose;
	gobject_class->finalize = gst_spinnaker_src_finalize;

	gst_element_class_add_pad_template (gstelement_class,
			gst_static_pad_template_get (&gst_spinnaker_src_template));

	gst_element_class_set_static_metadata (gstelement_class,
			"Spinnaker Video Source", "Source/Video",
			"Spinnaker Camera video source", "David Thompson <dave@republicofdave.net>");

	gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_spinnaker_src_start);
	gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_spinnaker_src_stop);
	gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_spinnaker_src_get_caps);
	gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_spinnaker_src_set_caps);

#ifdef OVERRIDE_CREATE
	gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_spinnaker_src_create);
	GST_DEBUG ("Using gst_spinnaker_src_create.");
#endif
#ifdef OVERRIDE_FILL
	gstpushsrc_class->fill   = GST_DEBUG_FUNCPTR (gst_spinnaker_src_fill);
	GST_DEBUG ("Using gst_spinnaker_src_fill.");
#endif
}

static void
init_properties(GstSpinnakerSrc * src)
{
    //Hardcoded hot-garbage **************************
  src->nWidth = 4000;
	src->nHeight = 3000;
	src->nRawWidth = 4000;
	src->nRawHeight = 3000;
  src->nBytesPerPixel = 4;
  src->binning = 1;
  src->n_frames = 0;
  src->framerate = 31;
  src->last_frame_time = 0;
  src->nPitch = src->nWidth * src->nBytesPerPixel;
	src->nRawPitch = src->nRawWidth * src->nBytesPerPixel;

}

static void
gst_spinnaker_src_init (GstSpinnakerSrc * src)
{
	/* set source as live (no preroll) */
	gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

	/* override default of BYTES to operate in time mode */
	gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

	init_properties(src);

	gst_spinnaker_src_reset (src);
}

static void
gst_spinnaker_src_reset (GstSpinnakerSrc * src)
{
	src->n_frames = 0;
	src->total_timeouts = 0;
	src->last_frame_time = 0;
}

void
gst_spinnaker_src_set_property (GObject * object, guint property_id,
		const GValue * value, GParamSpec * pspec)
{
	GstSpinnakerSrc *src;

	src = GST_SPINNAKER_SRC (object);
}

void
gst_spinnaker_src_get_property (GObject * object, guint property_id,
		GValue * value, GParamSpec * pspec)
{
	GstSpinnakerSrc *src;

	g_return_if_fail (GST_IS_SPINNAKER_SRC (object));
	src = GST_SPINNAKER_SRC (object);
}

void
gst_spinnaker_src_dispose (GObject * object)
{
	GstSpinnakerSrc *src;

	g_return_if_fail (GST_IS_SPINNAKER_SRC (object));
	src = GST_SPINNAKER_SRC (object);

	GST_DEBUG_OBJECT (src, "dispose");

	// clean up as possible.  may be called multiple times

	G_OBJECT_CLASS (gst_spinnaker_src_parent_class)->dispose (object);
}

void
gst_spinnaker_src_finalize (GObject * object)
{
	GstSpinnakerSrc *src;

	g_return_if_fail (GST_IS_SPINNAKER_SRC (object));
	src = GST_SPINNAKER_SRC (object);

	GST_DEBUG_OBJECT (src, "finalize");

	/* clean up object here */
	G_OBJECT_CLASS (gst_spinnaker_src_parent_class)->finalize (object);
}

static gboolean
gst_spinnaker_src_start (GstBaseSrc * bsrc)
{
	// Start will open the device but not start it, set_caps starts it, stop should stop and close it (as v4l2src)

	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (bsrc);
  unsigned int numCameras = 0;

	GST_DEBUG_OBJECT (src, "start");
  spinError errReturn = SPINNAKER_ERR_SUCCESS;
  spinError err = SPINNAKER_ERR_SUCCESS;

  // Retrieve singleton reference to system object
  spinSystem hSystem = NULL;

  EXEANDCHECK(spinSystemGetInstance(&hSystem));

	// Turn on automatic timestamping, if so we do not need to do it manually, BUT there is some evidence that automatic timestamping is laggy
//	gst_base_src_set_do_timestamp(bsrc, TRUE);
  EXEANDCHECK(spinCameraListCreateEmpty(&src->hCameraList));

  EXEANDCHECK(spinSystemGetCameras(hSystem, src->hCameraList));

  EXEANDCHECK(spinCameraListGetSize(src->hCameraList, &numCameras));

	// display error when no camera has been found
	if (numCameras==0){
		GST_ERROR_OBJECT(src, "No Flycapture device found.");
		
    // Clear and destroy camera list before releasing system
    EXEANDCHECK(spinCameraListClear(src->hCameraList));

    EXEANDCHECK(spinCameraListDestroy(src->hCameraList));

    // Release system
    EXEANDCHECK(spinSystemReleaseInstance(hSystem));

		GST_ERROR_OBJECT(src, "No device found.");
		goto fail;
	}

  // Select camera
  src->hCamera = NULL;

  EXEANDCHECK(spinCameraListGet(src->hCameraList, 0, &src->hCamera));
  EXEANDCHECK(spinCameraInit(src->hCamera));
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
  src->gst_stride = 4000 * 4;
  src->nHeight = 3000;
  EXEANDCHECK(spinCameraBeginAcquisition(src->hCamera));
	// NOTE:
	// from now on, the "deviceContext" handle can be used to access the camera board.
	// use fc2DestroyContext to end the usage
	src->cameraPresent = TRUE;

	return TRUE;

	fail:

    // Clear and destroy camera list before releasing system
    spinCameraListClear(src->hCameraList);

    spinCameraListDestroy(src->hCameraList);

    // Release system
    spinSystemReleaseInstance(hSystem);

	return FALSE;
}

static gboolean
gst_spinnaker_src_stop (GstBaseSrc * bsrc)
{
	// Start will open the device but not start it, set_caps starts it, stop should stop and close it (as v4l2src)

	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (bsrc);

	GST_DEBUG_OBJECT (src, "stop");
  EXEANDCHECK(spinCameraEndAcquisition(src->hCamera));

    // Retrieve singleton reference to system object
  spinSystem hSystem = NULL;
  //spinCameraList hCameraList = NULL;

  EXEANDCHECK(spinSystemGetInstance(&hSystem));
  EXEANDCHECK(spinSystemGetCameras(hSystem, src->hCameraList));
  EXEANDCHECK(spinCameraListClear(src->hCameraList));
  EXEANDCHECK(spinCameraListDestroy(src->hCameraList));
  EXEANDCHECK(spinSystemReleaseInstance(hSystem));

	gst_spinnaker_src_reset (src);

	fail:   // Needed for FLYCAPEXECANDCHECK, does nothing in this case
	return TRUE;
}

static GstCaps *
gst_spinnaker_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (bsrc);
	GstCaps *caps;

  
  caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));

	GST_DEBUG_OBJECT (src, "The caps are %" GST_PTR_FORMAT, caps);

	return caps;
}

static gboolean
gst_spinnaker_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
	// Start will open the device but not start it, set_caps starts it, stop should stop and close it (as v4l2src)

	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (bsrc);
	GstVideoInfo vinfo;
	//GstStructure *s = gst_caps_get_structure (caps, 0);
/*
    if(src->acq_started == TRUE){
		FLYCAPEXECANDCHECK(fc2StopCapture(src->deviceContext));
		src->acq_started = FALSE;
    }*/

	GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);

	//gst_video_info_from_caps (&vinfo, caps);

/*
	if (GST_VIDEO_INFO_FORMAT (&vinfo) != GST_VIDEO_FORMAT_UNKNOWN) {
		g_assert (src->deviceContext != NULL);
		//  src->vrm_stride = get_pitch (src->device);  // wait for image to arrive for this
		src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);
		src->nHeight = vinfo.height;
	} else {
		goto unsupported_caps;
	}*/

	// start freerun/continuous capture
	GST_DEBUG_OBJECT (src, "fc2StartCapture");
	//EXEANDCHECK(spinCameraBeginAcquisition(src->hCamera));
	GST_DEBUG_OBJECT (src, "fc2StartCapture COMPLETED");
	src->acq_started = TRUE;

	return TRUE;

	unsupported_caps:
	GST_ERROR_OBJECT (src, "Unsupported caps: %" GST_PTR_FORMAT, caps);
	return FALSE;

	fail:
	return FALSE;
}
/*
  for (i = 0; i < src->nHeight; i++) {
    memcpy (minfo->data + i * src->gst_stride, src->convertedImage.pData + i * src->nPitch, src->nPitch);
  }*/
//  This can override the push class create fn, it is the same as fill above but it forces the creation of a buffer here to copy into.
#ifdef OVERRIDE_CREATE
static GstFlowReturn
gst_spinnaker_src_create (GstPushSrc * psrc, GstBuffer ** buf)
{
	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (psrc);
	GstMapInfo minfo;

  spinImage hResultImage = NULL;
  EXEANDCHECK(spinImageCreateEmpty(&src->convertedImage));
  EXEANDCHECK(spinCameraGetNextImage(src->hCamera, &hResultImage));

  bool8_t isIncomplete = False;
  bool8_t hasFailed = False;

  EXEANDCHECK(spinImageIsIncomplete(hResultImage, &isIncomplete));

  
  //convert image to RGB
  EXEANDCHECK(spinImageConvert(hResultImage, PixelFormat_BGR8, src->convertedImage));
  //spinImageSave(spinnaker->convertedImage, "demo.jpg", JPEG);
  EXEANDCHECK(spinImageRelease(hResultImage));

  // Create a new buffer for the image
  //*buf = gst_buffer_new_and_alloc (spinnaker->nHeight * spinnaker->gst_stride);
  *buf = gst_buffer_new_and_alloc (3000 * 4000 * 3);

  gst_buffer_map (*buf, &minfo, GST_MAP_WRITE);
  size_t imageSize;
  EXEANDCHECK(spinImageGetBufferSize(src->convertedImage, &imageSize));

  void **data;
  data = (void**)malloc(imageSize * sizeof(void*));
  EXEANDCHECK(spinImageGetData(src->convertedImage, data));

  src->nPitch = 3 * 4000;
  src->gst_stride = 3 * 4000;
  src->nHeight = 3000;
  for (int i = 0; i < src->nHeight; i++) {
    	memcpy (minfo.data + i * src->gst_stride, *data + i * src->nPitch, src->nPitch);
  }
		//copy_duplicate_data(src, &minfo);
		//copy_interpolate_data(src, &minfo);  // NOT WORKING, SEE ABOVE

		// Normally this is commented out, useful for timing investigation
		//overlay_param_changed(src, &minfo);
    EXEANDCHECK(spinImageDestroy(src->convertedImage));
		gst_buffer_unmap (*buf, &minfo);

    src->duration = 1000000000.0/src->framerate; 
		// If we do not use gst_base_src_set_do_timestamp() we need to add timestamps manually
		src->last_frame_time += src->duration;   // Get the timestamp for this frame
		if(!gst_base_src_get_do_timestamp(GST_BASE_SRC(psrc))){
			GST_BUFFER_PTS(*buf) = src->last_frame_time;  // convert ms to ns
			GST_BUFFER_DTS(*buf) = src->last_frame_time;  // convert ms to ns
		}
		GST_BUFFER_DURATION(*buf) = src->duration;
		GST_DEBUG_OBJECT(src, "pts, dts: %" GST_TIME_FORMAT ", duration: %d ms", GST_TIME_ARGS (src->last_frame_time), GST_TIME_AS_MSECONDS(src->duration));

		// count frames, and send EOS when required frame number is reached
		GST_BUFFER_OFFSET(*buf) = src->n_frames;  // from videotestsrc
		src->n_frames++;
		GST_BUFFER_OFFSET_END(*buf) = src->n_frames;  // from videotestsrc
		if (psrc->parent.num_buffers>0)  // If we were asked for a specific number of buffers, stop when complete
			if (G_UNLIKELY(src->n_frames >= psrc->parent.num_buffers))
				return GST_FLOW_EOS;

	return GST_FLOW_OK;
  fail:
  return GST_FLOW_ERROR;
}
#endif // OVERRIDE_CREATE

static gboolean
plugin_init (GstPlugin * plugin)
{

  /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register (plugin, "gstspinnakersrc", GST_RANK_NONE,
      GST_TYPE_SPINNAKER_SRC);

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