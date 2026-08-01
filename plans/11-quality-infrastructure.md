# S11 质量基建(横切,S1 起步、贯穿所有阶段)

**目标**:建立支撑 S1–S10 验收的测试与质量基础设施:headless 渲染 harness、金图对比、状态快照、Sanitizer、CI 矩阵、生成式 API manifest、性能基准。本文件是各阶段"退出标准"里测试项的实现载体。

**现状(2026-07-26)**:CTest 套件 625 项全绿(7 个 smoke + 618 个 vendored piglit shader_test),GCC/Clang + ASan/UBSan CI 已建,api-manifest 生成式校验已接入。piglit 子集经自研 mini-runner(`tests/piglit_runner.c`,250x250 pbuffer,全部经 eglGetProcAddress)驱动,覆盖 glsl-1.10/1.20 的 execution+linker 测试;6 个 glslang/Mesa 语义差异记录于 `tests/piglit/KNOWN_DEVIATIONS.txt` 并以 WILL_FAIL 跟踪。金图系统、状态快照器、TSan 未落地;Q5 微基准已落地(场景基准未做)。

## 组件

### Q1 构建与静态质量(随 S1 落地)
- [x] CI(GitHub Actions):Linux GCC/Clang 矩阵 + ASan/UBSan 作业(`.github/workflows/ci.yml`);Android NDK arm64 交叉构建待补;
- [ ] 警告即错误(项目代码,3rdparty 豁免)——S1 清零后开启;
- [ ] clang-tidy 基线集(bugprone-*, cert-* 精选),增量执行。

### Q2 Headless 渲染 harness(S2 需要,尽早)
- [x] EGL surfaceless/pbuffer + llvmpipe(CI)/真实 GPU(本地)双模;GLES 3.0 context;SKIP_RETURN_CODE 77 无设备降级;
- [x] wrapper 以"被测库"形态加载:smoke_render / piglit_runner 均 dlopen + 全量 `eglGetProcAddress` 解析;
- [ ] 状态快照器:关键后端状态(program/VAO/buffer/texture 绑定、enable 集)+ wrapper 影子状态的结构化 dump 与 diff;
- [ ] 金图系统:PNG 输出、逐像素容差比较(默认容差 0,光照/雾类场景按通道 ±2)、失败时输出 diff 图 + 当帧 shader 源码与编译日志(利用 S1 日志层);金图按"驱动 profile"分目录(llvmpipe 与移动 GPU 光栅差异)。

### Q3 Sanitizer 与动态检查(随 S2 落地)
- [x] ASan+UBSan 测试作业(CI `linux-gcc-asan`,detect_leaks=0);
- [ ] S2 修复的每个缺陷都要有 Sanitizer 可见的复现测试(修复前红);
- [ ] TSan 作业(S7 起,mock 后端跑并发用例)。

### Q4 生成式 API Manifest(随 S3 落地,S10 定版)
- [x] 脚本解析 GETPROC 表 + 导出符号 → `docs/api-manifest.{md,json}`(`tools/gen_api_manifest.py`,CTest `api_manifest_current` 强制同步);
- [ ] "tested" 状态由测试标注反哺(测试用例声明覆盖的入口点,脚本聚合);
- [ ] CI 校验:与 gl.xml 2.1 profile 对比,新增/丢失符号必须显式改 manifest 才能过;
- [ ] 取代 `fpe_implementation_progress.yaml`(归档)。

### Q5 性能基准(随 S2/S6 落地)
- [x] 微基准:`tests/bench_fpe.c`(`-DSFPEW_BENCH=ON` 注册为 ctest,`ctest -L bench -V`;`SFPEW_BENCH_SCALE` 缩放迭代)——即时模式顶点吞吐、显示列表回放、program 缓存稳态/切换、GLSL 翻译耗时。
  首轮基线(GTX 1660 SUPER,NVIDIA GLES3,2026-07-26,14 项):
  - immediate 0.62 Mvert/s(1607 ns/vert)vs clientarrays 29.6 Mvert/s —— **即时模式路径比 client-array 路径慢 ~48x,纯 wrapper CPU 开销,GUI/粒子场景优化点**;
  - dlist 回放 speedup **1.00x——合批收益为零,回放仍逐命令重放,S6 优化点**;
  - tinybatch 19.2 μs/batch ≈ progcache 稳态 19.4 μs/draw ≈ texswitch 21.9 μs/draw —— **每 draw 固定开销 ~19 μs(commit_fpe_state_on_draw),小批次场景的主瓶颈**;
  - gatherarrays 43.4 Mvert/s **快于** interleaved 29.6 Mvert/s —— 反直觉,交错路径疑有多余拷贝,待查;
  - drawelements 61.9 Midx/s;matrixops 1818 ns/组(push+translate+rotate+pop,~450 ns/调用,偏高);getter 456 ns/次(TLS/锁开销,MC mod 会高频调用);
  - texupload 1.96 μs/16x16 sub(lightmap 模式)、2.7 GB/s 整图;readpixels 44.7 μs/256x256;
  - translate 1.10 ms/shader、progcompile 3.85 ms/program 首触 —— **印证 plans/09 源码 hash 缓存与 program 预热的必要性(移动端首帧卡顿源)**;
- [x] **性能冲刺后基线(同机同环境,2026-07-26,perf 分支 99b617a..dc452e3 六个提交)**——根因是 glvnd 桌面上 `eglGetCurrentContext` 每次 ~425ns(getpid fork 检查 + dispatch 互斥),而旧设计每次 `g_glstate` 宏展开都调用它(profile 中占全部周期 ~83%)。改造为"每导出入口恰一次严格解析 + TLS 快照下游复用 + Begin/End 顶点数据钉扎"(docs/context-model.md),并叠加 draw 路径/显示列表/翻译器优化:
  - immediate **24.2 Mvert/s(41 ns/vert,39x)**;dlist 回放 **188 Mvert/s(5.3 ns/vert,speedup 7.8x)**——glEndList 把 Begin/End 流编译成 baked draw(一次 ring 上传 + 一次 draw,相邻同布局 run 合并);
  - tinybatch **4.4 μs**、progcache 稳态 **4.4 μs**、texswitch **5.5 μs**(约 4x)——入口单解析 + 后端 VAO/element 绑定影子(消除 guard 每 draw 两次同步 glGetIntegerv)+ client 数组经持久映射 ring 上传(不再每 draw glBufferData 孤儿化);
  - clientarrays **143 Mvert/s(7.0 ns/vert,5.4x)**——已交错布局直通(跳过逐元素 gather 重打包);gatherarrays 60.3 Mvert/s;drawelements 115 Midx/s;
  - translate 重复源码 **~0 ms**(源码哈希 memoization,16MB 上限;首触不变 ~1.1ms);progcompile 首触 0.19 ms;
  - matrixops 450 ns/调用、getter 433 ns 为严格合同下限(= 1 次 eglGetCurrentContext);**`SFPEW_RELAXED_CONTEXT=1`**(每线程单上下文承诺,MC 启动器均满足)下 matrixops **25 ns/调用**、getter **7.9 ns**、tinybatch 3.1 μs——Android 真机上 eglGetCurrentContext 本身即为 TLS 读,严格模式即接近该水平;
  - advance() 改为缓存 span 拷贝计划(92 字节 memcmp 验证);终态 perf:除合规入口解析外,wrapper 无 >5% 单点热点(memmove 4%、advance 2%、program_hash 0.8%);
  - 全程 ctest 665/665 全绿(严格 + relaxed 两种模式),对照两轮对抗性审查(入口锚点完备性、钉扎语义、绑定影子一致性、编译回放语义)。
- [ ] 场景基准:MC 风格 chunk 重放帧时间(录制一段真实调用流回放——考虑用 apitrace 采一份 1.7.10 trace 作固定负载);
- [ ] 回归门槛:关键基准 ±5% 报警(S6 合批改造、S1 `-ffast-math` 移除都靠它裁决)。

### Q6 目标驱动矩阵(S5 起)
- [ ] Mesa llvmpipe(CI 每次)+ 本地至少一款移动 GLES 驱动(Adreno/Mali,每阶段收尾手动过);
- [ ] 真实应用冒烟清单:MC 1.7.10、1.12.2(现有目标)、加一个 GL2.0 shader 使用者(S9 起);结果记录进阶段验收。

## 各阶段接入时间表

| 阶段 | 必须就绪的组件 |
|---|---|
| S1 | Q1;Q2 骨架(EGL bootstrap + dlopen 测试) |
| S2 | Q2 完整(快照+金图)、Q3 ASan/UBSan、Q5 微基准 |
| S3 | Q4 manifest v1 |
| S4–S6 | 金图库扩容;Q5 场景基准;Q4 tested 反哺 |
| S7 | Q3 TSan;多 context 用例 |
| S8–S10 | Q6 双驱动全量;Q4 定版校验(gl.xml 全表) |

## 原则

- 测试是退出标准的一部分,**不允许"实现完了测试以后补"**——每阶段文件里的测试项与功能项同权;
- 金图失败必须能一眼定位:diff 图 + shader 源 + 状态快照三件套自动附带;
- 所有质量作业本地可跑(一条 `ctest` / 一个脚本),CI 只是自动化,不是唯一入口。
