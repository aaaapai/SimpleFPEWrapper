# S7 Context 与线程模型

**目标**:FPE 状态 per-context 化,定义创建/切换/销毁语义;统一 thread_local 与进程全局的混乱共存;明确并测试目标场景(MC 多线程 chunk 构建、多 context)的行为契约。

**前置**:S1(`fpe_context_t` 聚合层)、S6(server-state 影子镜像已挂在聚合体上)。建议在 S8/S9 大量新增状态之前完成。
**规模**:中大(3–4 周,架构手术)。

## 现状(审计证据)

- `g_glstate` 进程级单例(`fpe.cpp:15-18`)、`fpe_inited` 全局 bool 无失效路径(`fpe.cpp:133`)、`DisplayListManager` inline static(`list.h:68-74`)、`clientArrayBufferBindings` 进程级全局数组(`vertexpointer.cpp:17`)。
- 与之并存的 thread_local:`logicalArrayBufferState`、program 影子(`glUseProgram` 包装维护)、字形批处理器、显示列表两级缓存、`getter.cpp`/`ordered_passthrough.cpp` 的三个按 `eglGetCurrentContext` 失效的缓存。
- 后果(批判员点名无人全景回答):多 context 下进程全局与 thread_local 状态互相污染;绕过包装层直接改 program 会让拦截判定失效;无 teardown,context 销毁后 FPE 拥有的 GL 对象(环形 VBO、vertex arena、program 缓存、QUADS IBO)泄漏或悬挂。
- GL 对象归属现实:program/VBO/display list 属于 context(或 share group),FPE 把它们放进程全局,跨 context 使用 GL 对象名是未定义行为。

## 设计决策(先定契约再动代码)

1. **状态归属**:每个 EGL context 一份 `fpe_context_t`(全部 FPE 影子状态 + FPE 拥有的 GL 对象);share group 级共享的只有显示列表**定义**与纹理逻辑格式表(与 GL 对象共享域一致)。
2. **current 语义**:`eglMakeCurrent` 拦截切换"当前 fpe context"指针(thread_local);未拦截到的场景(应用直连 libEGL)用 `eglGetCurrentContext()` 惰性对账兜底。
3. **线程契约**:与 GL 相同——一个 context 同时只能 current 于一个线程;FPE 不加锁保护单 context 并发误用(与真实驱动一致),但 share group 共享表(显示列表定义)加锁。
4. **teardown**:拦截 `eglDestroyContext`/`eglTerminate` 释放对应 `fpe_context_t` 与 GL 对象;进程退出不依赖静态析构做 GL 调用。

## 任务

- [ ] EGL 拦截层:wrapper 需导出/挂钩 `eglMakeCurrent`、`eglDestroyContext`、`eglCreateContext`(记录 share_context 关系)、`eglTerminate`;确认当前部署方式(被替换的 libGL?LD_PRELOAD?Pojav 类启动器的 loader?)下这些符号确实经过 wrapper——**先做一次部署形态调研,结论写进本文件**。
- [ ] `fpe_context_t` 装载全部状态:S1 起新状态已收口,本阶段搬迁存量——`g_glstate`、矩阵栈、`clientArrayBufferBindings`(修复它与 thread_local `logicalArrayBufferState` 的口径不一致,审计 5 点名)、即时模式缓冲与环形 VBO、字形批处理器、QUADS IBO、program 缓存、vertex arena。
- [ ] 显示列表:定义表挂 share group(引用计数),per-context 的回放缓存保持 thread_local 但 key 加 context id(现有 `eglGetCurrentContext` 失效模式推广)。
- [ ] `fpe_inited` → per-context 惰性 init + 幂等;context 销毁时 GL 对象在**其仍 current 时**释放,否则标记延迟释放。
- [ ] program 影子失效路径:`glUseProgram` 绕过检测——在 draw 拦截判定处增加低频对账(每 N 次 draw 或 context 切换后重读真实 `GL_CURRENT_PROGRAM`),消除"绕过包装层即永久失效"的隐患(频率可调,默认保守)。
- [ ] 多线程/多 context 测试(S11 harness 扩展):双 context 交替渲染互不污染;线程 A/B 各持 context 并发渲染;context 销毁后再创建,无对象泄漏(用 `glGetError`+对象名审计);MC 场景模拟——主线程渲染 + 工作线程 share context 上传 VBO/纹理。
- [ ] 文档:`docs/context-model.md` 写清创建/切换/销毁/线程契约与已知限制。

## 退出标准

1. 双 context 污染测试、并发测试、销毁重建测试在 ASan/TSan 下通过;
2. 全仓 grep 无裸进程级 GL 状态(白名单:纯常量表);
3. 单 context 性能基准无回退(current 指针查找必须是 thread_local 一跳);
4. 部署形态调研结论 + context 模型文档合入。

## 风险与决策点

- **EGL 符号是否可靠经过 wrapper 是本阶段最大未知数** → 调研任务放最前;若某些部署形态拿不到 eglMakeCurrent,则全面退回 `eglGetCurrentContext()` 惰性对账方案(每个入口首查,成本高一点但正确)。
- thread_local 缓存 key 加 context id 后命中率下降 → 基准衡量;必要时缓存直接挂 `fpe_context_t`。
- TSan 对 GL 驱动误报多 → 并发测试用 mock 后端跑 TSan,真后端只跑功能。
