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
static gboolean gst_spinnaker_src_negotiate(GstBaseSrc* src);

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
	PROP_EXPOSURE_LOWER,
	PROP_EXPOSURE_UPPER,
	PROP_SHUTTER,
	PROP_OFFSET_X,
	PROP_OFFSET_Y
};

#define	FLYCAP_UPDATE_LOCAL  FALSE
#define	FLYCAP_UPDATE_CAMERA TRUE

#define DEFAULT_PROP_CAMERA	            0
#define DEFAULT_PROP_EXPOSURE_LOWER     100
#define DEFAULT_PROP_EXPOSURE_UPPER     15000
#define DEFAULT_PROP_SHUTTER			"Rolling"
#define DEFAULT_PROP_WIDTH 				640
#define DEFAULT_PROP_HEIGHT			    480
#define DEFAULT_PROP_FPS				30
#define DEFAULT_PROP_OFFSET_X			0
#define DEFAULT_PROP_OFFSET_Y			0


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
#define DEFAULT_PROP_GAMMA			    1.5


#define DEFAULT_GST_VIDEO_FORMAT GST_VIDEO_FORMAT_GRAY8
#define DEFAULT_SPIN_VIDEO_FORMAT SPIN
// Put matching type text in the pad template below

// pad template
static GstStaticPadTemplate gst_spinnaker_src_template =
		GST_STATIC_PAD_TEMPLATE ("src",
				GST_PAD_SRC,
				GST_PAD_ALWAYS,
				GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
						("{ GRAY8, RGB, BGRx }"))
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

// This function helps to check if a node is available and writable
bool8_t IsAvailableAndWritable(spinNodeHandle hNode, char nodeName[])
{
    bool8_t pbAvailable = False;
    spinError err = SPINNAKER_ERR_SUCCESS;
    err = spinNodeIsAvailable(hNode, &pbAvailable);
    if (err != SPINNAKER_ERR_SUCCESS)
    {
        printf("Unable to retrieve node availability (%s node), with error %d...\n\n", nodeName, err);
    }

    bool8_t pbWritable = False;
    err = spinNodeIsWritable(hNode, &pbWritable);
    if (err != SPINNAKER_ERR_SUCCESS)
    {
        printf("Unable to retrieve node writability (%s node), with error %d...\n\n", nodeName, err);
    }
    return pbWritable && pbAvailable;
}


#define TYPE_SHUTTER (shutter_get_type ())
static GType
shutter_get_type(void)
{
	static GType shutter_type = 0;

	if (!shutter_type) {
		static GEnumValue shutter_types[] = {
				  { GST_ROLLING, "Use rolling shutter.",    "0" },
				  { GST_GLOBAL_RESET, "Use rolling shutter with global reset.",    "1" },
				  { GST_GLOBAL, "Use global shutter.",    "2" },
				  { 0, NULL, NULL },
		};

		shutter_type =
			g_enum_register_static("ShutterType", shutter_types);
	}

	return shutter_type;
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
	//gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_spinnaker_src_get_caps);
	gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_spinnaker_src_set_caps);
	gstbasesrc_class->negotiate = GST_DEBUG_FUNCPTR(gst_spinnaker_src_negotiate);

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

	//exposure lower limit property
	g_object_class_install_property(gobject_class, PROP_EXPOSURE_LOWER,
		g_param_spec_int("exposure-lower", "Exposure Lower Limit", "Sets the minimum exposure time for the auto exposure algoritm (us)", 10, 29999999, DEFAULT_PROP_EXPOSURE_LOWER,
			(GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

	//exposure upper limit property
	g_object_class_install_property(gobject_class, PROP_EXPOSURE_UPPER,
		g_param_spec_int("exposure-upper", "Exposure Upper Limit", "Sets the maximum exposure time for the auto exposure algoritm (us)", 10, 29999999, DEFAULT_PROP_EXPOSURE_UPPER,
			(GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

	//shutter type property
	g_object_class_install_property(gobject_class, PROP_SHUTTER,
		g_param_spec_enum("shutter", "Shutter type", "Sets the shutter to use (Rolling, GlobalReset, Global)", TYPE_SHUTTER, GST_ROLLING,
			(GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

	//horizontal offset property
	g_object_class_install_property(gobject_class, PROP_OFFSET_X,
		g_param_spec_int("offset-x", "Offset X", "Sets the horizontal offset to use when using a lower resolution", 0, 10000, DEFAULT_PROP_OFFSET_X,
			(GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));

	//vertical offset property
	g_object_class_install_property(gobject_class, PROP_OFFSET_Y,
		g_param_spec_int("offset-y", "Offset Y", "Sets the vertical offset to use when using a lower resolution", 0, 10000, DEFAULT_PROP_OFFSET_Y,
			(GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING)));
}

static void
init_properties(GstSpinnakerSrc * src)
{
	GST_DEBUG_OBJECT(src, "setting initial properties");
    //Hardcoded hot-garbage **************************
  src->nWidth = DEFAULT_PROP_WIDTH;
  src->nHeight = DEFAULT_PROP_HEIGHT;
  src->nBytesPerPixel = 1;
  src->binning = 1;
  src->n_frames = 0;
  src->framerate = 30;
  src->last_frame_time = 0;
  src->nPitch = src->nWidth * src->nBytesPerPixel;
  src->gst_stride = src->nPitch;
  src->cameraID = DEFAULT_PROP_CAMERA;
  src->exposure_lower = DEFAULT_PROP_EXPOSURE_LOWER;
  src->exposure_upper = DEFAULT_PROP_EXPOSURE_UPPER;
  src->shutter = DEFAULT_PROP_SHUTTER;

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
	src->exposure_lower_set = FALSE;
	src->exposure_upper_set = FALSE;
}

void
gst_spinnaker_src_set_property (GObject * object, guint property_id,
		const GValue * value, GParamSpec * pspec)
{
	
	GstSpinnakerSrc *src;
	
	src = GST_SPINNAKER_SRC (object);
	GST_DEBUG_OBJECT(src, "setting properties");
	spinNodeHandle hWidth = NULL;
	int64_t maxWidth = 0;
	spinNodeHandle hHeight = NULL;
	int64_t maxHeight = 0;
	switch(property_id) {
	case PROP_CAMERA:
		src->cameraID = g_value_get_int (value);
		GST_DEBUG_OBJECT (src, "camera id: %d", src->cameraID);
		break;
	case PROP_EXPOSURE_LOWER:
		src->exposure_lower = g_value_get_int(value);
		src->exposure_lower_set = TRUE;
		break;
	case PROP_EXPOSURE_UPPER:
		src->exposure_upper = g_value_get_int(value);
		src->exposure_upper_set = TRUE;
		break;

	case PROP_SHUTTER:
		src->shutter_set = TRUE;
		switch (g_value_get_enum(value)) {
		case GST_ROLLING:
			src->shutter = "Rolling";
			break;
		case GST_GLOBAL_RESET:
			src->shutter = "GlobalReset";
			break;
		case GST_GLOBAL:
			src->shutter = "Global";
			break;
		default:
			src->shutter = "Rolling";
			break;
		}
		break;
	case PROP_OFFSET_X:
		src->offset_x_set = TRUE;
		src->nOffsetX = g_value_get_int(value);
		break;
	case PROP_OFFSET_Y:
		src->offset_y_set = TRUE;
		src->nOffsetY = g_value_get_int(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}

	fail:
	return;
}

void
gst_spinnaker_get_node_int(GstBaseSrc* bsrc, const char* nodeName, int64_t* val) {
	GstSpinnakerSrc* src = GST_SPINNAKER_SRC(bsrc);
	spinNodeHandle hNode = NULL;
	spinCamera hCamera = NULL;

	spinNodeMapHandle hNodeMap = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));
	// Retrieve node

	EXEANDCHECK(spinNodeMapGetNode(hNodeMap, nodeName, &hNode));

	if (IsAvailableAndWritable(hNode, nodeName))
	{
		EXEANDCHECK(spinIntegerGetValue(hNode, val));
	}
	else {
		GST_ERROR_OBJECT(src, "Cannot get value");
	}

	EXEANDCHECK(spinCameraRelease(hCamera));
	return;
fail:
	GST_DEBUG_OBJECT(src, "Failed to set node");
	EXEANDCHECK(spinCameraRelease(hCamera));
	return;
}

void
gst_spinnaker_set_node_int(GstBaseSrc* bsrc, const char* nodeName, int64_t* val) {
	GstSpinnakerSrc* src = GST_SPINNAKER_SRC(bsrc);
	spinNodeHandle hNode = NULL;
	spinCamera hCamera = NULL;
	int64_t maxVal = 0;
	int64_t minVal = 0;
	//spinNodeMapHandle hNodeMap = NULL;
	//val = 1000;
	GST_DEBUG_OBJECT(src, "selecting camera %d", src->cameraID);
	spinNodeMapHandle hNodeMap = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));
	// Retrieve node
	
	EXEANDCHECK(spinNodeMapGetNode(hNodeMap, nodeName, &hNode));
	// Retrieve min and max values
	
	if (IsAvailableAndWritable(hNode, nodeName))
	{
		EXEANDCHECK(spinIntegerGetMax(hNode, &maxVal));
		EXEANDCHECK(spinIntegerGetMin(hNode, &minVal));
	}
	else {
		GST_ERROR_OBJECT(src, "Cannot get min/max values");
	}

	if (val > maxVal) {
		GST_ERROR_OBJECT(src, "Unsupported value, desired value exceeds node maximum: %d, %d", val, maxVal);
		return; //make sure we don't exceed max val
	}

	if (val < minVal) {
		GST_ERROR_OBJECT(src, "Unsupported value, desired value below node minimum: %d, %d", val, minVal);
		return; //make sure we don't exceed max val
	}

	EXEANDCHECK(spinIntegerSetValue(hNode, val));
	EXEANDCHECK(spinCameraRelease(hCamera));
	return;
fail:
	GST_DEBUG_OBJECT(src, "Failed to set node");
	EXEANDCHECK(spinCameraRelease(hCamera));
	return;
}

void
gst_spinnaker_get_node_float(GstBaseSrc* bsrc, const char* nodeName, double* val) {
	GstSpinnakerSrc* src = GST_SPINNAKER_SRC(bsrc);
	spinNodeHandle hNode = NULL;
	spinCamera hCamera = NULL;

	GST_DEBUG_OBJECT(src, "selecting camera %d", src->cameraID);
	spinNodeMapHandle hNodeMap = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));
	// Retrieve node

	EXEANDCHECK(spinNodeMapGetNode(hNodeMap, nodeName, &hNode));

	//retrieve current value
	if (IsAvailableAndWritable(hNode, nodeName))
	{
		EXEANDCHECK(spinFloatGetValue(hNode, val));
	}
	else {
		GST_ERROR_OBJECT(src, "Cannot get value");
	}

	EXEANDCHECK(spinCameraRelease(hCamera));
	return;
fail:
	GST_DEBUG_OBJECT(src, "Failed to set node");
	EXEANDCHECK(spinCameraRelease(hCamera));
	return;
}

void
gst_spinnaker_set_node_float(GstBaseSrc* bsrc, const char* nodeName, double val) {
	GstSpinnakerSrc* src = GST_SPINNAKER_SRC(bsrc);
	spinNodeHandle hNode = NULL;
	spinCamera hCamera = NULL;
	double maxVal = 0;
	double minVal = 0;
	//spinNodeMapHandle hNodeMap = NULL;
	//val = 1000;
	GST_DEBUG_OBJECT(src, "selecting camera %d", src->cameraID);
	spinNodeMapHandle hNodeMap = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));
	// Retrieve node

	EXEANDCHECK(spinNodeMapGetNode(hNodeMap, nodeName, &hNode));
	// Retrieve min and max values

	if (IsAvailableAndWritable(hNode, nodeName))
	{
		EXEANDCHECK(spinFloatGetMax(hNode, &maxVal));
		EXEANDCHECK(spinFloatGetMin(hNode, &minVal));
	}
	else {
		GST_ERROR_OBJECT(src, "Cannot get min/max values");
	}

	if (val > maxVal) {
		GST_ERROR_OBJECT(src, "Unsupported value, desired value exceeds node maximum: %d, %d", val, maxVal);
		return; //make sure we don't exceed max val
	}

	if (val < minVal) {
		GST_ERROR_OBJECT(src, "Unsupported value, desired value below node minimum: %d, %d", val, minVal);
		return; //make sure we don't exceed max val
	}

	EXEANDCHECK(spinFloatSetValue(hNode, val));
	EXEANDCHECK(spinCameraRelease(hCamera));
	return;
fail:
	GST_DEBUG_OBJECT(src, "Failed to set node");
	EXEANDCHECK(spinCameraRelease(hCamera));
	return;
}

void
gst_spinnaker_set_node_boolean(GstBaseSrc* bsrc, const char* nodeName, bool8_t* val) {
	GstSpinnakerSrc* src = GST_SPINNAKER_SRC(bsrc);
	spinNodeHandle hNode = NULL;
	spinCamera hCamera = NULL;

	spinNodeMapHandle hNodeMap = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));
	// Retrieve node

	EXEANDCHECK(spinNodeMapGetNode(hNodeMap, nodeName, &hNode));

	if (IsAvailableAndWritable(hNode, nodeName))
	{
		EXEANDCHECK(spinBooleanSetValue(hNode, val));
	}
	else
	{
		GST_ERROR_OBJECT(src, "boolean node not available");
	}
	EXEANDCHECK(spinCameraRelease(hCamera));
	return;
fail:
	GST_DEBUG_OBJECT(src, "Failed to set node");
	EXEANDCHECK(spinCameraRelease(hCamera));
	return;
}

void
gst_spinnaker_set_node_enum(GstBaseSrc* bsrc, const char* nodeName, const char* val) {
	GstSpinnakerSrc* src = GST_SPINNAKER_SRC(bsrc);
	spinNodeHandle hNode = NULL;
	spinCamera hCamera = NULL;
	spinNodeHandle hEnum = NULL;
	spinNodeHandle hDesiredEnum = NULL;
	int64_t enumInt = 0;

	spinNodeMapHandle hNodeMap = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));

	EXEANDCHECK(spinNodeMapGetNode(hNodeMap, nodeName, &hEnum));

	if (IsAvailableAndReadable(hEnum, nodeName))
	{
		EXEANDCHECK(spinEnumerationGetEntryByName(hEnum, val, &hDesiredEnum));
	}
	else
	{
		GST_ERROR_OBJECT(src, "failed to get enum handle");
	}

	char* name_with_extension;
	name_with_extension = malloc(strlen(nodeName) + strlen(val)); /* make space for the new string (should check the return value ...) */
	strcpy(name_with_extension, nodeName); /* copy name into the new var */
	strcat(name_with_extension, val); /* add the extension */
	// Retrieve integer value from entry node
	if (IsAvailableAndReadable(hDesiredEnum, name_with_extension))
	{
		EXEANDCHECK(spinEnumerationEntryGetIntValue(hDesiredEnum, &enumInt));
	}
	else
	{
		GST_ERROR_OBJECT(src, "failed to get enum int value");
	}

	free(name_with_extension);
	// Set integer as new value for enumeration node
	if (IsAvailableAndWritable(hEnum, val))
	{
		EXEANDCHECK(spinEnumerationSetIntValue(hEnum, enumInt));
	}
	else
	{
		GST_ERROR_OBJECT(src, "failed to set enum");
	}
	EXEANDCHECK(spinCameraRelease(hCamera));

	return;
fail:
	GST_ERROR_OBJECT(src, "failed to set enum");
	spinCameraRelease(hCamera);
	return;
}

void
gst_spinnaker_apply_property(GstBaseSrc* bsrc) {
	GstSpinnakerSrc* src = GST_SPINNAKER_SRC(bsrc);

	return;
fail:
	return;
	//EXEANDCHECK(spinCameraRelease(hCamera));
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
		GST_ERROR_OBJECT(src, "No camera found.");
		
    // Clear and destroy camera list before releasing system
    EXEANDCHECK(spinCameraListClear(src->hCameraList));

    EXEANDCHECK(spinCameraListDestroy(src->hCameraList));

    // Release system
    EXEANDCHECK(spinSystemReleaseInstance(src->hSystem));

		GST_ERROR_OBJECT(src, "No device found.");
		goto fail;
	}
	/*
    // Select camera
  	spinCamera hCamera = NULL;
	GST_DEBUG_OBJECT (src, "selecting camera");
    EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	GST_DEBUG_OBJECT (src, "initializing camera");
    EXEANDCHECK(spinCameraInit(hCamera));
	EXEANDCHECK(spinCameraRelease(hCamera));
	*/
	//starts camera acquisition. Doesn't actually fill the gstreamer buffer. see create function
	/*
	GST_DEBUG_OBJECT (src, "starting acquisition");
    EXEANDCHECK(spinCameraBeginAcquisition(hCamera));
	EXEANDCHECK(spinCameraRelease(hCamera));

	src->cameraPresent = TRUE;
	*/
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

static gboolean
gst_spinnaker_src_negotiate(GstBaseSrc* bsrc) {
	GstSpinnakerSrc* src = GST_SPINNAKER_SRC(bsrc);
	GST_DEBUG_OBJECT(src, "negotiating caps");

	gboolean negotiate = FALSE;
	GstCaps* caps;
	GstVideoInfo vinfo;
	GstCaps* peercaps = NULL;
	GstStructure* pref = NULL;
	peercaps = gst_pad_peer_query_caps(GST_BASE_SRC_PAD(src), NULL);
	GST_DEBUG_OBJECT(src, "caps of peer: %" GST_PTR_FORMAT, peercaps);
	pref = gst_caps_get_structure(peercaps, 0);

	// Select camera
	spinCamera hCamera = NULL;
	GST_DEBUG_OBJECT(src, "selecting camera");
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	bool8_t streaming = FALSE;
	spinCameraIsStreaming(hCamera, &streaming);
	//if (streaming) {
		GST_DEBUG_OBJECT(src, "Already streaming, ending acquisition....");
		spinCameraEndAcquisition(hCamera);
		spinCameraDeInit(hCamera);
	//}
	GST_DEBUG_OBJECT(src, "initializing camera");
	EXEANDCHECK(spinCameraInit(hCamera));
	
	// Retrieve GenICam nodemap
	spinNodeMapHandle hNodeMap = NULL;

	GST_DEBUG_OBJECT(src, "getting node map");
	EXEANDCHECK(spinCameraGetNodeMap(hCamera, &hNodeMap));
	EXEANDCHECK(spinCameraRelease(hCamera));
	GST_DEBUG_OBJECT(src, "got node map");
	gst_video_info_init(&vinfo);

	vinfo.fps_n = 0; //0 means variable FPS
	vinfo.fps_d = 1;
	vinfo.interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
	
	
	if (src->shutter_set) {
		gst_spinnaker_set_node_enum(bsrc, "SensorShutterMode", src->shutter);
	}

	//char* pref_format;
	GstVideoFormat pref_format_enum;
	if (gst_structure_has_field(pref, "format")) {
		//gst_structure_get_enum(pref, "format", G_GUINT64_FORMAT, &pref_format);
		
		const char* pref_format = gst_structure_get_string(pref, "format");
		pref_format_enum = gst_video_format_from_string(pref_format);
		
	}
	

	switch (pref_format_enum)
	{
	case 25: //GRAY8 but actually bayer
		GST_DEBUG_OBJECT(src, "format is: %d, turning off ISP", pref_format_enum);
		src->ISP = FALSE;
		src->nBytesPerPixel = 1;
		gst_spinnaker_set_node_enum(bsrc, "PixelFormat", "BayerRG8");
		gst_spinnaker_set_node_boolean(bsrc, "IspEnable", FALSE);
		vinfo.finfo = gst_video_format_get_info(GST_VIDEO_FORMAT_GRAY8);
		break;
	case 8: //BGRx
		GST_DEBUG_OBJECT(src, "format is: %d, turning on ISP", pref_format_enum);
		src->ISP = TRUE;
		src->nBytesPerPixel = 4;
		gst_spinnaker_set_node_enum(bsrc, "PixelFormat", "BGRa8");
		//gst_spinnaker_set_node_boolean(bsrc, "IspEnable", TRUE);
		vinfo.finfo = gst_video_format_get_info(GST_VIDEO_FORMAT_BGRx);
		break;
	case 15: //RGB
		GST_DEBUG_OBJECT(src, "format is: %d, turning on ISP", pref_format_enum);
		src->ISP = TRUE;
		src->nBytesPerPixel = 3;
		gst_spinnaker_set_node_enum(bsrc, "PixelFormat", "RGB8Packed");
		//gst_spinnaker_set_node_boolean(bsrc, "IspEnable", TRUE);
		vinfo.finfo = gst_video_format_get_info(GST_VIDEO_FORMAT_RGB);
		break;
	default:
		GST_ERROR_OBJECT(src, "video format unknown: %d", pref_format_enum);
		vinfo.finfo = gst_video_format_get_info(DEFAULT_GST_VIDEO_FORMAT);
	}

	spinNodeHandle hHeight = NULL;
	spinNodeHandle hWidth = NULL;
	int64_t maxHeight = 0;
	// Retrieve Height node
	gint pref_height = 0;
	//GstVideoFormat
	if (gst_structure_has_field(pref, "height")) {
		gst_structure_get_int(pref, "height", &pref_height);
	}

	if (pref_height > 0) {
		gst_spinnaker_set_node_int(bsrc, "Height", (int64_t)pref_height);
	}
	else {
		int64_t spinHeight = 0;
		gst_spinnaker_get_node_int(bsrc, "Height", &spinHeight);
		pref_height = (gint)spinHeight;
	}
	src->nHeight = pref_height;
	gint pref_width = 0;
	if (gst_structure_has_field(pref, "width")) {
		gst_structure_get_int(pref, "width", &pref_width);
	
	}

	if (pref_width > 0) {
		gst_spinnaker_set_node_int(bsrc, "Width", (int64_t)pref_width);
	}
	else {
		int64_t spinWidth = 0;
		gst_spinnaker_get_node_int(bsrc, "Width", &spinWidth);
		pref_width = (gint)spinWidth;
	}
	src->nWidth = pref_width;
	src->nPitch = pref_width * src->nBytesPerPixel;
	src->gst_stride = pref_width * src->nBytesPerPixel;


	vinfo.width = pref_width;
	vinfo.height = pref_height;
	gint fps_n = 0;
	gint fps_d = 0;
	if (gst_structure_has_field(pref, "framerate")) {
		gst_structure_get_fraction(pref, "framerate", &fps_n, &fps_d);
	}

	if (fps_n > 0) {
		gst_spinnaker_set_node_float(bsrc, "AcquisitionFrameRate", (double)fps_n / (double)fps_d);
	}
	else {
		double spinFps = 3.0;
		gst_spinnaker_get_node_float(bsrc, "AcquisitionFrameRate", &spinFps);
		fps_n = (gint)spinFps;
		
	}

	src->framerate = fps_n;
	vinfo.fps_n = fps_n;
	vinfo.fps_d = 1;
	if (src->exposure_lower_set) {
		gst_spinnaker_set_node_float(bsrc, "AutoExposureExposureTimeLowerLimit", (double)src->exposure_lower);
	}

	if (src->exposure_upper_set) {
		gst_spinnaker_set_node_float(bsrc, "AutoExposureExposureTimeUpperLimit", (double)src->exposure_upper);
	}

	if (src->offset_x_set) {
		gst_spinnaker_set_node_int(bsrc, "OffsetX", (int64_t)src->nOffsetX);
	}

	if (src->offset_y_set) {
		gst_spinnaker_set_node_int(bsrc, "OffsetY", (int64_t)src->nOffsetY);
	}

	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	caps = gst_video_info_to_caps(&vinfo);
	GST_DEBUG_OBJECT(src, "The caps are %" GST_PTR_FORMAT, caps);
	gst_spinnaker_src_set_caps(bsrc, caps);
	//gst_spinnaker_apply_property(bsrc);

	GST_DEBUG_OBJECT(src, "starting acquisition");
	EXEANDCHECK(spinCameraBeginAcquisition(hCamera));
	EXEANDCHECK(spinCameraRelease(hCamera));

	return TRUE;

fail:
	spinCameraRelease(hCamera);
	return FALSE;
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
	vinfo.fps_n = 0; //0 means variable FPS
	vinfo.fps_d = 1;
	vinfo.interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
	vinfo.finfo = gst_video_format_get_info(DEFAULT_GST_VIDEO_FORMAT);
  
	caps = gst_video_info_to_caps(&vinfo);

	GST_DEBUG_OBJECT (src, "The caps are %" GST_PTR_FORMAT, caps);

	return caps;
}

static gboolean
gst_spinnaker_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
	GstSpinnakerSrc *src = GST_SPINNAKER_SRC (bsrc);
	GstVideoInfo vinfo;

	//Currently using fixed caps
	GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);
	//gst_caps_fixate(bsrc->srcpad, caps);
	if (!gst_pad_set_caps(bsrc->srcpad, caps))
		return FALSE;
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
	//GST_DEBUG_OBJECT(src, "entered create");
	GstMapInfo minfo;

	//query camera and grab next image
	spinImage hResultImage = NULL;
	spinCamera hCamera = NULL;
	EXEANDCHECK(spinCameraListGet(src->hCameraList, src->cameraID, &hCamera));
	EXEANDCHECK(spinCameraGetNextImage(hCamera, &hResultImage));
	EXEANDCHECK(spinCameraRelease(hCamera));
	//GST_DEBUG_OBJECT(src, "retrieved image");
	bool8_t isIncomplete = False;
	bool8_t hasFailed = False;

	//check if image is complete 
	//WARNING: This returns a boolean and is not handled if the image is incomplete
	EXEANDCHECK(spinImageIsIncomplete(hResultImage, &isIncomplete));

	// Print image information
	size_t width = 0;
	size_t height = 0;

	EXEANDCHECK(spinImageGetWidth(hResultImage, &width));

	EXEANDCHECK(spinImageGetHeight(hResultImage, &height));

	//GST_DEBUG_OBJECT(src, "height is: %d", height);
	if (width != src->nWidth) {
		GST_ERROR_OBJECT(src, "Width doesn't match: %d, %d" , width, src->nWidth);
		return GST_FLOW_ERROR;
	}
	if (height != src->nHeight) {
		GST_ERROR_OBJECT(src, "Height doesn't match: %d, %d", height, src->nHeight);
		return GST_FLOW_ERROR;
	}
	// Create a new buffer for the image
	*buf = gst_buffer_new_and_alloc (src->nHeight * src->nWidth * src->nBytesPerPixel);

	gst_buffer_map (*buf, &minfo, GST_MAP_WRITE);

	/*
	size_t imageSize;
	EXEANDCHECK(spinImageGetBufferSize(hResultImage, &imageSize));
	*/
	//grab pointer to image data	
	void *data;
	EXEANDCHECK(spinImageGetData(hResultImage, &data)); 
	//GST_DEBUG_OBJECT(src, "starting image copy");
	//copy image data into gstreamer buffer

	for (int i = 0; i < src->nHeight; i++) {
		memcpy (minfo.data + i * src->gst_stride, ((char*)data) + i * src->nPitch, src->nPitch);
	}
	//release image and buffer
	EXEANDCHECK(spinImageRelease(hResultImage));
	gst_buffer_unmap (*buf, &minfo);

    src->duration = 1000000000.0/src->framerate; 
	// If we do not use gst_base_src_set_do_timestamp() we need to add timestamps manually
	src->last_frame_time += src->duration;   // Get the timestamp for this frame
	if(!gst_base_src_get_do_timestamp(GST_BASE_SRC(psrc))){
		GST_BUFFER_PTS(*buf) = src->last_frame_time;  // convert ms to ns
		GST_BUFFER_DTS(*buf) = src->last_frame_time;  // convert ms to ns
	}
	GST_BUFFER_DURATION(*buf) = src->duration;
	//GST_DEBUG_OBJECT(src, "pts, dts: %" GST_TIME_FORMAT ", duration: %d ms", GST_TIME_ARGS (src->last_frame_time), GST_TIME_AS_MSECONDS(src->duration));

	// count frames, and send EOS when required frame number is reached
	GST_BUFFER_OFFSET(*buf) = src->n_frames;  // from videotestsrc
	src->n_frames++;
	GST_BUFFER_OFFSET_END(*buf) = src->n_frames;  // from videotestsrc
	if (psrc->parent.num_buffers>0)  // If we were asked for a specific number of buffers, stop when complete
		if (G_UNLIKELY(src->n_frames >= psrc->parent.num_buffers))
			return GST_FLOW_EOS;
	//GST_DEBUG_OBJECT(src, "image copied");
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
