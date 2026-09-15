#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "datadefine.h"
#include "ImageWidget.h"
#include "pointcloudwidget.h"

#include "showcpumemory.h"
#include "showdatetime.h"
#include "usermanagerdialog.h"
#include <QElapsedTimer>
#include <QEvent>
#include <QLabel>
#include <QMainWindow>
#include <QProcess>
#include <QTime>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    virtual void closeEvent(QCloseEvent* event) override;

    // 事件过滤器（用于监控用户活动）
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void on_actionExit_triggered();

    void on_actionUserLogin_triggered();

    void on_actionSystem_triggered();

    void on_actionAbout_triggered();

    void on_pushButtonLoadImage_clicked();
    void on_actionInputCloud_triggered();
    void on_actionOpen_triggered();

    // 处理外部进程状态变化
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

    /// <summary>
    /// 检查用户活动超时
    /// </summary>
    void checkUserActivityTimeout();

private:
    // 初始化状态栏
    void initStatusBar();
    // 初始化日志系统
    void initLog();
    // 初始化用户活动监控
    void initUserActivityMonitor();
    /// 重置用户活动计时器
    void resetUserActivityTimer();
    // 执行自动注销
    void performAutoLogout();
    // 初始化外部进程
    void initExternalProcesses();
    // 启动外部进程
    bool startExternalProcess(const QString& program, const QStringList& arguments = QStringList());
    // 停止所有外部进程
    void stopAllExternalProcesses();
    // 检查进程是否运行
    bool isProcessRunning(const QString& processName);

private:
    void initPointCloudWidget();

    void initImageWidget();

private:
    Ui::MainWindow* ui;

    ImageWidget* m_imageWidget = nullptr;
    PointCloudWidget* m_pointCloudWidget = nullptr;

    // 初始化showcpumemory类
    ShowCPUMemory* m_ShowCPUMemory = nullptr;
    // 初始化showdatetime类
    ShowDateTime* m_ShowDateTime = nullptr;
    // 底部动态条
    QVector<QProgressBar*> m_vecprgrssBar;
    QVector<QLabel*> m_vecLabel;
    static constexpr int NUMBAR = 3;

    // 外部进程管理
    QVector<QProcess*> m_externalProcesses;

    // 用户活动监控相关
    QTimer* m_userActivityTimer;                                // 检查超时的定时器
    QElapsedTimer m_lastActivityTime;                           // 记录最后活动时间
    static constexpr int ACTIVITY_TIMEOUT_MS = 20 * 60 * 1000;  // 20分钟(毫秒)
    static constexpr int CHECK_INTERVAL_MS = 60 * 1000;         // 每分钟检查一次
    bool m_isMonitoringUserActivity;                            // 是否正在监控用户活动
};
#endif // MAINWINDOW_H
