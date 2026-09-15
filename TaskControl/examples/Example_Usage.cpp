/**
 * @file Example_Usage.cpp
 * @brief Algorithm SDK 使用示例
 */

#include "core/AlgorithmSDK.h"
#include "core/TaskflowExecutor.h"

 // 示例中显式包含部分算子头，便于独立演示算子能力
#include "operators/feature/PlaneFitOperator.h"
#include "operators/measure/DistanceOperator.h"
#include "operators/measure/ResultJudgeOperator.h"
#include "operators/preprocess/PointCloudFilterOperator.h"

#include <opencv2/core.hpp>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>

#include "DataDefines.h"
#include "TaskControl.h"
#include "TaskControl_C.h"
#include "XLogger.h"

using namespace AlgorithmSDK;

static PointCloud createTestPointCloud()
{
    PointCloud cloud;
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < 100; ++j) {
            float noise = (rand() % 100 - 50) * 0.0001f;
            cloud.addPoint(i * 0.1f, j * 0.1f, noise);
        }
    }
    return cloud;
}

static std::vector<std::vector<PointData>> createLegacyPointData(int rows, int cols)
{
    std::vector<std::vector<PointData>> points(rows, std::vector<PointData>(cols));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            PointData p;
            p.x = c * 0.01;
            p.y = r * 0.01;
            p.z = ((r + c) % 7) * 0.001;
            p.intensity = (r + c) % 255;
            points[r][c] = p;
        }
    }
    return points;
}

static cv::Mat createTestImage(int width, int height)
{
    cv::Mat img(height, width, CV_8UC1);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            img.at<unsigned char>(y, x) = static_cast<unsigned char>((x + y) % 256);
        }
    }
    return img;
}

/**
 * @brief 示例：使用 AlgorithmTask（内部基于 Taskflow 调度）
 */
void exampleUsage()
{
    AlgorithmTask task;
    if (!task.open()) {
        xError("Failed to open task");
        return;
    }

    QFile configFile("config/pipeline_example.json");
    if (!configFile.open(QIODevice::ReadOnly)) {
        xError("Failed to open pipeline config");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    if (doc.isNull() || !doc.isObject()) {
        xError("Invalid pipeline config json");
        return;
    }

    if (!task.setConfig(doc.object())) {
        xError("Failed to set config: {}", task.lastError().toStdString());
        task.close();
        return;
    }

    AlgorithmInput input;
    input.workpieceId = 1;
    input.pointclouds.append(createTestPointCloud());
    task.context()->setPoint3D("measure_point", Geometry::Point3D(5.0, 5.0, 0.1));
    task.setInput(input);

    if (!task.process()) {
        xError("Process failed: {}", task.lastError().toStdString());
        task.close();
        return;
    }

    AlgorithmOutput output = task.getOutput();
    xInfo("Result: {}", (output.ok ? "OK" : "NG"));
    xInfo("Processing time: {} ms", output.processingTimeMs);

    for (const auto& result : output.results) {
        xInfo("  {}: {} ({})",
            result.name.toStdString(),
            result.value,
            (result.status == 0 ? "OK" : "NG"));
    }

    task.close();
}

/**
 * @brief 示例：直接使用 TaskflowExecutor 执行算法图
 */
void exampleTaskflowExecutor()
{
    QFile configFile("config/pipeline_example.json");
    if (!configFile.open(QIODevice::ReadOnly)) {
        xError("Failed to open pipeline config");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    if (doc.isNull() || !doc.isObject()) {
        xError("Invalid pipeline config json");
        return;
    }

    QJsonObject config = doc.object();
    OperatorGraph graph = OperatorGraph::fromJson(config);
    if (!graph.isValid()) {
        xError("Invalid operator graph");
        return;
    }

    for (OperatorNode& node : graph.nodes()) {
        OperatorNode* n = graph.getNode(node.id);
        if (!n) {
            continue;
        }
        n->operatorInstance = OperatorRegistry::instance().create(n->operatorName);
        if (!n->operatorInstance) {
            xError("Unknown operator: {}", n->operatorName.toStdString());
            return;
        }
        n->operatorInstance->setParams(n->params);
    }

    AlgorithmContext ctx;
    ctx.setPointCloud("input_pointcloud_0", createTestPointCloud());
    ctx.set<int>("workpiece_id", 1);
    ctx.setPoint3D("measure_point", Geometry::Point3D(5.0, 5.0, 0.1));

    TaskflowExecutor executor;
    if (config.contains("numThreads")) {
        executor.setNumThreads(static_cast<size_t>(config["numThreads"].toInt()));
    }

    QElapsedTimer timer;
    timer.start();

    if (!executor.execute(graph, ctx)) {
        std::string reason = ctx.hasError()
            ? ctx.getError().toStdString()
            : executor.lastError().toStdString();
        xError("Process failed: {}", reason);
        return;
    }

    AlgorithmOutput output;
    output.ok = !ctx.hasError();
    output.reason = ctx.getError();
    output.processingTimeMs = timer.elapsed();

    for (const QString& key : ctx.keys()) {
        if (key.startsWith("result_")) {
            output.results.append(ctx.getMeasureResult(key));
        }
    }

    xInfo("Result: {}", (output.ok ? "OK" : "NG"));
    xInfo("Processing time: {} ms", output.processingTimeMs);

    for (const auto& result : output.results) {
        xInfo("  {}: {} ({})",
            result.name.toStdString(),
            result.value,
            (result.status == 0 ? "OK" : "NG"));
    }
}

/**
 * @brief 示例：直接使用算子
 */
void exampleDirectOperator()
{
    AlgorithmContext ctx;

    PointCloud cloud;
    for (int i = 0; i < 1000; ++i) {
        float x = (rand() % 100) * 0.1f;
        float y = (rand() % 100) * 0.1f;
        float z = (rand() % 10) * 0.01f;
        cloud.addPoint(x, y, z);
    }
    ctx.setPointCloud("input_pointcloud_0", cloud);

    auto filterOp = OperatorRegistry::instance().create("PointCloudFilter");
    auto planeFitOp = OperatorRegistry::instance().create("PlaneFit");

    if (!filterOp || !planeFitOp) {
        xError("Failed to create operators");
        return;
    }

    QJsonObject filterParams;
    filterParams["inputKey"] = "input_pointcloud_0";
    filterParams["outputKey"] = "filtered";
    filterParams["stddevMulThresh"] = 2.0;
    filterOp->setParams(filterParams);

    QJsonObject fitParams;
    fitParams["inputKey"] = "filtered";
    fitParams["outputKey"] = "plane";
    planeFitOp->setParams(fitParams);

    filterOp->execute(ctx);
    planeFitOp->execute(ctx);

    if (ctx.has("plane")) {
        Geometry::Plane plane = ctx.getPlane("plane");
        xInfo("Fitted plane normal: ({}, {}, {}), d = {}",
            plane.normal.x, plane.normal.y, plane.normal.z, plane.d);
    }
}

/**
 * @brief 示例：通过 C++ 导出接口 TaskControl 输入图像+点云
 */
void exampleTaskControlCppMixedInput()
{
    TaskControl task;
    if (!task.OpenTask("config/pipeline_example.json")) {
        xError("TaskControl C++: OpenTask failed");
        return;
    }

    auto points = createLegacyPointData(64, 64);
    if (!task.AddData(points)) {
        xError("TaskControl C++: AddData failed");
        return;
    }

    cv::Mat image = createTestImage(256, 256);
    if (!task.AddImageData(image)) {
        xError("TaskControl C++: AddImageData failed");
        return;
    }

    std::string resultJson;
    if (!task.RunTask(resultJson)) {
        xError("TaskControl C++: RunTask failed");
        return;
    }

    xInfo("TaskControl C++ mixed-input result json: {}", resultJson);
}

/**
 * ============================================================================
 * C API 集成说明（README 风格）
 * ============================================================================
 * 1) 句柄生命周期
 *    - TaskControlHandle TaskControl_Create()
 *      创建任务句柄，失败返回 nullptr。
 *    - int TaskControl_Destroy(TaskControlHandle handle)
 *      释放句柄与内部资源；handle 不能为空。
 *
 * 2) 配置加载
 *    - int TaskControl_OpenTask(TaskControlHandle handle, const char* filePath)
 *      参数说明：
 *      - handle  : 由 TaskControl_Create 返回的有效句柄
 *      - filePath: 配置文件路径（如 "config/pipeline_example.json"）
 *
 * 3) 输入点云（可选）
 *    - int TaskControl_AddData(TaskControlHandle handle,
 *                              double* pointsData,
 *                              int dataRows,
 *                              int dataCols,
 *                              int channels)
 *      参数说明：
 *      - pointsData: 行优先连续内存，按 [x, y, z, intensity] 排列
 *      - dataRows  : 行数
 *      - dataCols  : 列数
 *      - channels  : 每点通道数，建议 4（x,y,z,intensity），最少 3
 *      索引公式：index = (r * dataCols + c) * channels
 *
 * 4) 输入图像（可选）
 *    - int TaskControl_AddImageData(TaskControlHandle handle,
 *                                   const unsigned char* imageData,
 *                                   int width,
 *                                   int height,
 *                                   int channels,
 *                                   int step)
 *      参数说明：
 *      - imageData: 图像首地址（8-bit）
 *      - width/height: 图像宽高（像素）
 *      - channels: 1/3/4（灰度/BGR/BGRA）
 *      - step: 每行字节数；传 0 时内部按 width * channels 推导
 *      说明：库内部会复制图像数据，调用后可释放外部缓冲区。
 *
 * 5) 执行与取结果
 *    - int TaskControl_RunTask(TaskControlHandle handle, char* output, int* length)
 *      参数说明：
 *      - output: 调用方分配的输出缓冲区（JSON 字符串）
 *      - length: 返回实际 JSON 长度（不含终止符）
 *      建议：预分配足够大的缓冲区（如 64KB 或更大）。
 *
 * 6) 推荐调用顺序
 *    Create -> OpenTask -> (AddData/AddImageData 可重复、多次) -> RunTask -> Destroy
 *
 * 7) 返回码约定
 *    - RET_OK: 成功
 *    - 其他错误码（INIT_ERROR / DATA_ERROR / PROCESS_ERROR / RESULT_ERROR 等）
 *      请参考 DataDefines 中定义。
 * ============================================================================
 */

 /**
  * @brief 示例：通过 C 导出接口 TaskControl_C 输入图像+点云
  */
void exampleTaskControlCMixedInput()
{
    TaskControlHandle handle = TaskControl_Create();
    if (!handle) {
        xError("TaskControl C: create failed");
        return;
    }

    if (TaskControl_OpenTask(handle, "config/pipeline_example.json") != RET_OK) {
        xError("TaskControl C: OpenTask failed");
        TaskControl_Destroy(handle);
        return;
    }

    int rows = 32;
    int cols = 32;
    int channels = 4;
    std::vector<double> pointBuffer(rows * cols * channels, 0.0);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int index = (r * cols + c) * channels;
            pointBuffer[index] = c * 0.02;
            pointBuffer[index + 1] = r * 0.02;
            pointBuffer[index + 2] = ((r + c) % 5) * 0.001;
            pointBuffer[index + 3] = (r + c) % 255;
        }
    }

    if (TaskControl_AddData(handle, pointBuffer.data(), rows, cols, channels) != RET_OK) {
        xError("TaskControl C: AddData failed");
        TaskControl_Destroy(handle);
        return;
    }

    cv::Mat image = createTestImage(128, 128);
    if (TaskControl_AddImageData(
        handle,
        image.data,
        image.cols,
        image.rows,
        image.channels(),
        static_cast<int>(image.step)) != RET_OK) {
        xError("TaskControl C: AddImageData failed");
        TaskControl_Destroy(handle);
        return;
    }

    std::vector<char> outputBuffer(65536, 0);
    int length = 0;
    if (TaskControl_RunTask(handle, outputBuffer.data(), &length) != RET_OK) {
        xError("TaskControl C: RunTask failed");
        TaskControl_Destroy(handle);
        return;
    }

    xInfo("TaskControl C mixed-input result json: {}",
        std::string(outputBuffer.data(), static_cast<size_t>(length)));

    TaskControl_Destroy(handle);
}

int main(int argc, char* argv[])
{
    XLogger::init();

    xInfo("=== Algorithm SDK Example ===");

    xInfo("--- Example 1: Using AlgorithmTask ---");
    exampleUsage();

    xInfo("--- Example 2: Using TaskflowExecutor ---");
    exampleTaskflowExecutor();

    xInfo("--- Example 3: Direct Operator Usage ---");
    exampleDirectOperator();

    xInfo("--- Example 4: TaskControl C++ Mixed Input ---");
    exampleTaskControlCppMixedInput();

    xInfo("--- Example 5: TaskControl C Mixed Input ---");
    exampleTaskControlCMixedInput();

    return 0;
}