# S9 GL 2.0 可编程管线

**目标**:让 GL 2.0 应用的 GLSL 1.10/1.20 shader 在 ESSL 3.00 后端上工作:源码翻译、内建变量绑定到 FPE 状态、shader objects API(含 ARB 别名)、固定管线与用户 shader 的混合语义、点精灵。这是通往 2.1 的主体工程。

**前置**:S4/S5(FPE 状态 uniform 体系成熟——内建 uniform 要绑它们)、S7(per-context:用户 program 对象表)。
**规模**:大(6–8 周;翻译管线〔预处理器 + glslang/SPIRV-Cross 集成〕是独立子项目)。

## 现状

- GLES3 后端原生有 `glCreateShader/glShaderSource/glCompileShader/...`,当前 wrapper 对这些大概率透传(S9 起必须拦截:GLSL 110/120 源码直接喂 ES3 驱动会编译失败)。
- ARB 后缀 shader objects(`glCreateShaderObjectARB`、`GLhandleARB` 语义、`glGetObjectParameterivARB`…)完全缺失,LWJGL2 时代应用(MC shader mod、OptiFine)全走 ARB 入口。
- FPE 已有完整的矩阵/光照/雾/texenv 状态与 uniform 上传体系(S4/S5 后更完整),内建 uniform 翻译可直接复用。

## 任务

### 9.1 GLSL 1.10/1.20 → ESSL 3.00 翻译管线(preprocess + glslang)

**路线已定(2026-07-26):不自研翻译器。** 管线:

```
用户源码 (GLSL 110/120)
  → 自制预处理器(compat 内建改写 + prelude 注入,输出"core 化"的 GLSL)
  → glslang(desktop profile 前端解析 → SPIR-V)
  → SPIRV-Cross(SPIR-V → ESSL 3.00 源码)
  → 后端 glCompileShader
```

语法级转换(attribute/varying→in/out、`gl_FragColor`→out、`texture2D`→`texture`、非方矩阵等 120 特性)由 glslang+SPIRV-Cross 承担,**预处理器只做工具链吃不掉的部分**:

- [ ] 预处理器(自制,token 级文本处理):
  - `glShaderSource` 多段拼接、`#version` 识别与规范化(110/120/缺省 → 对应 glslang profile 参数);
  - compat 内建改写:`gl_ModelViewMatrix/gl_ProjectionMatrix/gl_ModelViewProjectionMatrix/gl_NormalMatrix/gl_TextureMatrix[n]/gl_LightSource[n]/gl_FrontMaterial/gl_BackMaterial/gl_Fog/gl_LightModel/gl_ClipPlane[n]/gl_Point` → 注入的 `fpe_*` uniform;`gl_Vertex/gl_Normal/gl_Color/gl_SecondaryColor/gl_MultiTexCoord0..7/gl_FogCoord` → 注入的 `fpe_*` attribute(对齐 `vp2idx` 槽位);`gl_FrontColor/gl_BackColor/gl_TexCoord[]/gl_FogFragCoord` → 注入 varying;`ftransform()` 展开为 `fpe_ModelViewProjectionMatrix * fpe_Vertex`。注意:**必须做 token 替换而非 `#define`**(`gl_` 开头宏名是 GLSL 保留字);
  - 注入 prelude 声明块,同时产出"该 shader 消费的 FPE 状态清单",供增量 uniform 上传订阅(挂接 glstate.cpp 体系,加"用户 program 也订阅状态上传"的通道);
  - 插入 `#line` 指令,使 glslang 的 info log 行号可映射回用户原始源码。
- [ ] glslang 集成:以 3rdparty 子模块引入并 pin 版本;裁剪构建(去 HLSL 前端等无关组件);desktop 110/120 profile 解析参数调优(老代码常依赖宽松的隐式转换)。
- [ ] SPIRV-Cross 集成:ESSL 3.00 输出目标(ES 3.0 版本、精度修饰策略);其 reflection 结果用于 `glGetActiveUniform` 的内建 uniform 过滤与 location 映射(9.2 依赖)。
- [ ] 早期原型(本阶段第一周):≥20 个真实 MC 生态 shader 全链路打通,产物在 Mesa GLES3 上零 error —— 作为工具链路线的验收关卡;改写覆盖面缺口在此暴露。
- [ ] 运行时成本控制:翻译结果按源码 hash 缓存(内存 + 可选磁盘持久化,Android 放 app cache 目录);`glCompileShader` 同步翻译的耗时进 S11 基准。
- [ ] 翻译失败路径:保留原文与各级错误日志(经 S1 日志层),`glCompileShader` 报 FALSE + info log 含预处理/glslang/SPIRV-Cross 三级诊断(经 `#line` 映射);**绝不把错误文本拼进 GLSL**(吸取 `fpe_shadergen.cpp:1175` 教训)。
- [ ] 翻译管线独立单测集:MC 1.7/1.8 时代典型 shader(OptiFine 内部 shader、常见 shaderpack 片段)作离线用例,纯 CPU 测试不需要 GL(预处理产物与最终 ESSL 均做金样比对)。

### 9.2 Shader objects API
- [ ] 核心入口拦截:`glShaderSource`(拼接多段源→翻译)、`glCompileShader`、`glLinkProgram`(链接前注入内建绑定;属性位置绑定语义:`glBindAttribLocation` + attrib 0 别名 `gl_Vertex`)、`glGetShaderiv/glGetProgramiv/InfoLog/Source`(Source 返回**原始**源码,按规范)。
- [ ] `glGetActiveUniform/glGetUniformLocation`:过滤/映射注入的内建 uniform(不能把 `fpe_u_ModelView` 之类漏给应用);`glUniform*` 全家族透传。
- [ ] ARB 别名层:`GLhandleARB` 句柄空间(与 GLuint 统一即可,但 `glGetHandleARB`/`glDeleteObjectARB`/`glGetObjectParameterivARB` 的对象类型多态语义要按 ARB_shader_objects 规范实现)、`glUseProgramObjectARB`、`glGetInfoLogARB` 等全表注册 GETPROC。
- [ ] `glValidateProgram`、`glIsShader/glIsProgram`、`glDetachShader`、删除延迟语义(attached 时 delete 挂起)。

### 9.3 混合管线语义(GL 2.0 的难点)
- [ ] 用户 program(VS+FS 齐全)current 时:FPE 完全让路(现有拦截判定 program==0 已正确),但内建 uniform/attribute 上传通道必须激活。
- [ ] **FS-only program**:GL 2.0 允许——固定管线顶点处理 + 用户 FS。方案:FPE 生成配套 VS(现有 shadergen 的 VS 半边)+ 用户翻译后 FS 链接;program hash 引入"用户 FS id"维度。
- [ ] **VS-only program**:用户 VS + 固定管线片元(texenv/fog/alpha)——对称方案,FPE 生成 FS 半边。
- [ ] varying 匹配规则:用户半边引用的 `gl_TexCoord[]` 等内建 varying 与生成半边对接;命名/位置以 SPIRV-Cross 的输出规则为准(reflection 驱动,FPE 生成的半边跟随该命名约定);不匹配时按链接错误报告。
- [ ] `glVertexAttrib{1,2,3,4}*` 常量顶点属性(含 attrib 0 触发顶点语义的坑——ES3 无此语义,由 FPE 即时模式桥接)。

### 9.4 GL 2.0 杂项
- [ ] 点精灵:`GL_POINT_SPRITE` enable + per-unit `GL_COORD_REPLACE`(texenv 状态)→ FPE FS 用 `gl_PointCoord` 替换该单元纹理坐标;`GL_VERTEX_PROGRAM_POINT_SIZE`(ES3 恒开,状态接受即可);`glPointParameter{f,i}[v]`(衰减、fade threshold,GL 1.4)状态+VS 消费。
- [ ] `glDrawBuffers`(ES3 原生透传 + `GL_FRONT/BACK` 桌面枚举映射)、`glStencilOpSeparate/glStencilFuncSeparate/glStencilMaskSeparate`(原生)、`glBlendEquationSeparate`(原生)。
- [ ] `GL_EXTENSIONS`/`GL_VERSION` 字符串策展:版本报 `2.0`(本阶段末)并广告 `GL_ARB_shader_objects/vertex_shader/fragment_shader/shading_language_100/point_sprite` 等(`getter.cpp` 现有伪装逻辑系统化,列表由 manifest 生成)。

## 退出标准

1. 翻译器离线单测 ≥ 50 个真实 shader 用例全过(编译到 ES3 驱动零 error);
2. 集成金图:用户 VS+FS 绘制、FS-only(固定顶点+用户片元)、VS-only、`glUniform` 各类型、`gl_LightSource` 读值正确性(点灯改 uniform 立即生效)、点精灵粒子场景;
3. ARB 入口全表经 `eglGetProcAddress` 解析并有至少冒烟测试;
4. MC 场景验收:一个使用 GL20 入口的真实 mod/版本(如 1.8 的 shaders)可渲染。

## 风险与决策点

- glslang + SPIRV-Cross 的二进制体积与 NDK 构建集成是主要工程成本 → 裁剪构建(去 HLSL/无关组件)+ MinSizeRel + strip,记录 `.so` 体积增量并设预算门槛;若超预算再评估仅 Release 链接或动态下发。
- glslang 对 110/120 老语法的宽松度(隐式类型转换、老式数组用法)与真实老 shader 的兼容性未知 → 9.1 的 20-shader 原型是硬关卡,不过关先扩预处理器改写、不回退自研路线。
- 首次编译延迟(预处理 + glslang + SPIRV-Cross 双跳)在移动端可感 → 源码 hash 缓存 + 可选磁盘持久化;S11 基准跟踪 P95 编译耗时。
- FS-only/VS-only 组合爆炸(用户半边 × FPE 状态 hash)→ program 缓存 key 设计要先行评审(S2 的碰撞修复基础上扩展)。
- `gl_LightSource` 等大结构 uniform 在弱 GPU 上的 UBO vs 散装 uniform 取舍 → 基准后定,初版散装。
