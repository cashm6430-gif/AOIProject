#pragma once

#include "PclVtkConverter.h"

#include <QVTKOpenGLNativeWidget.h>

#include <array>
#include <functional>
#include <vector>

#include <vtkSmartPointer.h>
#include <vtkType.h>

class vtkActor;
class vtkAxesActor;
class vtkBoxWidget;
class vtkCallbackCommand;
class vtkGenericOpenGLRenderWindow;
class vtkInteractorStyleTrackballCamera;
class vtkObject;
class vtkOrientationMarkerWidget;
class vtkPolyData;
class vtkRenderWindowInteractor;
class vtkRenderer;

class VtkCloudRenderer
{
public:
    using PickResultCallback = std::function<void(bool, vtkIdType, const std::array<double, 3>&)>;

    void Initialize(QVTKOpenGLNativeWidget* widget);
    void ShowPointCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud);

    void SetPickingEnabled(bool enabled);
    void SetPickResultCallback(PickResultCallback callback);

    bool IsCropBoxVisible() const;
    void ToggleCropBox(const pcl::PointCloud<pcl::PointXYZ>& cloud);
    bool CropPointsInsideBox(pcl::PointCloud<pcl::PointXYZ>& cloud, int& removedCount);

private:
    static void OnLeftButtonPress(vtkObject* caller, unsigned long eventId, void* clientData, void* callData);

    bool IsInitialized() const;
    void HandleLeftButtonPress();
    void AddPickMarker(const std::array<double, 3>& point);
    void ClearPickMarkers();

    QVTKOpenGLNativeWidget* m_widget = nullptr;

    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkOrientationMarkerWidget> m_orientationAxes;
    vtkSmartPointer<vtkInteractorStyleTrackballCamera> m_styleCamera;
    vtkSmartPointer<vtkRenderWindowInteractor> m_interactor;
    vtkSmartPointer<vtkCallbackCommand> m_leftButtonObserver;

    vtkSmartPointer<vtkActor> m_pointActor;
    vtkSmartPointer<vtkPolyData> m_cloudPolyData;
    vtkSmartPointer<vtkBoxWidget> m_boxWidget;
    std::vector<vtkSmartPointer<vtkActor>> m_pickMarkers;

    bool m_isPickingEnabled = false;
    bool m_isBoxShown = false;
    PickResultCallback m_pickCallback;
};
