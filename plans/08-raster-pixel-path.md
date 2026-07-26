# S8 光栅与像素路径

**目标**:实现 GL 2.1 的 CPU 侧光栅状态与像素传输路径:RasterPos/WindowPos、glBitmap、glDrawPixels/glCopyPixels/glReadPixels 桌面语义、PixelZoom、pixel transfer/map、stipple、PolygonMode 线框、ClipPlane、LogicOp。GLES3 缺失的功能以"有文档的模拟/偏差"落地。

**前置**:S5(像素重打包管线、格式映射)、S7(per-context,像素路径引入大量新状态)。
**规模**:大(4–6 周)。

## 现状

- `glRasterPos*` 24 个变体、`glWindowPos*`、`glBitmap`、`glDrawPixels`、`glCopyPixels`、`glPixelZoom`、`glPixelTransfer*`、`glPixelMap*`、`glLineStipple`、`glPolygonStipple` 全部未实现(多数连声明都没有);`glReadPixels` 仅做过 BGRA 修正。
- `glPolygonMode`/`glClipPlane`/`glLogicOp` 状态在 S3 已存,效果未实现。
- GLES 3.0 硬缺失:线框/点模式、stipple、logic op、CLAMP 顶点外裁剪面、bitmap/DrawPixels。

## 任务

### 8.1 Raster 位置
- [ ] `glRasterPos{2,3,4}{s,i,f,d}[v]`:完整变换管线(MVP → 裁剪 → viewport),更新 raster 状态:窗口坐标、valid 位(视锥外 invalid)、关联 color(当前 color 或光照结果)、texcoords、fog coord。
- [ ] `glWindowPos{2,3}{s,i,f,d}[v]`(GL 1.4):直写窗口坐标,恒 valid。
- [ ] 查询:`GL_CURRENT_RASTER_POSITION/COLOR/TEXTURE_COORDS/POSITION_VALID` 接入 getter 分发表。

### 8.2 Bitmap 与 DrawPixels
- [ ] `glBitmap`:位图解包(受 UNPACK 状态含 LSB_FIRST 影响)→ R8 纹理 → 以 raster pos 为锚的屏幕对齐 quad,alpha test 式贴片(0 像素透明);raster pos 按 xmove/ymove 前进;w/h=0 仅移动路径;受 color mask/blend/depth 现行状态影响的语义核对。
- [ ] `glDrawPixels`:`GL_RGBA/GL_RGB/GL_LUMINANCE*/GL_ALPHA` × 常用 type → S5 重打包管线 → 纹理 → raster pos 定位的 quad(尺寸受 `glPixelZoom` 缩放,负 zoom 翻转);`GL_DEPTH_COMPONENT` 用 gl_FragDepth 写入;`GL_STENCIL_INDEX` 标 unsupported(GLES 无路径,manifest 记录)。
- [ ] pixel transfer(scale/bias)与 `glPixelMap*`(I2R 等查表):在 CPU 重打包阶段应用;`GL_PIXEL_MAP_*` 查询。
- [ ] `glCopyPixels`:`GL_COLOR` 用 FBO blit(zoom≠1 时经纹理 quad);DEPTH 经 depth 纹理采样写 gl_FragDepth;STENCIL 标 unsupported。
- [ ] `glReadPixels` 桌面语义:BGRA、packed type、PACK_SWAP_BYTES 等经重打包管线;`GL_DEPTH_COMPONENT`/`GL_STENCIL_INDEX` 读取按 ES3 能力落地或标注。

### 8.3 光栅化状态模拟
- [ ] `glPolygonMode(GL_LINE)`:FPE 路径在索引重写层把三角形/quad 展开为线段索引(front/back 分别设置时按面拆分需要 CPU 判面——初版支持 FRONT_AND_BACK 一致的常用情形,分面情形 manifest 标偏差);`GL_POINT` 同理展开为点。边旗标(`glEdgeFlag`,S3 已存)参与内部边剔除。
- [ ] `glLineStipple`:screen-space 距离累计在 ES3.0 无几何着色器下不可精确 → 采用"按片元屏幕坐标近似"方案(gl_FragCoord 沿主轴取模)+ manifest 声明偏差;factor/pattern 状态与查询完整。
- [ ] `glPolygonStipple`:32×32 pattern → R8 纹理,fragment 按 gl_FragCoord 采样 discard;pattern 受 UNPACK 状态影响;`glGetPolygonStipple` 查询。
- [ ] `glClipPlane` 消费:有 `EXT_clip_cull_distance` 用 gl_ClipDistance;否则 FPE shader 内逐平面 discard(顶点插值 distance varying);最多 6 平面;`GL_CLIP_PLANEi` enable 进 program hash。
- [ ] `glLogicOp`:`GL_COPY` no-op、`GL_XOR` 等在有 `EXT_shader_framebuffer_fetch` 时 shader 模拟,无扩展时常用可混合近似(`GL_AND` 黑、`GL_OR` 白等不可精确的)标 unsupported;manifest 逐 op 记录。
- [ ] 点大小收尾:`glPointSize` 消费(S3 已存)、`GL_POINT_SMOOTH`/`GL_LINE_SMOOTH` 状态接受(效果 no-op 合法,MSAA 场景注明)、`GL_ALIASED/SMOOTH_POINT_SIZE_RANGE` 查询。

### 8.4 共享基建
- [ ] "屏幕对齐贴片绘制器"(bitmap/drawpixels/copypixels 共用):自带极简 shader、不污染 FPE program 缓存、保存恢复全部触碰状态(走 S2 guard 扩展)。
- [ ] CPU 像素重打包管线固化为独立模块(S5 已起步):format×type×pack 状态 → 规范化 RGBA8/float,双向;单测矩阵覆盖。

## 退出标准

1. 金图:RasterPos+Bitmap 文本叠加、DrawPixels 各格式、PixelZoom(含负值翻转)、CopyPixels、polygon stipple、line stipple(近似基线)、PolygonMode 线框、ClipPlane 裁剪场景;
2. raster 状态全量查询单测;
3. 偏差清单(stipple 近似、LogicOp 受限、STENCIL 像素路径 unsupported、分面 PolygonMode)全部进 manifest 与文档;
4. 老 MC 冒烟不回退(此路径 MC 使用少,重点是不引入新崩溃)。

## 风险与决策点

- 本阶段"精确 vs 文档化近似"的取舍最多 → 原则:**状态与查询必须精确,光栅效果允许文档化近似**;任何近似必须可被测试基线锁定(近似本身也要回归)。
- PolygonMode CPU 展开与显示列表合批交互 → 非 FILL 模式一律作为合批屏障(S6 表驱动里加一行)。
