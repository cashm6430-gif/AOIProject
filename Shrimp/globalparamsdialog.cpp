#include "globalparamsdialog.h"
#include "ui_globalparamsdialog.h"

#include "algrecipe.h"
#include <QDir>
#include <QFileDialog>
#include <QSettings>

GlobalParamsDialog::GlobalParamsDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::GlobalParamsDialog)
{
    ui->setupUi(this);

    auto commonParams = AlgRecipe::getInstance().GetAlgParams().commonParams;
    ui->comboBoxMeasureUnit->setCurrentIndex(static_cast<int>(commonParams.measureUnit));
    ui->lineEditPixelDistinction->setText(QString::number(commonParams.pixelDistinction));
    ui->checkBoxAutoModify->setChecked(true);
    ui->lineEditPixelDistinction->setDisabled(true);
    ui->comboBoxMeasureUnit->setDisabled(true);
    ui->pushButtonSelectExecutePath->setDisabled(true);

    // 加载上次设置记录
    QString strName = "history.ini";
    QString strDir = qApp->applicationDirPath() + "/history";
    strName = strDir + "/" + strName;
    QSettings* readSettings = new QSettings(strName, QSettings::IniFormat);
    auto lastExecutePath = readSettings->value("matPlotExecute_Path", "Select Execute Path..").toString();
    delete readSettings;
    ui->lineEditExecutePath->setText(lastExecutePath);
    ui->lineEditExecutePath->setDisabled(true);
}

GlobalParamsDialog::~GlobalParamsDialog()
{
    delete ui;
}

void GlobalParamsDialog::on_buttonBox_accepted()
{
    // 手动修改
    AlgParams algParams = AlgRecipe::getInstance().GetAlgParams();
    CommonParams commonParams;
    commonParams.measureUnit = static_cast<MeasureUnit>(ui->comboBoxMeasureUnit->currentIndex());
    commonParams.pixelDistinction = ui->lineEditPixelDistinction->text().toDouble();

    algParams.commonParams = commonParams;
    AlgRecipe::getInstance().SetAlgParams(algParams);
    emit sig_updateGParam(commonParams);
    close();
}

void GlobalParamsDialog::slt_updateGParam(const CommonParams& param)
{
    AlgParams algParams = AlgRecipe::getInstance().GetAlgParams();
    // 自动修改
    if (ui->checkBoxAutoModify->isChecked()) {
        // 更新配方参数
        algParams.commonParams = param;
        AlgRecipe::getInstance().SetAlgParams(algParams);
    }
}

void GlobalParamsDialog::on_buttonBox_rejected()
{
    close();
}

void GlobalParamsDialog::on_checkBoxAutoModify_clicked(bool checked)
{
    ui->lineEditPixelDistinction->setDisabled(checked);
    ui->comboBoxMeasureUnit->setDisabled(checked);
    ui->lineEditExecutePath->setDisabled(checked);
    ui->pushButtonSelectExecutePath->setDisabled(checked);
}

void GlobalParamsDialog::on_pushButtonSelectExecutePath_clicked()
{
    auto saveHistorySetting = [](QString key, QString value) {
        QString strName = "history.ini";
        QString strDir = qApp->applicationDirPath() + "/history";
        QDir dir(strDir);
        if (!dir.exists()) {
            dir.mkpath(strDir);
        }
        strName = strDir + "/" + strName;
        QSettings* settingIniWrite = new QSettings(strName, QSettings::IniFormat);
        settingIniWrite->setValue(key, value);

        delete settingIniWrite;
        };

    auto currDir = ui->lineEditExecutePath->text();
    QFileInfo fileInfo(currDir);
    auto path = fileInfo.absolutePath();
    QDir dir(path);
    if (!dir.exists()) {
        currDir = "";
    }
    else {
        currDir = path;
    }
    auto pathSelected = QFileDialog::getOpenFileName(this, "选择EXE文件", currDir, "Exe Files(*.exe)");

    if (pathSelected != "")
    {
        ui->lineEditExecutePath->setText(pathSelected);
        saveHistorySetting("matPlotExecute_Path", pathSelected);
    }
}