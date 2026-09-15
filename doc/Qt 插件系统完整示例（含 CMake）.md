### 4.1 插件接口 IOperatorPlugin

```cpp
// ioperator_plugin.hpp
#pragma once
#include <QtPlugin>
#include "ioperator.hpp"

class IOperatorPlugin {
public:
    virtual ~IOperatorPlugin() = default;
    virtual QList<OperatorPtr> createOperators() = 0;
};
#define IOperatorPlugin_iid "com.company.Algorithm.IOperatorPlugin/1.0"
Q_DECLARE_INTERFACE(IOperatorPlugin, IOperatorPlugin_iid)
```

### 4.2 插件实现示例

```cpp
// gaussian_plugin.hpp
#pragma once
#include "ioperator_plugin.hpp"
#include "op_gaussian_blur.hpp"

class GaussianPlugin : public QObject, public IOperatorPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IOperatorPlugin_iid)
    Q_INTERFACES(IOperatorPlugin)
public:
    QList<OperatorPtr> createOperators() override {
        return { OperatorPtr::create<GaussianBlurOperator>() };
    }
};
```

### 4.3 插件 CMake 示例

```cmake
# CMakeLists.txt (for plugin)
cmake_minimum_required(VERSION 3.16)
project(gaussian_plugin LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_library(gaussian_plugin SHARED
    gaussian_plugin.hpp
    gaussian_plugin.cpp
    op_gaussian_blur.hpp
    op_gaussian_blur.cpp
)

target_link_libraries(gaussian_plugin
    PRIVATE Qt6::Core Qt6::Gui
)

target_compile_definitions(gaussian_plugin
    PRIVATE QT_PLUGIN
)
```

### 4.4 主库中加载插件
```cpp
// plugin_loader.hpp
#pragma once
#include <QDir>
#include <QPluginLoader>
#include "operator_registry.hpp"
#include "ioperator_plugin.hpp"

class PluginLoader {
public:
    void loadFromDir(const QString& dirPath, OperatorRegistry& reg) {
        QDir dir(dirPath);
        for (const auto& file : dir.entryList(QDir::Files)) {
            QPluginLoader loader(dir.absoluteFilePath(file));
            QObject* obj = loader.instance();
            if (!obj) continue;
            auto plugin = qobject_cast<IOperatorPlugin*>(obj);
            if (!plugin) continue;

            for (auto& op : plugin->createOperators()) {
                reg.registerOperator(op->name(), [op]() {
                    // 每次创建新实例
                    return OperatorPtr(op->metaObject()->newInstance());
                });
            }
            loaders_.push_back(std::move(loader));
        }
    }

private:
    QVector<QPluginLoader> loaders_;
};
```