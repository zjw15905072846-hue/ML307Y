# -*- encoding: utf-8 -*-

import argparse
import configparser
import json
import os
import sys
from enum import IntEnum, unique

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection


class PartitionChecker:
    def __init__(self, name):
        self.name = name
        self.load_addr_alignment = 0x1000


class APPPartitionChecker(PartitionChecker):
    def __init__(self, name, ini_file, elf_file):
        super().__init__(name)
        self.ini_file = ini_file
        self.elf_file = elf_file
        self.elffile = ELFFile(open(self.elf_file, 'rb'))
        self.load_partition_info()

    def load_partition_info(self):
        conf = configparser.ConfigParser()
        conf.read(self.ini_file)
        self.flash_begin = int(
            conf.get('global', 'FLASH_BEGIN', fallback='0'), 16)
        self.flash_len = int(conf.get('global', 'FLASH_LEN', fallback='0'), 16)
        self.partition_len = int(
            conf.get('global', 'PARTITION_LEN', fallback='0'), 16)
        self.psram_len = int(conf.get('global', 'PSRAM_LEN', fallback='0'), 16)

        self.psram_bss_start_limit = int(
            conf.get('global', 'PSRAM_BSS_START_LIMIT', fallback='0'), 16)
        self.psram_bss_end_limit = int(
            conf.get('global', 'PSRAM_BSS_END_LIMIT', fallback='0'), 16)

    def __del__(self):
        if hasattr(self, 'elffile'):
            self.elffile.stream.close()

    def find_symbol_address(self, symbol_name):
        elffile = self.elffile
        for section in elffile.iter_sections():
            if isinstance(section, SymbolTableSection):
                for symbol in section.iter_symbols():
                    if symbol.name == symbol_name:
                        return symbol['st_value']
        return 0

    def check_partition(self, image_infos):
        self.print_common_partition_size()
        if self.check_flash_overflow(image_infos):
            return False
        if self.check_psram_bss_overflow():
            return False
        return True

    def check_psram_bss_overflow(self):
        if self.psram_bss_start_limit != 0:
            psram_bss_start = self.find_symbol_address("__psram_bss_start__")
            if psram_bss_start >= self.psram_bss_start_limit:
                print(f'{self.name} psram bss start address 0x{psram_bss_start:x} exceeds the limit 0x{self.psram_bss_start_limit:x}, no abundant space for boot heap.')
                return True
        if self.psram_bss_end_limit != 0:
            psram_bss_end = self.find_symbol_address(
                "__psram_actual_bss_end__")
            if psram_bss_end >= self.psram_bss_end_limit:
                print(f'{self.name} psram bss end address 0x{psram_bss_end:x} exceeds the limit 0x{self.psram_bss_end_limit:x}, no abundant space for ap heap.')
                return True

    def check_flash_overflow(self, image_infos):
        # print(f'Checking partition for image: {image_infos}')
        actual_total_image_len = 0
        actual_total_image_len_align = 0
        for i, image_info in enumerate(image_infos):
            if i < len(image_infos) - 1:
                image_aligned_len = (image_info['image_len'] + self.load_addr_alignment - 1) & (
                    ~(self.load_addr_alignment - 1))
                actual_total_image_len += image_aligned_len
            else:
                actual_total_image_len += image_info['image_len']
            actual_total_image_len_align += (image_info['image_len'] + self.load_addr_alignment - 1) & (
                ~(self.load_addr_alignment - 1))
        if actual_total_image_len > self.flash_len:
            print(
                f'error: app partition exceeds flash size limit {self.flash_len} bytes, overflow size: {actual_total_image_len - self.flash_len} bytes!')
            return True

        print(f'{self.name} app.bin flash start:0x{self.flash_begin:x}, Total:0x{self.flash_len:x}, Used:0x{actual_total_image_len:x}, Free space:0x{self.flash_len - actual_total_image_len:x}')
        print(f'{self.name} app.bin first FOTA package storage area start:0x{(self.flash_begin + self.flash_len):x}, Length:0x{(self.partition_len - self.flash_len):x}')
        return False

    def print_common_partition_size(self):
        print(
            f'{self.name} app psram total space:0x{self.psram_len:x}, app heap space:0x{self.heap_size:x}')
        print(f'{self.name} psram text space:0x{self.psram_text_size:x}, psram rodata space:0x{self.psram_rodata_size:x}, psram data space:0x{self.psram_data_size:x}, psram bss space:0x{self.psram_bss_size:x}')

    @property
    def heap_size(self):
        psram_heap_begin = self.find_symbol_address("_PSRAM_Free_Begin")
        psram_heap_limit = self.find_symbol_address("_PSRAM_Free_Limit")
        return psram_heap_limit - psram_heap_begin

    @property
    def psram_text_size(self):
        psram_text_begin = self.find_symbol_address("__psram_text_vma_start")
        psram_text_limit = self.find_symbol_address("__psram_text_vma_end")
        return psram_text_limit - psram_text_begin

    @property
    def psram_rodata_size(self):
        psram_rodata_begin = self.find_symbol_address(
            "__psram_rodata_vma_start")
        psram_rodata_limit = self.find_symbol_address("__psram_rodata_vma_end")
        return psram_rodata_limit - psram_rodata_begin

    @property
    def psram_data_size(self):
        psram_data_begin = self.find_symbol_address("__psram_data_vma_start")
        psram_data_limit = self.find_symbol_address("__psram_data_vma_end")
        return psram_data_limit - psram_data_begin

    @property
    def psram_bss_size(self):
        psram_bss_begin = self.find_symbol_address("__psram_bss_vma_start")
        psram_bss_limit = self.find_symbol_address("__psram_bss_vma_end")
        return psram_bss_limit - psram_bss_begin


class APPartitionChecker(PartitionChecker):
    def __init__(self, name, ini_file, elf_file):
        super().__init__(name)
        self.ini_file = ini_file
        self.elf_file = elf_file
        self.elffile = ELFFile(open(self.elf_file, 'rb'))
        self.load_partition_info()

    def load_partition_info(self):
        conf = configparser.ConfigParser()
        conf.read(self.ini_file)
        self.flash_begin = int(
            conf.get('global', 'FLASH_BEGIN', fallback='0'), 16)
        self.flash_len = int(conf.get('global', 'FLASH_LEN', fallback='0'), 16)
        self.psram_len = int(conf.get('global', 'PSRAM_LEN', fallback='0'), 16)
        self.fs_size = int(conf.get('global', 'FS_SIZE', fallback='0'), 16)
        self.fota_size = int(conf.get('global', 'FOTA_SIZE', fallback='0'), 16)

        self.psram_bss_start_limit = int(
            conf.get('global', 'PSRAM_BSS_START_LIMIT', fallback='0'), 16)
        self.psram_bss_end_limit = int(
            conf.get('global', 'PSRAM_BSS_END_LIMIT', fallback='0'), 16)

    def __del__(self):
        if hasattr(self, 'elffile'):
            self.elffile.stream.close()

    def check_partition(self, image_infos):
        self.print_common_partition_size()
        if self.check_flash_overflow(image_infos):
            return False
        if self.check_psram_bss_overflow():
            return False
        return True

    def find_symbol_address(self, symbol_name):
        elffile = self.elffile
        for section in elffile.iter_sections():
            if isinstance(section, SymbolTableSection):
                for symbol in section.iter_symbols():
                    if symbol.name == symbol_name:
                        return symbol['st_value']
        return 0

    def flash_free_size(self, used_size):
        free = self.flash_len - used_size
        if free < 0x2000:
            print(f'{self.name} flash free space is less than 8K bytes!!!!!!!!!!')
        return free

    def check_psram_bss_overflow(self):
        if self.psram_bss_start_limit != 0:
            psram_bss_start = self.find_symbol_address("__psram_bss_start__")
            if psram_bss_start >= self.psram_bss_start_limit:
                print(f'{self.name} psram bss start address 0x{psram_bss_start:x} exceeds the limit 0x{self.psram_bss_start_limit:x}, no abundant space for boot heap.')
                return True
        if self.psram_bss_end_limit != 0:
            psram_bss_end = self.find_symbol_address(
                "__psram_actual_bss_end__")
            if psram_bss_end >= self.psram_bss_end_limit:
                print(f'{self.name} psram bss end address 0x{psram_bss_end:x} exceeds the limit 0x{self.psram_bss_end_limit:x}, no abundant space for ap heap.')
                return True

    def check_flash_overflow(self, image_infos):
        actual_total_image_len = 0
        actual_total_image_len_align = 0
        for i, image_info in enumerate(image_infos):
            if i < len(image_infos) - 1:
                image_aligned_len = (image_info['image_len'] + self.load_addr_alignment - 1) & (
                    ~(self.load_addr_alignment - 1))
                actual_total_image_len += image_aligned_len
            else:
                actual_total_image_len += image_info['image_len']
            actual_total_image_len_align += (image_info['image_len'] + self.load_addr_alignment - 1) & (
                ~(self.load_addr_alignment - 1))
        if actual_total_image_len >= self.flash_len:
            print(f'Warning!!! {self.name} ap code flash space not enough, start:0x{self.flash_begin:x}, total len:0x{self.flash_len:x}, '
                  f'code flash end addr:0x{(self.flash_begin + self.flash_len):x}, '
                  f'overflow len:0x{(actual_total_image_len - self.flash_len):x}')
            print(
                f'Please close unused module in memlayout/{self.name}/define.cmake or export.list!')
            return True
        else:
            print(f'{self.name} ap code flash start:0x{self.flash_begin:x}, total len:0x{self.flash_len:x}, have used:0x{actual_total_image_len_align:x}, free space:0x{self.flash_free_size(actual_total_image_len_align):x}')
            return False

    def print_common_partition_size(self):
        print(f'{self.name} fs partition flash size: 0x{self.fs_size:x}')
        print(f'{self.name} ota package partition size: 0x{self.fota_size:x}')
        print(
            f'{self.name} ap psram total space:0x{self.psram_len:x}, ap heap space:0x{self.heap_size:x}')
        print(f'{self.name} psram text space:0x{self.psram_text_size:x}, psram data space:0x{self.psram_data_size:x}, psram bss space:0x{self.psram_bss_size:x}')

    @property
    def heap_size(self):
        psram_heap_begin = self.find_symbol_address("_PSRAM_Heap_Begin")
        psram_heap_limit = self.find_symbol_address("_PSRAM_Heap_Limit")
        return psram_heap_limit - psram_heap_begin

    @property
    def psram_text_size(self):
        psram_text_begin = self.find_symbol_address("__psram_stext")
        psram_text_limit = self.find_symbol_address("_psram_sidata")
        return psram_text_limit - psram_text_begin

    @property
    def psram_data_size(self):
        psram_data_begin = self.find_symbol_address("__psram_data_start__")
        psram_data_limit = self.find_symbol_address("__psram_data_end__")
        return psram_data_limit - psram_data_begin

    @property
    def psram_bss_size(self):
        psram_bss_begin = self.find_symbol_address("__psram_bss_start__")
        psram_bss_limit = self.find_symbol_address("__psram_actual_bss_end__")
        return psram_bss_limit - psram_bss_begin


def check_partition(temp_info_file, name, ini_file, elf_file, project_type):
    if not os.path.exists(temp_info_file):
        print(f'Temporary info file {temp_info_file} does not exist.')
        return False
    if not os.path.exists(ini_file):
        print(f'INI file {ini_file} does not exist.')
        return False

    temp_info = []
    with open(temp_info_file, 'r', encoding='utf-8') as f:
        temp_info = json.load(f)
    if project_type == 'app':
        if not APPPartitionChecker(name, ini_file, elf_file).check_partition(temp_info):
            print('App partition check failed.')
            return False
    elif project_type == 'ap':
        if not APPartitionChecker(name, ini_file, elf_file).check_partition(temp_info):
            print('AP partition check failed.')
            return False

    return True


def main():
    parser = argparse.ArgumentParser(description='Partition check script')
    parser.add_argument('-t', '--temp_info_file', required=True,
                        help='File containing temporary image information')
    parser.add_argument('-i', '--ini_file', required=True,
                        help='Path to the configuration ini file')
    parser.add_argument('-n', '--name', required=True,
                        help='Name of the project')
    parser.add_argument('-p', '--project_type', required=False, type=str, choices=['ap', 'app'],
                        help='Type of project', default='ap')
    parser.add_argument('-e', '--elf_file', required=False, type=str,
                        help='Path to the ELF file for AP partition check (optional)', default='')

    args = parser.parse_args()
    if check_partition(args.temp_info_file, args.name, args.ini_file, args.elf_file, args.project_type):
        pass
    else:
        sys.exit(-1)


if __name__ == '__main__':
    main()
