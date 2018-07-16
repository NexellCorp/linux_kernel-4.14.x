ifeq ($(CONFIG_OF_OVERLAY),y)

dtbo-$(CONFIG_ARCH_NEXELL) += \
	sip-s31nx-artik310s-explorer-spi.dtbo

targets	+= dtbs dtbs_install
targets	+= $(dtb-y)

endif

always		:= $(dtbo-y)
clean-files	:= *.dtbo
