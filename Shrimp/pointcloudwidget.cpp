#include "pointcloudwidget.h"
#include "ui_pointcloudwidget.h"

#include "core/AlgorithmSDK.h"
#include "operators/RegisterBuiltinOperators.h"
#include "xlogger.h"

#include <QAbstractItemView>
#include <QColor>
#include <QGraphicsView>
#include <QHeaderView>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

namespace {
AlgorithmSDK::PointCloud toAlgorithmPointCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
    AlgorithmSDK::PointCloud result;
    result.reserve(static_cast<int>(cloud.size()));
    for (const auto& point : cloud.points) result.addPoint(point.x, point.y, point.z);
    return result;
}
}

PointCloudWidget::PointCloudWidget(QWidget* parent)
    : QWidget(parent), ui(new Ui::PointCloudWidget)
{
    ui->setupUi(this);
    m_roiScene = new QGraphicsScene(this);
    ui->roiView->setScene(m_roiScene);
    ui->roiView->setRenderHint(QPainter::Antialiasing, true);
    ui->roiView->hide();
    initVtkRenderer();

    m_resultGroup = new QGroupBox(tr("测量结果"), this);
    auto* layout = new QVBoxLayout(m_resultGroup);
    m_resultTable = new QTableWidget(0, 3, m_resultGroup);
    m_resultTable->setHorizontalHeaderLabels({tr("测量项"), tr("数值"), tr("判定")});
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_resultTable);
    m_summaryLabel = new QLabel(tr("—"), m_resultGroup);
    layout->addWidget(m_summaryLabel);
    ui->verticalLayout->addWidget(m_resultGroup);
}

PointCloudWidget::~PointCloudWidget() { delete ui; }

void PointCloudWidget::ShowPointCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
    m_cloud = cloud;
    ui->roiView->hide();
    m_roiItem = nullptr;
    m_hasPick = false;
    m_cloudRenderer.ShowPointCloud(cloud);
}

void PointCloudWidget::initVtkRenderer()
{
    m_cloudRenderer.Initialize(ui->vtkOpenGLWidget);
    m_cloudRenderer.SetPickResultCallback([this](bool hit, vtkIdType index, const std::array<double, 3>& point) {
        if (!m_isPickingMode) return;
        if (!hit) {
            xWarning("未点中有效点云，请对准点云再试，或调大点尺寸。");
            return;
        }
        m_hasPick = true;
        m_pick = point;
        xInfo("选点成功：索引=%lld，坐标 X=%.4f Y=%.4f Z=%.4f", index, point[0], point[1], point[2]);
    });
}

void PointCloudWidget::on_roiButton_clicked() { showProjection(); }
void PointCloudWidget::on_measureButton_clicked() { runMeasurement(); }

void PointCloudWidget::showProjection()
{
    if (m_cloud.empty()) {
        xWarning("当前没有点云，无法框选 ROI。");
        return;
    }
    double minX = std::numeric_limits<double>::max();
    double maxX = -std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxY = -std::numeric_limits<double>::max();
    for (const auto& point : m_cloud.points) {
        minX = std::min(minX, static_cast<double>(point.x)); maxX = std::max(maxX, static_cast<double>(point.x));
        minY = std::min(minY, static_cast<double>(point.y)); maxY = std::max(maxY, static_cast<double>(point.y));
    }
    if (maxX <= minX) maxX = minX + 1e-9;
    if (maxY <= minY) maxY = minY + 1e-9;
    double scale = std::min(640.0 / (maxX - minX), 640.0 / (maxY - minY));
    int width = std::max(1, static_cast<int>((maxX - minX) * scale) + 1);
    int height = std::max(1, static_cast<int>((maxY - minY) * scale) + 1);
    if (width > 2048 || height > 2048) {
        scale /= static_cast<double>(std::max(width, height)) / 2048.0;
        width = static_cast<int>((maxX - minX) * scale) + 1;
        height = static_cast<int>((maxY - minY) * scale) + 1;
    }
    m_projMinX = minX; m_projMinY = minY; m_projScaleX = scale; m_projScaleY = scale; m_projH = height;
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::black);
    for (const auto& point : m_cloud.points) {
        const int px = static_cast<int>((point.x - minX) * scale + 0.5);
        const int py = static_cast<int>((maxY - point.y) * scale + 0.5);
        if (px >= 0 && px < width && py >= 0 && py < height) image.setPixel(px, py, qRgb(120, 200, 120));
    }
    m_roiScene->clear();
    m_roiScene->addPixmap(QPixmap::fromImage(image));
    m_roiScene->setSceneRect(image.rect());
    m_roiItem = new ROIItem(QRectF(image.width() * 0.2, image.height() * 0.2, image.width() * 0.6, image.height() * 0.6));
    m_roiItem->setEditable(true);
    m_roiItem->setColor(QColor(220, 40, 40));
    m_roiScene->addItem(m_roiItem);
    ui->roiView->show();
    ui->roiView->fitInView(m_roiScene->sceneRect(), Qt::KeepAspectRatio);
    xInfo("已生成投影。请调整红色 ROI 框，然后点「测量」。");
}

void PointCloudWidget::runMeasurement()
{
    if (!m_roiItem) { xWarning("请先点「框选ROI」。"); return; }
    const QRectF roi = m_roiItem->getROI();
    if (roi.width() <= 1 || roi.height() <= 1) { xWarning("ROI 无效，请重新框选。"); return; }
    const double xMin = m_projMinX + roi.left() / m_projScaleX;
    const double xMax = m_projMinX + roi.right() / m_projScaleX;
    const double maxY = m_projMinY + (m_projH - 1) / m_projScaleY;
    const double yMin = maxY - roi.bottom() / m_projScaleY;
    const double yMax = maxY - roi.top() / m_projScaleY;
    pcl::PointCloud<pcl::PointXYZ> roiCloud;
    for (const auto& point : m_cloud.points) {
        if (point.x >= xMin && point.x <= xMax && point.y >= yMin && point.y <= yMax) roiCloud.points.push_back(point);
    }
    if (roiCloud.empty()) { xWarning("ROI 内没有点，请重新框选。"); return; }
    double px = m_pick[0], py = m_pick[1], pz = m_pick[2];
    if (!m_hasPick) {
        px = py = pz = 0.0;
        for (const auto& point : roiCloud.points) { px += point.x; py += point.y; pz += point.z; }
        px /= roiCloud.size(); py /= roiCloud.size(); pz /= roiCloud.size();
    }

    AlgorithmSDK::RegisterBuiltinOperators();
    const QJsonObject recipe{{"numThreads", 4}, {"pipeline", QJsonArray{
        QJsonObject{{"id", "filter"}, {"operator", "PointCloudFilter"}, {"params", QJsonObject{{"inputKey", "input_pointcloud_0"}, {"outputKey", "filtered_pointcloud"}, {"meanK", 50}, {"stddevMulThresh", 3.0}}}, {"deps", QJsonArray{}}},
        QJsonObject{{"id", "fit_plane"}, {"operator", "PlaneFit"}, {"params", QJsonObject{{"inputKey", "filtered_pointcloud"}, {"outputKey", "fitted_plane"}}}, {"deps", QJsonArray{"filter"}}},
        QJsonObject{{"id", "measure_distance"}, {"operator", "Distance"}, {"params", QJsonObject{{"mode", "point_to_plane"}, {"pointKey", "measure_point"}, {"planeKey", "fitted_plane"}, {"outputKey", "result_distance"}, {"nominalValue", 0.0}, {"upperTolerance", 0.05}, {"lowerTolerance", -0.05}}}, {"deps", QJsonArray{"fit_plane"}}},
        QJsonObject{{"id", "judge"}, {"operator", "ResultJudge"}, {"params", QJsonObject{{"resultKeys", QJsonArray{"result_distance"}}, {"outputKey", "result_final"}}}, {"deps", QJsonArray{"measure_distance"}}}
    }}};
    AlgorithmSDK::AlgorithmInput input;
    input.pointclouds.append(toAlgorithmPointCloud(roiCloud));
    AlgorithmSDK::AlgorithmTask task;
    bool ok = task.open();
    if (ok) ok = task.setConfig(recipe);
    if (ok) {
        task.setInput(input);
        task.setInputPoint3D("measure_point", AlgorithmSDK::Geometry::Point3D(px, py, pz));
        ok = task.process();
    }
    if (!ok) { xError("TaskControl 初始化、配方加载或执行失败：{}", task.lastError().toStdString()); return; }
    const auto output = task.getOutput();
    const bool finalOk = task.context()->get<bool>("__final_ok__", false);
    m_resultTable->setRowCount(0);
    double distance = 0.0;
    for (const auto& measurement : output.results) {
        const int row = m_resultTable->rowCount();
        m_resultTable->insertRow(row);
        m_resultTable->setItem(row, 0, new QTableWidgetItem(measurement.name));
        m_resultTable->setItem(row, 1, new QTableWidgetItem(QString::number(measurement.value, 'f', 4)));
        const bool pass = measurement.status == 0;
        auto* judge = new QTableWidgetItem(pass ? "PASS" : "FAIL");
        judge->setForeground(QColor(pass ? "#2a8a2a" : "#c33"));
        m_resultTable->setItem(row, 2, judge);
        if (measurement.name == "result_distance") distance = measurement.value;
    }
    const auto plane = task.context()->getPlane("fitted_plane");
    const QString planeText = QString("平面法向 a=%1 b=%2 c=%3 d=%4").arg(plane.normal.x, 0, 'f', 5).arg(plane.normal.y, 0, 'f', 5).arg(plane.normal.z, 0, 'f', 5).arg(plane.d, 0, 'f', 5);
    m_summaryLabel->setText(QString("ROI点数=%1  耗时=%2ms  判定=%3   |   %4").arg(static_cast<long long>(roiCloud.size())).arg(output.processingTimeMs).arg(finalOk ? "OK" : "NG").arg(planeText));
    m_summaryLabel->setStyleSheet(finalOk ? "color:#2a8a2a;" : "color:#c33;");
    ui->measureResult->setText(QString("距离=%1  %2").arg(distance, 0, 'f', 4).arg(finalOk ? "PASS" : "FAIL"));
    ui->measureResult->setStyleSheet(finalOk ? "color:#2a8a2a;" : "color:#c33;");
}
