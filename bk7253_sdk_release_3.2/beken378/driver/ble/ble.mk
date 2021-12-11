NAME := ble

$(NAME)_TYPE := kernel

$(NAME)_INCLUDES := ../../app/standalone-ap \
					../../app/standalone-station \
					../../driver/sdio \
					../../driver/uart \
					../../driver/sys_ctrl \
					../../driver/gpio \
					../../driver/common/reg \
					../../driver/dma \
					../../driver/intc \
					../../driver/phy \
					../../driver/rc_beken \
					../../driver/general_dma \
					../../driver/spidma \
					../../driver/rw_pub \
					../../func/sdio_intf \
					../../func/power_save \
					../../func/temp_detect \
					../../func/saradc_intf \
					../../func/spidma_intf \
					../../func/ethernet_intf \
					../../func/rwnx_intf \
					../../func/rf_test \
					../../func/joint_up

$(NAME)_INCLUDES += ../../ip/ke \
					../../ip/mac \
					../../ip/lmac/src/hal \
					../../ip/lmac/src/mm \
					../../ip/lmac/src/ps \
					../../ip/lmac/src/rd \
					../../ip/lmac/src/rwnx \
					../../ip/lmac/src/rx \
					../../ip/lmac/src/scan \
					../../ip/lmac/src/sta \
					../../ip/lmac/src/tx \
					../../ip/lmac/src/vif \
					../../ip/lmac/src/rx/rxl \
					../../ip/lmac/src/tx/txl \
					../../ip/lmac/src/tpc \
					../../ip/lmac/src/tdls \
					../../ip/lmac/src/p2p \
					../../ip/lmac/src/chan \
					../../ip/lmac/src/td \
					../../ip/umac/src/bam \
					../../ip/umac/src/llc \
					../../ip/umac/src/me \
					../../ip/umac/src/rxu \
					../../ip/umac/src/scanu \
					../../ip/umac/src/sm \
					../../ip/umac/src/txu \
					../../ip/umac/src/apm \
					../../ip/umac/src/mesh \
					../../ip/umac/src/rc

$(NAME)_INCLUDES += . \
					ble_pub/ip/ble/hl/inc \
					ble_pub/ip/ble/profiles/sdp/api \
					ble_pub/ip/ble/profiles/comm/api \
					ble_pub/modules/rwip/api \
					ble_pub/modules/app/api \
					ble_pub/modules/common/api \
					ble_pub/modules/dbg/api \
					ble_pub/modules/rwip/api \
					ble_pub/modules/rf/api \
					ble_pub/modules/ecc_p256/api \
					ble_pub/plf/refip/src/arch \
					ble_pub/plf/refip/src/arch/boot \
					ble_pub/plf/refip/src/arch/compiler \
					ble_pub/plf/refip/src/arch/ll \
					ble_pub/plf/refip/src/arch \
					ble_pub/plf/refip/src/build/ble_full/reg/fw \
					ble_pub/plf/refip/src/driver/reg \
					ble_pub/plf/refip/src/driver/ble_icu \
					ble_lib/ip/ble/hl/inc \
					ble_lib/ip/ble/hl/api \
					ble_lib/ip/ble/hl/src/gatt/atts \
					ble_lib/ip/ble/ll/src/rwble \
					ble_lib/ip/ble/ll/src/lld \
					ble_lib/ip/ble/ll/src/em \
					ble_lib/ip/ble/ll/src/llm \
					ble_lib/ip/ble/ll/src/llc \
					ble_lib/ip/em/api \
					ble_lib/ip/ea/api \
					ble_lib/ip/hci/api \
					ble_lib/ip/ahi/api \
					ble_lib/modules/ke/api \
					ble_lib/modules/ke/src \
					ble_lib/modules/h4tl/api 

$(NAME)_SOURCES +=  ble.c \
					ble_pub/ip/ble/hl/src/prf/prf.c \
					ble_pub/ip/ble/profiles/sdp/src/sdp_service.c \
					ble_pub/ip/ble/profiles/sdp/src/sdp_service_task.c \
					ble_pub/ip/ble/profiles/comm/src/comm.c \
					ble_pub/ip/ble/profiles/comm/src/comm_task.c \
					ble_pub/modules/app/src/app_ble.c \
					ble_pub/modules/app/src/app_task.c \
					ble_pub/modules/app/src/app_sdp.c \
					ble_pub/modules/app/src/app_sec.c \
					ble_pub/modules/app/src/app_comm.c \
					ble_pub/modules/common/src/common_list.c \
					ble_pub/modules/common/src/common_utils.c \
					ble_pub/modules/common/src/RomCallFlash.c \
					ble_pub/modules/dbg/src/dbg.c \
					ble_pub/modules/dbg/src/dbg_mwsgen.c \
					ble_pub/modules/dbg/src/dbg_swdiag.c \
					ble_pub/modules/dbg/src/dbg_task.c \
					ble_pub/modules/rwip/src/rwip.c \
					ble_pub/modules/rf/src/ble_rf_xvr.c \
					ble_pub/modules/ecc_p256/src/ecc_p256.c \
					ble_pub/plf/refip/src/driver/uart/uart.c 
