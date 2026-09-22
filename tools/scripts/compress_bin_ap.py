# -*- encoding: utf-8 -*-
import configparser
import ctypes
import os
import platform
import re
import struct
import sys

uncompressed_size_step = 1
compressed_bin_magic_number = 0x1A2B3C4D

# lz4hc configuration parameters
lz4hc_compression_level = 9

# lzma configuration parameters
probs_size = 5
level = 6
lc = 1
lp = 0
pb = 2
fb = -1
num_threads = 2
dict_size = 1 << (level + 10)

bin_align_size = 64


class XYCompressor:

    def __init__(self, dll_dir, ini_file, single_compressed_size, bins_dir):
        self.single_compressed_size = single_compressed_size
        self.bins_dir = bins_dir
        self.dll = None
        self.load_dll(dll_dir)
        self.image_infos = self.get_image_info(ini_file)

    def load_dll(self, dll_dir):
        if platform.system().lower() == 'windows':
            if sys.version_info.major >= 3 and sys.version_info.minor >= 8:
                self.dll = ctypes.CDLL(os.path.join(
                    dll_dir, 'xycompress.dll'), winmode=0)
            else:
                self.dll = ctypes.CDLL(os.path.join(dll_dir, 'xycompress.dll'))
        elif platform.system().lower() == 'linux':
            self.dll = ctypes.cdll.LoadLibrary(
                os.path.join(dll_dir, 'libxycompress.so'))
        else:
            print("Unsupported platform.")
            sys.exit(-1)
        self.dll.LzmaCompress.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint32), ctypes.c_char_p, ctypes.c_uint32,
                                          ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint32), ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
        self.dll.LzmaCompress.restype = ctypes.c_int

        self.dll.XYCompress_LZ4HC.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
        self.dll.XYCompress_LZ4HC.restype = ctypes.c_int

    def get_image_info(self, ini_file):
        conf = configparser.ConfigParser()
        conf.read(ini_file)
        image_sections = [section for section in conf.sections(
        ) if re.match(r'^image\d+$', section)]

        image_infos = []
        for image_section in image_sections:
            image_infos.append((conf.get(image_section, 'image_name'), bool(
                conf.get(image_section, 'is_compress').lower() == 'true')))

        return image_infos

    def single_compress_lzma(self, input_data):
        src_data = input_data
        src_size = ctypes.c_uint32(len(src_data))
        dst_size = ctypes.c_uint32(
            src_size.value + (src_size.value >> 4) + 128)
        dst_data = ctypes.create_string_buffer(dst_size.value)
        out_probs = ctypes.create_string_buffer(probs_size)
        out_probs_size = ctypes.c_uint32(probs_size)
        ret = self.dll.LzmaCompress(dst_data, ctypes.byref(dst_size), src_data, src_size,
                                    out_probs, ctypes.byref(out_probs_size), level, lc, lp, pb, fb, num_threads)
        if ret != 0:
            print("Compress failed.")
            sys.exit(-1)
        return out_probs_size.value + dst_size.value, out_probs.raw + dst_data.raw[:dst_size.value]

    def single_compress_lz4hc(self, input_data):
        src_data = input_data
        src_size = ctypes.c_int32(len(src_data))
        dst_size = ctypes.c_int32(src_size.value + src_size.value//255 + 16)
        dst_data = ctypes.create_string_buffer(dst_size.value)
        ret = self.dll.XYCompress_LZ4HC(
            src_data, dst_data, src_size, dst_size, ctypes.c_int32(lz4hc_compression_level))
        if ret == 0:
            print("Compress failed.")
            sys.exit(-1)
        return ret, dst_data.raw[:ret]

    def single_compress(self, input_data):
        return self.single_compress_lzma(input_data)
        # return self.single_compress_lz4hc(input_data)

    def bisect_compress_bin(self, input_data, size_list, left_index, right_index, failed_index_set):
        if left_index == right_index:
            if left_index in failed_index_set:
                compressed_size, compressed_data = self.single_compress(
                    input_data[:size_list[left_index - 1]])
                return size_list[left_index - 1], compressed_size, compressed_data
            compressed_size, compressed_data = self.single_compress(
                input_data[:size_list[left_index]])
            return size_list[left_index], compressed_size, compressed_data
        mid_index = (left_index + right_index) // 2
        compressed_size, compressed_data = self.single_compress(
            input_data[:size_list[mid_index]])
        if compressed_size > self.single_compressed_size:
            failed_index_set.add(mid_index)
            return self.bisect_compress_bin(input_data, size_list, left_index, mid_index, failed_index_set)
        return self.bisect_compress_bin(input_data, size_list, mid_index + 1, right_index, failed_index_set)

    def compress_bin(self, input_file):
        # print(f'Compressing {input_file}...')
        origin_file = f'{os.path.abspath(input_file)}.origin'
        if os.path.exists(origin_file):
            os.remove(origin_file)
        os.rename(input_file, origin_file)
        output_file = os.path.abspath(input_file)

        with open(origin_file, 'rb') as file_in:
            input_data = file_in.read()
        offset = 0
        remain_size = len(input_data)
        res_data_list = []
        while remain_size > 0:
            if remain_size > self.single_compressed_size:
                compressed_size, compressed_data = self.single_compress(
                    input_data[offset:offset + remain_size])
                if compressed_size <= self.single_compressed_size:
                    res_data_list.append(
                        (remain_size, compressed_size, compressed_data))
                    # print(remain_size, compressed_size)
                    break
                size_list = list(range(self.single_compressed_size,
                                       remain_size, uncompressed_size_step))
                size_list.append(remain_size)
                uncompressed_size, compressed_size, compressed_data = self.bisect_compress_bin(
                    input_data[offset:offset + remain_size], size_list, 0, len(size_list) - 1, set([len(size_list) - 1]))
                offset += uncompressed_size
                remain_size -= uncompressed_size
                res_data_list.append(
                    (uncompressed_size, compressed_size, compressed_data))
                # print(uncompressed_size, compressed_size)
            else:
                compressed_size, compressed_data = self.single_compress(
                    input_data[offset:offset + remain_size])
                res_data_list.append(
                    (remain_size, compressed_size, compressed_data))
                # print(remain_size, compressed_size)
                remain_size = 0
                break

        with open(output_file, 'wb') as file_out:
            info_bytes_list = []
            for i, (uncompressed_size, compressed_size, compressed_data) in enumerate(res_data_list):
                file_out.write(compressed_data)
                if i < len(res_data_list) - 1:
                    aligned_size = self.single_compressed_size - compressed_size
                    file_out.write(b'\xFF' * aligned_size)
                else:
                    remainder = compressed_size % bin_align_size
                    if remainder != 0:
                        aligned_size = bin_align_size - remainder
                        file_out.write(b'\xFF' * aligned_size)
                    else:
                        aligned_size = 0
                info_bytes_list.append(struct.pack(
                    '<III', uncompressed_size, compressed_size, compressed_size + aligned_size))
                # print(uncompressed_size, compressed_size,
                #       compressed_size + aligned_size)
            info_bytes = b''.join(info_bytes_list)
        return info_bytes, len(info_bytes_list)

    def compress_bins(self):
        bin_info_bytes_list = []
        for image_name, is_compress in self.image_infos:
            input_file = os.path.join(self.bins_dir, image_name)
            if is_compress:
                bin_info_bytes_list.append(
                    self.compress_bin(input_file))
            else:
                bin_info_bytes_list.append((b'', 0))
        first_bin = os.path.join(self.bins_dir, self.image_infos[0][0])
        with open(first_bin, 'rb') as f:
            first_bin_content = f.read()
        with open(f'{first_bin}', 'wb') as f:
            header_length = 0
            f.write(struct.pack('<III', compressed_bin_magic_number, 0,
                    len(bin_info_bytes_list)))
            header_length += 12
            total_info_bytes_length = 0
            for info_bytes, info_bytes_count in bin_info_bytes_list:
                if info_bytes_count == 0:
                    f.write(struct.pack('<I', 0))
                    header_length += 4
                    total_info_bytes_length += 4
                else:
                    f.write(struct.pack('<I', 1))
                    f.write(struct.pack('<I', len(info_bytes)))
                    f.write(info_bytes)
                    header_length += 8 + len(info_bytes)
                    total_info_bytes_length += 8 + len(info_bytes)
            f.seek(4, os.SEEK_SET)
            f.write(struct.pack('<I', total_info_bytes_length))
            f.seek(0, os.SEEK_END)
            f.write(b'\xFF' * (4096 - header_length))
            f.write(first_bin_content[4096:])


def main():
    if len(sys.argv) < 5:
        print('no enough arguments, exit...')
        sys.exit(-1)

    dll_dir = sys.argv[1]
    ini_file = sys.argv[2]
    if sys.argv[3].startswith('0x') or sys.argv[3].startswith('0X'):
        single_compressed_size = int(sys.argv[3], 16)
    else:
        single_compressed_size = int(sys.argv[3])
    bins_dir = sys.argv[4]

    xy_compressor = XYCompressor(
        dll_dir, ini_file, single_compressed_size, bins_dir)
    xy_compressor.compress_bins()


if __name__ == '__main__':
    main()
