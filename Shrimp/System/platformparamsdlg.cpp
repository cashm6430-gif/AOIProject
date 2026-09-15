#include "platformparamsdlg.h"
#include "ui_platformparamsdlg.h"

#include <QMessageBox>

PlatformParamsDlg::PlatformParamsDlg(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::PlatformParamsDlg)
{
    ui->setupUi(this);

    // 设置窗口为模态对话框
    setModal(true);

    // 初始化默认参数
    m_platformParams.xxyParams.R = 0.0;
    m_platformParams.xxyParams.thetaX1 = 0.0;
    m_platformParams.xxyParams.thetaX2 = 0.0;
    m_platformParams.xxyParams.thetaY = 0.0;

    updateUI();
}

PlatformParamsDlg::~PlatformParamsDlg()
{
    delete ui;
}

void PlatformParamsDlg::setPlatformParams(const PlatformParams& params)
{
    m_platformParams = params;
    updateUI();
}

PlatformParams PlatformParamsDlg::getPlatformParams() const
{
    return m_platformParams;
}

void PlatformParamsDlg::updateUI()
{
    // 更新平台参数
    ui->lineEditR->setText(QString::number(m_platformParams.xxyParams.R, 'f', 2));
    ui->lineEditThetaX1->setText(QString::number(m_platformParams.xxyParams.thetaX1, 'f', 2));
    ui->lineEditThetaX2->setText(QString::number(m_platformParams.xxyParams.thetaX2, 'f', 2));
    ui->lineEditThetaY->setText(QString::number(m_platformParams.xxyParams.thetaY, 'f', 2));
    ui->comboBoxXXYType->setCurrentIndex(m_platformParams.xxyParams.xxyType);

    // 如果是 XYARCTANR 平台，禁用除了R轴以外的参数
    if (m_platformParams.motionPlatform == MotionPlatform::XY_ARCTANR)
    {
        ui->lineEditThetaX1->text().clear();
        ui->lineEditThetaX2->text().clear();
        //ui->lineEditThetaY->text().clear();
        ui->lineEditThetaX1->setEnabled(false);
        ui->lineEditThetaX2->setEnabled(false);
        //ui->lineEditThetaY->setEnabled(false);
        //ui->comboBoxXXYType->setEnabled(false);
    }
}

void PlatformParamsDlg::readFromUI()
{
    // 读取平台参数
    m_platformParams.xxyParams.R = ui->lineEditR->text().toDouble();
    m_platformParams.xxyParams.thetaX1 = ui->lineEditThetaX1->text().toDouble();
    m_platformParams.xxyParams.thetaX2 = ui->lineEditThetaX2->text().toDouble();
    m_platformParams.xxyParams.thetaY = ui->lineEditThetaY->text().toDouble();
    m_platformParams.xxyParams.xxyType = ui->comboBoxXXYType->currentIndex();

    // 如果是 XYARCTANR 平台，禁用除了R轴以外的参数
    if (m_platformParams.motionPlatform == MotionPlatform::XY_ARCTANR)
    {
        m_platformParams.xxyParams.thetaX1 = 0.0;
        m_platformParams.xxyParams.thetaX2 = 0.0;
        //m_platformParams.xxyParams.thetaY = 0.0;
    }
}

void PlatformParamsDlg::on_buttonBox_accepted()
{
    readFromUI();
    accept();
}

void PlatformParamsDlg::on_buttonBox_rejected()
{
    reject();
}