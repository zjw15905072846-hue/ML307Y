import json
import sys
import os
import shutil

# 获取当前脚本所在目录的父目录（即项目根目录）
# 添加项目路径到 sys.path
# 用于下面包含子模块
script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

from image.Image import Image, BinFile

help_doc = '''
<-dl> : 
    功能描述: 往UE下载程序 
    参数: 
        <PortNo.>:    串口号（例如com4)。  
        <DownFile>:   下载文件全目录。 
        [EraseAll]:   0 升级下载，其它为全擦。 
        [Password]:   文件密码（可选）。 
        成功打印OK,失败打印失败结果


<-m> :  
    功能描述: 合并下载文件 
    参数: 
        <Folder>:     要合并的目录 
        <File>:       合并目标文件,必须加上正确的扩展名。 
        [EncodeCode]: 合并加密码。 
        成功打印OK,失败打印失败结果


<-s> :  
    功能描述: 拆分文件 
    参数: 
        <File>:       要拆分的文件 
        <Folder>:     拆分后保存文件的目标文件夹。 
        [decodeCode]: 拆分解密码 
        成功打印OK,失败打印失败结果

'''
def do_pack(argv):
    """
    :param argv:
        argv[0]: folder
        argv[1]: file
        argv[2]: encode
    :return:
    """
    # 压缩文件，参与打包
    # ap_elf目录
    folder_path = os.path.join(argv[0], 'ap_elf')
    if os.path.exists(folder_path):
        shutil.make_archive(folder_path, 'zip', folder_path)
    # app_elf目录
    folder_path = os.path.join(argv[0], 'app_elf')
    if os.path.exists(folder_path):
        shutil.make_archive(folder_path, 'zip', folder_path)

    try:
        image = Image()
        ret = image.load_from_folder(argv[0])
        if ret == False:
            print("pack abort!!!")
            return

        if len(argv) == 3:
            image.save_to_file(argv[1], argv[2])
        else:
            image.save_to_file(argv[1])
    except Exception as e:
        print(e)
        print('pack abort')

    # print("pack success!!!")


if __name__ == '__main__':
    if sys.argv[1] == '-m':
        do_pack(sys.argv[2:])
    else:
        raise ValueError('parameter not supported...')
