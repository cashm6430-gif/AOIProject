#pragma once

/**
 * @file PointCloud.h
 * @brief 点云数据结构
 */

#include <cmath>
#include <cstdint>
#include <limits>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QVector>

namespace AlgorithmSDK {
    /**
     * @brief 3D 点（不含强度）
     */
    struct PointXYZ
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        PointXYZ() = default;
        PointXYZ(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

        bool isValid() const
        {
            return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
        }
    };

    /**
     * @brief 3D 点（含强度）
     */
    struct PointXYZI
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float intensity = 0.0f;

        PointXYZI() = default;
        PointXYZI(float x_, float y_, float z_, float i_ = 0.0f)
            : x(x_), y(y_), z(z_), intensity(i_) {
        }

        // 转换为 PointXYZ
        PointXYZ toXYZ() const { return PointXYZ(x, y, z); }

        bool isValid() const
        {
            return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
        }
    };

    /**
     * @brief 点云容器
     * 支持结构化（m×n）和非结构化点云
     */
    class PointCloud
    {
    public:
        PointCloud() = default;

        // 从 PointXYZ 数组构造
        explicit PointCloud(const QVector<PointXYZ>& pts)
            : m_hasIntensity(false), m_width(pts.size()), m_height(1)
        {
            m_pointsXYZI.reserve(pts.size());
            for (const auto& p : pts) {
                m_pointsXYZI.append(PointXYZI(p.x, p.y, p.z, 0.0f));
            }
        }

        // 从 PointXYZI 数组构造
        explicit PointCloud(const QVector<PointXYZI>& pts)
            : m_pointsXYZI(pts), m_hasIntensity(true), m_width(pts.size()), m_height(1)
        {
        }

        // 创建结构化点云
        static PointCloud createOrganized(int width, int height, bool hasIntensity = false)
        {
            PointCloud cloud;
            cloud.m_width = width;
            cloud.m_height = height;
            cloud.m_hasIntensity = hasIntensity;
            cloud.m_pointsXYZI.resize(width * height);
            return cloud;
        }

        // 基本属性
        int width() const { return m_width; }
        int height() const { return m_height; }
        int size() const { return m_pointsXYZI.size(); }
        bool isEmpty() const { return m_pointsXYZI.isEmpty(); }
        bool hasIntensity() const { return m_hasIntensity; }
        bool isOrganized() const { return m_height > 1; }

        // 设置属性
        void setWidth(int w) { m_width = w; }
        void setHeight(int h) { m_height = h; }
        void setHasIntensity(bool v) { m_hasIntensity = v; }

        // 数据访问
        const QVector<PointXYZI>& points() const { return m_pointsXYZI; }
        QVector<PointXYZI>& points() { return m_pointsXYZI; }

        // 索引访问
        const PointXYZI& at(int index) const { return m_pointsXYZI.at(index); }
        PointXYZI& at(int index) { return m_pointsXYZI[index]; }

        const PointXYZI& operator[](int index) const { return m_pointsXYZI[index]; }
        PointXYZI& operator[](int index) { return m_pointsXYZI[index]; }

        // 结构化访问 (row, col)
        const PointXYZI& at(int row, int col) const
        {
            return m_pointsXYZI.at(row * m_width + col);
        }
        PointXYZI& at(int row, int col)
        {
            return m_pointsXYZI[row * m_width + col];
        }

        // 添加点
        void addPoint(const PointXYZ& p)
        {
            m_pointsXYZI.append(PointXYZI(p.x, p.y, p.z, 0.0f));
            if (!isOrganized()) m_width = m_pointsXYZI.size();
        }

        void addPoint(const PointXYZI& p)
        {
            m_pointsXYZI.append(p);
            if (!isOrganized()) m_width = m_pointsXYZI.size();
        }

        void addPoint(float x, float y, float z, float intensity = 0.0f)
        {
            addPoint(PointXYZI(x, y, z, intensity));
        }

        // 清空
        void clear()
        {
            m_pointsXYZI.clear();
            m_width = 0;
            m_height = 1;
        }

        // 预分配空间
        void reserve(int n) { m_pointsXYZI.reserve(n); }
        void resize(int n) { m_pointsXYZI.resize(n); }

        // 获取有效点（过滤无效点）
        PointCloud getValidPoints() const
        {
            PointCloud result;
            result.m_hasIntensity = m_hasIntensity;
            for (const auto& p : m_pointsXYZI) {
                if (p.isValid()) {
                    result.addPoint(p);
                }
            }
            return result;
        }

        // 获取边界框
        void getBoundingBox(PointXYZ& minPt, PointXYZ& maxPt) const
        {
            if (isEmpty()) return;

            minPt = PointXYZ(std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max());
            maxPt = PointXYZ(std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest());

            for (const auto& p : m_pointsXYZI) {
                if (!p.isValid()) continue;
                minPt.x = std::min(minPt.x, p.x);
                minPt.y = std::min(minPt.y, p.y);
                minPt.z = std::min(minPt.z, p.z);
                maxPt.x = std::max(maxPt.x, p.x);
                maxPt.y = std::max(maxPt.y, p.y);
                maxPt.z = std::max(maxPt.z, p.z);
            }
        }

        // JSON 序列化（仅元数据，不含点数据）
        QJsonObject toJsonMeta() const
        {
            QJsonObject obj;
            obj["width"] = m_width;
            obj["height"] = m_height;
            obj["hasIntensity"] = m_hasIntensity;
            obj["pointCount"] = m_pointsXYZI.size();
            return obj;
        }

    private:
        QVector<PointXYZI> m_pointsXYZI;
        bool m_hasIntensity = false;
        int m_width = 0;
        int m_height = 1;  // 默认非结构化
    };
} // namespace AlgorithmSDK

// 注册到 Qt 元类型系统
Q_DECLARE_METATYPE(AlgorithmSDK::PointCloud)
