import hashlib
import hmac
import json
import os
from collections import namedtuple


class Package:
    Image_Header = namedtuple('Image_Header', ['Header', 'LogSize', 'ImageSize', 'password'])

    Img_Header = namedtuple('Img_Header', ['MagicNum', 'Version', 'SHA', 'Imgs_Num'])

    Bin_Header = namedtuple('Bin_Struct',
                            ['ImageCore', 'ImageLoadAddr', 'ImageExecAddr', 'ImageType', 'ImageNameLen',
                             'FileName', 'ImageLen', 'SHA'])

    Img_Struct = namedtuple('Img_Struct', ['Img_Header', 'Bin_Headers', 'Contents'])

    Img_File = namedtuple('Imgs_Collection', ['FileName', 'Contents'])

    Image_Struct = namedtuple('Image_Struct', ['Img_Header', 'log_file', 'MergedImg', 'Json', 'FileNames', 'Imgs'])

    def __init__(self):
        self.Image = None
        self.JConfig = None

    def unpack(self, _image, _directory, password=None):
        _directory = '/'.join([os.path.abspath(_directory), os.path.splitext(os.path.basename(_image))[0]])
        print(_directory)

        if not os.path.exists(_directory):
            os.makedirs(_directory, exist_ok=True)

        print('start unpack image {} to {}...'.format(_image, _directory))

        with open(_image, mode='rb') as file:
            ########################################################################################
            _header = file.read(4)
            _log_size = file.read(4)
            _image_size = file.read(4)

            _password = ''
            if _header == 'PASS':
                _password = file.read(32)
                if _password != bytes(password):
                    print('password error...')
                    return

            _image_header = self.Image_Header._make([_header, _log_size, _image_size, _password])
            print('unpack image header ok...')

            ########################################################################################
            _logfile = open('/'.join([_directory, 'loginfo.info']), mode='wb+')
            _log_content = file.read(int.from_bytes(_log_size, byteorder='little'))
            _logfile.write(_log_content)
            _logfile.close()
            print('unpack loginfo.info ok...')

            #######################################################################################

            _magic_num = file.read(4)
            print(int.from_bytes(_magic_num, byteorder='little'))
            _version = file.read(4)
            _sha = file.read(20)

            res = file.read(int.from_bytes(_image_size, byteorder='little') - 28)
            digest_maker = hmac.new('XY1100'.encode(), res, hashlib.sha1)

            if _sha == digest_maker.digest():
                print("check SHA OK")
            else:
                print('check  SHA FAIL')
                return
            #######################################################################################

            file.seek(-(int.from_bytes(_image_size, byteorder='little') - 28), 1)
            _img_num = file.read(4)
            print(int.from_bytes(_img_num, byteorder='little'))

            _img_header = self.Img_Header._make([_magic_num, _version, _sha, _img_num])

            _bin_headers = list()

            for i in range(int.from_bytes(_img_num, byteorder='little')):
                _image_core = file.read(4)
                _image_load_addr = file.read(4)
                _image_exec_addr = file.read(4)
                _image_type = file.read(4)
                _image_name_len = file.read(4)
                _image_name = file.read(int.from_bytes(_image_name_len, byteorder='little'))
                _image_len = file.read(4)
                _image_sha = file.read(20)
                _bin_headers.append(self.Bin_Header._make(
                    [_image_core, _image_load_addr, _image_exec_addr, _image_type, _image_name_len, _image_name,
                     _image_len,
                     _image_sha]))

            _bin_Coutents = list()
            for i in range(int.from_bytes(_img_num, byteorder='little')):
                _content = file.read(int.from_bytes(_bin_headers[i].ImageLen, byteorder='little'))
                _bin_Coutents.append(_content)

            _merged_img = self.Img_Struct._make([_img_header, _bin_headers, _bin_Coutents])
            print('unpack merged img ok...')
            ##########################################################################################
            _json_size = file.read(4)
            _json_content = file.read(int.from_bytes(_json_size, byteorder='little'))

            _json_file = open('/'.join([_directory, 'config.json']), mode='wb')
            _json_file.write(_json_content)
            _json_file.close()

            self.JConfig = json.loads(_json_content.decode())
            print('unpack config.json ok...')
            ##########################################################################################

            _correspond_img_name = list()
            _file_count = file.read(4)

            for i in range(int.from_bytes(_file_count, byteorder='little')):
                _name_size = file.read(4)
                _name = file.read(int.from_bytes(_name_size, byteorder='little'))
                _correspond_img_name.append(_name)

            ##########################################################################################

            _imgs = list()
            _file_count = file.read(4)
            for i in range(int.from_bytes(_file_count, byteorder='little')):
                _name_size = file.read(4)
                _name = file.read(int.from_bytes(_name_size, byteorder='little'))
                _content_size = file.read(4)
                _content = file.read(int.from_bytes(_content_size, byteorder='little'))
                _img_file = open('/'.join([_directory, _name.decode()]), mode='wb')
                _img_file.write(_content)
                _img_file.close()
                _imgs.append(self.Img_File._make([_name, _content]))
                print('unpack {} ok...'.format(_name.decode()))

            ##########################################################################################

            self.Image = self.Image_Struct._make(
                [_image_header, _log_content, _merged_img, _json_content, _correspond_img_name, _imgs])

            print('unpack ok...')
            ##########################################################################################
        return

    def pack(self, _directory, _image, password=b''):
        if not os.path.exists(_directory):
            print('{} do not exist..'.format(_directory))
            return

        if not os.path.exists(os.path.dirname(_image)):
            os.makedirs(os.path.dirname(_image), exist_ok=True)

        ########################################################################################
        _json_name = '/'.join([_directory, 'config.json'])
        _json_file = open(_json_name, mode='rb')
        if not _json_file:
            print('{} do not exist'.format(_json_name))
            return
        _json_content = _json_file.read()
        _json_file.close()

        self.JConfig = json.loads(_json_content.decode())

        _imgs = list()
        _imgs += [img for img in self.JConfig["internal"]["Images"]]
        _imgs += [img[0] for img in self.JConfig["internal"]["NVs"]]
        _imgs += [img for img in self.JConfig["extends"]["Images"]]
        _imgs += [img[0] for img in self.JConfig["extends"]["NVs"]]
        print(_imgs)
        print('load {} ok...'.format(_json_name))

        ########################################################################################

        _bin_headers = list()
        _bin_contents = bytearray()
        _bin_num = list()
        for __img in _imgs:

            __img = '/'.join([_directory, __img])
            print('load {}...'.format(__img))
            if not os.path.exists(__img):
                print('[Error] {} do not exist...'.format(__img))

            with open(__img, mode='rb') as f:
                f.seek(4 + 4 + 20, 1)
                __bin_num = int.from_bytes(f.read(4), byteorder='little')
                _bin_num.append(__bin_num)
                _sub_bin_headers = list()
                for i in range(__bin_num):
                    _image_core = f.read(4)
                    _image_load_addr = f.read(4)
                    _image_exec_addr = f.read(4)
                    _image_type = f.read(4)
                    _image_name_len = f.read(4)
                    _image_name = f.read(int.from_bytes(_image_name_len, byteorder='little'))
                    _image_len = f.read(4)
                    _image_sha = f.read(20)
                    _sub_bin_headers.append(self.Bin_Header._make(
                        [_image_core, _image_load_addr, _image_exec_addr, _image_type, _image_name_len, _image_name,
                         _image_len,
                         _image_sha]))

                for i in range(__bin_num):
                    _bin_content = f.read(int.from_bytes(_sub_bin_headers[i].ImageLen, byteorder='little'))
                    if int.from_bytes(_sub_bin_headers[i].ImageType, byteorder='little') == 1:
                        _bin_content = Package.str2hex(_bin_content)
                        _new = _sub_bin_headers[i]._replace(
                            ImageLen=int.to_bytes(len(_bin_content), 4, byteorder='little'))
                        _sub_bin_headers.pop(i)
                        _sub_bin_headers.insert(i, _new)
                        digest_maker = hmac.new('XY1100'.encode(), _bin_content, hashlib.sha1)
                        _new = _sub_bin_headers[i]._replace(SHA=digest_maker.digest())
                        _sub_bin_headers.pop(i)
                        _sub_bin_headers.insert(i, _new)
                    _bin_contents += _bin_content
                _bin_headers += _sub_bin_headers

        _total_num = 0
        for __num in _bin_num:
            _total_num += __num

        _img_file = open(_image, mode='wb+')
        _num_byte = int.to_bytes(_total_num, 4, byteorder='little')
        _img_file.write(_num_byte)
        for _header in _bin_headers:
            _img_file.write(_header.ImageCore)
            _img_file.write(_header.ImageLoadAddr)
            _img_file.write(_header.ImageExecAddr)
            _img_file.write(_header.ImageType)
            _img_file.write(_header.ImageNameLen)
            _img_file.write(_header.FileName)
            _img_file.write(_header.ImageLen)
            _img_file.write(_header.SHA)
        _img_file.write(_bin_contents)

        _img_file.seek(0, 0)
        _res = _img_file.read()
        digest_maker = hmac.new('XY1100'.encode(), _res, hashlib.sha1)

        _img_file.seek(0, 0)
        _img_file.write(int.to_bytes(9527, 4, byteorder='little'))
        _img_file.write(int.to_bytes(1, 4, byteorder='little'))
        _img_file.write(digest_maker.digest())
        _img_file.write(_res)

        _image_size = 4 + 4 + 20 + len(_res)
        ########################################################################################
        _img_file.seek(0, 0)
        _img_content = _img_file.read()

        _img_file.seek(0, 0)
        if password == b'':
            _img_file.write('HEAD'.encode())
        else:
            _img_file.write('PASS'.encode())
            for i in range(len(password)):
                password[i] -= (ord('0') + 10 + i)

        print('load loginfo.info...')
        with open(_directory + '/loginfo.info', mode='rb') as _logfile:
            _log_content = _logfile.read()

        _img_file.write(int.to_bytes(len(_log_content), 4, byteorder='little'))
        _img_file.write(int.to_bytes(len(_img_content), 4, byteorder='little'))

        if not password:
            _img_file.write(password)

        _img_file.write(_log_content)
        _img_file.write(_img_content)

        with open(_directory + '/config.json', mode='rb') as _json_file:
            _json_content = _json_file.read()

        _img_file.write(_json_content)

        _img_file.write(int.to_bytes(len(_bin_headers), 4, byteorder='little'))

        for _header in _bin_headers:
            _img_file.write(int.to_bytes(len(_header.ImageNameLen), 4, byteorder='little'))
            _img_file.write(_header.FileName)

        _img_file.write(int.to_bytes(len(_imgs), 4, byteorder='little'))
        for _img in _imgs:
            with open(_directory + '/' + _img, mode='rb') as file:
                _content = file.read()

            _img_file.write(int.to_bytes(len(_img), 4, byteorder='little'))
            _img_file.write(_img.encode())
            _img_file.write(int.to_bytes(len(_content), 4, byteorder='little'))
            _img_file.write(_content)

        print("pack ok..")

    @staticmethod
    def str2hex(src):
        hex_char = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'a', 'b', 'c', 'd', 'e', 'f', 'A', 'B', 'C',
                    'D', 'E', 'F']
        _tmp = bytearray()
        for i in range(len(src)):
            if chr(src[i]) in hex_char:
                _tmp.append(src[i])

        src = bytes.fromhex(_tmp.decode())
        return src


if __name__ == '__main__':
    pack = Package()
pack.unpack('./TAG_V1.0.10_6_SDK_LTCC_XO/TAG_V1.0.10_6_SDK_LTCC_XO.mimgx', './1/')
pack.pack('./1/TAG_V1.0.10_6_SDK_LTCC_XO', './1/TAG_V1.0.10_6_SDK_LTCC_XO.mimgx')
