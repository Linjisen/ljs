/*
 * Script for GNU linker.
 * Describes layout of sections, location of stack.
 *
 * In this case vectors are at location 0 (reset @ 0x08)
 *
 * +------------+  0x00400000
 * data         |
 * end
 * |(heap)      |
 * .            .
 * .            .
 * |(heap limit)|
 *
 * |- - - - - - |
 * stack bottom    256k
 * +------------+
 *
 * +------------+  0x0000000
 * |vectors     |
 * |            |
 * |------------+
 * |text        |
 * |data        |
 * |            |  1024k
 * +------------+
 */


/* Split memory into area for vectors and ram */
MEMORY
{
        flash  (rx) : ORIGIN = 0x00010000, LENGTH = 1215k /* 1216KB - 96B */
        tcm   (rw!x): ORIGIN = 0x003F8000, LENGTH = 32k
        ram   (rw!x): ORIGIN = 0x00400020, LENGTH = 262112
}

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_vector_start);
_vector_start = ORIGIN(flash);

SECTIONS
{
    /* vectors go to vectors region */
    . = ORIGIN(flash);
    .vectors :
    {
        KEEP(*(*.vectors))
        KEEP( *(*.rom1))
    } > flash

    /* instructions go to the text region*/

    . = ALIGN(0x8);
    /* code, instructions.for example: i=i+1; */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.stub)
        /* .gnu.warning sections are handled specially by elf32.em.  */
        *(.gnu.warning)
        *(.gnu.linkonce.t*)
        *(.glue_7t) *(.glue_7)

        KEEP(*(.fini))

        /* section information for finsh shell */
        . = ALIGN(4);
        __fsymtab_start = .;
        KEEP(*(FSymTab))
        __fsymtab_end = .;
        . = ALIGN(4);
        __vsymtab_start = .;
        KEEP(*(VSymTab))
        __vsymtab_end = .;
        . = ALIGN(4);

        /* section information for modules */
        . = ALIGN(4);
        __rtmsymtab_start = .;
        KEEP(*(RTMSymTab))
        __rtmsymtab_end = .;

        /* section information for initialization */
        . = ALIGN(4);
        __rt_init_start = .;
        KEEP(*(SORT(.rti_fn*)))
        __rt_init_end = .;
    } > flash

    /* read only data.for example: const int rom_data[3]={1,2,3}; */
    .rodata ALIGN(8) :
    {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r*)
        *(.eh_frame)
    } > flash

    . = ALIGN(4);
    .ctors :
    {
        PROVIDE(__ctors_start__ = .);
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)
        PROVIDE(__ctors_end__ = .);
    } > flash

    .dtors :
    {
        PROVIDE(__dtors_start__ = .);
        *crtbegin.o(.dtors)
        *crtbegin?.o(.dtors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
        *(SORT(.dtors.*))
        *(.dtors)
        PROVIDE(__dtors_end__ = .);
    } > flash

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > flash

    /* The .ARM.exidx section is used for C++ exception handling. */
    /* .ARM.exidx is sorted, so has to go in its own output section.  */
    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx*)
        *(.gnu.linkonce.armexidx.*)
    } > flash
    
    .rwdata ALIGN(8) :
    {
        /* globals.for example: int ram_data[3]={4,5,6}; VMA in RAM, but keep LMA in flash */
        _begin_data = .;
        . = . + SIZEOF(.data);
        . = ALIGN(4);
    } > flash

    __exidx_end = .;
    
 	. = ORIGIN(tcm);
    .tcm ALIGN(8) :
    {
		/* BEKEN */
		*mem_arch.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*hostapd*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*bk7011_cal*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rwnx_intf*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*power_save*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*wlan_ui*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ate_app*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*arch_main*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*irda*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*spi*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*wlan_cli*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rtos_pub*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*phy_trident*.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sa_station.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ieee802_11_demo.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*dd.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*dma.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*wpa_psk_cache.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*pwm_bk7231n.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*wpa_debug.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ctrl_iface.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*wpa_psk_cache.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*arbitrate.o(.bss .bss.* .scommon .sbss .dynbss COMMON)

		/* driver */
		*drv_model.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*intc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*role_launch.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*manual_cal_bk7231U.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*main_none.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*eloop.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*main_supplicant.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*wpa_supplicant.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rw_ieee80211.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rw_msg_rx.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*temp_detect.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*flash.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*gpio.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*mac_phy_bypass.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*bk_timer.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*saradc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sys_ctrl.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*BkDriverFlash.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*cmd_rx_sensitivity.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*cmd_evm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rx_sensitivity.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*start_type.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*common.o(.bss .bss.* .scommon .sbss .dynbss COMMON)

		/* beken_sdk */
		*bk7231N_cal.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*fake_clock.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*tx_evm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*param_config.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sa_ap.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*apm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*apm_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*bam_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*bam.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*chan.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*hal_dma.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*hal_machw.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ke_env.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*me_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ps.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rwnx.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rx_swdesc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rxu_cntrl.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rxl_cntrl.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rxl_hwdesc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sta_mgmt.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*scan_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*scan.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*scanu.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*scanu_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sm_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*me.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*mm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*mm_bcn.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*mm_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*mm_timer.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*td.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*tx_swdesc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*txl_buffer.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*txl_cfm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*txl_cntrl.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*vif_mgmt.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*wdt.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*app.o(.bss .bss.* .scommon .sbss .dynbss COMMON)

		/* beken_ble */
		*ble_rf_port.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*app_ble_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rwip.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*gapc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*gapc_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*gapm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*gapm_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*gattc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*gattc_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*gattm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*gattm_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*l2cc_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*l2cm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*llc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*lld.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*llm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*hci.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sch_slice.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ble_aes.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ecc_p256.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*kernel.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rwip_driver.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*l2cc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ble_util_buf.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*llc_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*lld_adv.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*lld_con.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*lld_per_adv.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*lld_test.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*llm_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*hci_fc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sch_alarm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sch_arb.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sch_plan.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sch_prog.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*kernel_event.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*kernel_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*app_ble.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ble_ui.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*hci_tl.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*h4tl.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sdp_service.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*app_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*app_sdp.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ble.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*prf.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*comm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*comm_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sdp_service_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*app_comm.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*app_sec.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*common_list.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*common_utils.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*RomCallFlash.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*dbg.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*dbg_mwsgen.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*dbg_swdiag.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*dbg_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ble_rf_xvr.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rwip.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*uart.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*app_ble_task.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*prf_utils.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rf_xvr.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*uart_ble.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ble_main.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*rwble.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*app_ble_init.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sdp_common.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*lld_init.o(.bss .bss.* .scommon .sbss .dynbss COMMON)

		/* don't add it to tcm */
		/* *hal_desc.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*scanu_shared.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*txl_buffer_shared.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*txl_frame_shared.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*scan_shared.o(.bss .bss.* .scommon .sbss .dynbss COMMON)*/
		
		/* LWIP */
		*dhcp-server-main.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*dhcp-server.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*timeouts.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*tcp_in.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*tcp.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*raw.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*netif.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ip4.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ip4_addr.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*igmp.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*etharp.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*dhcp.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*dns.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*ping.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*tcpip.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sockets.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*net.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*sys_arch.o(.bss .bss.* .scommon .sbss .dynbss COMMON)

		*idle.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		*kservice.o(.bss .bss.* .scommon .sbss .dynbss COMMON)
		
    } >tcm AT>flash
    _tcmbss_start = ADDR(.tcm);
    _tcmbss_end = _tcmbss_start + SIZEOF(.tcm);

    
    . = ORIGIN(ram);
    .data :
    {
        *(.data .data.*)
        *(.sdata)

        . = ALIGN(4);
        /* preinit data */
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);

        . = ALIGN(4);
        /* init data */
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);

        . = ALIGN(4);
        /* finit data */
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN (__fini_array_end = .);

        KEEP(*(.jcr*))
        . = ALIGN(4);
        /* All data end */

        *(.gnu.linkonce.d*)
        SORT(CONSTRUCTORS)
    } >ram AT >flash
    _end_data = .;

    /* Loader will copy data from _flash_begin to _ram_begin..ram_end */
    _data_flash_begin = LOADADDR(.data);
    _data_ram_begin = ADDR(.data);
    _data_ram_end = .;





    /* uninitialized data section - global   int i; */
    .bss ALIGN(8):
    {
        _bss_start = .;
        *(.bss .bss.*)
        *(.scommon)
        *(.sbss)
        *(.dynbss)
        *(COMMON)
        /* Align here to ensure that the .bss section occupies space up to
           _end.  Align after .bss to ensure correct alignment even if the
           .bss section disappears because there are no input sections.  */
        . = ALIGN(32 / 8);
        _bss_end = .;
    } > ram

    . = ALIGN (8);
    _empty_ram = .;

    /* This symbol defines end of code/data sections. Heap starts here. */
    PROVIDE(end = .);

    /* _stack symbol defines initial stack bottom addres. Stack grows to lower addresses.
       Typically you set this to be top of your RAM. Note: code never checks, if stack
       grows into heap area!
    */
    PROVIDE(_stack_unused      = 0x440000 - 0x3F0 - 0x7F0 - 0xFF0 - 0x3F0 - 0x10); /* 0x10*/
    PROVIDE(_stack_svc         = 0x440000 - 0x3F0 - 0x7F0 - 0xFF0 - 0x3F0); /* 0x3F0*/
    PROVIDE(_stack_irq         = 0x440000 - 0x3F0 - 0x7F0 - 0xFF0); /* 0xFF0*/
    PROVIDE(_stack_fiq         = 0x440000 - 0x3F0 - 0x7F0); /* 0x7F0*/
    PROVIDE(_stack_sys         = 0x440000 - 0x3F0); /* 0x3F0*/
}

GROUP(
   libgcc.a
   libg.a
   libc.a
   libm.a
   libnosys.a
 )