class ShaInfo:
    def __init__(self):
        self.sha_address = 0
        self.sha_len = 0
        self.isWrite = False
        self.sha_value = bytearray()


class NvInfo:
    def __init__(self):
        self.name = 0
        self.is_write = False


class FotaBackupInfo:
    def __init__(self):
        self.fota_backup_base = 0
        self.fota_backup_len = 0

class CfgFileInfo:
    def __init__(self):
        self.image_core = ''
        self.image_load_addr = 0
        self.image_exec_addr = 0
        self.image_name = ''
        self.image_type = ''
        self.force_exist = True


class JsonCfg:
    def __init__(self):
        self.cfg = CfgFileInfo()

    def load(self, file_name):
        pass

    def save(self, file_name):
        pass
