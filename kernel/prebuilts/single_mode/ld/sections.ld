/* Automatically generated file; DO NOT EDIT */
/* XinyiSemi IoT Development Framework Linker Script */
/* Generated from: F:\work\cat1\ML307Y\code\ml307y_kernel\kernel\platform\board\public\sections.ld.in */

/*
 * Default linker script for RISC-V (it includes specifics for t-head e90x).
 *
 * To make use of the multi-region initialisations, define
 * OS_INCLUDE_STARTUP_INIT_MULTIPLE_RAM_SECTIONS for the _startup.c file.
 */

/*
 * The '__stack' definition is required by crt0, do not remove it.
 */
__stack = ORIGIN(SYSRAM) + LENGTH(SYSRAM);

/*
 * Default stack sizes.
 * These are used by the startup in order to allocate stacks
 * for the different modes.
 */

__Main_Stack_Size = 0xC00 ;

PROVIDE ( _Main_Stack_Size = __Main_Stack_Size ) ;

/*
 * There will be a link error if there is not this amount of
 * RAM free at the end.
 */
_Minimum_Stack_Size = __Main_Stack_Size * 1 ;

/*
 * Default heap definitions.
 * The heap start immediately after the last statically allocated
 * .sbss/.noinit section, and extends up to the main stack limit.
 */
PROVIDE ( _Heap_Begin = _end_noinit ) ;
PROVIDE ( _Heap_Limit = __stack - _Minimum_Stack_Size ) ;

/*
 * The limit of PSRAM region.
 */
__PSRAM_Limit = ORIGIN(PSRAM) + LENGTH(PSRAM);

__FLASH_Limit = ORIGIN(FLASH) + LENGTH(FLASH);

/*
 * The entry point is Reset_Handler, does system initialise before
 * going to main function.
 */
ENTRY(Reset_Handler)


/* Sections Definitions */

SECTIONS
{
    /*
     * flash sections
     */

    .flash.text : ALIGN(8)
    {
        __flash_text_start__ = . ;

        FILL(0xFF)
        BYTE(0xFF)
        . = ALIGN(0x1000) ;     /* used for storing compressed info */

        *.flash.o(.text .text.*)
        __/opencpu/custom/custom_main/*(.text .text.*)
        *lib__idf_application_water_mtr_ai.a:(.text .text.*)
        *lib__idf_application_websocket.a:(.text .text.*)
        *lib__idf_cmiot.a:(EXCLUDE_FILE(*lib__idf_cmiot.a:at_cm_pwm.*) .text EXCLUDE_FILE(*lib__idf_cmiot.a:at_cm_pwm.*) .text.*)
        *lib__idf_cmiot.a:at_cm_pwm.*(.text .text.at_MPWMCTRL_req .text.at_MPWMDATA_req .text.pwm_zy_ctl_insert.isra.0)
        *lib__idf_cmiot_api_application_cm_cot.a:(.text .text.*)
        *lib__idf_cmiot_api_application_cm_fs.a:(.text .text.*)
        *lib__idf_cmiot_api_application_cm_ftp.a:(.text .text.*)
        *lib__idf_cmiot_api_application_cm_http.a:(.text .text.*)
        *lib__idf_cmiot_api_application_cm_mqtt.a:(.text .text.*)
        *lib__idf_cmiot_api_application_cm_ota.a:(.text .text.*)
        *lib__idf_cmiot_api_application_cm_ping.a:(.text .text.*)
        *lib__idf_cmiot_api_application_cm_sntp.a:(.text .text.*)
        *lib__idf_cmiot_api_application_cm_ssl.a:(.text .text.*)
        *lib__idf_cmiot_api_application_cm_utils.a:(.text .text.*)
        *lib__idf_cmiot_api_driver_cm_adc.a:(.text .text.*)
        *lib__idf_cmiot_api_driver_cm_gpio.a:(.text .text.*)
        *lib__idf_cmiot_api_driver_cm_i2c.a:(.text .text.*)
        *lib__idf_cmiot_api_driver_cm_iomux.a:(.text .text.*)
        *lib__idf_cmiot_api_driver_cm_powerkey.a:(.text .text.*)
        *lib__idf_cmiot_api_driver_cm_pwm.a:(.text .text.*)
        *lib__idf_cmiot_api_driver_cm_rtc.a:(.text .text.*)
        *lib__idf_cmiot_api_driver_cm_sif.a:(.text .text.*)
        *lib__idf_cmiot_api_driver_cm_spi.a:(.text .text.*)
        *lib__idf_cmiot_api_protocol_stack_cm_modem.a:(.text .text.*)
        *lib__idf_cmiot_api_protocol_stack_cm_sim.a:(.text .text.*)
        *lib__idf_cmiot_api_protocol_stack_cm_sms.a:(.text .text.*)
        *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a:(.text .text.*)
        *libcompress_jpeg.a:(.text .text.*)
        *libcompress_webp.a:(.text .text.*)
        *libvision_meter_static.a:(.text .text.*)

        /* for cache line aligned, the code and data are no longer in the same cache line. */
        . = ALIGN(0x40) ;

        __flash_text_end__ = . ;

    } >FLASH

    .flash.rodata : ALIGN(8)
    {
        __flash_rodata_start__ = . ;

        *.flash.o(.constdata .constdata.* .rodata .rodata.* .srodata .srodata.*)
        __/opencpu/custom/custom_main/*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_water_mtr_ai.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_websocket.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_application_cm_cot.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_application_cm_fs.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_application_cm_ftp.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_application_cm_http.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_application_cm_mqtt.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_application_cm_ota.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_application_cm_ping.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_application_cm_sntp.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_application_cm_ssl.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_application_cm_utils.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_driver_cm_adc.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_driver_cm_gpio.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_driver_cm_i2c.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_driver_cm_iomux.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_driver_cm_powerkey.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_driver_cm_pwm.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_driver_cm_rtc.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_driver_cm_sif.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_driver_cm_spi.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_protocol_stack_cm_modem.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_protocol_stack_cm_sim.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_protocol_stack_cm_sms.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a:(.constdata .constdata.* .rodata .rodata.*)
        *libcompress_jpeg.a:(.constdata .constdata.* .rodata .rodata.*)
        *libcompress_webp.a:(.constdata .constdata.* .rodata .rodata.*)
        *libvision_meter_static.a:(.constdata .constdata.* .rodata .rodata.*)

        . = ALIGN(8) ;

        __flash_rodata_end__ = . ;

    } >FLASH


    /*
     * psram sections
     */

    .psram.text : ALIGN(8)
    {
        __psram_stext = . ;

        *(.startup .startup.*)

        *.psram.o(.text .text.*)
        *lib__idf_application_LBS.a:(.text .text.*)
        *lib__idf_application_LED.a:(.text .text.*)
        *lib__idf_application_SIF.a:(.text .text.*)
        *lib__idf_application_SIF2.a:(.text .text.*)
        *lib__idf_application_at_cmd.a:(.text .text.*)
        *lib__idf_application_at_ctrl.a:(.text .text.*)
        *lib__idf_application_atc_ps_cmd.a:(.text .text.*)
        *lib__idf_application_cJSON.a:(.text .text.*)
        *lib__idf_application_common.a:(.text .text.*)
        *lib__idf_application_fota.a:(.text .text.*)
        *lib__idf_application_fs.a:(.text .text.*)
        *lib__idf_application_ftp.a:(.text .text.*)
        *lib__idf_application_http.a:(.text .text.*)
        *lib__idf_application_lock.a:(.text .text.*)
        *lib__idf_application_mqtt.a:(.text .text.*)
        *lib__idf_application_netdata.a:(.text .text.*)
        *lib__idf_application_network.a:(.text .text.*)
        *lib__idf_application_pin_ctl.a:(.text .text.*)
        *lib__idf_application_ping.a:(.text .text.*)
        *lib__idf_application_ppp.a:(.text .text.*)
        *lib__idf_application_socket.a:(.text .text.*)
        *lib__idf_application_socket_proxy.a:(.text .text.*)
        *lib__idf_application_ssl_at_ssl.a:(.text .text.*)
        *lib__idf_application_ssl_mbedtls-3.6.4.a:(.text .text.*)
        *lib__idf_application_ssl_ssl_api.a:(.text .text.*)
        *lib__idf_application_ssl_xy_port.a:(.text .text.*)
        *lib__idf_application_xy_version.a:(.text .text.*)
        *lib__idf_cmiot.a:at_cm_pwm.*(.text.pwm_hardware_deinit)
        *lib__idf_cmiot.a:at_cm_pwm.*(.text.zy_pwm0_hander)
        *lib__idf_cmiot.a:at_cm_pwm.*(.text.zy_pwm1_hander)
        *lib__idf_cmiot_api_driver_cm_uart.a:(.text .text.*)
        *lib__idf_cmiot_api_oss.a:(.text .text.*)
        *lib__idf_drivers.a:(EXCLUDE_FILE(*lib__idf_drivers.a:driver_utilities.* *lib__idf_drivers.a:hal_cameraspi.* *lib__idf_drivers.a:usb_udc.* *lib__idf_drivers.a:xy4100_ll_clock_driver.* *lib__idf_drivers.a:xy4100_ll_dma0.* *lib__idf_drivers.a:xy4100_ll_dma1.* *lib__idf_drivers.a:xy4100_ll_opi_flash.* *lib__idf_drivers.a:xy4100_ll_opi_psram.* *lib__idf_drivers.a:xy4100_ll_prcm.* *lib__idf_drivers.a:xy4100_ll_uart.*) .text EXCLUDE_FILE(*lib__idf_drivers.a:driver_utilities.* *lib__idf_drivers.a:hal_cameraspi.* *lib__idf_drivers.a:usb_udc.* *lib__idf_drivers.a:xy4100_ll_clock_driver.* *lib__idf_drivers.a:xy4100_ll_dma0.* *lib__idf_drivers.a:xy4100_ll_dma1.* *lib__idf_drivers.a:xy4100_ll_opi_flash.* *lib__idf_drivers.a:xy4100_ll_opi_psram.* *lib__idf_drivers.a:xy4100_ll_prcm.* *lib__idf_drivers.a:xy4100_ll_uart.*) .text.*)
        *lib__idf_drivers.a:hal_cameraspi.*(.text .text.HAL_CAMERA_SPI0_IRQHandler .text.HAL_CAMERA_SPI_DeInit .text.HAL_CAMERA_SPI_Disable .text.HAL_CAMERA_SPI_Enable .text.HAL_CAMERA_SPI_Init .text.HAL_CAMERA_SPI_Mclk_Config .text.HAL_CAMERA_SPI_ReInit_Partly .text.get_camera_block_isr .text.get_newest_block .text.release_block .text.set_bus_status)
        *lib__idf_drivers.a:usb_udc.*(.text .text.csi_dcache_clean_range .text.csi_dcache_invalid_range.constprop.0 .text.usb_clear_event_handle .text.usb_fs_get_flag .text.usb_fs_set_flag .text.usb_get_device_speed .text.usb_get_device_state .text.usb_get_interface_number .text.usb_get_port_status .text.usb_get_u9_count .text.usb_handle_event .text.usb_issue_command.isra.0 .text.usb_process_event_ring .text.usb_register_ep_xfer_comp .text.usb_register_port_state_change .text.usb_register_setup_handle .text.usb_request_clear_feature .text.usb_request_get_status .text.usb_request_set_feature .text.usb_set_device_speed .text.usb_set_device_state .text.usb_set_interface_number .text.usb_set_u9_count .text.usb_setup_control_status_trb.constprop.0 .text.usb_setup_isoch_trb.constprop.0 .text.usb_setup_normal_trb.constprop.0 .text.usb_skip_enq_trb .text.usb_udc_bulk_xfer .text.usb_udc_bulk_xfer_multi .text.usb_udc_bulk_xfer_multi_log .text.usb_udc_bulk_xfer_multi_split .text.usb_udc_bulk_xfer_split .text.usb_udc_ccs .text.usb_udc_chk_phy_vbus .text.usb_udc_ep0_xfer .text.usb_udc_ep_disable .text.usb_udc_ep_enable .text.usb_udc_ep_halt .text.usb_udc_ep_is_halted .text.usb_udc_ep_unhalt .text.usb_udc_full_speed .text.usb_udc_handle_interrupt .text.usb_udc_init .text.usb_udc_interrupt_xfer .text.usb_udc_isoch_xfer .text.usb_udc_knock_doorbell .text.usb_udc_pp .text.usb_udc_remote_wakeup .text.usb_udc_reset .text.usb_udc_run_ep .text.usb_udc_set_address .text.usb_udc_start .text.usb_udc_stop .text.usb_udc_stop_ep .text.usb_unregister_ep_xfer_comp .text.usb_update_current_speed .text.usb_xfer_trb_is_enough)
        *lib__idf_kernel_appstart.a:(EXCLUDE_FILE(*lib__idf_kernel_appstart.a:msp.*) .text EXCLUDE_FILE(*lib__idf_kernel_appstart.a:msp.*) .text.*)
        *lib__idf_kernel_appstart.a:msp.*(.text .text.LL_DMA1_Start .text.LL_DMA1_Stop .text.csi_dcache_clean .text.csi_dcache_clean_range .text.csi_dcache_invalid_range .text.gpio_int_handle .text.pmu_event_int_handle .text.root_xtal_int_handle .text.sdk_hardware_init .text.switch_clk_disable .text.switch_clk_enable .text.wakeup_int_handle)
        *lib__idf_kernel_cmux.a:(.text .text.*)
        *lib__idf_kernel_decompress.a:(.text .text.*)
        *lib__idf_kernel_diag_ctrl.a:(.text .text.*)
        *lib__idf_kernel_flash_daemon.a:(.text .text.*)
        *lib__idf_kernel_inter_core.a:(.text .text.*)
        *lib__idf_kernel_lowpower.a:(EXCLUDE_FILE(*lib__idf_kernel_lowpower.a:deepsleep.* *lib__idf_kernel_lowpower.a:dual_wfi.* *lib__idf_kernel_lowpower.a:fast_recovery.* *lib__idf_kernel_lowpower.a:fast_recovery_asm.* *lib__idf_kernel_lowpower.a:idle_hook.* *lib__idf_kernel_lowpower.a:lightsleep.* *lib__idf_kernel_lowpower.a:lpm_common.* *lib__idf_kernel_lowpower.a:sleep.* *lib__idf_kernel_lowpower.a:xy_lpm.*) .text EXCLUDE_FILE(*lib__idf_kernel_lowpower.a:deepsleep.* *lib__idf_kernel_lowpower.a:dual_wfi.* *lib__idf_kernel_lowpower.a:fast_recovery.* *lib__idf_kernel_lowpower.a:fast_recovery_asm.* *lib__idf_kernel_lowpower.a:idle_hook.* *lib__idf_kernel_lowpower.a:lightsleep.* *lib__idf_kernel_lowpower.a:lpm_common.* *lib__idf_kernel_lowpower.a:sleep.* *lib__idf_kernel_lowpower.a:xy_lpm.*) .text.*)
        *lib__idf_kernel_lowpower.a:deepsleep.*(.text.DeepSleep_Admittance_Check)
        *lib__idf_kernel_lowpower.a:deepsleep.*(.text.DeepSleep_Power_Manage_PSRAM)
        *lib__idf_kernel_lowpower.a:deepsleep.*(.text.lpm_nv_save)
        *lib__idf_kernel_lowpower.a:dual_wfi.*(.text.DualWfi_Admittance_Check)
        *lib__idf_kernel_lowpower.a:lightsleep.*(.text.LightSleep_Admittance_Check)
        *lib__idf_kernel_lowpower.a:lpm_common.*(.text .text.AP_RTCAlarm_Coming .text.CP_Is_Alive .text.CP_RTCAlarm_Coming .text.Debug_Sleep_URC_Send .text.LPM_CopyFromRetmem .text.LPM_CopyToRetmem .text.LPM_GetExpectedIdleTick .text.LPM_GetMtimeOffset .text.LPM_Get_Rtccnt .text.LPM_Get_Rtccnt_Edge .text.LPM_OStick_Recover .text.LPM_OStick_Snapshot .text.LpmSleepModeGet .text.Retmem_Byte_Read .text.Retmem_Byte_Write .text.csi_dcache_invalid_range.constprop.0 .text.flash_recover .text.lpm_reset_rtc_int_tick .text.lpm_set_rtc_int_tick)
        *lib__idf_kernel_lowpower.a:sleep.*(.text.Sleep_Admittance_Check)
        *lib__idf_kernel_lowpower.a:sleep.*(.text.Sleep_Power_Manage)
        *lib__idf_kernel_lowpower.a:xy_lpm.*(.text .text.Create_Powerdown_SleepLock .text.DeepsleepIsEnable .text.Get_Boot_Reason .text.Get_Boot_Sub_Reason .text.Is_WakeUp_By_Lpuart .text.Is_WakeUp_By_Self .text.Is_WakeUp_From_Dsleep .text.Is_WakeUp_From_Sleep .text.Is_user_permit_lpm .text.LL_PMU_PRCM_Enable_UTC_Wkup_PD .text.LL_PMU_PRCM_Set_Enter_PD .text.LightsleepIsEnable .text.Restore_Peripherals_After_Sleep .text.SleepIsEnable .text.Wakeup_Sleep_SubReason_Get .text.WfiIsEnable .text.clear_sleep_lock .text.control_hrc_in_lightsleep .text.control_hrc_in_sleep .text.create_sleep_lock .text.csi_dcache_clean .text.csi_dcache_clean_range .text.delete_sleep_lock .text.get_lock_map .text.get_lock_stat .text.get_sleep_lock_num .text.hold_cp_handshake .text.is_sleep_locked .text.print_lock_name .text.read_reset_data .text.save_reset_data .text.sleep_lock .text.sleep_unlock .text.sleep_unlock_inner .text.xy_KeepSleepIO .text.xy_Set_Enter_PD .text.xy_Soft_Reset .text.xy_Soft_Reset_safe .text.xy_io_wakeup_set)
        *lib__idf_kernel_mempool.a:(.text .text.*)
        *lib__idf_kernel_misc.a:(.text .text.*)
        *lib__idf_kernel_newlib.a:(.text .text.*)
        *lib__idf_kernel_os_FreeRTOS.a:(EXCLUDE_FILE(*lib__idf_kernel_os_FreeRTOS.a:port.* *lib__idf_kernel_os_FreeRTOS.a:portASM.* *lib__idf_kernel_os_FreeRTOS.a:tasks.*) .text EXCLUDE_FILE(*lib__idf_kernel_os_FreeRTOS.a:port.* *lib__idf_kernel_os_FreeRTOS.a:portASM.* *lib__idf_kernel_os_FreeRTOS.a:tasks.*) .text.*)
        *lib__idf_kernel_os_FreeRTOS.a:port.*(.text .text.prvTaskExitError .text.pxPortInitialiseStack .text.vPortEndScheduler .text.vPortSetupSoftwareInterrupt .text.vPortSetupTimerInterrupt .text.vPortSysTickHandler .text.xPortStartScheduler)
        *lib__idf_kernel_os_FreeRTOS.a:portASM.*(.text .text.vPortStartFirstTask)
        *lib__idf_kernel_os_FreeRTOS.a:tasks.*(.text .text.eTaskGetLowPowerFlag .text.eTaskGetState .text.pcTaskGetName .text.prvAddCurrentTaskToDelayedList .text.prvAddNewTaskToReadyList .text.prvDeleteTCB .text.prvIdleTask .text.prvInitialiseNewTask.constprop.0 .text.prvListTasksGetRunTimeCountertWithinSingleList .text.prvListTasksReseRunTimeCountertWithinSingleList .text.prvListTasksResetRunTimeCounter .text.prvResetNextTaskUnblockTime .text.prvSearchForNameWithinSingleList .text.pvTaskGetEndOfStack .text.pvTaskIncrementMutexHeldCount .text.pxTaskGetStackInfo .text.ulTaskGenericNotifyTake .text.ulTaskGenericNotifyValueClear .text.ulTaskGetIdleRunTimeCounter .text.uxTaskGetIndStackHighWaterMark .text.uxTaskGetNumberOfTasks .text.uxTaskGetRunTimeMessage .text.uxTaskGetRunTimeTops .text.uxTaskGetStackHighWaterMark .text.uxTaskIFGetRunTimeTops .text.uxTaskPriorityGet .text.uxTaskPriorityGetFromISR .text.uxTaskResetEventItemValue .text.uxTaskResetRunTimeCounter .text.vStackStatsListFree .text.vStackStatsListInsert .text.vTaskDelay .text.vTaskDelete .text.vTaskEndScheduler .text.vTaskGenericNotifyGiveFromISR .text.vTaskInternalSetTimeOutState .text.vTaskMissedYield .text.vTaskPlaceOnEventList .text.vTaskPlaceOnEventListRestricted .text.vTaskPlaceOnUnorderedEventList .text.vTaskPriorityDisinheritAfterTimeout .text.vTaskPrioritySet .text.vTaskRemoveFromUnorderedEventList .text.vTaskResume .text.vTaskSetLowPowerFlag .text.vTaskSetTimeOutState .text.vTaskSleepStepTick .text.vTaskStartScheduler .text.vTaskSuspend .text.vTaskSuspendAll .text.vTasksRecordInterruptInRunTime .text.vTasksRecordInterruptOutRunTime .text.xTaskCatchUpTicks .text.xTaskCheckForTimeOut .text.xTaskCreate .text.xTaskCreateStatic .text.xTaskDelayUntil .text.xTaskGenericNotify .text.xTaskGenericNotifyFromISR .text.xTaskGenericNotifyStateClear .text.xTaskGenericNotifyWait .text.xTaskGetExpectedIdleTime .text.xTaskGetHandle .text.xTaskGetTickCount .text.xTaskGetTickCountFromISR .text.xTaskIncrementTick .text.xTaskPriorityDisinherit .text.xTaskPriorityInherit .text.xTaskRemoveFromEventList .text.xTaskResumeAll .text.xTaskResumeFromISR)
        *lib__idf_kernel_osal.a:(.text .text.*)
        *lib__idf_kernel_posix.a:(EXCLUDE_FILE(*lib__idf_kernel_posix.a:lpuart_at_device.*) .text EXCLUDE_FILE(*lib__idf_kernel_posix.a:lpuart_at_device.*) .text.*)
        *lib__idf_kernel_posix.a:lpuart_at_device.*(.text .text.is_lpuart_rxfifo_empty .text.lpuart_at_assert_print .text.lpuart_at_device_close .text.lpuart_at_device_init .text.lpuart_at_device_ioctl .text.lpuart_at_device_open .text.lpuart_at_device_read .text.lpuart_at_device_write .text.lpuart_at_get_baudrate .text.lpuart_at_int_handle .text.lpuart_at_rx_sem_release .text.lpuart_at_set_rx_mode.part.0 .text.lpuart_at_set_tx_mode .text.lpuart_at_tx_sem_release .text.lpuart_at_wait_autobaud .text.lpuart_at_wait_tx_allout .text.lpuart_get_char .text.lpuart_recovery .text.lpuart_recovery_after_sleep_for_opencpu .text.lpuart_save_before_sleep_for_opencpu .text.rb_init .text.rb_read .text.rb_write .text.set_lock_by_rate .text.write_rb_from_fifo)
        *lib__idf_kernel_prcm.a:(EXCLUDE_FILE(*lib__idf_kernel_prcm.a:xy_prcm.*) .text EXCLUDE_FILE(*lib__idf_kernel_prcm.a:xy_prcm.*) .text.*)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text .text.KeyScan_TopPinMuxRelease .text.KeyScan_TopPinMuxSet .text.KeyScan_TopPinMuxSet3x3 .text.KeyScan_TopPinMuxSet7x8 .text.LL_ADC_Reset .text.LL_CAMERA_SPI_DeInit .text.LL_CAMERA_SPI_MclkDisable .text.LL_CAMERA_SPI_MclkEnable .text.LL_CAMERA_SPI_PclkHclkDisable .text.LL_CAMERA_SPI_PclkHclkEnable .text.LL_CSP_DeInit .text.LL_DCAN_DeInit .text.LL_DMA1_Reset .text.LL_FLASH_OPI_CorePrcm_Reset_DMA .text.LL_FLASH_OPI_CorePrcm_Reset_Opi .text.LL_I2C_DeInit .text.LL_JTAG_GPIO29_32_Select_CpJtag .text.LL_JTAG_GPIO29_32_Select_Gpio .text.LL_KeyScan_Release_TopPinMux3x3 .text.LL_KeyScan_Release_TopPinMux7x8 .text.LL_OPI_PSRAM_CorePrcm_Reset .text.LL_OPI_PSRAM_Enable_Clock_Gate .text.LL_SPILCD_DIS_APJTAG .text.LL_SPILCD_EN_APJTAG .text.LL_SPILCD_InitialGPIO_A2 .text.LL_SPI_DeInit .text.LL_WDT_WdtClkEnable .text.PRCM_Clock_AllEnable .text.get_ahbclk_freq .text.get_apb1clk_freq .text.get_apb2clk_freq .text.get_audio_rclk_freq .text.get_i2c0_rclk_freq .text.get_i2c1_rclk_freq .text.get_lptmr_rclk_freq .text.get_lpuart_rclk_freq .text.get_lsioclk_freq .text.get_sysbusclk_freq .text.get_sysclk_freq .text.measure_32k .text.measure_hrc)
        *lib__idf_kernel_rc32k.a:(.text .text.*)
        *lib__idf_kernel_rtc.a:(EXCLUDE_FILE(*lib__idf_kernel_rtc.a:rtc_utils.*) .text EXCLUDE_FILE(*lib__idf_kernel_rtc.a:rtc_utils.*) .text.*)
        *lib__idf_kernel_rtc.a:rtc_utils.*(.text .text.clock_calendar_to_utc .text.clock_dayoftheweek .text.clock_days_before_month .text.clock_utc_to_calendar)
        *lib__idf_kernel_special.a:(.text .text.*)
        *lib__idf_kernel_usbx.a:(.text .text.*)
        *libc.a:(.text .text.*)
        *libc_nano.a:(.text .text.*)
        *libgcc.a:(.text .text.*)
        *libm.a:(.text .text.*)
        *libm_nano.a:(.text .text.*)

        . = ALIGN(8);

        *(vtable)

        *(.eh_frame*)

        . = ALIGN(8);

        __ctor_start__ = . ;
        *(SORT(.ctors.*))
        *(.ctors)
        __ctor_end__ = . ;

        *(SORT(.dtors.*))
        *(.dtors)
        __dtor_end__ = . ;

        *(.gcc_except_table .gcc_except_table.*)

        /* for cache line aligned, the code and data are no longer in the same cache line. */
        . = ALIGN(0x40);

        __psram_text_end__ = . ;

    } >PSRAM

    .psram.rodata : ALIGN(8)
    {
        __psram_rodata_start__ = . ;

        *.psram.o(.constdata .constdata.* .rodata .rodata.* .srodata .srodata.*)
        *lib__idf_application_LBS.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_LED.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_SIF.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_SIF2.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_at_cmd.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_at_ctrl.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_atc_ps_cmd.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_cJSON.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_common.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_fota.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_fs.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_ftp.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_http.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_lock.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_mqtt.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_netdata.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_network.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_pin_ctl.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_ping.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_ppp.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_socket.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_socket_proxy.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_ssl_at_ssl.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_ssl_mbedtls-3.6.4.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_ssl_ssl_api.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_ssl_xy_port.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_application_xy_version.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_driver_cm_uart.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_cmiot_api_oss.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_drivers.a:(EXCLUDE_FILE(*lib__idf_drivers.a:driver_utilities.* *lib__idf_drivers.a:xy4100_ll_clock_driver.* *lib__idf_drivers.a:xy4100_ll_dma0.* *lib__idf_drivers.a:xy4100_ll_dma1.* *lib__idf_drivers.a:xy4100_ll_opi_flash.* *lib__idf_drivers.a:xy4100_ll_opi_psram.* *lib__idf_drivers.a:xy4100_ll_prcm.* *lib__idf_drivers.a:xy4100_ll_uart.*) .constdata EXCLUDE_FILE(*lib__idf_drivers.a:driver_utilities.* *lib__idf_drivers.a:xy4100_ll_clock_driver.* *lib__idf_drivers.a:xy4100_ll_dma0.* *lib__idf_drivers.a:xy4100_ll_dma1.* *lib__idf_drivers.a:xy4100_ll_opi_flash.* *lib__idf_drivers.a:xy4100_ll_opi_psram.* *lib__idf_drivers.a:xy4100_ll_prcm.* *lib__idf_drivers.a:xy4100_ll_uart.*) .constdata.* EXCLUDE_FILE(*lib__idf_drivers.a:driver_utilities.* *lib__idf_drivers.a:xy4100_ll_clock_driver.* *lib__idf_drivers.a:xy4100_ll_dma0.* *lib__idf_drivers.a:xy4100_ll_dma1.* *lib__idf_drivers.a:xy4100_ll_opi_flash.* *lib__idf_drivers.a:xy4100_ll_opi_psram.* *lib__idf_drivers.a:xy4100_ll_prcm.* *lib__idf_drivers.a:xy4100_ll_uart.*) .rodata EXCLUDE_FILE(*lib__idf_drivers.a:driver_utilities.* *lib__idf_drivers.a:xy4100_ll_clock_driver.* *lib__idf_drivers.a:xy4100_ll_dma0.* *lib__idf_drivers.a:xy4100_ll_dma1.* *lib__idf_drivers.a:xy4100_ll_opi_flash.* *lib__idf_drivers.a:xy4100_ll_opi_psram.* *lib__idf_drivers.a:xy4100_ll_prcm.* *lib__idf_drivers.a:xy4100_ll_uart.*) .rodata.*)
        *lib__idf_kernel_appstart.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_cmux.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_decompress.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_diag_ctrl.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_flash_daemon.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_inter_core.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_lowpower.a:(EXCLUDE_FILE(*lib__idf_kernel_lowpower.a:deepsleep.* *lib__idf_kernel_lowpower.a:dual_wfi.* *lib__idf_kernel_lowpower.a:fast_recovery.* *lib__idf_kernel_lowpower.a:fast_recovery_asm.* *lib__idf_kernel_lowpower.a:idle_hook.* *lib__idf_kernel_lowpower.a:lightsleep.* *lib__idf_kernel_lowpower.a:sleep.*) .constdata EXCLUDE_FILE(*lib__idf_kernel_lowpower.a:deepsleep.* *lib__idf_kernel_lowpower.a:dual_wfi.* *lib__idf_kernel_lowpower.a:fast_recovery.* *lib__idf_kernel_lowpower.a:fast_recovery_asm.* *lib__idf_kernel_lowpower.a:idle_hook.* *lib__idf_kernel_lowpower.a:lightsleep.* *lib__idf_kernel_lowpower.a:sleep.*) .constdata.* EXCLUDE_FILE(*lib__idf_kernel_lowpower.a:deepsleep.* *lib__idf_kernel_lowpower.a:dual_wfi.* *lib__idf_kernel_lowpower.a:fast_recovery.* *lib__idf_kernel_lowpower.a:fast_recovery_asm.* *lib__idf_kernel_lowpower.a:idle_hook.* *lib__idf_kernel_lowpower.a:lightsleep.* *lib__idf_kernel_lowpower.a:sleep.*) .rodata EXCLUDE_FILE(*lib__idf_kernel_lowpower.a:deepsleep.* *lib__idf_kernel_lowpower.a:dual_wfi.* *lib__idf_kernel_lowpower.a:fast_recovery.* *lib__idf_kernel_lowpower.a:fast_recovery_asm.* *lib__idf_kernel_lowpower.a:idle_hook.* *lib__idf_kernel_lowpower.a:lightsleep.* *lib__idf_kernel_lowpower.a:sleep.*) .rodata.*)
        *lib__idf_kernel_mempool.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_misc.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_newlib.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_os_FreeRTOS.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_osal.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_posix.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_prcm.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_rc32k.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_rtc.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_special.a:(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_usbx.a:(.constdata .constdata.* .rodata .rodata.*)
        *libc.a:(.rodata .rodata.* .srodata .srodata.*)
        *libc_nano.a:(.rodata .rodata.* .srodata .srodata.*)
        *libgcc.a:(.rodata .rodata.* .srodata .srodata.*)
        *libm.a:(.rodata .rodata.* .srodata .srodata.*)
        *libm_nano.a:(.rodata .rodata.* .srodata .srodata.*)

        . = ALIGN(8) ;

        __appRegTable_start__ = .;
        KEEP(*(.appRegTable .appRegTable.*))
        __appRegTable_end__ = .;

        __appRegTable_start2__ = .;
        KEEP(*(.appRegTable2))
        __appRegTable_end2__ = .;

        . = ALIGN(8) ;

        __psram_rodata_end__ = . ;

    } >PSRAM

    _psram_sidata = LOADADDR(.psram.data);

    .psram.data : ALIGN(8)
    {
        FILL(0xFF)
        __psram_data_start__ = . ;

        *.flash.o(.data .data.* .sdata .sdata.*)
        *.psram.o(.data .data.* .sdata .sdata.*)
        __/opencpu/custom/custom_main/*(.data .data.*)
        *lib__idf_application_LBS.a:(.data .data.*)
        *lib__idf_application_LED.a:(.data .data.*)
        *lib__idf_application_SIF.a:(.data .data.*)
        *lib__idf_application_SIF2.a:(.data .data.*)
        *lib__idf_application_at_cmd.a:(.data .data.*)
        *lib__idf_application_at_ctrl.a:(.data .data.*)
        *lib__idf_application_atc_ps_cmd.a:(.data .data.*)
        *lib__idf_application_cJSON.a:(.data .data.*)
        *lib__idf_application_common.a:(.data .data.*)
        *lib__idf_application_fota.a:(.data .data.*)
        *lib__idf_application_fs.a:(.data .data.*)
        *lib__idf_application_ftp.a:(.data .data.*)
        *lib__idf_application_http.a:(.data .data.*)
        *lib__idf_application_lock.a:(.data .data.*)
        *lib__idf_application_mqtt.a:(.data .data.*)
        *lib__idf_application_netdata.a:(.data .data.*)
        *lib__idf_application_network.a:(.data .data.*)
        *lib__idf_application_pin_ctl.a:(.data .data.*)
        *lib__idf_application_ping.a:(.data .data.*)
        *lib__idf_application_ppp.a:(.data .data.*)
        *lib__idf_application_socket.a:(.data .data.*)
        *lib__idf_application_socket_proxy.a:(.data .data.*)
        *lib__idf_application_ssl_at_ssl.a:(.data .data.*)
        *lib__idf_application_ssl_mbedtls-3.6.4.a:(.data .data.*)
        *lib__idf_application_ssl_ssl_api.a:(.data .data.*)
        *lib__idf_application_ssl_xy_port.a:(.data .data.*)
        *lib__idf_application_water_mtr_ai.a:(.data .data.*)
        *lib__idf_application_websocket.a:(.data .data.*)
        *lib__idf_application_xy_version.a:(.data .data.*)
        *lib__idf_cmiot.a:(.data .data.*)
        *lib__idf_cmiot_api_application_cm_cot.a:(.data .data.*)
        *lib__idf_cmiot_api_application_cm_fs.a:(.data .data.*)
        *lib__idf_cmiot_api_application_cm_ftp.a:(.data .data.*)
        *lib__idf_cmiot_api_application_cm_http.a:(.data .data.*)
        *lib__idf_cmiot_api_application_cm_mqtt.a:(.data .data.*)
        *lib__idf_cmiot_api_application_cm_ota.a:(.data .data.*)
        *lib__idf_cmiot_api_application_cm_ping.a:(.data .data.*)
        *lib__idf_cmiot_api_application_cm_sntp.a:(.data .data.*)
        *lib__idf_cmiot_api_application_cm_ssl.a:(.data .data.*)
        *lib__idf_cmiot_api_application_cm_utils.a:(.data .data.*)
        *lib__idf_cmiot_api_driver_cm_adc.a:(.data .data.*)
        *lib__idf_cmiot_api_driver_cm_gpio.a:(.data .data.*)
        *lib__idf_cmiot_api_driver_cm_i2c.a:(.data .data.*)
        *lib__idf_cmiot_api_driver_cm_iomux.a:(.data .data.*)
        *lib__idf_cmiot_api_driver_cm_powerkey.a:(.data .data.*)
        *lib__idf_cmiot_api_driver_cm_pwm.a:(.data .data.*)
        *lib__idf_cmiot_api_driver_cm_rtc.a:(.data .data.*)
        *lib__idf_cmiot_api_driver_cm_sif.a:(.data .data.*)
        *lib__idf_cmiot_api_driver_cm_spi.a:(.data .data.*)
        *lib__idf_cmiot_api_driver_cm_uart.a:(.data .data.*)
        *lib__idf_cmiot_api_oss.a:(.data .data.*)
        *lib__idf_cmiot_api_protocol_stack_cm_modem.a:(.data .data.*)
        *lib__idf_cmiot_api_protocol_stack_cm_sim.a:(.data .data.*)
        *lib__idf_cmiot_api_protocol_stack_cm_sms.a:(.data .data.*)
        *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a:(.data .data.*)
        *lib__idf_drivers.a:(EXCLUDE_FILE(*lib__idf_drivers.a:driver_utilities.* *lib__idf_drivers.a:xy4100_ll_clock_driver.* *lib__idf_drivers.a:xy4100_ll_dma0.* *lib__idf_drivers.a:xy4100_ll_dma1.* *lib__idf_drivers.a:xy4100_ll_opi_flash.* *lib__idf_drivers.a:xy4100_ll_opi_psram.* *lib__idf_drivers.a:xy4100_ll_prcm.* *lib__idf_drivers.a:xy4100_ll_uart.* *lib__idf_drivers.a:xy_dma.*) .data EXCLUDE_FILE(*lib__idf_drivers.a:driver_utilities.* *lib__idf_drivers.a:xy4100_ll_clock_driver.* *lib__idf_drivers.a:xy4100_ll_dma0.* *lib__idf_drivers.a:xy4100_ll_dma1.* *lib__idf_drivers.a:xy4100_ll_opi_flash.* *lib__idf_drivers.a:xy4100_ll_opi_psram.* *lib__idf_drivers.a:xy4100_ll_prcm.* *lib__idf_drivers.a:xy4100_ll_uart.* *lib__idf_drivers.a:xy_dma.*) .data.*)
        *lib__idf_drivers.a:xy_dma.*(.data)
        *lib__idf_kernel_appstart.a:(.data .data.*)
        *lib__idf_kernel_cmux.a:(.data .data.*)
        *lib__idf_kernel_decompress.a:(.data .data.*)
        *lib__idf_kernel_diag_ctrl.a:(.data .data.*)
        *lib__idf_kernel_flash_daemon.a:(.data .data.*)
        *lib__idf_kernel_inter_core.a:(.data .data.*)
        *lib__idf_kernel_lowpower.a:(EXCLUDE_FILE(*lib__idf_kernel_lowpower.a:deepsleep.* *lib__idf_kernel_lowpower.a:dual_wfi.* *lib__idf_kernel_lowpower.a:fast_recovery.* *lib__idf_kernel_lowpower.a:fast_recovery_asm.* *lib__idf_kernel_lowpower.a:idle_hook.* *lib__idf_kernel_lowpower.a:lightsleep.* *lib__idf_kernel_lowpower.a:sleep.*) .data EXCLUDE_FILE(*lib__idf_kernel_lowpower.a:deepsleep.* *lib__idf_kernel_lowpower.a:dual_wfi.* *lib__idf_kernel_lowpower.a:fast_recovery.* *lib__idf_kernel_lowpower.a:fast_recovery_asm.* *lib__idf_kernel_lowpower.a:idle_hook.* *lib__idf_kernel_lowpower.a:lightsleep.* *lib__idf_kernel_lowpower.a:sleep.*) .data.*)
        *lib__idf_kernel_mempool.a:(.data .data.*)
        *lib__idf_kernel_misc.a:(.data .data.*)
        *lib__idf_kernel_newlib.a:(.data .data.*)
        *lib__idf_kernel_os_FreeRTOS.a:(.data .data.*)
        *lib__idf_kernel_osal.a:(.data .data.*)
        *lib__idf_kernel_posix.a:(.data .data.*)
        *lib__idf_kernel_prcm.a:(.data .data.*)
        *lib__idf_kernel_rc32k.a:(.data .data.*)
        *lib__idf_kernel_rtc.a:(.data .data.*)
        *lib__idf_kernel_special.a:(.data .data.*)
        *lib__idf_kernel_usbx.a:(.data .data.*)
        *libc.a:(.data .data.* .sdata .sdata.*)
        *libc_nano.a:(.data .data.* .sdata .sdata.*)
        *libcompress_jpeg.a:(.data .data.*)
        *libcompress_webp.a:(.data .data.*)
        *libgcc.a:(.data .data.* .sdata .sdata.*)
        *libm.a:(.data .data.* .sdata .sdata.*)
        *libm_nano.a:(.data .data.* .sdata .sdata.*)
        *libvision_meter_static.a:(.data .data.*)

        . = ALIGN(8);
        __psram_data_end__ = . ;

    } >PSRAM

    .psram.bss (NOLOAD) : ALIGN(8)
    {
        __psram_bss_start__ = . ;

        *.flash.o(.bss .bss.* .sbss .sbss.*)
        *.psram.o(.bss .bss.* .sbss .sbss.*)
        __/opencpu/custom/custom_main/*(.bss .bss.*)
        *lib__idf_application_LBS.a:(.bss .bss.*)
        *lib__idf_application_LED.a:(.bss .bss.*)
        *lib__idf_application_SIF.a:(.bss .bss.*)
        *lib__idf_application_SIF2.a:(.bss .bss.*)
        *lib__idf_application_at_cmd.a:(.bss .bss.*)
        *lib__idf_application_at_ctrl.a:(.bss .bss.*)
        *lib__idf_application_atc_ps_cmd.a:(.bss .bss.*)
        *lib__idf_application_cJSON.a:(.bss .bss.*)
        *lib__idf_application_common.a:(.bss .bss.*)
        *lib__idf_application_fota.a:(.bss .bss.*)
        *lib__idf_application_fs.a:(.bss .bss.*)
        *lib__idf_application_ftp.a:(.bss .bss.*)
        *lib__idf_application_http.a:(.bss .bss.*)
        *lib__idf_application_lock.a:(.bss .bss.*)
        *lib__idf_application_mqtt.a:(.bss .bss.*)
        *lib__idf_application_netdata.a:(.bss .bss.*)
        *lib__idf_application_network.a:(.bss .bss.*)
        *lib__idf_application_pin_ctl.a:(.bss .bss.*)
        *lib__idf_application_ping.a:(.bss .bss.*)
        *lib__idf_application_ppp.a:(.bss .bss.*)
        *lib__idf_application_socket.a:(.bss .bss.*)
        *lib__idf_application_socket_proxy.a:(.bss .bss.*)
        *lib__idf_application_ssl_at_ssl.a:(.bss .bss.*)
        *lib__idf_application_ssl_mbedtls-3.6.4.a:(.bss .bss.*)
        *lib__idf_application_ssl_ssl_api.a:(.bss .bss.*)
        *lib__idf_application_ssl_xy_port.a:(.bss .bss.*)
        *lib__idf_application_water_mtr_ai.a:(.bss .bss.*)
        *lib__idf_application_websocket.a:(.bss .bss.*)
        *lib__idf_application_xy_version.a:(.bss .bss.*)
        *lib__idf_cmiot.a:(.bss .bss.*)
        *lib__idf_cmiot_api_application_cm_cot.a:(.bss .bss.*)
        *lib__idf_cmiot_api_application_cm_fs.a:(.bss .bss.*)
        *lib__idf_cmiot_api_application_cm_ftp.a:(.bss .bss.*)
        *lib__idf_cmiot_api_application_cm_http.a:(.bss .bss.*)
        *lib__idf_cmiot_api_application_cm_mqtt.a:(.bss .bss.*)
        *lib__idf_cmiot_api_application_cm_ota.a:(.bss .bss.*)
        *lib__idf_cmiot_api_application_cm_ping.a:(.bss .bss.*)
        *lib__idf_cmiot_api_application_cm_sntp.a:(.bss .bss.*)
        *lib__idf_cmiot_api_application_cm_ssl.a:(.bss .bss.*)
        *lib__idf_cmiot_api_application_cm_utils.a:(.bss .bss.*)
        *lib__idf_cmiot_api_driver_cm_adc.a:(.bss .bss.*)
        *lib__idf_cmiot_api_driver_cm_gpio.a:(.bss .bss.*)
        *lib__idf_cmiot_api_driver_cm_i2c.a:(.bss .bss.*)
        *lib__idf_cmiot_api_driver_cm_iomux.a:(.bss .bss.*)
        *lib__idf_cmiot_api_driver_cm_powerkey.a:(.bss .bss.*)
        *lib__idf_cmiot_api_driver_cm_pwm.a:(.bss .bss.*)
        *lib__idf_cmiot_api_driver_cm_rtc.a:(.bss .bss.*)
        *lib__idf_cmiot_api_driver_cm_sif.a:(.bss .bss.*)
        *lib__idf_cmiot_api_driver_cm_spi.a:(.bss .bss.*)
        *lib__idf_cmiot_api_driver_cm_uart.a:(.bss .bss.*)
        *lib__idf_cmiot_api_oss.a:(.bss .bss.*)
        *lib__idf_cmiot_api_protocol_stack_cm_modem.a:(.bss .bss.*)
        *lib__idf_cmiot_api_protocol_stack_cm_sim.a:(.bss .bss.*)
        *lib__idf_cmiot_api_protocol_stack_cm_sms.a:(.bss .bss.*)
        *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a:(.bss .bss.*)
        *lib__idf_drivers.a:(EXCLUDE_FILE(*lib__idf_drivers.a:driver_utilities.* *lib__idf_drivers.a:hal_cameraspi.* *lib__idf_drivers.a:xy4100_ll_clock_driver.* *lib__idf_drivers.a:xy4100_ll_dma0.* *lib__idf_drivers.a:xy4100_ll_dma1.* *lib__idf_drivers.a:xy4100_ll_opi_flash.* *lib__idf_drivers.a:xy4100_ll_opi_psram.* *lib__idf_drivers.a:xy4100_ll_prcm.* *lib__idf_drivers.a:xy4100_ll_uart.*) .bss EXCLUDE_FILE(*lib__idf_drivers.a:driver_utilities.* *lib__idf_drivers.a:hal_cameraspi.* *lib__idf_drivers.a:xy4100_ll_clock_driver.* *lib__idf_drivers.a:xy4100_ll_dma0.* *lib__idf_drivers.a:xy4100_ll_dma1.* *lib__idf_drivers.a:xy4100_ll_opi_flash.* *lib__idf_drivers.a:xy4100_ll_opi_psram.* *lib__idf_drivers.a:xy4100_ll_prcm.* *lib__idf_drivers.a:xy4100_ll_uart.*) .bss.*)
        *lib__idf_drivers.a:hal_cameraspi.*(.bss .bss.g_cameraspi_handle .bss.is_bus_used .bss.line_num .bss.now_frame_block .bss.now_line_psram_addr .bss.now_line_sysram_A_addr .bss.now_line_sysram_B_addr .bss.now_sysram_A_num .bss.now_sysram_B_num .bss.pool)
        *lib__idf_kernel_appstart.a:(.bss .bss.*)
        *lib__idf_kernel_cmux.a:(.bss .bss.*)
        *lib__idf_kernel_decompress.a:(.bss .bss.*)
        *lib__idf_kernel_diag_ctrl.a:(EXCLUDE_FILE(*lib__idf_kernel_diag_ctrl.a:diag_process.*) .bss EXCLUDE_FILE(*lib__idf_kernel_diag_ctrl.a:diag_process.*) .bss.*)
        *lib__idf_kernel_diag_ctrl.a:diag_process.*(.bss .bss.g_diag_heart_timer_handle .bss.g_diag_send_thread_handle .bss.g_diag_send_timer_handle .bss.g_diag_thread_send_flag .bss.g_diag_timer_run_flag)
        *lib__idf_kernel_flash_daemon.a:(.bss .bss.*)
        *lib__idf_kernel_inter_core.a:(EXCLUDE_FILE(*lib__idf_kernel_inter_core.a:ic_port.*) .bss EXCLUDE_FILE(*lib__idf_kernel_inter_core.a:ic_port.*) .bss.*)
        *lib__idf_kernel_inter_core.a:ic_port.*(.bss .bss.Icc_TCB .bss.task_num.1 .bss.thread_idx.0)
        *lib__idf_kernel_lowpower.a:(EXCLUDE_FILE(*lib__idf_kernel_lowpower.a:deepsleep.* *lib__idf_kernel_lowpower.a:dual_wfi.* *lib__idf_kernel_lowpower.a:fast_recovery.* *lib__idf_kernel_lowpower.a:fast_recovery_asm.* *lib__idf_kernel_lowpower.a:idle_hook.* *lib__idf_kernel_lowpower.a:lightsleep.* *lib__idf_kernel_lowpower.a:sleep.* *lib__idf_kernel_lowpower.a:xy_lpm.*) .bss EXCLUDE_FILE(*lib__idf_kernel_lowpower.a:deepsleep.* *lib__idf_kernel_lowpower.a:dual_wfi.* *lib__idf_kernel_lowpower.a:fast_recovery.* *lib__idf_kernel_lowpower.a:fast_recovery_asm.* *lib__idf_kernel_lowpower.a:idle_hook.* *lib__idf_kernel_lowpower.a:lightsleep.* *lib__idf_kernel_lowpower.a:sleep.* *lib__idf_kernel_lowpower.a:xy_lpm.*) .bss.*)
        *lib__idf_kernel_lowpower.a:xy_lpm.*(.bss .bss.control_hrc_sleep_flag .bss.gWakupNumDebug .bss.g_flash_boot .bss.g_lock_list .bss.g_safe_reset)
        *lib__idf_kernel_mempool.a:(.bss .bss.*)
        *lib__idf_kernel_misc.a:(EXCLUDE_FILE(*lib__idf_kernel_misc.a:oss_nv.*) .bss EXCLUDE_FILE(*lib__idf_kernel_misc.a:oss_nv.*) .bss.*)
        *lib__idf_kernel_misc.a:oss_nv.*(.bss .bss.g_close_sim1 .bss.g_softap_fac_nv .bss.g_softap_var_nv)
        *lib__idf_kernel_newlib.a:(.bss .bss.*)
        *lib__idf_kernel_os_FreeRTOS.a:(EXCLUDE_FILE(*lib__idf_kernel_os_FreeRTOS.a:port.* *lib__idf_kernel_os_FreeRTOS.a:tasks.*) .bss EXCLUDE_FILE(*lib__idf_kernel_os_FreeRTOS.a:port.* *lib__idf_kernel_os_FreeRTOS.a:tasks.*) .bss.*)
        *lib__idf_kernel_os_FreeRTOS.a:port.*(.bss .bss.ullNextTime)
        *lib__idf_kernel_os_FreeRTOS.a:tasks.*(.bss .bss.os_idle_running_time .bss.os_into_idle_count .bss.pxDelayedTaskList .bss.pxOverflowDelayedTaskList .bss.pxReadyTasksLists .bss.pxRunTimeTopsGroup .bss.ulTaskSwitchedInTime .bss.ulTotalRunTime .bss.uxCurrentNumberOfInterrupts .bss.uxCurrentNumberOfTasks .bss.uxDeletedTasksWaitingCleanUp .bss.uxIdleEntryTick .bss.uxInterruptRunTimeCounter .bss.uxLastGetMeassageTime .bss.uxTaskNumber .bss.uxTopReadyPriority .bss.xDelayedTaskList1 .bss.xDelayedTaskList2 .bss.xIdleTaskHandle .bss.xNextTaskUnblockTime .bss.xNumOfOverflows .bss.xPendedTicks .bss.xPendingReadyList .bss.xSuspendedTaskList .bss.xTasksWaitingTermination .bss.xTickCount .bss.xYieldPending)
        *lib__idf_kernel_osal.a:(EXCLUDE_FILE(*lib__idf_kernel_osal.a:osal_freertos_hook.*) .bss EXCLUDE_FILE(*lib__idf_kernel_osal.a:osal_freertos_hook.*) .bss.*)
        *lib__idf_kernel_osal.a:osal_freertos_hook.*(.bss .bss.Idle_TCB .bss.Timer_Stack.0 .bss.Timer_TCB.1)
        *lib__idf_kernel_posix.a:(.bss .bss.*)
        *lib__idf_kernel_prcm.a:(.bss .bss.*)
        *lib__idf_kernel_rc32k.a:(.bss .bss.*)
        *lib__idf_kernel_rtc.a:(.bss .bss.*)
        *lib__idf_kernel_special.a:(.bss .bss.*)
        *lib__idf_kernel_usbx.a:(.bss .bss.*)
        *libc.a:(.bss .bss.* .sbss .sbss.*)
        *libc_nano.a:(.bss .bss.* .sbss .sbss.*)
        *libcompress_jpeg.a:(.bss .bss.*)
        *libcompress_webp.a:(.bss .bss.*)
        *libgcc.a:(.bss .bss.* .sbss .sbss.*)
        *libm.a:(.bss .bss.* .sbss .sbss.*)
        *libm_nano.a:(.bss .bss.* .sbss .sbss.*)
        *libvision_meter_static.a:(.bss .bss.*)

        . = ALIGN(8);

        __psram_bss_end__ = . ;

        *lib__idf_kernel_diag_ctrl.a:diag_process.*(.bss.g_diag_buffer_area)
        *lib__idf_kernel_diag_ctrl.a:diag_process.*(.bss.g_diag_buffer_desc)
        *(.psram.common .psram.common.*)

        . = ALIGN(8);

        __psram_actual_bss_end__ = . ;

    } >PSRAM

    /* the start and end address of psram heap region. */
    _PSRAM_Heap_Begin = __psram_actual_bss_end__ ;
    _PSRAM_Heap_Limit = __PSRAM_Limit;


    /*
     * sysram sections
     */

    .text : ALIGN(8)
    {
        __stext = . ;

        KEEP(*(.vectors))

        *(EXCLUDE_FILE(__/opencpu/custom/custom_main/* *lib__idf_application_LBS.a *lib__idf_application_LED.a *lib__idf_application_SIF.a *lib__idf_application_SIF2.a *lib__idf_application_at_cmd.a *lib__idf_application_at_ctrl.a *lib__idf_application_atc_ps_cmd.a *lib__idf_application_cJSON.a *lib__idf_application_common.a *lib__idf_application_fota.a *lib__idf_application_fs.a *lib__idf_application_ftp.a *lib__idf_application_http.a *lib__idf_application_lock.a *lib__idf_application_mqtt.a *lib__idf_application_netdata.a *lib__idf_application_network.a *lib__idf_application_pin_ctl.a *lib__idf_application_ping.a *lib__idf_application_ppp.a *lib__idf_application_socket.a *lib__idf_application_socket_proxy.a *lib__idf_application_ssl_at_ssl.a *lib__idf_application_ssl_mbedtls-3.6.4.a *lib__idf_application_ssl_ssl_api.a *lib__idf_application_ssl_xy_port.a *lib__idf_application_water_mtr_ai.a *lib__idf_application_websocket.a *lib__idf_application_xy_version.a *lib__idf_cmiot.a *lib__idf_cmiot_api_application_cm_cot.a *lib__idf_cmiot_api_application_cm_fs.a *lib__idf_cmiot_api_application_cm_ftp.a *lib__idf_cmiot_api_application_cm_http.a *lib__idf_cmiot_api_application_cm_mqtt.a *lib__idf_cmiot_api_application_cm_ota.a *lib__idf_cmiot_api_application_cm_ping.a *lib__idf_cmiot_api_application_cm_sntp.a *lib__idf_cmiot_api_application_cm_ssl.a *lib__idf_cmiot_api_application_cm_utils.a *lib__idf_cmiot_api_driver_cm_adc.a *lib__idf_cmiot_api_driver_cm_gpio.a *lib__idf_cmiot_api_driver_cm_i2c.a *lib__idf_cmiot_api_driver_cm_iomux.a *lib__idf_cmiot_api_driver_cm_powerkey.a *lib__idf_cmiot_api_driver_cm_pwm.a *lib__idf_cmiot_api_driver_cm_rtc.a *lib__idf_cmiot_api_driver_cm_sif.a *lib__idf_cmiot_api_driver_cm_spi.a *lib__idf_cmiot_api_driver_cm_uart.a *lib__idf_cmiot_api_oss.a *lib__idf_cmiot_api_protocol_stack_cm_modem.a *lib__idf_cmiot_api_protocol_stack_cm_sim.a *lib__idf_cmiot_api_protocol_stack_cm_sms.a *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a *lib__idf_drivers.a *lib__idf_kernel_appstart.a *lib__idf_kernel_cmux.a *lib__idf_kernel_decompress.a *lib__idf_kernel_diag_ctrl.a *lib__idf_kernel_flash_daemon.a *lib__idf_kernel_inter_core.a *lib__idf_kernel_lowpower.a *lib__idf_kernel_mempool.a *lib__idf_kernel_misc.a *lib__idf_kernel_newlib.a *lib__idf_kernel_os_FreeRTOS.a *lib__idf_kernel_osal.a *lib__idf_kernel_posix.a *lib__idf_kernel_prcm.a *lib__idf_kernel_rc32k.a *lib__idf_kernel_rtc.a *lib__idf_kernel_special.a *lib__idf_kernel_usbx.a *libc.a *libc_nano.a *libcompress_jpeg.a *libcompress_webp.a *libgcc.a *libm.a *libm_nano.a *libvision_meter_static.a) .text EXCLUDE_FILE(__/opencpu/custom/custom_main/* *lib__idf_application_LBS.a *lib__idf_application_LED.a *lib__idf_application_SIF.a *lib__idf_application_SIF2.a *lib__idf_application_at_cmd.a *lib__idf_application_at_ctrl.a *lib__idf_application_atc_ps_cmd.a *lib__idf_application_cJSON.a *lib__idf_application_common.a *lib__idf_application_fota.a *lib__idf_application_fs.a *lib__idf_application_ftp.a *lib__idf_application_http.a *lib__idf_application_lock.a *lib__idf_application_mqtt.a *lib__idf_application_netdata.a *lib__idf_application_network.a *lib__idf_application_pin_ctl.a *lib__idf_application_ping.a *lib__idf_application_ppp.a *lib__idf_application_socket.a *lib__idf_application_socket_proxy.a *lib__idf_application_ssl_at_ssl.a *lib__idf_application_ssl_mbedtls-3.6.4.a *lib__idf_application_ssl_ssl_api.a *lib__idf_application_ssl_xy_port.a *lib__idf_application_water_mtr_ai.a *lib__idf_application_websocket.a *lib__idf_application_xy_version.a *lib__idf_cmiot.a *lib__idf_cmiot_api_application_cm_cot.a *lib__idf_cmiot_api_application_cm_fs.a *lib__idf_cmiot_api_application_cm_ftp.a *lib__idf_cmiot_api_application_cm_http.a *lib__idf_cmiot_api_application_cm_mqtt.a *lib__idf_cmiot_api_application_cm_ota.a *lib__idf_cmiot_api_application_cm_ping.a *lib__idf_cmiot_api_application_cm_sntp.a *lib__idf_cmiot_api_application_cm_ssl.a *lib__idf_cmiot_api_application_cm_utils.a *lib__idf_cmiot_api_driver_cm_adc.a *lib__idf_cmiot_api_driver_cm_gpio.a *lib__idf_cmiot_api_driver_cm_i2c.a *lib__idf_cmiot_api_driver_cm_iomux.a *lib__idf_cmiot_api_driver_cm_powerkey.a *lib__idf_cmiot_api_driver_cm_pwm.a *lib__idf_cmiot_api_driver_cm_rtc.a *lib__idf_cmiot_api_driver_cm_sif.a *lib__idf_cmiot_api_driver_cm_spi.a *lib__idf_cmiot_api_driver_cm_uart.a *lib__idf_cmiot_api_oss.a *lib__idf_cmiot_api_protocol_stack_cm_modem.a *lib__idf_cmiot_api_protocol_stack_cm_sim.a *lib__idf_cmiot_api_protocol_stack_cm_sms.a *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a *lib__idf_drivers.a *lib__idf_kernel_appstart.a *lib__idf_kernel_cmux.a *lib__idf_kernel_decompress.a *lib__idf_kernel_diag_ctrl.a *lib__idf_kernel_flash_daemon.a *lib__idf_kernel_inter_core.a *lib__idf_kernel_lowpower.a *lib__idf_kernel_mempool.a *lib__idf_kernel_misc.a *lib__idf_kernel_newlib.a *lib__idf_kernel_os_FreeRTOS.a *lib__idf_kernel_osal.a *lib__idf_kernel_posix.a *lib__idf_kernel_prcm.a *lib__idf_kernel_rc32k.a *lib__idf_kernel_rtc.a *lib__idf_kernel_special.a *lib__idf_kernel_usbx.a *libc.a *libc_nano.a *libcompress_jpeg.a *libcompress_webp.a *libgcc.a *libm.a *libm_nano.a *libvision_meter_static.a) .text.*)
        *lib__idf_drivers.a:driver_utilities.*(.text .text.*)
        *lib__idf_drivers.a:hal_cameraspi.*(.text.HAL_CAMERA_SPI_Sysram_Mode_IRQHandler)
        *lib__idf_drivers.a:hal_cameraspi.*(.text.YUV422ToRGB565)
        *lib__idf_drivers.a:usb_udc.*(.text.usb_request_set_feature_test_mode)
        *lib__idf_drivers.a:xy4100_ll_clock_driver.*(.text .text.*)
        *lib__idf_drivers.a:xy4100_ll_dma0.*(.text .text.*)
        *lib__idf_drivers.a:xy4100_ll_dma1.*(.text .text.*)
        *lib__idf_drivers.a:xy4100_ll_opi_flash.*(.text .text.*)
        *lib__idf_drivers.a:xy4100_ll_opi_psram.*(.text .text.*)
        *lib__idf_drivers.a:xy4100_ll_prcm.*(.text .text.*)
        *lib__idf_drivers.a:xy4100_ll_uart.*(.text .text.*)
        *lib__idf_kernel_appstart.a:msp.*(.text.change_pll1_clk)
        *lib__idf_kernel_appstart.a:msp.*(.text.switch_2_pll1)
        *lib__idf_kernel_appstart.a:msp.*(.text.switch_2_pll2)
        *lib__idf_kernel_lowpower.a:deepsleep.*(.text .text.DeepSleep_Fail_Power_Manage .text.DeepSleep_Power_Manage_SRAM .text.DeepSleep_Process .text.LL_PMU_PRCM_Disable_EnterSleep .text.LL_PMU_PRCM_Disable_Rc32k_Calib_Wkup .text.LL_PMU_PRCM_Disable_UTC_Wkup .text.csi_dcache_invalid_range)
        *lib__idf_kernel_lowpower.a:dual_wfi.*(.text .text.DualWfi_Power_Manage .text.DualWfi_Power_Recover .text.Dual_Wfi_Process)
        *lib__idf_kernel_lowpower.a:fast_recovery.*(.text .text.*)
        *lib__idf_kernel_lowpower.a:fast_recovery_asm.*(.text .text.*)
        *lib__idf_kernel_lowpower.a:idle_hook.*(.text .text.*)
        *lib__idf_kernel_lowpower.a:lightsleep.*(.text .text.LL_PMU_PRCM_Disable_EnterSleep .text.LL_PMU_PRCM_Enable_Ldopadrv_Pu .text.LL_PMU_PRCM_Set_Sido_LP .text.LL_PMU_PRCM_Set_Sido_Voltage_Sleep .text.LightSleep_Power_Manage .text.LightSleep_Power_Recover .text.LightSleep_Process .text.csi_dcache_clean_range.constprop.0)
        *lib__idf_kernel_lowpower.a:lpm_common.*(.text.PLL_Recover)
        *lib__idf_kernel_lowpower.a:lpm_common.*(.text.aquire_lpm_sema)
        *lib__idf_kernel_lowpower.a:lpm_common.*(.text.check_dma_status)
        *lib__idf_kernel_lowpower.a:lpm_common.*(.text.dfe_dbb_reset)
        *lib__idf_kernel_lowpower.a:lpm_common.*(.text.xtal_lp_2_hp)
        *lib__idf_kernel_lowpower.a:lpm_common.*(.text.xtal_recover)
        *lib__idf_kernel_lowpower.a:sleep.*(.text .text.EnLpuartWakupSleep .text.LL_PMU_PRCM_Disable_EnterSleep .text.LL_PMU_PRCM_Disable_Rc32k_Calib_Wkup .text.LL_PMU_PRCM_Disable_UTC_Wkup .text.LL_PMU_PRCM_Set_Sido_LP .text.LL_PMU_PRCM_Set_Sido_Voltage_Sleep .text.Sleep_Fail_Power_Recover .text.Sleep_Power_Manage_SRAM .text.Sleep_Process .text.csi_dcache_invalid_range.constprop.0)
        *lib__idf_kernel_lowpower.a:xy_lpm.*(.text.Soc_Enter_PD)
        *lib__idf_kernel_lowpower.a:xy_lpm.*(.text.do_reset)
        *lib__idf_kernel_os_FreeRTOS.a:port.*(.text.vPortEnterCritical)
        *lib__idf_kernel_os_FreeRTOS.a:port.*(.text.vPortExitCritical)
        *lib__idf_kernel_os_FreeRTOS.a:portASM.*(.text.Machine_Software_Handler)
        *lib__idf_kernel_os_FreeRTOS.a:tasks.*(.text.vTaskSwitchContext)
        *lib__idf_kernel_os_FreeRTOS.a:tasks.*(.text.xTaskGetCurrentTaskHandle)
        *lib__idf_kernel_os_FreeRTOS.a:tasks.*(.text.xTaskGetIdleTaskHandle)
        *lib__idf_kernel_os_FreeRTOS.a:tasks.*(.text.xTaskGetSchedulerState)
        *lib__idf_kernel_posix.a:lpuart_at_device.*(.text.lpuart_check_fifo)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.AON_PRCM_Clock_Disable_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.AON_PRCM_Clock_Enable_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.AON_PRCM_Release_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.AON_PRCM_Reset_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.CORE_PRCM_Clock_Disable_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.CORE_PRCM_Clock_Enable_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.CORE_PRCM_Release_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.CORE_PRCM_Reset_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.Clear_Bit_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.Get_AON_PRCM_Clock_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.Get_Bit_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.Get_CORE_PRCM_Clock_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.Set_Bit_Protect)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.get_aonprcm_sema_hardware)
        *lib__idf_kernel_prcm.a:xy_prcm.*(.text.put_aonprcm_sema_hardware)
        *lib__idf_kernel_rtc.a:rtc_utils.*(.text.utc_cnt_delay)

        /* for cache line aligned, the code and data are no longer in the same cache line. */
        . = ALIGN(0x40) ;

        __sysram_text_end__ = . ;

    } >SYSRAM

    .rodata : ALIGN(8)
    {
        __rodata_start__ = . ;

        *(EXCLUDE_FILE(__/opencpu/custom/custom_main/* *lib__idf_application_LBS.a *lib__idf_application_LED.a *lib__idf_application_SIF.a *lib__idf_application_SIF2.a *lib__idf_application_at_cmd.a *lib__idf_application_at_ctrl.a *lib__idf_application_atc_ps_cmd.a *lib__idf_application_cJSON.a *lib__idf_application_common.a *lib__idf_application_fota.a *lib__idf_application_fs.a *lib__idf_application_ftp.a *lib__idf_application_http.a *lib__idf_application_lock.a *lib__idf_application_mqtt.a *lib__idf_application_netdata.a *lib__idf_application_network.a *lib__idf_application_pin_ctl.a *lib__idf_application_ping.a *lib__idf_application_ppp.a *lib__idf_application_socket.a *lib__idf_application_socket_proxy.a *lib__idf_application_ssl_at_ssl.a *lib__idf_application_ssl_mbedtls-3.6.4.a *lib__idf_application_ssl_ssl_api.a *lib__idf_application_ssl_xy_port.a *lib__idf_application_water_mtr_ai.a *lib__idf_application_websocket.a *lib__idf_application_xy_version.a *lib__idf_cmiot.a *lib__idf_cmiot_api_application_cm_cot.a *lib__idf_cmiot_api_application_cm_fs.a *lib__idf_cmiot_api_application_cm_ftp.a *lib__idf_cmiot_api_application_cm_http.a *lib__idf_cmiot_api_application_cm_mqtt.a *lib__idf_cmiot_api_application_cm_ota.a *lib__idf_cmiot_api_application_cm_ping.a *lib__idf_cmiot_api_application_cm_sntp.a *lib__idf_cmiot_api_application_cm_ssl.a *lib__idf_cmiot_api_application_cm_utils.a *lib__idf_cmiot_api_driver_cm_adc.a *lib__idf_cmiot_api_driver_cm_gpio.a *lib__idf_cmiot_api_driver_cm_i2c.a *lib__idf_cmiot_api_driver_cm_iomux.a *lib__idf_cmiot_api_driver_cm_powerkey.a *lib__idf_cmiot_api_driver_cm_pwm.a *lib__idf_cmiot_api_driver_cm_rtc.a *lib__idf_cmiot_api_driver_cm_sif.a *lib__idf_cmiot_api_driver_cm_spi.a *lib__idf_cmiot_api_driver_cm_uart.a *lib__idf_cmiot_api_oss.a *lib__idf_cmiot_api_protocol_stack_cm_modem.a *lib__idf_cmiot_api_protocol_stack_cm_sim.a *lib__idf_cmiot_api_protocol_stack_cm_sms.a *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a *lib__idf_drivers.a *lib__idf_kernel_appstart.a *lib__idf_kernel_cmux.a *lib__idf_kernel_decompress.a *lib__idf_kernel_diag_ctrl.a *lib__idf_kernel_flash_daemon.a *lib__idf_kernel_inter_core.a *lib__idf_kernel_lowpower.a *lib__idf_kernel_mempool.a *lib__idf_kernel_misc.a *lib__idf_kernel_newlib.a *lib__idf_kernel_os_FreeRTOS.a *lib__idf_kernel_osal.a *lib__idf_kernel_posix.a *lib__idf_kernel_prcm.a *lib__idf_kernel_rc32k.a *lib__idf_kernel_rtc.a *lib__idf_kernel_special.a *lib__idf_kernel_usbx.a *libcompress_jpeg.a *libcompress_webp.a *libvision_meter_static.a) .constdata EXCLUDE_FILE(__/opencpu/custom/custom_main/* *lib__idf_application_LBS.a *lib__idf_application_LED.a *lib__idf_application_SIF.a *lib__idf_application_SIF2.a *lib__idf_application_at_cmd.a *lib__idf_application_at_ctrl.a *lib__idf_application_atc_ps_cmd.a *lib__idf_application_cJSON.a *lib__idf_application_common.a *lib__idf_application_fota.a *lib__idf_application_fs.a *lib__idf_application_ftp.a *lib__idf_application_http.a *lib__idf_application_lock.a *lib__idf_application_mqtt.a *lib__idf_application_netdata.a *lib__idf_application_network.a *lib__idf_application_pin_ctl.a *lib__idf_application_ping.a *lib__idf_application_ppp.a *lib__idf_application_socket.a *lib__idf_application_socket_proxy.a *lib__idf_application_ssl_at_ssl.a *lib__idf_application_ssl_mbedtls-3.6.4.a *lib__idf_application_ssl_ssl_api.a *lib__idf_application_ssl_xy_port.a *lib__idf_application_water_mtr_ai.a *lib__idf_application_websocket.a *lib__idf_application_xy_version.a *lib__idf_cmiot.a *lib__idf_cmiot_api_application_cm_cot.a *lib__idf_cmiot_api_application_cm_fs.a *lib__idf_cmiot_api_application_cm_ftp.a *lib__idf_cmiot_api_application_cm_http.a *lib__idf_cmiot_api_application_cm_mqtt.a *lib__idf_cmiot_api_application_cm_ota.a *lib__idf_cmiot_api_application_cm_ping.a *lib__idf_cmiot_api_application_cm_sntp.a *lib__idf_cmiot_api_application_cm_ssl.a *lib__idf_cmiot_api_application_cm_utils.a *lib__idf_cmiot_api_driver_cm_adc.a *lib__idf_cmiot_api_driver_cm_gpio.a *lib__idf_cmiot_api_driver_cm_i2c.a *lib__idf_cmiot_api_driver_cm_iomux.a *lib__idf_cmiot_api_driver_cm_powerkey.a *lib__idf_cmiot_api_driver_cm_pwm.a *lib__idf_cmiot_api_driver_cm_rtc.a *lib__idf_cmiot_api_driver_cm_sif.a *lib__idf_cmiot_api_driver_cm_spi.a *lib__idf_cmiot_api_driver_cm_uart.a *lib__idf_cmiot_api_oss.a *lib__idf_cmiot_api_protocol_stack_cm_modem.a *lib__idf_cmiot_api_protocol_stack_cm_sim.a *lib__idf_cmiot_api_protocol_stack_cm_sms.a *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a *lib__idf_drivers.a *lib__idf_kernel_appstart.a *lib__idf_kernel_cmux.a *lib__idf_kernel_decompress.a *lib__idf_kernel_diag_ctrl.a *lib__idf_kernel_flash_daemon.a *lib__idf_kernel_inter_core.a *lib__idf_kernel_lowpower.a *lib__idf_kernel_mempool.a *lib__idf_kernel_misc.a *lib__idf_kernel_newlib.a *lib__idf_kernel_os_FreeRTOS.a *lib__idf_kernel_osal.a *lib__idf_kernel_posix.a *lib__idf_kernel_prcm.a *lib__idf_kernel_rc32k.a *lib__idf_kernel_rtc.a *lib__idf_kernel_special.a *lib__idf_kernel_usbx.a *libcompress_jpeg.a *libcompress_webp.a *libvision_meter_static.a) .constdata.* EXCLUDE_FILE(__/opencpu/custom/custom_main/* *lib__idf_application_LBS.a *lib__idf_application_LED.a *lib__idf_application_SIF.a *lib__idf_application_SIF2.a *lib__idf_application_at_cmd.a *lib__idf_application_at_ctrl.a *lib__idf_application_atc_ps_cmd.a *lib__idf_application_cJSON.a *lib__idf_application_common.a *lib__idf_application_fota.a *lib__idf_application_fs.a *lib__idf_application_ftp.a *lib__idf_application_http.a *lib__idf_application_lock.a *lib__idf_application_mqtt.a *lib__idf_application_netdata.a *lib__idf_application_network.a *lib__idf_application_pin_ctl.a *lib__idf_application_ping.a *lib__idf_application_ppp.a *lib__idf_application_socket.a *lib__idf_application_socket_proxy.a *lib__idf_application_ssl_at_ssl.a *lib__idf_application_ssl_mbedtls-3.6.4.a *lib__idf_application_ssl_ssl_api.a *lib__idf_application_ssl_xy_port.a *lib__idf_application_water_mtr_ai.a *lib__idf_application_websocket.a *lib__idf_application_xy_version.a *lib__idf_cmiot.a *lib__idf_cmiot_api_application_cm_cot.a *lib__idf_cmiot_api_application_cm_fs.a *lib__idf_cmiot_api_application_cm_ftp.a *lib__idf_cmiot_api_application_cm_http.a *lib__idf_cmiot_api_application_cm_mqtt.a *lib__idf_cmiot_api_application_cm_ota.a *lib__idf_cmiot_api_application_cm_ping.a *lib__idf_cmiot_api_application_cm_sntp.a *lib__idf_cmiot_api_application_cm_ssl.a *lib__idf_cmiot_api_application_cm_utils.a *lib__idf_cmiot_api_driver_cm_adc.a *lib__idf_cmiot_api_driver_cm_gpio.a *lib__idf_cmiot_api_driver_cm_i2c.a *lib__idf_cmiot_api_driver_cm_iomux.a *lib__idf_cmiot_api_driver_cm_powerkey.a *lib__idf_cmiot_api_driver_cm_pwm.a *lib__idf_cmiot_api_driver_cm_rtc.a *lib__idf_cmiot_api_driver_cm_sif.a *lib__idf_cmiot_api_driver_cm_spi.a *lib__idf_cmiot_api_driver_cm_uart.a *lib__idf_cmiot_api_oss.a *lib__idf_cmiot_api_protocol_stack_cm_modem.a *lib__idf_cmiot_api_protocol_stack_cm_sim.a *lib__idf_cmiot_api_protocol_stack_cm_sms.a *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a *lib__idf_drivers.a *lib__idf_kernel_appstart.a *lib__idf_kernel_cmux.a *lib__idf_kernel_decompress.a *lib__idf_kernel_diag_ctrl.a *lib__idf_kernel_flash_daemon.a *lib__idf_kernel_inter_core.a *lib__idf_kernel_lowpower.a *lib__idf_kernel_mempool.a *lib__idf_kernel_misc.a *lib__idf_kernel_newlib.a *lib__idf_kernel_os_FreeRTOS.a *lib__idf_kernel_osal.a *lib__idf_kernel_posix.a *lib__idf_kernel_prcm.a *lib__idf_kernel_rc32k.a *lib__idf_kernel_rtc.a *lib__idf_kernel_special.a *lib__idf_kernel_usbx.a *libcompress_jpeg.a *libcompress_webp.a *libvision_meter_static.a) .rodata EXCLUDE_FILE(__/opencpu/custom/custom_main/* *lib__idf_application_LBS.a *lib__idf_application_LED.a *lib__idf_application_SIF.a *lib__idf_application_SIF2.a *lib__idf_application_at_cmd.a *lib__idf_application_at_ctrl.a *lib__idf_application_atc_ps_cmd.a *lib__idf_application_cJSON.a *lib__idf_application_common.a *lib__idf_application_fota.a *lib__idf_application_fs.a *lib__idf_application_ftp.a *lib__idf_application_http.a *lib__idf_application_lock.a *lib__idf_application_mqtt.a *lib__idf_application_netdata.a *lib__idf_application_network.a *lib__idf_application_pin_ctl.a *lib__idf_application_ping.a *lib__idf_application_ppp.a *lib__idf_application_socket.a *lib__idf_application_socket_proxy.a *lib__idf_application_ssl_at_ssl.a *lib__idf_application_ssl_mbedtls-3.6.4.a *lib__idf_application_ssl_ssl_api.a *lib__idf_application_ssl_xy_port.a *lib__idf_application_water_mtr_ai.a *lib__idf_application_websocket.a *lib__idf_application_xy_version.a *lib__idf_cmiot.a *lib__idf_cmiot_api_application_cm_cot.a *lib__idf_cmiot_api_application_cm_fs.a *lib__idf_cmiot_api_application_cm_ftp.a *lib__idf_cmiot_api_application_cm_http.a *lib__idf_cmiot_api_application_cm_mqtt.a *lib__idf_cmiot_api_application_cm_ota.a *lib__idf_cmiot_api_application_cm_ping.a *lib__idf_cmiot_api_application_cm_sntp.a *lib__idf_cmiot_api_application_cm_ssl.a *lib__idf_cmiot_api_application_cm_utils.a *lib__idf_cmiot_api_driver_cm_adc.a *lib__idf_cmiot_api_driver_cm_gpio.a *lib__idf_cmiot_api_driver_cm_i2c.a *lib__idf_cmiot_api_driver_cm_iomux.a *lib__idf_cmiot_api_driver_cm_powerkey.a *lib__idf_cmiot_api_driver_cm_pwm.a *lib__idf_cmiot_api_driver_cm_rtc.a *lib__idf_cmiot_api_driver_cm_sif.a *lib__idf_cmiot_api_driver_cm_spi.a *lib__idf_cmiot_api_driver_cm_uart.a *lib__idf_cmiot_api_oss.a *lib__idf_cmiot_api_protocol_stack_cm_modem.a *lib__idf_cmiot_api_protocol_stack_cm_sim.a *lib__idf_cmiot_api_protocol_stack_cm_sms.a *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a *lib__idf_drivers.a *lib__idf_kernel_appstart.a *lib__idf_kernel_cmux.a *lib__idf_kernel_decompress.a *lib__idf_kernel_diag_ctrl.a *lib__idf_kernel_flash_daemon.a *lib__idf_kernel_inter_core.a *lib__idf_kernel_lowpower.a *lib__idf_kernel_mempool.a *lib__idf_kernel_misc.a *lib__idf_kernel_newlib.a *lib__idf_kernel_os_FreeRTOS.a *lib__idf_kernel_osal.a *lib__idf_kernel_posix.a *lib__idf_kernel_prcm.a *lib__idf_kernel_rc32k.a *lib__idf_kernel_rtc.a *lib__idf_kernel_special.a *lib__idf_kernel_usbx.a *libcompress_jpeg.a *libcompress_webp.a *libvision_meter_static.a) .rodata.*)
        *lib__idf_drivers.a:driver_utilities.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_drivers.a:xy4100_ll_clock_driver.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_drivers.a:xy4100_ll_dma0.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_drivers.a:xy4100_ll_dma1.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_drivers.a:xy4100_ll_opi_flash.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_drivers.a:xy4100_ll_opi_psram.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_drivers.a:xy4100_ll_prcm.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_drivers.a:xy4100_ll_uart.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_lowpower.a:deepsleep.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_lowpower.a:dual_wfi.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_lowpower.a:fast_recovery.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_lowpower.a:fast_recovery_asm.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_lowpower.a:idle_hook.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_lowpower.a:lightsleep.*(.constdata .constdata.* .rodata .rodata.*)
        *lib__idf_kernel_lowpower.a:sleep.*(.constdata .constdata.* .rodata .rodata.*)

        . = ALIGN(8) ;

        __rodata_end__ = . ;

    } >SYSRAM

    _sidata = LOADADDR(.data);

    .data : ALIGN(8)
    {
        FILL(0xFF)
        __data_start__ = . ;

        /* RISC-V global point register (gp) used, initialised gp register as
        this values and never change it again in current running. */
        __global_pointer$ = . + 0x800 ;

        *(.data_begin .data_begin.*)

        *(EXCLUDE_FILE(__/opencpu/custom/custom_main/* *lib__idf_application_LBS.a *lib__idf_application_LED.a *lib__idf_application_SIF.a *lib__idf_application_SIF2.a *lib__idf_application_at_cmd.a *lib__idf_application_at_ctrl.a *lib__idf_application_atc_ps_cmd.a *lib__idf_application_cJSON.a *lib__idf_application_common.a *lib__idf_application_fota.a *lib__idf_application_fs.a *lib__idf_application_ftp.a *lib__idf_application_http.a *lib__idf_application_lock.a *lib__idf_application_mqtt.a *lib__idf_application_netdata.a *lib__idf_application_network.a *lib__idf_application_pin_ctl.a *lib__idf_application_ping.a *lib__idf_application_ppp.a *lib__idf_application_socket.a *lib__idf_application_socket_proxy.a *lib__idf_application_ssl_at_ssl.a *lib__idf_application_ssl_mbedtls-3.6.4.a *lib__idf_application_ssl_ssl_api.a *lib__idf_application_ssl_xy_port.a *lib__idf_application_water_mtr_ai.a *lib__idf_application_websocket.a *lib__idf_application_xy_version.a *lib__idf_cmiot.a *lib__idf_cmiot_api_application_cm_cot.a *lib__idf_cmiot_api_application_cm_fs.a *lib__idf_cmiot_api_application_cm_ftp.a *lib__idf_cmiot_api_application_cm_http.a *lib__idf_cmiot_api_application_cm_mqtt.a *lib__idf_cmiot_api_application_cm_ota.a *lib__idf_cmiot_api_application_cm_ping.a *lib__idf_cmiot_api_application_cm_sntp.a *lib__idf_cmiot_api_application_cm_ssl.a *lib__idf_cmiot_api_application_cm_utils.a *lib__idf_cmiot_api_driver_cm_adc.a *lib__idf_cmiot_api_driver_cm_gpio.a *lib__idf_cmiot_api_driver_cm_i2c.a *lib__idf_cmiot_api_driver_cm_iomux.a *lib__idf_cmiot_api_driver_cm_powerkey.a *lib__idf_cmiot_api_driver_cm_pwm.a *lib__idf_cmiot_api_driver_cm_rtc.a *lib__idf_cmiot_api_driver_cm_sif.a *lib__idf_cmiot_api_driver_cm_spi.a *lib__idf_cmiot_api_driver_cm_uart.a *lib__idf_cmiot_api_oss.a *lib__idf_cmiot_api_protocol_stack_cm_modem.a *lib__idf_cmiot_api_protocol_stack_cm_sim.a *lib__idf_cmiot_api_protocol_stack_cm_sms.a *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a *lib__idf_drivers.a *lib__idf_kernel_appstart.a *lib__idf_kernel_cmux.a *lib__idf_kernel_decompress.a *lib__idf_kernel_diag_ctrl.a *lib__idf_kernel_flash_daemon.a *lib__idf_kernel_inter_core.a *lib__idf_kernel_lowpower.a *lib__idf_kernel_mempool.a *lib__idf_kernel_misc.a *lib__idf_kernel_newlib.a *lib__idf_kernel_os_FreeRTOS.a *lib__idf_kernel_osal.a *lib__idf_kernel_posix.a *lib__idf_kernel_prcm.a *lib__idf_kernel_rc32k.a *lib__idf_kernel_rtc.a *lib__idf_kernel_special.a *lib__idf_kernel_usbx.a *libcompress_jpeg.a *libcompress_webp.a *libvision_meter_static.a) .data EXCLUDE_FILE(__/opencpu/custom/custom_main/* *lib__idf_application_LBS.a *lib__idf_application_LED.a *lib__idf_application_SIF.a *lib__idf_application_SIF2.a *lib__idf_application_at_cmd.a *lib__idf_application_at_ctrl.a *lib__idf_application_atc_ps_cmd.a *lib__idf_application_cJSON.a *lib__idf_application_common.a *lib__idf_application_fota.a *lib__idf_application_fs.a *lib__idf_application_ftp.a *lib__idf_application_http.a *lib__idf_application_lock.a *lib__idf_application_mqtt.a *lib__idf_application_netdata.a *lib__idf_application_network.a *lib__idf_application_pin_ctl.a *lib__idf_application_ping.a *lib__idf_application_ppp.a *lib__idf_application_socket.a *lib__idf_application_socket_proxy.a *lib__idf_application_ssl_at_ssl.a *lib__idf_application_ssl_mbedtls-3.6.4.a *lib__idf_application_ssl_ssl_api.a *lib__idf_application_ssl_xy_port.a *lib__idf_application_water_mtr_ai.a *lib__idf_application_websocket.a *lib__idf_application_xy_version.a *lib__idf_cmiot.a *lib__idf_cmiot_api_application_cm_cot.a *lib__idf_cmiot_api_application_cm_fs.a *lib__idf_cmiot_api_application_cm_ftp.a *lib__idf_cmiot_api_application_cm_http.a *lib__idf_cmiot_api_application_cm_mqtt.a *lib__idf_cmiot_api_application_cm_ota.a *lib__idf_cmiot_api_application_cm_ping.a *lib__idf_cmiot_api_application_cm_sntp.a *lib__idf_cmiot_api_application_cm_ssl.a *lib__idf_cmiot_api_application_cm_utils.a *lib__idf_cmiot_api_driver_cm_adc.a *lib__idf_cmiot_api_driver_cm_gpio.a *lib__idf_cmiot_api_driver_cm_i2c.a *lib__idf_cmiot_api_driver_cm_iomux.a *lib__idf_cmiot_api_driver_cm_powerkey.a *lib__idf_cmiot_api_driver_cm_pwm.a *lib__idf_cmiot_api_driver_cm_rtc.a *lib__idf_cmiot_api_driver_cm_sif.a *lib__idf_cmiot_api_driver_cm_spi.a *lib__idf_cmiot_api_driver_cm_uart.a *lib__idf_cmiot_api_oss.a *lib__idf_cmiot_api_protocol_stack_cm_modem.a *lib__idf_cmiot_api_protocol_stack_cm_sim.a *lib__idf_cmiot_api_protocol_stack_cm_sms.a *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a *lib__idf_drivers.a *lib__idf_kernel_appstart.a *lib__idf_kernel_cmux.a *lib__idf_kernel_decompress.a *lib__idf_kernel_diag_ctrl.a *lib__idf_kernel_flash_daemon.a *lib__idf_kernel_inter_core.a *lib__idf_kernel_lowpower.a *lib__idf_kernel_mempool.a *lib__idf_kernel_misc.a *lib__idf_kernel_newlib.a *lib__idf_kernel_os_FreeRTOS.a *lib__idf_kernel_osal.a *lib__idf_kernel_posix.a *lib__idf_kernel_prcm.a *lib__idf_kernel_rc32k.a *lib__idf_kernel_rtc.a *lib__idf_kernel_special.a *lib__idf_kernel_usbx.a *libcompress_jpeg.a *libcompress_webp.a *libvision_meter_static.a) .data.*)
        *lib__idf_drivers.a:driver_utilities.*(.data .data.*)
        *lib__idf_drivers.a:xy4100_ll_clock_driver.*(.data .data.*)
        *lib__idf_drivers.a:xy4100_ll_dma0.*(.data .data.*)
        *lib__idf_drivers.a:xy4100_ll_dma1.*(.data .data.*)
        *lib__idf_drivers.a:xy4100_ll_opi_flash.*(.data .data.*)
        *lib__idf_drivers.a:xy4100_ll_opi_psram.*(.data .data.*)
        *lib__idf_drivers.a:xy4100_ll_prcm.*(.data .data.*)
        *lib__idf_drivers.a:xy4100_ll_uart.*(.data .data.*)
        *lib__idf_drivers.a:xy_dma.*(.data.gDmaChannelMap)
        *lib__idf_kernel_lowpower.a:deepsleep.*(.data .data.*)
        *lib__idf_kernel_lowpower.a:dual_wfi.*(.data .data.*)
        *lib__idf_kernel_lowpower.a:fast_recovery.*(.data .data.*)
        *lib__idf_kernel_lowpower.a:fast_recovery_asm.*(.data .data.*)
        *lib__idf_kernel_lowpower.a:idle_hook.*(.data .data.*)
        *lib__idf_kernel_lowpower.a:lightsleep.*(.data .data.*)
        *lib__idf_kernel_lowpower.a:sleep.*(.data .data.*)

        *(.data_end .data_end.*)

        . = ALIGN(8);
        __data_end__ = . ;

    } >SYSRAM

    /* Check sysram limit, Cannot exceed to the secondary boot area. */
    ASSERT(__data_end__ < 0x10010000, "sysram exceed to the secondary boot area.")

    .bss (NOLOAD) : ALIGN(8)
    {
        __bss_start__ = . ;
        *(.bss_begin .bss_begin.*)

        *(EXCLUDE_FILE(__/opencpu/custom/custom_main/* *lib__idf_application_LBS.a *lib__idf_application_LED.a *lib__idf_application_SIF.a *lib__idf_application_SIF2.a *lib__idf_application_at_cmd.a *lib__idf_application_at_ctrl.a *lib__idf_application_atc_ps_cmd.a *lib__idf_application_cJSON.a *lib__idf_application_common.a *lib__idf_application_fota.a *lib__idf_application_fs.a *lib__idf_application_ftp.a *lib__idf_application_http.a *lib__idf_application_lock.a *lib__idf_application_mqtt.a *lib__idf_application_netdata.a *lib__idf_application_network.a *lib__idf_application_pin_ctl.a *lib__idf_application_ping.a *lib__idf_application_ppp.a *lib__idf_application_socket.a *lib__idf_application_socket_proxy.a *lib__idf_application_ssl_at_ssl.a *lib__idf_application_ssl_mbedtls-3.6.4.a *lib__idf_application_ssl_ssl_api.a *lib__idf_application_ssl_xy_port.a *lib__idf_application_water_mtr_ai.a *lib__idf_application_websocket.a *lib__idf_application_xy_version.a *lib__idf_cmiot.a *lib__idf_cmiot_api_application_cm_cot.a *lib__idf_cmiot_api_application_cm_fs.a *lib__idf_cmiot_api_application_cm_ftp.a *lib__idf_cmiot_api_application_cm_http.a *lib__idf_cmiot_api_application_cm_mqtt.a *lib__idf_cmiot_api_application_cm_ota.a *lib__idf_cmiot_api_application_cm_ping.a *lib__idf_cmiot_api_application_cm_sntp.a *lib__idf_cmiot_api_application_cm_ssl.a *lib__idf_cmiot_api_application_cm_utils.a *lib__idf_cmiot_api_driver_cm_adc.a *lib__idf_cmiot_api_driver_cm_gpio.a *lib__idf_cmiot_api_driver_cm_i2c.a *lib__idf_cmiot_api_driver_cm_iomux.a *lib__idf_cmiot_api_driver_cm_powerkey.a *lib__idf_cmiot_api_driver_cm_pwm.a *lib__idf_cmiot_api_driver_cm_rtc.a *lib__idf_cmiot_api_driver_cm_sif.a *lib__idf_cmiot_api_driver_cm_spi.a *lib__idf_cmiot_api_driver_cm_uart.a *lib__idf_cmiot_api_oss.a *lib__idf_cmiot_api_protocol_stack_cm_modem.a *lib__idf_cmiot_api_protocol_stack_cm_sim.a *lib__idf_cmiot_api_protocol_stack_cm_sms.a *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a *lib__idf_drivers.a *lib__idf_kernel_appstart.a *lib__idf_kernel_cmux.a *lib__idf_kernel_decompress.a *lib__idf_kernel_diag_ctrl.a *lib__idf_kernel_flash_daemon.a *lib__idf_kernel_inter_core.a *lib__idf_kernel_lowpower.a *lib__idf_kernel_mempool.a *lib__idf_kernel_misc.a *lib__idf_kernel_newlib.a *lib__idf_kernel_os_FreeRTOS.a *lib__idf_kernel_osal.a *lib__idf_kernel_posix.a *lib__idf_kernel_prcm.a *lib__idf_kernel_rc32k.a *lib__idf_kernel_rtc.a *lib__idf_kernel_special.a *lib__idf_kernel_usbx.a *libcompress_jpeg.a *libcompress_webp.a *libvision_meter_static.a) .bss EXCLUDE_FILE(__/opencpu/custom/custom_main/* *lib__idf_application_LBS.a *lib__idf_application_LED.a *lib__idf_application_SIF.a *lib__idf_application_SIF2.a *lib__idf_application_at_cmd.a *lib__idf_application_at_ctrl.a *lib__idf_application_atc_ps_cmd.a *lib__idf_application_cJSON.a *lib__idf_application_common.a *lib__idf_application_fota.a *lib__idf_application_fs.a *lib__idf_application_ftp.a *lib__idf_application_http.a *lib__idf_application_lock.a *lib__idf_application_mqtt.a *lib__idf_application_netdata.a *lib__idf_application_network.a *lib__idf_application_pin_ctl.a *lib__idf_application_ping.a *lib__idf_application_ppp.a *lib__idf_application_socket.a *lib__idf_application_socket_proxy.a *lib__idf_application_ssl_at_ssl.a *lib__idf_application_ssl_mbedtls-3.6.4.a *lib__idf_application_ssl_ssl_api.a *lib__idf_application_ssl_xy_port.a *lib__idf_application_water_mtr_ai.a *lib__idf_application_websocket.a *lib__idf_application_xy_version.a *lib__idf_cmiot.a *lib__idf_cmiot_api_application_cm_cot.a *lib__idf_cmiot_api_application_cm_fs.a *lib__idf_cmiot_api_application_cm_ftp.a *lib__idf_cmiot_api_application_cm_http.a *lib__idf_cmiot_api_application_cm_mqtt.a *lib__idf_cmiot_api_application_cm_ota.a *lib__idf_cmiot_api_application_cm_ping.a *lib__idf_cmiot_api_application_cm_sntp.a *lib__idf_cmiot_api_application_cm_ssl.a *lib__idf_cmiot_api_application_cm_utils.a *lib__idf_cmiot_api_driver_cm_adc.a *lib__idf_cmiot_api_driver_cm_gpio.a *lib__idf_cmiot_api_driver_cm_i2c.a *lib__idf_cmiot_api_driver_cm_iomux.a *lib__idf_cmiot_api_driver_cm_powerkey.a *lib__idf_cmiot_api_driver_cm_pwm.a *lib__idf_cmiot_api_driver_cm_rtc.a *lib__idf_cmiot_api_driver_cm_sif.a *lib__idf_cmiot_api_driver_cm_spi.a *lib__idf_cmiot_api_driver_cm_uart.a *lib__idf_cmiot_api_oss.a *lib__idf_cmiot_api_protocol_stack_cm_modem.a *lib__idf_cmiot_api_protocol_stack_cm_sim.a *lib__idf_cmiot_api_protocol_stack_cm_sms.a *lib__idf_cmiot_api_protocol_stack_cm_virt_at.a *lib__idf_drivers.a *lib__idf_kernel_appstart.a *lib__idf_kernel_cmux.a *lib__idf_kernel_decompress.a *lib__idf_kernel_diag_ctrl.a *lib__idf_kernel_flash_daemon.a *lib__idf_kernel_inter_core.a *lib__idf_kernel_lowpower.a *lib__idf_kernel_mempool.a *lib__idf_kernel_misc.a *lib__idf_kernel_newlib.a *lib__idf_kernel_os_FreeRTOS.a *lib__idf_kernel_osal.a *lib__idf_kernel_posix.a *lib__idf_kernel_prcm.a *lib__idf_kernel_rc32k.a *lib__idf_kernel_rtc.a *lib__idf_kernel_special.a *lib__idf_kernel_usbx.a *libcompress_jpeg.a *libcompress_webp.a *libvision_meter_static.a) .bss.*)
        *lib__idf_drivers.a:driver_utilities.*(.bss .bss.*)
        *lib__idf_drivers.a:hal_cameraspi.*(.bss.A_Line_Addr_at_Sysram)
        *lib__idf_drivers.a:hal_cameraspi.*(.bss.B_Line_Addr_at_Sysram)
        *lib__idf_drivers.a:xy4100_ll_clock_driver.*(.bss .bss.*)
        *lib__idf_drivers.a:xy4100_ll_dma0.*(.bss .bss.*)
        *lib__idf_drivers.a:xy4100_ll_dma1.*(.bss .bss.*)
        *lib__idf_drivers.a:xy4100_ll_opi_flash.*(.bss .bss.*)
        *lib__idf_drivers.a:xy4100_ll_opi_psram.*(.bss .bss.*)
        *lib__idf_drivers.a:xy4100_ll_prcm.*(.bss .bss.*)
        *lib__idf_drivers.a:xy4100_ll_uart.*(.bss .bss.*)
        *lib__idf_kernel_lowpower.a:deepsleep.*(.bss .bss.*)
        *lib__idf_kernel_lowpower.a:dual_wfi.*(.bss .bss.*)
        *lib__idf_kernel_lowpower.a:fast_recovery.*(.bss .bss.*)
        *lib__idf_kernel_lowpower.a:fast_recovery_asm.*(.bss .bss.*)
        *lib__idf_kernel_lowpower.a:idle_hook.*(.bss .bss.*)
        *lib__idf_kernel_lowpower.a:lightsleep.*(.bss .bss.*)
        *lib__idf_kernel_lowpower.a:sleep.*(.bss .bss.*)
        *lib__idf_kernel_lowpower.a:xy_lpm.*(.bss.control_hrc_lightsleep_flag)
        *lib__idf_kernel_lowpower.a:xy_lpm.*(.bss.g_DSLP_off_RtcWakup)
        *lib__idf_kernel_lowpower.a:xy_lpm.*(.bss.g_enter_pd)
        *lib__idf_kernel_lowpower.a:xy_lpm.*(.bss.g_sleep_gpio_ret_mode)
        *lib__idf_kernel_misc.a:oss_nv.*(.bss.gLpuartWakSleepEn)
        *lib__idf_kernel_misc.a:oss_nv.*(.bss.g_EnWdtForPD)
        *lib__idf_kernel_misc.a:oss_nv.*(.bss.g_io_wakeup_enable)
        *lib__idf_kernel_misc.a:oss_nv.*(.bss.g_sim_efuse)
        *lib__idf_kernel_misc.a:oss_nv.*(.bss.g_usb_mode)
        *lib__idf_kernel_misc.a:oss_nv.*(.bss.g_vbat_mode)
        *lib__idf_kernel_os_FreeRTOS.a:port.*(.bss.uxCriticalNesting)
        *lib__idf_kernel_os_FreeRTOS.a:port.*(.bss.uxInterruptState)
        *lib__idf_kernel_os_FreeRTOS.a:port.*(.bss.uxUserCriticalNesting)
        *lib__idf_kernel_os_FreeRTOS.a:tasks.*(.bss.pxCurrentTCB)
        *lib__idf_kernel_os_FreeRTOS.a:tasks.*(.bss.uxSchedulerSuspended)
        *lib__idf_kernel_os_FreeRTOS.a:tasks.*(.bss.xSchedulerRunning)

        *(.bss_end .bss_end.*)
        . = ALIGN(8);

        __bss_end__ = . ;

        *lib__idf_kernel_inter_core.a:ic_port.*(.bss.Icc_Stack)
        *lib__idf_kernel_osal.a:osal_freertos_hook.*(.bss.Idle_Stack)
        *(.sysram.common .sysram.common.*)
        . = ALIGN(8);

        __actual_bss_end__ = . ;

    } >SYSRAM

    .noinit (NOLOAD) : ALIGN(8)
    {
        _noinit = . ;

        *(.noinit .noinit.*)

        . = ALIGN(8) ;

        _end_noinit = . ;
    } > SYSRAM

    /* Mandatory to be word aligned, _sbrk assumes this */
    PROVIDE ( end = _end_noinit ); /* was _ebss */
    PROVIDE ( _end = _end_noinit );
    PROVIDE ( __end = _end_noinit );
    PROVIDE ( __end__ = _end_noinit );

    .heap (NOLOAD) : ALIGN(8)
    {
        __heap_start__ = . ;

        *(.heap .heap.*)

        . = ALIGN(8);

        __heap_end__ = . ;
    } >SYSRAM

    /*
     * Used for validation only, do not allocate anything here!
     *
     * This is just to check that there is enough RAM left for the Main
     * stack. It should generate an error if it's full.
     */
    ._check_stack : ALIGN(8)
    {
        . = . + _Minimum_Stack_Size ;
    } >SYSRAM

    /* After that there are only debugging sections. */

    /* This can remove the debugging information from the standard libraries */
    /*
    DISCARD :
    {
     libc.a ( * )
     libm.a ( * )
     libgcc.a ( * )
     }
     */

    /* Stabs debugging sections.  */
    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    .stab.excl     0 : { *(.stab.excl) }
    .stab.exclstr  0 : { *(.stab.exclstr) }
    .stab.index    0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment       0 : { *(.comment) }
    /*
     * DWARF debug sections.
     * Symbols in the DWARF debugging sections are relative to the beginning
     * of the section so we begin them at 0.
     */
    /* DWARF 1 */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    /* GNU DWARF 1 extensions */
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    /* DWARF 1.1 and DWARF 2 */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    /* DWARF 2 */
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    /* SGI/MIPS DWARF 2 extensions */
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }
    /* DWARF 3 */
    .debug_ranges    0 : { *(.debug_ranges) }
    .debug_pubtypes  0 : { *(.debug_pubtypes) }
    .debug_macro     0 : { *(.debug_macro) }
    /* DWARF 4 */
    .debug_types     0 : { *(.debug_types) }
    .debug_addr      0 : { *(.debug_addr) }
    /* DWARF 5 */
    .debug_gnu_pubtypes 0 : { *(.debug_gnu_pubtypes) }
    .debug_gnu_pubnames 0 : { *(.debug_gnu_pubnames) }
    .debug_line_str     0 : { *(.debug_line_str) }
    .debug_str_offsets  0 : { *(.debug_str_offsets) }
    .debug_loclists     0 : { *(.debug_loclists) }
    .debug_rnglists     0 : { *(.debug_rnglists) }
    .debug_names        0 : { *(.debug_names) }
    /* Others */
    .note.GNU-stack         0 : { *(.note.GNU-stack) }
    .note.ABI-tag           0 : { *(.note.ABI-tag) }
    .gnu.build.attributes   0 : { *(.gnu.build.attributes) }
    .riscv.attributes       0 : { *(.riscv.attributes) }
    .rela                   0 : { *(.rela .rela.*) }
}

/* pmp align */
PMP_ALIGN_SIZE = (0x1000);
__sysram_text_start_align_up_4k__ = ALIGN(__stext, PMP_ALIGN_SIZE);
__sysram_text_end_align_down_4k__ = (__data_start__ & (~(PMP_ALIGN_SIZE - 1)));
__sysram_data_start_align_up_4k__ = ALIGN(__data_start__, PMP_ALIGN_SIZE);
__sysram_data_end_align_down_4k__ = (__actual_bss_end__ & (~(PMP_ALIGN_SIZE - 1)));
__sysram_2boot_start_align_up_4k__ = (0x10000000);
__sysram_2boot_end_align_down_4k__ = (ORIGIN(SYSRAM) & (~(PMP_ALIGN_SIZE - 1)));
__psram_text_start_align_up_4k__ = ALIGN(__psram_stext, PMP_ALIGN_SIZE);
__psram_text_end_align_down_4k__ = (__psram_data_start__ & (~(PMP_ALIGN_SIZE - 1)));
__psram_data_start_align_up_4k__ = ALIGN(__psram_data_start__, PMP_ALIGN_SIZE);
__psram_data_end_align_down_4k__ = (__psram_actual_bss_end__ & (~(PMP_ALIGN_SIZE - 1)));
