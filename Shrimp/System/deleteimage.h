#pragma once

#include <QFileInfoList>
#include <QMap>
#include <QMutex>
#include <QQueue>
#include <QString>
#include <QThread>
#include <QVector>

class DeleteImage : public QThread
{
public:
    DeleteImage();
    ~DeleteImage();
    virtual void run();
    bool delImgRunning = false;
    bool getMemoryStatus();
    bool getFileName_date(const QString& imgPath_time);
    bool deleteImg();

private:
    double dTotalBytes;
    double dFreeBytes;
    QMap<QString, QQueue<QString>> m_panelInfoInDateQVec;   // key--保存的是日期文件夹绝对路径，value--保存的是某个日期文件夹下的所有文件队列
};

extern DeleteImage delImg;
