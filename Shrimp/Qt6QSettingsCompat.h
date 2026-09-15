#pragma once

#include <QSettings>

// QSettings::setIniCodec was removed in Qt 6. INI files use UTF-8 there,
// so legacy callers retain their behaviour through this source-local shim.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class AOIQSettings final : public QSettings
{
public:
    using QSettings::QSettings;

    void setIniCodec(const char*) {}
};

#define QSettings AOIQSettings
#endif
