# -*- coding: utf-8 -*-
# ===================================================================
#
# Copyright © 2023 China Mobile IOT. All rights reserved.
#
# Date:
# Author:
# Modify:
# Modify:   by cmiot0752@2026/1/16，多目标构建（userapp/kernel/boot）
# Function: scons工程文件
#
# ===================================================================


import os
import subprocess
import sys

from SCons.Script import (BoolVariable, Dir, EnumVariable, Environment, File,
                          GetOption, Help, Platform, Return, SConscript,
                          Variables)

# scons脚本路径添加至系统
sys.path.append(os.path.realpath(os.path.join('tools', 'scons')))
from EnvironConfig import *
from UtilsTools import *
from KernelTools import *
#from ObmTools import *

# 系统环境配置
op_sys = Platform()
print("Operation system: %s" % op_sys)

build_target = TargetFactory.get_current_target_config()
scons_file = build_target['sconscript']
env = build_target['env']
env.PrependENVPath('PATH', PathConfig.GUN_PATH)  # GUN工具路径
env.PrependENVPath('PATH', PathConfig.OEM_PATH)  # OEM工具路径
# print('PATH: ', env['ENV']['PATH'])

# 读取命令行参数
# debug = ARGUMENTS.get('debug', 0)

def install_spawn(env):
    """
    命令行太长时(通常是链接命令)，绕过shell，使用subprocess执行
    """
    # This code is from the SCons wiki: https://github.com/SCons/scons/wiki/LongCmdLinesOnWin32
    if env['PLATFORM'] == 'win32':
        old_spawn = env['SPAWN']

        def my_spawn(sh, escape, cmd, args, spawnenv):
            if ">" in args or "<" in args:  # 忽略重定向命令
                return old_spawn(sh, escape, cmd, args, spawnenv)

            if "riscv64-unknown-elf" in cmd:  # 子进程PATH环境无法传入
                cmd = PathConfig.GUN_PATH + os.path.sep + cmd

            newargs = ' '.join(args[1:])
            cmdline = cmd + " " + newargs
            startupinfo = subprocess.STARTUPINFO()
            startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            proc = subprocess.Popen(cmdline, startupinfo = startupinfo, shell = False, env = spawnenv)
            # data, err = proc.communicate()
            rv = proc.wait()
            return rv

        env['SPAWN'] = my_spawn


# 生成工程环境
root_dir = os.path.join(Dir('#').abspath)
install_spawn(env)

if env['BUILD_TARGET'] == 'userapp':
    install_tools(env)  # 安装工具
elif env['BUILD_TARGET'] == 'kernel':
    install_kernel_tools(env)
elif env['BUILD_TARGET'] == 'obm':
    install_obm_tools(env)


Export('env', 'root_dir')
SConscript(scons_file, variant_dir=env['OBJECT_DIR'], duplicate=0)

# 编译帮助信息
Help("""
'scons target=userapp'                      编译userapp，默认编译custom程序。'target=userapp' 可省略
'scons target=userapp test=y'               编译userapp测试程序。
'scons target=userapp demo=mifi'            编译userapp mifi示例。
'scons target=kernel'                       编译kernel，仅导出符号表
'scons target=obm'                          编译obm
'scons target=userapp -c'                   清除userapp编译，'target=userapp' 可省略
'scons target=userapp test=y -c'            清除userapp test编译
'scons target=userapp demo=mifi -c'         清除userapp mifi示例。
'scons target=kernel -c'                    清除kernel编译
'scons target=obm -c'                       清除obm编译
""")
