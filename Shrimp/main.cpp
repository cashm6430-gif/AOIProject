#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QMessageBox>
#include <QSharedMemory>
#include <QSurfaceFormat>
#include <qvtkopenglnativewidget.h>
#include <QVersionNumber>
#include "pointcloudwidget.h"
#include "logger/xlogger.h"
#include <iostream>
#include <pcl/point_types.h>

void setupHighDpiSupport()
{
    // ---------------------------------------------------------
    // Qt 6.0 及以上
    // Qt 6 默认启用高 DPI 缩放，AA_EnableHighDpiScaling 已移除。
    // ---------------------------------------------------------
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        // Qt 6 中高 DPI 默认启用，无需额外设置
        // 如需强制指定 DPI，可使用环境变量：
        // qputenv("QT_FONT_DPI", "96");
    qDebug() << "[DPI] Running on Qt6, default high DPI handling enabled.";

    // ---------------------------------------------------------
    // Qt 5.14 及以上
    // 引入非整数缩放支持和 HighDpiScaleFactorRoundingPolicy。
    // ---------------------------------------------------------
#elif QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    qDebug() << "[DPI] Running on Qt 5.14+, PassThrough policy enabled.";

    // ---------------------------------------------------------
    // Qt 5.6 到 5.13
    // 使用传统的 Attribute 方式，AA_EnableHighDpiScaling 在此版本引入。
    // ---------------------------------------------------------
#elif QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    qDebug() << "[DPI] Running on Qt 5.6-5.13, legacy attribute mode enabled.";

    // ---------------------------------------------------------
    // Qt 5.6 以下：基本不支持自动高 DPI
    // ---------------------------------------------------------
#else
    qDebug() << "[DPI] Qt version is too old (< 5.6), high DPI support is limited.";
#endif
}

// ---- --selftest：真实端到端自检（加载→框选ROI→测量→结果面板），供无人工验证 ----
static int PointCloudWidgetSelfTest(QApplication&)
{
    XLogger::init("selftest.log", LogLevel::info);

    pcl::PointCloud<pcl::PointXYZ> cloud;
    for (int i = 0; i < 600; ++i) {
        float x = (i % 30) * 0.02f - 0.30f;
        float y = (i / 30) * 0.02f - 0.20f;
        cloud.points.push_back({ x, y, static_cast<float>(i % 7) * 0.0001f + 0.02f });
    }

    PointCloudWidget w;
    w.resize(820, 640);
    w.show();
    w.ShowPointCloud(cloud);

    // 模拟用户点击「框选ROI」→「测量」
    QMetaObject::invokeMethod(&w, "on_roiButton_clicked", Qt::DirectConnection);
    QMetaObject::invokeMethod(&w, "on_measureButton_clicked", Qt::DirectConnection);

    int rows = w.resultRowCount();
    QString summary = w.resultSummary();
    // Flush explicitly instead of relying on the CRT's exit-time flush.
    //
    // `\n` only ends the line: the bytes stay in stdout's buffer until the
    // stream is flushed when the process leaves through the normal C runtime
    // path -- and this process can leave without that happening.  Measured on
    // this build: runs that returned 0 (which is `rows > 0`, i.e. the whole
    // load -> ROI -> measure path had worked) sometimes printed none of the
    // three lines below.  That is invisible by hand but fatal to automation:
    // `scripts/build-debug.sh run` and `scripts/verify-bundle.sh run` decide by
    // grepping for "END-TO-END PASS", so they reported a failure for a run that
    // had passed.  std::endl flushes; the extra call keeps that true even if
    // the lines above are edited later.
    std::cout << "[selftest] result table rows = " << rows << std::endl;
    std::cout << "[selftest] summary = " << summary.toStdString() << std::endl;
    std::cout << "[selftest] => " << (rows > 0 ? "END-TO-END PASS" : "END-TO-END FAIL")
              << std::endl;
    std::cout.flush();
    return (rows > 0) ? 0 : 1;
}


int main(int argc, char* argv[])
{
    //// 初始化软件授权模块
    //HardwareUUID hwUUID("GVisionPro");
    //if (initModule("AOI", hwUUID.getUUID().data()))
    //    return 0;
    //// 检查授权
    //if (!checkAuthorisation())
    //    return 0;

        // 启用高 DPI 支持
    setupHighDpiSupport();

    // QVTKOpenGLNativeWidget 需在 QApplication 构造前设置兼容的默认 OpenGL surface format，
    // 否则 3D 点云视图整片空白（背景、坐标轴都不渲染）。
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication a(argc, argv);

    // --selftest：不进入主界面，直接跑真实端到端自检
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == "--selftest") {
            return PointCloudWidgetSelfTest(a);
        }
    }

    // 加载 QSS 主题文件：优先 exe 所在目录，其次工作目录（保证双击 exe 也能加载到主题）
    QFile qssFile(QApplication::applicationDirPath() + "/Themes/dark_industrial.qss");
    if (!qssFile.exists())
        qssFile.setFileName("Themes/dark_industrial.qss");
    if (qssFile.open(QFile::ReadOnly | QFile::Text))
    {
        a.setStyleSheet(QString::fromUtf8(qssFile.readAll()));
        qssFile.close();
    }
    else
    {
        qWarning() << "[Theme] 未找到 Themes/dark_industrial.qss，使用默认 Palette 主题";
    }

    // 只允许单个实例运行
    auto appName = QCoreApplication::applicationName();
    QSharedMemory sharedMem(appName);
    if (!sharedMem.create(1))
    {
        QMessageBox::information(nullptr, QObject::tr("提示"), QObject::tr("软件已开，请勿重复开启软件！"), QMessageBox::Ok);
        return 0;
    }

    if (sharedMem.attach())
        return 0;

    MainWindow w;
    w.show();

    return a.exec();
}
