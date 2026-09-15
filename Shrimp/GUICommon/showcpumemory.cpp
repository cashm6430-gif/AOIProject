#include "showcpumemory.h"
#include "systemsetting.h"
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QTimer>

#ifdef Q_OS_WIN
#include "windows.h"
#endif
#define GB (1024 * 1024 * 1024)
#define MB (1024 * 1024)
#define KB (1024)

ShowCPUMemory::ShowCPUMemory(QObject* parent) : QObject(parent)
{
    totalNew = idleNew = totalOld = idleOld = 0;
    cpuPercent = 0;

    memoryPercent = 0;
    memoryAll = 0;
    memoryUse = 0;
    //labCPUMemory = 0;
    m_prgrssBrCPU = 0;
    m_prgrssBrMemory = 0;
    m_prgrssBrSpace = 0;

    timerSpace = new QTimer(this);
    connect(timerSpace, SIGNAL(timeout()), this, SLOT(GetSpace()));

    timerCPU = new QTimer(this);
    connect(timerCPU, SIGNAL(timeout()), this, SLOT(GetCPU()));

    timerMemory = new QTimer(this);
    connect(timerMemory, SIGNAL(timeout()), this, SLOT(GetMemory()));

    process = new QProcess(this);
    connect(process, SIGNAL(readyRead()), this, SLOT(ReadData()));
}

void ShowCPUMemory::SetBar(QProgressBar* space, QProgressBar* mempory, QProgressBar* cpu)
{
    this->m_prgrssBrSpace = space;
    this->m_prgrssBrMemory = mempory;
    this->m_prgrssBrCPU = cpu;

    m_prgrssBrSpace->setRange(0, 100);
    m_prgrssBrMemory->setRange(0, 100);
    m_prgrssBrCPU->setRange(0, 100);
    m_prgrssBrSpace->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);  // 对齐方式
    m_prgrssBrMemory->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);  // 对齐方式
    m_prgrssBrCPU->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);  // 对齐方式
    GetCPU();
    GetMemory();
}

void ShowCPUMemory::Start(int interval)
{
    timerCPU->start(interval);
    timerMemory->start(interval + 200);
    timerSpace->start(interval + 100);
}

void ShowCPUMemory::Stop()
{
    timerCPU->stop();
    timerMemory->stop();
    timerSpace->stop();
}

void ShowCPUMemory::GetCPU()
{
#ifdef Q_OS_WIN
#if (QT_VERSION >= QT_VERSION_CHECK(4,8,7))

    static FILETIME preidleTime = { 0, 0 };
    static FILETIME prekernelTime = { 0, 0 };
    static FILETIME preuserTime = { 0, 0 };

    FILETIME idleTime;
    FILETIME kernelTime;
    FILETIME userTime;
    GetSystemTimes(&idleTime, &kernelTime, &userTime);

    ULARGE_INTEGER fTime1, fTime2;
    ULARGE_INTEGER idle, kernel, user;
    fTime1.HighPart = preidleTime.dwHighDateTime, fTime1.LowPart = preidleTime.dwLowDateTime;
    fTime2.HighPart = idleTime.dwHighDateTime, fTime2.LowPart = idleTime.dwLowDateTime;
    idle.QuadPart = fTime2.QuadPart - fTime1.QuadPart;

    fTime1.HighPart = prekernelTime.dwHighDateTime, fTime1.LowPart = prekernelTime.dwLowDateTime;
    fTime2.HighPart = kernelTime.dwHighDateTime, fTime2.LowPart = kernelTime.dwLowDateTime;
    kernel.QuadPart = fTime2.QuadPart - fTime1.QuadPart;

    fTime1.HighPart = preuserTime.dwHighDateTime, fTime1.LowPart = preuserTime.dwLowDateTime;
    fTime2.HighPart = userTime.dwHighDateTime, fTime2.LowPart = userTime.dwLowDateTime;
    user.QuadPart = fTime2.QuadPart - fTime1.QuadPart;

    if ((kernel.QuadPart + user.QuadPart) != 0) {
        cpuPercent = abs(long long(kernel.QuadPart + user.QuadPart - idle.QuadPart)) * 100 / (kernel.QuadPart + user.QuadPart);
    }
    else {
        cpuPercent = 0;
    }

    preidleTime = idleTime;
    prekernelTime = kernelTime;
    preuserTime = userTime;

    m_prgrssBrCPU->setValue(cpuPercent);
    m_prgrssBrCPU->setFormat(tr("CPU使用率:%1%").arg(cpuPercent));

    //根据数值确定进度条颜色
    QString qss;
    if (cpuPercent < 75) {
        qss = "QProgressBar{background:rgb(128,128,128);color:black} QProgressBar::chunk{border-radius:5px;background:rgb(0,128,0)}";
    }
    else if (cpuPercent < 90) {
        qss = "QProgressBar{background:rgb(128,128,128);color:black} QProgressBar::chunk{border-radius:5px;background:rgb(128,128,0)}";
    }
    else {
        qss = "QProgressBar{background:rgb(128,128,128);color:black} QProgressBar::chunk{border-radius:5px;background:rgb(128,0,0)}";
    }

    m_prgrssBrCPU->setStyleSheet(qss);

#endif
#else
    if (process->state() == QProcess::NotRunning) {
        totalNew = idleNew = 0;
        process->start("cat /proc/stat");
    }
#endif
}

void ShowCPUMemory::GetSpace()
{
#ifdef Q_OS_WIN
    QFileInfoList list = QDir::drives();

    if (list.size() < 1) {
        return;
    }

    auto dataSavePath = SystemSetting::getInstance().GetSystemSetting().strSaveImagePath;
    QDir dir(dataSavePath);
    if (dir.exists())
        dir.setPath(dataSavePath);
    else
        dir.setPath(QDir::currentPath());

    QString dirName = dir.absoluteFilePath(dataSavePath);
    LPCWSTR lpcwstrDriver = (LPCWSTR)dirName.utf16();
    ULARGE_INTEGER liFreeBytesAvailable, liTotalBytes, liTotalFreeBytes;
    if (GetDiskFreeSpaceEx(lpcwstrDriver, &liFreeBytesAvailable, &liTotalBytes, &liTotalFreeBytes)) {
        QString use = QString::number((double)(liTotalBytes.QuadPart - liTotalFreeBytes.QuadPart) / GB, 'f', 1);
        use += "G";
        QString free = QString::number((double)liTotalFreeBytes.QuadPart / GB, 'f', 1);
        free += "G";
        QString all = QString::number((double)liTotalBytes.QuadPart / GB, 'f', 1);
        all += "G";
        int percent = 100 - ((double)liTotalFreeBytes.QuadPart / liTotalBytes.QuadPart) * 100;

        m_prgrssBrSpace->setValue(percent);
        m_prgrssBrSpace->setFormat(tr("%1盘空间: %2/%3(GB)").arg(dirName[0]).arg(use).arg(all));

        //根据数值确定进度条颜色
        QString qss;
        if (percent < 75) {
            qss = "QProgressBar{background:rgb(128,128,128);color:black} QProgressBar::chunk{border-radius:5px;background:rgb(0,128,0)}";
        }
        else if (percent < 90) {
            qss = "QProgressBar{background:rgb(128,128,128);color:black} QProgressBar::chunk{border-radius:5px;background:rgb(128,128,0)}";
        }
        else {
            qss = "QProgressBar{background:rgb(128,128,128);color:black} QProgressBar::chunk{border-radius:5px;background:rgb(128,0,0)}";
        }

        m_prgrssBrSpace->setStyleSheet(qss);
    }

#endif
}

void ShowCPUMemory::GetMemory()
{
#ifdef Q_OS_WIN
#if (QT_VERSION >= QT_VERSION_CHECK(4,8,7))
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    memoryPercent = statex.dwMemoryLoad;
    memoryAll = statex.ullTotalPhys / MB;
    memoryFree = statex.ullAvailPhys / MB;
    memoryUse = memoryAll - memoryFree;

    m_prgrssBrMemory->setValue(memoryPercent);
    m_prgrssBrMemory->setFormat(tr("内存使用率:%1%").arg(memoryPercent));;

    QString qss;
    if (memoryPercent < 75) {
        qss = "QProgressBar{background:rgb(128,128,128);color:black} QProgressBar::chunk{border-radius:5px;background:rgb(0,128,0)}";
    }
    else if (memoryPercent < 90) {
        qss = "QProgressBar{background:rgb(128,128,128);color:black} QProgressBar::chunk{border-radius:5px;background:rgb(128,128,0)}";
    }
    else {
        qss = "QProgressBar{background:rgb(128,128,128);color:black} QProgressBar::chunk{border-radius:5px;background:rgb(128,0,0)}";
    }

    m_prgrssBrMemory->setStyleSheet(qss);

#endif
#else
    if (process->state() == QProcess::NotRunning) {
        process->start("cat /proc/meminfo");
    }
#endif
}

void ShowCPUMemory::ReadData()
{
    while (!process->atEnd()) {
        QString s = QLatin1String(process->readLine());
        if (s.startsWith("cpu")) {
            QStringList list = s.split(" ");
            idleNew = list.at(5).toInt();
            foreach(QString value, list) {
                totalNew += value.toInt();
            }

            int total = totalNew - totalOld;
            int idle = idleNew - idleOld;
            cpuPercent = 100 * (total - idle) / total;
            totalOld = totalNew;
            idleOld = idleNew;
            break;
        }
        else if (s.startsWith("MemTotal")) {
            s = s.replace(" ", "");
            s = s.split(":").at(1);
            memoryAll = s.left(s.length() - 3).toInt() / KB;
        }
        else if (s.startsWith("MemFree")) {
            s = s.replace(" ", "");
            s = s.split(":").at(1);
            memoryFree = s.left(s.length() - 3).toInt() / KB;
        }
        else if (s.startsWith("Buffers")) {
            s = s.replace(" ", "");
            s = s.split(":").at(1);
            memoryFree += s.left(s.length() - 3).toInt() / KB;
        }
        else if (s.startsWith("Cached")) {
            s = s.replace(" ", "");
            s = s.split(":").at(1);
            memoryFree += s.left(s.length() - 3).toInt() / KB;
            memoryUse = memoryAll - memoryFree;
            memoryPercent = 100 * memoryUse / memoryAll;
            break;
        }
    }

    m_prgrssBrMemory->setValue(memoryPercent);
    m_prgrssBrMemory->setFormat(tr("内存使用率:%1%").arg(memoryPercent));;
    m_prgrssBrCPU->setValue(cpuPercent);
    m_prgrssBrCPU->setFormat(tr("CPU使用率:%1%").arg(cpuPercent));
}