#include "algcontrol.h"
#include "algrecipe.h"
#include "TaskControl.h"
#include <algorithm>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>

using namespace std;
using namespace cv;

QJsonObject AlgControl::Process(const std::vector<cv::Mat>& vecInputData, const QString& workDir)
{
    return QJsonObject();
}

cv::Mat AlgControl::getResultImage() const
{
    return cv::Mat();
}

void AlgControl::loadAlgParams(const QJsonObject& paramObj)
{
    QJsonDocument doc(paramObj);
    AlgRecipe::getInstance().SetRecipe(doc);
}

void AlgControl::setInputData(const std::vector<cv::Mat>& vecInputData)
{}

int AlgControl::process()
{
    return 0;
}

QJsonObject genResultJsonObj(const std::map<std::string, std::any>& data,
    const std::vector<std::string>& fields, int id = -1)
{
    QJsonObject obj;

    // 初始化默认值
    obj["ID"] = id;
    obj["BlockName"] = "";
    obj["Value"] = 0.0;
    obj["Min"] = 0.0;
    obj["Max"] = 0.0;
    obj["ResultState"] = true;  // 默认成功

    bool bOutput = false;
    if (data.count("Output") && data.at("Output").has_value()) {
        try {
            bOutput = std::any_cast<bool>(data.at("Output"));
        }
        catch (const std::bad_any_cast&) {
            bOutput = false;  // 如果类型不匹配，设为 false
        }
    }

    if (bOutput) {
        // 遍历字段并赋值
        for (const auto& field : fields) {
            if (data.count(field) && data.at(field).has_value()) {
                try {
                    if (field == "BlockName") {
                        obj["BlockName"] = QString::fromStdString(std::any_cast<std::string>(data.at(field)));
                    }
                    else if (field == "Value" || field == "Min" || field == "Max") {
                        obj[QString::fromStdString(field)] = std::any_cast<double>(data.at(field));
                    }
                    else if (field == "ResultState") {
                        obj["ResultState"] = std::any_cast<bool>(data.at(field));
                    }
                }
                catch (const std::bad_any_cast&) {
                    // 处理类型转换异常，这里可以做日志记录或者其他操作
                    QMessageBox::warning(nullptr, "Warning", "ResultJsonObj匹配错误!");
                }
            }
        }
    }

    // 仅当有有效数据时才返回该对象，否则返回空对象
    return bOutput ? obj : QJsonObject();
}

QJsonObject AlgControl::genResult()
{
    return QJsonObject();
}