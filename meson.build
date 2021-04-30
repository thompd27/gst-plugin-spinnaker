project('gst-template', 'c', version : '1.16.2', license : 'LGPL')

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('c')

gst_version = meson.project_version()

api_version = '1.0'

gst_dep = dependency('gstreamer-1.0', version : '>=1.16',
    required : true, fallback : ['gstreamer', 'gst_dep'])

spinnaker_inc = include_directories('/opt/spinnaker/include/spinc')
#spinnaker_lib = static_library(['/opt/spinnaker/lib/libSpinnaker_C.so', '/opt/spinnaker/lib/libSpinVideo_C.so])
spinnaker_dep = declare_dependency(link_args : ['-L/opt/spinnaker/lib', '-lSpinnaker_C', '-lSpinVideo_C'],
  include_directories : spinnaker_inc)
  
plugin_c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-template-plugin')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'GStreamer template Plug-ins')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://gstreamer.freedesktop.org')
configure_file(output : 'config.h', configuration : cdata)

#gstaudio_dep = dependency('gstreamer-audio-1.0',
#   fallback: ['gst-plugins-base', ''])

gstvideo_dep = dependency('gstreamer-video-1.0',
	fallback: ['gst-plugins-base', ''])
	
# Plugin 1
plugin_sources = [
  'src/gstspinnaker.c'
  ]


incdir = include_directories('/opt/spinnaker/include/spinc')

gstspinnakerplugin= library('gstspinnaker',
  plugin_sources,
  c_args: plugin_c_args,
  dependencies : [gst_dep, gstvideo_dep, spinnaker_dep],
  install : true,
  install_dir : plugins_install_dir,
)
