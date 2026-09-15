#ifndef POINTCLOUDWIDGET_H
#define POINTCLOUDWIDGET_H

#include "VtkCloudRenderer.h"
#include "roiitem.h"
#include <pcl/io/pcd_io.h>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QLabel>
#include <QTableWidget>
#include <QWidget>
#include <array>

namespace Ui { class PointCloudWidget; }

class PointCloudWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PointCloudWidget(QWidget* parent = nullptr);
    ~PointCloudWidget();
    void ShowPointCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud);
    int resultRowCount() const { return m_resultTable ? m_resultTable->rowCount() : 0; }
    QString resultSummary() const { return m_summaryLabel ? m_summaryLabel->text() : QString(); }
private slots:
    void on_roiButton_clicked();
    void on_measureButton_clicked();
private:
    void initVtkRenderer();
    void showProjection();
    void runMeasurement();
    Ui::PointCloudWidget* ui;
    pcl::PointCloud<pcl::PointXYZ> m_cloud;
    VtkCloudRenderer m_cloudRenderer;
    bool m_isPickingMode = false;
    QGraphicsScene* m_roiScene = nullptr;
    ROIItem* m_roiItem = nullptr;
    double m_projMinX = 0.0;
    double m_projMinY = 0.0;
    double m_projScaleX = 1.0;
    double m_projScaleY = 1.0;
    int m_projH = 0;
    bool m_hasPick = false;
    std::array<double, 3> m_pick = {0.0, 0.0, 0.0};
    QGroupBox* m_resultGroup = nullptr;
    QTableWidget* m_resultTable = nullptr;
    QLabel* m_summaryLabel = nullptr;
};
#endif // POINTCLOUDWIDGET_H
