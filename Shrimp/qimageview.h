#ifndef QIMAGEVIEW_H
#define QIMAGEVIEW_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsTextItem>
#include <QMenu>
#include <QPixmap>

class ROIItem;

/**
 * @brief 图像显示控件，支持缩放、拖拽、鼠标交互
 */
class QImageView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit QImageView(QWidget* parent = nullptr);
    ~QImageView() override;

    /**
     * @brief 设置显示的图像（保留完整的像素信息包括Alpha通道）
     * @param image 图像对象
     * @param bTopShow 是否显示在最上层
     * @param fitInView 是否适应视图
     */
    void setImage(const QImage& image, bool bTopShow = false, bool fitInView = true);

    /**
     * @brief 设置显示的图像
     * @param pixmap 图像对象（使用常量引用避免拷贝）
     * @param bTopShow 是否显示在最上层
     */
    void setPixmap(const QPixmap& pixmap, bool bTopShow = false);

    /**
     * @brief 获取当前显示的原始图像
     * @return 原始 QImage（包含完整像素信息）
     */
    const QImage& getOriginalImage() const { return m_originalImage; }

    /**
     * @brief 检查是否有图像
     */
    bool hasImage() const { return !m_originalImage.isNull(); }

    /**
     * @brief 适应窗口大小显示
     */
    void fitToView();

    /**
     * @brief 重置为原始大小（1:1）
     */
    void resetZoom();

    /**
     * @brief 放大
     */
    void zoomIn();

    /**
     * @brief 缩小
     */
    void zoomOut();

    /**
     * @brief 设置缩放因子
     * @param factor 缩放因子（1.0 = 100%）
     */
    void setZoomFactor(double factor);

    /**
     * @brief 获取当前缩放因子
     */
    double getZoomFactor() const;

    /**
     * @brief 添加图形项
     * @param item 图形项指针
     * @param setAsCurrent 是否将其设置为当前项
     * @return 添加的图形项指针
     */
    QGraphicsItem* addItem(QGraphicsItem* item, bool setAsCurrent = false);

    /**
     * @brief 移除图形项
     * @param item 图形项指针
     */
    void removeItem(QGraphicsItem* item);

    /**
     * @brief 设置当前关注的图形项
     * @param item 要设置为当前的项
     */
    void setCurrentItem(QGraphicsItem* item);

    /**
     * @brief 获取当前图形项
     */
    QGraphicsItem* getCurrentItem() const;

    /**
     * @brief 清除当前图形项
     */
    void clearCurrentItem();

    /**
     * @brief 移除选中的图形项
     */
    void removeSelectedItems();

    /**
     * @brief 清除所有图形项（保留图像）
     */
    void clearItems();

    /**
     * @brief 清除所有内容（包括图像）
     */
    void clearAll();

    /**
     * @brief 设置拖拽模式
     * @param useHandDrag true=手型拖拽模式，false=指针选择模式
     */
    void setHandDragMode(bool useHandDrag);

    /**
     * @brief 获取当前是否为手型拖拽模式
     */
    bool isHandDragMode() const { return m_isHandDragMode; }

    /**
     * @brief 设置是否显示像素信息
     * @param show true=显示，false=隐藏
     */
    void setShowPixelInfo(bool show);

    /**
     * @brief 获取当前是否显示像素信息
     */
    bool isShowPixelInfo() const { return m_showPixelInfo; }

    /**
     * @brief 隐藏指定的图形项
     * @param item 要隐藏的图形项指针
     */
    void hideItem(QGraphicsItem* item);

    /**
     * @brief 显示指定的图形项
     * @param item 要显示的图形项指针
     */
    void showItem(QGraphicsItem* item);

    /**
     * @brief 隐藏所有图形项（除了图像和像素信息）
     */
    void hideAllItems();

    /**
     * @brief 显示所有图形项
     */
    void showAllItems();

    /**
     * @brief 设置图形项的可见性
     * @param item 图形项指针
     * @param visible true=显示，false=隐藏
     */
    void setItemVisible(QGraphicsItem* item, bool visible);

    /**
     * @brief 获取所有图形项
     * @return 图形项列表
     */
    QList<QGraphicsItem*> allItems() const;

    /**
     * @brief 获取所有图形项，排除系统项（图像项和像素信息项）
     * @return 图形项列表
     */
    QList<QGraphicsItem*> allItemsExcludeSysItems() const;

    /**
     * @brief 获取所有 ROI 图形项
     * @return ROI 图形项列表
     */
    QList<ROIItem*> allROIItems() const;

signals:
    /**
     * @brief 缩放比例变化信号
     * @param percentage 百分比（100 = 100%）
     */
    void sig_zoomFactorChanged(double percentage);

    /**
     * @brief 鼠标移动信号
     * @param scenePos 场景坐标
     */
    void sig_mouseMovePoint(const QPointF& scenePos);

    /**
     * @brief 鼠标单击信号
     * @param scenePos 场景坐标
     */
    void sig_mouseClicked(const QPointF& scenePos);

    /**
     * @brief 鼠标双击信号
     * @param scenePos 场景坐标
     */
    void sig_mouseDoubleClick(const QPointF& scenePos);

    /**
     * @brief 拖拽模式变化信号
     * @param isHandDrag true=手型模式，false=指针模式
     */
    void sig_dragModeChanged(bool isHandDrag);

    /**
     * @brief 像素信息显示状态变化信号
     * @param show true=显示，false=隐藏
     */
    void sig_pixelInfoVisibilityChanged(bool show);

    /**
     * @brief Item 被移除信号
     * @param item 被移除的图形项指针（在删除前发出）
     */
    void sig_itemRemoved(QGraphicsItem* item);

    /**
     * @brief 所有 Item 被清除信号（在清除前发出）
     */
    void sig_allItemsCleared();

    /**
     * @brief Item 几何形状变化信号（位置或大小改变）
     * @param item 变化的图形项指针
     * @param rect Item 在场景中的矩形区域
     */
    void sig_itemGeometryChanged(QGraphicsItem* item, const QRectF& rect);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    /**
     * @brief 切换拖拽模式槽函数
     */
    void toggleDragMode();

    /**
     * @brief 切换像素信息显示
     */
    void togglePixelInfo();

    /**
     * @brief 添加 ROI 槽函数
     */
    void onAddROI();

    /**
     * @brief 删除选中项槽函数
     */
    void onRemoveSelectedItems();

    /**
     * @brief 删除当前项槽函数
     */
    void onRemoveCurrentItem();

    /**
     * @brief 清除所有图形项槽函数
     */
    void onClearAllItems();

    /**
     * @brief 隐藏所有图形项槽函数
     */
    void onHideAllItems();

    /**
     * @brief 显示所有图形项槽函数
     */
    void onShowAllItems();

    /**
     * @brief 隐藏未选中的图形项槽函数
     */
    void onHideUnselectedItems();

    /**
     * @brief 显示所有未选中的图形项槽函数
     */
    void onShowUnselectedItems();

    /**
     * @brief 场景变化槽函数（监听 Item 的移动和大小变化）
     * @param region 变化的区域列表
     */
    void onSceneChanged(const QList<QRectF>& region);

private:
    /**
     * @brief 执行缩放操作
     * @param factor 缩放因子（>1放大，<1缩小）
     */
    void applyZoom(double factor);

    /**
     * @brief 更新缩放因子显示
     */
    void updateZoomFactor();

    /**
     * @brief 清理图像项
     */
    void cleanupPixmapItem();

    /**
     * @brief 创建右键菜单
     */
    void createContextMenu();

    /**
     * @brief 更新光标状态
     */
    void updateCursor();

    /**
     * @brief 更新像素信息显示
     * @param scenePos 场景坐标
     */
    void updatePixelInfo(const QPointF& scenePos);

    /**
     * @brief 获取指定位置的像素值
     * @param x 图像X坐标
     * @param y 图像Y坐标
     * @return 像素值字符串（RGB或灰度）
     */
    QString getPixelValueString(int x, int y) const;

private:
    // 场景和显示
    QGraphicsScene* m_scene = nullptr;              ///< 图形场景
    QGraphicsPixmapItem* m_pixmapItem = nullptr;    ///< 图像项（用于显示）

    // 原始图像数据
    QImage m_originalImage;                         ///< 原始图像（保留完整像素信息，用于查询）

    // 鼠标交互
    QPoint m_lastMousePos;                          ///< 上次鼠标位置
    bool m_isPanning = false;                       ///< 是否正在拖拽

    // 缩放参数
    double m_currentZoom = 1.0;                     ///< 当前缩放因子
    static constexpr double MIN_ZOOM = 0.05;        ///< 最小缩放
    static constexpr double MAX_ZOOM = 20.0;        ///< 最大缩放
    static constexpr double ZOOM_STEP = 1.1;        ///< 缩放步长

    // 拖拽模式
    bool m_isHandDragMode = false;                  ///< 是否为手型拖拽模式（默认为指针模式）

    // 像素信息显示
    bool m_showPixelInfo = false;                   ///< 是否显示像素信息（默认不显示）
    QGraphicsTextItem* m_pixelInfoText = nullptr;   ///< 像素信息文本项

    // 右键菜单
    QMenu* m_contextMenu = nullptr;                 ///< 右键菜单
    QAction* m_actionToggleDragMode = nullptr;      ///< 切换拖拽模式动作
    QAction* m_actionTogglePixelInfo = nullptr;     ///< 切换像素信息显示动作
    QAction* m_actionAddROI = nullptr;              ///< 添加 ROI 动作
    QAction* m_actionRemoveSelected = nullptr;      ///< 删除选中项动作
    QAction* m_actionRemoveCurrent = nullptr;       ///< 删除当前项动作
    QAction* m_actionClearItems = nullptr;          ///< 清除所有图形项动作

    // 图形项显示控制
    QAction* m_actionHideAllItems = nullptr;        ///< 隐藏所有图形项动作
    QAction* m_actionShowAllItems = nullptr;        ///< 显示所有图形项动作
    QAction* m_actionHideUnselected = nullptr;      ///< 隐藏未选中项动作
    QAction* m_actionShowUnselected = nullptr;      ///< 显示未选中项动作

    // 图形项管理
    QGraphicsItem* m_currentItem = nullptr;                ///< 当前关注的图形项
};

#endif // QIMAGEVIEW_H
