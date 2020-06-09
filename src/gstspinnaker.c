/* GStreamer Spinnaker Plugin
 * Copyright (C) 2019 Embry-Riddle Aeronautical University
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
 * Author: D Thompson
 *
 */
/**
 * SECTION:element-GstSpinnaker_src
 *
 * The spinnakersrc element is a source for a USB 3 camera supported by the FLIR Spinnaker SDK.
 * A live source, operating in push mode.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 spinnakersrc ! videoconvert ! autovideosink
 * ]|
 * </refsect2>
 */

// Which functions of the base class to override. Create must alloc and fill the buffer. Fill just needs to fill it
//#define OVERRIDE_FILL  !!! NOT IMPLEMENTED !!!
#define OVERRIDE_CREATE

//#include <unistd.h> // for usleep
#include <string.h> // for memcpy
#include <math.h>  // for pow
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>
#include <stdio.h>
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
	PROP_CAMERA,
	PROP_WIDTH,
	PROP_HEIGHT
};

#define	FLYCAP_UPDATE_LOCAL  FALSE
#define	FLYCAP_UPDATE_CAMERA TRUE

#define DEFAULT_PROP_CAMERA	           0
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
#define DEFAULT_PROP_WIDTH 				800
#define DEFAULT_PROP_HEIGHT			    600
#define MAX_BUFF_LEN 256

#define DEFAULT_GST_VIDEO_FORMAT GST_VIDEO_FORMAT_GRAY8
//#define DEFAULT_FLYCAP_VIDEO_FORMAT FC2_PIXEL_FORMAT_RGB8
// Put matching type text in the pad template below

// pad template
static GstStaticPadTemplate gst_spinnaker_src_template =
		GST_STATIC_PAD_TEMPLATE ("src",
				GST_PAD_SRC,
				GST_PAD_ALWAYS,
				GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
						("{ GRAY8 }"))
		);

#define EXEANDCHECK(function) \
{\
	spinError Ret = function;\
	if (SPINNAKER_ERR_SUCCESS != Ret){\
		GST_ERROR_OBJECT(src, "Spinnaker call failed: %d", Ret);\
		goto fail;\
	}\
}

// This function configures the camera to add chunk data to each image. It does
// this by enabling each type of chunk data before enabling chunk data mode.
// When chunk data is turned on, the data is made available in both the nodemap
// and each image.


G_DEFINE_TYPE_WITH_CODE (GstSpinnakerSrc, gst_spinnaker_src, GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "spinnaker", 0,
        "debug category for spinnaker element"));

// This function helps to check if a node is available and writable
bool8_t IsAvailableAndWritable(spinNodeHandle hNode, char nodeName[])
{
    bool8_t pbAvailable = False;
    spinError err = SPINNAKER_ERR_SUCCESS;
    err = spinNodeIsAvailable(hNode, &pbAvailable);
    if (err != SPINNAKER_ERR_SUCCESS)
    {
        //printf("Unable to retrieve node availability (%s node), with error %d...\n\n", nodeName, err);
    }

    bool8_t pbWritable = False;
    err = spinNodeIsWritable(hNode, &pbWritable);
    if (err != SPINNAKER_ERR_SUCCESS)
    {
        //printf("Unable to retrieve node writability (%s node), with error %d...\n\n", nodeName, err);
    }
    return pbWritable && pbAvailable;
}

// This function helps to check if a node is available and readable
bool8_t IsAvailableAndReadable(spinNodeHandle hNode, char nodeName[])
{
	bool8_t pbAvailable = False;
	spinError err = SPINNAKER_ERR_SUCCESS;
	err = spinNodeIsAvailable(hNode, &pbAvailable);
	if (err != SPINNAKER_ERR_SUCCESS)
	{
		printf("Unable to retrieve node availability (%s node), with error %d...\n\n", nodeName, err);
	}

	bool8_t pbReadable = False;
	err = spinNodeIsReadable(hNode, &pbReadable);
	if (err != SPINNAKER_ERR_SUCCESS)
	{
		printf("Unable to retrieve node readability (%s node), with error %d...\n\n", nodeName, err);
	}
	return pbReadable && pbAvailable;
}


spinError ConfigureChunkData(spinNodeMapHandle hNodeMap)
{
	spinError err = SPINNAKER_ERR_SUCCESS;

	unsigned int i = 0;

	//printf("\n\n*** CONFIGURING CHUNK DATA ***\n\n");

	//
	// Activate chunk mode
	//
	// *** NOTES ***
	// Once enabled, chunk data will be available at the end of the payload of
	// every image captured until it is disabled. Chunk data can also be
	// retrieved from the nodemap.
	//
	spinNodeHandle hChunkModeActive = NULL;

	err = spinNodeMapGetNode(hNodeMap, "ChunkModeActive", &hChunkModeActive);

	if (err != SPINNAKER_ERR_SUCCESS)
	{
		//printf("Unable to activate chunk mode. Aborting with error %d...\n\n", err);
		return err;
	}

	//check if available and writable
	if (IsAvailableAndWritable(hChunkModeActive, "ChunkModeActive"))
	{
		err = spinBooleanSetValue(hChunkModeActive, True);
		if (err != SPINNAKER_ERR_SUCCESS)
		{
			//printf("Unable to activate chunk mode. Aborting with error %d...\n\n", err);
			return err;
		}
	}
	else
	{
		//PrintRetrieveNodeFailure("node", "ChunkModeActive");
		return SPINNAKER_ERR_ACCESS_DENIED;
	}

	//printf("Chunk mode activated...\n");

	//
	// Enable all types of chunk data
	//
	// *** NOTES ***
	// Enabling chunk data requires working with nodes: "ChunkSelector" is an
	// enumeration selector node and "ChunkEnable" is a boolean. It requires
	// retrieving the selector node (which is of enumeration node type),
	// selecting the entry of the chunk data to be enabled, retrieving the
	// corresponding boolean, and setting it to true.
	//
	// In this example, all chunk data is enabled, so these steps are performed
	// in a loop. Once this is complete, chunk mode still needs to be activated.
	//
	spinNodeHandle hChunkSelector = NULL;
	size_t numEntries = 0;

	// Retrieve selector node, check if available and readable and writable
	err = spinNodeMapGetNode(hNodeMap, "ChunkSelector", &hChunkSelector);

	if (err != SPINNAKER_ERR_SUCCESS)
	{
		//printf("Unable to retrieve chunk selector entries. Aborting with error %d...\n\n", err);
		return err;
	}

	// Retrieve number of entries
	if (IsAvailableAndReadable(hChunkSelector, "ChunkSelector"))
	{
		err = spinEnumerationGetNumEntries(hChunkSelector, &numEntries);
		if (err != SPINNAKER_ERR_SUCCESS)
		{
			//printf("Unable to retrieve number of entries. Aborting with error %d...\n\n", err);
			return err;
		}
	}
	else
	{
		//PrintRetrieveNodeFailure("node", "ChunkSelector");
		return SPINNAKER_ERR_ACCESS_DENIED;
	}

	printf("Enabling entries...\n");


	for (i = 0; i < numEntries; i++)
	{
		// Retrieve entry node
		spinNodeHandle hEntry = NULL;

		err = spinEnumerationGetEntryByIndex(hChunkSelector, i, &hEntry);
		if (err != SPINNAKER_ERR_SUCCESS)
		{
			//printf("\tUnable to enable chunk entry (error %d)...\n\n", err);
			continue;
		}

		// Check if available and readable, retrieve entry name
		char entryName[MAX_BUFF_LEN];
		size_t lenEntryName = MAX_BUFF_LEN;

		if (IsAvailableAndReadable(hEntry, "ChunkEntry"))
		{
			err = spinNodeGetDisplayName(hEntry, entryName, &lenEntryName);
			if (err != SPINNAKER_ERR_SUCCESS)
			{
				//printf("\t%s: unable to retrieve chunk entry display name (error %d)...\n", entryName, err);
				//strcpy(entryName, "Unknown");
			}
		}
		else
		{
			//PrintRetrieveNodeFailure("entry", "ChunkEntry");
			//strcpy(entryName, "Unknown");
			continue;
		}
		// Retrieve enum entry integer value
		int64_t value = 0;

		err = spinEnumerationEntryGetIntValue(hEntry, &value);

		if (err != SPINNAKER_ERR_SUCCESS)
		{
			//printf("\t%s: unable to get chunk entry value (error %d)...\n", entryName, err);
			continue;
		}

		// Set integer value
		if (IsAvailableAndWritable(hChunkSelector, "ChunkSelector"))
		{
			err = spinEnumerationSetIntValue(hChunkSelector, value);
			if (err != SPINNAKER_ERR_SUCCESS)
			{
				//printf("\t%s: unable to set chunk entry value (error %d)...\n", entryName, err);
				continue;
			}
		}
		else
		{
			//PrintRetrieveNodeFailure("node", "ChunkSelector");
			return SPINNAKER_ERR_ACCESS_DENIED;
		}

		// Retrieve corresponding chunk enable node
		spinNodeHandle hChunkEnable = NULL;

		err = spinNodeMapGetNode(hNodeMap, "ChunkEnable", &hChunkEnable);
		if (err != SPINNAKER_ERR_SUCCESS)
		{
			//printf("\t%s: unable to get entry from nodemap (error %d)...\n", entryName, err);
			continue;
		}

		// Retrieve chunk enable value and set to true if necessary
		bool8_t isEnabled = False;

		if (IsAvailableAndWritable(hChunkEnable, "ChunkEnable"))
		{
			err = spinBooleanGetValue(hChunkEnable, &isEnabled);
			if (err != SPINNAKER_ERR_SUCCESS)
			{
				//printf("\t%s: unable to get chunk entry boolean value (error %d)...\n", entryName, err);
				continue;
			}
		}
		else
		{
			//PrintRetrieveNodeFailure("node", "ChunkEnable");
			continue;
		}
		// Consider the case in which chunk data is enabled but not writable
		if (!isEnabled)
		{
			// Set chunk enable value to true

			err = spinBooleanSetValue(hChunkEnable, True);
			if (err != SPINNAKER_ERR_SUCCESS)
			{
				//printf("\t%s: unable to set chunk entry boolean value (error %d)...\n", entryName, err);
				continue;
			}

		}

		//printf("\t%s: enabled\n", entryName);
	}

	return err;
}
/* class initialisation */
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
	//camera id property
	g_object_class_install_property (gobject_class, PROP_CAMERA,
		g_param_spec_int("camera-id", "Camera ID", "Camera ID to open.", 0,7, DEFAULT_PROP_CAMERA,
		 (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));
}

static void
init_properties(GstSpinnakerSrc * src)
{
    //Hardcoded hot-garbage **************************
  src->nWidth = DEFAULT_PROP_WIDTH;
  src->nHeight = DEFAULT_PROP_HEIGHT;
  src->nBytesPerPixel = 1;
  src->binning = 1;
  src->n_frames = 0;
  src->framerate = 31;
  src->last_frame_time = 0;
  src->nPitch = src->nWidth * src->nBytesPerPixel;
  src->gst_stride = src->nPitch;
  src->cameraID = DEFAULT_PROP_CAMERA;

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
	src->cameraID = 0;
	src->hCameraList = NULL;
	src->cameraPresent = FALSE;
	src->hSystem = NULL;
}

void
gst_spinnaker_src_set_property (GObject * object, guint property_id,
		const GValue * value, GParamSpec * pspec)
{
	GstSpinnakerSrc *src;

	src = GST_SPINNAKER_SRC (object);

	/*
	spinImage hCamera = NULL;
	spinNodeMapHandle hNodeMap = NULL;
  	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));
	*/
	spinNodeHandle hWidth = NULL;
	int64_t maxWidth = 0;
	spinNodeHandle hHeight = NULL;
	int64_t maxHeight = 0;
	switch(property_id) {
	case PROP_CAMERA:
		src->cameraID = g_value_get_int (value);
		GST_DEBUG_OBJECT (src, "camera id: %d", src->cameraID);
		break;
	case PROP_WIDTH:
		/*
		EXEANDCHECK(spinNodeMapGetNode(hNodeMap, "Width", &hWidth));

		// Retrieve maximum width
		if (IsAvailableAndWritable(hWidth, "Width"))
		{
			EXEANDCHECK(spinIntegerGetMax(hWidth, &maxWidth));
			int64_t param = g_value_get_int(value);
			if (param > maxWidth){
				EXEANDCHECK(spinIntegerSetValue(hWidth, maxWidth));
				src->nWidth = maxWidth;
			}
			else {
				EXEANDCHECK(spinIntegerSetValue(hWidth, param));
				src->nWidth = param;
			}
		}
		*/
		break;
	case PROP_HEIGHT:
		/*
		EXEANDCHECK(spinNodeMapGetNode(hNodeMap, "Height", &hHeight));

		// Retrieve maximum width
		if (IsAvailableAndWritable(hHeight, "Height"))
		{
			EXEANDCHECK(spinIntegerGetMax(hHeight, &maxHeight));
			int64_t param = g_value_get_int(value);
			if (param > maxHeight){
				EXEANDCHECK(spinIntegerSetValue(hHeight, maxHeight));
				src->nHeight = maxHeight;
			}
			else {
				EXEANDCHECK(spinIntegerSetValue(hWidth, param));
				src->nHeight = param;
			}
		}
		*/
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}

	fail:
	return;
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

//queries camera devices and begins acquisition
static gboolean
gst_spinnaker_src_start (GstBaseSrc * bsrc)
{
	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (bsrc);
    size_t numCameras = 0;

	GST_DEBUG_OBJECT (src, "start");
	
  	spinError errReturn = SPINNAKER_ERR_SUCCESS;
  	spinError err = SPINNAKER_ERR_SUCCESS;

	//grab system reference
    EXEANDCHECK(spinSystemGetInstance(&src->hSystem));

	//query available cameras
    EXEANDCHECK(spinCameraListCreateEmpty(&src->hCameraList));
	GST_DEBUG_OBJECT (src, "getting camera list");
    EXEANDCHECK(spinSystemGetCameras(src->hSystem, src->hCameraList));
	GST_DEBUG_OBJECT (src, "getting number of cameras");
    EXEANDCHECK(spinCameraListGetSize(src->hCameraList, &numCameras));
	
	// display error when no camera has been found
	if (numCameras==0){
		GST_ERROR_OBJECT(src, "No Point Grey device found.");
		
    // Clear and destroy camera list before releasing system
    EXEANDCHECK(spinCameraListClear(src->hCameraList));

    EXEANDCHECK(spinCameraListDestroy(src->hCameraList));

    // Release system
    EXEANDCHECK(spinSystemReleaseInstance(src->hSystem));

		GST_ERROR_OBJECT(src, "No device found.");
		goto fail;
	}

    // Select camera
  	spinCamera hCamera = NULL;
	GST_DEBUG_OBJECT (src, "selecting camera");
    EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	GST_DEBUG_OBJECT (src, "initializing camera");
    EXEANDCHECK(spinCameraInit(hCamera));

	//set camera to send back timestamp information
	spinNodeMapHandle hNodeMap = NULL;
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));
	err = ConfigureChunkData(hNodeMap);
	if (err != SPINNAKER_ERR_SUCCESS) { goto fail; };
	//starts camera acquisition. Doesn't actually fill the gstreamer buffer. see create function
	GST_DEBUG_OBJECT (src, "starting acquisition");
    EXEANDCHECK(spinCameraBeginAcquisition(hCamera));

	//grab 1 image to set initial timestamp
	spinImage hResultImage = NULL;
	EXEANDCHECK(spinCameraGetNextImage(hCamera, &hResultImage));
	bool8_t isIncomplete = False;
	bool8_t hasFailed = False;
	//check if image is complete 
	//WARNING: This returns a boolean and is not handled if the image is incomplete
	EXEANDCHECK(spinImageIsIncomplete(hResultImage, &isIncomplete));
	int64_t timestamp = 0;
	EXEANDCHECK(spinImageChunkDataGetIntValue(hResultImage, "ChunkTimestamp", &timestamp));
	src->last_frame_time = timestamp;
	EXEANDCHECK(spinCameraRelease(hCamera));
	EXEANDCHECK(spinImageRelease(hResultImage));
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
    spinSystemReleaseInstance(src->hSystem);

	return FALSE;
}

//stops streaming and closes the camera
static gboolean
gst_spinnaker_src_stop (GstBaseSrc * bsrc)
{
	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (bsrc);

	GST_DEBUG_OBJECT (src, "stop");
	spinImage hCamera = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
  	EXEANDCHECK(spinCameraEndAcquisition(hCamera));
  	EXEANDCHECK(spinCameraDeInit(hCamera));
  	EXEANDCHECK(spinCameraRelease(hCamera));

	EXEANDCHECK(spinCameraListClear(src->hCameraList));
	EXEANDCHECK(spinCameraListDestroy(src->hCameraList));
	EXEANDCHECK(spinSystemReleaseInstance(src->hSystem));

	gst_spinnaker_src_reset (src);
	GST_DEBUG_OBJECT (src, "stop completed");
	return TRUE;

	fail:  
	return TRUE;
}

static GstCaps *
gst_spinnaker_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (bsrc);
	GstCaps *caps;

	GstVideoInfo vinfo;

	gst_video_info_init(&vinfo);

	vinfo.width = src->nWidth;
	vinfo.height = src->nHeight;
	spinImage hCamera = NULL;
	spinNodeMapHandle hNodeMap = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));
	spinNodeHandle hWidth = NULL;
	int64_t camWidth = 0;
	spinNodeHandle hHeight = NULL;
	int64_t camHeight = 0;

	EXEANDCHECK(spinNodeMapGetNode(hNodeMap, "Width", &hWidth));

	// Retrieve maximum width
	if (IsAvailableAndReadable(hWidth, "Width"))
	{
		EXEANDCHECK(spinIntegerGetValue(hWidth, &camWidth));
		vinfo.width = camWidth;
	}

	if (IsAvailableAndReadable(hHeight, "Height"))
	{
		EXEANDCHECK(spinIntegerGetValue(hHeight, &camHeight));
		vinfo.height = camHeight;
	}

	vinfo.fps_n = 0; //0 means variable FPS
	vinfo.fps_d = 1;
	vinfo.interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
	vinfo.finfo = gst_video_format_get_info(DEFAULT_GST_VIDEO_FORMAT);
  
	caps = gst_video_info_to_caps(&vinfo);

	GST_DEBUG_OBJECT (src, "The caps are %" GST_PTR_FORMAT, caps);

	return caps;

fail:
	return 0;
}

static gboolean
gst_spinnaker_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (bsrc);
	GstVideoInfo vinfo;
	gst_video_info_init(&vinfo);

	vinfo.width = src->nWidth;
	vinfo.height = src->nHeight;
	spinImage hCamera = NULL;
	spinNodeMapHandle hNodeMap = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));
	spinNodeHandle hWidth = NULL;
	int64_t camWidth = 0;
	spinNodeHandle hHeight = NULL;
	int64_t camHeight = 0;

	EXEANDCHECK(spinNodeMapGetNode(hNodeMap, "Width", &hWidth));
	if (IsAvailableAndReadable(hWidth, "Width"))
	{
		EXEANDCHECK(spinIntegerGetValue(hWidth, &camWidth));
		vinfo.width = camWidth;
		GST_DEBUG_OBJECT(src, "The width being set is %" GST_PTR_FORMAT, vinfo.width);
	}

	if (IsAvailableAndReadable(hHeight, "Height"))
	{
		EXEANDCHECK(spinIntegerGetValue(hHeight, &camHeight));
		vinfo.height = camHeight;
		GST_DEBUG_OBJECT(src, "The width being set is %" GST_PTR_FORMAT, vinfo.height);
	}
	
	vinfo.fps_n = 0; //0 means variable FPS
	vinfo.fps_d = 1;
	vinfo.interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
	vinfo.finfo = gst_video_format_get_info(DEFAULT_GST_VIDEO_FORMAT);

	caps = gst_video_info_to_caps(&vinfo);

	//Currently using fixed caps
	GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);
	src->acq_started = TRUE;

	return TRUE;

	unsupported_caps:
	GST_ERROR_OBJECT (src, "Unsupported caps: %" GST_PTR_FORMAT, caps);
	return FALSE;

	fail:
	return FALSE;
}

//Grabs next image from camera and puts it into a gstreamer buffer
#ifdef OVERRIDE_CREATE
static GstFlowReturn
gst_spinnaker_src_create (GstPushSrc * psrc, GstBuffer ** buf)
{
	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (psrc);
	GstMapInfo minfo;

	//query camera and grab next image
	spinImage hResultImage = NULL;
	spinImage hCamera = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNextImage(hCamera, &hResultImage));
	EXEANDCHECK(spinCameraRelease(hCamera));

	bool8_t isIncomplete = False;
	bool8_t hasFailed = False;

	//check if image is complete 
	//WARNING: This returns a boolean and is not handled if the image is incomplete
	EXEANDCHECK(spinImageIsIncomplete(hResultImage, &isIncomplete));
	int64_t timestamp = 0;
	EXEANDCHECK(spinImageChunkDataGetIntValue(hResultImage, "ChunkTimestamp", &timestamp));
	// Create a new buffer for the image
	*buf = gst_buffer_new_and_alloc ((double)src->nHeight * (double)src->nWidth * (double)src->nBytesPerPixel);

	gst_buffer_map (*buf, &minfo, GST_MAP_WRITE);

	size_t imageSize;
	EXEANDCHECK(spinImageGetBufferSize(hResultImage, &imageSize));

	//grab pointer to image data	
	void *data;
	EXEANDCHECK(spinImageGetData(hResultImage, &data)); 

	//copy image data into gstreamer buffer
	for (int64_t i = 0; i < src->nHeight; i++) {
		memcpy (minfo.data + i * src->gst_stride, (void*)((int)data + i * src->nPitch), src->nPitch);
	}

	//release image and buffer
	EXEANDCHECK(spinImageRelease(hResultImage));
	gst_buffer_unmap (*buf, &minfo);

    //src->duration = 1000000000.0/src->framerate; 
	src->duration = timestamp - src->last_frame_time;
	src->last_frame_time = timestamp;
	src->stream_time += src->duration;
	// If we do not use gst_base_src_set_do_timestamp() we need to add timestamps manually
	//src->last_frame_time += src->duration;   // Get the timestamp for this frame
	if(!gst_base_src_get_do_timestamp(GST_BASE_SRC(psrc))){
		GST_BUFFER_PTS(*buf) = src->stream_time;  // convert ms to ns
		GST_BUFFER_DTS(*buf) = src->stream_time;  // convert ms to ns
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
  return gst_element_register (plugin, "spinnakersrc", GST_RANK_NONE,
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
#define PACKAGE_NAME "spinnakersrc"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "github.com/thompd27"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    spinnaker,
    "plugin for interfacing with FLIR machine vision cameras based on the spinnaker API",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN);
