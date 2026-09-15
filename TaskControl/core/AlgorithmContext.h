#pragma once

/**
 * @file AlgorithmContext.h
 * @brief 算子数据传递上下文（数据总线）
 */

#include "AlgorithmIO.h"
#include "geometry/Geometry.h"
#include "ImageData.h"
#include "PointCloud.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QVariant>

namespace AlgorithmSDK {
    /**
     * @brief 算法上下文类
     * 用于算子之间的数据传递，支持类型安全的 get/set 操作
     */
    class AlgorithmContext
    {
    public:
        AlgorithmContext() = default;
        ~AlgorithmContext() = default;

        // 禁止拷贝，允许移动
        AlgorithmContext(const AlgorithmContext&) = delete;
        AlgorithmContext& operator=(const AlgorithmContext&) = delete;
        AlgorithmContext(AlgorithmContext&&) = default;
        AlgorithmContext& operator=(AlgorithmContext&&) = default;

        /**
         * @brief 设置数据
         */
        template<typename T>
        void set(const QString& key, const T& value)
        {
            QMutexLocker locker(&m_mutex);
            m_data[key] = QVariant::fromValue(value);
        }

        /**
         * @brief 获取数据
         */
        template<typename T>
        T get(const QString& key) const
        {
            QMutexLocker locker(&m_mutex);
            auto it = m_data.find(key);
            if (it == m_data.end()) {
                return T();
            }
            return it.value().value<T>();
        }

        /**
         * @brief 获取数据（带默认值）
         */
        template<typename T>
        T get(const QString& key, const T& defaultValue) const
        {
            QMutexLocker locker(&m_mutex);
            auto it = m_data.find(key);
            if (it == m_data.end()) {
                return defaultValue;
            }
            return it.value().value<T>();
        }

        /**
         * @brief 检查是否存在某个键
         */
        bool has(const QString& key) const
        {
            QMutexLocker locker(&m_mutex);
            return m_data.contains(key);
        }

        /**
         * @brief 移除数据
         */
        void remove(const QString& key)
        {
            QMutexLocker locker(&m_mutex);
            m_data.remove(key);
        }

        /**
         * @brief 清空所有数据
         */
        void clear()
        {
            QMutexLocker locker(&m_mutex);
            m_data.clear();
        }

        /**
         * @brief 获取所有键
         */
        QList<QString> keys() const
        {
            QMutexLocker locker(&m_mutex);
            return m_data.keys();
        }

        /**
         * @brief 获取数据数量
         */
        int count() const
        {
            QMutexLocker locker(&m_mutex);
            return m_data.count();
        }

        // ==================== 便捷方法 ====================

        // 图像相关
        void setImage(const QString& key, const ImageData& image)
        {
            set<ImageData>(key, image);
        }

        ImageData getImage(const QString& key) const
        {
            return get<ImageData>(key);
        }

        // 点云相关
        void setPointCloud(const QString& key, const PointCloud& cloud)
        {
            set<PointCloud>(key, cloud);
        }

        PointCloud getPointCloud(const QString& key) const
        {
            return get<PointCloud>(key);
        }

        // 几何对象相关
        void setPoint3D(const QString& key, const Geometry::Point3D& point)
        {
            set<Geometry::Point3D>(key, point);
        }

        Geometry::Point3D getPoint3D(const QString& key) const
        {
            return get<Geometry::Point3D>(key);
        }

        void setLine3D(const QString& key, const Geometry::Line3D& line)
        {
            set<Geometry::Line3D>(key, line);
        }

        Geometry::Line3D getLine3D(const QString& key) const
        {
            return get<Geometry::Line3D>(key);
        }

        void setPlane(const QString& key, const Geometry::Plane& plane)
        {
            set<Geometry::Plane>(key, plane);
        }

        Geometry::Plane getPlane(const QString& key) const
        {
            return get<Geometry::Plane>(key);
        }

        // 测量结果相关
        void setMeasureResult(const QString& key, const MeasureResult& result)
        {
            set<MeasureResult>(key, result);
        }

        MeasureResult getMeasureResult(const QString& key) const
        {
            return get<MeasureResult>(key);
        }

        // 错误状态
        void setError(const QString& message)
        {
            set<QString>("__error__", message);
            set<bool>("__hasError__", true);
        }

        bool hasError() const
        {
            return get<bool>("__hasError__", false);
        }

        QString getError() const
        {
            return get<QString>("__error__");
        }

        void clearError()
        {
            remove("__error__");
            remove("__hasError__");
        }

    private:
        mutable QMutex m_mutex;
        QHash<QString, QVariant> m_data;
    };
} // namespace AlgorithmSDK
