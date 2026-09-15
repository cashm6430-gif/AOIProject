#include "ImageWidget.h"
#include "ui_ImageWidget.h"

#include "qimageview.h"
#include <QGraphicsLineItem>
#include <QPen>

ImageWidget::ImageWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ImageWidget)
{
    ui->setupUi(this);

    // 连接缩放因子变化信号
    connect(ui->graphicsViewImage, &QImageView::sig_zoomFactorChanged,
        this, &ImageWidget::onZoomFactorChanged);

    // 设置 lineEditZoom 为只读
    ui->lineEditZoom->setReadOnly(true);
}

ImageWidget::~ImageWidget()
{
    removeCrossLines();

    delete ui;
}

void ImageWidget::displayImage(const cv::Mat& image, bool fitInView)
{
    if (image.empty())
    {
        return;
    }

    m_currentImage = image.clone();

    // 将cv::Mat转换为QImage
    QImage qImage;
    if (image.channels() == 3)
    {
        cv::Mat rgbImage;
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
        qImage = QImage((const uchar*)rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888).copy();
    }
    else if (image.channels() == 1)
    {
        qImage = QImage((const uchar*)image.data, image.cols, image.rows, image.step, QImage::Format_Grayscale8).copy();
    }
    else if (image.channels() == 4)
    {
        cv::Mat rgbaImage;
        cv::cvtColor(image, rgbaImage, cv::COLOR_BGRA2RGBA);
        qImage = QImage((const uchar*)rgbaImage.data, rgbaImage.cols, rgbaImage.rows, rgbaImage.step, QImage::Format_RGBA8888).copy();
    }
    else
    {
        // 不支持的通道数
        return;
    }

    // 在QImageView中显示图像
    //ui->graphicsViewImage->setPixmap(QPixmap::fromImage(qImage), true);
    ui->graphicsViewImage->setImage(qImage, true, fitInView);

    // 如果十字线开启，更新十字线位置
    if (m_showCross)
    {
        updateCrossLines();
    }
}

cv::Mat ImageWidget::getCurrentImage() const
{
    return m_currentImage;
}

void ImageWidget::setCurrentImage(const cv::Mat& image)
{
    if (image.empty())
    {
        return;
    }

    m_currentImage = image.clone();
}

QImageView* ImageWidget::getImageView() const
{
    return ui->graphicsViewImage;
}

void ImageWidget::on_checkBoxShowCross_stateChanged(int state)
{
    m_showCross = (state == Qt::Checked);

    if (m_showCross)
    {
        // 显示十字线
        updateCrossLines();
    }
    else
    {
        // 隐藏十字线
        removeCrossLines();
    }
}

void ImageWidget::on_pushButtonFit_clicked()
{
    // 调用 QImageView 的 fitToView 方法
    ui->graphicsViewImage->fitToView();
}

void ImageWidget::on_pushButtonZoomNormal_clicked()
{
    // 调用 QImageView 的 resetZoom 方法，恢复1:1显示
    ui->graphicsViewImage->resetZoom();
}

void ImageWidget::onZoomFactorChanged(double percentage)
{
    // 更新缩放显示文本
    ui->lineEditZoom->setText(QString("%1 %").arg(QString::number(percentage, 'f', 1)));

    // 如果十字线开启，更新十字线位置（因为缩放会改变场景范围）
    if (m_showCross)
    {
        updateCrossLines();
    }
}

void ImageWidget::updateCrossLines()
{
    if (m_currentImage.empty())
    {
        return;
    }

    // 移除旧的十字线
    removeCrossLines();

    // 获取图像中心点
    double centerX = m_currentImage.cols / 2.0;
    double centerY = m_currentImage.rows / 2.0;

    // 创建十字线画笔
    QPen crossPen(Qt::green, 2, Qt::SolidLine);
    crossPen.setCosmetic(true);  // 设置为cosmetic，使线宽不受缩放影响

    // 创建水平十字线（从左到右穿过中心）
    m_crossLineH = new QGraphicsLineItem(0, centerY, m_currentImage.cols, centerY);
    m_crossLineH->setPen(crossPen);
    m_crossLineH->setZValue(100);  // 设置高层级，确保显示在图像上方

    // 创建垂直十字线（从上到下穿过中心）
    m_crossLineV = new QGraphicsLineItem(centerX, 0, centerX, m_currentImage.rows);
    m_crossLineV->setPen(crossPen);
    m_crossLineV->setZValue(100);

    // 添加到场景
    ui->graphicsViewImage->addItem(m_crossLineH);
    ui->graphicsViewImage->addItem(m_crossLineV);
}

void ImageWidget::removeCrossLines()
{
    if (m_crossLineH)
    {
        ui->graphicsViewImage->removeItem(m_crossLineH);
        m_crossLineH = nullptr;
    }

    if (m_crossLineV)
    {
        ui->graphicsViewImage->removeItem(m_crossLineV);
        m_crossLineV = nullptr;
    }
}