#pragma once

/**
 * @file PluginLoader.h
 * @brief 插件加载器
 */

#include "IOperatorPlugin.h"
#include "OperatorRegistry.h"
#include <QDir>
#include <QList>
#include <QPluginLoader>
#include <QStringList>

namespace AlgorithmSDK {
    /**
     * @brief 插件加载器
     * 负责加载和管理算子插件
     */
    class PluginLoader
    {
    public:
        PluginLoader() = default;
        ~PluginLoader() { unloadAll(); }

        void clearLastErrors()
        {
            m_lastErrors.clear();
        }

        QStringList lastErrors() const
        {
            return m_lastErrors;
        }

        /**
         * @brief 从目录加载所有插件
         * @param dirPath 插件目录
         * @return 成功加载的插件数量
         */
        int loadFromDirectory(const QString& dirPath)
        {
            QDir dir(dirPath);
            if (!dir.exists()) {
                return 0;
            }

            int count = 0;
            QStringList filters;
#ifdef Q_OS_WIN
            filters << "*.dll";
#else
            filters << "*.so" << "*.dylib";
#endif

            for (const QString& fileName : dir.entryList(filters, QDir::Files)) {
                QString filePath = dir.absoluteFilePath(fileName);
                if (loadPlugin(filePath)) {
                    count++;
                }
            }
            return count;
        }

        /**
         * @brief 加载单个插件
         * @param filePath 插件文件路径
         * @return 是否成功
         */
        bool loadPlugin(const QString& filePath)
        {
            // 检查是否已加载
            for (const auto& info : m_loadedPlugins) {
                if (info.filePath == filePath) {
                    return true;  // 已加载
                }
            }

            QPluginLoader* loader = new QPluginLoader(filePath);
            if (!loader->load()) {
                m_lastErrors.append(QString("Load failed [%1]: %2").arg(filePath, loader->errorString()));
                delete loader;
                return false;
            }

            QObject* instance = loader->instance();
            if (!instance) {
                m_lastErrors.append(QString("Create instance failed [%1]: %2").arg(filePath, loader->errorString()));
                loader->unload();
                delete loader;
                return false;
            }

            IOperatorPlugin* plugin = qobject_cast<IOperatorPlugin*>(instance);
            if (!plugin) {
                m_lastErrors.append(QString("Interface cast failed [%1]: not an IOperatorPlugin").arg(filePath));
                loader->unload();
                delete loader;
                return false;
            }

            // 初始化插件
            if (!plugin->initialize()) {
                m_lastErrors.append(QString("Plugin initialize failed [%1]: %2").arg(filePath, plugin->pluginName()));
                loader->unload();
                delete loader;
                return false;
            }

            // 注册算子
            QList<OperatorPtr> operators = plugin->createOperators();
            for (const auto& op : operators) {
                OperatorRegistry::instance().registerOperator(op->name(),
                    [op]() { return op; });  // 注意：这里共享实例，实际应该创建新实例
            }

            // 保存插件信息
            PluginInfo info;
            info.filePath = filePath;
            info.loader = loader;
            info.plugin = plugin;
            info.operatorNames.reserve(operators.size());
            for (const auto& op : operators) {
                info.operatorNames.append(op->name());
            }

            m_loadedPlugins.append(info);
            return true;
        }

        /**
         * @brief 卸载所有插件
         */
        void unloadAll()
        {
            for (auto& info : m_loadedPlugins) {
                // 注销算子
                for (const QString& name : info.operatorNames) {
                    OperatorRegistry::instance().unregisterOperator(name);
                }

                // 清理插件
                if (info.plugin) {
                    info.plugin->cleanup();
                }

                // 卸载
                if (info.loader) {
                    info.loader->unload();
                    delete info.loader;
                }
            }
            m_loadedPlugins.clear();
        }

        /**
         * @brief 获取已加载的插件列表
         */
        QStringList loadedPlugins() const
        {
            QStringList result;
            for (const auto& info : m_loadedPlugins) {
                result.append(info.plugin->pluginName());
            }
            return result;
        }

        /**
         * @brief 获取已加载插件数量
         */
        int count() const { return m_loadedPlugins.size(); }

    private:
        struct PluginInfo
        {
            QString filePath;
            QPluginLoader* loader = nullptr;
            IOperatorPlugin* plugin = nullptr;
            QStringList operatorNames;
        };

        QList<PluginInfo> m_loadedPlugins;
        QStringList m_lastErrors;
    };
} // namespace AlgorithmSDK
