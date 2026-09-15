#include "deleteimage.h"
#include "systemsetting.h"
#include "windows.h"
#include "xlogger.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QQueue>

DeleteImage delImg;

DeleteImage::DeleteImage()
{}

DeleteImage::~DeleteImage()
{
    requestInterruption();
    quit();
    wait();
}
bool DeleteImage::getFileName_date(const QString& imgPath)
{
    m_panelInfoInDateQVec.clear();
    QDir dir(imgPath);
    if (!dir.exists())
        return false;
    //dir.setFilter(QDir::NoDotAndDotDot);//除了目录，其他过滤掉
    QString filename;
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::Name);
    QFileInfoList DateDirInfoList = dir.entryInfoList();
    for (int i = 0; i < DateDirInfoList.size(); i++)
    {
        QDir DateDir(DateDirInfoList[i].absoluteFilePath());
        if (DateDir.exists())
        {
            QQueue<QString> allPanelInDateDir;
            DateDir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs);
            DateDir.setSorting(QDir::Time | QDir::Reversed);
            QFileInfoList panelInfoList = DateDir.entryInfoList();
            for (int j = 0; j < panelInfoList.size(); j++)
            {
                allPanelInDateDir.enqueue(panelInfoList[j].absoluteFilePath());
            }
            m_panelInfoInDateQVec.insert(DateDirInfoList[i].absoluteFilePath(), allPanelInDateDir);
        }
    }

    return true;
}

bool DeleteImage::deleteImg()
{
    bool ret = true;
    QDir dir;
    if (m_panelInfoInDateQVec.size() > 0)
    {
        QQueue<QString>* allPanelInDateDir = &m_panelInfoInDateQVec.first();
        if (allPanelInDateDir->empty())
        {
            auto currDatePath = m_panelInfoInDateQVec.firstKey();
            if (dir.exists(currDatePath))
            {
                dir.setPath(currDatePath);

                auto datePath = currDatePath.toLocal8Bit().toStdString();

                if (!dir.removeRecursively())
                    xWarning("当前 {} 空文件夹删除失败", datePath);
                else
                    xInfo("当前 {} 空文件夹删除成功", datePath);
            }

            m_panelInfoInDateQVec.remove(currDatePath);
        }
        else
        {
            auto panelPath = allPanelInDateDir->dequeue();
            if (dir.exists(panelPath))
            {
                dir.setPath(panelPath);

                auto stdPanelPath = panelPath.toLocal8Bit().toStdString();
                if (!dir.removeRecursively())
                    xWarning("当前 {} panel删图失败", stdPanelPath);
                else
                    xInfo("当前{} panel删图成功", stdPanelPath);
            }
        }
    }
    else
        ret = false;
    return ret;
}

void DeleteImage::run()
{
    xInfo("进入删图程序线程");
    dFreeBytes = 1024.0;
    SystemParams sysSetting = SystemSetting::getInstance().GetSystemSetting();
    getFileName_date(sysSetting.strSaveImagePath);
    while (delImgRunning)
    {
        if (!sysSetting.autoDeleteImg)
            break;
        getMemoryStatus();
        if (m_panelInfoInDateQVec.isEmpty())
        {
            xInfo("目前文件夹为空，即将退出删图程序线程");
            break;
        }
        int remainMem = sysSetting.remainSpace;
        if (dFreeBytes < remainMem)
        {
            xInfo("当前剩余空间 {} GB，即将开始删图", dFreeBytes);
            if (!deleteImg())
            {
                xInfo("目前文件夹为空，即将退出删图程序线程");
                break;
            }
            int intervalsec = sysSetting.deleteTimeInterval;   // mPara.mparaEnv.intervalSec;
            intervalsec = intervalsec >= 1 ? intervalsec : 1;   //add by bss 2020-3-23 设置最小值为 1
            QThread::msleep(intervalsec * 1000);
        }
        else
            break;
    }

    xInfo("退出删图程序线程");

    delImgRunning = false;
}

bool DeleteImage::getMemoryStatus()
{
    SystemParams sysSetting = SystemSetting::getInstance().GetSystemSetting();
    //QString strDisk = QDir::currentPath();
    //LPCWSTR lpcwstrDriver = (LPCWSTR)strDisk.utf16();
    QDir dir(sysSetting.strSaveImagePath);
    if (!dir.exists())
        dir.mkpath(sysSetting.strSaveImagePath);

    LPCWSTR lpcwstrDriver = (LPCWSTR)sysSetting.strSaveImagePath.utf16();
    ULARGE_INTEGER lFreeBytesAvailable, lTotalBytesTemp, lTotalFreeBytes;
    if (!GetDiskFreeSpaceEx(lpcwstrDriver, &lFreeBytesAvailable, &lTotalBytesTemp, &lTotalFreeBytes))
    {
        QMessageBox::warning(0, "Warning", "Acquire Disk Space Failed !");
        dTotalBytes = -1;
        dFreeBytes = -1;
        return false;
    }
    //unit : GB
    dTotalBytes = lTotalBytesTemp.QuadPart / 1024.0 / 1024 / 1024;
    dFreeBytes = lTotalFreeBytes.QuadPart / 1024.0 / 1024 / 1024;
    //qDebug() << " totalbytes : " << dTotalBytes << " dfreebytes: " <<dFreeBytes;
    return true;
}