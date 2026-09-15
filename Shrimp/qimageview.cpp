#include "qimageview.h"
#include "roiitem.h"
#include "xlogger.h"
#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QtMath>
#include <QWheelEvent>

QImageView::QImageView(QWidget* parent)
    : QGraphicsView(parent)
{
    // 初始化场景
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    // 连接场景变化信号（监听 Item 的移动和大小调整）
    connect(m_scene, &QGraphicsScene::changed, this, &QImageView::onSceneChanged);

    // 设置视图属性
    setCursor(Qt::ArrowCursor);
    setDragMode(QGraphicsView::RubberBandDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemShape);
    setViewportUpdateMode(FullViewportUpdate);
    setResizeAnchor(AnchorUnderMouse);
    setTransformationAnchor(AnchorUnderMouse);

    // 启用抗锯齿
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 启用鼠标追踪（即使不按下按钮也能接收移动事件）
    setMouseTracking(true);

    // 创建像素信息文本项
    m_pixelInfoText = new QGraphicsTextItem();
    m_pixelInfoText->setDefaultTextColor(Qt::green);
    m_pixelInfoText->setZValue(1000);  // 确保显示在最上层
    // 忽略场景变换，保持文本大小不变
    m_pixelInfoText->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);

    // 设置文本字体
    QFont font = m_pixelInfoText->font();
    font.setPointSize(12);
    font.setBold(true);
    m_pixelInfoText->setFont(font);

    m_scene->addItem(m_pixelInfoText);
    m_pixelInfoText->setVisible(m_showPixelInfo);

    // 创建右键菜单
    createContextMenu();
}

QImageView::~QImageView()
{
    cleanupPixmapItem();

    if (m_pixelInfoText)
    {
        m_scene->removeItem(m_pixelInfoText);
        delete m_pixelInfoText;
        m_pixelInfoText = nullptr;
    }
}

void QImageView::setImage(const QImage& image, bool bTopShow, bool fitInView)
{
    if (image.isNull())
    {
        xWarning("QImageView::setImage - 尝试设置空图像");
        return;
    }

    // 清理旧的图像项
    cleanupPixmapItem();

    // 保存原始图像（用于像素查询，完整保留所有通道信息）
    m_originalImage = image;

    // 转换为 QPixmap 用于高性能显示
    QPixmap pixmap = QPixmap::fromImage(image);

    // 创建新的图像项
    m_pixmapItem = m_scene->addPixmap(pixmap);

    // 设置图层顺序
    m_pixmapItem->setZValue(bTopShow ? 1 : -1);

    // 设置场景范围
    m_scene->setSceneRect(pixmap.rect());

    // 自动适应窗口
    if (fitInView)
    {
        fitToView();
    }
}

void QImageView::setPixmap(const QPixmap& pixmap, bool bTopShow)
{
    if (pixmap.isNull())
    {
        xWarning("QImageView::setPixmap - 尝试设置空图像");
        return;
    }

    // ⚠️ 注意：QPixmap::toImage() 可能丢失 Alpha 通道
    setImage(pixmap.toImage(), bTopShow);
}

void QImageView::fitToView()
{
    if (!m_pixmapItem || m_scene->items().isEmpty())
    {
        return;
    }

    // 获取场景边界矩形
    QRectF sceneRect = m_scene->itemsBoundingRect();

    // 重置变换
    resetTransform();

    // 适应视图（保持纵横比）
    fitInView(sceneRect, Qt::KeepAspectRatio);

    // 更新缩放因子
    updateZoomFactor();
}

void QImageView::resetZoom()
{
    if (!m_pixmapItem || m_scene->items().isEmpty())
    {
        return;
    }

    // 重置变换矩阵
    resetTransform();

    // 居中显示
    centerOn(m_pixmapItem);

    m_currentZoom = 1.0;
    updateZoomFactor();
}

void QImageView::zoomIn()
{
    applyZoom(ZOOM_STEP);
}

void QImageView::zoomOut()
{
    applyZoom(1.0 / ZOOM_STEP);
}

void QImageView::setZoomFactor(double factor)
{
    if (factor < MIN_ZOOM || factor > MAX_ZOOM)
    {
        xWarning("QImageView::setZoomFactor - 缩放因子超出范围: {}", factor);
        return;
    }

    // 重置变换
    resetTransform();

    // 应用新的缩放
    scale(factor, factor);

    m_currentZoom = factor;
    updateZoomFactor();
}

double QImageView::getZoomFactor() const
{
    return m_currentZoom;
}

void QImageView::setHandDragMode(bool useHandDrag)
{
    if (m_isHandDragMode == useHandDrag)
    {
        return;  // 状态未改变，无需更新
    }

    m_isHandDragMode = useHandDrag;

    // 更新拖拽模式
    if (m_isHandDragMode)
    {
        // 手型拖拽模式
        setDragMode(QGraphicsView::ScrollHandDrag);
        setCursor(Qt::OpenHandCursor);
    }
    else
    {
        // 指针选择模式
        setDragMode(QGraphicsView::RubberBandDrag);
        setCursor(Qt::ArrowCursor);
    }

    // 更新菜单文本
    if (m_actionToggleDragMode)
    {
        if (m_isHandDragMode)
        {
            m_actionToggleDragMode->setText("切换到指针模式");
            m_actionToggleDragMode->setIcon(QIcon(":/icons/pointer.png"));
        }
        else
        {
            m_actionToggleDragMode->setText("切换到手型模式");
            m_actionToggleDragMode->setIcon(QIcon(":/icons/hand.png"));
        }
    }

    // 根据模式显示/隐藏像素信息菜单项
    if (m_actionTogglePixelInfo)
    {
        // 手型模式下隐藏像素信息菜单项，指针模式下显示
        m_actionTogglePixelInfo->setVisible(!m_isHandDragMode);
    }

    // 发送模式变化信号
    emit sig_dragModeChanged(m_isHandDragMode);
}

void QImageView::setShowPixelInfo(bool show)
{
    if (m_showPixelInfo == show)
    {
        return;
    }

    m_showPixelInfo = show;

    if (m_pixelInfoText)
    {
        m_pixelInfoText->setVisible(show && !m_isHandDragMode);
    }

    // 更新菜单文本
    if (m_actionTogglePixelInfo)
    {
        m_actionTogglePixelInfo->setText(show ? "隐藏像素信息" : "显示像素信息");
        m_actionTogglePixelInfo->setChecked(show);
    }

    emit sig_pixelInfoVisibilityChanged(show);
}

void QImageView::hideItem(QGraphicsItem* item)
{
    if (item && item != m_pixmapItem && item != m_pixelInfoText)
    {
        item->setVisible(false);
    }
}

void QImageView::showItem(QGraphicsItem* item)
{
    if (item && item != m_pixmapItem && item != m_pixelInfoText)
    {
        item->setVisible(true);
    }
}

void QImageView::hideAllItems()
{
    if (!m_scene)
    {
        return;
    }

    QList<QGraphicsItem*> allItems = m_scene->items();
    for (QGraphicsItem* item : allItems)
    {
        // 跳过需要保留显示的项
        if (item == m_pixmapItem || item == m_pixelInfoText)
        {
            continue;
        }

        item->setVisible(false);
    }
}

void QImageView::showAllItems()
{
    if (!m_scene)
    {
        return;
    }

    QList<QGraphicsItem*> allItems = m_scene->items();
    for (QGraphicsItem* item : allItems)
    {
        // 跳过系统项
        if (item == m_pixmapItem || item == m_pixelInfoText)
        {
            continue;
        }

        item->setVisible(true);
    }
}

void QImageView::setItemVisible(QGraphicsItem* item, bool visible)
{
    if (item && item != m_pixmapItem && item != m_pixelInfoText)
    {
        item->setVisible(visible);
    }
}

QList<QGraphicsItem*> QImageView::allItems() const
{
    if (!m_scene)
    {
        return QList<QGraphicsItem*>();
    }

    return m_scene->items();
}

QList<QGraphicsItem*> QImageView::allItemsExcludeSysItems() const
{
    QList<QGraphicsItem*> items;
    if (!m_scene)
    {
        return items;
    }
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (QGraphicsItem* item : allItems)
    {
        // 跳过系统项
        if (item == m_pixmapItem || item == m_pixelInfoText)
        {
            continue;
        }
        items.append(item);
    }
    return items;
}

QList<ROIItem*> QImageView::allROIItems() const
{
    QList<ROIItem*> roiItems;
    if (!m_scene)
    {
        return roiItems;
    }

    QList<QGraphicsItem*> allItems = m_scene->items();
    for (QGraphicsItem* item : allItems)
    {
        // 尝试转换为 ROIItem
        ROIItem* roi = dynamic_cast<ROIItem*>(item);
        if (roi)
        {
            roiItems.append(roi);
        }
    }
    return roiItems;
}

void QImageView::togglePixelInfo()
{
    setShowPixelInfo(!m_showPixelInfo);
}

void QImageView::onAddROI()
{
    if (!m_pixmapItem || m_originalImage.isNull())
    {
        xWarning("QImageView::onAddROI - 没有图像，无法添加 ROI");
        return;
    }

    // 在图像中心创建一个默认大小的 ROI（图像大小的 1/4）
    QRectF imageRect = m_pixmapItem->boundingRect();
    qreal roiWidth = imageRect.width() * 0.25;
    qreal roiHeight = imageRect.height() * 0.25;
    qreal roiX = (imageRect.width() - roiWidth) / 2;
    qreal roiY = (imageRect.height() - roiHeight) / 2;

    QRectF roiRect(roiX, roiY, roiWidth, roiHeight);
    ROIItem* roi = new ROIItem(roiRect);
    addItem(roi, true);
}

void QImageView::onRemoveSelectedItems()
{
    removeSelectedItems();
}

void QImageView::onRemoveCurrentItem()
{
    if (m_currentItem)
    {
        removeItem(m_currentItem);
    }
}

void QImageView::onClearAllItems()
{
    clearItems();
}

void QImageView::onHideAllItems()
{
    hideAllItems();
    xDebug("已隐藏所有图形项");
}

void QImageView::onShowAllItems()
{
    showAllItems();
    xDebug("已显示所有图形项");
}

void QImageView::onHideUnselectedItems()
{
    if (!m_scene)
    {
        return;
    }

    // 获取所有选中的项
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();

    // 获取所有项
    QList<QGraphicsItem*> allItems = m_scene->items();

    int hiddenCount = 0;
    for (QGraphicsItem* item : allItems)
    {
        // 跳过系统项
        if (item == m_pixmapItem || item == m_pixelInfoText)
        {
            continue;
        }

        // 如果不在选中列表中，则隐藏
        if (!selectedItems.contains(item))
        {
            item->setVisible(false);
            hiddenCount++;
        }
    }

    xDebug("已隐藏 {} 个未选中的图形项", hiddenCount);
}

void QImageView::onShowUnselectedItems()
{
    if (!m_scene)
    {
        return;
    }

    // 获取所有选中的项
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();

    // 获取所有项
    QList<QGraphicsItem*> allItems = m_scene->items();

    int shownCount = 0;
    for (QGraphicsItem* item : allItems)
    {
        // 跳过系统项
        if (item == m_pixmapItem || item == m_pixelInfoText)
        {
            continue;
        }

        // 如果不在选中列表中且当前不可见，则显示
        if (!selectedItems.contains(item) && !item->isVisible())
        {
            item->setVisible(true);
            shownCount++;
        }
    }

    xDebug("已显示 {} 个未选中的图形项", shownCount);
}

void QImageView::onSceneChanged(const QList<QRectF>& region)
{
    Q_UNUSED(region);

    if (!m_scene)
    {
        return;
    }

    // 获取所有非系统项
    QList<QGraphicsItem*> allItems = m_scene->items();
    for (QGraphicsItem* item : allItems)
    {
        // 跳过系统项
        if (item == m_pixmapItem || item == m_pixelInfoText)
        {
            continue;
        }

        // 尝试转换为 ROIItem 以获取准确的 ROI 区域
        ROIItem* roiItem = dynamic_cast<ROIItem*>(item);
        if (roiItem)
        {
            // 获取场景坐标中的矩形
            QRectF sceneRect = roiItem->getROI();

            // 发送几何变化信号
            emit sig_itemGeometryChanged(item, sceneRect);
        }
        else
        {
            // 对于其他类型的 Item，使用场景边界矩形
            QRectF sceneRect = item->sceneBoundingRect();
            emit sig_itemGeometryChanged(item, sceneRect);
        }
    }
}

void QImageView::applyZoom(double factor)
{
    double newZoom = m_currentZoom * factor;

    // 限制缩放范围
    if (newZoom < MIN_ZOOM || newZoom > MAX_ZOOM)
    {
        return;
    }

    // 应用缩放
    scale(factor, factor);

    m_currentZoom = newZoom;
    updateZoomFactor();
}

void QImageView::updateZoomFactor()
{
    // 获取实际的变换矩阵缩放因子
    double actualZoom = transform().m11();
    m_currentZoom = actualZoom;

    // 发送百分比信号
    emit sig_zoomFactorChanged(actualZoom * 100.0);
}

QGraphicsItem* QImageView::addItem(QGraphicsItem* item, bool setAsCurrent)
{
    if (!item || !m_scene)
        return nullptr;

    m_scene->addItem(item);

    if (setAsCurrent)
    {
        setCurrentItem(item);
    }

    return item;
}

void QImageView::removeItem(QGraphicsItem* item)
{
    if (!item || !m_scene)
        return;

    // 发送移除信号（在删除前发出，让外部有机会清理引用）
    emit sig_itemRemoved(item);

    // 如果移除的是当前项,清除引用
    if (item == m_currentItem)
    {
        m_currentItem = nullptr;
    }

    m_scene->removeItem(item);
    delete item;
}

void QImageView::setCurrentItem(QGraphicsItem* item)
{
    m_currentItem = item;
}

QGraphicsItem* QImageView::getCurrentItem() const
{
    return m_currentItem;
}

void QImageView::clearCurrentItem()
{
    m_currentItem = nullptr;
}

void QImageView::removeSelectedItems()
{
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem* item : selectedItems)
    {
        if (item != m_pixmapItem && item != m_pixelInfoText)  // 不删除图像项
        {
            // 发送移除信号
            emit sig_itemRemoved(item);

            m_scene->removeItem(item);
            delete item;
        }
    }
}

void QImageView::clearItems()
{
    if (!m_scene)
    {
        return;
    }

    // 发送清除信号（在清除前发出）
    emit sig_allItemsCleared();

    // 获取所有 item 的副本
    QList<QGraphicsItem*> allItems = m_scene->items();

    // 遍历并删除非系统 item
    for (QGraphicsItem* item : allItems)
    {
        // 跳过需要保留的项
        if (item == m_pixmapItem || item == m_pixelInfoText)
        {
            continue;
        }

        // 移除并删除 item
        m_scene->removeItem(item);
        delete item;
    }

    // 清除当前项引用
    m_currentItem = nullptr;
}

void QImageView::clearAll()
{
    cleanupPixmapItem();
    m_scene->clear();
    m_currentZoom = 1.0;

    // 确保像素信息文本项被重新添加到场景
    if (m_pixelInfoText && !m_scene->items().contains(m_pixelInfoText))
    {
        m_scene->addItem(m_pixelInfoText);
    }
}

void QImageView::cleanupPixmapItem()
{
    if (m_pixmapItem)
    {
        m_scene->removeItem(m_pixmapItem);
        delete m_pixmapItem;
        m_pixmapItem = nullptr;
    }

    // 清空原始图像
    m_originalImage = QImage();
}

void QImageView::createContextMenu()
{
    // 创建右键菜单
    m_contextMenu = new QMenu(this);

    // ========== 模式切换区 ==========
    m_actionToggleDragMode = new QAction("切换到手型模式", this);
    m_actionToggleDragMode->setToolTip("在手型拖拽模式和指针选择模式之间切换 (快捷键: H)");
    connect(m_actionToggleDragMode, &QAction::triggered, this, &QImageView::toggleDragMode);
    m_contextMenu->addAction(m_actionToggleDragMode);

    m_actionTogglePixelInfo = new QAction("显示像素信息", this);
    m_actionTogglePixelInfo->setCheckable(true);
    m_actionTogglePixelInfo->setChecked(m_showPixelInfo);
    m_actionTogglePixelInfo->setToolTip("显示/隐藏鼠标位置的像素信息");
    connect(m_actionTogglePixelInfo, &QAction::triggered, this, &QImageView::togglePixelInfo);
    m_contextMenu->addAction(m_actionTogglePixelInfo);

    m_contextMenu->addSeparator();

    // ========== 图形项管理区 ==========
    m_actionAddROI = new QAction("添加 ROI", this);
    m_actionAddROI->setToolTip("在图像中心添加矩形 ROI");
    connect(m_actionAddROI, &QAction::triggered, this, &QImageView::onAddROI);
    m_contextMenu->addAction(m_actionAddROI);

    m_actionRemoveSelected = new QAction("删除选中项", this);
    m_actionRemoveSelected->setToolTip("删除当前选中的所有图形项 (快捷键: Delete)");
    connect(m_actionRemoveSelected, &QAction::triggered, this, &QImageView::onRemoveSelectedItems);
    m_contextMenu->addAction(m_actionRemoveSelected);

    m_actionRemoveCurrent = new QAction("删除当前项", this);
    m_actionRemoveCurrent->setToolTip("删除当前关注的图形项");
    connect(m_actionRemoveCurrent, &QAction::triggered, this, &QImageView::onRemoveCurrentItem);
    m_contextMenu->addAction(m_actionRemoveCurrent);

    m_actionClearItems = new QAction("清除所有图形项", this);
    m_actionClearItems->setToolTip("清除所有图形项（保留图像）");
    connect(m_actionClearItems, &QAction::triggered, this, &QImageView::onClearAllItems);
    m_contextMenu->addAction(m_actionClearItems);

    m_contextMenu->addSeparator();

    // ========== 隐藏/显示区 ==========
    m_actionHideAllItems = new QAction("隐藏所有图形项", this);
    m_actionHideAllItems->setToolTip("隐藏所有图形项（但不删除）");
    connect(m_actionHideAllItems, &QAction::triggered, this, &QImageView::onHideAllItems);
    m_contextMenu->addAction(m_actionHideAllItems);

    m_actionShowAllItems = new QAction("显示所有图形项", this);
    m_actionShowAllItems->setToolTip("显示所有已隐藏的图形项");
    connect(m_actionShowAllItems, &QAction::triggered, this, &QImageView::onShowAllItems);
    m_contextMenu->addAction(m_actionShowAllItems);

    m_actionHideUnselected = new QAction("隐藏未选中项", this);
    m_actionHideUnselected->setToolTip("隐藏所有未选中的图形项");
    connect(m_actionHideUnselected, &QAction::triggered, this, &QImageView::onHideUnselectedItems);
    m_contextMenu->addAction(m_actionHideUnselected);

    m_actionShowUnselected = new QAction("显示未选中项", this);
    m_actionShowUnselected->setToolTip("显示所有未选中的图形项");
    connect(m_actionShowUnselected, &QAction::triggered, this, &QImageView::onShowUnselectedItems);
    m_contextMenu->addAction(m_actionShowUnselected);

    m_contextMenu->addSeparator();

    // ========== 视图控制区 ==========
    QAction* actionFit = new QAction("适应窗口 (F)", this);
    connect(actionFit, &QAction::triggered, this, &QImageView::fitToView);
    m_contextMenu->addAction(actionFit);

    QAction* actionReset = new QAction("原始大小 (0)", this);
    connect(actionReset, &QAction::triggered, this, &QImageView::resetZoom);
    m_contextMenu->addAction(actionReset);

    m_contextMenu->addSeparator();

    QAction* actionZoomIn = new QAction("放大 (+)", this);
    connect(actionZoomIn, &QAction::triggered, this, &QImageView::zoomIn);
    m_contextMenu->addAction(actionZoomIn);

    QAction* actionZoomOut = new QAction("缩小 (-)", this);
    connect(actionZoomOut, &QAction::triggered, this, &QImageView::zoomOut);
    m_contextMenu->addAction(actionZoomOut);
}

void QImageView::toggleDragMode()
{
    // 切换拖拽模式
    setHandDragMode(!m_isHandDragMode);

    // 手型模式下隐藏像素信息
    if (m_pixelInfoText)
    {
        m_pixelInfoText->setVisible(m_showPixelInfo && !m_isHandDragMode);
    }
}

void QImageView::updateCursor()
{
    if (!m_isPanning)
    {
        if (m_isHandDragMode)
        {
            setCursor(Qt::OpenHandCursor);
        }
        else
        {
            setCursor(Qt::ArrowCursor);
        }
    }
}

void QImageView::updatePixelInfo(const QPointF& scenePos)
{
    // 快速检查：如果不需要显示，直接返回
    if (!m_pixelInfoText || !m_showPixelInfo || m_isHandDragMode)
    {
        if (m_pixelInfoText && m_pixelInfoText->isVisible())
        {
            m_pixelInfoText->setVisible(false);
        }
        return;
    }

    // 检查是否有图像
    if (!m_pixmapItem || m_originalImage.isNull())
    {
        m_pixelInfoText->setVisible(false);
        return;
    }

    // 将场景坐标转换为图像坐标
    QPointF imagePos = m_pixmapItem->mapFromScene(scenePos);
    int x = qRound(imagePos.x());  // 使用 qRound 更精确
    int y = qRound(imagePos.y());

    // 检查坐标是否在图像范围内
    if (x < 0 || x >= m_originalImage.width() || y < 0 || y >= m_originalImage.height())
    {
        m_pixelInfoText->setVisible(false);
        return;
    }

    // 获取像素值字符串
    QString pixelValue = getPixelValueString(x, y);

    // 构建显示文本
    QString infoText = QString("Pos: (%1, %2)\n%3").arg(x).arg(y).arg(pixelValue);
    m_pixelInfoText->setPlainText(infoText);

    // 获取文本边界矩形（在项的本地坐标系中）
    QRectF textRect = m_pixelInfoText->boundingRect();

    // 场景矩形
    QRectF sceneRect = m_scene->sceneRect();

    // 固定偏移量（视图坐标下的像素单位，不受缩放影响）
    const qreal offsetPixels = 20.0;

    // 获取当前缩放比例
    qreal currentZoom = transform().m11();

    // 计算场景坐标下的偏移量（除以缩放比例）
    qreal sceneOffsetX = offsetPixels / currentZoom;
    qreal sceneOffsetY = offsetPixels / currentZoom;

    // 计算文本位置（场景坐标）
    QPointF textPos = scenePos + QPointF(sceneOffsetX, sceneOffsetY);

    // 边界检查：确保文本不超出场景边界
    // textRect 的宽高也需要转换到场景坐标系
    qreal textWidthInScene = textRect.width() / currentZoom;
    qreal textHeightInScene = textRect.height() / currentZoom;

    if (textPos.x() + textWidthInScene > sceneRect.right())
    {
        textPos.setX(scenePos.x() - textWidthInScene - sceneOffsetX);
    }
    if (textPos.y() + textHeightInScene > sceneRect.bottom())
    {
        textPos.setY(scenePos.y() - textHeightInScene - sceneOffsetY);
    }

    // 设置文本位置
    m_pixelInfoText->setPos(textPos);
    m_pixelInfoText->setVisible(true);
}

QString QImageView::getPixelValueString(int x, int y) const
{
    // 参数验证
    if (m_originalImage.isNull())
    {
        return QString();
    }

    if (x < 0 || x >= m_originalImage.width() || y < 0 || y >= m_originalImage.height())
    {
        return QString();
    }

    // 读取像素值
    QRgb pixel = m_originalImage.pixel(x, y);

    // 根据图像格式返回相应的字符串
    switch (m_originalImage.format())
    {
    case QImage::Format_Grayscale8:
    case QImage::Format_Indexed8:
        // 灰度图像
        return QString("Gray: %1").arg(qGray(pixel));

    default:
        // 彩色图像
        if (m_originalImage.hasAlphaChannel())
        {
            // RGBA
            return QString("R:%1 G:%2 B:%3 A:%4")
                .arg(qRed(pixel))
                .arg(qGreen(pixel))
                .arg(qBlue(pixel))
                .arg(qAlpha(pixel));
        }
        else
        {
            // RGB
            return QString("R:%1 G:%2 B:%3")
                .arg(qRed(pixel))
                .arg(qGreen(pixel))
                .arg(qBlue(pixel));
        }
    }
}

// ========== 事件处理 ==========

void QImageView::wheelEvent(QWheelEvent* event)
{
    // 使用滚轮进行缩放
    if (event->angleDelta().y() > 0)
    {
        zoomIn();
    }
    else
    {
        zoomOut();
    }

    event->accept();
}

void QImageView::mouseMoveEvent(QMouseEvent* event)
{
    // 发送场景坐标
    QPointF scenePos = mapToScene(event->pos());
    emit sig_mouseMovePoint(scenePos);

    // 在指针模式下更新像素信息
    if (!m_isHandDragMode && m_showPixelInfo)
    {
        updatePixelInfo(scenePos);
    }

    // 手型拖拽模式的处理由 QGraphicsView 自动完成
    // 中键拖拽（在任何模式下都可用）
    if (m_isPanning)
    {
        QPointF delta = mapToScene(event->pos()) - mapToScene(m_lastMousePos);

        // 计算缩放后的偏移
        delta *= transform().m11();

        // 更新视图中心
        QPointF newCenter = mapToScene(viewport()->rect().center()) - delta;
        centerOn(newCenter);

        m_lastMousePos = event->pos();
    }

    QGraphicsView::mouseMoveEvent(event);
}

void QImageView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton)
    {
        // 中键拖拽（在任何模式下都可用）
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    else if (event->button() == Qt::LeftButton && !m_isHandDragMode)
    {
        // 指针模式下的左键点击
        QPointF scenePos = mapToScene(event->pos());

        // 获取鼠标下的图形项
        QGraphicsItem* itemUnderMouse = itemAt(event->pos());

        // 如果点击到了图形项（且不是图像项和像素信息文本）
        if (itemUnderMouse && itemUnderMouse != m_pixmapItem && itemUnderMouse != m_pixelInfoText)
        {
            // 检查是否按下 Ctrl 键（用于多选）
            bool ctrlPressed = (event->modifiers() & Qt::ControlModifier);

            // 如果没有按 Ctrl 键，且点击的 Item 未被选中，则取消其他选中
            if (!ctrlPressed && !itemUnderMouse->isSelected())
            {
                // 取消所有图形项的选中状态
                QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
                for (QGraphicsItem* item : selectedItems)
                {
                    item->setSelected(false);
                }

                // 选中当前图形项
                itemUnderMouse->setSelected(true);
            }
            // 如果按了 Ctrl 键，或者点击的是已选中的 Item，保持多选状态
            // QGraphicsView 的默认行为会处理 Ctrl+点击的多选逻辑

            // 设置为当前项
            setCurrentItem(itemUnderMouse);
        }
        else if (!itemUnderMouse || itemUnderMouse == m_pixmapItem)
        {
            // 点击空白区域或图像项，且没有按 Ctrl 键时，取消所有选中
            bool ctrlPressed = (event->modifiers() & Qt::ControlModifier);
            if (!ctrlPressed)
            {
                QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
                for (QGraphicsItem* item : selectedItems)
                {
                    item->setSelected(false);
                }
                clearCurrentItem();
            }
        }

        emit sig_mouseClicked(scenePos);
    }

    QGraphicsView::mousePressEvent(event);
}

void QImageView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton)
    {
        // 结束中键拖拽
        m_isPanning = false;
        updateCursor();
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void QImageView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        // 双击适应窗口
        fitToView();

        // 发送双击信号
        QPointF scenePos = mapToScene(event->pos());
        emit sig_mouseDoubleClick(scenePos);
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

void QImageView::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_contextMenu)
    {
        return;
    }

    // 动态更新菜单项可用性
    bool hasImage = m_pixmapItem && !m_originalImage.isNull();
    bool hasSelectedItems = !m_scene->selectedItems().isEmpty();
    bool hasCurrentItem = (m_currentItem != nullptr);

    // 统计非图像项数量（排除 m_pixmapItem 和 m_pixelInfoText）
    int itemCount = 0;
    int visibleItemCount = 0;
    int hiddenItemCount = 0;
    int selectedItemCount = m_scene->selectedItems().count();

    for (QGraphicsItem* item : m_scene->items())
    {
        if (item != m_pixmapItem && item != m_pixelInfoText)
        {
            itemCount++;
            if (item->isVisible())
            {
                visibleItemCount++;
            }
            else
            {
                hiddenItemCount++;
            }
        }
    }
    bool hasItems = (itemCount > 0);
    bool hasVisibleItems = (visibleItemCount > 0);
    bool hasHiddenItems = (hiddenItemCount > 0);

    // 更新菜单项状态
    if (m_actionAddROI)
    {
        m_actionAddROI->setEnabled(hasImage);
    }

    if (m_actionRemoveSelected)
    {
        m_actionRemoveSelected->setEnabled(hasSelectedItems);
        // 更新文本显示选中数量
        if (selectedItemCount > 0)
        {
            m_actionRemoveSelected->setText(QString("删除选中项 (%1)").arg(selectedItemCount));
        }
        else
        {
            m_actionRemoveSelected->setText("删除选中项");
        }
    }

    if (m_actionRemoveCurrent)
    {
        m_actionRemoveCurrent->setEnabled(hasCurrentItem);
    }

    if (m_actionClearItems)
    {
        m_actionClearItems->setEnabled(hasItems);
        // 更新文本显示项目数量
        if (itemCount > 0)
        {
            m_actionClearItems->setText(QString("清除所有图形项 (%1)").arg(itemCount));
        }
        else
        {
            m_actionClearItems->setText("清除所有图形项");
        }
    }

    // 更新隐藏/显示菜单项状态
    if (m_actionHideAllItems)
    {
        m_actionHideAllItems->setEnabled(hasVisibleItems);
        if (visibleItemCount > 0)
        {
            m_actionHideAllItems->setText(QString("隐藏所有图形项 (%1)").arg(visibleItemCount));
        }
        else
        {
            m_actionHideAllItems->setText("隐藏所有图形项");
        }
    }

    if (m_actionShowAllItems)
    {
        m_actionShowAllItems->setEnabled(hasHiddenItems);
        if (hiddenItemCount > 0)
        {
            m_actionShowAllItems->setText(QString("显示所有图形项 (%1)").arg(hiddenItemCount));
        }
        else
        {
            m_actionShowAllItems->setText("显示所有图形项");
        }
    }

    if (m_actionHideUnselected)
    {
        // 只有在有选中项且有其他可见项时才启用
        int unselectedVisibleCount = visibleItemCount - selectedItemCount;
        m_actionHideUnselected->setEnabled(hasSelectedItems && unselectedVisibleCount > 0);
        if (unselectedVisibleCount > 0 && hasSelectedItems)
        {
            m_actionHideUnselected->setText(QString("隐藏未选中项 (%1)").arg(unselectedVisibleCount));
        }
        else
        {
            m_actionHideUnselected->setText("隐藏未选中项");
        }
    }

    if (m_actionShowUnselected)
    {
        // 统计未选中且隐藏的项数量
        int unselectedHiddenCount = 0;
        QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
        for (QGraphicsItem* item : m_scene->items())
        {
            if (item != m_pixmapItem && item != m_pixelInfoText)
            {
                if (!selectedItems.contains(item) && !item->isVisible())
                {
                    unselectedHiddenCount++;
                }
            }
        }

        m_actionShowUnselected->setEnabled(unselectedHiddenCount > 0);
        if (unselectedHiddenCount > 0)
        {
            m_actionShowUnselected->setText(QString("显示未选中项 (%1)").arg(unselectedHiddenCount));
        }
        else
        {
            m_actionShowUnselected->setText("显示未选中项");
        }
    }

    // 显示菜单
    m_contextMenu->exec(event->globalPos());
    event->accept();
}

void QImageView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomIn();
        break;
    case Qt::Key_Minus:
        zoomOut();
        break;
    case Qt::Key_0:
        resetZoom();
        break;
    case Qt::Key_F:
        fitToView();
        break;
    case Qt::Key_H:
        // 快捷键：H 键切换手型/指针模式
        toggleDragMode();
        break;
    case Qt::Key_Delete:
        // 快捷键：Delete 键删除选中项
        removeSelectedItems();
        break;
    default:
        QGraphicsView::keyPressEvent(event);
        break;
    }
}