#include "roiitem.h"
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QtMath>

ROIItem::ROIItem(const QRectF& rect, QGraphicsItem* parent)
    : QGraphicsRectItem(rect, parent)
{
    // 设置标志
    setFlags(QGraphicsItem::ItemIsMovable |
        QGraphicsItem::ItemIsSelectable |
        QGraphicsItem::ItemSendsGeometryChanges);

    // 启用悬停事件
    setAcceptHoverEvents(true);

    // 设置默认样式
    QPen pen(m_color, m_lineWidth);
    pen.setCosmetic(true);  // 线宽不受缩放影响
    setPen(pen);

    // 设置为无填充（透明）
    setBrush(Qt::NoBrush);

    // 设置 Z 值，确保显示在图像上方
    setZValue(100);
}

QRectF ROIItem::getROI() const
{
    // 返回场景坐标中的矩形
    return mapRectToScene(rect());
}

void ROIItem::setROI(const QRectF& rect)
{
    setRect(0, 0, rect.width(), rect.height());
    setPos(rect.topLeft());
}

void ROIItem::setEditable(bool editable)
{
    m_editable = editable;
    setFlag(QGraphicsItem::ItemIsMovable, editable);
    setFlag(QGraphicsItem::ItemIsSelectable, editable);
    setAcceptHoverEvents(editable);
}

void ROIItem::setColor(const QColor& color)
{
    m_color = color;
    QPen p = pen();
    p.setColor(color);
    setPen(p);
    update();
}

void ROIItem::setLineWidth(qreal width)
{
    m_lineWidth = width;
    QPen p = pen();
    p.setWidthF(width);
    setPen(p);
    update();
}

void ROIItem::setHandleSize(qreal size)
{
    if (size > 0 && size != m_handleSize)
    {
        m_handleSize = size;
        prepareGeometryChange();  // 通知场景边界矩形将要改变
        update();
    }
}

QRectF ROIItem::boundingRect() const
{
    // 扩展边界以包含控制点
    qreal extra = m_handleSize;
    return rect().adjusted(-extra, -extra, extra, extra);
}

qreal ROIItem::calculateHandleSize(int imageWidth, int imageHeight)
{
    // 根据图像对角线长度计算控制点大小
    // 使控制点在视觉上保持合适的比例
    qreal diagonal = std::sqrt(static_cast<qreal>(imageWidth * imageWidth + imageHeight * imageHeight));

    // 控制点大小约为图像对角线的 1%，但限制在合理范围内
    qreal size = diagonal * 0.01;

    // 限制控制点大小在 8 到 32 像素之间
    constexpr qreal minHandleSize = 8.0;
    constexpr qreal maxHandleSize = 32.0;

    return std::clamp(size, minHandleSize, maxHandleSize);
}

void ROIItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // 绘制矩形框
    painter->setPen(pen());
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect());

    // 如果可编辑且被选中，绘制控制点
    if (m_editable && isSelected())
    {
        drawHandles(painter);
    }
}

void ROIItem::drawHandles(QPainter* painter) const
{
    // 设置控制点样式
    QPen handlePen(m_color, 1.0);
    handlePen.setCosmetic(true);
    QBrush handleBrush(Qt::white);

    painter->setPen(handlePen);
    painter->setBrush(handleBrush);

    // 绘制8个控制点
    for (int i = TopLeft; i <= LeftCenter; ++i)
    {
        HandlePosition handle = static_cast<HandlePosition>(i);
        QRectF handleRect = getHandleRect(handle);
        painter->drawRect(handleRect);
    }
}

ROIItem::HandlePosition ROIItem::getHandleAtPos(const QPointF& pos) const
{
    if (!m_editable)
        return None;

    // 检查是否在某个控制点上
    for (int i = TopLeft; i <= LeftCenter; ++i)
    {
        HandlePosition handle = static_cast<HandlePosition>(i);
        QRectF handleRect = getHandleRect(handle);

        if (handleRect.contains(pos))
        {
            return handle;
        }
    }

    // 检查是否在矩形内部（用于移动）
    if (rect().contains(pos))
    {
        return Center;
    }

    return None;
}

QRectF ROIItem::getHandleRect(HandlePosition handle) const
{
    QRectF r = rect();
    qreal half = m_handleSize / 2.0;
    QPointF center;

    switch (handle)
    {
    case TopLeft:
        center = r.topLeft();
        break;
    case TopCenter:
        center = QPointF(r.center().x(), r.top());
        break;
    case TopRight:
        center = r.topRight();
        break;
    case RightCenter:
        center = QPointF(r.right(), r.center().y());
        break;
    case BottomRight:
        center = r.bottomRight();
        break;
    case BottomCenter:
        center = QPointF(r.center().x(), r.bottom());
        break;
    case BottomLeft:
        center = r.bottomLeft();
        break;
    case LeftCenter:
        center = QPointF(r.left(), r.center().y());
        break;
    default:
        return QRectF();
    }

    return QRectF(center.x() - half, center.y() - half, m_handleSize, m_handleSize);
}

Qt::CursorShape ROIItem::getCursorForHandle(HandlePosition handle) const
{
    switch (handle)
    {
    case TopLeft:
    case BottomRight:
        return Qt::SizeFDiagCursor;
    case TopRight:
    case BottomLeft:
        return Qt::SizeBDiagCursor;
    case TopCenter:
    case BottomCenter:
        return Qt::SizeVerCursor;
    case LeftCenter:
    case RightCenter:
        return Qt::SizeHorCursor;
    case Center:
        return Qt::SizeAllCursor;
    default:
        return Qt::ArrowCursor;
    }
}

void ROIItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_editable)
    {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton)
    {
        m_lastMousePos = event->pos();
        m_activeHandle = getHandleAtPos(event->pos());
        m_originalRect = rect();

        if (m_activeHandle != None && m_activeHandle != Center)
        {
            // 调整大小时不移动整个项
            setFlag(QGraphicsItem::ItemIsMovable, false);
        }

        event->accept();
    }
    else
    {
        QGraphicsRectItem::mousePressEvent(event);
    }
}

void ROIItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_editable || m_activeHandle == None)
    {
        QGraphicsRectItem::mouseMoveEvent(event);
        return;
    }

    QPointF currentPos = event->pos();

    if (m_activeHandle == Center)
    {
        // 移动整个矩形
        QGraphicsRectItem::mouseMoveEvent(event);
    }
    else
    {
        // 调整矩形大小
        updateRectFromHandle(m_activeHandle, currentPos);
    }

    event->accept();
}

void ROIItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_activeHandle = None;
        setFlag(QGraphicsItem::ItemIsMovable, m_editable);
    }

    QGraphicsRectItem::mouseReleaseEvent(event);
}

void ROIItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    if (!m_editable)
    {
        QGraphicsRectItem::hoverMoveEvent(event);
        return;
    }

    HandlePosition handle = getHandleAtPos(event->pos());
    setCursor(getCursorForHandle(handle));

    QGraphicsRectItem::hoverMoveEvent(event);
}

void ROIItem::updateRectFromHandle(HandlePosition handle, const QPointF& pos)
{
    QRectF newRect = rect();

    switch (handle)
    {
    case TopLeft:
        newRect.setTopLeft(pos);
        break;
    case TopCenter:
        newRect.setTop(pos.y());
        break;
    case TopRight:
        newRect.setTopRight(pos);
        break;
    case RightCenter:
        newRect.setRight(pos.x());
        break;
    case BottomRight:
        newRect.setBottomRight(pos);
        break;
    case BottomCenter:
        newRect.setBottom(pos.y());
        break;
    case BottomLeft:
        newRect.setBottomLeft(pos);
        break;
    case LeftCenter:
        newRect.setLeft(pos.x());
        break;
    default:
        return;
    }

    // 确保宽高为正值
    newRect = newRect.normalized();

    // 设置最小尺寸
    const qreal minSize = 10.0;
    if (newRect.width() < minSize)
        newRect.setWidth(minSize);
    if (newRect.height() < minSize)
        newRect.setHeight(minSize);

    setRect(newRect);
}