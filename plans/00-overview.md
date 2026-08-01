# SimpleFPEWrapper 总体计划:一步步推进到标准 OpenGL 2.1

> 基于 2026-07-26 的多智能体全量代码审计(9 个审计角色、逐文件核对,基准提交 `ed2e70b`)。
> 文中 `file:line` 行号以 `ed2e70b` 为准,后续提交会漂移,引用时请以符号名为主。

## 最终目标

在 OpenGL ES 3.x 后端上提供**完整的标准 OpenGL 2.1**:

1. 符号面完整:GL 2.1 全部入口点可通过 `eglGetProcAddress` 与导出符号解析,不返回空指针;
2. 行为正确:已实现的调用符合 GL 2.1 规范语义(含固定管线 + GLSL 1.10/1.20 可编程管线);
3. 错误契约:不支持/非法调用通过 GL 错误状态机(`glGetError`)可预测地失败,绝不静默吞掉、透传到不兼容后端或崩溃;
4. 可验证:每阶段有构建、单测、headless 渲染回归三层验收;
5. 允许**有文档的偏差**(如 stipple 近似、LogicOp 受限、颜色索引模式不支持),但必须显式发布,不允许未声明的静默缺失。

与旧 ROADMAP.md 的关键差异:evaluators、feedback/selection、累积缓冲、像素路径、texgen 等原先"无限期推迟"的特性**全部纳入计划**(它们在 GL 2.1 标准内),只是排在后期阶段。

## 审计结论速览(当前进度)

| 旧 ROADMAP 阶段 | 判定 | 一句话现状 |
|---|---|---|
| Phase 0 构建与安全基线 | 进行中 | GCC 构建失败(state.cpp 11 处空 `default:` + Clang 专有 `-flto=thin`);全局 `-w` 仍在;零测试;静态初始化失败会 `std::terminate` |
| Phase 1 绘制拦截与状态隔离 | 进行中(约一半) | `glDrawArrays` 拦截/透传判定与状态 guard 已达标;**`glDrawElements` 纯透传无 FPE 转换**;shader 失败不设 GL error |
| Phase 2 最小固定管线子集 | 进行中 | 矩阵(含 `glLoadMatrix*`/`glFrustum`)与 Vertex/Normal/Color 全变体已完成并导出;TexCoord/MultiTexCoord 56 个变体、错误/查询行为、API manifest 全缺 |
| Phase 3 纹理与片元操作 | 基本完成 | texenv 五种模式、16 纹理单元、fog、alpha test 均已实现;COMBINE 降级为 MODULATE;无图像级验证 |
| Phase 4 光照与材质 | 进行中 | 顶点级 ambient+diffuse+emission、双面光照、color material 已可渲染;specular/衰减/聚光/GL_FLAT 零生成,位置光被当方向光 |
| Phase 5 显示列表 | 进行中 | 核心 API + 五层合批远超账面;server state 不进列表、自引用列表栈溢出、`glRect*` 未实现 |
| Quality Gates | 未开始 | 无 CTest/CI/ASan/headless 渲染器/生成式 manifest |

注意:`fpe_implementation_progress.yaml`(2026-04-30)已严重过期——其后 14 个提交完成了它标为"未实现"的大量特性(光照 shader、多纹理、glTexEnv、glMaterial、即时模式全变体、lookup 重写等)。阶段 3 中将用生成式 API manifest 取代它。

## 阶段划分与依赖

```
S1 构建与安全基线 ──► S2 正确性与错误契约 ──► S3 GL1.x 核心补全 ──► S4 光照完成
        │                                          │                    │
        └────────► S11 质量基建(横切,S1 起步,贯穿所有阶段) ◄────────────┘
                                                   │
                    S5 纹理完善 ◄──────────────────┤
                    S6 显示列表与属性栈 ◄──────────┤
                    S7 Context 与线程模型 ◄────────┘(建议在 S5/S6 之后、S8 之前完成)
                                                   │
                    S8 光栅与像素路径 ◄────────────┘
                    S9 GL 2.0 可编程管线 ◄── S8
                    S10 GL 2.1 收尾 ◄── S9
```

| 阶段 | 文件 | 目标一句话 | 规模 |
|---|---|---|---|
| S1 | [01-build-safety-baseline.md](01-build-safety-baseline.md) | GCC+Clang 双构建通过、去 `-w`、消灭加载期崩溃、最小 CTest | 小 |
| S2 | [02-correctness-bugfixes.md](02-correctness-bugfixes.md) | 修复审计发现的全部崩溃/正确性缺陷,建立 GL 错误状态机,补 `glDrawElements` 转换 | 中 |
| S3 | [03-gl1x-core-completion.md](03-gl1x-core-completion.md) | 补全 GL 1.x 核心 API 面:TexCoord 变体、glRect、getters、容量查询、缺失符号 | 中 |
| S4 | [04-lighting.md](04-lighting.md) | 光照完成:位置光/specular/衰减/聚光/GL_FLAT/查询 | 中 |
| S5 | [05-texturing.md](05-texturing.md) | 纹理完善:COMBINE、texgen、legacy 内部格式、BGRA、像素存储 | 中大 |
| S6 | [06-display-lists-attrib-stack.md](06-display-lists-attrib-stack.md) | 显示列表完备(server state 录制)+ glPushAttrib 属性栈 | 中 |
| S7 | [07-context-threading-model.md](07-context-threading-model.md) | per-context 状态、teardown、多线程契约 | 中大 |
| S8 | [08-raster-pixel-path.md](08-raster-pixel-path.md) | RasterPos/Bitmap/DrawPixels/stipple/PolygonMode/ClipPlane/LogicOp | 大 |
| S9 | [09-gl20-programmable.md](09-gl20-programmable.md) | GLSL 1.10/1.20→ESSL 3.00 翻译、shader objects、固定管线互操作、点精灵 | 大 |
| S10 | [10-gl21-completion.md](10-gl21-completion.md) | PBO/sRGB、evaluators、selection/feedback、accum、最终符号面清点 | 大 |
| S11 | [11-quality-infrastructure.md](11-quality-infrastructure.md) | 测试与质量基建(横切):headless EGL、金图、ASan、CI、生成式 manifest | 持续 |

### 排序理由

- **S1/S2 先行**:所有后续工作都需要可靠的双编译器构建、错误状态机和回归测试兜底;审计发现的崩溃类缺陷(空指针、递归、下溢)不修,后面每个阶段都在流沙上盖楼。
- **S3–S6 完成 GL 1.x**:按"应用最常踩到的缺口"排序——LWJGL 类加载器会批量解析符号,缺 `glGetError`/`glPushAttrib`/容量查询的杀伤力大于缺 evaluators。
- **S7 在大规模新状态引入之前、像素路径之后**:per-context 改造是架构手术,越晚做迁移面越大;S1 会先引入 `fpe_context_t` 聚合层降低后续迁移成本。
- **S8–S10 通往 2.1**:像素路径是 GLSL 翻译器和 selection/feedback 的基础设施(CPU 侧几何/像素处理管线复用);GL 2.0 可编程管线是 2.1 的主体;最后收尾补全标准内的长尾特性。

## 使用约定

- 每个阶段文件内的任务用 `- [ ]` 复选框跟踪,完成后勾选并附提交号;
- 阶段的**退出标准全部满足**才能宣布阶段完成;测试类退出标准不许"以后补";
- 特性状态变更(supported ↔ unsupported)必须同步更新生成式 API manifest(S3 起)与本目录文档;
- 发现计划与代码现实冲突时,以代码审计流程重验,更新计划文件并注明日期。
