#include "PclVtkConverter.h"

#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

vtkSmartPointer<vtkPolyData> PclVtkConverter::ToPolyData(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkCellArray> vertices = vtkSmartPointer<vtkCellArray>::New();

    points->SetDataTypeToFloat();
    points->Allocate(static_cast<vtkIdType>(cloud.size()));
    vertices->AllocateEstimate(static_cast<vtkIdType>(cloud.size()), 1);

    for (const auto& point : cloud.points) {
        const vtkIdType pointId = points->InsertNextPoint(point.x, point.y, point.z);
        vertices->InsertNextCell(1, &pointId);
    }

    polyData->SetPoints(points);
    polyData->SetVerts(vertices);
    return polyData;
}