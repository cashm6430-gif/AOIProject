#pragma once

#include "AlgorithmIO.h"
#include "CancellationToken.h"
#include "Error.h"
#include "geometry/Geometry.h"
#include "ImageData.h"
#include "PointCloud.h"

#include <memory>
#include <QHash>
#include <QReadWriteLock>
#include <QString>
#include <QVariant>

namespace AlgorithmSDK {

// Thread-safe data bus for one execution of one workpiece.
class AlgorithmContext {
public:
    AlgorithmContext() = default;
    ~AlgorithmContext() = default;
    AlgorithmContext(const AlgorithmContext&) = delete;
    AlgorithmContext& operator=(const AlgorithmContext&) = delete;

    template<typename T>
    void set(const QString& key, const T& value)
    {
        QWriteLocker locker(&m_lock);
        m_data[key] = QVariant::fromValue(value);
    }

    template<typename T>
    T get(const QString& key) const
    {
        QReadLocker locker(&m_lock);
        const auto it = m_data.constFind(key);
        return it == m_data.cend() ? T() : it->value<T>();
    }

    template<typename T>
    T get(const QString& key, const T& defaultValue) const
    {
        QReadLocker locker(&m_lock);
        const auto it = m_data.constFind(key);
        return it == m_data.cend() ? defaultValue : it->value<T>();
    }

    bool has(const QString& key) const;
    void remove(const QString& key);
    void clear();
    QStringList keys() const;
    int count() const;

    void setImage(const QString& key, const ImageData& image) { set<ImageData>(key, image); }
    ImageData getImage(const QString& key) const { return get<ImageData>(key); }
    void setPointCloud(const QString& key, const PointCloud& cloud) { set<PointCloud>(key, cloud); }
    PointCloud getPointCloud(const QString& key) const { return get<PointCloud>(key); }
    void setPoint3D(const QString& key, const Geometry::Point3D& point) { set<Geometry::Point3D>(key, point); }
    Geometry::Point3D getPoint3D(const QString& key) const { return get<Geometry::Point3D>(key); }
    void setLine3D(const QString& key, const Geometry::Line3D& line) { set<Geometry::Line3D>(key, line); }
    Geometry::Line3D getLine3D(const QString& key) const { return get<Geometry::Line3D>(key); }
    void setPlane(const QString& key, const Geometry::Plane& plane) { set<Geometry::Plane>(key, plane); }
    Geometry::Plane getPlane(const QString& key) const { return get<Geometry::Plane>(key); }
    void setMeasureResult(const QString& key, const MeasureResult& result) { set<MeasureResult>(key, result); }
    MeasureResult getMeasureResult(const QString& key) const { return get<MeasureResult>(key); }
    QVector<MeasureResult> measureResults() const;

    // Compatibility API. New code should attach a structured Error with
    // pushError(); legacy operators still report through setError().
    void setError(const QString& message);
    void pushError(Error error);
    ErrorList errors() const;
    bool hasError() const;
    QString getError() const;
    void clearError();

    void setCancellationToken(std::shared_ptr<CancellationToken> token);
    bool isCancelled() const;

    void setResource(const QString& key, std::shared_ptr<void> resource);
    template<typename T>
    std::shared_ptr<const T> resource(const QString& key) const
    {
        QReadLocker locker(&m_lock);
        const auto it = m_resources.constFind(key);
        return it == m_resources.cend() ? nullptr : std::static_pointer_cast<const T>(it.value());
    }
    void clearResources();

private:
    mutable QReadWriteLock m_lock;
    QHash<QString, QVariant> m_data;
    QHash<QString, std::shared_ptr<void>> m_resources;
    ErrorList m_errors;
    std::shared_ptr<CancellationToken> m_token;
};

} // namespace AlgorithmSDK
