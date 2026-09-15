#pragma once

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <vtkSmartPointer.h>

class vtkPolyData;

class PclVtkConverter
{
public:
    static vtkSmartPointer<vtkPolyData> ToPolyData(const pcl::PointCloud<pcl::PointXYZ>& cloud);
};
