#pragma once

#include "IOperator.h"
#include "OperatorDescriptor.h"
#include "OperatorRegistry.h"

#include <QList>
#include <QtPlugin>

namespace AlgorithmSDK {

struct PluginOperatorRegistration {
    OperatorDescriptor descriptor;
    OperatorFactory factory;
};

// Version 2 plugin contract: plugins publish metadata and factories, never
// operator instances. Factory invocation must return a new instance each time.
class IOperatorPlugin {
public:
    virtual ~IOperatorPlugin() = default;
    virtual QString pluginName() const = 0;
    virtual QString pluginVersion() const = 0;
    virtual QString pluginDescription() const { return QString(); }
    virtual QList<PluginOperatorRegistration> createOperators() = 0;
    virtual bool initialize() { return true; }
    virtual void cleanup() {}
};

// Kept solely to identify and reject 1.x binaries safely. Their prototype API
// cannot produce independent instances, so loading them would reintroduce P1.
class LegacyOperatorPlugin {
public:
    virtual ~LegacyOperatorPlugin() = default;
    virtual QString pluginName() const = 0;
    virtual QString pluginVersion() const = 0;
    virtual QString pluginDescription() const { return QString(); }
    virtual QList<OperatorPtr> createOperators() = 0;
    virtual bool initialize() { return true; }
    virtual void cleanup() {}
};

} // namespace AlgorithmSDK

#define IOperatorPlugin_IID "com.algorithmSDK.IOperatorPlugin/2.0"
#define LegacyOperatorPlugin_IID "com.algorithmSDK.IOperatorPlugin/1.0"
Q_DECLARE_INTERFACE(AlgorithmSDK::IOperatorPlugin, IOperatorPlugin_IID)
Q_DECLARE_INTERFACE(AlgorithmSDK::LegacyOperatorPlugin, LegacyOperatorPlugin_IID)
