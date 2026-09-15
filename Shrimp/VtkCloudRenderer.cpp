#include "VtkCloudRenderer.h"

#include <pcl/common/common.h>

#include <vtkActor.h>
#include <vtkAxesActor.h>
#include <vtkBoxWidget.h>
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPlanes.h>
#include <vtkPointPicker.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSphereSource.h>
#include <vtkVertexGlyphFilter.h>

#include <algorithm>
#include <cmath>

void VtkCloudRenderer::Initialize(QVTKOpenGLNativeWidget* widget)
{
    if (widget == nullptr) {
        return;
    }

    m_widget = widget;
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_orientationAxes = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    m_styleCamera = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();

    m_renderWindow->AddRenderer(m_renderer);
    m_widget->setRenderWindow(m_renderWindow);
    m_widget->setFocusPolicy(Qt::StrongFocus);

    m_interactor = m_widget->interactor();
    if (m_interactor) {
        m_interactor->SetInteractorStyle(m_styleCamera);
    }

    // 设置背景色为深蓝色
    m_renderer->SetBackground(0.1, 0.1, 0.2);

    vtkSmartPointer<vtkAxesActor> axesActor = vtkSmartPointer<vtkAxesActor>::New();
    axesActor->SetTotalLength(1.0, 1.0, 1.0);
    m_orientationAxes->SetOrientationMarker(axesActor);
    m_orientationAxes->SetInteractor(m_interactor);

    m_renderWindow->Render();
    int* winSize = m_renderWindow->GetSize();
    const double w = (winSize && winSize[0] > 0) ? static_cast<double>(winSize[0]) : 1.0;
    const double h = (winSize && winSize[1] > 0) ? static_cast<double>(winSize[1]) : 1.0;
    const double frac = 0.1;
    const double wFrac = frac * (h / w);
    m_orientationAxes->SetViewport(0.0, 0.0, wFrac, frac);
    m_orientationAxes->SetEnabled(true);
    m_orientationAxes->InteractiveOn();

    m_leftButtonObserver = vtkSmartPointer<vtkCallbackCommand>::New();
    m_leftButtonObserver->SetClientData(this);
    m_leftButtonObserver->SetCallback(VtkCloudRenderer::OnLeftButtonPress);
    if (m_interactor) {
        m_interactor->AddObserver(vtkCommand::LeftButtonPressEvent, m_leftButtonObserver);
    }

    m_renderWindow->Render();
}

void VtkCloudRenderer::ShowPointCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
    if (!IsInitialized()) {
        return;
    }

    if (m_boxWidget && m_isBoxShown) {
        m_boxWidget->Off();
        m_isBoxShown = false;
    }

    ClearPickMarkers();
    if (m_pointActor) {
        m_renderer->RemoveActor(m_pointActor);
        m_pointActor = nullptr;
    }

    m_cloudPolyData = PclVtkConverter::ToPolyData(cloud);
    vtkNew<vtkVertexGlyphFilter> glyph;
    glyph->SetInputData(m_cloudPolyData);
    glyph->Update();

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(glyph->GetOutputPort());

    m_pointActor = vtkSmartPointer<vtkActor>::New();
    m_pointActor->SetMapper(mapper);
    m_pointActor->GetProperty()->SetColor(250.0 / 255.0, 10.0 / 255.0, 10.0 / 255.0);
    m_pointActor->GetProperty()->SetPointSize(2.0);

    m_renderer->AddActor(m_pointActor);
    m_renderer->ResetCamera();
    m_renderWindow->Render();
}

void VtkCloudRenderer::SetPickingEnabled(bool enabled)
{
    m_isPickingEnabled = enabled;
}

void VtkCloudRenderer::SetPickResultCallback(PickResultCallback callback)
{
    m_pickCallback = std::move(callback);
}

bool VtkCloudRenderer::IsCropBoxVisible() const
{
    return m_isBoxShown;
}

void VtkCloudRenderer::ToggleCropBox(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
    if (!IsInitialized()) {
        return;
    }

    if (m_isBoxShown) {
        if (m_boxWidget) {
            m_boxWidget->Off();
        }
        m_isBoxShown = false;
        m_renderWindow->Render();
        return;
    }

    if (cloud.empty()) {
        return;
    }

    if (!m_boxWidget) {
        m_boxWidget = vtkSmartPointer<vtkBoxWidget>::New();
        m_boxWidget->SetInteractor(m_interactor);
        m_boxWidget->SetPlaceFactor(1.0);
        m_boxWidget->SetHandleSize(0.01);
        m_boxWidget->RotationEnabledOn();
        m_boxWidget->ScalingEnabledOn();
        m_boxWidget->TranslationEnabledOn();
    }

    pcl::PointXYZ minPt;
    pcl::PointXYZ maxPt;
    pcl::getMinMax3D(cloud, minPt, maxPt);

    double bounds[6] = {
        static_cast<double>(minPt.x), static_cast<double>(maxPt.x),
        static_cast<double>(minPt.y), static_cast<double>(maxPt.y),
        static_cast<double>(minPt.z), static_cast<double>(maxPt.z)
    };
    m_boxWidget->PlaceWidget(bounds);
    m_boxWidget->On();

    m_isBoxShown = true;
    m_renderWindow->Render();
}

bool VtkCloudRenderer::CropPointsInsideBox(pcl::PointCloud<pcl::PointXYZ>& cloud, int& removedCount)
{
    removedCount = 0;
    if (!m_isBoxShown || !m_boxWidget) {
        return false;
    }

    vtkSmartPointer<vtkPlanes> planes = vtkSmartPointer<vtkPlanes>::New();
    m_boxWidget->GetPlanes(planes);

    vtkSmartPointer<vtkPolyData> boxPoly = vtkSmartPointer<vtkPolyData>::New();
    m_boxWidget->GetPolyData(boxPoly);
    double center[3] = { 0.0, 0.0, 0.0 };
    boxPoly->GetCenter(center);
    const bool centerNeg = (planes->EvaluateFunction(center[0], center[1], center[2]) < 0.0);

    pcl::PointCloud<pcl::PointXYZ> remained;
    remained.reserve(cloud.size());

    for (const auto& point : cloud.points) {
        const bool inside = ((planes->EvaluateFunction(point.x, point.y, point.z) < 0.0) == centerNeg);
        if (!inside) {
            remained.push_back(point);
        }
        else {
            ++removedCount;
        }
    }

    cloud = remained;
    cloud.width = static_cast<uint32_t>(cloud.size());
    cloud.height = 1;
    cloud.is_dense = false;

    m_boxWidget->Off();
    m_isBoxShown = false;
    return true;
}

void VtkCloudRenderer::OnLeftButtonPress(vtkObject* caller, unsigned long, void* clientData, void*)
{
    auto* self = static_cast<VtkCloudRenderer*>(clientData);
    if (self == nullptr) {
        return;
    }

    self->HandleLeftButtonPress();
}

bool VtkCloudRenderer::IsInitialized() const
{
    return m_widget != nullptr && m_renderer != nullptr && m_renderWindow != nullptr;
}

void VtkCloudRenderer::HandleLeftButtonPress()
{
    if (!m_isPickingEnabled || !m_interactor || !m_cloudPolyData || m_cloudPolyData->GetNumberOfPoints() == 0) {
        return;
    }
    if (!m_interactor->GetShiftKey()) {
        return;
    }

    int* pos = m_interactor->GetEventPosition();
    if (pos == nullptr) {
        return;
    }

    vtkNew<vtkPointPicker> picker;
    picker->SetTolerance(0.01);
    const bool hit = (picker->Pick(pos[0], pos[1], 0.0, m_renderer) == 1);
    const vtkIdType pointId = picker->GetPointId();
    if (!hit || pointId < 0 || pointId >= m_cloudPolyData->GetNumberOfPoints()) {
        if (m_pickCallback) {
            m_pickCallback(false, -1, { 0.0, 0.0, 0.0 });
        }
        return;
    }

    double point[3] = { 0.0, 0.0, 0.0 };
    m_cloudPolyData->GetPoint(pointId, point);

    const std::array<double, 3> pickPoint = { point[0], point[1], point[2] };
    AddPickMarker(pickPoint);

    if (m_pickCallback) {
        m_pickCallback(true, pointId, pickPoint);
    }
    m_renderWindow->Render();
}

void VtkCloudRenderer::AddPickMarker(const std::array<double, 3>& point)
{
    double radius = 0.1;
    if (m_cloudPolyData && m_cloudPolyData->GetNumberOfPoints() > 1) {
        double bounds[6] = { 0 };
        m_cloudPolyData->GetBounds(bounds);
        const double dx = bounds[1] - bounds[0];
        const double dy = bounds[3] - bounds[2];
        const double dz = bounds[5] - bounds[4];
        const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (std::isfinite(diagonal) && diagonal > 0.0) {
            radius = std::max(0.001, diagonal * 0.005);
        }
    }

    vtkNew<vtkSphereSource> sphere;
    sphere->SetCenter(point[0], point[1], point[2]);
    sphere->SetRadius(radius);
    sphere->SetThetaResolution(20);
    sphere->SetPhiResolution(20);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(sphere->GetOutputPort());

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.0, 0.0, 1.0);

    m_renderer->AddActor(actor);
    m_pickMarkers.push_back(actor);
}

void VtkCloudRenderer::ClearPickMarkers()
{
    for (const auto& marker : m_pickMarkers) {
        m_renderer->RemoveActor(marker);
    }
    m_pickMarkers.clear();
}