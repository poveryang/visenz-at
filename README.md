<!--
 * @Author: Lu ShaoAn, Smartmore Corporation
 * @Brief: 
 * @Version: 0.1
 * @Date: 2024-04-10 14:46:32
 * @Copyright: Copyright (c) 2022
-->
# ViSenz_AT4VS

编译及运行步骤
1. copy想要运行的机型的barcode sdk文件夹到 3rdparty/libBarcode中，文件夹名与CMakeLists.txt对应，如： 
   ./3rdparty/libBarcode/vs800p/ 
   ./3rdparty/libBarcode/vs1000p/  
2. 配置CMakeLists.txt中，根据机型选择正确的编译器路径（CMAKE_C_COMPILER， CMAKE_CXX_COMPILER）和opencv路径  
3. 正常使用cmake生成可执行文件和库文件  
   ```
   mkdir build 
   cd build  
   cmake .. -DDEVICE=vs1000p  # DEVICE必填，要根据机型选择对应的编译器和opencv路径和libbarcode文件，其他option看情况选择
   make
   ```
4. 把libBarcode中的模型文件、lib文件、config文件，和libAT.so、TEST_AT 拷入扫码器中，并设置好相应路径  
5. 设置好扫码器的库搜索路径  
   `
   export LD_LIBRATY_PATH = /usr/scanner/debug/at/full_at/libs/
   `
6. 运行TEST_AT，给好相应的命令行参数
   ./TEST_AT vs1000p 2d
   