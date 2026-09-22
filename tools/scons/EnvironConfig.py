# -*- coding: utf-8 -*-
# Copyright © 2023 China Mobile IOT. All rights reserved.
#
# Date:     2023/02/20
# Author:   zhangxw
# Modify： cmiot0752@20260119，模块编译
# Function: 编译环境配置
#
# ===================================================================


from SCons.Script import *


class TargetConfig:
    """目标配置基类，定义通用逻辑"""
    def __init__(self, target_name):
        self.target_name = target_name
        self.env = self._create_env()
        self.sconscript = f'src/{target_name}/SConscript'

    def _create_env(self):
        """创建环境，子类需重写此方法"""
        env = Environment(
            ENV={'PATH': os.environ['PATH']},
            tools=['default', 'gcc', 'g++', 'gas', 'ar', 'gnulink'],
            toolpath=['tools/scons'],
            PROGSUFFIX='.elf',  # 可执行文件后缀
            BUILD_TARGET=self.target_name
        )
        env['MODULE_NAME'] = ARGUMENTS.get('module', 'ML307Y')
        env['MODULE_SUB_NAME'] = ARGUMENTS.get('sub_module', 'DL')
        env['MODULE_TARGET'] = env['MODULE_NAME']
        env['TARGET_NAME'] = env['MODULE_TARGET'] + '_APP'
        env['DEMO_SUPPORT'] = ARGUMENTS.get('demo', 'n')
        env['TEST_SUPPORT'] = ARGUMENTS.get('test', 'n')
        env['XY_TEST_SUPPORT'] = ARGUMENTS.get('xydemo', 'n')
        env['OC_ENTRY'] = 'userapp'
        return env

    def get_config(self):
        """返回目标配置"""
        return {
            'env': self.env,
            'sconscript': self.sconscript,
        }


class TargetUserAppConfig(TargetConfig):
    """userapp的构建配置（使用 gcc）"""
    def __init__(self, target_name):
        super().__init__(target_name)  # 调用基类的 __init__ 方法
        self.sconscript = f'SConscript-u'  # 自定义路径

    def _create_env(self):
        return super()._create_env()


class TargetKernelConfig(TargetConfig):
    """kernel的构建配置（使用 armcc）"""
    def __init__(self, target_name):
        super().__init__(target_name)  # 调用基类的 __init__ 方法
        self.sconscript = f'SConscript-k'  # 自定义路径

    def _create_env(self):
        env = super()._create_env()
        env['CP_TARGET'] = 'ap'
        env['OC_ENTRY'] = ARGUMENTS.get('oc_entry', 'userapp')
        return env


class TargetObmConfig(TargetConfig):
    """obm的构建配置（使用 armcc）"""
    def __init__(self, target_name):
        super().__init__(target_name)  # 调用基类的 __init__ 方法
        self.sconscript = f'boot/SConscript'  # 自定义路径

    def _create_env(self):
        return super()._create_env()


class TargetFactory:
    """目标工厂类，用于创建目标配置"""
    @staticmethod
    def get_target_config(build_target):
        # 默认目标为 userapp
        build_target = build_target or 'userapp'

        # 目标配置映射表
        target_classes = {
            'userapp': TargetUserAppConfig,
            'kernel': TargetKernelConfig,
            'obm': TargetObmConfig,
        }

        # 检查目标是否存在
        if build_target not in target_classes:
            raise ValueError(f"Error: Unknown target '{build_target}'")

        # 创建目标配置实例
        target_class = target_classes[build_target]
        return target_class(build_target).get_config()

    @staticmethod
    def get_current_target_config():
        """获取当前目标配置（基于命令行参数）"""
        build_target = ARGUMENTS.get('target', 'userapp')  # 从命令行获取 target 参数
        return TargetFactory.get_target_config(build_target)


class BaseBuildConfig:
    """构建配置基类，包含所有公共配置"""
    root = os.path.join(Dir('#').abspath)
    triple = 'riscv64-unknown-elf-'
    CPU = 'riscv'
    CC = triple + 'gcc'
    CXX = triple + 'g++'
    AR = triple + 'ar'
    SZ = triple + 'size'
    COPY = triple + 'objcopy'
    OBJDUMP = triple + 'objdump'
    READELF = triple + 'readelf'
    LIBPATH = [
        # os.path.join(root, 'prebuild', 'libs'),
    ]
    LIBS = [
        # 'core',
    ]
    COMMON_INCS = [
        # '#include/cmiot',
    ]
    COMMON_DEFS = [  # 全局宏定义
        # '_SYS_SELECT_H',
    ]
    COMMON_LINKFLAGS = [
        '-march=rv64imac_zicntr_zicsr_zifencei_zihpm_xtheadc',
        '-mabi=lp64',
        '-mtune=c906v',
        '-mcmodel=medlow',
        '-nostartfiles',
        '-fno-rtti',
        '-Wl,--gc-sections',
        '-specs=nano.specs',
        '-specs=nosys.specs',
        '-Wl,--orphan-handling=place',
        '-Wl,--wrap=_malloc_r',
        '-Wl,--wrap=_free_r',
        '-Wl,--wrap=_realloc_r',
        '-Wl,--wrap=_calloc_r',
        '-Wl,--wrap=_write',
        '-Wl,--wrap=_reclaim_reent',
        '-Wl,--wrap=__assert_func',
        '-Wl,--wrap=__assert',
        '-Wl,--wrap=memcpy',
        '-Wl,--wrap=memset',
        '-Wl,--wrap=memcmp',
        '-Wl,--wrap=memmove',
        '-Wl,--wrap=strcat',
        '-Wl,--wrap=strchr',
        '-Wl,--wrap=strcmp',
        '-Wl,--wrap=strcpy',
        '-Wl,--wrap=strlen',
        '-Wl,--wrap=strncat',
        '-Wl,--wrap=strncpy',
        '-Wl,--wrap=fputc',
        '-Wl,--wrap=fputs'
    ]
    COMMON_CCFLAGS = [  # 编译参数
        '-march=rv64imac_zicntr_zicsr_zifencei_zihpm_xtheadc',
        '-mabi=lp64',
        '-mtune=c906v',
        '-mcmodel=medlow',
        '-std=gnu11',
        '-ffixed-gp',
        '-ffixed-tp',
        '-mstrict-align',
        '-g3',
        '-gdwarf-4',
        '-Wall',
        '-ffunction-sections',
        '-fdata-sections',
        '-Os'
    ]


class UserAppBuildConfig(BaseBuildConfig):
    """UserApp构建配置，继承基础配置"""
    LIBPATH = [
        # os.path.join(root, 'prebuild', 'libs'),
    ]
    LIBS = [
        # 'core',
    ]
    COMMON_INCS = [
        '#include/cmiot',
        '#include/platform',
        '#include/platform/kernel/xinyi',
        '#include/platform/network/lwip/lwip-2.1.3/include',
        '#include/platform/network/lwip/lwip-2.1.3/lwip_config',
        '#include/platform/network/lwip/lwip-2.1.3/lwip_port',
        '#third-party/embedded-cli/inc',
    ]
    COMMON_DEFS = [  # 全局宏定义
        '_SYS_SELECT_H',
        '_REENT_SMALL',
        '_REENT_GLOBAL_ATEXIT'
    ]
    COMMON_LINKFLAGS = [
        '-L%s' % os.path.join(BaseBuildConfig.root, 'prebuild', 'ld'),
    ] + BaseBuildConfig.COMMON_LINKFLAGS
    COMMON_CCFLAGS = [

    ] + BaseBuildConfig.COMMON_CCFLAGS


class KernelBuildConfig(BaseBuildConfig):
    """Kernel构建配置，继承基础配置"""
    LIBPATH = [

    ]
    LIBS = [

    ]
    COMMON_INCS = [
        '#include/cmiot',
        '#include/platform',
        '#include/platform/kernel/xinyi',
        '#include/platform/network/lwip/lwip-2.1.3/include',
        '#include/platform/network/lwip/lwip-2.1.3/lwip_config',
        '#include/platform/network/lwip/lwip-2.1.3/lwip_port',
        '#third-party/embedded-cli/inc',
    ]
    COMMON_DEFS = [
        'XY4100LD=1',
        'AT_BASE_QUEC=0',
        'AT_BASE_CMCC=0',
        'AT_BASE_NB=0',
        'AT_PING_QUEC=0',
        'AT_PING_CMCC=0',
        'AT_NTP_QUEC=0',
        'AT_NTP_CMCC=0',
        'AT_SOCK_QUEC=0',
        'AT_SOCK_CMCC=0',
        'AT_SOCK_NB=0',
        'AT_SSL_QUEC=0',
        'AT_SSL_CMCC=0',
        'AT_HTTP_QUEC=0',
        'AT_HTTP_CMCC=0',
        'HTTP_SSL=1',
        'AT_MQTT_QUEC=0',
        'AT_MQTT_CMCC=0',
        'MQTT_SSL=1',
        'MQTT_TASK=0',
        'AT_FTP_QUEC=0',
        'AT_FTP_CMCC=0',
        'FTP_SSL=1',
        'AT_FOTA_QUEC=0',
        'AT_FOTA_CMCC=0',
        'FOTA_HTTP=1',
        'FOTA_FTP=0',
        'AT_LED_QUEC=0',
        'AT_LED_CMCC=0',
        'AT_FS_QUEC=0',
        'AT_FS_CMCC=0',
        'LBS_ONEOS=1',
        'LBS_AMAP=1',
        'MBEDTLS_CONFIG_FILE=<mbedtls_config_dtls.h>',
        'MBEDTLS_ALLOW_PRIVATE_ACCESS=1',
        'SPI_FLASH_SUPPORT=0',
        'QSPI_FLASH_SUPPORT=0',
        'EXT_FLASH_LEN=0',
        'EXTFS_START_BASE=0x602B8000',
        'EXTFS_LEN=0xC0000',
        'DIAG_EN=1',
        'AT_TEST_OFF=0',
        'LPUART_AT=0',
        'LPUART_ABR=0',
        'UART_AT=0',
        'LOG_AT=0',
        'CSP_LOG=0',
        'DEBUG_UART=0',
        'IP_LOG=0',
        'PACKET_DEBUG=0',
        'PERIOD_LOG=1',
        'XY_PPP=0',
        'DTR_PIN=0',
        'RI_PIN=10',
        'CFUN_PIN=0',
        'USB_DEVICE=1',
        'USB_RNDIS=0',
        'USB_ECM=0',
        'USB_AT=1',
        'USB_LOG=1',
        'USB_MODEM=0',
        'USB_PKT_INFO=0',
        'USB_FUNC_RECORD_TIME_ON=0',
        'USB_TTYACM=0',
        'USB_HID_MOUSE=0',
        'USB_SUSPEND_RESUME=0',
        'USB_ENUM_DEBUG=0',
        'USB_DATA_DEBUG=0',
        'CM_TTS_ENABLE=0',
        'MULTI_SUBNET=1',
        'TCP_WND_OPTIMIZE=1',
        'DNS_RELAY=1',
        'DNS6_RELAY=1',
        'XY_IKE=0',
        'MODULE_TYPE=0',
        'VER_BC95=0',
        '_REENT_SMALL',
        '_REENT_GLOBAL_ATEXIT',
        'NDEBUG',
        'TX_INCLUDE_USER_DEFINE_FILE',
        'UX_INCLUDE_USER_DEFINE_FILE',
        'OS_HEAP_TYPE=9',
        'CM_COT_ENABLE=1',
        'COT_CMDMP=1',
        'XY_TEST=1',
        'PS_TEST_MODE=0',
    ]
    COMMON_LINKFLAGS = [
        '-L%s' % os.path.join(BaseBuildConfig.root, 'kernel', 'prebuilts', 'ld'),
        '-Wl,-u,_printf_float',
        '-Wl,-u,_scanf_float'
    ] + BaseBuildConfig.COMMON_LINKFLAGS
    COMMON_CCFLAGS = [

    ] + BaseBuildConfig.COMMON_CCFLAGS

class ObmBuildConfig(BaseBuildConfig):
    """OBM构建配置，继承基础配置"""
    # 仅重写与基类不同的部分
    LIBPATH = [

    ]
    LIBS = [

    ]
    COMMON_INCS = [

    ]
    COMMON_DEFS = [

    ]
    COMMON_LINKFLAGS = [

    ] + BaseBuildConfig.COMMON_LINKFLAGS
    COMMON_CCFLAGS = [

    ] + BaseBuildConfig.COMMON_CCFLAGS


class PathConfig:
    root = os.path.join(Dir('#').abspath)
    GUN_PATH = os.path.join(root, 'tools', 'toolchain', 'riscv64-elf-tools-v3', 'bin')  # 使用join方法屏蔽平台路径差异
    OEM_PATH = os.path.join(root, 'tools', 'oem_tools')
