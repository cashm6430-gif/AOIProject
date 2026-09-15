#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "qt_textedit_sink.h"
#include "systemsetting.h"
#include "systemsettingdlg.h"
#include "usermanager.h"
#include "usermanagerdialog.h"
#include "xlogger.h"
#include <QDateTime>
#include <QEvent>
#include <QLabel>
#include <QMessageBox>
#include <QScrollBar>
#include <pcl/io/ply_io.h>
#include <QFileInfo>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 打开/导入/加载按钮的槽按 on_<对象>_<信号> 命名，setupUi 已自动连接；
    // 这里不要再显式 connect，否则一次点击会触发两次（点云被连续载入两次）。


    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<cv::Point2d>("cv::Point2d");
    qRegisterMetaType<cv::Point3d>("cv::Point3d");
    qRegisterMetaType<cv::Point2f>("cv::Point2f");
    qRegisterMetaType<cv::Point3f>("cv::Point3f");
    qRegisterMetaType<cv::Vec3f>("cv::Vec3f");
    qRegisterMetaType<cv::Vec3d>("cv::Vec3d");
    qRegisterMetaType<QVector<float>>("QVector<float>");

    auto appName = QCoreApplication::applicationName();
    auto appVersion = QCoreApplication::applicationVersion();
    setWindowTitle(appName + " - V" + appVersion);

    initStatusBar();
    initLog();

    initPointCloudWidget();

    initImageWidget();

    xInfo("程序启动...");

    initUserActivityMonitor();

    setWindowState(Qt::WindowMaximized | Qt::WindowActive);

    // ========== 初始化并启动外部进程 ==========
    initExternalProcesses();

    xDebug("MainWindow 构造完成");
}

MainWindow::~MainWindow()
{
    // 停止所有外部进程
    stopAllExternalProcesses();

    // 停止并删除定时器
    if (m_userActivityTimer)
    {
        m_userActivityTimer->stop();
        delete m_userActivityTimer;
        m_userActivityTimer = nullptr;
    }

    // 移除事件过滤器
    qApp->removeEventFilter(this);

    delete ui;
}

void MainWindow::initUserActivityMonitor()
{
    // 创建定时器
    m_userActivityTimer = new QTimer(this);
    connect(m_userActivityTimer, &QTimer::timeout, this, &MainWindow::checkUserActivityTimeout);

    // 启动定时器，每分钟检查一次
    m_userActivityTimer->start(CHECK_INTERVAL_MS);

    // 初始化最后活动时间
    m_lastActivityTime.start();

    // 安装全局事件过滤器，监控所有鼠标和键盘事件
    qApp->installEventFilter(this);

    m_isMonitoringUserActivity = true;

    xInfo("用户活动监控已启动 (超时时间: {} 分钟)", ACTIVITY_TIMEOUT_MS / 60000);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // 只在有用户登录时监控活动
    if (m_isMonitoringUserActivity)
    {
        auto userInfo = UserManager::instance().getCurrentUser();
        if (userInfo.userRole >= UserRole::Operator)  // 有用户登录
        {
            // 监控这些事件作为用户活动
            switch (event->type())
            {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseMove:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::Wheel:
            case QEvent::TouchBegin:
            case QEvent::TouchUpdate:
            case QEvent::TouchEnd:
                resetUserActivityTimer();
                break;
            default:
                break;
            }
        }
    }

    // 继续传递事件
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::resetUserActivityTimer()
{
    // 重置计时器
    m_lastActivityTime.restart();
}

void MainWindow::checkUserActivityTimeout()
{
    auto userInfo = UserManager::instance().getCurrentUser();

    // 只有在有用户登录时才检查超时
    if (userInfo.userRole < UserRole::Operator)
    {
        return;  // 没有用户登录，无需检查
    }

    // 获取自上次活动以来经过的时间(毫秒)
    qint64 elapsedTime = m_lastActivityTime.elapsed();

    // 检查是否超时
    if (elapsedTime >= ACTIVITY_TIMEOUT_MS)
    {
        xWarning("用户 {} 无操作超时({} 分钟)，自动注销",
            getUserName(userInfo.userRole),
            ACTIVITY_TIMEOUT_MS / 60000);

        performAutoLogout();
    }
    else
    {
        // 可选：输出调试信息，显示剩余时间
        qint64 remainingTime = ACTIVITY_TIMEOUT_MS - elapsedTime;
        int remainingMinutes = static_cast<int>(remainingTime / 60000);

        xDebug("用户活动检查: 剩余 {} 分钟后超时", remainingMinutes);
    }
}

void MainWindow::performAutoLogout()
{
    auto userInfo = UserManager::instance().getCurrentUser();
    QString userName = getUserName(userInfo.userRole);

    // 注销用户
    UserManager::instance().clearCurrentUser();

    // 显示通知消息
    QMessageBox::warning(this, tr("自动注销"),
        tr("由于 %1 分钟无操作，您已被自动注销。\n\n"
            "用户：%2\n"
            "如需继续操作，请重新登录。")
        .arg(ACTIVITY_TIMEOUT_MS / 60000)
        .arg(userName));

    // 重置活动计时器
    m_lastActivityTime.restart();
}

void MainWindow::initExternalProcesses()
{
    xInfo("初始化外部进程...");

    // 从配置文件读取需要启动的进程列表
    //auto sysSetting = SystemSetting::getInstance().GetSystemSetting();

    // 假设在 SystemSetting 中添加了外部进程配置
    // 您需要在 systemsetting.h 中添加相应的配置项，例如：
    // QVector<QPair<QString, QStringList>> externalProcesses;

    // for (const auto& processConfig : sysSetting.externalProcesses)
    // {
    //     startExternalProcess(processConfig.first, processConfig.second);
    // }

    //startExternalProcess("TestRCF.exe");

    //startExternalProcess("Sorter.exe");

    xInfo("外部进程初始化完成");
}

bool MainWindow::startExternalProcess(const QString& program, const QStringList& arguments)
{
    // ========== 检查进程是否已经在运行 ==========
    // 1. 检查已管理的进程列表
    for (QProcess* existingProcess : m_externalProcesses)
    {
        if (existingProcess && existingProcess->state() != QProcess::NotRunning)
        {
            QString existingProgram = existingProcess->property("program").toString();
            if (existingProgram == program)
            {
                xWarning("外部进程已在运行，跳过启动: {} (PID: {})",
                    program,
                    existingProcess->processId());
                return true;
            }
        }
    }

    // 2. 检查系统中是否已存在同名进程（可选，更严格的检查）
    QString programName = QFileInfo(program).fileName();
    if (isProcessRunning(programName))
    {
        xWarning("系统中已存在同名进程，跳过启动: {}", program);
        return true;
    }

    // ========== 启动新进程 ==========
    QProcess* process = new QProcess(this);

    // 连接信号槽以监控进程状态
    connect(process, &QProcess::started, this, &MainWindow::onProcessStarted);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &MainWindow::onProcessFinished);
    connect(process, &QProcess::errorOccurred, this, &MainWindow::onProcessError);

    // 设置进程属性
    process->setProperty("program", program);

    xInfo("正在启动外部进程: {}", program);

    // 启动进程
    process->start(program, arguments);

    // 等待启动（最多等待3秒）
    if (!process->waitForStarted(3000))
    {
        xError("启动外部进程失败: {}, 错误: {}",
            program,
            process->errorString());
        delete process;
        return false;
    }

    // 保存进程指针
    m_externalProcesses.append(process);

    xInfo("外部进程启动成功: {}, PID: {}",
        program,
        process->processId());

    return true;
}

void MainWindow::stopAllExternalProcesses()
{
    if (m_externalProcesses.isEmpty())
    {
        return;
    }

    xInfo("正在停止所有外部进程...");

    for (QProcess* process : m_externalProcesses)
    {
        if (process && process->state() != QProcess::NotRunning)
        {
            QString program = process->property("program").toString();
            xInfo("正在停止进程: {}", program);

            // 尝试正常终止进程
            process->terminate();

            // 等待进程终止（最多等待3秒）
            if (!process->waitForFinished(3000))
            {
                xWarning("进程未能正常终止，强制结束: {}", program);
                process->kill();
                process->waitForFinished(1000);
            }

            process->deleteLater();
        }
    }

    m_externalProcesses.clear();
    xInfo("所有外部进程已停止");
}

bool MainWindow::isProcessRunning(const QString& processName)
{
#ifdef Q_OS_WIN
    // Windows 平台：使用 tasklist 命令检查进程
    QProcess checkProcess;
    checkProcess.start("tasklist", QStringList() << "/FI" << QString("IMAGENAME eq %1").arg(processName));

    if (!checkProcess.waitForFinished(3000))
    {
        xWarning("检查进程超时: {}", processName);
        return false;
    }

    QString output = QString::fromLocal8Bit(checkProcess.readAllStandardOutput());
    bool isRunning = output.contains(processName, Qt::CaseInsensitive);

    if (isRunning)
    {
        xDebug("在系统中检测到进程: {}", processName);
    }

    return isRunning;

#elif defined(Q_OS_LINUX) || defined(Q_OS_MAC)
    // Linux/Mac 平台：使用 pgrep 命令
    QProcess checkProcess;
    checkProcess.start("pgrep", QStringList() << "-x" << processName);

    if (!checkProcess.waitForFinished(3000))
    {
        xWarning("检查进程超时: {}", processName);
        return false;
    }

    return checkProcess.exitCode() == 0;

#else
    // 其他平台：仅检查已管理的进程
    xWarning("当前平台不支持系统级进程检查");
    return false;
#endif
}

void MainWindow::initPointCloudWidget()
{
    m_pointCloudWidget = new PointCloudWidget(ui->pointCloudWidget);

    auto* layout = new QVBoxLayout(ui->pointCloudWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_pointCloudWidget);

    ui->pointCloudWidget->setLayout(layout);
}

void MainWindow::initImageWidget()
{
    m_imageWidget = new ImageWidget(ui->imageShowWidget);
    m_imageWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* layout = new QVBoxLayout(ui->imageShowWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_imageWidget);

    ui->imageShowWidget->setLayout(layout);
}

void MainWindow::initStatusBar()
{
    // 初始化showdatetime
    m_ShowDateTime = new ShowDateTime();
    // 初始化ShowCPUMemory
    m_ShowCPUMemory = new ShowCPUMemory();
    m_vecprgrssBar.clear();
    for (int i = 0; i < NUMBAR; i++)
    {
        QProgressBar* temp = new QProgressBar();
        m_vecprgrssBar.push_back(temp);
    }

    // 初始化动态条
    m_ShowCPUMemory->SetBar(m_vecprgrssBar.at(0), m_vecprgrssBar.at(1), m_vecprgrssBar.at(2));
    m_ShowCPUMemory->Start(1000);

    // 添加到状态栏
    for (int i = 0; i < 3; i++)
    {
        ui->statusbar->insertWidget(i, m_vecprgrssBar.at(i), 1);
    }

    for (int i = 0; i < 3; i++)
    {
        QLabel* temp = new QLabel();
        m_vecLabel.push_back(temp);
    }

    // 初始化时间
    m_ShowDateTime->SetLab(m_vecLabel.at(1), m_vecLabel.at(2));
    m_ShowDateTime->Start(1000);
    ui->statusbar->insertWidget(3, m_vecLabel.at(0), 1);
    ui->statusbar->insertWidget(4, m_vecLabel.at(1), 1);
    ui->statusbar->insertWidget(5, m_vecLabel.at(2), 1);
}

void MainWindow::initLog()
{
    auto sysSetting = SystemSetting::getInstance().GetSystemSetting();
    auto appPath = QApplication::applicationDirPath();
    XLogger::init(sysSetting.strSaveLogPath.toStdString() + "/daliy_log.log", static_cast<LogLevel>(sysSetting.loglevel));

    // Qt 5.9 需要启用富文本支持
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    ui->textEditAllLog->setAcceptRichText(true);
    ui->textEditCriticalLog->setAcceptRichText(true);
#endif

    auto qt_sink_all_log = std::make_shared<spdlog::sinks::qt_textedit_sink_mt>(ui->textEditAllLog, 1000, true, true);
    qt_sink_all_log->set_level(static_cast<LogLevel>(sysSetting.loglevel));
    qt_sink_all_log->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    XLogger::getInstance()->sinks().push_back(qt_sink_all_log);
    auto qt_sink_critical_log = std::make_shared<spdlog::sinks::qt_textedit_sink_mt>(ui->textEditCriticalLog, 500, true, true);
    qt_sink_critical_log->set_level(spdlog::level::warn);
    qt_sink_critical_log->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    XLogger::getInstance()->sinks().push_back(qt_sink_critical_log);

    // 设置textEdit始终滚动到最后
    connect(ui->textEditAllLog, &QTextEdit::textChanged, [this]() {
        ui->textEditAllLog->verticalScrollBar()->setValue(ui->textEditAllLog->verticalScrollBar()->maximum());
        });
    connect(ui->textEditCriticalLog, &QTextEdit::textChanged, [this]() {
        ui->textEditCriticalLog->verticalScrollBar()->setValue(ui->textEditCriticalLog->verticalScrollBar()->maximum());
        });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    auto uiRet = QMessageBox::information(this, tr("提示"), tr("是否退出系统!"), QMessageBox::Ok | QMessageBox::Cancel);
    if (uiRet == QMessageBox::Ok)
    {
        xInfo("程序退出...");

        qApp->quit();
    }
    else
    {
        xInfo("取消退出...");
        event->ignore();
    }
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionUserLogin_triggered()
{
    xInfo("打开用户登录");
    UserManagerDialog dialog(this);
    dialog.setWindowTitle(tr("用户登录"));

    if (dialog.exec() == QDialog::Accepted)
    {
        // 用户登录成功后，重置活动计时器
        resetUserActivityTimer();
        xInfo("用户登录成功，活动计时器已重置");
    }
}

void MainWindow::on_actionSystem_triggered()
{
    const auto& oldUserInfo = UserManager::instance().getCurrentUser();

    UserManagerDialog dialog(this);
    dialog.setUserRole(UserRole::Admin);
    if (dialog.exec() == QDialog::Accepted)
    {
        auto userInfo = UserManager::instance().getCurrentUser();
        if (userInfo.userRole == UserRole::Admin)
        {
            xInfo("管理员登录成功");

            // 重置活动计时器
            resetUserActivityTimer();

            xInfo("打开系统设置");
            SystemSettingDlg dialog(this);
            dialog.exec();
        }
        else
        {
            xError("请使用管理员权限登录");
            QMessageBox::critical(this, "错误", "请使用管理员权限登录");
        }
    }
    else
    {
        xInfo("用户登录已取消");
    }

    UserManager::instance().setCurrentUser(oldUserInfo);
}

void MainWindow::on_actionAbout_triggered()
{
    xInfo("点击关于");

    QMessageBox::about(this, tr("关于"),
        tr("<h2>Shrimp算法平台</h2>"
            "<p>版本：V%1</p>"
            "<p>作者：Shrimp Team</p>"
            "<p>苏州佳智彩光电科技有限公司 版权所有 &copy; 2017~%2</p>")
        .arg(QCoreApplication::applicationVersion())
        .arg(QDate::currentDate().toString("yyyy")));
}

void MainWindow::on_pushButtonLoadImage_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择 3D 点云文件",
        "E:/Data_3d",
        "Point Cloud Files (*.pcd *.ply);;All Files (*.*)"
    );
    if (filePath.isEmpty()) {
        return;
    }

    QString ext = QFileInfo(filePath).suffix().toLower();
    // 中文路径 → 本地编码(GBK)，否则 PCL 用窄字符打开会失败
    std::string localPath = filePath.toLocal8Bit().toStdString();

    pcl::PointCloud<pcl::PointXYZ> cloud;
    int ret = -1;
    if (ext == "ply") {
        ret = pcl::io::loadPLYFile(localPath, cloud);
    } else {
        ret = pcl::io::loadPCDFile(localPath, cloud);
    }

    if (ret == 0 && !cloud.empty()) {
        xInfo("成功加载点云: {}", filePath);
        ui->tabWidget_2->setCurrentIndex(0);   // 自动切到 3D 页，避免用户在 2D 页时以为没显示
        m_pointCloudWidget->ShowPointCloud(cloud);
    } else {
        xWarning("加载点云失败: {} （格式不支持或路径含中文）", localPath);
    }
}

void MainWindow::on_actionInputCloud_triggered()
{
    on_pushButtonLoadImage_clicked();
}

void MainWindow::on_actionOpen_triggered()
{
    on_pushButtonLoadImage_clicked();
}

void MainWindow::onProcessStarted()
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (process)
    {
        QString program = process->property("program").toString();
        xInfo("进程已启动: {}", program);
    }
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (process)
    {
        QString program = process->property("program").toString();

        if (exitStatus == QProcess::NormalExit)
        {
            xInfo("进程正常退出: {}, 退出代码: {}", program, exitCode);
        }
        else
        {
            xWarning("进程异常退出: {}, 退出代码: {}", program, exitCode);
        }
    }
}

void MainWindow::onProcessError(QProcess::ProcessError error)
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (process)
    {
        QString program = process->property("program").toString();
        QString errorString = process->errorString();

        xError("进程发生错误: {}, 错误类型: {}, 错误信息: {}",
            program,
            static_cast<int>(error),
            errorString);
    }
}
