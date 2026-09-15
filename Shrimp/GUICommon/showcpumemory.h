#ifndef SHOWCPUMEMORY_H
#define SHOWCPUMEMORY_H

#include <QDebug>
#include <QObject>
#include <QProgressBar>

class QLabel;
class QTimer;
class QProcess;

class ShowCPUMemory : public QObject
{
    Q_OBJECT
public:
    explicit ShowCPUMemory(QObject* parent = 0);

    void SetBar(QProgressBar* space, QProgressBar* mempory, QProgressBar* cpu);
    void Start(int interval);
    void Stop();

private slots:
    void GetCPU();
    void GetSpace();
    void GetMemory();
    void ReadData();

private:
    int totalNew, idleNew, totalOld, idleOld;
    int cpuPercent;

    int memoryPercent;
    int memoryAll;
    int memoryUse;
    int memoryFree;

    QTimer* timerSpace;             //定时器获取C盘空间
    QTimer* timerCPU;               //定时器获取CPU信息
    QTimer* timerMemory;            //定时器获取内存信息
    QProcess* process;
    QProgressBar* m_prgrssBrMemory; //显示内存空间
    QProgressBar* m_prgrssBrCPU;    //显示CPU信息
    QProgressBar* m_prgrssBrSpace;  //显示C盘存储空间
};

#endif // SHOWCPUMEMORY_H
