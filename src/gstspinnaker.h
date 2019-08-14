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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_SPINNAKER_H_
#define _GST_SPINNAKER_H_

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/video/video.h>
#include <SpinnakerC.h>

G_BEGIN_DECLS

#define GST_TYPE_SPINNAKER   (gst_spinnaker_get_type())
#define GST_SPINNAKER(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SPINNAKER,GstSpinnaker))
#define GST_SPINNAKER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SPINNAKER,GstSpinnakerClass))
#define GST_IS_SPINNAKER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SPINNAKER))
#define GST_IS_SPINNAKER_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SPINNAKER))

typedef struct _GstSpinnaker GstSpinnaker;
typedef struct _GstSpinnakerClass GstSpinnakerClass;

struct _GstSpinnaker
{
  GstBaseSrc base_spinnaker;
  spinCamera hCamera;
  spinImage convertedImage;

  unsigned int nWidth;
  unsigned int nHeight;
  unsigned int nBitsPerPixel;
  unsigned int nBytesPerPixel;
  unsigned int nPitch;   // Stride in bytes between lines
  unsigned int nImageSize;  // Image size in bytes
  unsigned int binning;

  unsigned int nRawWidth;  // because of binning the raw image size may be smaller than nWidth
  unsigned int nRawHeight;  // because of binning the raw image size may be smaller than nHeight
  unsigned int nRawPitch;  // because of binning the raw image size may be smaller than nHeight

  gfloat framerate;
  gint n_frames;
  gint gst_stride;  // Stride/pitch for the GStreamer buffer
  GstClockTime duration;
  GstClockTime last_frame_time;
};

struct _GstSpinnakerClass
{
  GstBaseSrcClass base_spinnaker_class;
};

GType gst_spinnaker_get_type (void);

G_END_DECLS

#endif
