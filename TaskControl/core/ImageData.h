#pragma once

/**
 * @file ImageData.h
 * @brief 图像数据封装
 */

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <QHash>
#include <QJsonObject>
#include <QMetaType>
#include <QVariant>

namespace AlgorithmSDK {
    /**
     * @brief 图像数据封装类
      * 基于 cv::Mat，添加 ROI 和元数据支持
     */
    class ImageData
    {
    public:
        ImageData() = default;

        // 从 cv::Mat 构造
        explicit ImageData(const cv::Mat& img) : m_image(img) {}

        // 从文件加载
        static ImageData fromFile(const QString& filePath)
        {
            ImageData data;
            data.m_image = cv::imread(filePath.toStdString(), cv::IMREAD_UNCHANGED);
            return data;
        }

        // 创建空图像
        static ImageData create(int width, int height, int type = CV_8UC1)
        {
            ImageData data;
            data.m_image = cv::Mat::zeros(height, width, type);
            return data;
        }

        // 基本属性
        int width() const { return m_image.cols; }
        int height() const { return m_image.rows; }
        bool isEmpty() const { return m_image.empty(); }
        int format() const { return m_image.type(); }

        // 通道数
        int channels() const
        {
            return m_image.channels();
        }

        // 是否灰度图
        bool isGrayscale() const
        {
            return channels() == 1;
        }

        // cv::Mat 访问
        const cv::Mat& image() const { return m_image; }
        cv::Mat& image() { return m_image; }
        void setImage(const cv::Mat& img) { m_image = img; }

        // ROI 操作
        bool hasROI() const { return m_roi.width > 0 && m_roi.height > 0; }
        cv::Rect roi() const { return m_roi; }
        void setROI(const cv::Rect& rect) { m_roi = rect; }
        void clearROI() { m_roi = cv::Rect(); }

        // 获取 ROI 区域图像
        ImageData getROIImage() const
        {
            if (!hasROI()) return *this;
            ImageData result;
            cv::Rect imageBounds(0, 0, width(), height());
            cv::Rect boundedRoi = m_roi & imageBounds;
            if (boundedRoi.width <= 0 || boundedRoi.height <= 0) {
                return result;
            }
            result.m_image = m_image(boundedRoi).clone();
            result.m_roi = boundedRoi;
            return result;
        }

        // 转换为灰度图
        ImageData toGrayscale() const
        {
            if (isGrayscale()) return *this;
            ImageData result;
            if (m_image.channels() == 3) {
                cv::cvtColor(m_image, result.m_image, cv::COLOR_BGR2GRAY);
            }
            else if (m_image.channels() == 4) {
                cv::cvtColor(m_image, result.m_image, cv::COLOR_BGRA2GRAY);
            }
            else {
                result.m_image = m_image.clone();
            }
            result.m_roi = m_roi;
            return result;
        }

        // 像素访问
        uchar* scanLine(int y) { return m_image.ptr<uchar>(y); }
        const uchar* scanLine(int y) const { return m_image.ptr<uchar>(y); }
        const uchar* constScanLine(int y) const { return m_image.ptr<uchar>(y); }

        // 元数据
        void setMetadata(const QString& key, const QVariant& value) { m_metadata[key] = value; }
        QVariant metadata(const QString& key) const { return m_metadata.value(key); }
        bool hasMetadata(const QString& key) const { return m_metadata.contains(key); }

        // JSON 序列化（仅元数据）
        QJsonObject toJsonMeta() const
        {
            QJsonObject obj;
            obj["width"] = width();
            obj["height"] = height();
            obj["channels"] = channels();
            obj["format"] = m_image.type();
            if (hasROI()) {
                QJsonObject roiObj;
                roiObj["x"] = m_roi.x;
                roiObj["y"] = m_roi.y;
                roiObj["width"] = m_roi.width;
                roiObj["height"] = m_roi.height;
                obj["roi"] = roiObj;
            }
            return obj;
        }

    private:
        cv::Mat m_image;
        cv::Rect m_roi;
        QHash<QString, QVariant> m_metadata;
    };
} // namespace AlgorithmSDK

Q_DECLARE_METATYPE(AlgorithmSDK::ImageData)
