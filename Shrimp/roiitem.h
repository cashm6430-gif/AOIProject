#ifndef ROIITEM_H
#define ROIITEM_H

#include <QBrush>
#include <QCursor>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QPen>

/**
 * @brief 可交互的 ROI 矩形框
 *
 * 功能：
 * - 支持拖拽移动整个矩形
 * - 支持通过8个控制点调整大小
 * - 可获取矩形在图像中的坐标
 */
class ROIItem : public QGraphicsRectItem
{
public:
    /**
     * @brief 控制点位置枚举
     */
    enum HandlePosition
    {
        None = 0,
        TopLeft,
        TopCenter,
        TopRight,
        RightCenter,
        BottomRight,
        BottomCenter,
        BottomLeft,
        LeftCenter,
        Center  // 整体移动
    };

    /**
     * @brief 构造函数
     * @param rect 初始矩形区域
     * @param parent 父图形项
     */
    explicit ROIItem(const QRectF& rect = QRectF(0, 0, 100, 100), QGraphicsItem* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ROIItem() override = default;

    /**
     * @brief 获取 ROI 矩形（场景坐标）
     */
    QRectF getROI() const;

    /**
     * @brief 设置 ROI 矩形
     */
    void setROI(const QRectF& rect);

    /**
     * @brief 设置是否可编辑
     */
    void setEditable(bool editable);

    /**
     * @brief 获取是否可编辑
     */
    bool isEditable() const { return m_editable; }

    /**
     * @brief 设置 ROI 颜色
     */
    void setColor(const QColor& color);

    /**
     * @brief 设置线宽
     */
    void setLineWidth(qreal width);

    /**
     * @brief 设置控制点大小
     * @param size 控制点大小（像素）
     */
    void setHandleSize(qreal size);

    /**
     * @brief 获取控制点大小
     * @return 控制点大小
     */
    qreal handleSize() const { return m_handleSize; }

    /**
     * @brief 根据图像大小计算合适的控制点大小
     * @param imageWidth 图像宽度
     * @param imageHeight 图像高度
     * @return 推荐的控制点大小
     */
    static qreal calculateHandleSize(int imageWidth, int imageHeight);

protected:
    // 重写的虚函数
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    QRectF boundingRect() const override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    /**
     * @brief 获取鼠标位置对应的控制点
     */
    HandlePosition getHandleAtPos(const QPointF& pos) const;

    /**
     * @brief 获取控制点矩形
     */
    QRectF getHandleRect(HandlePosition handle) const;

    /**
     * @brief 根据控制点获取光标样式
     */
    Qt::CursorShape getCursorForHandle(HandlePosition handle) const;

    /**
     * @brief 更新矩形大小
     */
    void updateRectFromHandle(HandlePosition handle, const QPointF& pos);

    /**
     * @brief 绘制控制点
     */
    void drawHandles(QPainter* painter) const;

private:
    bool m_editable = true;                  ///< 是否可编辑
    HandlePosition m_activeHandle = None;    ///< 当前活动的控制点
    QPointF m_lastMousePos;                  ///< 上次鼠标位置
    QRectF m_originalRect;                   ///< 调整前的矩形

    // 样式参数
    QColor m_color = Qt::green;              ///< ROI 颜色
    qreal m_lineWidth = 2.0;                 ///< 线宽
    qreal m_handleSize = 16.0;               ///< 控制点大小
};

#endif // ROIITEM_H
