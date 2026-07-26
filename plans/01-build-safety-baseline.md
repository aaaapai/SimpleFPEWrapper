# S1 构建与安全基线

**目标**:GCC 与 Clang 的 Debug/Release 四种组合全部构建通过;去掉全局警告抑制;库在无 GL context 时加载不崩溃;有第一个 CTest 测试;为 per-context 改造(S7)埋好聚合层。

**前置**:无(第一阶段)。
**规模**:小(约 1–2 周单人)。

## 现状(审计证据)

- GCC 16 无法编译:`fpe/state.cpp` 有 **11 处空 `default:` 标签**(`default:` 后只有注释直接闭括号,C++20 硬错误;Clang 仅按 C++23 扩展警告、又被 `-w` 压掉)。标签行 `grep -n 'default:'` 位于 195/224/254/…,GCC 报错行偏移约 +2。
- `CMakeLists.txt:22,24` 使用 Clang 专有 `-flto=thin`,GCC 直接报错;`:12,13,22,23` 四条编译 flags 全带 `-w`。
- `CMakeLists.txt:11` `if(MACOS)` 不是标准变量(应为 `APPLE`),该分支恒为死代码;`:26` `CMAKE_ANDROID_STL_TYPE` 在 AGP+NDK 工具链下无效(AGP 走 `ANDROID_STL`)。
- `-ffast-math`(`CMakeLists.txt:22`)作用于全部矩阵/光照数学与 glm,存在 NaN/精度/非确定性渲染风险,引入时(commit `2d1e510`)无任何回归验证。
- 全局 `-DNDEBUG` 使所有边界 `assert` 失效:`drawing1x.cpp:339,349` 的 `glMultiTexCoord*` target 范围断言、`drawing1x.h:92` 的 `glBegin` 配对断言——见 S2 修复项。
- 静态初始化:`init.cpp:36-40` `static InitClass` 在 dlopen 期运行,找不到 `libEGL` 时在静态构造中 `throw`(`init.cpp:24-31`)→ 未捕获 → `std::terminate`;Windows/macOS 分支恒失败。
- `getter.cpp:321-323` `glGetIntegerv(pname, nullptr)` 会 `throw std::invalid_argument`——`extern "C"` 入口向 C 调用方抛 C++ 异常是 UB(批判员实测确认,两份子审计均漏掉)。
- `getter.cpp:270-282` 后端 `glGetString` 返回 NULL 时 `std::string((char*)nullptr)` UB,且结果被 static 缓存永久保留;`getter.cpp:332-339` 无 context 时 `cachedNumExtensions` 被污染为 7 且永不重查。
- `loader.cpp:507-508` 死代码必崩 bug:`glGetIntegerv(GL_MAJOR_VERSION, (GLint*)caps.Version[0])` 把整型值当指针(向地址 0x3 写入);`FillInBackendGLCapabilities` 目前无调用点。
- `state.cpp:348→351` 显示列表录制路径先按 `pname_to_count` 深拷贝 `params` 再判空,COMPILE 模式传 nullptr 即崩。
- `include/android_debug.h:12` logcat 重定向被注释,移动端看不到任何诊断输出。
- 仓库无 `settings.gradle`/`gradlew`,`build.gradle`(commit `4c679de`)无法独立构建;无 abiFilters、无 GLES 版本声明。

## 任务

### 1.1 修复双编译器构建
- [ ] 修复 `fpe/state.cpp` 全部 11 处空 `default:`(补 `break;` 或删除标签);用 `g++ -std=c++20 -fsyntax-only` 全文件验证零 error。
- [ ] `-flto=thin` 按编译器分支:Clang 用 `-flto=thin`,GCC 用 `-flto`(或 Release 下统一 `INTERPROCEDURAL_OPTIMIZATION`)。
- [ ] 移除全部四处 `-w`;项目代码开 `-Wall -Wextra`;当前警告要么修复、要么逐条 `#pragma`/局部 flag 显式豁免(3rdparty 除外)。
- [ ] `if(MACOS)` → `if(APPLE)`;`CMAKE_ANDROID_STL_TYPE` → 按工具链正确设置(AGP 下由 gradle 传 `ANDROID_STL`)。
- [ ] 评审 `-ffast-math`:建议移除或降为 `-fno-math-errno -ffp-contract=fast`;若保留,必须在 S11 金图测试建立后用图像对比证明无回归。**决策点:默认移除。**
- [ ] 补 `settings.gradle` + gradle wrapper,或在 README 写明作为 AGP 子模块集成的方式;声明 `abiFilters`(arm64-v8a 起步)。

### 1.2 消灭加载期/无 context 崩溃
- [ ] 移除 `static InitClass`:改为首个被拦截调用时的惰性初始化,且仅在 `eglGetCurrentContext() != EGL_NO_CONTEXT` 后触碰 GL;dlopen 阶段零 GL 调用、零异常逃逸。
- [ ] `Init()` 失败路径:返回错误码 + 日志,后续调用降级为 no-op(而非 terminate);所有 `extern "C"` 入口保证 `noexcept` 语义(异常在边界内捕获)。
- [ ] `glGetIntegerv(pname, nullptr)`:删掉 `throw`,改为按 GL 语义静默返回(生成 GL_INVALID_VALUE 的决策归 S2 错误状态机)。
- [ ] `glGetString`/`glGetStringi`:后端返回 NULL 时返回缓存的安全字符串或 NULL,不构造 `std::string(nullptr)`;失败结果不得进入 static 缓存。
- [ ] `cachedNumExtensions`:仅在成功读取后缓存;无 context 时不写缓存。
- [ ] 删除或修复 `FillInBackendGLCapabilities`(死代码 + 必崩写法);若保留,改为 `glGetIntegerv(GL_MAJOR_VERSION, &caps.Version[0])` 并接入判空。
- [ ] `state.cpp` 显示列表录制:所有 `LIST_RECORD` 之前先做参数判空/长度校验(以 `glLightfv` 为模板全量排查)。
- [ ] 后端函数指针使用前判空:以 `getter.cpp`、`fpe.cpp:210,220,224` 为起点全量排查(`ordered_passthrough.cpp`/`drawing.cpp` 已做,拉平)。

### 1.3 断言与日志
- [ ] 制定规则:**边界/安全检查一律用运行时检查**(返回 + GL error),`assert` 只用于内部不变量;把 `drawing1x.cpp:339,349`、`drawing1x.h:92` 等改造为运行时检查(具体行为语义在 S2 定义)。
- [ ] 启用 `android_debug.h` 的 logcat 输出;封装统一日志宏(桌面 stderr / Android logcat),shader 编译失败日志改走该层(替换 printf/stdout)。

### 1.4 最小测试与聚合层
- [ ] CTest 起步:`enable_testing()` + 两个测试——(a) 干净 configure+build 冒烟;(b) `dlopen` 本库且不创建 context,进程存活、无 GL 调用发出(用 mock loader 或 LD_PRELOAD 断言)。
- [ ] 引入 `fpe_context_t` 聚合结构 + 单一访问函数(现阶段仍是进程单例,仅做**收口**):新增状态一律挂在聚合体上,为 S7 的 per-context 化铺路。`g_glstate`(`fpe.cpp:15-18`)、`fpe_inited`(`fpe.cpp:133`)先收进来。

## 退出标准

1. GCC 16 与 Clang 22 × Debug/Release 四组合零 error 构建(CI 脚本化,见 S11);
2. 项目代码零 `-w`;警告清零或显式豁免;
3. `ctest` 至少 2 个测试通过;无 context dlopen 不 terminate、不发 GL 调用;
4. 上述崩溃类修复各有一个最小复现单测(空指针 getter、nullptr 光参数录制等);
5. `fpe_context_t` 聚合层合入,新代码评审规则生效。

## 风险与决策点

- `-ffast-math` 移除可能带来可测的性能回退 → 用 S11 的帧时间基准衡量,必要时按文件粒度局部启用。
- 惰性初始化改造会触碰所有入口的 init 检查路径 → 用宏/内联函数统一,避免 60+ 入口手写。
