# S2 正确性与错误契约

**目标**:修复审计发现的全部正确性/内存安全缺陷;建立 wrapper 自有的 GL 错误状态机;补齐 `glDrawElements` 的 FPE 转换,完成旧 ROADMAP Phase 1 的硬缺口。

**前置**:S1(双编译器构建 + CTest + 日志层)。
**规模**:中(2–4 周)。

## A. GL 错误状态机(后续一切错误契约的地基)

现状:整个 wrapper **没有自己的错误状态**;`glGetError` 甚至不在 `lookup.cpp` 的 GETPROC 表里(批判员 grep 证实),非法枚举被静默吞掉(如 `transformation.cpp:220-221` `matrix_idx` 对非法 enum 返回 0 写进 ModelView)。

- [ ] 实现 per-context 错误槽(挂 `fpe_context_t`):`fpe_set_error(GLenum)` 只记录第一个未读错误(GL 语义)。
- [ ] 拦截并导出 `glGetError`:返回 wrapper 错误优先,其次转查后端错误;定义两者合并顺序并写入文档。
- [ ] 全库替换"静默吞错"点:非法枚举 → `GL_INVALID_ENUM`,非法值/越界 → `GL_INVALID_VALUE`,状态机违规(如 `glBegin` 嵌套、`glEnd` 无配对,`drawing1x.cpp:280-293`)→ `GL_INVALID_OPERATION`。
- [ ] `glBegin/glEnd` 之间禁止的调用(矩阵、状态等)按规范设 `GL_INVALID_OPERATION`。
- [ ] 矩阵栈:设定深度上限(ModelView ≥ 32,其余 ≥ 4,可查询,见 S3),溢出/下溢设 `GL_STACK_OVERFLOW`/`GL_STACK_UNDERFLOW`(替换现在的无限增长 `transformation.cpp:452` 与空栈静默返回 `:469`)。
- [ ] shader 编译/链接失败:跳过绘制(现状正确,`fpe.cpp:216-219`)**并设 `GL_INVALID_OPERATION`**;失败 program 的缓存策略改为"缓存失败标记 + 状态变化后允许重试"。

## B. 绘制路径正确性

现状:`glDrawArrays` 拦截判定(program==0 且 GL_VERTEX_ARRAY 启用,`drawing.cpp:706-714`)与状态 guard(`fpe.hpp:33-62`)达标;但 **`glDrawElements` 是纯透传**(`ordered_passthrough.cpp:50-51`),索引化 GL_QUADS 不存在。

- [ ] `glDrawElements` 接入 FPE:与 `glDrawArrays` 相同的拦截判定;client-memory 索引与 VBO 索引(GL_ELEMENT_ARRAY_BUFFER)两条路径;`GL_UNSIGNED_BYTE/SHORT/INT` 三种索引类型。
- [ ] 索引化 `GL_QUADS`/`GL_QUAD_STRIP`/`GL_POLYGON` 的索引重写(quad→两三角;复用 `fpe.cpp:273-303` 的既有 QUADS 基建)。
- [ ] `glDrawRangeElements`/`glArrayElement`/`glInterleavedArrays` 一并接入(后两个符号面在 S3 注册)。
- [ ] 修复 stride=0 client 指针路径:`vertexpointerarray.cpp:60-64` 把裸地址当 offset 参与 stride 计算并截断为 `GLsizei` —— 改为 per-attribute 紧凑 stride(显示列表捕获路径已有正确实现,下沉共用)。
- [ ] 修复 `pointer < starting_pointer` 时无符号下溢产生巨大 offset(`vertexpointerarray.cpp:60-61`)。
- [ ] 修复调用方 VBO 覆写风险:`commit_fpe_state_on_draw` 在 `previous_array_buffer != 0` 时不换绑 fpe_vbo(`fpe.cpp:223-232`),client 指针数据可能写进调用方 VBO——上传前强制绑定 FPE 私有 VBO。
- [ ] 修复 `*count * vpa.stride` 的 `GLsizei` 乘法溢出(`fpe.cpp:266`)→ 用 64 位中间量 + 上限校验。
- [ ] guard 补齐 active texture 恢复(旧 ROADMAP Phase 1 点名;现 `fpe_backend_draw_state_guard_t` 不含)。
- [ ] 定义并测试"零启用数组"行为:透传或按规范空操作,不得进 FPE 崩溃。

## C. 即时模式正确性

- [ ] primitive 中途属性布局改变:`advance()` 按当时 sizes 打包(`drawstate1x.cpp:24-27`)而 `drawImmediateVertices` 用最终 sizes 解释全部数据(`drawing1x.cpp:148-159`)——首顶点之后才首次 `glColor*`/`glTexCoord*` 会画出损坏几何(GL 1.x 合法用法)。方案:`glBegin` 时锁定布局为"全属性、缺省填充当前值",或检测布局变化时重打包已收集顶点。
- [ ] 脏顶点泄漏:`glEnd` 在 `primitive==GL_NONE` 时提前 return 不重置缓冲(`drawing1x.cpp:291-293`);`glBegin` 外的 `glVertex*` 静默丢弃(NDEBUG 下断言失效,S1 已改运行时检查)→ 丢弃 + 不污染下一批次。
- [ ] `glMultiTexCoord*` target 越界:运行时检查 `GL_TEXTURE0 <= target < GL_TEXTURE0+MAX_TEX`,越界设 `GL_INVALID_ENUM`(替换失效 assert;`mglTexCoord` 直写 `texcoord[texid]` 的负下标/越界写,`drawing1x.h:31-40`)。
- [ ] `GL_QUADS` 顶点数非 4 倍数:按规范丢弃不完整四边形(现状 `(drawCount/4)*6` 静默截断恰好符合,补测试固化语义)。
- [ ] 字形批处理 flush 完备性:批次 key 只含 sizes+activeTexture+纹理绑定(`drawing1x.cpp:201-206`),`glTexParameter*` 修改同一纹理对象不会触发 flush → 把 `glTexParameter*` 加入 flush 触发集;将"所有状态修改入口必须调 `flushPendingImmediateDraws`"的 allowlist 约定改为**集中式钩子**(状态写入统一走 `fpe_context_t` 访问层,S1 埋点),消除约 60 处手工调用漏点风险。
- [ ] 环形 VBO 同步:扩容/回绕处的 `glFinish`(`drawing1x.cpp:46,85`)改为 fence sync(`glFenceSync`/`glClientWaitSync`)多段环形。

## D. 显示列表安全

- [ ] 自引用/相互递归列表无限递归(`list.h:179-188`):引入回放深度计数,超过 `GL_MAX_LIST_NESTING`(定为 64)即停止并记录日志。
- [ ] `glDeleteLists(list, -1)`:`GLsizei` 负 range 被转无符号循环约 2^32 次(`list.h:102-104`)→ `range < 0` 设 `GL_INVALID_VALUE` 直接返回。
- [ ] `glCallLists(n<0)`:录制路径 `n * type_to_bytes` 的 `size_t` 下溢导致 memcpy 巨值崩溃 → 前置校验;未知 `type` 得 0 长度 → 设 `GL_INVALID_ENUM`。
- [ ] `pname_to_count`/`material_param_count` 等映射(`pointer_utils.cpp:42-72`)对未收录 pname 返回 0 → 深拷贝空 buffer、回放堆外读:改为"未知 pname → 不录制 + `GL_INVALID_ENUM`",并逐项核对映射表覆盖全部已实现 pname(批判员点名无人核对过)。
- [ ] program 缓存 hash 碰撞:`fpe_programs` 以 64 位 XXHash64 裸值为 key、命中不比对全状态(`glstate.cpp:311-329`)→ 命中后追加全状态 memcmp,碰撞则用完整状态副本作 key 兜底;清理 `types.h:368` 过时 TODO(注:该 TODO 所述"vp 作 key"已过时,现 hash 已覆盖全部 shader 相关状态——审计 6 的表述有误,勿按它改)。
- [ ] shader 生成器防御:未知 `alpha_func` 会把错误文本拼进 GLSL(`fpe_shadergen.cpp:1175`)、非法 `fog_mode` 导致 `fogFactor` 未声明(`state.cpp:208-209` 不校验 + `fpe_shadergen.cpp:1520-1606` 无 default)→ 在状态写入层校验枚举(设 GL error),生成器对非法值取安全默认。

## 退出标准

1. 上述每一项修复都有对应的最小复现单测(修复前红、修复后绿),ASan/UBSan 下全绿;
2. 混合管线集成测试(S11 harness):程序化渲染一帧 + FPE 渲染一帧交替两轮,前后状态快照逐项一致(旧 ROADMAP Phase 1 退出标准);
3. `glDrawElements`(client 内存 / VBO 索引 × 三种索引类型 × QUADS/TRIANGLES)渲染结果与 `glDrawArrays` 等价场景一致;
4. 错误契约测试:非法枚举/越界/状态机违规各类至少一例,`glGetError` 返回符合规范。

## 风险与决策点

- 错误状态机会引入每调用开销 → 错误路径设计为冷路径(仅出错时写),热路径零成本。
- `glDrawElements` 索引重写在大索引缓冲上有 CPU 成本 → 缓存重写结果(与现有 QUADS IBO 缓存同策略)。
- 集中式状态钩子改造面大 → 允许分两步:先补 `glTexParameter*` 等已知漏点,后做访问层收口(与 S7 合并评估)。
