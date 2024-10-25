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


发布及更新
**记得在cmake中更新版本**

## Release Notes  

### 5.1.1
1. 修改计算清晰度的算法，当没有roi时（没找到码或上位机没给roi）使用中值滤波和腐蚀，以减小噪声背景点对sharpness计算的影响
2. 修改了计算亮度的算法，以解决黑背景下有高反光物体的场景（主要是锂电）
3. refine_code_brt = {96, 64, 32} -> {64, 96, 32};
4. 删除了af_impl中，SlidingWinSearch()对于鞍点的加速处理，该处理有可能会导致在有高度差的场景loop不到后面的焦段
5. af时，若没有传入roi的情况下，先使用高斯模糊，以减少噪声
6. 修改了调整灯光的逻辑，在无法达到给定亮度时不会直接改变灯光，而是会跑完剩余亮度后再调整灯光，该做法虽然更慢，但是找码能力更强
7. 修改亮灯顺序 lights_sets = {polarized, unpolarized, all} -> {unpolarized, polarized, all} 先开偏振

### 5.1.0
1. 遍历所有灯光仍无法定位到码区时，使用全开灯及AEQT时的曝光作为最终返回值
2. 修改逻辑，当关闭af只打开ae时，也会走refine流程
3. 增加target_brt=16、8，以应对极端场景，为了不明显增加整体耗时，删除关灯的轮询，完全覆盖note 1中的修改
4. 自适应ae_obj.QuickTune() 的threshold = std::min(15, int(target_brt/2))
5. 增加了ae interface的接口，可从外部清除ae impl中ae_fail的状态，当aeqt失败时，还能在后续的aest或refine中重新调整亮度，
   毕竟当找到码区后，不需要考虑全图的情况下，或许就能达到想要的亮度
   该功能在关闭灯光搜索且ae max受限的情况下很有使用价值
6. refine增加遍历亮度查找解码稳定性最高的那一个作为返回
