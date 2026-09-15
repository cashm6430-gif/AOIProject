#pragma once

#include "datadefine.h"
#include "opencv2/opencv.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <vector>

class AlgControl
{
public:
    AlgControl() = default;
    QJsonObject Process(const std::vector<cv::Mat>& vecInputData, const QString& workDir);

    cv::Mat getResultImage() const;

private:
    void loadAlgParams(const QJsonObject& paramObj);
    void setInputData(const std::vector<cv::Mat>& vecInputData);
    int process();
    QJsonObject genResult();
};
