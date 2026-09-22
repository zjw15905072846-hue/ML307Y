import copy
import hashlib
import hmac
import json
import os
import re
import shutil

SHA_LEN = 20
PASSWORD_LEN = 32

FILE_FORMAT_HEX = 1
FILE_FORMAT_BIN = 2


def str2hex(src):
    hex_char = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'a', 'b', 'c', 'd', 'e', 'f', 'A', 'B', 'C',
                'D', 'E', 'F']
    _tmp = bytearray()
    for i in range(len(src)):
        if chr(src[i]) in hex_char:
            _tmp.append(src[i])

    src = bytes.fromhex(_tmp.decode())
    return src


def hex2str(src):
    _tmp = bytearray()
    for i in range(len(src)):
        _tmp += int.to_bytes(int(src[i]), 2, byteorder='little')
        if i % 4 == 0:
            _tmp += b'\r\n'
    return _tmp


def int_to_bytes(value):
    return int.to_bytes(value, 4, byteorder='little')


def str_to_bytes(value):
    return value.encode()


def token_bytearray(buff, size, index):
    return buff[index: index + size], index + size


def token_bytearray_str(buff, size, index):
    return buff[index: index + size].decode(), index + size


def token_bytearray_str_ignore(buff, size, index):
    return buff[index: index + size].decode('utf8', 'ignore'), index + size


def token_bytearray_int(buff, index):
    return int.from_bytes(buff[index: index + 4], byteorder='little'), index + 4


def DebugPrint(str_msg):
    pass
    # print(str_msg)


class Partition:
    def __init__(self):
        self.name = ''
        self.addr = 0
        self.length = 0
        self.erase_flag = 0
        self.id = None

    def setValue(self, name, addr, length, erase_flag, nId=None):
        self.name = name
        self.addr = addr
        self.length = length
        self.erase_flag = erase_flag
        self.id = nId

    def toString(self):
        return 'name: %s, addr: %x, len: %x, erase_flag: %d, id:%s' % (
            self.name, self.addr, self.length, self.erase_flag, self.id)


class SecBootInfo:
    def __init__(self):
        self.name = ''
        self.image_core = 0
        self.check_mode = 0
        self.image_crypto_policy = 0


class LogHeader:
    def __init__(self):
        self.logName = ''
        self.logSize = 0

    def serialize(self):
        _buff = bytearray()
        self.logName = self.logName.ljust(32, '\0')
        _buff += str_to_bytes(self.logName)
        _buff += int_to_bytes(self.logSize)
        return _buff

    def deserialize(self, buff, pos):
        _pos = pos
        self.logName, _pos = token_bytearray_str_ignore(buff, 32, _pos)
        self.logSize, _pos = token_bytearray_int(buff, _pos)
        return True, _pos


class BinHeader:
    def __init__(self):
        self.image_core = 0
        self.image_load_addr = 0
        self.image_exec_addr = 0
        self.image_name_len = 0
        self.image_type = 0
        self.name = ""
        self.file_len = 0
        self.sha = bytearray()

    def serialize(self):
        _buff = bytearray()
        _buff += int_to_bytes(self.image_core)
        _buff += int_to_bytes(self.image_load_addr)
        _buff += int_to_bytes(self.image_exec_addr)
        _buff += int_to_bytes(self.image_type)
        _buff += int_to_bytes(len(self.name))
        _buff += str_to_bytes(self.name)
        _buff += int_to_bytes(self.file_len)
        _buff += self.sha
        return _buff

    def deserialize(self, buff, pos):
        _pos = pos
        self.image_core, _pos = token_bytearray_int(buff, _pos)
        self.image_load_addr, _pos = token_bytearray_int(buff, _pos)
        self.image_exec_addr, _pos = token_bytearray_int(buff, _pos)
        self.image_type, _pos = token_bytearray_int(buff, _pos)
        self.image_name_len, _pos = token_bytearray_int(buff, _pos)
        self.name, _pos = token_bytearray_str(buff, self.image_name_len, _pos)
        self.file_len, _pos = token_bytearray_int(buff, _pos)
        self.sha, _pos = token_bytearray(buff, 20, _pos)
        return True, _pos


class BinFile:
    def __init__(self, parent=""):
        self.header = BinHeader()
        self._content = bytearray()
        self.parent = parent

    @property
    def content(self):
        return self._content

    @content.setter
    def content(self, value):
        self._content = bytearray(value)
        self.header.file_len = len(self._content)
        digest_maker = hmac.new('XY1100'.encode(), self._content, hashlib.sha1)
        self.header.sha = digest_maker.digest()

    def serialize(self):
        pass

    def deserialize(self):
        pass


class ImgHeader:
    def __init__(self):
        self.magic_num = 9527
        self.version = 1
        self.sha = bytearray()  # 20
        self.file_num = 0

    def serialize(self):
        _buff = bytearray()
        _buff += int_to_bytes(self.magic_num)
        _buff += int_to_bytes(self.version)
        _buff += self.sha
        _buff += int_to_bytes(self.file_num)
        return _buff

    def deserialize(self, buff, pos=0):
        _pos = pos
        self.magic_num, _pos = token_bytearray_int(buff, _pos)
        self.version, _pos = token_bytearray_int(buff, _pos)
        self.sha, _pos = token_bytearray(buff, 20, _pos)
        self.file_num, _pos = token_bytearray_int(buff, _pos)
        return True, _pos


class ImgFile:
    def __init__(self):
        self.name = ""
        self.header = ImgHeader()
        self._files = list()  # BinFile

    @property
    def files(self):
        return self._files

    @files.setter
    def files(self, files):
        self.header.file_num = len(files)
        self._files = copy.deepcopy(files)

        digest_maker = hmac.new('XY1100'.encode(), int_to_bytes(self.header.file_num), hashlib.sha1)

        for _file in self._files:
            digest_maker.update(_file.header.serialize())

        for _file in self._files:
            digest_maker.update(_file.content)

        self.header.sha = digest_maker.digest()

    def serialize(self):
        _buff = bytearray()

        _buff += self.header.serialize()
        for i in range(self.header.file_num):
            _buff += self.files[i].header.serialize()

        for i in range(self.header.file_num):
            _buff += self.files[i].content

        return _buff

    def deserialize(self, buff, pos=0):
        _pos = pos
        _is_Ok, _pos = self.header.deserialize(buff, _pos)

        self.files.clear()
        for i in range(self.header.file_num):
            self.files.append(BinFile(self.name))

        for i in range(self.header.file_num):
            _is_Ok, _pos = self.files[i].header.deserialize(buff, _pos)

        for i in range(self.header.file_num):
            self.files[i].content = buff[_pos:_pos + self.files[i].header.file_len]
            _pos += self.files[i].header.file_len

        return True, _pos


class ImageHeader:
    def __init__(self):
        self.header = b'HEAD'
        self.log_size = 0
        self.image_size = 0
        self.password = bytearray()  # size 32

    def serialize(self):
        _buff = bytearray()
        _buff += self.header
        _buff += int_to_bytes(self.log_size)
        _buff += int_to_bytes(self.image_size)

        return _buff

    def deserialize(self, buff, pos=0, password=''):
        _pos = pos
        self.header, _pos = token_bytearray(buff, 4, _pos)
        self.log_size, _pos = token_bytearray_int(buff, _pos)
        self.image_size, _pos = token_bytearray_int(buff, _pos)

        return True, _pos


class Image:
    def __init__(self):
        self.header = ImageHeader()
        self._log_info = bytearray()
        self._merged_img = ImgFile()
        self._org_imgs = list()  # ImgFile
        self.cfg_file = bytearray()

        self.model_name = None

        self.ShaWriteEnable = False
        self.sha = []

        self.img_names = []
        self.mini_boots = []
        self.bin_mini_boots = []
        self.is_download_miniboot = False

        self.others = ["loginfo.info"]

        self.check_version = 0
        self.public_pem_addr = 0
        self.sec_boot_info = []
        self.sec_boot_enable = False
        self.img_header_addr = 0
        self.img_RfDebug_addr = 0
        self.otp_lock = False
        self.first_boot_check = 0
        self.jtag_disable = 0
        self.otp_backup_addr = 0
        self.write_aes_key = False

        # 4100s
        self.n2ndFlashHeaderSrcAddr = 0
        self.n2ndFlashHeaderDstAddr = 0

        self.version = 0
        self.partitions = list()

    def load_from_file(self, file_name, password=''):
        with open(file_name, mode='rb') as file:
            _buff = file.read()
            self.deserialize(_buff, 0, password)
            self.load_config()

    def load_config(self):
        if not self.cfg_file or len(self.cfg_file.strip()) == 0:
            print("警告: cfg_file 为空，无法加载配置")
            return

        try:
            _json_cfg = json.loads(self.cfg_file.decode('utf-8'))
        except UnicodeDecodeError:
            _json_cfg = json.loads(self.cfg_file.decode('gb18030'))

        self.img_names = list()
        self.img_names += [img for img in _json_cfg["internal"]["Images"]]
        self.img_names += [img[0] for img in _json_cfg["internal"]["NVs"]]

        if "MiniBoots" in _json_cfg["internal"]:
            self.mini_boots = [img for img in _json_cfg["internal"]["MiniBoots"]]
            if len(self.mini_boots) > 0:
                self.is_download_miniboot = True
            self.img_names += self.mini_boots

        DebugPrint(self.img_names)

        if "model" in _json_cfg:
            self.model_name = _json_cfg['model']
        else:
            self.model_name = 'XY1100'

        DebugPrint(self.model_name)

        if 'WriteSHA' in _json_cfg['internal']:
            self.ShaWriteEnable = _json_cfg['internal']['WriteSHA']
        else:
            if self.model_name == 'XY1100':
                self.ShaWriteEnable = True
            else:
                self.ShaWriteEnable = False

        if self.ShaWriteEnable:
            self.sha = [int(_json_cfg["internal"]["SHA"][0], 16), self.merged_img.header.sha]

            DebugPrint(self.ShaWriteEnable, self.sha)

        if 'check_version' in _json_cfg['internal']:
            self.check_version = int(_json_cfg['internal']['check_version'])
            DebugPrint('check_version: ' + str(self.check_version))

        if 'PublicPemAddr' in _json_cfg['internal']:
            self.public_pem_addr = int(_json_cfg['internal']['PublicPemAddr'], 16)

        if 'imgs' in _json_cfg['internal']:
            self.sec_boot_info = _json_cfg['internal']['imgs']
            self.sec_boot_enable = True
            DebugPrint(self.sec_boot_info)

        if 'imgHeader' in _json_cfg['internal']:
            self.img_header_addr = int(_json_cfg["internal"]["imgHeader"], 16)
            DebugPrint(self.img_header_addr)

        if 'RF_DEBUG_ADDR' in _json_cfg['internal']:
            self.img_RfDebug_addr = int(_json_cfg["internal"]["RF_DEBUG_ADDR"], 16)

        if 'OtpBackupAddr' in _json_cfg['internal']:
            self.otp_backup_addr = int(_json_cfg["internal"]["OtpBackupAddr"], 16)
        if 'OtpLock' in _json_cfg['internal']:
            self.otp_lock = bool(_json_cfg["internal"]["OtpLock"])
        if 'WriteAesKey' in _json_cfg['internal']:
            self.write_aes_key = bool(_json_cfg["internal"]["WriteAesKey"])
        if '1stBootCheck' in _json_cfg['internal']:
            self.first_boot_check = int(_json_cfg["internal"]["1stBootCheck"])  # 0:不动，1:检查，2:恢复
        if 'JtagDisable' in _json_cfg['internal']:
            self.jtag_disable = int(_json_cfg["internal"]["JtagDisable"])  # 0:不动，1:禁用，2:恢复
        if 'Others' in _json_cfg['internal']:
            self.others.clear()
            self.others += [loginfo for loginfo in _json_cfg["internal"]["Others"]]
            DebugPrint(self.others)

        if '2ndFlashHeaderAddr' in _json_cfg['internal']:
            addr_list = _json_cfg['internal']['2ndFlashHeaderAddr']
            if isinstance(addr_list, list) and len(addr_list) >= 2:
                self.n2ndFlashHeaderSrcAddr = int(addr_list[0], 16)
                self.n2ndFlashHeaderDstAddr = int(addr_list[1], 16)

        if 'memmap' in _json_cfg:
            if 'partitions' in _json_cfg['memmap']:
                Version = 2
                partitions = _json_cfg['memmap']['partitions']
                if 'configVersion' in _json_cfg:
                    configVersion = _json_cfg['configVersion']
                    Version = int(configVersion.split('.')[0])
                    if Version > 1000:
                        Version -= 1000
                self.version = Version
                for partition in partitions:
                    name = partition[0]
                    addr = int(partition[1], 16)
                    length = int(partition[2], 16)
                    erase_flag = int(partition[3])
                    nId = None
                    erasePart = Partition()
                    if len(partition) > 4:
                        nId = partition[4]
                        erasePart.setValue(name, addr, length, erase_flag, nId)
                    else:
                        erasePart.setValue(name, addr, length, erase_flag)
                    self.partitions.append(erasePart)

    def load_config_json(self, json_file):
        with open(json_file, mode='rb') as _file:
            self.cfg_file = _file.read()
            self.load_config()

    def load_from_folder(self, folder_name):
        if not os.path.exists(folder_name):
            print(f"[{folder_name}] not exists!")
            return
        # with open('/'.join([folder_name, 'loginfo.info']), mode='rb') as _file:
        #     self.log_info = _file.read()

        self.load_config_json('/'.join([folder_name, 'config.json']))

        for img in self.org_imgs:
            if img.name in self.mini_boots:
                for bin_file in img.files:
                    self.bin_mini_boots.append(bin_file)

        self.merge_loginfo(folder_name)

        _imgs = list()
        for _img_name in self.img_names:
            with open('/'.join([folder_name, _img_name]), mode='rb') as _file:
                _buff = _file.read()
                _img = ImgFile()
                _img.name = _img_name
                _img.deserialize(_buff)
                _imgs.append(_img)
        self.org_imgs = _imgs

    def save_to_file(self, file_name, password=''):
        file_path = os.path.dirname(file_name)
        if file_path == '':
            file_path = os.getcwd()
        if not os.path.exists(file_path):
            os.makedirs(file_path, exist_ok=True)

        with open(file_name, mode='wb+') as file:
            _buff = self.serialize(password)
            file.write(_buff)

    def save_to_folder(self, folder_name):
        if not os.path.exists(folder_name):
            os.makedirs(folder_name, exist_ok=True)

        with open('/'.join([folder_name, 'tempLoginfo.info']), mode='wb+') as _file:
            _file.write(self._log_info)

        with open('/'.join([folder_name, 'config.json']), mode='wb+') as _file:
            _file.write(self.cfg_file)
        self.load_config_json('/'.join([folder_name, 'config.json']))

        for img in self.org_imgs:
            if img.name in self.mini_boots:
                for bin_file in img.files:
                    self.bin_mini_boots.append(bin_file)

        self.split_loginfo('/'.join([folder_name, 'tempLoginfo.info']), folder_name)

        for img in self.org_imgs:
            with open('/'.join([folder_name, img.name]), mode='wb+') as _file:
                _buff = img.serialize()
                _file.write(_buff)

    def split_loginfo(self, loginfo_file_name, folder_name):
        if len(self.others) > 1:
            index = 0
            loginfo_file = open(loginfo_file_name, mode='rb+')
            _buff = loginfo_file.read()
            splitedLoginfo = list()
            for loginfo in self.others:
                log_header = LogHeader()
                is_ok, index = log_header.deserialize(_buff, index)
                log_header.logName = re.sub('([^\u4e00-\u9fa5\u0030-\u0039\u0041-\u007a\.])', '', log_header.logName)
                if log_header.logName == '' or log_header.logSize == 0:
                    break
                if log_header.logName not in self.others:
                    if not os.path.exists(folder_name + '/loginfo.info'):
                        shutil.copyfile(loginfo_file_name, folder_name + '/loginfo.info')
                    break
                log_file = open('/'.join([folder_name, log_header.logName]), mode='wb')
                log_content, index = token_bytearray(_buff, log_header.logSize, index)
                log_file.write(log_content)
                log_file.close()
                splitedLoginfo.append(log_header.logName)
            loginfo_file.close()
            for loginfo in self.others:
                if loginfo not in splitedLoginfo and loginfo != 'loginfo.info':
                    print(loginfo + ' not found!!!')
            os.remove(loginfo_file_name)

        else:
            shutil.copyfile('/'.join([folder_name, 'tempLoginfo.info']), '/'.join([folder_name, self.others[0]]))
            os.remove('/'.join([folder_name, 'tempLoginfo.info']))

    def merge_loginfo(self, folder_name):
        if len(self.others) > 1:
            buff = bytearray()
            for loginfo_name in self.others:
                log_header = LogHeader()
                log_header.logName = loginfo_name
                if not os.path.exists('/'.join([folder_name, loginfo_name])):
                    print(loginfo_name + ' not found!!!')
                    continue
                with open('/'.join([folder_name, loginfo_name]), mode='rb') as _file:
                    log_info = _file.read()
                    log_header.logSize = len(log_info)
                    buff += log_header.serialize()
                    buff += log_info
            self.log_info = buff

        else:
            with open('/'.join([folder_name, 'loginfo.info']), mode='rb') as _file:
                self.log_info = _file.read()

    @property
    def log_info(self):
        return self._log_info

    @log_info.setter
    def log_info(self, value):
        self._log_info = value
        self.header.log_size = len(self._log_info)

    @property
    def merged_img(self):
        return self._merged_img

    @merged_img.setter
    def merged_img(self, value):
        self._merged_img = value
        self.header.image_size = len(self._merged_img)

    @property
    def org_imgs(self):
        return self._org_imgs

    @org_imgs.setter
    def org_imgs(self, value):
        self._org_imgs = value
        _files = list()
        for img in self._org_imgs:
            _files += img.files

        _merged_img_files = list()
        for _file in _files:
            _file_temp = copy.deepcopy(_file)
            if _file_temp.header.image_type == FILE_FORMAT_HEX:
                _file_temp.content = str2hex(_file.content)
            _merged_img_files.append(_file_temp)
        self._merged_img.files = _merged_img_files

        _buff = self.merged_img.serialize()
        self.header.image_size = len(_buff)

    def serialize(self, password=''):
        if len(password) == 32:
            self.header.header = b'PASS'
        elif len(password) == 0:
            self.header.header = b'HEAD'
        else:
            raise Exception('password len is error')

        _buff = bytearray()

        _buff += self.header.serialize()

        _new_code = bytearray(password.encode())
        for i in range(len(_new_code)):
            _new_code[i] = (_new_code[i] - ord('0') - 10 - i) % 256

        _buff += _new_code

        _buff += self._log_info

        _buff += self.merged_img.serialize()

        _buff += int_to_bytes(len(self.cfg_file))
        _buff += self.cfg_file

        _buff += int_to_bytes(len(self.merged_img.files))

        for _binFile in self.merged_img.files:
            _buff += int_to_bytes(len(_binFile.parent))
            _buff += str_to_bytes(_binFile.parent)

        _buff += int_to_bytes(len(self.org_imgs))
        for _img in self.org_imgs:
            _buff += int_to_bytes(len(_img.name))
            _buff += str_to_bytes(_img.name)

            _buff1 = _img.serialize()
            _buff += int_to_bytes(len(_buff1))
            _buff += _buff1
        return _buff

    def deserialize(self, buff, pos=0, password=''):
        def check_password(code, decode):
            _new_code = bytearray(32)
            for i in range(len(code)):
                _new_code[i] = (code[i] + ord('0') + 10 + i) % 256
            m = hashlib.md5()
            m.update(_new_code)
            m.hexdigest()
            DebugPrint(m.hexdigest())
            if decode != m.hexdigest():
                raise Exception("password is error..")

        _pos = pos
        _is_ok, _pos = self.header.deserialize(buff, _pos)

        if self.header.header == b"PASS":
            _password, _pos = token_bytearray(buff, 32, _pos)
            check_password(_password, password)

        self.log_info, _pos = token_bytearray(buff, self.header.log_size, _pos)

        _is_ok, _pos = self.merged_img.deserialize(buff, _pos)

        _size, _pos = token_bytearray_int(buff, _pos)
        self.cfg_file, _pos = token_bytearray(buff, _size, _pos)

        _size, _pos = token_bytearray_int(buff, _pos)
        for i in range(_size):
            _size1, _pos = token_bytearray_int(buff, _pos)
            self.merged_img.files[i].parent, _pos = token_bytearray_str(buff, _size1, _pos)

        _size, _pos = token_bytearray_int(buff, _pos)
        self._org_imgs.clear()
        for i in range(_size):
            self._org_imgs.append(ImgFile())
            _size1, _pos = token_bytearray_int(buff, _pos)
            self._org_imgs[i].name, _pos = token_bytearray_str(buff, _size1, _pos)
            _size1, _pos = token_bytearray_int(buff, _pos)
            _is_ok, _pos = self.org_imgs[i].deserialize(buff, _pos)
        return True, _pos


if __name__ == '__main__':
    image = Image()

    # image.load_from_file('.//xinyiNBSoc.mimgx', 'fe473686987e1da8ac8dbc6a19e5ca33')
    image.load_from_file("C:\\Users\\yuhj\\Desktop\\temp\\雅迅\\雅迅硬件_307Y_双bin_测试.mimgx")
    # image.save_to_folder("C:\\Users\\yuhj\\Desktop\\diff\\old")

    # image.load_from_folder('../1/')

    # image.save_to_file('../TAG_V1.0.11_SDK_LTCC_XO1.mimgx', '800e5b0ae89a862f3a3132ff925e3f01')

    # image.save_to_folder('../2/')
    for file in image.merged_img.files:
        print(file.header.name, file.header.image_core, hex(file.header.image_load_addr), hex(file.header.file_len))
