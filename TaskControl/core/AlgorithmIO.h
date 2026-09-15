#pragma once

#include "Error.h"
#include "ImageData.h"
#include "PointCloud.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QVector>

namespace AlgorithmSDK {

struct MeasureResult {
    QString name;
    double value = 0.0;
    double nominalValue = 0.0;
    double upperTolerance = 0.0;
    double lowerTolerance = 0.0;
    QString unit = "mm";
    int status = 0;

    bool isOK() const
    {
        const double deviation = value - nominalValue;
        return deviation >= lowerTolerance && deviation <= upperTolerance;
    }

    QJsonObject toJson() const
    {
        return {{"name", name}, {"value", value}, {"nominalValue", nominalValue},
            {"upperTolerance", upperTolerance}, {"lowerTolerance", lowerTolerance},
            {"unit", unit}, {"status", status}};
    }

    static MeasureResult fromJson(const QJsonObject& object)
    {
        MeasureResult result;
        result.name = object.value("name").toString();
        result.value = object.value("value").toDouble();
        result.nominalValue = object.value("nominalValue").toDouble();
        result.upperTolerance = object.value("upperTolerance").toDouble();
        result.lowerTolerance = object.value("lowerTolerance").toDouble();
        result.unit = object.value("unit").toString("mm");
        result.status = object.value("status").toInt();
        return result;
    }
};

struct AlgorithmInput {
    int workpieceId = 0;
    QVector<ImageData> images;
    QVector<PointCloud> pointclouds;
    QJsonObject additionalData;

    bool isEmpty() const { return images.isEmpty() && pointclouds.isEmpty(); }
    void clear()
    {
        workpieceId = 0;
        images.clear();
        pointclouds.clear();
        additionalData = {};
    }
    QJsonObject toJson() const
    {
        QJsonObject object{{"workpieceId", workpieceId}, {"imageCount", images.size()},
            {"pointcloudCount", pointclouds.size()}};
        if (!additionalData.isEmpty()) object["additionalData"] = additionalData;
        return object;
    }
};

struct AlgorithmOutput {
    bool ok = false;
    QString reason;
    QVector<MeasureResult> results;
    ErrorList errors;
    QJsonObject additionalData;
    qint64 processingTimeMs = 0;

    void clear()
    {
        ok = false;
        reason.clear();
        results.clear();
        errors.clear();
        additionalData = {};
        processingTimeMs = 0;
    }

    QJsonObject toJson() const
    {
        QJsonObject object{{"ok", ok}, {"reason", reason}, {"processingTimeMs", processingTimeMs}};
        QJsonArray resultArray;
        for (const MeasureResult& result : results) resultArray.append(result.toJson());
        object["results"] = resultArray;

        QJsonArray errorArray;
        for (const Error& error : errors) {
            errorArray.append(QJsonObject{{"category", static_cast<int>(error.category)},
                {"code", error.code}, {"message", error.message}, {"subject", error.subject}});
        }
        object["errors"] = errorArray;
        if (!additionalData.isEmpty()) object["additionalData"] = additionalData;
        return object;
    }

    static AlgorithmOutput fromJson(const QJsonObject& object)
    {
        AlgorithmOutput output;
        output.ok = object.value("ok").toBool();
        output.reason = object.value("reason").toString();
        output.processingTimeMs = object.value("processingTimeMs").toVariant().toLongLong();
        for (const QJsonValue& value : object.value("results").toArray()) {
            output.results.append(MeasureResult::fromJson(value.toObject()));
        }
        for (const QJsonValue& value : object.value("errors").toArray()) {
            const QJsonObject errorObject = value.toObject();
            output.errors.append({static_cast<ErrorCategory>(errorObject.value("category").toInt()),
                errorObject.value("code").toInt(), errorObject.value("message").toString(),
                errorObject.value("subject").toString()});
        }
        if (object.contains("additionalData")) output.additionalData = object.value("additionalData").toObject();
        return output;
    }
};

} // namespace AlgorithmSDK

Q_DECLARE_METATYPE(AlgorithmSDK::MeasureResult)
Q_DECLARE_METATYPE(AlgorithmSDK::AlgorithmInput)
Q_DECLARE_METATYPE(AlgorithmSDK::AlgorithmOutput)
