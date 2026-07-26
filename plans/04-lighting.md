# S4 光照与材质完成

**目标**:把光照从"顶点级 ambient+diffuse 方向光"推进到 GL 2.1 完整固定管线光照:位置光、衰减、聚光、specular、GL_FLAT、双面细化、color material 全模式、查询 API。

**前置**:S2(错误状态机)、S3(getter 分发表)。
**规模**:中(2–3 周)。

## 现状(审计证据)

- 已生成并可渲染:顶点级 ambient+diffuse+emission、双面光照、`GL_COLOR_MATERIAL`(`fpe_shadergen.cpp:1431-1483`),uniform 增量上传完整(`glstate.cpp:89-126`);`GL_NORMALIZE`/`GL_RESCALE_NORMAL` 已处理(`fpe_shadergen.cpp:1400-1429`);`GL_POSITION` 的 call-time ModelView 变换已实现(`state.cpp:375-383`)。
- 死数据(存储完整、渲染零消费):specular/shininess、constant/linear/quadratic 衰减、spot cutoff/exponent/direction、`GL_LIGHT_MODEL_LOCAL_VIEWER`、`GL_LIGHT_MODEL_COLOR_CONTROL`。
- 位置光被当方向光:`fpe_shadergen.cpp:1437-1442` 忽略 position 的 w 分量。
- `GL_SPOT_DIRECTION` 未做 call-time ModelView 变换(`state.cpp:384-387`),与 GL_POSITION 行为不一致(规范要求两者都在调用时变换)。
- `glShadeModel(GL_FLAT)` 完全不生效:`shade_model` 参与 program hash 却从不被生成器读取(grep 证实)。
- 查询 API(`glIsEnabled(GL_LIGHTING)`、`glGetLightfv`、`glGetMaterialfv`)缺失(S3 已建分发表,本阶段接数据)。
- 防护已有:light index > GL_LIGHT7、texunit 越界有保护(`state.cpp:29-38`);`glLightModelfv|iv` 空指针检查在 S1 已补。

## 任务

### 4.1 光源模型补全(shader 生成 + uniform)
- [ ] 位置光:按 `position.w != 0` 区分方向光/位置光;位置光用 eye-space 光向量 + 距离衰减 `1/(kc + kl·d + kq·d²)`。
- [ ] specular:Blinn 半角向量(`GL_LIGHT_MODEL_LOCAL_VIEWER` 控制视点方向取法),`shininess` 幂;材质 `GL_SPECULAR` × 光源 `GL_SPECULAR`。
- [ ] `GL_LIGHT_MODEL_COLOR_CONTROL`:`GL_SEPARATE_SPECULAR_COLOR` 时 specular 走 secondary color、在纹理应用之后相加(与 S3 的 `glSecondaryColor` 状态共用 varying;`GL_COLOR_SUM` enable 一并接通)。
- [ ] 聚光:cutoff(180 特判)、exponent、`GL_SPOT_DIRECTION` 补 call-time ModelView 逆转置变换(修 `state.cpp:384-387` 不一致)。
- [ ] scene ambient(`GL_LIGHT_MODEL_AMBIENT`)与 per-light ambient 汇总核对(现有实现已有,回归即可)。
- [ ] 生成策略:逐灯 enable 参与 program hash(现状如此)→ 评估改为 uniform 数组 + 灯数 uniform 以减少 shader 变体爆炸;**决策点:先保持 hash 变体,观察 S11 基准的编译抖动再定。**

### 4.2 着色模型与插值
- [ ] `GL_FLAT`:颜色(primary+secondary)varying 加 `flat` 限定符(ESSL 3.00 原生支持);其余 varying 保持 smooth。
- [ ] provoking vertex 语义:GL 2.1 flat 取 primitive **最后一个**顶点;ES 3.0 flat 取 provoking vertex 默认也是最后顶点,但 **QUADS→TRIANGLES 分解会改变"最后顶点"归属** —— 分解时需重排索引使每个三角形的 provoking vertex 落在原 quad 的第 4 顶点上;`GL_POLYGON`/strip/fan 同样逐一核对。
- [ ] 双面光照回归:`gl_FrontFacing` 反面材质路径与 flat 组合测试。

### 4.3 材质与 color material
- [ ] `glColorMaterial` 全模式核对:`GL_EMISSION/GL_AMBIENT/GL_DIFFUSE/GL_SPECULAR/GL_AMBIENT_AND_DIFFUSE` × `GL_FRONT/GL_BACK/GL_FRONT_AND_BACK`(现实现于 `state.cpp:518` 并被 `mglColor` 消费,逐模式补测试)。
- [ ] `glMaterial*` 在 `glBegin/glEnd` 内合法(GL 特例)→ 确认即时模式路径允许并正确 flush 批次。
- [ ] 显示列表内的材质/光照命令录制回放等价测试(与 S6 协同)。

### 4.4 查询
- [ ] `glGetLightfv|iv`(位置/方向返回**存储时已变换**的 eye-space 值,按规范)、`glGetMaterialfv|iv`、`glIsEnabled` 光照相关全枚举、`glGetFloatv(GL_CURRENT_NORMAL/GL_LIGHT_MODEL_AMBIENT…)` 接入 S3 分发表。

## 退出标准

1. 参考场景金图(S11):方向光、位置光+衰减、聚光、specular(含 SEPARATE_SPECULAR)、color material、GL_FLAT vs GL_SMOOTH、双面、关灯——每场景一图,对 Mesa llvmpipe 基线;
2. 位置光/聚光方向的 call-time 变换语义各有一个状态时序测试(先设矩阵后设光 vs 反序);
3. 光照全参数经 `glGetLightfv/glGetMaterialfv` 读回一致;
4. shader 变体数与编译耗时记录进基准(防止 hash 维度爆炸无感知)。

## 风险与决策点

- 顶点光照是 GL 2.1 规范行为(片元光照是增强)→ 本阶段只做顶点级,保持与规范一致;可选的 per-pixel 开关留待 2.1 达成后。
- flat + QUADS 分解的 provoking vertex 重排会触碰 S2 的索引重写缓存 → 缓存 key 需含 shade_model。
