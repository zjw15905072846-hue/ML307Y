# -*- coding: utf-8 -*-
# ===================================================================
#
# Copyright © 2025 China Mobile IOT. All rights reserved.
#
# Date:     2025/03/06
# Author:   cmiot0752
# Function: kernel编译工具集合
#
# ===================================================================


import glob
import hashlib
import os
import shutil
import subprocess
import sys

from SCons.Script import *
from EnvironConfig import *


def install_kernel_tools(env):
    """
    基于工程环境安装个性化工具
    :param env: 工程环境变量
    :return: None
    """
    env['CC'] = KernelBuildConfig.CC
    env['CXX'] = KernelBuildConfig.CXX
    env['AR'] = KernelBuildConfig.AR
    # Use the C++ linker (g++) for linking so C++ runtime/linking semantics are correct
    env['LINK'] = KernelBuildConfig.CXX
    env['SZ'] = KernelBuildConfig.SZ
    env['OBJCOPY'] = KernelBuildConfig.COPY
    env['OBJDUMP'] = KernelBuildConfig.OBJDUMP
    env['READELF'] = KernelBuildConfig.READELF
    env['CCFLAGS'] = KernelBuildConfig.COMMON_CCFLAGS
    env['CPPDEFINES'] = KernelBuildConfig.COMMON_DEFS
    env['CPPPATH'] = KernelBuildConfig.COMMON_INCS
    env['IMAGE_DIR'] = os.path.join(Dir('#').abspath, 'out', 'kernel', 'image')
    env['OBJECT_DIR'] = os.path.join('out', 'kernel', 'obj')
    env['IMAGE_INFO_JSON'] = os.path.join(
        'out', 'kernel', 'image', 'temp_image_info.json')
    env['PREBUILT_MODE'] = 'single_mode' if env.get('OC_ENTRY') == 'kernel' else 'open_mode'
    env['PREBUILT_DIR'] = os.path.join('kernel', 'prebuilts', env['PREBUILT_MODE'])
    env['LIB_DIR'] = os.path.join(env['PREBUILT_DIR'], 'libs')
    env['LIB_RSP'] = os.path.join(env['LIB_DIR'], 'prebuilt_lib.rsp')
    env['EXPORT_RSP'] = os.path.join(env['IMAGE_DIR'], 'mandatory_link.rsp')
    env['LD_INI_SRC'] = os.path.join(env['PREBUILT_DIR'], 'ld', 'apld.ini')
    env['PARTITION_CHECK_INI_SRC'] = os.path.join(
        env['PREBUILT_DIR'], 'ld', 'ap_partition.ini')
    if env['PREBUILT_MODE'] == 'open_mode':
        env['EXPORT_SRC'] = os.path.join('kernel', 'export', 'open_mode', 'xy_export.list')

    legacy_ld_flag = '-L%s' % os.path.join(BaseBuildConfig.root, 'kernel', 'prebuilts', 'ld')
    active_ld_flag = '-L%s' % os.path.join(BaseBuildConfig.root, env['PREBUILT_DIR'], 'ld')
    env['LINKFLAGS'] = [flag for flag in KernelBuildConfig.COMMON_LINKFLAGS if flag != legacy_ld_flag]
    env['LINKFLAGS'].insert(0, active_ld_flag)
    env['LIB_GROUP_PREFIX'] = ['-Wl,--start-group']
    env['LIB_GROUP_SUFFIX'] = ['-Wl,--end-group']
    env['LINKCOM'] = '$LINK -o $TARGET -Tmem.ld -Tsections.ld -Wl,-Map=${TARGET.base}.map $LINKFLAGS $__RPATH $SOURCES $_LIBDIRFLAGS '
    if env['PREBUILT_MODE'] == 'open_mode':
        env['LINKCOM'] += '@$EXPORT_RSP '
    env['LINKCOM'] += '-Wl,--whole-archive @$LIB_RSP -Wl,--no-whole-archive'
    env['CCCOMSTR'] = 'Compiling $SOURCE'  # 自定义编译输出
    # env['LINKCOMSTR'] = 'Linking $TARGET'  # 自定义链接输出
    env['LSTCOMSTR'] = 'Generating $TARGETS'
    env['SECTCOMSTR'] = 'Generating $TARGETS'
    env['PACKCOMSTR'] = 'Generating $TARGETS'
    env['UTILSCOMSTR'] = 'Generating $TARGETS'

    env['SCRIPTS_DIR'] = os.path.join('tools', 'scripts')
    env['IMAGE_COMPRESSOR'] = os.path.join(
        env['SCRIPTS_DIR'], 'compress_bin_ap.py')
    env['PACKER'] = os.path.join(env['SCRIPTS_DIR'], 'xy_packer.py')
    env['EXPORT_PY'] = os.path.join(env['SCRIPTS_DIR'], 'mandatory_link.py')
    env['PARTITION_CHECKER'] = File(os.path.join(
        'tools', 'scripts', 'partition_check.py')).srcnode().abspath
    env.Append(BUILDERS={'FlashBin': flash_binary_builder})  # 增加自定义命令
    env.Append(BUILDERS={'SysramBin': sysram_binary_builder})
    env.Append(BUILDERS={'PsramBin': psram_binary_builder})
    env.Append(BUILDERS={'GenRsp': rsp_builder})
    env.Append(BUILDERS={'GenLd': ld_builder})
    env.Append(BUILDERS={'GenAsm': asm_builder})
    env.Append(BUILDERS={'GenImg': img_builder})
    env.Append(BUILDERS={'GenLibRsp': lib_rsp_builder})


def generate_img(target, source, env):
    """
    生成ap.img
    """
    # 打印源文件路径（仅路径，不含文件名）
    bin_dir = os.path.dirname(str(source[1]))

    cmd = [sys.executable, env['IMAGE_COMPRESSOR'],  env['SCRIPTS_DIR'],
           env['LD_INI_SRC'], '0x10000', bin_dir]
    status = subprocess.run(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, check=False)
    if status.returncode != 0:
        print(status.stdout.decode('utf-8'))  # 解码并打印错误信息
        return None

    cmd = [sys.executable, env['PACKER'], '-b', bin_dir,
           '-i', env['LD_INI_SRC'], '-t', env['IMAGE_INFO_JSON']]
    status = subprocess.run(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, check=False)
    if status.returncode != 0:
        print(status.stdout.decode('utf-8'))  # 解码并打印错误信息
    else:
        # 获取CP_TARGET对应的elf路径
        elf_path = os.path.join(env['IMAGE_DIR'], env['CP_TARGET'] + '.elf')
        cmd = [sys.executable, env['PARTITION_CHECKER'], '-t', env['IMAGE_INFO_JSON'], '-i', env['PARTITION_CHECK_INI_SRC'],
               '-p', 'ap', '-n', env['CP_TARGET'], '-e', elf_path]
        status = subprocess.run(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
        if status.returncode != 0:
            print(status.stdout.decode('utf-8'))  # 解码并打印错误信息
        else:
            # print(status.stdout.decode('utf-8'))
            print('generate package Done')


def generate_rsp(target, source, env):
    """
    生成rsp文件
    """
    # 使用与目标文件相同的目录作为临时文件目录
    temp_dir = os.path.dirname(str(target[0]))
    mandatory_src = os.path.join(temp_dir, 'mandatory_link.c')
    link_opt = os.path.join(temp_dir, 'mandatory_link_opt.rsp')
    
    # 生成mandatory_link.c
    cmd = [sys.executable,
        env['EXPORT_PY'],
        '-e', 
        '-l', 
        str(source[0]), 
        '-o', 
        mandatory_src
    ]
    status = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if status.returncode != 0:
        print(status.stdout.decode('utf-8'))  # 解码并打印错误信息
        return None
    
    # 生成mandatory_link_options.rsp
    def_list = env.get('CPPDEFINES')
    with open(link_opt, 'w') as f:
        for d in def_list:
            f.write(f"-D{d[0]}={d[1]}\n" if isinstance(d, tuple) else f"-D{d}\n")
    
    # 生成mandatory_link.o
    cmd = [
        os.path.join(PathConfig.GUN_PATH, KernelBuildConfig.CC),
        '-c',
        mandatory_src, 
        *(KernelBuildConfig.COMMON_CCFLAGS if isinstance(KernelBuildConfig.COMMON_CCFLAGS, list) else KernelBuildConfig.COMMON_CCFLAGS.split()),
        f"@{link_opt}",
        '-o',
        str(target[1])
    ]
    status = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
    if status.returncode != 0:
        print(status.stdout.decode('utf-8'))  # 解码并打印错误信息
        return None

    # 生成mandatory_link.rsp
    cmd = [
        sys.executable,
        env['EXPORT_PY'],
        '-r', 
        '-c',
        str(target[1]),
        '-o',
        str(target[0])
    ]
    status = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if status.returncode != 0:
        print(status.stdout.decode('utf-8'))  # 解码并打印错误信息
        return None


def generate_ld(target, source, env):
    """
    生成ld文件
    """
    cmd = [
        sys.executable,
        env['EXPORT_PY'],
        '-i',
        '-t',
        str(source[0]),
        '-c',
        str(source[1]),
        '-o',
        str(target[0])
    ]
    status = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if status.returncode != 0:
        print(status.stdout.decode('utf-8'))  # 解码并打印错误信息
        return None


def generate_lib_rsp(target, source, env):
    """
    生成lib.rsp文件
    """
    lib_dir = env['LIB_DIR']
    lib_rsp = env['LIB_RSP']

    # 查找所有.a文件，生成以LIB_DIR开头的相对路径
    lib_files = glob.glob(os.path.join(lib_dir, '*.a'))
    lib_paths = [os.path.relpath(f, start=os.path.dirname(
        lib_rsp)).replace('\\', '/') for f in lib_files]

    # 写入LIB_RSP文件
    with open(lib_rsp, 'w') as f:
        for path in lib_paths:
            f.write(os.path.join(lib_dir,
                    os.path.basename(path)).replace('\\', '/') + "\n")


# Convert from ELF to binary
flash_binary_builder = Builder(
    action=Action('$OBJCOPY -O binary -j .flash.text -j .flash.rodata $SOURCE $TARGET', '$UTILSCOMSTR'),
    suffix='.bin',
    src_suffix='.elf'
)

# Convert from ELF to binary
psram_binary_builder = Builder(
    action=Action('$OBJCOPY -O binary -j .psram.text -j .psram.rodata -j .psram.data $SOURCE $TARGET', '$UTILSCOMSTR'),
    suffix='.bin',
    src_suffix='.elf'
)

# Convert from ELF to binary
sysram_binary_builder = Builder(
    action=Action('$OBJCOPY -O binary -j .text -j .rodata -j .data $SOURCE $TARGET', '$UTILSCOMSTR'),
    suffix='.bin',
    src_suffix='.elf'
)

# Generate img file
img_builder = Builder(
    action=Action(generate_img, '$UTILSCOMSTR'),
    suffix='.img',
)

# Generate asm file
asm_builder = Builder(
    action=Action('$OBJDUMP $SOURCE -x -S > $TARGET', '$UTILSCOMSTR'),
    suffix='.asm',
    src_suffix='.elf'
)

# Generate mandatory_link.rsp
rsp_builder = Builder(
    action=Action(generate_rsp, '$UTILSCOMSTR'),
)

# Generate import_func.ld
ld_builder = Builder(
    action=Action(generate_ld, '$UTILSCOMSTR'),
    suffix='.ld',
)

lib_rsp_builder = Builder(
    action=Action(generate_lib_rsp, '$UTILSCOMSTR'),
    suffix='.rsp',
)
