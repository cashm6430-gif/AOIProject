#pragma once

/**
 * @file AlgorithmIO.h
 * @brief 算法输入输出数据结构
 */

#include "ImageData.h"
#include "PointCloud.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QVector>

namespace AlgorithmSDK {
    /**
     * @brief 测量结果
     */
    struct MeasureResult
    {
        QString name;                   // 测量项名称
        double value = 0.0;             // 测量值
        double nominalValue = 0.0;      // 标称值
        double upperTolerance = 0.0;    // 上公差
        double lowerTolerance = 0.0;    // 下公差
        QString unit = "mm";            // 单位
        int status = 0;                 // 0: OK, 1: NG, -1: Error

        bool isOK() const
        {
            double deviation = value - nominalValue;
            return deviation >= lowerTolerance && deviation <= upperTolerance;
        }

        QJsonObject toJson() const
        {
            QJsonObject obj;
            obj["name"] = name;
            obj["value"] = value;
            obj["nominalValue"] = nominalValue;
            obj["upperTolerance"] = upperTolerance;
            obj["lowerTolerance"] = lowerTolerance;
            obj["unit"] = unit;
            obj["status"] = status;
            return obj;
        }

        static MeasureResult fromJson(const QJsonObject& obj)
        {
            MeasureResult r;
            r.name = obj["name"].toString();
            r.value = obj["value"].toDouble();
            r.nominalValue = obj["nominalValue"].toDouble();
            r.upperTolerance = obj["upperTolerance"].toDouble();
            r.lowerTolerance = obj["lowerTolerance"].toDouble();
            r.unit = obj["unit"].toString("mm");
            r.status = obj["status"].toInt();
            return r;
        }
    };

    /**
     * @brief 算法输入数据
     */
    struct AlgorithmInput
    {
        int workpieceId = 0;                    // 工件ID
        QVector<ImageData> images;              // 图像数据
        QVector<PointCloud> pointclouds;        // 点云数据
        QJsonObject additionalData;             // 附加数据

        bool isEmpty() const
        {
            return images.isEmpty() && pointclouds.isEmpty();
        }

        void clear()
        {
            workpieceId = 0;
            images.clear();
            pointclouds.clear();
            additionalData = QJsonObject();
        }

        QJsonObject toJson() const
        {
            QJsonObject obj;
            obj["workpieceId"] = workpieceId;
            obj["imageCount"] = images.size();
            obj["pointcloudCount"] = pointclouds.size();
            if (!additionalData.isEmpty()) {
                obj["additionalData"] = additionalData;
            }
            return obj;
        }
    };

    /**
     * @brief 算法输出数据
     */
    struct AlgorithmOutput
    {
        bool ok = false;                        // 总体结果
        QString reason;                         // 失败原因
        QVector<MeasureResult> results;         // 测量结果列表
        QJsonObject additionalData;             // 附加输出数据
        qint64 processingTimeMs = 0;            // 处理耗时（毫秒）

        void clear()
        {
            ok = false;
            reason.clear();
            results.clear();
            additionalData = QJsonObject();
            processingTimeMs = 0;
        }

        QJsonObject toJson() const
        {
            QJsonObject obj;
            obj["ok"] = ok;
            obj["reason"] = reason;
            obj["processingTimeMs"] = processingTimeMs;

            QJsonArray resultsArray;
            for (const auto& r : results) {
                resultsArray.append(r.toJson());
            }
            obj["results"] = resultsArray;

            if (!additionalData.isEmpty()) {
                obj["additionalData"] = additionalData;
            }
            return obj;
        }

        static AlgorithmOutput fromJson(const QJsonObject& obj)
        {
            AlgorithmOutput out;
            out.ok = obj["ok"].toBool();
            out.reason = obj["reason"].toString();
            out.processingTimeMs = obj["processingTimeMs"].toVariant().toLongLong();

            QJsonArray arr = obj["results"].toArray();
            for (const auto& v : arr) {
                out.results.append(MeasureResult::fromJson(v.toObject()));
            }

            if (obj.contains("additionalData")) {
                out.additionalData = obj["additionalData"].toObject();
            }
            return out;
        }
    };
} // namespace AlgorithmSDK

Q_DECLARE_METATYPE(AlgorithmSDK::MeasureResult)
Q_DECLARE_METATYPE(AlgorithmSDK::AlgorithmInput)
Q_DECLARE_METATYPE(AlgorithmSDK::AlgorithmOutput)
