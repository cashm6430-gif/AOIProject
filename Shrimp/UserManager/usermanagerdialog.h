#ifndef USERMANAGERDIALOG_H
#define USERMANAGERDIALOG_H

#include "usermanager.h"
#include <QDialog>

namespace Ui {
    class UserManagerDialog;
}

class UserManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserManagerDialog(QWidget* parent = nullptr);
    ~UserManagerDialog();

    void setUserName(const QString& userName);
    void setUserRole(UserRole userRole);

private slots:
    void on_buttonBoxOkCancel_accepted();

    void on_buttonBoxOkCancel_rejected();

    void on_pushButtonLogout_clicked();

private:
    Ui::UserManagerDialog* ui;
};

#endif // USERMANAGERDIALOG_H
