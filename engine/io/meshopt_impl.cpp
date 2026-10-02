// meshoptimizer 实现展开单元（M5，D-035）：header-only 库需在某一个翻译单元
// 定义 MESHOPTIMIZER_IMPLEMENTATION 才能得到 meshopt_simplify 等函数实体。
// 本文件被 engine/ 与 app-android 的 GLOB 编译收集，链接期解决 undefined reference。
#define MESHOPTIMIZER_IMPLEMENTATION
#include "meshoptimizer.h"