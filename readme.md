###scons 安装
scons 安装：
(1) 需预安装python3.7以上版本，安装完python后，打开命令行执行python -m pip install scons或执行python -m pip3 install scons安装scons工具;
(2) 将scons.exe所在的路径加入到系统环境变量中，重启电脑生效；
elftools安装：python -m pip install pyelftools

# 编译须知
## 1. 编译帮助
- **帮助信息**：
  ```
  scons -h
  ```
  或
  ```
  scons --help
  ```

## 2. **open客户二次开发工程编译（双bin，open_mode）**：在根目录下执行

   ```
   scons target=kernel     # 编译底包image。清理：scons target=kernel -c
   scons target=userapp    # 编译custom用户二次开发代码。清理：scons target=userapp -c
   scons test=y            # 编译test测试用例。清理：scons test=y -c
   scons demo=wifiscan     # 编译examples下某个模块的DEMO
   ```

## 3. **单bin固件编译（single_mode）**：在根目录下执行

   ```
   scons target=kernel oc_entry=kernel   # 打包生成单bin固件ap.mimgx。清理：scons target=kernel -c
   ```

## 4. **编译生成的文件**：

   - 双bin可烧录文件：`out\userapp\image\ML307Y_APP.mimgx`
   - 单bin可烧录文件：`out\kernel\image\ap.mimgx`
   - 烧录文件对应的allbins目录：`out\userapp\image\pkg\`