# STATUS

## 当前任务
M3-G 验证闭环（等待 G-01 任务卡）

## 已完成（按序）
- /STATUS.md 进度真相源建立 ◻
- /engine/platform/i_log.h 日志接口（write(level, tag, message)）◻
- /engine/platform/i_clock.h 墙钟/单调钟/sleep 接口 ◻
- /engine/platform/i_file_io.h 二进制安全文件接口 + FileStat ◻
- /engine/platform/i_http_transport.h 出站 HTTP 请求/响应结构 ◻
- /engine/platform/platform_services.h 服务聚合表 + 兜底实现声明 ◻
- /engine/platform/platform_services.cpp stderr 日志 / stdio 文件 / 不可用 HTTP 兜底 + 注入点 ◻
- /engine/core/math_types.h GLM 宏统一、Vec/Mat/Quat 别名、Transform、Aabb、signed_volume ◻
- /engine/core/log.h 等级过滤 + tag 前缀 + log_* 便捷函数 ◻
- /engine/core/log.cpp 过滤实现 ◻
- /engine/tools/tool_result.h ToolResult(ok/data/validator/hints) 与四种构造 ◻
- /CMakeLists.txt 顶层：C++17、开关 PM_BUILD_TESTS/PM_WITH_MANIFOLD/PM_WITH_RENDER/PM_WITH_MCP ◻
- /cmake/pm_deps.cmake glm/nlohmann_json/doctest 必装，manifold/httplib 按开关 ◻
- /engine/CMakeLists.txt pm_engine 静态库（GLOB + -I engine + GLM 双保险 + manifold 开关）◻
- /tests/CMakeLists.txt pm_tests 目标 + add_test ◻
- /tests/doctest_main.cpp 单测入口 + 日志降噪 ◻
- /tests/platform_services_test.cpp 平台层用例（兜底/注入/部分注入/二进制往返/错误路径/日志过滤）◻
- /engine/geom/indexed_mesh.h 属性数组模型 + MeshArrays/MeshQuality/WeldResult ◻
- /engine/geom/mesh_hash.h 跨编译单元共享：量化哈希/边键/退化判定/safe_normalize ◻
- /engine/geom/indexed_mesh.cpp 构建/变换/统计/合并/数组互转（362 行）◻
- /engine/geom/indexed_mesh_query.cpp 校验 validate + 焊接 weld（272 行）◻
- /tests/indexed_mesh_test.cpp 几何层用例 21 个 ◻
- 代码审计修复 8 处（详见 D-017）◻
- /engine/geom/generators.h 五图元参数结构 + 入口声明 ◻
- /engine/geom/generators.cpp 五个图元实现（box/sphere/cylinder 含圆锥圆台带盖/plane/torus，CCW 朝外，340 行）◻
- /tests/generators_test.cpp 图元用例 8 个：box×2/sphere/cylinder/cone/plane/torus/非法参数 ◻
- /engine/geom/manifold_bridge.h 布尔桥契约（枚举/BooleanResult/boolean 声明）◻
- /engine/geom/manifold_bridge.cpp OFF 桩 + ON 真实 manifold 双路径（输入流形校验 + 三角形摘要 + 法线重算）◻
- /tests/manifold_bridge_test.cpp 布尔桥用例 2 个（OFF 桩可读错误 + 输入兜底，ON 分支条件编译）◻
- /engine/tools/tool_registry.h 工具注册中心（合法名/schema/分发/describe）◻
- /engine/tools/tool_registry.cpp 五图元生成器工具 + model_boolean 占位（scene_not_available）+ 异常隔离 ◻
- /tests/tool_registry_test.cpp 注册中心用例 11 个（名字/分发/五图元/开放网格/required/describe）◻
- /engine/platform/i_event_sink.h 事件类型 + Event + IEventSink（引擎→壳层 upcall 通道）◻
- /engine/platform/i_render_surface.h 渲染表面抽象（handle/尺寸/valid + 空桩）◻
- /engine/platform/platform_services.h/.cpp 服务表扩 event_sink/render_surface + emit_event（走注入时钟补时间戳）◻
- /engine/platform/modeler_host.h/.cpp 生命周期状态机（start/stop/pause/resume/表面/触控/自动保存节流 10次/60s）◻
- /tests/modeler_host_test.cpp 宿主用例 6 个（幂等/暂停恢复/表面/触控/操作阈值/时间阈值）◻
- /tests/platform_services_test.cpp 增事件时间戳/静默丢弃/表面注入 3 用例（总数 9）◻
- /app-android 全套 M2a 壳层：settings/build.gradle.kts + gradle.properties + AndroidManifest + 资源 ◻
- /app-android/app/src/main/java/.../NativeBridge.kt 顶层 external fun + @file:JvmName（JNI 九函数匹配 jclass 静态方法）◻
- /app-android/.../ModelerService.kt 前台服务（DATA_SYNC + WAKE_LOCK + 通知节流 + 事件 upcall 注册）◻
- /app-android/.../MainActivity.kt / ViewportActivity.kt / GalleryActivity.kt 壳层三 Activity ◻
- /app-android/.../HttpTransportImpl.kt OkHttp 出站（TLS 归 Kotlin，C++ 不碰 SSL）【M5a-01 预留】◻
- /app-android/app/src/main/cpp/jni_bridge.cpp JNI 薄桥（9 函数 + OnLoad 缓存 VM + 跨线程 attach/detach upcall + ANativeWindow 生命周期管理）◻
- /app-android/app/src/main/cpp/CMakeLists.txt 复用 pm_deps + GLOB 引擎源（PM_WITH_* OFF）◻
- /engine/mcp/rpc_types.h/.cpp JSON-RPC 2.0 编解码 + 错误码 + ToolResult→MCP 映射 ◻
- /tests/rpc_types_test.cpp 协议用例 5 个（请求/通知/解析/信封/错误码/映射）◻
- /engine/mcp/auth.h/.cpp Token 常量时间比较 + Bearer 提取 + 会话超时 + instructions ◻
- /tests/auth_test.cpp 鉴权用例 5 个（Bearer/常量时间/缺失/错误/超时/刷新）◻
- /engine/mcp/dispatcher.h/.cpp 分发器（initialize/ping/tools list/call/未知方法/通知）◻
- /tests/dispatcher_test.cpp 分发用例 7 个 ◻
- /engine/mcp/http_server.h/.cpp cpp-httplib 包装（pimpl 隔离 + /mcp POST + Bearer 401 + parse error + 413 上限；OFF 走桩）◻
- /engine/render/viewport.h/.cpp 视口契约：双相机（用户/截图隔离预留）+ 轨道/平移/缩放 + surface 生命周期 + 渲染状态机（OFF 可编译可单测）◻
- /tests/viewport_test.cpp 视口用例 6 个（状态机/表面/轨道/缩放/平移/投影随宽高比）◻
- /engine/render/render_thread.h/.cpp 渲染线程宿主（独立线程 + host 状态联动 + surface 无效 sleep(200ms) 轮询 + 停止退出）◻
- /tests/render_thread_test.cpp 渲染线程用例 4 个（stopped 退出/running 跑帧/paused sleep/start 幂等）◻
- /engine/io/project_store.h/.cpp 项目存储（创建/打开/列表/触摸 + project.json 元数据 + 命名校验）◻
- /tests/project_store_test.cpp 项目存储用例 4 个（创建打开/重名非法名/列表排序/损坏 JSON）◻
- /engine/render/thumbnail.h/.cpp 画廊缩略图（参数校验 + overwrite 语义 + OFF 桩 / ON 占位 PNG）◻
- /tests/thumbnail_test.cpp 缩略图用例 3 个（非法参数/桩错误/覆盖保护）◻
- /app-android GalleryActivity.kt 真实画廊（标题+修改时间+缩略图灰块占位）◻
- /engine/render/playback_controller.h/.cpp 回放控制（录指令时间轴/回放推进/暂停恢复不含暂停时长）◻
- /tests/playback_controller_test.cpp 回放用例 6 个（录制时间戳/未录制拒绝/时间轴触发/暂停排除/状态迁移）◻
- /engine/render/capture_views.h/.cpp 四视图拼图（正交 front/top/right + 透视 45°俯角，AABB 自适应 + 10% padding，2×2 JPEG 契约；OFF 桩）◻
- /tests/capture_views_test.cpp 四视图用例 4 个（单位盒相机/非法 bbox/尺寸缩放/矩阵有效）◻
- /engine/scene/validator.h/.cpp 几何校验器（穿模 AABB 粗筛 + 浮空支撑判定 + 比例中位数；O(n²) 上限 >200 增量模式）◻
- /tests/validator_test.cpp 校验器用例 6 个（穿模 0.3/接触 0.02 不报/浮空 0.5/同 group 豁免/比例 12 倍/干净场景）◻
- /engine/scene/assert_registry.h/.cpp 会话内 assert 登记表（5 类型白名单 + 参数数量校验 + 同类型覆盖）◻
- /tests/assert_registry_test.cpp assert 登记用例 4 个（注册/未知类型/参数数量/覆盖/清理）◻
- /engine/tools/tool_registry.h/.cpp 增 model_assert 工具（register_assert_tools 集成钩子）◻
- /engine/tools/assembly_tools.h/.cpp 组装工具（attach 面贴合 + snap_fit stack/side + group + mirror_complete 对称）◻
- /tests/assembly_tools_test.cpp 组装用例 8 个（屋顶贴墙/柱贴墙/offset/非法面/stack/side/group/mirror 对称）◻
- /engine/tools/meta_tools.h/.cpp 元工具（batch ≤20 不嵌套 + plan 浅校验 + snapshot 上限 8 FIFO）◻
- /tests/meta_tools_test.cpp 元工具用例 6 个（batch 成功/继续执行/截断+嵌套拒/plan 引用/snapshot 往返+上限/倒序）◻

## 进行中
- M3-G 验证闭环：G-01 停手清场 → G-02 仓库初始化 → G-03 首次推送 → G-04 CI 工作流 → G-05 修错循环至引擎全绿

## 下一步
1. G-01 停手清场
2. G-02 仓库初始化
3. G-03 首次推送

## 决策记录
D-001：工作区根目录即项目根，/engine 与 /app-android 直接位于其下。
D-002：C++ 命名空间根为 pm，子命名空间与目录同名：pm::core / pm::geom / pm::scene / pm::render / pm::tools / pm::mcp / pm::assets / pm::io / pm::platform。
D-003：/engine 内部 include 一律以 engine/ 为根（写 "geom/indexed_mesh.h"），编译时加 -I<repo>/engine；头文件自给自足。
D-004：平台能力经 PlatformServices 聚合注入（log / file_io / clock / http 四个指针），核心库不持有业务全局单例，服务表可整体替换以便测试注入。
D-005：单测集中在 /tests，文件名 <module>_test.cpp，顶层 CMakeLists 统一 add_subdirectory(tests)，doctest 入口为 tests/doctest_main.cpp。
D-006：重型/未落地依赖用 CMake 开关隔离：PM_WITH_MANIFOLD、PM_WITH_RENDER、PM_WITH_MCP 默认 OFF，避免未实现模块阻塞编译；依赖缺失一律走 find_package 优先、FetchContent 兜底。
D-007：IndexedMesh 采用属性数组模型：positions 必填；normals / uvs 要么与 positions 等长，要么为空表示缺失；indices 为 uint32 三角形列表（size % 3 == 0）。
D-008：坐标系右手 Y-up、单位米；深度 0..1 由 core/math_types.h 统一强定义 GLM_FORCE_RADIANS + GLM_FORCE_DEPTH_ZERO_TO_ONE，与 bgfx 视口一致。
D-009：三角形绕序逆时针（CCW）朝外为唯一合法朝向；有符号体积为负表示整体绕序反了，不自动修正，由工具层显式调用 flip_winding。
D-010：engine 源文件不逐个列进 CMake，用 GLOB_RECURSE + CONFIGURE_DEPENDS，避免每加文件改构建脚本。
D-011：工具边界错误一律经 ToolResult（ok / data / validator / hints）返回，validator 承载机器可读失败原因，异常禁止跨工具边界。
D-012：几何层校验结果用独立的 MeshQuality 结构，不直接返回 ToolResult，保持依赖方向 core→geom→scene→tools→mcp，工具层负责 MeshQuality→ToolResult 的映射。
D-013：变换含镜像（det<0）时几何层自动翻转载序并在结果中标记，因为镜像必然反转手性；此规则不属于 D-009 所禁止的"绕序猜测修正"。
D-014：顶点焊接采用量化空间哈希（key 由坐标按 epsilon 量化取整），epsilon 默认 1e-5 m；焊接后丢弃退化三角形，不保留孤立顶点。
D-015：MeshQuality.code 为稳定字符串枚举：致命（empty_mesh / index_out_of_range / index_count_not_multiple_of_three / non_finite_position / normals_length_mismatch / uvs_length_mismatch / triangle_slots_length_mismatch），提示（degenerate_triangles / degenerate_heavy / unreferenced_vertices / duplicate_vertices）。
D-016：T-002 实际产出 4 个实现/头文件（indexed_mesh.h 含 MeshArrays/MeshQuality/WeldResult，mesh_hash.h 共享 detail 工具，indexed_mesh.cpp 构建侧，indexed_mesh_query.cpp 校验/焊接），超出任务卡"一卡 ≤2 文件"纪律；后续任务卡严格按 ≤2 文件切分，必要拆分以"聚合头 + 子实现"为唯一合法形态。
D-017：代码审计实证修复 8 处——(1) weld 对越界索引直接读 old_to_new 造成越界读，改为哨兵 kUnmapped + 丢弃坏三角形；(2) weld 槽位越界读，with_slots 额外要求 slots.size()==triangle_count()；(3) weld 保留孤立顶点违反 D-014，增加压缩映射移除孤立顶点并同步法线/UV/索引；(4) split_vertices_per_face 单点越界导致索引长度非 3 的倍数，改为整三角形丢弃；(5) transform 退化线性部分求逆得 inf/NaN，改为不可逆时保持原法线；(6) GLM_FORCE_* 在 CMake 与 math_types.h 双定义（替换序列不同会产生重定义告警），math_types.h 加 #ifndef guard；(7) append 槽位重复登记（ensure_triangle_slots 会把 other 的三角形也补 0 再 push 槽位），改为只补本侧历史三角形；(8) face_normal/face_centroid 越界索引数组读，增加范围检查。
D-018（v2.1 补丁登记）：M3 规划目录 /engine/scene/validator、/engine/tools/assembly_tools.cpp、/engine/tools/meta_tools.cpp、/engine/render/capture_views.cpp 已存档，随里程碑实际创建，当前不落盘。
D-019（v2.1 铁律第 8 条）：校验器为纯几何计算，零网络零截图；所有写场景工具返回值必须带 validator 字段（可全局开关 auto_validate）；穿模判定 AABB 最小轴交叠 >0.05（可配）报 intersect，同 group 内互不报（组合体重叠合法）；浮空判定 minY>0.05 且 XZ 投影下方无支撑 AABB 报 floating；参考物（1m 网格 + 1.7m 人形剪影）仅截图管线临时注入，不进 SceneGraph、不序列化、不参与校验与 stats。
D-020：capture_views 固定 2×2 拼图单 JPEG（正交 front/top/right + 透视 persp），子图左上带标签（bgfx dbg text，禁字体库），JPEG 质量 85，文件 ≤300KB；四视图取景 = 场景 AABB 自适应 + 10% padding，与用户相机无关。
D-021（=补丁 D-07）：attach 语义为平移 + 绕世界 Y 轴 yaw 对齐，默认不翻转子物体姿态（柱子靠墙保持竖直、挂画贴墙）；需姿态翻转的场景由调用方显式 transform。
D-022（=补丁 D-08）：batch 单次上限 20 个操作，单项失败记录错误并继续执行，batch 不嵌套 batch（防递归炸弹）。
D-023（=补丁 D-09）：参考物只在截图管线注入，渲完即弃，不进 SceneGraph、不序列化、不参与校验与 stats。
D-024：v2.1 补丁中"M1 ✅ M2(a-e) ✅ 当前 M3"与磁盘实况不符（M1 未收尾、M2 全部缺失），判定为模板预填而非实况；M3a-01/02、M3b-01/02、M3c-01/02、M3d-01/02 任务卡已接收存档（含 D-05 capture_views 2×2、D-06 0.05 阈值豁免等细节），待前置里程碑落地后按序启用。
D-025（T-003 产出登记）：T-003 实际产出 3 文件（generators.h / generators.cpp / generators_test.cpp），含 5 图元 + 8 用例；实现过程中对 box/sphere/plane/torus/cylinder 全部绕序做了逐面代数验证（D-009 CCW 朝外、封闭体 volume>0、watertight），并修正 box segments 字段未实现即删除、push_side_wall 重复 push 顶点破坏水密、圆锥冗余顶环、顶/底盖绕序等 4 处生成期实证缺陷。
D-026（v2.2 结构增补登记）：/engine/geom/deform_ops、/engine/geom/sweeps、/engine/geom/generators/（树/石/房/栅栏/家具/角色）、/engine/tools/deform_tools.cpp、/engine/tools/array_tools.cpp、/engine/tools/template_tools.cpp 已存档，随 M4 里程碑实际创建，当前不落盘。
D-027（=补丁 D-10/D-11）：一切随机操作种子化（seed 默认 42，同 seed 逐顶点可复现）；变形/生成后法线全量重算（面积加权顶点法线）。
D-028（=补丁 D-12~D-14）：lathe 轮廓 x=半径绕 Y 轴，开放端部可选自动加盖；loft 要求各截面顶点数一致否则可读错误；sweep 用平行传输标架，profile 顶点逆时针为外法线。
D-029（=补丁 D-15/D-16）：solidify=法向偏移（薄面复制+翻转+桥接为板，封闭体气球化），真抽壳不做；模板生成器纯参数化零网络，template_apply 一次调用产出组合体并自动 group。
D-030（=补丁 D-17/D-18）：repeat/scatter 产物自动成组（组名=源物体名+"_array"）；本版门窗为贴面件不布尔挖洞（M6 后再议挖洞选项）。
D-031：性能保护（v2.2 铁律 9）：subdivide levels ≤3、scatter count ≤500、单工具三角形增量 >20万 时返回可读错误而非卡死；M4a-01~M4e-03 任务卡已存档（含各自验收数学断言），待前置里程碑落地后按序启用。
D-032：M4 任务卡中"M1 ✅ M2 ✅ M3 ✅ 当前 M4"与磁盘实况不符（M1 未收尾 ToolRegistry 未落地、M2/M3 全部缺失），沿用 D-024 判定为模板预填；当前真实任务仍为 T-004 manifold_bridge（布尔桥），完成并过测后才进入 M1 收尾。
D-033（T-004 产出登记）：manifold_bridge 三文件落盘（头 31 行 / cpp 121 行 / 测试 85 行）。OFF 走纯桩返回 "manifold_support_disabled"（D-006，不崩不伪装成功）；ON 路径要求输入通过完整流形校验（ok + 无水密/边界/非流形/退化，否则 "invalid_input_mesh"），经 OfMesh 构建 → 布尔 → AsOriginal 取回 → compute_normals（D-11）→ 输出三角形摘要；单测 2 用例（桩 + 输入兜底）默认构建常跑，ON 分支条件编译含 union≈1.5 / difference=0.5 体积断言。字段名 vertPos/triVerts 标注【假设】按 manifold v3.0.1，若版本改名仅两处随版本调整。
D-034（ToolRegistry 产出登记）：tool_registry 三文件落盘（头 53 行 / cpp 310 行 / 测试 149 行）。工具名合法性手动逐字符校验 ^[a-zA-Z0-9_-]{1,64}$（无正则依赖）；参数校验 required + properties.type 轻量匹配、未知字段宽容；call 内 try/catch 隔离异常（D-011，异常禁止跨工具边界）；内置 6 工具全带 model_ 前缀（box/sphere/cylinder/plane/torus/boolean），生成器返回 vertex/triangle/bounds/volume/watertight 统计；generator_result 只拒致命错误不强制 watertight——开放网格（plane、cap=false 管道）是合法产出；model_boolean 因 M1 无 SceneGraph 如实返回 scene_not_available（不伪造成功）。
D-035（v2.3 固定选型登记）：tinygltf（含 stb_image，GLB 只读导入）与 meshoptimizer（减面/简化）加入固定选型，禁止更换；M5 阶段才引入，当前不拉取依赖。
D-036（v2.3 资产网络规则，=铁律 10）：一切出站 HTTP 仅经 IHttpTransport（C++ 不碰 SSL）；域白名单固定 api.polyhaven.com / dl.polyhaven.org / kenney.nl / quaternius.com 及其 CDN 子域；单文件上限 200MB；超时 15s 重试 3 次指数退避；下载线程独立于 Worker（不持场景锁）；缓存 app://assets/，同 asset_id 二次导入零网络；贴图走 L3 位图材质（D-25 同源）。
D-037（=补丁 D-19/D-20）：减面后端 = meshoptimizer simplify，失败回退原网格；Kenney/Quaternius 无官方 API，走内置精选索引 curated_index.json（随 APK 打包，URL 为直链，idx_version 备热更）。
D-038（=补丁 D-21/D-22）：缓存 app://assets/，同 asset_id 去重零网络，贴图存 app://assets/textures/ 供导出复用；尺度适配：索引声明 units（米制直用，其他按声明换算），未声明默认米并在 hints 提示。
D-039（=补丁 D-23/D-24）：preserve_edges 用顶点锁定（边界/硬边顶点简化不动）实现；auto_fix 固定顺序 法线→流形→浮空→穿插→比例，单项失败不阻断。
D-040（=补丁 D-25/D-26）：导入资产材质转三级系统：有贴图→L3（贴图+粗糙金属参数），纯色→L1；mat 工具可覆盖；PolyHaven 贴图默认取 1K/2K 变体（省流量，4K 需显式参数）。
D-041：M5 任务卡中"M1 ✅ M2 ✅ M3 ✅ M4 ✅ 当前 M5"与磁盘实况不符（M1 待编译验证闭环、M2~M4 全部缺失），沿用 D-024/D-032 判定为模板预填；M5a-01~04、M5b-01~03 任务卡已存档，待 M1 闭环与 M2~M4 落地后按序启用。
D-042：assets/ 目录与 io/glb_import、geom/simplify 随 M5 里程碑实际创建，当前不落盘（与 D-018/D-026 同款登记）。
D-043（最高优先级撤回编译）：用户指示"请不要使用本地编译（触发本地编译被截断，最高优先级撤回编译）"，立即停止所有 cmake/g++/make 本地构建；build/ 残留已删除。M1 验证闭环改走静态审查路径（命名空间限定/头自给自足/TODO 残留已查净），真实编译交由 GitHub Actions CI（上游已建 .github/workflows，用仓库 GITHUB_TOKEN 跑 pm_tests）。
D-044（M2b 产出登记）：/engine/mcp 四模块落盘——rpc_types（JSON-RPC 2.0 编解码/错误码/ToolResult→MCP）、auth（常量时间 token 比较 + Bearer 提取 + 会话超时 + instructions）、dispatcher（initialize/ping/tools list/call/通知）、http_server（cpp-httplib pimpl 包装 /mcp POST + 401 + parse error + 413 上限，PM_WITH_MCP OFF 走桩）。测试 5+5+7 用例。
D-045（M2c 产出登记）：/engine/render 两模块落盘——viewport（用户/截图相机隔离预留、轨道/平移/缩放、surface 生命周期、渲染状态机：paused 或 surface 无效不渲染帧）、render_thread（独立线程、host 状态联动、idle sleep 200ms、停止退出）。测试 6+4 用例；bgfx 真实绘制仅 PM_WITH_RENDER 分支，OFF 构建可单测全部状态与相机数学。
D-046：D-043 作废（用户裁定）。验证策略的制定权只属于用户；模型无权撤回、降级、推迟任何验证环节。静态审查只是风格检查，不构成正确性证据；未执行的设计声明必须标注【未验证】，禁止写成既成事实。
D-047：STATUS.md 双状态制：✅ = CI 真实编译且测试通过；◻ = 代码完成未验证。未验证项禁止使用 ✅。
D-048：本地构建线永久关闭（用户裁定）：不得安装任何编译器 / 工具链，不得恢复本地编译；验证唯一出口 = GitHub Actions。
D-049：凭据安全协议（宪法级铁律）：/storage/emulated/0/ADM/.git-credentials 是 git 凭据文件，禁止以任何方式读取其内容（cat/head/tail/grep/less/more/od/xxd/cp 等全部禁止），禁止将其内容写入任何文件/代码/配置/输出/提交/CI secrets，禁止把 token 内嵌进任何 URL（remote URL 必须是干净的 https://github.com/... 形式）；允许且仅允许的操作 = 把该路径作为 credential helper 的配置值写入 git config（写路径字符串）；自检义务 = 输出中若出现 ghp_ 或 github_pat_ 前缀立即停止并告知用户"疑似凭据泄露，请撤销 token"，禁止复述该字符串。原因：云端 API 读到的一切都会离开设备进入会话历史，读取即泄露。
D-050：推送协议：每完成一张任务卡（或一个修复轮）即 commit + push 一次；commit message 格式 = "卡号: 一行摘要"；push 失败重试至多 2 次，仍失败则原样粘贴错误等待用户处理；禁止为通过认证而读取凭据文件。
D-051：仓库公开 + MIT：公开仓库标准运行器免费、无额度限制；CI 永远只用 ubuntu-latest（macOS 运行器对公开仓库收费，本项目不需要）。
D-052：仓库门面克制：不创建 README.md，不填仓库 description / topics 等任何介绍性内容；项目介绍在项目彻底完成后由用户发起专门任务卡撰写。唯一例外：LICENSE 现在就建立（标准 MIT 全文，版权行 "Copyright (c) 2026 <持有者>"，未获用户指定时用 Pocket Modeler Contributors 并标注【假设】）。
D-053：版本铁律——四段式 0.0.0.N，前三段冻结 0.0.0，N 是不进位整数计数器（0.0.0.9 之后是 0.0.0.10）；当前版本 = 0.0.0.1；版本变更权只属于用户（里程碑 CI 转绿时可建议升版，执行须用户明确指令）；首个正式版必须用户手动确认；版本唯一来源三处一致 = CMake project(VERSION 0.0.0.1) / Android versionName "0.0.0.1" + versionCode 1 / MCP serverInfo.version，三处不一致视为 bug。
D-054：安卓产物路线：引擎 CI 全绿后新增 M3-A 阶段，Actions 自动构建 debug APK 并发布到 GitHub Releases；0.0.0.x 阶段 APK 仅用于"看样子"（壳层 UI / 服务 / 生命周期行为），不承诺工程精度；不上架任何应用商店。
D-055：CI 反馈环契约：用户输入只有两种合法形式——粘贴 Actions 日志报错段，或告知"跑完了"；两种输入均合法，禁止抱怨输入方式；收到日志先自查 token 前缀（D-049）再分析。
D-056（G-01 迁移）：源码根目录自沙箱迁移至持久目录 /storage/emulated/0/PocketModeler/（用户确认）。复制 92 文件，计数双向一致 + 抽样 diff 全同；沙箱副本保留至首次 push 后清理。G-02 起仓库根 = /storage/emulated/0/PocketModeler。
D-057（G-02 版本对齐）：CMake project 增 VERSION 0.0.0.1；Android versionName 自 "0.1.0" 改 "0.0.0.1"（versionCode 1 保持）；MCP serverInfo.version 未接线，登记待办（随 M2b 后续卡或 M3-G 后实现），本卡不实现（D-053e）。
