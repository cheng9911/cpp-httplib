# Robot Plugin Framework (RPF) 测试需求文档

## 文档信息

| 项目 | 内容 |
|------|------|
| 项目名称 | Robot Plugin Framework (RPF) |
| 版本 | 1.0.0 |
| 文档版本 | 1.0 |
| 编写日期 | 2026-05-28 |

---

## 一、基础功能测试

### 1.1 元数据加载和解析测试（PluginMetadata）

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| M-001 | 元数据 | JSON 序列化 - 完整字段 | 1. 构造包含所有字段的 PluginMetadata 对象<br>2. 调用 `nlohmann::json j = meta` 序列化<br>3. 验证 JSON 各字段值 | JSON 包含 name, version, description, author, license, dependencies, provides, category, platform, entry_point，值与对象一致 | P0 |
| M-002 | 元数据 | JSON 反序列化 - 完整字段 | 1. 构造完整 JSON 对象<br>2. 调用 `j.get<PluginMetadata>()` 反序列化<br>3. 验证各字段值 | 各字段值与 JSON 一致 | P0 |
| M-003 | 元数据 | JSON 反序列化 - 仅必填字段 | 1. 构造仅含 name 和 version 的 JSON<br>2. 反序列化为 PluginMetadata | name 和 version 正确，其余字段为默认值（空字符串、空数组等） | P1 |
| M-004 | 元数据 | JSON 序列化 - 缺少必填字段 name | 1. 构造不含 name 字段的 JSON<br>2. 调用 `j.get<PluginMetadata>()` | 抛出异常（nlohmann::json::out_of_range 或类似异常） | P1 |
| M-005 | 元数据 | JSON 序列化 - 缺少必填字段 version | 1. 构造不含 version 字段的 JSON<br>2. 调用 `j.get<PluginMetadata>()` | 抛出异常 | P1 |
| M-006 | 元数据 | JSON 序列化 - 字段类型错误 | 1. 构造 name 字段为整数类型的 JSON<br>2. 调用 `j.get<PluginMetadata>()` | 抛出类型转换异常 | P2 |
| M-007 | 元数据 | JSON 序列化 - 空 JSON 对象 | 1. 构造空 JSON `{}`<br>2. 调用 `j.get<PluginMetadata>()` | 抛出异常（name 字段缺失） | P1 |
| M-008 | 元数据 | Dependency 序列化/反序列化 | 1. 构造 Dependency 对象（name="dep1", version_constraint=">=1.0.0", required=true）<br>2. 序列化为 JSON<br>3. 反序列化回 Dependency | 字段完全一致 | P0 |
| M-009 | 元数据 | Dependency 反序列化 - 可选字段缺失 | 1. 构造仅含 name 的 Dependency JSON<br>2. 反序列化 | name 正确，version_constraint 为空字符串，required 为 true（默认值） | P1 |
| M-010 | 元数据 | 从文件加载 - 正常文件 | 1. 写入合法 .meta.json 文件到 /tmp<br>2. 调用 `loadMetadataFromFile(path)`<br>3. 验证返回的元数据 | 返回正确的 PluginMetadata，各字段与文件内容一致 | P0 |
| M-011 | 元数据 | 从文件加载 - 文件不存在 | 1. 调用 `loadMetadataFromFile("/nonexistent/path.json")` | 抛出 std::runtime_error，消息包含 "Cannot open metadata file" | P0 |
| M-012 | 元数据 | 从文件加载 - JSON 格式错误 | 1. 写入非法 JSON（如 `{invalid}`）到文件<br>2. 调用 `loadMetadataFromFile(path)` | 抛出 JSON 解析异常 | P1 |
| M-013 | 元数据 | 从文件加载 - 空文件 | 1. 写入空文件<br>2. 调用 `loadMetadataFromFile(path)` | 抛出 JSON 解析异常 | P1 |
| M-014 | 元数据 | 从文件加载 - 真实插件元数据文件 | 1. 读取 mujoco_hardware_plugin.meta.json<br>2. 验证 name="mujoco_hardware", version="1.0.0", provides 包含 "Hardware" 和 "Simulator" | 各字段与文件内容完全一致 | P0 |
| M-015 | 元数据 | 从文件加载 - motion_control_plugin.meta.json | 1. 读取 motion_control_plugin.meta.json<br>2. 验证 name="motion_control", provides 包含 "MotionController" 和 "TrajectoryPlanner" | 各字段正确 | P0 |
| M-016 | 元数据 | 从文件加载 - vision_plugin.meta.json | 1. 读取 vision_plugin.meta.json<br>2. 验证 name="vision", provides 包含 "VisionDetector" 和 "PoseEstimator" | 各字段正确 | P0 |
| M-017 | 元数据 | 带依赖的元数据序列化 | 1. 构造含 dependencies 数组的 PluginMetadata（含多个 Dependency，required=true/false）<br>2. 序列化再反序列化 | dependencies 数组完整保留，各 Dependency 字段正确 | P1 |
| M-018 | 元数据 | 扫描目录函数 scanPluginDirectory | 1. 创建临时目录，放入多个 .meta.json 文件和非 json 文件<br>2. 调用 `scanPluginDirectory(dir)` | 返回所有 .meta.json 对应的元数据列表，忽略非 meta.json 文件 | P0 |
| M-019 | 元数据 | 扫描目录 - 空目录 | 1. 创建空目录<br>2. 调用 `scanPluginDirectory(dir)` | 返回空 vector | P1 |
| M-020 | 元数据 | 扫描目录 - 目录不存在 | 1. 调用 `scanPluginDirectory("/nonexistent/dir")` | 返回空 vector（不抛异常） | P1 |
| M-021 | 元数据 | 扫描目录 - 含损坏的 meta.json | 1. 目录中放入一个合法 .meta.json 和一个损坏的 .meta.json<br>2. 调用 `scanPluginDirectory(dir)` | 返回合法的元数据，损坏文件被跳过并输出错误信息到 stderr | P1 |
| M-022 | 元数据 | pluginStateToString 函数 | 1. 对每个 PluginState 枚举值调用 `pluginStateToString()` | Unloaded->"Unloaded", Loaded->"Loaded", Initialized->"Initialized", Running->"Running", Stopped->"Stopped", Error->"Error" | P1 |
| M-023 | 元数据 | 元数据往返一致性 | 1. 构造 PluginMetadata 对象 A<br>2. 序列化为 JSON<br>3. 反序列化为 PluginMetadata 对象 B<br>4. 再次序列化为 JSON | 两次 JSON 内容完全相同 | P1 |

### 1.2 依赖解析测试（DependencyResolver）

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| D-001 | 依赖解析 | 无依赖拓扑排序 | 1. 构造 A, B, C 三个无依赖插件<br>2. 调用 `resolveLoadOrder()` | 返回包含 A, B, C 的列表，大小为 3 | P0 |
| D-002 | 依赖解析 | 线性依赖排序 A->B->C | 1. C 无依赖，B 依赖 C，A 依赖 B<br>2. 调用 `resolveLoadOrder()` | C 排在 B 前，B 排在 A 前 | P0 |
| D-003 | 依赖解析 | 多依赖排序 | 1. A 依赖 B 和 C，B 和 C 无依赖<br>2. 调用 `resolveLoadOrder()` | B 和 C 排在 A 前，B 和 C 顺序不定 | P0 |
| D-004 | 依赖解析 | 菱形依赖 A->B, A->C, B->D, C->D | 1. 构造菱形依赖图<br>2. 调用 `resolveLoadOrder()` | D 排在 B 和 C 前，B 和 C 排在 A 前 | P1 |
| D-005 | 依赖解析 | 循环依赖检测 - A->B->C->A | 1. A 依赖 B，B 依赖 C，C 依赖 A<br>2. 调用 `resolveLoadOrder()` | 抛出 std::runtime_error，消息包含 "Circular dependency detected" | P0 |
| D-006 | 依赖解析 | 自循环依赖 A->A | 1. A 依赖自身<br>2. 调用 `resolveLoadOrder()` | 抛出 std::runtime_error | P1 |
| D-007 | 依赖解析 | 空插件列表 | 1. 传入空 map<br>2. 调用 `resolveLoadOrder()` | 返回空 vector | P1 |
| D-008 | 依赖解析 | 单个无依赖插件 | 1. 仅一个无依赖插件 A<br>2. 调用 `resolveLoadOrder()` | 返回 [A] | P1 |
| D-009 | 依赖解析 | 非必选依赖不影响排序 | 1. A 有 required=false 的依赖 B<br>2. B 不在插件列表中<br>3. 调用 `resolveLoadOrder()` | 正常返回，A 在结果中 | P1 |
| D-010 | 依赖解析 | checkDependencies - 依赖满足 | 1. A 依赖 B（required=true），B 在列表中<br>2. 调用 `checkDependencies("A", plugins)` | 返回 true | P0 |
| D-011 | 依赖解析 | checkDependencies - 依赖不满足 | 1. A 依赖 B（required=true），B 不在列表中<br>2. 调用 `checkDependencies("A", plugins)` | 返回 false | P0 |
| D-012 | 依赖解析 | checkDependencies - 插件不存在 | 1. 查询不存在的插件 "X"<br>2. 调用 `checkDependencies("X", plugins)` | 返回 false | P1 |
| D-013 | 依赖解析 | checkDependencies - 可选依赖缺失 | 1. A 依赖 B（required=false），B 不在列表中<br>2. 调用 `checkDependencies("A", plugins)` | 返回 true（可选依赖不影响） | P1 |
| D-014 | 依赖解析 | checkDependencies - 无依赖 | 1. A 无依赖<br>2. 调用 `checkDependencies("A", plugins)` | 返回 true | P1 |
| D-015 | 依赖解析 | detectCycles - 存在循环 | 1. A->B->C->A 循环<br>2. 调用 `detectCycles()` | 返回非空 vector | P0 |
| D-016 | 依赖解析 | detectCycles - 无循环 | 1. A->B->C 线性依赖<br>2. 调用 `detectCycles()` | 返回空 vector | P0 |
| D-017 | 依赖解析 | detectCycles - 多个独立环 | 1. A->B->A 和 C->D->C 两个独立环<br>2. 调用 `detectCycles()` | 检测到循环（至少一个） | P2 |
| D-018 | 依赖解析 | 大规模插件排序性能 | 1. 构造 100 个无依赖插件<br>2. 调用 `resolveLoadOrder()` | 在合理时间内完成（<100ms），结果包含所有插件 | P2 |
| D-019 | 依赖解析 | 依赖插件不在列表中（忽略外部依赖） | 1. A 依赖 "external_lib"，但 external_lib 不在插件列表中<br>2. 调用 `resolveLoadOrder()` | external_lib 被忽略，A 仍然出现在结果中 | P1 |

### 1.3 事件总线测试（EventBus）

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| E-001 | 事件总线 | 订阅和发布 - 基本功能 | 1. 订阅 "test.event"<br>2. 发布 "test.event"<br>3. 验证 handler 被调用一次 | handler 被调用，event 参数为 "test.event" | P0 |
| E-002 | 事件总线 | 多订阅者 | 1. 两个 handler 订阅同一事件<br>2. 发布事件 | 两个 handler 都被调用各一次 | P0 |
| E-003 | 事件总线 | 取消订阅 | 1. 订阅事件，获取 subscription_id<br>2. 发布事件（handler 被调用）<br>3. 调用 `unsubscribe(id)`<br>4. 再次发布事件 | 取消订阅后 handler 不再被调用 | P0 |
| E-004 | 事件总线 | 数据传递 | 1. 订阅事件<br>2. 发布带 JSON 数据的事件 `{"key":"value","number":42}`<br>3. 验证 handler 接收的 data | handler 收到的 data 与发送的数据一致 | P0 |
| E-005 | 事件总线 | 不同事件隔离 | 1. 订阅 "event.1" 和 "event.2"<br>2. 发布 "event.1"<br>3. 验证 "event.2" 的 handler 未被调用 | 事件之间互不影响 | P0 |
| E-006 | 事件总线 | 发布无订阅者的事件 | 1. 发布 "unknown.event"（无任何订阅） | 不抛异常，正常返回 | P1 |
| E-007 | 事件总线 | 取消不存在的 subscription_id | 1. 调用 `unsubscribe(99999)` | 不抛异常，正常返回 | P1 |
| E-008 | 事件总线 | 多次订阅同一事件 | 1. 同一 handler 订阅同一事件两次<br>2. 发布事件 | handler 被调用两次 | P1 |
| E-009 | 事件总线 | 空数据发布 | 1. 订阅事件<br>2. 发布事件，不传 data 参数（使用默认空 JSON） | handler 被调用，data 为空 JSON `{}` | P1 |
| E-010 | 事件总线 | clear 清除所有订阅 | 1. 订阅多个事件<br>2. 调用 `clear()`<br>3. 发布已订阅的事件 | 所有 handler 均不被调用 | P1 |
| E-011 | 事件总线 | 复杂嵌套 JSON 数据 | 1. 发布包含嵌套对象和数组的 JSON<br>2. 验证 handler 接收的数据 | 嵌套结构完整保留 | P1 |
| E-012 | 事件总线 | handler 内部异常不影响其他 handler | 1. 订阅两个 handler，第一个抛异常<br>2. 发布事件 | 第二个 handler 仍被调用（注意：当前实现可能因异常导致第二个不被调用，需确认行为） | P2 |
| E-013 | 事件总线 | subscription_id 唯一性 | 1. 连续订阅 100 个事件<br>2. 验证所有返回的 id 互不相同 | 所有 id 唯一 | P1 |
| E-014 | 事件总线 | 大量订阅者并发发布 | 1. 订阅 1000 个 handler 到同一事件<br>2. 发布事件 | 所有 handler 都被调用一次 | P2 |
| E-015 | 事件总线 | 线程安全 - 并发订阅和发布 | 1. 线程 A 并发订阅事件<br>2. 线程 B 并发发布事件<br>3. 无死锁或数据竞争 | 不崩溃，不丢事件（允许部分竞争行为） | P1 |

### 1.4 服务注册表测试（ServiceRegistry）

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| S-001 | 服务注册 | 注册和获取服务 | 1. 创建 shared_ptr<TestService><br>2. `registerService("svc", ptr)`<br>3. `getService<TestService>("svc")` | 返回的指针与注册的指针指向同一对象 | P0 |
| S-002 | 服务注册 | 获取不存在的服务 | 1. `getService<TestService>("nonexistent")` | 返回 nullptr | P0 |
| S-003 | 服务注册 | 注销服务 | 1. 注册服务 "svc"<br>2. `unregisterService("svc")` 返回 true<br>3. `getService("svc")` 返回 nullptr | 注销成功，后续获取返回 nullptr | P0 |
| S-004 | 服务注册 | 注销不存在的服务 | 1. `unregisterService("nonexistent")` | 返回 false | P1 |
| S-005 | 服务注册 | hasService 查询 | 1. 注册服务 "svc"<br>2. `hasService("svc")` 返回 true<br>3. `hasService("other")` 返回 false | 查询结果正确 | P0 |
| S-006 | 服务注册 | listServices 列出所有服务 | 1. 注册 "svc1", "svc2", "svc3"<br>2. `listServices()` | 返回包含 "svc1", "svc2", "svc3" 的 vector，大小为 3 | P0 |
| S-007 | 服务注册 | listServices - 空注册表 | 1. 不注册任何服务<br>2. `listServices()` | 返回空 vector | P1 |
| S-008 | 服务注册 | clear 清除所有服务 | 1. 注册多个服务<br>2. `clear()`<br>3. `listServices()` | 返回空 vector，`hasService()` 对所有已注册服务返回 false | P1 |
| S-009 | 服务注册 | 覆盖注册同名服务 | 1. 注册 "svc" 指向对象 A<br>2. 注册 "svc" 指向对象 B<br>3. `getService("svc")` | 返回对象 B（后注册覆盖先注册） | P1 |
| S-010 | 服务注册 | 类型擦除 - 不同类型服务 | 1. 注册 ServiceA 类型 "svcA"<br>2. 注册 ServiceB 类型 "svcB"<br>3. 分别用正确类型获取 | 各自返回正确类型的服务 | P0 |
| S-011 | 服务注册 | 类型转换错误 | 1. 注册 ServiceA 类型 "svc"<br>2. 用 ServiceB 类型 `getService<ServiceB>("svc")` | 返回的指针类型不匹配（未定义行为，需确认是否添加类型检查） | P2 |
| S-012 | 服务注册 | 线程安全 - 并发读写 | 1. 线程 A 并发注册/注销服务<br>2. 线程 B 并发获取/查询服务<br>3. 无死锁或崩溃 | 使用 shared_mutex 保证线程安全，不崩溃 | P1 |
| S-013 | 服务注册 | 服务生命周期 - shared_ptr 引用计数 | 1. 注册 shared_ptr 服务<br>2. 获取服务引用<br>3. 注销服务<br>4. 验证之前获取的引用仍有效 | shared_ptr 引用计数保证对象在仍有引用时不被销毁 | P1 |
| S-014 | 服务注册 | 空名称服务注册 | 1. 注册名称为空字符串 "" 的服务 | 注册成功（当前实现无名称校验） | P2 |
| S-015 | 服务注册 | 大量服务注册和查询性能 | 1. 注册 1000 个服务<br>2. 循环查询所有服务 | 在合理时间内完成 | P2 |

### 1.5 插件管理器测试（PluginManager）

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| PM-001 | 插件管理器 | 构造函数 - 正常目录 | 1. 创建 PluginManager，指定存在的目录 | 不抛异常，实例创建成功 | P0 |
| PM-002 | 插件管理器 | 构造函数 - 不存在的目录 | 1. 创建 PluginManager，指定不存在的目录 | 不抛异常（后续扫描返回空） | P1 |
| PM-003 | 插件管理器 | 禁止拷贝 | 1. 尝试拷贝构造 PluginManager | 编译错误（已 delete） | P1 |
| PM-004 | 插件管理器 | 扫描空目录 | 1. 空目录下 `scanAvailablePlugins()` | 返回空 vector | P0 |
| PM-005 | 插件管理器 | 扫描含 meta.json 的目录 | 1. 目录下放置多个 .meta.json 文件<br>2. `scanAvailablePlugins()` | 返回所有解析成功的元数据列表 | P0 |
| PM-006 | 插件管理器 | 扫描 - 含非 meta.json 文件 | 1. 目录下放置 .json（非 .meta.json）、.so、.txt 文件<br>2. `scanAvailablePlugins()` | 只返回 .meta.json 文件对应的元数据 | P1 |
| PM-007 | 插件管理器 | 扫描 - 含损坏的 meta.json | 1. 目录下放置一个合法和一个损坏的 .meta.json<br>2. `scanAvailablePlugins()` | 返回合法的元数据，损坏的被跳过 | P1 |
| PM-008 | 插件管理器 | getPluginMetadata - 已扫描的插件 | 1. 先 `scanAvailablePlugins()`<br>2. `getPluginMetadata("mujoco_hardware")` | 返回正确的 PluginMetadata | P0 |
| PM-009 | 插件管理器 | getPluginMetadata - 未扫描的插件 | 1. 不扫描，直接 `getPluginMetadata("xxx")` | 抛出 std::runtime_error "Plugin not found" | P1 |
| PM-010 | 插件管理器 | loadPlugin - 正常加载 | 1. 准备好 .meta.json 和 .so 文件<br>2. `loadPlugin("test_plugin")` | 返回 true，状态变为 Loaded | P0 |
| PM-011 | 插件管理器 | loadPlugin - 无 meta 文件 | 1. 仅有 .so 文件，无 .meta.json<br>2. `loadPlugin("test_plugin")` | 返回 false | P0 |
| PM-012 | 插件管理器 | loadPlugin - 无 .so 文件 | 1. 仅有 .meta.json 文件，无 .so<br>2. `loadPlugin("test_plugin")` | 返回 false | P0 |
| PM-013 | 插件管理器 | loadPlugin - 重复加载 | 1. `loadPlugin("test")` 成功<br>2. 再次 `loadPlugin("test")` | 返回 true（幂等操作） | P1 |
| PM-014 | 插件管理器 | loadPlugin - 依赖不满足 | 1. A 依赖 B，B 未加载且不在目录中<br>2. `loadPlugin("A")` | 返回 false | P0 |
| PM-015 | 插件管理器 | initializePlugin - 正常初始化 | 1. loadPlugin 成功<br>2. `initializePlugin("test")` | 返回 true，状态变为 Initialized | P0 |
| PM-016 | 插件管理器 | initializePlugin - 未加载 | 1. 直接 `initializePlugin("test")`（未 load） | 返回 false | P0 |
| PM-017 | 插件管理器 | initializePlugin - 已初始化 | 1. load + initialize 成功<br>2. 再次 `initializePlugin("test")` | 返回 false（状态不是 Loaded） | P1 |
| PM-018 | 插件管理器 | startPlugin - 正常启动 | 1. load + initialize 成功<br>2. `startPlugin("test")` | 返回 true，状态变为 Running | P0 |
| PM-019 | 插件管理器 | startPlugin - 未初始化 | 1. 仅 load 成功<br>2. `startPlugin("test")` | 返回 false（状态是 Loaded，不是 Initialized） | P0 |
| PM-020 | 插件管理器 | stopPlugin - 正常停止 | 1. 完整流程 load->init->start<br>2. `stopPlugin("test")` | 返回 true，状态变为 Stopped | P0 |
| PM-021 | 插件管理器 | stopPlugin - 未运行 | 1. load + initialize，未 start<br>2. `stopPlugin("test")` | 返回 false | P1 |
| PM-022 | 插件管理器 | unloadPlugin - 正常卸载 | 1. 完整流程后 `unloadPlugin("test")` | 返回 true，状态变为 Unloaded，从 loaded_plugins_ 中移除 | P0 |
| PM-023 | 插件管理器 | unloadPlugin - 未加载 | 1. `unloadPlugin("nonexistent")` | 返回 false | P1 |
| PM-024 | 插件管理器 | unloadPlugin - 运行中卸载 | 1. 状态为 Running 的插件<br>2. `unloadPlugin("test")` | 自动先 stop，再 unload，返回 true | P0 |
| PM-025 | 插件管理器 | 完整生命周期流程 | 1. scan -> load -> initialize -> start -> stop -> unload<br>2. 每步验证状态变化 | 状态依次为: Unloaded->Loaded->Initialized->Running->Stopped->Unloaded | P0 |
| PM-026 | 插件管理器 | loadAll - 批量加载 | 1. 目录下有多个插件<br>2. `loadAll()` | 按依赖顺序加载所有插件，返回 true | P0 |
| PM-027 | 插件管理器 | loadAll - 含循环依赖 | 1. 目录下有循环依赖的插件<br>2. `loadAll()` | 返回 false（resolveLoadOrder 抛异常被捕获） | P1 |
| PM-028 | 插件管理器 | startAll - 批量启动 | 1. loadAll 成功后<br>2. `startAll()` | 所有插件从 Loaded 变为 Running | P0 |
| PM-029 | 插件管理器 | stopAll - 批量停止 | 1. 所有插件运行中<br>2. `stopAll()` | 所有 Running 插件变为 Stopped | P0 |
| PM-030 | 插件管理器 | unloadAll - 批量卸载 | 1. 所有插件运行中<br>2. `unloadAll()` | 所有插件被卸载，loaded_plugins_ 为空 | P0 |
| PM-031 | 插件管理器 | 析构函数自动清理 | 1. 加载并启动多个插件<br>2. PluginManager 对象析构 | 自动调用 unloadAll，不崩溃 | P0 |
| PM-032 | 插件管理器 | getLoadedPlugins 查询 | 1. 加载 A, B, C<br>2. `getLoadedPlugins()` | 返回 ["A", "B", "C"] | P0 |
| PM-033 | 插件管理器 | getPluginState 查询 | 1. load 后查询状态<br>2. start 后查询状态 | 分别返回 Loaded 和 Running | P0 |
| PM-034 | 插件管理器 | getPlugin 获取实例 | 1. load + initialize<br>2. `getPlugin("test")` | 返回非空 IPlugin 指针 | P0 |
| PM-035 | 插件管理器 | getPlugin - 未加载 | 1. `getPlugin("nonexistent")` | 返回 nullptr | P1 |
| PM-036 | 插件管理器 | getServiceRegistry | 1. 获取 ServiceRegistry 引用 | 返回有效引用，可正常使用注册/获取功能 | P0 |
| PM-037 | 插件管理器 | getEventBus | 1. 获取 EventBus 引用 | 返回有效引用，可正常使用订阅/发布功能 | P0 |
| PM-038 | 插件管理器 | 线程安全 - 并发操作 | 1. 线程 A 并发 load/unload<br>2. 线程 B 并发 getPluginState<br>3. 使用 shared_mutex 保护 | 不死锁，不崩溃 | P1 |
| PM-039 | 插件管理器 | 服务注册集成 | 1. 加载 mujoco_hardware 插件<br>2. 通过 getService("Hardware") 获取 Hardware 接口 | 返回非空指针，可调用 Hardware 接口方法 | P0 |
| PM-040 | 插件管理器 | 插件目录路径含空格 | 1. 目录路径含空格<br>2. 执行 scan/load 操作 | 正常工作 | P2 |

### 1.6 硬件接口测试（Hardware/Simulator）

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| HW-001 | 硬件接口 | MuJoCo 插件初始化 | 1. load + initialize mujoco_hardware<br>2. 验证 joint_names_ 为 6 个关节 | 6 个关节: joint1~joint6，初始位置均为 0 | P0 |
| HW-002 | 硬件接口 | getState 获取机器人状态 | 1. 初始化后调用 `getState()` | 返回 RobotState，包含 6 个 JointState，timestamp=0，is_moving=false | P0 |
| HW-003 | 硬件接口 | setJointPosition 设置关节位置 | 1. `setJointPosition("joint1", 1.57)` | 返回 true | P0 |
| HW-004 | 硬件接口 | setJointPosition 无效关节名 | 1. `setJointPosition("invalid_joint", 1.0)` | 返回 false | P1 |
| HW-005 | 硬件接口 | setJointPositions 批量设置 | 1. `setJointPositions({{"joint1", 1.0}, {"joint2", 2.0}})` | 返回 true | P0 |
| HW-006 | 硬件接口 | stepSimulation 仿真步进 | 1. 设置关节目标位置<br>2. `stepSimulation(0.001)`<br>3. 读取关节状态 | 关节位置向目标方向变化，timestamp 增加 | P0 |
| HW-007 | 硬件接口 | reset 重置仿真 | 1. 设置关节位置和速度<br>2. `reset()`<br>3. 读取状态 | 所有关节位置/速度/力矩归零，timestamp=0 | P0 |
| HW-008 | 硬件接口 | startSimulation/stopSimulation | 1. `startSimulation()` 返回 true<br>2. `isSimulating()` 返回 true<br>3. `stopSimulation()` 返回 true<br>4. `isSimulating()` 返回 false | 仿真线程正确启停 | P0 |
| HW-009 | 硬件接口 | startSimulation 重复启动 | 1. `startSimulation()`<br>2. 再次 `startSimulation()` | 第二次返回 true（幂等），仅一个仿真线程 | P1 |
| HW-010 | 硬件接口 | stopSimulation 未运行时 | 1. 未启动仿真<br>2. `stopSimulation()` | 返回 true（幂等） | P1 |
| HW-011 | 硬件接口 | getSensorData 获取传感器数据 | 1. 初始化后调用 `getSensorData()` | 返回 3 个传感器: force_sensor, torque_sensor, imu | P0 |
| HW-012 | 硬件接口 | getJointNames | 1. 初始化后调用 `getJointNames()` | 返回 ["joint1", "joint2", ..., "joint6"] | P0 |
| HW-013 | 硬件接口 | getBodyNames | 1. 初始化后调用 `getBodyNames()` | 返回 ["base_link", "link1", ..., "link6"]（7 个） | P1 |
| HW-014 | 硬件接口 | getSensorNames | 1. 初始化后调用 `getSensorNames()` | 返回 ["force_sensor", "torque_sensor", "imu"] | P1 |
| HW-015 | 硬件接口 | PD 控制器收敛 | 1. 设置 joint1 目标为 1.0<br>2. 运行 100 步 stepSimulation(0.5)<br>3. 读取 position | position 向 1.0 方向收敛（低增益 PD，需要多步） | P1 |
| HW-016 | 硬件接口 | getService 接口 | 1. `getService("Hardware")`<br>2. `getService("Simulator")`<br>3. `getService("Unknown")` | 分别返回 Hardware*, Simulator*, nullptr | P0 |
| HW-017 | 硬件接口 | unload 后清理 | 1. unload mujoco_hardware<br>2. 验证内部状态 | joint_states_ 等容器被清空 | P1 |
| HW-018 | 硬件接口 | setJointVelocity | 1. `setJointVelocity("joint1", 2.0)` | 返回 true | P1 |
| HW-019 | 硬件接口 | setJointEffort | 1. `setJointEffort("joint1", 5.0)` | 返回 true | P1 |
| HW-020 | 硬件接口 | 线程安全 - 仿真线程与状态查询并发 | 1. 启动仿真<br>2. 并发调用 getState() 和 setJointPosition() | 不崩溃，数据一致性由 mutex 保护 | P1 |

---

## 二、Web 页面功能按钮测试

### 2.1 插件管理区域

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| W-001 | Web UI | Scan 按钮 - 正常扫描 | 1. 服务端运行，目录下有 3 个插件<br>2. 点击 Scan 按钮 | 调用 `GET /api/plugins/scan`，插件列表显示 3 个插件项（名称、版本、状态标签） | P0 |
| W-002 | Web UI | Scan 按钮 - 空目录 | 1. 插件目录为空<br>2. 点击 Scan 按钮 | 列表为空，日志显示 "Found 0 plugins" | P1 |
| W-003 | Web UI | Scan 按钮 - 服务不可达 | 1. 服务端未启动<br>2. 点击 Scan 按钮 | 连接状态变为 Disconnected（红色），列表无变化 | P0 |
| W-004 | Web UI | Load 按钮 - 正常加载 | 1. 扫描后点击某个插件的 Load 按钮 | 调用 `POST /api/plugins/{name}/load`，日志显示成功，列表刷新，状态标签更新为 "Loaded" | P0 |
| W-005 | Web UI | Load 按钮 - 加载 mujoco_hardware | 1. 点击 mujoco_hardware 的 Load 按钮 | 加载成功后自动调用 `initJointControls()`，关节控制区域显示 6 个滑块 | P0 |
| W-006 | Web UI | Load 按钮 - 加载失败 | 1. 点击一个缺少 .so 文件的插件的 Load 按钮 | 日志显示 "Failed to load plugin xxx"（红色） | P1 |
| W-007 | Web UI | Start 按钮 - 正常启动 | 1. 加载后点击 Start 按钮 | 调用 `POST /api/plugins/{name}/start`，状态变为 "Running"（绿色标签） | P0 |
| W-008 | Web UI | Start 按钮 - 未加载直接启动 | 1. 扫描后直接点击 Start 按钮（未先 Load） | API 自动执行 load->init->start，启动成功 | P0 |
| W-009 | Web UI | Start 按钮 - 已运行再启动 | 1. 插件已在 Running 状态<br>2. 点击 Start 按钮 | API 返回 success=true, message="Already running"，无异常 | P1 |
| W-010 | Web UI | Start 按钮 - mujoco_hardware 启动后初始化关节控制 | 1. 启动 mujoco_hardware<br>2. 验证关节控制和自动刷新 | 关节控制区域显示 6 个滑块，自动刷新启动 | P0 |

### 2.2 仿真控制区域

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| W-011 | Web UI | Auto Run 按钮 | 1. 点击 "Auto Run" 按钮 | 调用 `POST /api/simulation/start`，日志显示 "Simulation started (Auto)"，仿真状态变为 "Running"，模式显示 "Auto" | P0 |
| W-012 | Web UI | Auto Run - 硬件插件未加载 | 1. 不加载 mujoco_hardware<br>2. 点击 "Auto Run" | API 返回 404 {"error": "Hardware plugin not loaded"} | P1 |
| W-013 | Web UI | Stop 按钮 | 1. 仿真运行中<br>2. 点击 "Stop" 按钮 | 调用 `POST /api/simulation/stop`，日志显示 "Simulation stopped"，状态变为 "Stopped"，模式显示 "Manual" | P0 |
| W-014 | Web UI | Stop - 未运行时点击 | 1. 仿真未运行<br>2. 点击 "Stop" | API 返回 success=true（幂等），无异常 | P1 |
| W-015 | Web UI | Reset 按钮 | 1. 运行仿真一段时间后<br>2. 点击 "Reset" 按钮 | 调用 `POST /api/simulation/reset`，日志显示 "Simulation reset"，图表数据清空，模式显示 "Manual" | P0 |
| W-016 | Web UI | Reset - 图表数据清空 | 1. 积累图表数据后<br>2. 点击 "Reset"<br>3. 验证图表 | jointChart 和 effortChart 数据点清空，图表更新 | P0 |
| W-017 | Web UI | Step x1 按钮 | 1. 点击 "Step x1" 按钮 | 先调用 stop（如在运行），再调用 `POST /api/simulation/step` {dt:0.1}，日志显示 "Simulation stepped: dt=0.1s"，状态立即更新 | P0 |
| W-018 | Web UI | Step x5 按钮 | 1. 点击 "Step x5" 按钮 | 先 stop，然后循环 5 次调用 step API，每次间隔 50ms，日志显示 "Stepped 5 times" | P0 |
| W-019 | Web UI | Step x10 按钮 | 1. 点击 "Step x10" 按钮 | 循环 10 次 step，日志显示 "Stepped 10 times" | P1 |
| W-020 | Web UI | Step x50 按钮 | 1. 点击 "Step x50" 按钮 | 循环 50 次 step，日志显示 "Stepped 50 times" | P1 |
| W-021 | Web UI | Step - 时间显示更新 | 1. 多次 Step 后<br>2. 检查 Time 显示 | sim-time 元素显示累积仿真时间 | P0 |
| W-022 | Web UI | Mode 显示 | 1. Auto Run 后检查 Mode<br>2. Stop 后检查 Mode<br>3. Step 后检查 Mode | Auto Run->"Auto"，Stop/Step->"Manual" | P0 |

### 2.3 关节控制区域

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| W-023 | Web UI | 关节控制初始化 | 1. 加载并启动 mujoco_hardware<br>2. 检查关节控制区域 | 显示 6 个滑块（joint1~joint6），每个有标签、滑块、数值显示、Set 按钮 | P0 |
| W-024 | Web UI | 关节滑块范围 | 1. 检查滑块属性 | min=-3.14, max=3.14, step=0.01, 初始值=0 | P0 |
| W-025 | Web UI | 滑块拖动 - 数值显示更新 | 1. 拖动 joint1 滑块到 1.57<br>2. 检查数值显示 | 显示 "1.57 rad" | P0 |
| W-026 | Web UI | Set 按钮 - 设置关节位置 | 1. 拖动 joint1 滑块到 1.00<br>2. 点击 joint1 的 Set 按钮 | 调用 `POST /api/robot/joints/joint1/position` {position: 1.0} | P0 |
| W-027 | Web UI | Set 按钮 - 设置负值 | 1. 拖动 joint2 滑块到 -2.00<br>2. 点击 Set | 调用 API，position=-2.0 | P1 |
| W-028 | Web UI | Set 按钮 - 边界值 3.14 | 1. 滑块拖到最大值 3.14<br>2. 点击 Set | 调用 API，position=3.14 | P1 |
| W-029 | Web UI | Set 按钮 - 边界值 -3.14 | 1. 滑块拖到最小值 -3.14<br>2. 点击 Set | 调用 API，position=-3.14 | P1 |
| W-030 | Web UI | 关节控制 - 插件未加载时显示 | 1. 未加载 mujoco_hardware<br>2. 检查关节控制区域 | 显示提示文本 "Load mujoco_hardware plugin first" | P1 |
| W-031 | Web UI | 自动刷新时滑块位置同步 | 1. 启动仿真和自动刷新<br>2. 观察滑块位置 | 滑块位置随仿真状态自动更新（当用户未拖动滑块时） | P0 |

### 2.4 传感器数据展示

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| W-032 | Web UI | 传感器数据初始显示 | 1. 加载硬件插件后<br>2. 检查传感器区域 | Force X/Y/Z, Accel X/Y/Z 六个数值卡片显示，初始值正确 | P0 |
| W-033 | Web UI | 力传感器数据更新 | 1. 自动刷新运行中<br>2. 观察 Force X/Y/Z | 力传感器数值从 API 获取并更新（Force Z 预期为 9.81） | P0 |
| W-034 | Web UI | IMU 数据更新 | 1. 自动刷新运行中<br>2. 观察 Accel X/Y/Z | Accel Z 预期为 9.81，X/Y 为 0.00 | P0 |
| W-035 | Web UI | 传感器数据精度 | 1. 检查数值显示 | 保留两位小数（toFixed(2)） | P1 |
| W-036 | Web UI | 传感器 API 不可用时 | 1. 硬件插件未加载<br>2. 自动刷新触发 | API 返回 404，传感器数值不更新，不报 JS 错误 | P1 |

### 2.5 图表实时更新

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| W-037 | Web UI | Joint Positions Chart 初始化 | 1. 页面加载<br>2. 检查 jointChart | Chart.js 折线图已初始化，6 条数据线（joint1~joint6），不同颜色 | P0 |
| W-038 | Web UI | Joint Efforts Chart 初始化 | 1. 页面加载<br>2. 检查 effortChart | Chart.js 折线图已初始化，6 条数据线 | P0 |
| W-039 | Web UI | 图表数据实时更新 | 1. 启动自动刷新<br>2. 运行仿真 | 每次 updateRobotState() 后图表更新，新数据点追加 | P0 |
| W-040 | Web UI | 图表最大数据点数 | 1. 持续运行超过 50 个数据点<br>2. 检查图表数据 | 数据点数不超过 maxDataPoints(50)，旧数据被移除 | P1 |
| W-041 | Web UI | Reset 后图表清空 | 1. 积累数据后 Reset<br>2. 检查图表 | chartData.labels 和 positions/efforts 数组清空，图表更新 | P0 |
| W-042 | Web UI | 图表动画关闭 | 1. 检查 Chart.js 配置 | animation.duration=0，实时更新无延迟动画 | P1 |

### 2.6 自动刷新开关

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| W-043 | Web UI | 自动刷新默认开启 | 1. 页面加载完成 | autoRefresh=true，toggle 为 active 状态，显示 "10 Hz" | P0 |
| W-044 | Web UI | 关闭自动刷新 | 1. 点击 Auto Refresh toggle | toggle 移除 active 类，刷新定时器停止，不再自动更新 | P0 |
| W-045 | Web UI | 重新开启自动刷新 | 1. 关闭后再次点击 toggle | toggle 恢复 active 类，定时器重新启动（100ms 间隔） | P0 |
| W-046 | Web UI | 自动刷新频率 | 1. 开启自动刷新<br>2. 测量 API 调用间隔 | 约 100ms 一次（10 Hz） | P1 |
| W-047 | Web UI | 自动刷新 - 服务不可达时 | 1. 关闭服务端<br>2. 自动刷新运行中 | 连接状态变为 Disconnected，不报未捕获错误 | P1 |

### 2.7 连接状态指示

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| W-048 | Web UI | 初始连接状态 | 1. 页面加载 | 显示 "Connecting..."，指示器无 connected/disconnected 类 | P1 |
| W-049 | Web UI | 连接成功状态 | 1. API 请求成功 | 指示器为绿色（connected 类），文本为 "Connected" | P0 |
| W-050 | Web UI | 连接断开状态 | 1. API 请求失败 | 指示器为红色（disconnected 类），文本为 "Disconnected" | P0 |
| W-051 | Web UI | 连接恢复 | 1. 断开后服务恢复<br>2. 下一次 API 请求成功 | 状态恢复为 Connected | P1 |

### 2.8 日志区域

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| W-052 | Web UI | 日志自动记录 | 1. 执行各种操作 | 每个操作在日志区域记录带时间戳的条目 | P0 |
| W-053 | Web UI | 日志类型 - info | 1. 初始化操作 | 日志条目为蓝色（info 类） | P1 |
| W-054 | Web UI | 日志类型 - success | 1. 操作成功 | 日志条目为绿色（success 类） | P1 |
| W-055 | Web UI | 日志类型 - error | 1. 操作失败 | 日志条目为红色（error 类） | P1 |
| W-056 | Web UI | Clear 按钮 | 1. 积累多条日志<br>2. 点击 Clear 按钮 | 日志区域清空 | P0 |
| W-057 | Web UI | 日志最大条数 | 1. 持续记录超过 100 条日志 | 日志条数不超过 100，最早的日志被移除 | P1 |
| W-058 | Web UI | 日志自动滚动 | 1. 日志超出可视区域 | 自动滚动到底部（scrollTop = scrollHeight） | P1 |

### 2.9 页面初始化

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| W-059 | Web UI | 页面加载初始化 | 1. 打开页面 | initCharts() 初始化图表，scanPlugins() 自动扫描，日志显示 "Debug console initialized" | P0 |
| W-060 | Web UI | 自动加载 mujoco_hardware | 1. 页面加载后等待 500ms | 自动调用 loadPlugin('mujoco_hardware') 和 startPlugin('mujoco_hardware') | P0 |
| W-061 | Web UI | 页面加载后自动启动刷新 | 1. 页面加载完成 | startAutoRefresh() 被调用，开始 100ms 间隔轮询 | P0 |

---

## 三、REST API 端点测试

### 3.1 插件管理 API

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| API-001 | REST API | GET /api/plugins/scan - 正常 | 1. 目录下有 3 个 .meta.json<br>2. 发送 GET 请求 | HTTP 200，JSON 数组包含 3 个对象，每个有 name, version, description, author, category, provides | P0 |
| API-002 | REST API | GET /api/plugins/scan - 空目录 | 1. 空插件目录<br>2. 发送 GET 请求 | HTTP 200，JSON 空数组 `[]` | P0 |
| API-003 | REST API | GET /api/plugins/{name}/metadata - 正常 | 1. 先 scan<br>2. `GET /api/plugins/mujoco_hardware/metadata` | HTTP 200，JSON 包含完整元数据 | P0 |
| API-004 | REST API | GET /api/plugins/{name}/metadata - 不存在 | 1. `GET /api/plugins/nonexistent/metadata` | HTTP 404，`{"error": "Plugin not found"}` | P0 |
| API-005 | REST API | POST /api/plugins/{name}/load - 正常 | 1. `POST /api/plugins/mujoco_hardware/load` | HTTP 200，`{"success": true, "plugin": "mujoco_hardware"}` | P0 |
| API-006 | REST API | POST /api/plugins/{name}/load - 不存在的插件 | 1. `POST /api/plugins/nonexistent/load` | HTTP 200，`{"success": false, "plugin": "nonexistent"}` | P0 |
| API-007 | REST API | POST /api/plugins/{name}/unload - 正常 | 1. 先加载<br>2. `POST /api/plugins/mujoco_hardware/unload` | HTTP 200，`{"success": true}` | P0 |
| API-008 | REST API | POST /api/plugins/{name}/unload - 未加载 | 1. `POST /api/plugins/nonexistent/unload` | HTTP 200，`{"success": false}` | P1 |
| API-009 | REST API | POST /api/plugins/{name}/start - 正常 | 1. 先加载<br>2. `POST /api/plugins/mujoco_hardware/start` | HTTP 200，`{"success": true}`，插件状态变为 Running | P0 |
| API-010 | REST API | POST /api/plugins/{name}/start - 未加载 | 1. `POST /api/plugins/mujoco_hardware/start`（未加载） | HTTP 200，API 自动执行 load->init->start，success=true | P0 |
| API-011 | REST API | POST /api/plugins/{name}/start - 已运行 | 1. 插件已在 Running<br>2. `POST /api/plugins/mujoco_hardware/start` | HTTP 200，`{"success": true, "message": "Already running"}` | P1 |
| API-012 | REST API | POST /api/plugins/{name}/stop - 正常 | 1. 运行中的插件<br>2. `POST /api/plugins/mujoco_hardware/stop` | HTTP 200，`{"success": true}`，状态变为 Stopped | P0 |
| API-013 | REST API | POST /api/plugins/{name}/stop - 未运行 | 1. 已加载未启动<br>2. `POST /api/plugins/mujoco_hardware/stop` | HTTP 200，`{"success": false}` | P1 |
| API-014 | REST API | GET /api/plugins/{name}/state - 各状态 | 1. 分别在 Loaded, Initialized, Running, Stopped 状态查询 | HTTP 200，`{"plugin": "name", "state": "对应状态字符串"}` | P0 |
| API-015 | REST API | GET /api/plugins/{name}/state - 未加载 | 1. 查询未加载插件状态 | HTTP 200，`{"state": "Unloaded"}` | P1 |
| API-016 | REST API | GET /api/plugins/loaded - 有已加载插件 | 1. 加载 A, B<br>2. `GET /api/plugins/loaded` | HTTP 200，JSON 数组 ["A", "B"] | P0 |
| API-017 | REST API | GET /api/plugins/loaded - 无已加载插件 | 1. 不加载任何插件<br>2. `GET /api/plugins/loaded` | HTTP 200，JSON 空数组 `[]` | P1 |

### 3.2 硬件接口 API

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| API-018 | REST API | GET /api/robot/state - 正常 | 1. 加载并启动 mujoco_hardware<br>2. `GET /api/robot/state` | HTTP 200，JSON 包含 timestamp, is_moving, joints 数组（6 个关节，每个有 name, position, velocity, effort） | P0 |
| API-019 | REST API | GET /api/robot/state - 插件未加载 | 1. 未加载 mujoco_hardware<br>2. `GET /api/robot/state` | HTTP 404，`{"error": "Hardware plugin not loaded"}` | P0 |
| API-020 | REST API | POST /api/robot/joints/{name}/position - 正常 | 1. 发送 `POST /api/robot/joints/joint1/position` body: `{"position": 1.57}` | HTTP 200，`{"success": true, "joint": "joint1", "position": 1.57}` | P0 |
| API-021 | REST API | POST /api/robot/joints/{name}/position - 无效关节 | 1. `POST /api/robot/joints/invalid/position` body: `{"position": 1.0}` | HTTP 200，`{"success": false}` | P1 |
| API-022 | REST API | POST /api/robot/joints/{name}/position - 无效 body | 1. body 为空或非法 JSON | HTTP 400，`{"error": "Invalid request body"}` | P0 |
| API-023 | REST API | POST /api/robot/joints/{name}/position - 缺少 position 字段 | 1. body: `{"other": 1.0}` | HTTP 400，`{"error": "Invalid request body"}` | P1 |
| API-024 | REST API | POST /api/robot/joints/{name}/position - 插件未加载 | 1. 未加载 mujoco_hardware | HTTP 404 | P0 |
| API-025 | REST API | POST /api/robot/joints/positions - 批量设置 | 1. body: `{"joint1": 1.0, "joint2": 2.0}` | HTTP 200，`{"success": true}` | P0 |
| API-026 | REST API | POST /api/robot/joints/positions - 空 body | 1. body: `{}` | HTTP 200，`{"success": true}`（无操作） | P1 |
| API-027 | REST API | POST /api/robot/joints/positions - 无效 body | 1. body: "not json" | HTTP 400 | P1 |
| API-028 | REST API | GET /api/robot/sensors - 正常 | 1. 加载硬件插件<br>2. `GET /api/robot/sensors` | HTTP 200，JSON 数组含 3 个传感器对象，每个有 name, type, values | P0 |
| API-029 | REST API | GET /api/robot/sensors - 插件未加载 | 1. 未加载硬件插件 | HTTP 404 | P0 |

### 3.3 仿真控制 API

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| API-030 | REST API | POST /api/simulation/start - 正常 | 1. 加载并启动 mujoco_hardware<br>2. `POST /api/simulation/start` | HTTP 200，`{"success": true}`，仿真线程启动 | P0 |
| API-031 | REST API | POST /api/simulation/start - 重复启动 | 1. 已在仿真中<br>2. 再次 start | HTTP 200，`{"success": true}`（幂等） | P1 |
| API-032 | REST API | POST /api/simulation/start - 插件未加载 | 1. 未加载硬件插件 | HTTP 404 | P0 |
| API-033 | REST API | POST /api/simulation/stop - 正常 | 1. 仿真运行中<br>2. `POST /api/simulation/stop` | HTTP 200，`{"success": true}`，仿真线程停止 | P0 |
| API-034 | REST API | POST /api/simulation/stop - 未运行 | 1. 仿真未运行<br>2. stop | HTTP 200，`{"success": true}`（幂等） | P1 |
| API-035 | REST API | POST /api/simulation/stop - 插件未加载 | 1. 未加载硬件插件 | HTTP 404 | P1 |
| API-036 | REST API | POST /api/simulation/reset - 正常 | 1. 运行仿真后<br>2. `POST /api/simulation/reset` | HTTP 200，`{"success": true}`，关节归零，时间归零 | P0 |
| API-037 | REST API | POST /api/simulation/reset - 插件未加载 | 1. 未加载硬件插件 | HTTP 404 | P1 |
| API-038 | REST API | POST /api/simulation/step - 默认 dt | 1. `POST /api/simulation/step`（无 body） | HTTP 200，`{"success": true, "dt": 0.001}` | P0 |
| API-039 | REST API | POST /api/simulation/step - 指定 dt | 1. body: `{"dt": 0.1}` | HTTP 200，`{"success": true, "dt": 0.1}` | P0 |
| API-040 | REST API | POST /api/simulation/step - dt=0 | 1. body: `{"dt": 0}` | HTTP 200，`{"success": true, "dt": 0}` | P2 |
| API-041 | REST API | POST /api/simulation/step - 负 dt | 1. body: `{"dt": -0.1}` | HTTP 200，`{"success": true, "dt": -0.1}`（当前无校验） | P2 |
| API-042 | REST API | POST /api/simulation/step - 非法 body | 1. body: "invalid" | HTTP 200，使用默认 dt=0.001（解析异常被捕获） | P1 |
| API-043 | REST API | POST /api/simulation/step - 插件未加载 | 1. 未加载硬件插件 | HTTP 404 | P1 |

### 3.4 服务和事件 API

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| API-044 | REST API | GET /api/services - 正常 | 1. 注册了若干服务<br>2. `GET /api/services` | HTTP 200，JSON 数组包含服务名列表 | P0 |
| API-045 | REST API | GET /api/services - 无服务 | 1. 未注册任何服务<br>2. `GET /api/services` | HTTP 200，JSON 空数组 | P1 |
| API-046 | REST API | POST /api/events/publish - 正常 | 1. body: `{"event": "test.event", "data": {"key": "value"}}`<br>2. 有订阅者监听该事件 | HTTP 200，`{"success": true, "event": "test.event"}`，订阅者收到事件 | P0 |
| API-047 | REST API | POST /api/events/publish - 无 data | 1. body: `{"event": "test.event"}` | HTTP 200，`{"success": true}`，订阅者收到空 data | P1 |
| API-048 | REST API | POST /api/events/publish - 无效 body | 1. body: "not json" | HTTP 400，`{"error": "Invalid request body"}` | P0 |
| API-049 | REST API | POST /api/events/publish - 缺少 event 字段 | 1. body: `{"data": {}}` | HTTP 400（抛异常被捕获） | P1 |
| API-050 | REST API | POST /api/events/publish - 无订阅者 | 1. 发布无人订阅的事件 | HTTP 200，`{"success": true}`，不抛异常 | P1 |

### 3.5 CORS 和通用测试

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| API-051 | REST API | CORS 预检请求 | 1. 发送 OPTIONS /api/plugins/scan | HTTP 204，Headers 包含 Access-Control-Allow-Origin: *，Allow-Methods: GET, POST, PUT, DELETE, OPTIONS | P0 |
| API-052 | REST API | CORS 响应头 | 1. 发送任意 GET /api/* 请求 | 响应包含 Access-Control-Allow-Origin: * | P0 |
| API-053 | REST API | Content-Type 验证 | 1. 发送 GET /api/plugins/scan | Content-Type: application/json | P0 |
| API-054 | REST API | 无效 API 路径 | 1. GET /api/invalid/path | HTTP 404（cpp-httplib 默认处理） | P1 |
| API-055 | REST API | 无效 HTTP 方法 | 1. DELETE /api/plugins/scan | HTTP 405 Method Not Allowed | P1 |
| API-056 | REST API | 并发 API 请求 | 1. 10 个并发 GET /api/robot/state<br>2. 同时 10 个并发 POST /api/simulation/step | 所有请求正常响应，不崩溃 | P1 |
| API-057 | REST API | 大请求体 | 1. POST /api/events/publish body 为超大 JSON（>1MB） | 正常处理或返回适当错误 | P2 |

### 3.6 服务器生命周期测试

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| API-058 | REST API | DebugAPI 启动 | 1. 创建 DebugAPI 实例<br>2. 调用 `start()` | 返回 true，isRunning() 为 true，HTTP 请求可响应 | P0 |
| API-059 | REST API | DebugAPI 停止 | 1. 启动后调用 `stop()` | 返回 true，isRunning() 为 false，HTTP 请求无法连接 | P0 |
| API-060 | REST API | DebugAPI 重复启动 | 1. 启动后再次 `start()` | 返回 true（幂等），不创建第二个线程 | P1 |
| API-061 | REST API | DebugAPI 重复停止 | 1. 未启动或已停止时 `stop()` | 返回 true（幂等） | P1 |
| API-062 | REST API | DebugAPI 析构自动停止 | 1. 启动后销毁 DebugAPI 对象 | 析构函数调用 stop()，线程正确 join | P0 |
| API-063 | REST API | 端口占用 | 1. 第一个实例占用端口 8080<br>2. 第二个实例尝试同一端口 | 第二个 start() 返回 false 或 listen 失败 | P1 |

---

## 四、示例插件功能测试

### 4.1 运动控制插件（MotionControlPlugin）

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| EX-001 | 示例插件 | motion_control 生命周期 | 1. load -> initialize -> start -> stop -> unload | 每步成功，状态正确转换 | P0 |
| EX-002 | 示例插件 | MotionController 接口 | 1. initialize 后<br>2. setJointPosition(0, 1.57)<br>3. getJointPosition(0) | 返回 1.57 | P0 |
| EX-003 | 示例插件 | MotionController - 无效关节 ID | 1. setJointPosition(-1, 1.0)<br>2. setJointPosition(10, 1.0) | 返回 false | P1 |
| EX-004 | 示例插件 | getJointCount | 1. initialize 后调用 | 返回 6 | P1 |
| EX-005 | 示例插件 | TrajectoryPlanner 接口 | 1. planTrajectory({0.0}, {1.0}, 1.0) | 返回线性插值轨迹点序列 | P1 |
| EX-006 | 示例插件 | getService 接口 | 1. getService("MotionController")<br>2. getService("TrajectoryPlanner")<br>3. getService("Unknown") | 前两个返回非空指针，第三个返回 nullptr | P0 |

### 4.2 视觉插件（VisionPlugin）

| 编号 | 类别 | 测试项 | 测试步骤 | 预期结果 | 优先级 |
|------|------|--------|----------|----------|--------|
| EX-007 | 示例插件 | vision 生命周期 | 1. load -> initialize -> start -> stop -> unload | 每步成功 | P0 |
| EX-008 | 示例插件 | VisionDetector.detect | 1. 传入空图像数据<br>2. 调用 detect() | 返回 2 个检测结果 (object_1, object_2) | P0 |
| EX-009 | 示例插件 | PoseEstimator.estimatePose | 1. 传入 Detection 对象<br>2. 调用 estimatePose() | 返回 Pose，x/y 基于 detection 坐标转换，z=0.5 | P1 |
| EX-010 | 示例插件 | getService 接口 | 1. getService("VisionDetector")<br>2. getService("PoseEstimator") | 返回非空指针 | P0 |

---

## 五、测试用例统计

| 测试类别 | 用例数 | P0 | P1 | P2 |
|----------|--------|----|----|-----|
| 元数据加载和解析 | 23 | 8 | 12 | 3 |
| 依赖解析 | 19 | 7 | 9 | 3 |
| 事件总线 | 15 | 5 | 7 | 3 |
| 服务注册表 | 15 | 6 | 6 | 3 |
| 插件管理器 | 40 | 22 | 14 | 4 |
| 硬件接口 | 20 | 9 | 9 | 2 |
| Web UI - 插件管理 | 10 | 7 | 3 | 0 |
| Web UI - 仿真控制 | 12 | 9 | 3 | 0 |
| Web UI - 关节控制 | 9 | 6 | 3 | 0 |
| Web UI - 传感器 | 5 | 2 | 3 | 0 |
| Web UI - 图表 | 6 | 3 | 3 | 0 |
| Web UI - 自动刷新 | 5 | 2 | 3 | 0 |
| Web UI - 连接状态 | 4 | 2 | 2 | 0 |
| Web UI - 日志 | 7 | 2 | 5 | 0 |
| Web UI - 页面初始化 | 3 | 3 | 0 | 0 |
| REST API - 插件管理 | 17 | 11 | 6 | 0 |
| REST API - 硬件接口 | 12 | 7 | 4 | 1 |
| REST API - 仿真控制 | 14 | 6 | 4 | 4 |
| REST API - 服务和事件 | 7 | 3 | 4 | 0 |
| REST API - CORS 和通用 | 7 | 3 | 3 | 1 |
| REST API - 服务器生命周期 | 6 | 3 | 3 | 0 |
| 示例插件 | 10 | 5 | 5 | 0 |
| **合计** | **266** | **124** | **112** | **24** |

---

## 六、测试环境要求

### 6.1 单元测试环境

- **测试框架**: Google Test (GTest) v1.14.0
- **编译器**: 支持 C++17 的编译器（GCC 9+, Clang 10+）
- **构建系统**: CMake 3.14+
- **依赖库**: nlohmann/json, dylib
- **运行命令**: `cd build && ctest --output-on-failure`

### 6.2 集成测试环境

- **运行中的 Debug Server**: 端口 8080
- **插件目录**: 包含已编译的 .so 文件和 .meta.json 文件
- **HTTP 客户端**: curl 或 httplib 测试客户端
- **浏览器**: Chrome/Firefox（Web UI 测试）

### 6.3 Web UI 测试环境

- **浏览器**: Chrome 最新版（推荐使用 DevTools Network 面板验证 API 调用）
- **测试工具**: 可选 Selenium WebDriver 或手动测试
- **验证要点**: 每个按钮点击对应的 API 调用、响应处理、UI 状态更新
