# gst-plugin-spinnaker
Gstreamer plugin for Point Grey/FLIR cameras using the Spinnaker API. Based off the gst-plugin-flycap (https://github.com/atdgroup/gst-plugin-flycap ).

Build like any other gstreamer plugin with Meson, by calling `meson build` from the main directory.

## Features

There are a few properties from the SDK implemented. If you require a specific feature to be added, you can use one of the existing props as an example and submit a pull request. These properties can also be found by running `gst-inspect-1.0`.

### Image Format
There are currently only 3 supported pixel formats: GRAY8 (which is actually BayerRG), RGB, and BGRx. Setting the format to GRAY8 will disable the onboard ISP, allowing some camera models to achieve the full advertised framerate. RGB and BGRx will both use the ISP. See the technical documentation for your camera model to see the max achievable framerate in these modes. 

`gst-launch-1.0 spinnakersrc ! "video/x-raw, width=1920, height=1080, format=GRAY8" ! videoconvert ! autovideosink -e` 

### Framerate
Framerate is set with the pipeline.

`gst-launch-1.0 spinnakersrc ! "video/x-raw, width=1920, height=1080, fps=30/1" ! videoconvert ! autovideosink -e`

### Resolution
Resolution is set with the pipeline.

`gst-launch-1.0 spinnakersrc ! "video/x-raw, width=1920, height=1080" ! videoconvert ! autovideosink -e`

### Exposure Limits
Set the minimum and maximum exposure limits with the `exposure-lower` and `exposure-upper` with the units in microseconds. 

`gst-launch-1.0 spinnakersrc exposure-lower=50 exposure-upper=15000 ! "video/x-raw, width=1920, height=1080" ! videoconvert ! autovideosink -e`

### Shutter Type
Can set the shutter type between rolling (0), global reset (1), or global shutter (2) mode. This is an enum so use the corresponding value. Defaults to rolling.

`gst-launch-1.0 spinnakersrc shutter=0 ! "video/x-raw, width=1920, height=1080" ! videoconvert ! autovideosink -e`

### Offsets
Sets the X and Y offsets of the camera frame if you aren't using the full sensor with `offset-x` and `offset-y` in pixels.

`gst-launch-1.0 spinnakersrc offset-x=10 offset-y=100 ! "video/x-raw, width=1920, height=1080" ! videoconvert ! autovideosink -e`
