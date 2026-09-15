#include "systemsettingdlg.h"
#include "ui_systemsettingdlg.h"

#include "deleteimage.h"
#include "platformparamsdlg.h"
#include "systemsetting.h"
#include <QColorDialog>
#include <QDesktopServices>
#include <QMenu>
#include <QUrl>

extern DeleteImage delImg;

bool SystemSettingDlg::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Wheel)
    {
        if (obj == ui->spinBoxRemainSpace || obj == ui->spinBoxCheckTimeInterval || obj == ui->spinBoxDeleteTimeInterval)
        {
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

SystemSettingDlg::SystemSettingDlg(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SystemSettingDlg)
{
    ui->setupUi(this);

    // 系统配置信息
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    ui->checkBoxSaveSrcImage->setChecked(sysSetting.bSaveSrcImage);
    ui->checkBoxSaveResultImage->setChecked(sysSetting.bSaveResultImage);
    ui->checkBoxSaveOnlyNgImage->setChecked(sysSetting.bSaveOnlyNgImage);

    switch (sysSetting.saveFormat)
    {
    case ImageSaveFormat::BMP:
        ui->radioButtonBmp->setChecked(true);
        break;
    case ImageSaveFormat::PNG:
        ui->radioButtonPng->setChecked(true);
        break;
    case ImageSaveFormat::JPG:
        ui->radioButtonJpg->setChecked(true);
        break;
    default:
        ui->radioButtonBmp->setChecked(true);
        break;
    }

    ui->lineEditImagePath->setText(sysSetting.strSaveImagePath);
    ui->lineEditLogPath->setText(sysSetting.strSaveLogPath);
    ui->comboBoxLogLevel->setCurrentIndex(sysSetting.loglevel);

    // 自动删除图片配置加载
    ui->checkBoxAutoDeleteImg->setChecked(sysSetting.autoDeleteImg);
    ui->spinBoxCheckTimeInterval->setValue(sysSetting.checkTimeInterval);
    ui->spinBoxDeleteTimeInterval->setValue(sysSetting.deleteTimeInterval);
    ui->spinBoxRemainSpace->setValue(sysSetting.remainSpace);

    // 工位参数配置加载
    ui->spinBoxStationCount->setValue(sysSetting.stationCount);
    ui->comboBoxStationList->clear();
    for (int i = 0; i < sysSetting.stationCount; i++)
    {
        ui->comboBoxStationList->addItem(QString("工位%1").arg(i + 1));
    }

    if (sysSetting.stationCount > 0)
    {
        updateStationParamsUI(0);
    }

    // 添加事件过滤器，屏蔽滚轴事件
    ui->spinBoxRemainSpace->installEventFilter(this);
    ui->spinBoxCheckTimeInterval->installEventFilter(this);
    ui->spinBoxDeleteTimeInterval->installEventFilter(this);
}

SystemSettingDlg::~SystemSettingDlg()
{
    delete ui;
}

void SystemSettingDlg::on_buttonBox_accepted()
{
    // 保存系统配置信息
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.strSaveImagePath = ui->lineEditImagePath->text();
    sysSetting.strSaveLogPath = ui->lineEditLogPath->text();
    sysSetting.loglevel = ui->comboBoxLogLevel->currentIndex();

    // 保存自动删除图片设置
    sysSetting.autoDeleteImg = ui->checkBoxAutoDeleteImg->checkState();
    sysSetting.remainSpace = ui->spinBoxRemainSpace->value();
    sysSetting.checkTimeInterval = ui->spinBoxCheckTimeInterval->value();
    sysSetting.deleteTimeInterval = ui->spinBoxDeleteTimeInterval->value();

    SystemSetting::getInstance().SetSystemSetting(sysSetting);
    SystemSetting::getInstance().SaveSystemSetting(sysSetting);

    QMessageBox::information(this, "information", tr("参数设置成功，重启软件以确保配置生效！"), QMessageBox::Ok);

    if (sysSetting.autoDeleteImg)
    {
        if (!delImg.delImgRunning)
        {
            delImg.delImgRunning = true;
            delImg.start();
        }
    }

    accept();
}

void SystemSettingDlg::on_buttonBox_rejected()
{
    reject();
}

void SystemSettingDlg::on_checkBoxSaveSrcImage_clicked(bool checked)
{
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.bSaveSrcImage = checked;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_checkBoxSaveResultImage_clicked(bool checked)
{
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.bSaveResultImage = checked;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_checkBoxSaveOnlyNgImage_clicked(bool checked)
{
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.bSaveOnlyNgImage = checked;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_radioButtonBmp_clicked()
{
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.saveFormat = ImageSaveFormat::BMP;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_radioButtonPng_clicked()
{
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.saveFormat = ImageSaveFormat::PNG;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_radioButtonJpg_clicked()
{
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.saveFormat = ImageSaveFormat::JPG;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_pushButtonImagePath_clicked()
{
    QString srcDirPath = QFileDialog::getExistingDirectory(this, tr("选择原图保存目录"), "/");
    if (srcDirPath.isEmpty() || srcDirPath.isNull())
    {
        return;
    }
    ui->lineEditImagePath->setText(srcDirPath);

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.strSaveImagePath = srcDirPath;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_pushButtonLogPath_clicked()
{
    QString logDirPath = QFileDialog::getExistingDirectory(this, tr("选择Log保存目录"), "/");
    if (logDirPath.isEmpty() || logDirPath.isNull())
    {
        return;
    }
    ui->lineEditLogPath->setText(logDirPath);

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.strSaveLogPath = logDirPath;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_checkBoxAutoDeleteImg_clicked(bool checked)
{
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.autoDeleteImg = checked;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_spinBoxStationCount_valueChanged(int value)
{
    // 启用/禁用确认按钮
    ui->pushButtonConfirmStationCount->setEnabled(value > 0);

    // 如果值小于1，不进行后续处理
    if (value < 1)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 获取当前工位参数列表
    auto vecStationParams = sysSetting.vecStationParams;
    int currentSize = vecStationParams.size();

    // 如果新的工位数量大于当前数量，添加新工位
    if (value > currentSize)
    {
        for (int i = currentSize; i < value; ++i)
        {
            StationParams stStationParams;
            stStationParams.stationID = i;
            stStationParams.stationName = QString("工位%1").arg(i + 1);
            stStationParams.bEnable = true;
            stStationParams.cameraCount = 1;
            stStationParams.offsetCount = 1;
            stStationParams.reverseAngle = false;
            stStationParams.enableLightControl = false;
            stStationParams.lightDelayMs = 20;
            stStationParams.enablePairedCatchers = false;
            stStationParams.ignoreMatchAngle = true;
            stStationParams.alignType = AlignType::ProductAlignment;
            stStationParams.judgeExistenceOnly = false;
            stStationParams.calibType = CalibType::EyeToHand;

            // 设置默认平台参数
            stStationParams.platformParams.motionPlatform = MotionPlatform::XYR;
            stStationParams.platformParams.pulsesPerMM.x = 1000.0;
            stStationParams.platformParams.pulsesPerMM.y = 1000.0;
            stStationParams.platformParams.pulsesPerMM.r = 1000.0;

            vecStationParams.append(stStationParams);
        }
    }
    // 如果新的工位数量小于当前数量，移除多余的工位
    else if (value < currentSize)
    {
        auto deleteCount = currentSize - value;
        QString deleteStationText;
        for (int i = 0; i < deleteCount; ++i)
        {
            deleteStationText.prepend(QString("工位%1").arg(currentSize - i));
            if (i < deleteCount - 1)
            {
                deleteStationText.prepend(", ");
            }
        }

        auto deleteStationMsg = tr("将删除以下工位配置：%1").arg(deleteStationText);

        // 询问用户是否确认删除
        auto reply = QMessageBox::question(this,
            "确认操作",
            tr("减少工位数量将删除多余的工位配置，是否继续？\n（%1）")
            .arg(deleteStationMsg),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            // 用户取消，恢复原值
            ui->spinBoxStationCount->blockSignals(true);
            ui->spinBoxStationCount->setValue(currentSize);
            ui->spinBoxStationCount->blockSignals(false);
            return;
        }

        // 删除多余的工位
        while (vecStationParams.size() > value)
        {
            vecStationParams.removeLast();
        }
    }

    // 更新系统设置
    sysSetting.stationCount = value;
    sysSetting.vecStationParams = vecStationParams;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);

    // 更新下拉列表
    int currentSelectedIndex = ui->comboBoxStationList->currentIndex();
    ui->comboBoxStationList->blockSignals(true);
    ui->comboBoxStationList->clear();

    for (int i = 0; i < value; ++i)
    {
        // 使用实际的工位名称
        if (i < vecStationParams.size())
        {
            ui->comboBoxStationList->addItem(vecStationParams[i].stationName);
        }
        else
        {
            ui->comboBoxStationList->addItem(QString("工位%1").arg(i + 1));
        }
    }

    ui->comboBoxStationList->blockSignals(false);

    // 恢复选中状态或选择第一个工位
    if (value > 0)
    {
        if (currentSelectedIndex >= 0 && currentSelectedIndex < value)
        {
            ui->comboBoxStationList->setCurrentIndex(currentSelectedIndex);
        }
        else
        {
            ui->comboBoxStationList->setCurrentIndex(0);
        }

        updateStationParamsUI(ui->comboBoxStationList->currentIndex());
    }
}

void SystemSettingDlg::on_pushButtonConfirmStationCount_clicked()
{
    auto stationCount = ui->spinBoxStationCount->value();
    if (stationCount < 1)
    {
        QMessageBox::warning(this, "warning", tr("工位数必须大于等于1!"), QMessageBox::Ok);
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    sysSetting.stationCount = stationCount;

    ui->comboBoxStationList->clear();
    for (int i = 0; i < stationCount; i++)
    {
        ui->comboBoxStationList->addItem(QString("工位%1").arg(i + 1));
    }

    auto vecStationParams = sysSetting.vecStationParams;
    if (vecStationParams.size() < stationCount)
    {
        for (int i = vecStationParams.size(); i < stationCount; i++)
        {
            StationParams stStationParams;
            stStationParams.stationID = i;
            stStationParams.stationName = QString("工位%1").arg(i + 1);
            // 设置默认平台参数
            stStationParams.platformParams.motionPlatform = MotionPlatform::XYR;
            stStationParams.platformParams.pulsesPerMM.x = 1000.0;
            stStationParams.platformParams.pulsesPerMM.y = 1000.0;
            stStationParams.platformParams.pulsesPerMM.r = 1000.0;
            vecStationParams.append(stStationParams);
            vecStationParams.append(stStationParams);
        }
    }

    sysSetting.vecStationParams = vecStationParams;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_comboBoxStationList_currentIndexChanged(int index)
{
    // 切换工位参数显示
    updateStationParamsUI(index);
}

void SystemSettingDlg::on_checkBoxEnableStation_stateChanged(int state)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 更新工位使能状态
    sysSetting.vecStationParams[stationIndex].bEnable = (state == Qt::Checked);

    // 保存到系统设置
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_checkBoxReverseAngle_stateChanged(int state)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();
    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新旋转方向取反状态
    sysSetting.vecStationParams[stationIndex].reverseAngle = (state == Qt::Checked);

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_comboBoxOffsetType_currentIndexChanged(int index)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新补偿类型
    sysSetting.vecStationParams[stationIndex].offsetType = static_cast<OffsetType>(index);

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_checkBoxEnableLightControl_stateChanged(int state)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();
    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新光源控制使能状态
    sysSetting.vecStationParams[stationIndex].enableLightControl = (state == Qt::Checked);

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_checkBoxEnablePairedCatchers_stateChanged(int state)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();
    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新启用双夹爪使能状态
    sysSetting.vecStationParams[stationIndex].enablePairedCatchers = (state == Qt::Checked);

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_checkBoxIgnoreMatchAngle_stateChanged(int state)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();
    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新忽略模板匹配角度使能状态
    sysSetting.vecStationParams[stationIndex].ignoreMatchAngle = (state == Qt::Checked);

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_comboBoxAlignType_currentIndexChanged(int index)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新对位场景类型
    sysSetting.vecStationParams[stationIndex].alignType = static_cast<AlignType>(index);

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_checkBoxJudgeExistenceOnly_stateChanged(int state)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();
    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新仅判断存在与否使能状态
    sysSetting.vecStationParams[stationIndex].judgeExistenceOnly = (state == Qt::Checked);

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_comboBoxCalibType_currentIndexChanged(int index)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新标定类型
    sysSetting.vecStationParams[stationIndex].calibType = static_cast<CalibType>(index);

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_pushButtonPlatformParams_clicked()
{
}

void SystemSettingDlg::on_pushButtonConfirmStationParams_clicked()
{
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0 || stationIndex >= sysSetting.vecStationParams.size())
    {
        QMessageBox::warning(this, "warning", tr("工位索引无效!"), QMessageBox::Ok);
        return;
    }

    // 从已有的工位参数复制，保留其他配置
    StationParams stationParams = sysSetting.vecStationParams[stationIndex];

    // 更新可编辑的参数
    stationParams.bEnable = ui->checkBoxEnableStation->isChecked();
    stationParams.stationName = ui->lineEditStationName->text();
    stationParams.cameraCount = ui->comboBoxCameraCount->currentIndex() + 1;
    stationParams.offsetCount = ui->spinBoxOffsetCount->value();
    stationParams.reverseAngle = ui->checkBoxReverseAngle->isChecked();
    stationParams.offsetType = static_cast<OffsetType>(ui->comboBoxOffsetType->currentIndex());
    stationParams.enableLightControl = ui->checkBoxEnableLightControl->isChecked();
    stationParams.lightDelayMs = ui->lineEditLightDelayMs->text().toInt();
    stationParams.enablePairedCatchers = ui->checkBoxEnablePairedCatchers->isChecked();
    stationParams.ignoreMatchAngle = ui->checkBoxIgnoreMatchAngle->isChecked();
    stationParams.alignType = static_cast<AlignType>(ui->comboBoxAlignType->currentIndex());
    stationParams.judgeExistenceOnly = ui->checkBoxJudgeExistenceOnly->isChecked();
    stationParams.calibType = static_cast<CalibType>(ui->comboBoxCalibType->currentIndex());
    stationParams.platformParams.motionPlatform = static_cast<MotionPlatform>(ui->comboBoxPlatformType->currentIndex());
    stationParams.platformParams.pulsesPerMM.x = ui->lineEditPulseCountX->text().toDouble();
    stationParams.platformParams.pulsesPerMM.y = ui->lineEditPulseCountY->text().toDouble();
    stationParams.platformParams.pulsesPerMM.r = ui->lineEditPulseCountR->text().toDouble();

    // 保存更新后的工位参数
    sysSetting.vecStationParams[stationIndex] = stationParams;
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
    SystemSetting::getInstance().SaveSystemSetting(sysSetting);

    QMessageBox::information(this, "information", tr("工位 %1 参数保存成功!").arg(stationIndex + 1), QMessageBox::Ok);
}

void SystemSettingDlg::on_lineEditStationName_textChanged(const QString& text)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新工位名称到对应的 StationParams
    sysSetting.vecStationParams[stationIndex].stationName = text;

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_comboBoxCameraCount_currentIndexChanged(int index)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新相机数量（index + 1，因为 comboBox 索引从 0 开始）
    sysSetting.vecStationParams[stationIndex].cameraCount = index + 1;

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_comboBoxPlatformType_currentIndexChanged(int index)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新平台类型
    sysSetting.vecStationParams[stationIndex].platformParams.motionPlatform =
        static_cast<MotionPlatform>(index);

    // 根据平台类型启用/禁用参数按钮
    if (ui->comboBoxPlatformType->currentIndex() == 0)
    {
        ui->pushButtonPlatformParams->setEnabled(false);
    }
    else
    {
        ui->pushButtonPlatformParams->setEnabled(true);
    }

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_spinBoxStdPosCount_valueChanged(int value)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 实时更新标准位数量
    sysSetting.vecStationParams[stationIndex].offsetCount = value;

    // 保存到系统设置（内存中）
    SystemSetting::getInstance().SetSystemSetting(sysSetting);
}

void SystemSettingDlg::on_lineEditPulseCountX_textChanged(const QString& text)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 将文本转换为 double，如果转换失败则使用默认值
    bool ok = false;
    double value = text.toDouble(&ok);

    if (ok)
    {
        // 实时更新 X 轴脉冲数
        sysSetting.vecStationParams[stationIndex].platformParams.pulsesPerMM.x = value;

        // 保存到系统设置（内存中）
        SystemSetting::getInstance().SetSystemSetting(sysSetting);
    }
}

void SystemSettingDlg::on_lineEditPulseCountY_textChanged(const QString& text)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 将文本转换为 double，如果转换失败则使用默认值
    bool ok = false;
    double value = text.toDouble(&ok);

    if (ok)
    {
        // 实时更新 Y 轴脉冲数
        sysSetting.vecStationParams[stationIndex].platformParams.pulsesPerMM.y = value;

        // 保存到系统设置（内存中）
        SystemSetting::getInstance().SetSystemSetting(sysSetting);
    }
}

void SystemSettingDlg::on_lineEditPulseCountR_textChanged(const QString& text)
{
    // 获取当前选中的工位索引
    auto stationIndex = ui->comboBoxStationList->currentIndex();

    if (stationIndex < 0)
    {
        return;
    }

    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 检查工位索引是否有效
    if (stationIndex >= sysSetting.vecStationParams.size())
    {
        return;
    }

    // 将文本转换为 double，如果转换失败则使用默认值
    bool ok = false;
    double value = text.toDouble(&ok);

    if (ok)
    {
        // 实时更新 R 轴（旋转）脉冲数
        sysSetting.vecStationParams[stationIndex].platformParams.pulsesPerMM.r = value;

        // 保存到系统设置（内存中）
        SystemSetting::getInstance().SetSystemSetting(sysSetting);
    }
}

void SystemSettingDlg::updateStationParamsUI(int index)
{
    // 切换工位参数显示
    auto vecStationParams = SystemSetting::getInstance().GetSystemSetting().vecStationParams;
    if (index < 0 || index >= vecStationParams.size())
    {
        return;
    }

    auto stationParams = vecStationParams.at(index);
    ui->checkBoxEnableStation->setChecked(stationParams.bEnable);
    ui->lineEditStationName->setText(stationParams.stationName);
    ui->comboBoxCameraCount->setCurrentIndex(stationParams.cameraCount - 1);
    ui->spinBoxOffsetCount->setValue(stationParams.offsetCount);
    ui->checkBoxReverseAngle->setChecked(stationParams.reverseAngle);
    ui->comboBoxOffsetType->setCurrentIndex(static_cast<int>(stationParams.offsetType));
    ui->checkBoxEnableLightControl->setChecked(stationParams.enableLightControl);
    ui->lineEditLightDelayMs->setText(QString::number(stationParams.lightDelayMs));
    ui->checkBoxEnablePairedCatchers->setChecked(stationParams.enablePairedCatchers);
    ui->checkBoxIgnoreMatchAngle->setChecked(stationParams.ignoreMatchAngle);
    ui->comboBoxAlignType->setCurrentIndex(static_cast<int>(stationParams.alignType));
    ui->checkBoxJudgeExistenceOnly->setChecked(stationParams.judgeExistenceOnly);
    ui->comboBoxCalibType->setCurrentIndex(static_cast<int>(stationParams.calibType));

    // 从 platformParams 中读取
    ui->comboBoxPlatformType->setCurrentIndex(static_cast<int>(stationParams.platformParams.motionPlatform));
    ui->lineEditPulseCountX->setText(QString::number(stationParams.platformParams.pulsesPerMM.x, 'f', 2));
    ui->lineEditPulseCountY->setText(QString::number(stationParams.platformParams.pulsesPerMM.y, 'f', 2));
    ui->lineEditPulseCountR->setText(QString::number(stationParams.platformParams.pulsesPerMM.r, 'f', 2));

    if (ui->comboBoxPlatformType->currentIndex() == 0)
    {
        ui->pushButtonPlatformParams->setEnabled(false);
    }
    else
    {
        ui->pushButtonPlatformParams->setEnabled(true);
    }
}