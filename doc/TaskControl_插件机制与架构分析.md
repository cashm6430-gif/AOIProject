# TaskControl 插件机制与代码架构分析

> 日期：2026-02-24
> 
> 分析目标：解释“通过 TaskControl C++ 接口加载 `pipeline_example.json` 提示找不到算子”的原因；梳理当前代码架构与主流程。

---

## 1. 结论摘要

当前项目存在**插件机制未打通**与**算子注册名不一致**两类关键问题：

1. 代码里有 `PluginLoader`，但在 `TaskControl -> TaskControlImpl -> AlgorithmTask` 的执行链路中**没有任何地方调用插件加载**。
2. 当前可用算子主要依赖 `REGISTER_OPERATOR(...)` 宏注册，但该宏默认以**类名**注册（如 `PlaneFitOperator`），而配方中使用的是**业务名**（如 `PlaneFit`），导致 `OperatorRegistry::create("PlaneFit")` 失败。
3. `Plugins/` 目录下虽然有插件源码，但并未纳入当前 `.slnx` 的工程构建链路；运行输出目录中也未看到插件 DLL 被部署。

因此，`Unknown operator` 是一个高概率、可复现的结果。

---

## 2. 现象与触发路径

典型调用链：

1. `TaskControl::OpenTask(...)`
2. `TaskControlImpl::OpenTask(...)`
3. `AlgorithmTask::open()`
4. `AlgorithmTask::setConfig(...)`
5. 遍历 pipeline 节点，调用 `OperatorRegistry::instance().create(node.operatorName)`

当 `create(...)` 返回空时，报错：`Unknown operator: <name>`。

对应代码位置：
- `TaskControl/TaskControl.cpp`
- `TaskControl/TaskControlImpl.cpp`
- `TaskControl/core/AlgorithmTask.h`

---

## 3. 插件机制核查结果

### 3.1 `PluginLoader` 存在但未接入执行主链

- `TaskControl/core/PluginLoader.h` 已实现目录扫描、`QPluginLoader` 加载、`IOperatorPlugin` 接口识别、注册/卸载逻辑。
- 但在 `AlgorithmTask` / `TaskControlImpl` 中未见 `loadFromDirectory(...)` 或 `loadPlugin(...)` 调用。
- 结果：运行时不会自动加载任何插件 DLL。

### 3.2 插件工程未纳入当前解决方案主构建

- `TaskControl.slnx` 仅包含：
  - `TaskControl/TaskControl.vcxproj`
  - `taskflow_demo/taskflow_demo.vcxproj`
- `Plugins/` 目录未作为独立项目加入当前 VS 工程链（仅文件夹存在）。

### 3.3 运行目录未见插件 DLL 部署

- `build/bin/Debug`、`build/bin/Release` 下可见 `TaskControl.dll`，未见 `PreprocessPlugin.dll`、`FeaturePlugin.dll` 等。
- 即使将来调用 `PluginLoader`，若目录中没有插件 DLL 也会加载失败。

### 3.4 插件源码路径组织存在风险

- `Plugins/*Plugin/*.h` 中使用 `../../core/...` 引用核心头。
- 当前核心代码在 `TaskControl/core`，目录基准不同，插件若单独构建时 include 路径需要严格校正。
- `Plugins/CMakeLists.txt` 的源文件路径写法（如 `feature/FeaturePlugin.cpp`）与当前目录结构（`FeaturePlugin/FeaturePlugin.cpp`）存在不一致风险。

> 说明：即使不考虑以上工程问题，仅“主链未调用 PluginLoader”这一条也足以导致插件机制失效。

---

## 4. 算子注册机制核查结果（关键根因）

### 4.1 当前宏注册名与配方名不一致

`OperatorRegistry.h` 中：

- `REGISTER_OPERATOR(PlaneFitOperator)` 会注册键：`"PlaneFitOperator"`（类名字符串）。
- 但 `pipeline_example.json` 使用的是：`"PlaneFit"`、`"PointCloudFilter"`、`"Distance"`、`"ResultJudge"`。

这会导致：

- `create("PlaneFit")` 找不到工厂（即便类已被 include 并执行了静态注册）。

### 4.2 当前“可用算子”还依赖 `Example_Usage.cpp` 的副作用 include

- `TaskControl/examples/Example_Usage.cpp` 手工 include 了少量算子头（注释写着“确保注册”）。
- 这意味着注册行为依赖示例文件是否参与编译，属于脆弱设计。
- 当前 `TaskControl.vcxproj` 确实编译了该示例文件，但这不是稳定的生产机制。

---

## 5. 为什么会报“找不到算子”

综合当前仓库状态，报错可由以下任一条件触发：

1. **未加载插件**：`PluginLoader` 没被调用，动态插件算子不可见。
2. **注册名不匹配**：宏注册的是 `xxxOperator`，配方写的是业务名。
3. **注册覆盖不足**：依赖 `Example_Usage.cpp` include 的少量注册，且不完整、不稳定。

这三条中，第 2 条在当前代码里是确定性问题。

---

## 6. 当前项目代码架构梳理

## 6.1 分层结构

1. **导出接口层**
   - C++ 接口：`TaskControl.h`
   - C 接口：`TaskControl_C.h/.cpp`
   - 错误码与对外数据：`TaskControlAPI_global.h`、`DataDefines.h`

2. **业务门面层**
   - `TaskControl.cpp`
   - `TaskControlImpl.h/.cpp`
   - 职责：配置加载、输入收集、调用算法引擎、输出转换

3. **算法核心层（core）**
   - `AlgorithmTask.h`：任务生命周期 + setConfig + process
   - `OperatorGraph.h`：pipeline DAG 数据结构
   - `TaskflowExecutor.h`：并行调度执行
   - `AlgorithmContext.h`：节点间数据传递
   - `OperatorRegistry.h`：算子工厂注册中心
   - `PluginLoader.h`：Qt 插件装载器（当前未接主链）

4. **算子实现层（内置头文件）**
   - `operators/preprocess/*`
   - `operators/feature/*`
   - `operators/geometry/*`
   - `operators/measure/*`

5. **插件层（目录存在，链路未闭合）**
   - `Plugins/*Plugin/*`
   - 定义了 `IOperatorPlugin` 实现，但工程构建/加载闭环不完整。

## 6.2 主执行逻辑

1. `OpenTask(path)`：打开引擎、读取 JSON、`setConfig`
2. `setConfig`：构建 `OperatorGraph`，为每个节点按名字 `create` 算子实例
3. `AddData / AddImageData`：缓存输入
4. `RunTask`：封装 `AlgorithmInput`，执行 `process`
5. `process`：写入上下文、执行 DAG、收集 `result_*` 输出
6. `RunTask` 返回 JSON 结果

---

## 7. 修复建议（按优先级）

### P0（必须先做）

1. **统一注册名与配方名**（二选一）：
   - 方案 A：将各算子改为 `REGISTER_OPERATOR_NAME(PlaneFitOperator, "PlaneFit")` 等；
   - 方案 B：调整 `REGISTER_OPERATOR` 宏，使其用 `OperatorClass().name()` 作为键。

2. **去掉对示例文件的注册依赖**：
   - 新建专用注册入口（例如 `operators/RegisterAllOperators.h/.cpp`），在库初始化时显式调用。

### P1（插件机制真正可用）

3. **把 `PluginLoader` 接入主链**：
   - 建议在 `TaskControlImpl::OpenTask` 或 `AlgorithmTask::open/setConfig` 中调用；
   - 从配置读取插件目录，如：
     ```json
     {
       "pluginDirs": ["./plugins"]
     }
     ```

4. **补齐插件构建与部署**：
   - 将插件工程加入 VS 解决方案，或修正 CMake 并纳入 CI；
   - 构建产物统一拷贝到 `build/bin/<Config>/plugins`。

### P2（工程质量）

5. `PluginLoader` 增加日志与错误回传（`loader->errorString()`）。
6. `TaskControlImpl::OpenTask` 在 `setConfig` 失败时打印 `m_algorithmTask->lastError()`，便于定位具体缺失算子。

---

## 8. 推荐落地路线

### 路线 A（最快恢复可用，先不启用动态插件）

- 先修注册名（P0-1）+
- 添加显式注册入口（P0-2）

优点：改动小、见效快，先保证 `pipeline_example.json` 可跑通。

### 路线 B（完整插件化）

- 在路线 A 基础上继续完成 P1，真正由 DLL 插件提供算子。

优点：符合“插件化加载”目标；缺点：工程改造量更大。

---

## 9. 附：本次分析涉及的关键文件

- `TaskControl/TaskControl.h`
- `TaskControl/TaskControl.cpp`
- `TaskControl/TaskControlImpl.h`
- `TaskControl/TaskControlImpl.cpp`
- `TaskControl/core/AlgorithmTask.h`
- `TaskControl/core/TaskflowExecutor.h`
- `TaskControl/core/OperatorRegistry.h`
- `TaskControl/core/PluginLoader.h`
- `TaskControl/core/OperatorGraph.h`
- `TaskControl/operators/*`
- `Plugins/*`
- `doc/pipeline_example.json`
- `TaskControl.slnx`

---

如果需要，我可以在下一步直接给出“路线 A”的最小代码补丁（一次性修正所有算子注册名 + 增加统一注册入口），并附上验证步骤。