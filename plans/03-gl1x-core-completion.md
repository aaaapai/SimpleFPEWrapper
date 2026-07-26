# S3 GL 1.x 核心 API 面补全

**目标**:补齐 GL 1.x 应用(尤其 LWJGL2/老 Minecraft)高频触碰、当前会拿到空函数指针或错误行为的 API 面:纹理坐标全变体、`glRect*`、getter 家族、容量/状态查询、常用杂项符号;建立**生成式 API manifest** 取代过期的手写 YAML。

**前置**:S2(错误状态机;`glGetError` 已可用)。
**规模**:中(2–4 周,任务高度并行)。

## 现状(审计证据)

- `glTexCoord`/`glMultiTexCoord` 家族 64 个声明中仅 2f/4f 有实现(`drawing1x.cpp:315-351`),其余 56 个纯声明、未注册 GETPROC;缺 `glMultiTexCoord4fARB` 别名(只有 2fARB)。
- `glRect*` 8 个变体、`glRasterPos*` 24 个变体:纯声明零实现(`drawing1x.h:283-321`)。RasterPos 归 S8,`glRect*` 在本阶段(低成本,委托 `glBegin(GL_QUADS)`)。
- Getter 缺口:`glGetBooleanv`/`glGetDoublev`(全仓零命中)/`glGetPointerv`/`glIsEnabled`/`glGetLightfv`/`glGetMaterialfv`/`glGetTexEnvfv|iv` 全缺;`glGetFloatv` 仅覆盖 MODELVIEW/PROJECTION 两个矩阵 pname(`getter.cpp:352-368`)。
- `glGetIntegerv` 仅拦截 5 个 pname(`getter.cpp:325-349`),**GL_MAX_LIGHTS、GL_MAX_TEXTURE_UNITS、GL_MATRIX_MODE、GL_MODELVIEW_STACK_DEPTH 等固定管线应用最常查询的 pname 全部直通 GLES** → `GL_INVALID_ENUM` 且不写出参(批判员实测)。
- 缺失符号(grep 证实不在 GETPROC 表):`glPolygonMode`、`glPointSize`、`glDepthRange`、`glPushAttrib`/`glPopAttrib`(归 S6)、`glTexGen*`(归 S5)、`glHint`、`glPixelStorei`、`glGetError`(S2 已做)。LWJGL 解析到空指针即崩。
- `include/` 自带 GL 头声明面与实际导出面不一致,无人系统盘点。
- `active_texture_index()` 每次 GL_TEXTURE 矩阵操作同步调后端 `glGetIntegerv`(`transformation.cpp:31-35`)→ 应改用影子状态。
- 手写 `fpe_implementation_progress.yaml` 过期近 3 个月且 meta 计数自相矛盾(正文实际 6+5+15=26 类,meta 写 18)。

## 任务

### 3.1 即时模式收尾
- [ ] `glTexCoord{1,2,3,4}{s,i,f,d}[v]` 32 个 + `glMultiTexCoord{1,2,3,4}{s,i,f,d}[v]` 32 个:沿用 `state.cpp:713-770` 的宏 + `mgl*` 模板模式实现;1D 补 t=0、3D/4D 按齐次语义;全部注册 GETPROC + `ARB` 别名(至少 `glMultiTexCoord{1,2,3,4}fARB`)。
- [ ] `glRect{d,f,i,s}[v]`:委托 `glBegin(GL_QUADS)+4×glVertex2+glEnd`;可进显示列表;补测试。
- [ ] `glEdgeFlag`/`glEdgeFlagv`/`glEdgeFlagPointer`:状态存储 + 录制(渲染效果依赖 S8 的 PolygonMode,先存不消费,manifest 标 partial)。
- [ ] `glSecondaryColor3*`/`glSecondaryColorPointer`、`glFogCoord*`/`glFogCoordPointer`:补声明+实现+GETPROC(`vp2idx`/`idx2vp` 已预留槽位);shader 消费分别归 S4(GL_COLOR_SUM)与 S5(GL_FOG_COORD_SRC)。
- [ ] `glArrayElement`、`glInterleavedArrays`(14 种 format):实现并接入 S2 的绘制路径。

### 3.2 矩阵与变换收尾
- [ ] 矩阵栈深度上限与查询:`GL_MAX_MODELVIEW_STACK_DEPTH`(≥32)、`GL_MAX_PROJECTION/TEXTURE/COLOR_STACK_DEPTH`(≥4/≥2 按规范),`GL_*_STACK_DEPTH` 当前深度查询;溢出错误已在 S2。
- [ ] `matrix_idx` 非法枚举 → `GL_INVALID_ENUM`(替换静默写 ModelView,`transformation.cpp:220-221`)。
- [ ] `active_texture_index()` 改读影子状态,消除每次同步 `glGetIntegerv` 往返。
- [ ] Color 矩阵:当前只存不用(imaging subset 之外 GL 2.1 也不要求消费)→ manifest 显式标注 "stored, not applied",查询可用。
- [ ] `glDepthRange(GLdouble,GLdouble)`:包装为 `glDepthRangef` + clamp;注册符号。

### 3.3 Getter 全家族
- [ ] 实现 `glGetBooleanv`/`glGetDoublev`/`glGetPointerv`/`glIsEnabled`,共用一套"pname → 值来源"分发表(FPE 影子状态优先,其余透传后端 + 类型转换)。
- [ ] `glGetIntegerv`/`glGetFloatv` 补全固定管线 pname:容量类(`GL_MAX_LIGHTS`=8、`GL_MAX_TEXTURE_UNITS`=可支持数、`GL_MAX_CLIP_PLANES`、栈深度上限)、状态类(`GL_MATRIX_MODE`、`GL_*_STACK_DEPTH`、`GL_CURRENT_COLOR/NORMAL/TEXTURE_COORDS`、`GL_LIST_INDEX/GL_LIST_MODE`、fog/alpha/light 各 enable 与参数)、全部四组矩阵(含 per-unit TEXTURE_MATRIX)。
- [ ] `glGetLightfv|iv`、`glGetMaterialfv|iv`、`glGetTexEnvfv|iv`:读已有存储;`glGetTexGen*` 待 S5 有 texgen 状态后接入。
- [ ] `glGetTexParameterfv|iv`:透传 + 对 wrapper 模拟的 pname(如 S5 的 border/clamp 转译)返回影子值。
- [ ] 以 GL 2.1 规范第 6 章状态表为清单逐项过一遍,能回答的回答,暂不能的显式 `GL_INVALID_ENUM` + manifest 记录(禁止直通 GLES 吃错误)。

### 3.4 杂项符号
- [ ] `glHint`:接受全部 GL 2.1 合法 target(FOG_HINT、PERSPECTIVE_CORRECTION_HINT 等),存状态、行为 no-op,非法 target 设错。
- [ ] `glPointSize`:存 FF 点大小,shader 生成器输出 `gl_PointSize`(点参数/衰减归 S9)。
- [ ] `glPolygonMode`:存状态 + 注册符号;`GL_FILL` 即时生效,`GL_LINE/GL_POINT` 的模拟归 S8(在此之前遇到非 FILL 模式打日志 + manifest 标 partial)。
- [ ] `glPixelStorei|f`:拦截并存全部 GL 2.1 pname;ES3 原生支持的(ROW_LENGTH/SKIP_* 等)透传,桌面专属的(SWAP_BYTES/LSB_FIRST)存状态,消费归 S5/S8。
- [ ] `glClipPlane`:存 plane 方程(call-time ModelView 变换),`glGetClipPlane` 查询;shader 消费(discard 或 EXT_clip_cull_distance)归 S8。

### 3.5 生成式 API Manifest(取代手写 YAML)
- [ ] 写脚本(python,进 CTest):解析 `lookup.cpp` GETPROC 表 + 导出符号 + `include/GL` 头文件声明,生成 manifest(状态:exported / implemented / tested / deferred / unsupported),输出 markdown + 机器可读 JSON。
- [ ] CI 校验:GL 2.1 全入口点清单(按 gl.xml 或头文件生成)与 manifest 差异必须显式标注状态,新增未标注符号即失败。
- [ ] 归档 `fpe_implementation_progress.yaml`(移入 `docs/archive/` 或删除,留 README 指向 manifest)。
- [ ] 决策落地:`glClientActiveTexture` 与显示列表——**遵循 GL 规范**(client state 命令立即执行、不编入列表),清理 `state.cpp:154` TODO,并修订 ROADMAP.md:127-128 的相反要求(该冲突为审计裁决项)。

## 退出标准

1. 老 MC 常用符号集(附录:LWJGL2 GL11/GL13 全表)经 `eglGetProcAddress` 解析零空指针;
2. manifest 生成进 CI,GL 1.x 入口点 100% 有状态标注;
3. getters 单测:每个新增 pname 一条断言;`glGetIntegerv(GL_MAX_LIGHTS)` 等容量查询返回规范最小值以上;
4. 新增即时模式变体经参考场景与既有 2f/4f 路径图像一致(S11 harness)。

## 风险与决策点

- `GL_MAX_TEXTURE_UNITS` 报多少?固定管线单元数(建议 8,匹配 MAX_TEX=16 中实际消费的合批/uniform 上传能力)与 `GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS` 分开报,避免应用按 16 全开踩没验证过的路径。
- `glGetDoublev` 引入 double 转换面 → 统一由分发表做 float→double,勿逐函数手写。
