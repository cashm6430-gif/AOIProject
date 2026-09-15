#include "ui_usermanagerdialog.h"
#include "usermanagerdialog.h"

#include "usermanager.h"
#include "xlogger.h"
#include <QMessageBox>

UserManagerDialog::UserManagerDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::UserManagerDialog)
{
    ui->setupUi(this);

    auto userInfo = UserManager::instance().getCurrentUser();
    auto index = userInfo.userRole < UserRole::Operator ? 0 : static_cast<int>(userInfo.userRole);
    ui->comboBoxUserName->setCurrentIndex(index);
    ui->labelCurrUserName->setText(getUserName(userInfo.userRole));
}

UserManagerDialog::~UserManagerDialog()
{
    delete ui;
}

void UserManagerDialog::setUserName(const QString& userName)
{
    // 设置用户名到界面上
    ui->comboBoxUserName->setCurrentText(userName);
    // 这里可以根据需要加载用户信息
    //UserManager::instance().loadUserInfo();
}

void UserManagerDialog::setUserRole(UserRole userRole)
{
    // 设置用户名到界面上
    ui->comboBoxUserName->setCurrentIndex(static_cast<int>(userRole));
}

void UserManagerDialog::on_buttonBoxOkCancel_accepted()
{
    // 获取用户输入的用户名和密码
    auto userRole = static_cast<UserRole>(ui->comboBoxUserName->currentIndex());
    auto password = ui->lineEditPassword->text();
    auto bOk = UserManager::instance().checkPassword(userRole, password);
    if (bOk)
    {
        setUserRole(userRole);
        ui->labelCurrUserName->setText(getUserName(userRole));
        UserManager::instance().setCurrentUser({ userRole, password });

        xInfo(" {} 登录成功", getUserName(userRole));

        accept();
    }
    else
    {
        UserManager::instance().clearCurrentUser();
        ui->labelCurrUserName->setText("");
        QMessageBox::warning(this, tr("错误"), tr("用户名或密码错误，请重试！"));
        xError("用户名或密码错误，用户名：{}", getUserName(userRole));
    }
}

void UserManagerDialog::on_buttonBoxOkCancel_rejected()
{
    reject();
}

void UserManagerDialog::on_pushButtonLogout_clicked()
{
    ui->comboBoxUserName->setCurrentIndex(0);
    ui->labelCurrUserName->setText("");

    UserManager::instance().clearCurrentUser();

    xWarning("用户已注销");
}