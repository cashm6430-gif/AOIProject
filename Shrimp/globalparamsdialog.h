#ifndef GLOBALPARAMSDIALOG_H
#define GLOBALPARAMSDIALOG_H

#include "datadefine.h"
#include <QDialog>

namespace Ui {
    class GlobalParamsDialog;
}

class GlobalParamsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GlobalParamsDialog(QWidget* parent = nullptr);
    ~GlobalParamsDialog();
signals:
    void sig_updateGParam(const CommonParams& param);

public slots:
    void slt_updateGParam(const CommonParams& param);

private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_rejected();

    void on_checkBoxAutoModify_clicked(bool checked);

    void on_pushButtonSelectExecutePath_clicked();
private:
    Ui::GlobalParamsDialog* ui;
};

#endif // GLOBALPARAMSDIALOG_H
