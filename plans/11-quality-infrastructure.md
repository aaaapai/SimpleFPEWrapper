# S11 质量基建(横切,S1 起步、贯穿所有阶段)

**目标**:建立支撑 S1–S10 验收的测试与质量基础设施:headless 渲染 harness、金图对比、状态快照、Sanitizer、CI 矩阵、生成式 API manifest、性能基准。本文件是各阶段"退出标准"里测试项的实现载体。

**现状(2026-07-26)**:CTest 套件 625 项全绿(7 个 smoke + 618 个 vendored piglit shader_test),GCC/Clang + ASan/UBSan CI 已建,api-manifest 生成式校验已接入。piglit 子集经自研 mini-runner(`tests/piglit_runner.c`,250x250 pbuffer,全部经 eglGetProcAddress)驱动,覆盖 glsl-1.10/1.20 的 execution+linker 测试;6 个 glslang/Mesa 语义差异记录于 `tests/piglit/KNOWN_DEVIATIONS.txt` 并以 WILL_FAIL 跟踪。金图系统、状态快照器、TSan、微基准未落地。

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
- [ ] 微基准:即时模式顶点吞吐、显示列表回放(合批收益)、program 缓存命中路径;
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
