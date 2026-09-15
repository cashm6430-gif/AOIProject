#pragma once

#include "commondefines.h"
#include <QDialog>

namespace Ui {
    class PlatformParamsDlg;
}

/**
 * @brief 平台参数编辑对话框
 *
 * 用于编辑不同运动平台的特定参数（如 UVW、XY_ARCTANR 等）
 */
class PlatformParamsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit PlatformParamsDlg(QWidget* parent = nullptr);
    ~PlatformParamsDlg();

    /**
     * @brief 设置平台参数
     * @param params 平台参数
     */
    void setPlatformParams(const PlatformParams& params);

    /**
     * @brief 获取平台参数
     * @return 平台参数
     */
    PlatformParams getPlatformParams() const;

private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

private:
    Ui::PlatformParamsDlg* ui;
    PlatformParams m_platformParams;

    /**
     * @brief 更新UI显示
     */
    void updateUI();

    /**
     * @brief 从UI读取参数
     */
    void readFromUI();
};