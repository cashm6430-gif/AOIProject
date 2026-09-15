#include "PluginLoader.h"

#include "RegistrationScope.h"

#include <QDir>
#include <QPluginLoader>

namespace AlgorithmSDK {

PluginLoader::~PluginLoader()
{
    unloadAll();
}

void PluginLoader::clearLastErrors()
{
    m_lastErrors.clear();
}

QStringList PluginLoader::lastErrors() const
{
    return m_lastErrors;
}

int PluginLoader::loadFromDirectory(const QString& dirPath)
{
    const QDir dir(dirPath);
    if (!dir.exists()) {
        return 0;
    }

#ifdef Q_OS_WIN
    const QStringList filters{"*.dll"};
#else
    const QStringList filters{"*.so", "*.dylib"};
#endif

    int loaded = 0;
    for (const QString& fileName : dir.entryList(filters, QDir::Files)) {
        loaded += loadPlugin(dir.absoluteFilePath(fileName)) ? 1 : 0;
    }
    return loaded;
}

bool PluginLoader::loadPlugin(const QString& filePath)
{
    for (const PluginInfo& info : m_loadedPlugins) {
        if (info.filePath == filePath) {
            return true;
        }
    }

    auto* loader = new QPluginLoader(filePath);
    if (!loader->load()) {
        m_lastErrors.append(QString("Load failed [%1]: %2").arg(filePath, loader->errorString()));
        delete loader;
        return false;
    }

    QObject* instance = loader->instance();
    auto* plugin = qobject_cast<IOperatorPlugin*>(instance);
    if (!plugin) {
        if (qobject_cast<LegacyOperatorPlugin*>(instance)) {
            m_lastErrors.append(QString("Plugin [%1] uses interface 1.0 and was not loaded; rebuild it against IOperatorPlugin 2.0 so every factory returns a new instance.").arg(filePath));
        } else {
            m_lastErrors.append(QString("Interface cast failed [%1]: not an IOperatorPlugin 2.0").arg(filePath));
        }
        loader->unload();
        delete loader;
        return false;
    }

    if (!plugin->initialize()) {
        m_lastErrors.append(QString("Plugin initialize failed [%1]: %2").arg(filePath, plugin->pluginName()));
        loader->unload();
        delete loader;
        return false;
    }

    const QList<PluginOperatorRegistration> registrations = plugin->createOperators();
    RegistrationScope scope;
    QStringList names;
    for (const PluginOperatorRegistration& registration : registrations) {
        if (!registration.descriptor.isValid() || !registration.factory) {
            m_lastErrors.append(QString("Plugin [%1] provided an invalid operator registration").arg(plugin->pluginName()));
            plugin->cleanup();
            loader->unload();
            delete loader;
            return false;
        }
        if (OperatorRegistry::instance().hasOperator(registration.descriptor.name) || names.contains(registration.descriptor.name)) {
            m_lastErrors.append(QString("Plugin [%1] attempted to replace registered operator '%2'").arg(plugin->pluginName(), registration.descriptor.name));
            plugin->cleanup();
            loader->unload();
            delete loader;
            return false;
        }
        names.append(registration.descriptor.name);
        scope.registerOperator(registration.descriptor, registration.factory);
    }
    scope.commit();

    m_loadedPlugins.append({filePath, loader, plugin, names});
    return true;
}

void PluginLoader::unloadAll()
{
    for (PluginInfo& info : m_loadedPlugins) {
        for (const QString& name : info.operatorNames) {
            OperatorRegistry::instance().unregisterOperator(name);
        }
        if (info.plugin) {
            info.plugin->cleanup();
        }
        if (info.loader) {
            info.loader->unload();
            delete info.loader;
        }
    }
    m_loadedPlugins.clear();
}

QStringList PluginLoader::loadedPlugins() const
{
    QStringList result;
    for (const PluginInfo& info : m_loadedPlugins) {
        if (info.plugin) {
            result.append(info.plugin->pluginName());
        }
    }
    return result;
}

int PluginLoader::count() const
{
    return m_loadedPlugins.size();
}

} // namespace AlgorithmSDK
