# S6 显示列表完备 + 属性栈

**目标**:显示列表覆盖全部"可编入列表"的已支持命令(尤其 server state),回放与立即执行严格等价;实现 `glPushAttrib`/`glPopAttrib`/`glPushClientAttrib`/`glPopClientAttrib`;为合批优化建立"状态屏障"正确性规则。

**前置**:S2(显示列表安全修复、错误状态机)、S3(manifest;`glClientActiveTexture` 决策已定)。
**规模**:中(2–3 周)。

## 现状(审计证据)

- 已完成:8 个核心 API、COMPILE/COMPILE_AND_EXECUTE 区分(`list.h:132`)、嵌套引用与删除安全(`list.cpp:78-88`)、指针深拷贝所有权(`list.h:134-167`)、五层合批(几何快照/tryMerge/矩阵融合/Push-Draw-Pop 折叠/MultiDraw 合批,`drawing.cpp:326-900`)+ 64MB 顶点 arena + 两级 thread_local 缓存(以 `mutationGeneration` 失效,单线程下逻辑严密)。
- 录制覆盖面限于 FPE 状态+矩阵+即时模式+`glDrawArrays`:**`glBindTexture`/`glTexParameter*`/`glBlendFunc` 等 server state 不进列表**(`ordered_passthrough.cpp:43-47` 宏无 LIST_RECORD;`list.h:28` "Todo: record more functions" 仍在)→ 回放 ≠ 立即执行;且 `tryMerge` 可能跨这些未录制的状态变化错误合并。
- `glActiveTexture` 无 LIST_RECORD(`getter.cpp:370-378`)——GL 规范要求它可编入列表(server state),含它的列表回放会写错单元状态。
- 无 `GL_MAX_LIST_NESTING` 深度限制(S2 修复递归崩溃,本阶段补规范语义与查询)。
- 捕获失败静默丢命令且无 GL error。
- `glPushAttrib` 家族完全未实现,且不在导出表(LWJGL 解析即空指针)。

## 任务

### 6.1 录制覆盖面
- [ ] 给 `ORDERED_PASSTHROUGH` 宏体系增加 LIST_RECORD 能力:`glBindTexture`、`glTexParameter{f,i,fv,iv}`、`glBlendFunc`、`glEnable/glDisable`(server 枚举)、`glDepthFunc/glDepthMask/glCullFace/glFrontFace/glStencil*`、`glTexEnv*`、`glTexImage*`/`glTexSubImage*`(数据深拷贝!注意 PBO 语义到 S10 再接)、`glActiveTexture`(修 `getter.cpp` 缺口)等——以 GL 2.1 规范 5.4 节"不可编入列表"黑名单为准,黑名单外的已支持命令全部可录。
- [ ] 黑名单命令(client state、`glFlush/glFinish`、getters、`glIsList` 等)在录制模式下**立即执行**(规范语义),逐项测试。
- [ ] `glNewList` 错误语义:list=0 → `GL_INVALID_VALUE`;录制中再 `glNewList` → `GL_INVALID_OPERATION`;`glEndList` 无配对 → `GL_INVALID_OPERATION`;捕获失败(内存)→ `GL_OUT_OF_MEMORY` 且列表整体作废,不再静默丢单条命令。
- [ ] `GL_MAX_LIST_NESTING`(64)查询接入;超深回放停止并设错(S2 已有防崩,这里补语义)。
- [ ] `GL_LIST_INDEX`/`GL_LIST_MODE`/`GL_LIST_BASE` 查询。

### 6.2 合批与状态屏障
- [ ] 定义"屏障命令"集合:新录制的每个 server-state 命令必须声明是否中断 `tryMerge`/矩阵融合/MultiDraw 合批(默认全部中断,再逐个放白);写成表驱动,禁止散落 if。
- [ ] 回放等价性测试骨架:同一命令序列 (a) 立即执行 (b) COMPILE+glCallList (c) COMPILE_AND_EXECUTE 三路,状态快照 + 渲染图像全等;纳入 S11 harness,后续每加一条录制命令自动生成三路用例。
- [ ] 缓存失效审查:`mutationGeneration` 对新录制命令类型的覆盖(列表内容含纹理数据时,`glDeleteTextures`/`glTexImage` 对已缓存合批的影响)。

### 6.3 属性栈
- [ ] `glPushAttrib`/`glPopAttrib`:按 GL 2.1 定义的 attrib group 逐组实现快照/恢复,**只覆盖 wrapper 已跟踪的状态**,每组的覆盖清单写进 manifest(与真实 GL 的差异显式化);`GL_ALL_ATTRIB_BITS` 支持;栈深 `GL_MAX_ATTRIB_STACK_DEPTH`(16),溢出/下溢设错。
- [ ] 透传状态入组:blend/depth/stencil 等后端状态需要影子跟踪才能恢复 → 借 6.1 的 LIST_RECORD 影子层同一套状态镜像(一次投资两处使用)。
- [ ] `glPushClientAttrib`/`glPopClientAttrib`:`GL_CLIENT_VERTEX_ARRAY_BIT`/`GL_CLIENT_PIXEL_STORE_BIT` 两组。
- [ ] 属性栈本身**不可**编入显示列表执行语义核对(Push/PopAttrib 是可录制的 server 命令,按规范录)。

## 退出标准

1. 等价性三路测试对全部已录制命令通过(图像 + 状态快照);
2. 含 `glActiveTexture`+`glTexEnv`+`glBindTexture` 的列表回放写对目标单元(现有已知缺陷的回归测试);
3. `glPushAttrib` 全组快照/恢复单测;组覆盖清单进 manifest;
4. 合批优化在屏障测试集(状态变化夹在可合并 draw 之间)下不产生错误合并,且合批收益基准无显著回退(S11 帧时间基准)。

## 风险与决策点

- server-state 影子镜像是 S6/S7 共同依赖 → 先在本阶段以进程单例实现,结构上挂 `fpe_context_t`(S1 聚合层),S7 直接搬迁。
- `glTexImage*` 录制的深拷贝内存量可能很大(MC 不常用,但规范要求)→ 设上限 + `GL_OUT_OF_MEMORY` 策略,manifest 注明。
- 合批中断规则保守化可能吃掉此前的性能优化 → 基准回归门槛设为 ±5%,超过则细化白名单。
