# S5 纹理完善

**目标**:补全 GL 2.1 纹理子系统:GL_COMBINE 完整实现、texgen、legacy 内部格式与 GL_BGRA、桌面像素存储语义、border/clamp、1D 纹理、纹理查询;消除"纹理/像素子系统无人审计"的盲区。

**前置**:S2(错误状态机)、S3(getter 分发表、glPixelStorei 状态)。
**规模**:中大(3–4 周)。

## 现状(审计证据 + 批判员盲区提示)

- 已完成:texenv `GL_MODULATE/REPLACE/DECAL/BLEND/ADD`(`fpe_shadergen.cpp:1553-1578`,入口 `state.cpp:605-706` 附近)、16 单元独立 enable(`types.h:83`)与逐单元 sampler/TexMat/TexEnvColor 上传(`glstate.cpp:128-148`)、fog 三模式逐像素、alpha test 全 8 函数、per-unit 纹理矩阵。
- `GL_COMBINE` 被有意降级为 MODULATE 近似,combiner 参数存储但从不读取。
- `GL_DECAL` 缺 `GL_RGB` 内部格式分支(只有 RGBA 路径)。
- `glTexGen*` 全缺:未声明、未实现、无状态。
- **纹理数据路径是审计盲区**(批判员点名,无专门审计):`glTexImage2D` 常规路径纯透传——legacy internalformat(`GL_ALPHA8`、`GL_LUMINANCE*`、`GL_INTENSITY*`、sized RGB)在 GLES3 后端的行为无人验证;commit `d30e557` 移除了 BGRA 处理,桌面应用传 `GL_BGRA` 会得 `GL_INVALID_ENUM`,后果无人评估过。
- `glPixelStorei` 桌面 pname(`GL_UNPACK_SWAP_BYTES` 等)无人处理(S3 已存状态,本阶段消费)。
- commit `254caef` 做了 legacy texture clamp 翻译(`GL_CLAMP`→?),覆盖面未验证;`GL_CLAMP_TO_BORDER`/border color GLES3 不支持。
- `glGetTexEnv*`、`glGetTexGen*`、`glGetTexLevelParameter*`、`glGetTexImage` 全缺。

## 任务

### 5.0 盲区摸底(先审后改)
- [ ] 专项审计纹理数据路径:`glTexImage1D/2D/3D`、`glTexSubImage*`、`glCopyTex*`、`glCompressedTex*` 当前透传行为;在 Mesa GLES3 上实测 legacy internalformat 各值的实际结果;产出格式转换需求表。此表决定 5.3 的映射清单。

### 5.1 texenv 收尾
- [ ] `GL_COMBINE` 完整实现:`COMBINE_RGB/ALPHA`(REPLACE/MODULATE/ADD/ADD_SIGNED/INTERPOLATE/SUBTRACT/DOT3_RGB/DOT3_RGBA)× `SRC0..2`(TEXTURE/TEXTUREn/CONSTANT/PRIMARY_COLOR/PREVIOUS)× `OPERAND0..2` + `RGB_SCALE/ALPHA_SCALE`;shader 生成器按单元展开。
- [ ] `GL_DECAL` 补 GL_RGB 分支(依赖 5.3 的内部格式跟踪:decal 语义按基础格式区分)。
- [ ] `GL_TEXTURE_ENV` 非法 pname/param 设错;`glGetTexEnvfv|iv` 接入。
- [ ] per-unit `GL_TEXTURE_1D/2D/3D/CUBE_MAP` enable 优先级语义(高维优先)——当前只有 texture_2d_enable,按 GL 2.1 优先级表扩展(1D/3D/cube 的实际支持见 5.4)。

### 5.2 texgen
- [ ] 状态:`glTexGen{i,f,d}[v]` S/T/R/Q 四坐标 × `GL_OBJECT_LINEAR/GL_EYE_LINEAR/GL_SPHERE_MAP/GL_NORMAL_MAP/GL_REFLECTION_MAP`;eye plane 存储时做 call-time ModelView 变换(与光位置同语义);`glGetTexGen*` 查询。
- [ ] shader 生成:enable 的坐标按模式生成坐标计算(sphere/normal/reflection 需要 eye-space normal,与 S4 光照共用);与显式纹理坐标按坐标分量混合(部分坐标 texgen、部分来自 glTexCoord 是合法的)。
- [ ] `GL_TEXTURE_GEN_{S,T,R,Q}` enable 进 program hash 与显示列表。

### 5.3 纹理格式与像素存储
- [ ] internalformat 映射表:`GL_ALPHA[4/8/12/16]`→`GL_R8`+swizzle(000R)、`GL_LUMINANCE*`→`GL_R8`+swizzle(RRR1)、`GL_LUMINANCE_ALPHA`→`GL_RG8`+swizzle(RRRG)、`GL_INTENSITY*`→`GL_R8`+swizzle(RRRR)、sized RGB/RGBA(GL_RGB5/RGB8/RGBA8/RGB10_A2…)→ES3 sized 格式;用 ES3 原生 `GL_TEXTURE_SWIZZLE_*` 实现,记录每纹理对象的逻辑格式(供 decal/查询用)。
- [ ] `GL_BGRA` 外部格式:优先 `EXT_texture_format_BGRA8888`,否则 CPU 就地换序;重新评估 `d30e557` 的移除决定(1.12.2 之外的桌面应用需要 BGRA)。
- [ ] `type` 桌面取值(`GL_UNSIGNED_INT_8_8_8_8[_REV]` 等 packed 类型)转换。
- [ ] `glPixelStore` 桌面语义消费:`GL_UNPACK_SWAP_BYTES`/`GL_UNPACK_LSB_FIRST` CPU 预处理;`ROW_LENGTH/SKIP_*`/`ALIGNMENT` ES3 原生透传;PACK 侧同理(与 `glReadPixels`/S8 共用重打包管线)。
- [ ] `glTexParameter` 翻译:`GL_CLAMP`(验证并完善 `254caef`)、`GL_CLAMP_TO_BORDER`+`GL_TEXTURE_BORDER_COLOR`→shader 内 clamp+border 混合(限 FPE 生成的 shader;透传给用户 shader 的场景 manifest 标注偏差);非法值设错。
- [ ] `GL_GENERATE_MIPMAP`(SGIS/1.4 core):tex 参数存储,TexImage 后自动 `glGenerateMipmap`。

### 5.4 纹理目标扩展
- [ ] 1D 纹理:`glTexImage1D` 等映射为 Nx1 的 2D 纹理;shader 采样 s→(s, 0.5);enable 优先级接入。
- [ ] 3D 纹理:ES3 原生 `GL_TEXTURE_3D`,打通 wrapper 路径 + enable。
- [ ] proxy targets(`GL_PROXY_TEXTURE_*`):按规范实现"只做尺寸/格式校验不产生数据",结果经 `glGetTexLevelParameter` 读取。
- [ ] `glGetTexLevelParameterfv|iv`:ES3.0 没有 → 由 wrapper 跟踪每对象每 level 的宽高/格式并作答。
- [ ] `glGetTexImage`:FBO attach + `glReadPixels` + PACK 重打包实现;压缩纹理路径标 unsupported。
- [ ] 纹理优先级 `GL_TEXTURE_PRIORITY`:存状态 no-op(合法实现)。

## 退出标准

1. 图像级测试:双纹理 COMBINE(至少 INTERPOLATE/DOT3 两例)、DECAL(RGB 与 RGBA)、texgen 四模式(含 sphere map 反射球场景)、legacy 格式各映射项(ALPHA/LUMINANCE/INTENSITY 上屏对色)、BGRA 上传、CLAMP/BORDER 行为;
2. 5.0 产出的格式需求表 100% 有归宿(实现/显式 unsupported);
3. `glGetTexEnv/TexGen/TexParameter/TexLevelParameter` 查询单测;
4. 老 MC 冒烟:1.7.10 与 1.12.2 地形+GUI 无回归(实机或 renderdoc 帧对比)。

## 风险与决策点

- swizzle 是 per-texture-object 状态,若应用自己也设 swizzle 会冲突 → GL 1.x/2.1 应用无 swizzle API,冲突面为零;manifest 注明。
- border clamp 的 shader 模拟只覆盖 FPE 生成的 shader → S9 用户 shader 场景的偏差要在 manifest 与文档声明。
- `glGetTexImage` 的格式组合矩阵很大 → 先覆盖常用 (RGBA/UNSIGNED_BYTE、BGRA、DEPTH_COMPONENT),其余设 `GL_INVALID_ENUM` 并记录。
