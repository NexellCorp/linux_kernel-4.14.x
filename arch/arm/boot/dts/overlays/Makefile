ifeq ($(CONFIG_OF_OVERLAY),y)

dtbo-$(CONFIG_ARCH_NEXELL) += \
	sip-s31nx-artik310s-explorer-spi.dtbo \
	sip-s31nx-artik310s-explorer-rgb.dtbo \
	sip-s31nx-artik310s-explorer-tw9900.dtbo \
	sip-s31nx-artik310s-trike-rgb.dtbo

targets	+= dtbs dtbs_install
targets	+= $(dtb-y)

endif

always		:= $(dtbo-y)
clean-files	:= *.dtbo
