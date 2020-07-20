# gst-plugin-spinnaker
Gstreamer plugin for Point Grey/FLIR cameras using the Spinnaker API. Based off the gst-plugin-flycap (https://github.com/atdgroup/gst-plugin-flycap ).

Tested on Windows, Ubuntu, and Embedded Jetson TX1/TX2/Xavier boards. In its current form, this plugin is built for a specific camera, and expects the camera ISP to be disabled, yielding raw bayer images. Future updates will allow gstreamer to configure the camera settings.
