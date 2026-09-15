#pragma once

#include "IOperatorPlugin.h"

#include <QList>
#include <QStringList>

class QPluginLoader;

namespace AlgorithmSDK {

class PluginLoader {
public:
    PluginLoader() = default;
    ~PluginLoader();

    void clearLastErrors();
    QStringList lastErrors() const;
    int loadFromDirectory(const QString& dirPath);
    bool loadPlugin(const QString& filePath);
    void unloadAll();
    QStringList loadedPlugins() const;
    int count() const;

private:
    struct PluginInfo {
        QString filePath;
        QPluginLoader* loader = nullptr;
        IOperatorPlugin* plugin = nullptr;
        QStringList operatorNames;
    };

    QList<PluginInfo> m_loadedPlugins;
    QStringList m_lastErrors;
};

} // namespace AlgorithmSDK
