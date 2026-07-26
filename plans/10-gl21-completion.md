# S10 GL 2.1 收尾

**目标**:补齐 GL 2.1 与 2.0 的差异(PBO、sRGB、GLSL 1.20 完备)以及标准内尚未落地的长尾特性(evaluators、selection/feedback、accum、颜色索引决策),完成最终符号面与状态表清点,宣布 GL 2.1。

**前置**:S8(CPU 几何/像素基建)、S9(翻译器、版本字符串体系)。
**规模**:大(5–8 周,子项彼此独立可并行)。

## 任务

### 10.1 GL 2.1 核心增量
- [ ] PBO:`GL_PIXEL_PACK/UNPACK_BUFFER` 绑定点(ES3 原生)——重点是**wrapper 模拟的像素路径也要尊重 PBO 绑定**(S5 的 TexImage 转换、S8 的 DrawPixels/ReadPixels 重打包在 PBO 绑定时按 offset 语义从 buffer 取数:map buffer → CPU 转换 → unmap);`GL_ARB_pixel_buffer_object` 别名枚举。
- [ ] sRGB 纹理:`GL_SRGB[8]/GL_SRGB_ALPHA/GL_SRGB8_ALPHA8/GL_COMPRESSED_SRGB*` internalformat 映射到 ES3 原生;`EXT_texture_sRGB` 广告。
- [ ] GLSL 1.20 完备性核对(翻译器):非方矩阵 uniform(`glUniformMatrix2x3fv` 等 6 个入口,ES3 原生)、`invariant`、`centroid`、数组/矩阵构造语法全量用例。
- [ ] 版本字符串升至 `2.1`;`GL_SHADING_LANGUAGE_VERSION` 报 `1.20`。

### 10.2 Evaluators(CPU 曲面求值)
- [ ] `glMap1/2{f,d}`(9 种 target:VERTEX_3/4、COLOR_4、NORMAL、TEXCOORD_1..4)状态存储与 `glGetMap{dv,fv,iv}` 查询;
- [ ] `glEvalCoord1/2*`:CPU 求值(Bernstein 多项式)后注入即时模式管线(等价于合成 glVertex/glColor/glNormal/glTexCoord 调用,规范语义);`GL_AUTO_NORMAL` 偏导叉积;
- [ ] `glMapGrid1/2*` + `glEvalMesh1/2`/`glEvalPoint*`:网格遍历生成;
- [ ] 可编入显示列表;金图:经典 Bezier 面片 + 茶壶片段。

### 10.3 Selection & Feedback(CPU 变换管线)
- [ ] `glRenderMode(GL_SELECT/GL_FEEDBACK/GL_RENDER)` 三态机;SELECT/FEEDBACK 模式下 FPE 绘制路径改走 CPU:顶点经当前 MVP 变换 + 视锥判定,**不发 GPU 绘制**(FPE 的 CPU 可见顶点数据使这可行——即时模式与顶点数组路径都在手上);
- [ ] selection:name 栈(`glInitNames/glPushName/glPopName/glLoadName`,栈深 64)+ hit record 写入(zmin/zmax 定点格式按规范);
- [ ] feedback:`glFeedbackBuffer` 各 type 的 token 流(顶点/颜色/纹理坐标按 type 裁剪)、`glPassThrough`;
- [ ] 溢出语义(返回 -1)、`glRenderMode` 返回值语义;用户 shader current 时的行为定义(按固定管线变换,manifest 注明);
- [ ] 单测:拾取立方体场景命中记录逐字节比对(可对照 Mesa 桌面 GL 输出)。

### 10.4 累积缓冲
- [ ] `glAccum(GL_ACCUM/LOAD/ADD/MULT/RETURN)` + `glClearAccum`:RGBA16F FBO 模拟,ACCUM/RETURN 用全屏 pass(加/乘/回写 × value);默认帧缓冲与 FBO 绑定切换保护;
- [ ] `GL_ACCUM_*_BITS` 查询报模拟精度;
- [ ] 金图:多帧累积运动模糊经典用例。

### 10.5 显式决策项
- [ ] 颜色索引模式:EGL 无 CI visual,按规范以"实现不提供 color index visual"处理——`glIndex*`/`glClearIndex`/`glIndexMask`/`glIndexPointer` 提供入口 + 状态存储(RGBA 模式下按规范多数为 no-op),manifest 标 `unsupported (no CI visuals)`;`drawing1x.h` 里注释掉的声明清理。
- [ ] `GL_ARB_imaging` subset(color table/convolution/histogram):GL 2.1 可选,**决策:不实现**,`GL_EXTENSIONS` 不广告,相关入口不导出(查询按无扩展语义)。
- [ ] `glGetTexImage` 若 S5 未完成压缩纹理分支,在此收尾或定版 unsupported。
- [ ] 双缓冲相关桌面枚举(`GL_FRONT` 读写、`glDrawBuffer(GL_FRONT)`):EGL 下按能力落地(单 surface 无 front 渲染 → `GL_INVALID_OPERATION` + 文档)。

### 10.6 最终清点(宣布 2.1 的门)
- [ ] 符号面:GL 2.1 全入口点(以 gl.xml 2.1 profile 为准,约 550 个)在 manifest 里 100% 状态化;`exported+implemented` 之外的每一项都有理由与文档;
- [ ] 状态表:GL 2.1 规范第 6 章全部状态项过一遍 getter 矩阵(脚本化:pname 清单 × get 函数 → 期望来源),缺口清零或标注;
- [ ] 错误契约抽查:每章节至少一组负面测试;
- [ ] `GL_EXTENSIONS` 终版策展(由 manifest 生成,与实现严格一致);
- [ ] 发布文档:支持矩阵、已声明偏差清单(stipple 近似、LogicOp 受限、CI 模式、front buffer、ARB_imaging)、目标驱动测试记录(Mesa + 至少一款移动 GPU)。

## 退出标准

1. manifest 显示 GL 2.1 入口点零"未标注";unsupported 清单全部有文档与错误路径测试;
2. 10.2–10.4 各自金图/单测通过;
3. 全量回归(S1–S9 所有测试)在 Mesa llvmpipe + 一款真实移动 GPU 上绿;
4. 对外宣布:`GL_VERSION = "2.1 SimpleFPEWrapper x.y"`。

## 风险与决策点

- selection/feedback 的 CPU 变换要与 GPU 光栅结果在边界处一致性妥协 → 按规范只要求变换/裁剪级一致,写清测试容差。
- accum 在移动 GPU 上带宽昂贵 → 文档标注性能特征,不做隐藏优化。
- 本阶段大量"为标准完整性"的特性在 MC 场景零使用 → 排期上允许与实际用户需求驱动的优化并行,但**不允许砍范围**(用户目标:标准 2.1)。
