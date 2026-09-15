#pragma once

#include <QDialog>
#include <QStringList>

namespace Ui
{
    class SystemSettingDlg;
}

class SystemSettingDlg : public QDialog
{
    Q_OBJECT
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
public:
    explicit SystemSettingDlg(QWidget* parent = nullptr);
    ~SystemSettingDlg();

private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_rejected();

    void on_checkBoxSaveSrcImage_clicked(bool checked);

    void on_checkBoxSaveResultImage_clicked(bool checked);

    void on_checkBoxSaveOnlyNgImage_clicked(bool checked);

    void on_radioButtonBmp_clicked();

    void on_radioButtonPng_clicked();

    void on_radioButtonJpg_clicked();

    void on_pushButtonImagePath_clicked();

    void on_pushButtonLogPath_clicked();

    void on_checkBoxAutoDeleteImg_clicked(bool checked);

    void on_spinBoxStationCount_valueChanged(int value);

    void on_pushButtonConfirmStationCount_clicked();

    void on_comboBoxStationList_currentIndexChanged(int index);

    void on_checkBoxEnableStation_stateChanged(int state);

    void on_checkBoxReverseAngle_stateChanged(int state);

    void on_comboBoxOffsetType_currentIndexChanged(int index);

    void on_checkBoxEnableLightControl_stateChanged(int state);

    void on_checkBoxEnablePairedCatchers_stateChanged(int state);

    void on_checkBoxIgnoreMatchAngle_stateChanged(int state);

    void on_comboBoxAlignType_currentIndexChanged(int index);

    void on_checkBoxJudgeExistenceOnly_stateChanged(int state);

    void on_comboBoxCalibType_currentIndexChanged(int index);

    void on_pushButtonPlatformParams_clicked();

    void on_pushButtonConfirmStationParams_clicked();

    void on_lineEditStationName_textChanged(const QString& text);

    void on_comboBoxCameraCount_currentIndexChanged(int index);

    void on_comboBoxPlatformType_currentIndexChanged(int index);

    void on_spinBoxStdPosCount_valueChanged(int value);

    void on_lineEditPulseCountX_textChanged(const QString& text);

    void on_lineEditPulseCountY_textChanged(const QString& text);

    void on_lineEditPulseCountR_textChanged(const QString& text);

private:
    /**
     * @brief 更新工位参数UI显示
     * @param index 工位索引
     */
    void updateStationParamsUI(int index);

private:
    Ui::SystemSettingDlg* ui;
};
