# SPDX-License-Identifier: GPL-2.0
# Nexell Platform Support

nexell_drm-y := nexell_crtc.o nexell_plane.o nexell_connector.o \
		nexell_encoder.o nexell_fb.o nexell_gem.o nexell_drv.o

nexell_drm-$(CONFIG_DRM_NEXELL_RGB)	+= nexell_lcd.o
obj-$(CONFIG_DRM_NEXELL)		+= soc/
obj-$(CONFIG_DRM_NEXELL)		+= nexell_drm.o
