#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import re
import argparse

# import 各种 section 对象
from elftools.elf.sections import SymbolTableSection
from elftools.elf.elffile import ELFFile


class MandatoryLink:
    def __init__(self, _import, _export, _response, elf_path, object_path, list_path, output):
        # 保存参数选项
        self._import = _import
        self._export = _export
        self._response = _response
        # 仅在生成导入文件时要读取elf
        if _import is True:
            # 保存函数名及地址的字典
            self.symbol_dict = self.__generate_symbol_dict(elf_path)
        # 仅在生成导出文件时需要独立列表文件
        if _export is True:
            # 保存要生成汇编的函数列表、头文件列表、宏定义列表
            self.func_list, self.header_list, self.macro_list = self.__get_func_list(list_path)
        # 在生成导入文件和强制链接文件时需要使用
        if _import is True or _response is True:
            self.object_path = os.path.abspath(object_path)
        # 保存输出文件路径
        self.output_file = os.path.abspath(output)

    def __generate_symbol_dict(self, elf_path):
        """生成符号字典，键为符号名称，值为符号地址"""
        # 文件是否存在
        if os.path.exists(elf_path) and os.path.isfile(elf_path):
            # 获取绝对路径
            file_path = os.path.abspath(elf_path)
            # 以二进制格式打开elf文件，并保存句柄到公有变量中，析构时需要关闭文件
            f_elf = open(file_path, 'rb')
            # 传入elf文件句柄，生成elftools的对象
            elf_object = ELFFile(f_elf)
        else:
            # 文件不存在，打印提示，退出程序
            print("file %s is not exists!" % elf_path)
            sys.exit()
        # 当前的elf文件中section个数为0，可能传入了错误elf文件，退出
        if elf_object.num_sections() == 0:
            print('elf file "%s" error, please check!' % elf_path)
            f_elf.close()
            sys.exit()
        # 保存符号表地址的字典
        symbol_dict = dict()
        # 找到符号表，并保存, self.elf_object.iter_sections() 返回 generator 类型
        for cur_section in elf_object.iter_sections():
            # 如果当前的 section 是 SymbolTableSection 类型，则符号表在该类型中
            if isinstance(cur_section, SymbolTableSection):
                # 遍历符号表中所有的符号，保存为字典
                for symbol in cur_section.iter_symbols():
                    # 获取符号名称和地址地址
                    symbol_name = symbol.name
                    symbol_addr = symbol.entry['st_value']
                    # 非零地址的符号保存到字典中，零地址可能是一些未定义符号
                    if symbol_addr != 0:
                        symbol_dict[symbol_name] = symbol_addr
                # 找到符号表则不再继续循环
                break
        # 关闭文件
        f_elf.close()
        # 返回字典
        return symbol_dict

    def __get_func_list(self, list_path):
        """从列表文件中获取需要生成跳转汇编的有效函数列表"""
        # 打开文件
        with open(list_path, 'r', encoding='utf-8', errors='ignore') as f_file:
            file_content = f_file.read()
            f_file.close()
        # 保存要强制链接的函数列表
        func_set = set()
        func_list = list()
        # 保存要头文件列表
        header_set = set()
        header_list = list()
        # 保存宏定义列表
        macro_list = list()
        # 头文件的匹配，同时捕获include用于判断是头文件还是条件编译，获取文件名查重
        header_pattern = r'#[ \t]*(include)[ \t]*(?:"([^"]+)"|<([^>]+)>)'
        # 匹配条件编译 #if, #elif, #else, #endif
        conditional_pattern = r'#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b.*'
        # 匹配宏定义 #define
        macro_pattern = r'#[ \t]*(define)[ \t]*[a-zA-Z_][a-zA-Z0-9_]*[ \t]*[0-9]*'
        # 函数名的匹配
        func_pattern = r'[a-zA-Z_][a-zA-Z0-9_]*'
        # 匹配 "//" 及之后的所有字符，直到行尾
        comment_pattern_single = r'//.*'
        # 匹配 "/*" 和 "*/" 及其之间的任何字符（包括换行符），使用非贪婪匹配
        comment_pattern_multi = r'/\*[\s\S]*?\*/'
        # 将注释的两种模式合并
        comment_pattern = f"{comment_pattern_single}|{comment_pattern_multi}"
        # 正则匹配头文件包含、条件编译、函数名、注释
        combined_pattern = f"({header_pattern})|({conditional_pattern})|({macro_pattern})|({func_pattern})|({comment_pattern})"
        pattern = re.compile(combined_pattern)
        # 条件编译的嵌套，条件编译期间的函数无条件加入
        conditional_nest = 0
        # 保存临时的头文件列表和函数列表，用于保存条件编译中的头文件和函数内容
        tmp_func_list = list()
        tmp_header_list = list()
        # 临时列表需要更新到正式列表的标志
        tmp_func_flag = False
        tmp_header_flag = False
        # 所有内容都进行匹配
        match_list = pattern.finditer(file_content)
        # 遍历每个匹配
        for match in match_list:
            # 获取匹配到的字符串
            match_str = match.group()
            # 如果是注释，直接丢弃
            if match_str.startswith(('/*', '//')):
                continue
            # 头文件包含或者是条件编译
            if match_str.startswith('#'):
                # 获取捕获字符串，确定是头文件、条件编译、宏定义
                # 头文件捕获和条件编译捕获不再同一个位置
                if match.group(2) is not None:
                    capture_str = match.group(2)
                elif match.group(6) is not None:
                    capture_str = match.group(6)
                else:
                    capture_str = match.group(8)
                # 头文件内容
                if capture_str == 'include':
                    # 不再条件编译范围内，查重加入字典中
                    if conditional_nest == 0:
                        # 获取到头文件名称，根据匹配情况返回非空的那个组
                        header = match.group(3) if match.group(3) is not None else match.group(4)
                        # 查重加入字典
                        if header not in header_set:
                            header_set.add(header)
                            header_list.append(match_str)
                    # 在条件编译范围内
                    else:
                        # 加入到临时列表中，一次性写入
                        tmp_header_list.append(match_str)
                        # 设置标志，需要更新到正式列表中
                        tmp_header_flag = True
                # 宏定义内容
                elif capture_str == 'define':
                    # 加入到宏定义列表中
                    macro_list.append(match_str)
                # 条件编译开始位置
                elif capture_str.startswith('if'):
                    # 增加条件编译嵌套
                    conditional_nest += 1
                    # 加入到临时列表中，条件编译结束后一并写入
                    tmp_func_list.append(match_str)
                    tmp_header_list.append(match_str)
                # 条件编译结束的位置
                elif capture_str == 'endif':
                    # 减少条件编译嵌套
                    conditional_nest -= 1
                    # 加入到临时列表中，条件编译结束后一并写入
                    tmp_func_list.append(match_str)
                    tmp_header_list.append(match_str)
                    # 如果嵌套层级已经到零，则把临时列表的内容写入到正式列表中
                    if conditional_nest == 0:
                        # 临时列表更新到正式列表中
                        if tmp_func_flag is True:
                            func_list.extend(tmp_func_list)
                        if tmp_header_flag is True:
                            header_list.extend(tmp_header_list)
                        # 写入标志置false
                        tmp_func_flag = False
                        tmp_header_flag = False
                        # 临时列表内容清空
                        tmp_func_list = list()
                        tmp_header_list = list()
                # #else #elif 等条件编译内容
                else:
                    # 加入到临时列表中，条件编译结束后一并写入
                    tmp_func_list.append(match_str)
                    tmp_header_list.append(match_str)
            # 匹配到函数名
            else:
                # 条件编译中间的内容加入到临时列表
                if conditional_nest > 0:
                    # 加入到临时列表中，一次性写入
                    tmp_func_list.append(match_str)
                    # 设置标志，需要更新到正式列表中
                    tmp_func_flag = True
                elif match_str not in func_set:
                    func_set.add(match_str)
                    func_list.append(match_str)
        # 返回两个列表
        return (func_list, header_list, macro_list)

    def __get_undefined_symbol_list(self):
        """从object文件中找到未定义的符号列表"""
        # 保存符号列表
        symbol_list = list()
        # 读取编译生成的obj文件，找到未定义符号
        with open(self.object_path, 'rb') as f_in:
            elffile = ELFFile(f_in)
            # 遍历 section，找到未定义符号，保存到列表中
            for cur_section in elffile.iter_sections():
                if isinstance(cur_section, SymbolTableSection):
                    for symbol in cur_section.iter_symbols():
                        if symbol['st_shndx'] == 'SHN_UNDEF' and symbol.name != '':
                            symbol_list.append(symbol.name)
                    break
        # 返回符号列表
        return symbol_list

    def generate_import_link(self):
        """生成所有跳转函数集合的汇编文件"""
        # 获取符号列表
        symbol_list = self.__get_undefined_symbol_list()
        # 保存写入链接脚本的内容
        link_str = str()
        # 遍历列表，获取每个未定义符号的地址
        for symbol in symbol_list:
            # 符号不在字典中，这种情况不该存在
            if symbol not in self.symbol_dict:
                raise ValueError(f'undefined symbol not linked, symbol is "{symbol}"')
            # 如果获取到的地址是0，说明这个地址没有链接到，跳过此符号生成，输出warning
            if symbol not in self.symbol_dict:
                raise KeyError('the symbol "{symbol}" not found!')
            # 从函数字典中获取函数地址
            addr = self.symbol_dict[symbol]
            # 写入内容
            link_str += f"{symbol} = 0x{addr:X};\n"
        # 返回要写入的文件内容
        return link_str

    def generate_link_response(self):
        """生成供链接器使用的强制链接的响应文件"""
        # 获取符号列表
        symbol_list = self.__get_undefined_symbol_list()
        # 保存写入文件的内容
        rsp_str = str()
        # 遍历列表，组成写入文件的内容
        for symbol in symbol_list:
            rsp_str += f'-Wl,-u,{symbol}\n'
        # 返回要写入的字符串
        return rsp_str

    def generate_export_asm_func(self):
        """生成确保导出函数被链接到的函数"""
        # 宏定义列表写入到字符串
        file_str = '\n'.join(self.macro_list) + '\n\n'
        # 头文件列表写入到字符串
        file_str += '\n'.join(self.header_list) + '\n\n'
        # 所有用到的函数，在汇编中对其进行声明
        declaration_str = str()
        # 函数开始部分
        function_str = str()
        function_str += '\n'
        function_str += '  .section  .text.mandatory_link_func,\"ax\",%progbits\n'
        function_str += '  .global   mandatory_link_func\n'
        function_str += '  .align    2\n'
        function_str += '  .type     mandatory_link_func, %function\n'
        function_str += 'mandatory_link_func:\n'
        # 函数中间部分，把需要的函数都链接到
        for func in self.func_list:
            # 如果是以'#'开头，认为是宏定义行，同时写入到声明和函数中
            if func.startswith('#'):
                declaration_str += func + '\n'
                function_str += func + '\n'
            else:
                # 通过增加调用实现链接到此函数
                declaration_str += f'.extern  {func}\n'
                function_str += f'  la      a0, {func}\n'
        # 函数结束部分
        function_str += '  ret\n'
        function_str += '.size  mandatory_link_func, .-mandatory_link_func\n'
        # 增加到文件中，先增加声明内容
        file_str += declaration_str + function_str
        # 返回要写入的文件内容
        return file_str

    def generate_export_c_func(self):
        """生成确保导出函数被链接到的函数"""
        # 宏定义列表写入到字符串
        file_str = '\n'.join(self.macro_list) + '\n\n'
        # 头文件列表写入到字符串
        file_str += '\n'.join(self.header_list) + '\n\n'
        # 保存无声明函数的字符串
        sym_add_str = str()
        # 为符号增加声明和调用点
        for symbol in self.func_list:
            # 如果是以'#'开头，认为是宏定义行，直接写入到函数中
            if symbol.startswith('#'):
                file_str += symbol + '\n'
                sym_add_str += symbol + '\n'
            # 函数中间部分，用以把需要的函数都链接到
            else:
                file_str += f'extern long {symbol};\n'
                sym_add_str += f'    value += (long) &{symbol};\n'
        # 函数开始部分
        file_str += '\n\n'
        file_str += 'long mandatory_link_func(void)\n'
        file_str += '{\n'
        file_str += '    long value = 0;\n'
        file_str += '\n'
        # 加入未声明函数
        file_str += sym_add_str
        # 函数结束部分
        file_str += '\n'
        file_str += '    return value;\n'
        file_str += '}\n'
        # 返回要写入的文件内容
        return file_str

    def generate(self):
        """根据参数选项生成文件"""
        # 保存要写入的字符串
        write_str = str()
        # 根据选项生成文件
        if self._import is True:
            write_str = self.generate_import_link()
        if self._export is True:
            write_str = self.generate_export_c_func()
        if self._response is True:
            write_str = self.generate_link_response()
        # 路径不存在则创建
        dir_path = os.path.dirname(self.output_file)
        if not os.path.exists(dir_path):
            os.makedirs(dir_path)
        # 写入到文件中
        with open(self.output_file, 'w', encoding='utf-8') as f_out:
            f_out.write(write_str)


def set_argument(par_list=None):
    # 生成对象，并添加对脚本的描述
    # description: 在参数帮助文档之前显示的文本（默认值：无）
    # prog: 程序的名称，默认就是 sys.arg[0]
    # '%(prog)s': 这个字符串在帮助消息里表示的是程序名称
    description = '%(prog)s is use for generate export function'
    parser = argparse.ArgumentParser(description=description)
    # name or flags - 一个命名或者一个选项字符串的列表，例如 foo 或 -f, --foo。
    # action - 当参数在命令行中出现时使用的动作基本类型。
    # nargs - 命令行参数应当消耗的数目。
    # const - 被一些 action 和 nargs 选择所需求的常数。
    # default - 当参数未在命令行中出现并且也不存在于命名空间对象时所产生的值。
    # type - 命令行参数应当被转换成的类型。
    # choices - 可用的参数的容器。
    # required - 此命令行选项是否可省略 （仅选项可用），True为不可忽略，False为可忽略。
    # help - 一个此选项作用的简单描述。
    # metavar - 在使用方法消息中使用的参数值示例。
    # dest - 被添加到 parse_args() 所返回对象上的属性名。
    # 增加设置输入文件的参数
    # required=True 指示该选项必须存在
    # metavar='<file>' 指示该参数的使用示例

    # 创建互斥组并强制选一个
    group = parser.add_mutually_exclusive_group(required=True)

    # export list中需要导出的符号生成为链接脚本
    help_str = 'generate the link script from the export list that'
    group.add_argument('-i', '--import', action='store_true', default=False, dest='_import', help=help_str)
    # 生成确保export list中函数一定能够被链接的函数
    help_str = 'generate a file that ensures that all the functions in the export list can be properly linked.'
    group.add_argument('-e', '--export', action='store_true', default=False, dest='_export', help=help_str)
    # 生成链接时使用的强制链接的函数的参数
    help_str = 'generate response file for mandatory link functions.'
    group.add_argument('-r', '--response', action='store_true', default=False, dest='_response', help=help_str)

    # 设置elf文件路径
    help_str = 'set target file path, actually the path of the elf file'
    parser.add_argument('-t', '--target', type=str, default=None, required=False, metavar='<file>', help=help_str)
    # 设置导出函数的列表文件路径
    help_str = 'set export list file path'
    parser.add_argument('-l', '--list', type=str, default=None, required=False, metavar='<file>', help=help_str)
    # 设置编译生成的文件，用于获取未定义的函数列表
    help_str = 'set object file path, generated by compiler'
    parser.add_argument('-c', '--object', type=str, default=None, required=False, metavar='<file>', help=help_str)
    # 设置输出文件的路径
    help_str = 'set output path'
    parser.add_argument('-o', '--output', type=str, default=None, required=True, metavar='<file>', help=help_str)

    # 根据参数，传入参数列表，如果没有手动传入参数，则从命令行参数列表中获取
    args = parser.parse_args(par_list)
    # 解析后验证条件依赖，-t/--target 选项尽在 -a/--asm 使能时生效
    if args._import and args.target is None:
        parser.error('the following argument is required when using -i/--import: -t/--target')
    elif args._export and args.list is None:
        parser.error('the following argument is required when using -e/--exmport: -l/--list')
    elif args._response and args.object is None:
        parser.error('the following argument is required when using -r/--response: -c/--object')
    # 返回参数列表
    return args


def main():
    # 从命令行中获取参数
    par_list = None
    # 手动设置参数，调试用
    # par_str = r'-i -t ap.elf -l xy_export.list -o import_func.ld'
    # par_str = r'-e -l xy_export.list -o mandatory_link.c'
    # par_list = par_str.split()
    # 获取参数列表
    args = set_argument(par_list)
    # 创建对象实例
    mandatory_link = MandatoryLink(args._import, args._export, args._response, args.target, args.object, args.list, args.output)
    mandatory_link.generate()


if __name__ == '__main__':
    main()
