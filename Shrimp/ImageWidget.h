#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include "opencv2/opencv.hpp"
#include <QGraphicsLineItem>
#include <QWidget>

class QImageView;

namespace Ui {
    class ImageWidget;
}

class ImageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageWidget(QWidget* parent = nullptr);
    ~ImageWidget();

    void displayImage(const cv::Mat& image, bool fitInView = true);

    cv::Mat getCurrentImage() const;

    void setCurrentImage(const cv::Mat& image);

    /**
     * @brief 获取图像视图
     */
    QImageView* getImageView() const;

private slots:
    void on_checkBoxShowCross_stateChanged(int state);

    /// <summary>
    /// 适应窗口
    /// </summary>
    void on_pushButtonFit_clicked();

    /// <summary>
    /// 恢复100%显示
    /// </summary>
    void on_pushButtonZoomNormal_clicked();

    /// <summary>
    /// 缩放因子变化槽函数
    /// </summary>
    void onZoomFactorChanged(double percentage);

private:
    /// <summary>
    /// 更新十字线显示
    /// </summary>
    void updateCrossLines();

    /// <summary>
    /// 移除十字线
    /// </summary>
    void removeCrossLines();

private:
    Ui::ImageWidget* ui;
    cv::Mat m_currentImage;

    // 十字线图形项
    QGraphicsLineItem* m_crossLineH = nullptr;  ///< 水平十字线
    QGraphicsLineItem* m_crossLineV = nullptr;  ///< 垂直十字线
    bool m_showCross = false;                   ///< 是否显示十字线
};

#endif // IMAGEWIDGET_H
