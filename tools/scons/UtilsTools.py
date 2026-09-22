# -*- coding: utf-8 -*-
# ===================================================================
#
# Copyright © 2023 China Mobile IOT. All rights reserved.
#
# Date:
# Author:
# Modify:
# Function: 工具集合
#
# ===================================================================


import glob
import hashlib
import shutil
import subprocess
import sys

from EnvironConfig import *
from SCons.Script import *


def install_tools(env):
    """
    基于工程环境安装个性化工具
    :param env: 工程环境变量
    :return: None
    """
    env['CC'] = UserAppBuildConfig.CC
    env['CXX'] = UserAppBuildConfig.CXX
    env['AR'] = UserAppBuildConfig.AR
    # Use the C++ linker (g++) for linking so C++ runtime/linking semantics are correct
    env['LINK'] = UserAppBuildConfig.CXX
    env['SZ'] = UserAppBuildConfig.SZ
    env['OBJCOPY'] = UserAppBuildConfig.COPY
    env['OBJDUMP'] = UserAppBuildConfig.OBJDUMP
    env['READELF'] = UserAppBuildConfig.READELF
    env['CCFLAGS'] = UserAppBuildConfig.COMMON_CCFLAGS
    env['LINKFLAGS'] = UserAppBuildConfig.COMMON_LINKFLAGS
    env['CPPDEFINES'] = UserAppBuildConfig.COMMON_DEFS
    env['CPPPATH'] = UserAppBuildConfig.COMMON_INCS
    env['LIBS'] = UserAppBuildConfig.LIBS
    env['LIBPATH'] = [
        os.path.join(Dir('#').abspath, 'prebuild', 'libs'),
    ] + UserAppBuildConfig.LIBPATH
    env['LNPATH'] = [os.path.join('prebuild', 'ld')]
    env['IMAGE_DIR'] = os.path.join(Dir('#').abspath, 'out', 'userapp', 'image')
    env['IMAGE_ORIGIN_DIR'] = os.path.join(env['IMAGE_DIR'], 'origin_bins')
    env['OBJECT_DIR'] = os.path.join('out', 'userapp', 'obj')
    env['IMAGE_INFO_JSON'] = os.path.join(
        'out', 'userapp', 'image', 'temp_image_info.json')
    env['LDS_SRC'] = os.path.join('prebuild', 'ld', 'app.ld')
    env['LD_INI_SRC'] = os.path.join('prebuild', 'ld', 'appld.ini')
    env['PARTITION_CHECK_INI_SRC'] = os.path.join(
        'prebuild', 'ld', 'app_partition.ini')
    env['LIB_GROUP_PREFIX'] = ['-Wl,--start-group']
    env['LIB_GROUP_SUFFIX'] = ['-Wl,--end-group']
    env['LINKCOM'] = '$LINK -o $TARGET -L$IMAGE_DIR -Tlinker.lds -Timport_func.ld -Wl,-Map=${TARGET.base}.map $LINKFLAGS $__RPATH $SOURCES $_LIBDIRFLAGS ' \
                     '$LIB_GROUP_PREFIX $_LIBFLAGS -lm -lgcc $LIB_GROUP_SUFFIX'
    env['CCCOMSTR'] = 'Compiling $SOURCE'  # 自定义编译输出
    env['LINKCOMSTR'] = 'Linking $TARGET'  # 自定义链接输出
    env['LSTCOMSTR'] = 'Generating $TARGET'
    env['SECTCOMSTR'] = 'Generating $TARGET'
    env['PACKCOMSTR'] = 'Generating $TARGET'
    env['UTILSCOMSTR'] = 'Generating $TARGET'

    env['SCRIPTS_DIR'] = Dir(os.path.join(
        'tools', 'scripts')).srcnode().abspath
    env['IMAGE_COMPRESSOR'] = File(os.path.join(
        'tools', 'scripts', 'compress_bin.py')).srcnode().abspath
    env['PACKER'] = File(os.path.join(
        'tools', 'scripts', 'xy_packer.py')).srcnode().abspath
    env['PARTITION_CHECKER'] = File(os.path.join(
        'tools', 'scripts', 'partition_check.py')).srcnode().abspath
    env.Append(BUILDERS={'Binary': binary_builder})  # 增加自定义命令
    env.Append(BUILDERS={'GenAsm': asm_builder})
    env.Append(BUILDERS={'GenLds': lds_builder})
    env.Append(BUILDERS={'Compress': compress_builder})
    env.Append(BUILDERS={'GenImg': img_builder})


def objcopy_multi_sections(target, source, env, for_signature):
    src = str(source[0])
    tgt = str(target[0])

    objcopy = env.get('OBJCOPY', 'objcopy')
    sections = env.get('SECTIONS', [])
    cmd = [objcopy, '-O', 'binary']
    if not sections:
        return ''
    for sec in sections:
        cmd.extend(['-j', sec])
    cmd.extend([src, tgt])
    return ' '.join(cmd)


# Convert from ELF to binary
binary_builder = Builder(
    generator=objcopy_multi_sections,
    src_suffix='.elf'
)

# Generate asm file
asm_builder = Builder(
    action=Action('$OBJDUMP $SOURCE -x -S > $TARGET', '$UTILSCOMSTR'),
    suffix='.asm',
    src_suffix='.elf'
)


# Generate ld script
lds_builder = Builder(
    action=Action('$CC -E -P -w - <$SOURCE -o $TARGET', '$UTILSCOMSTR'),
    suffix='.lds',
    src_suffix='.ld'
)


def compress_bins(target, source, env):
    if os.path.exists(env['IMAGE_ORIGIN_DIR']):
        cmd = [sys.executable, env['IMAGE_COMPRESSOR'],  env['SCRIPTS_DIR'],
               env['LD_INI_SRC'], '0x10000', env['IMAGE_ORIGIN_DIR'], env['IMAGE_DIR']]
        status = subprocess.run(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
        if status.returncode != 0:
            print(status.stdout.decode('utf-8'))  # 解码并打印错误信息
            return -1
        else:
            # print(status.stdout.decode('utf-8'))
            print('compress_bins Done')
    return 0


# Generate release package
compress_builder = Builder(
    action=Action(compress_bins, 'compressing bins...')
)


def generate_img(target, source, env):
    """
    生成app.img
    """
    # print('Generating package...')
    if os.path.exists(env['IMAGE_DIR']):
        cmd = [sys.executable, env['PACKER'], '-b', env['IMAGE_DIR'],
               '-i', env['LD_INI_SRC'], '-t', env['IMAGE_INFO_JSON']]
        status = subprocess.run(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
        if status.returncode != 0:
            # print('generate package Failed')
            print(status.stdout.decode('utf-8'))  # 解码并打印错误信息
            return -1
        else:
            # 获取TARGET_NAME对应的elf路径
            elf_path = os.path.join(
                env['IMAGE_DIR'], env['TARGET_NAME'] + '.elf')
            cmd = [sys.executable, env['PARTITION_CHECKER'], '-t', env['IMAGE_INFO_JSON'], '-i', env['PARTITION_CHECK_INI_SRC'],
                   '-p', 'app', '-n', env['TARGET_NAME'], '-e', elf_path]
            status = subprocess.run(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
            if status.returncode != 0:
                print(status.stdout.decode('utf-8'))  # 解码并打印错误信息
                return -1
            else:
                # print(status.stdout.decode('utf-8'))
                print('generate package Done')
        return 0
    return -1


# Generate release package
img_builder = Builder(
    action=Action(generate_img, '$UTILSCOMSTR')
)
