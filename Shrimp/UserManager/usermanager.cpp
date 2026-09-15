#include "usermanager.h"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>

static QString iniFilePath()
{
    const QString appDir = QApplication::applicationDirPath();
    QDir dir(appDir);
    if (!dir.exists("system"))
    {
        dir.mkpath("system");
    }

    return dir.absoluteFilePath("system/UserConfig.ini");
}

static QString roleKey(UserRole role)
{
    switch (role)
    {
    case UserRole::Operator: return "Operator";
    case UserRole::Engineer: return "Engineer";
    case UserRole::Admin:    return "Admin";
    default:                 return "None";
    }
}

static UserRole roleFromInt(int v)
{
    switch (v)
    {
    case static_cast<int>(UserRole::Operator): return UserRole::Operator;
    case static_cast<int>(UserRole::Engineer): return UserRole::Engineer;
    case static_cast<int>(UserRole::Admin):    return UserRole::Admin;
    default:                                   return UserRole::None;
    }
}

UserManager::UserManager()
{
    loadUserInfo();
}

UserManager& UserManager::instance()
{
    static UserManager instance;
    return instance;
}

void UserManager::setCurrentUser(const UserInfo& userInfo)
{
    m_currentUser = userInfo;
}

void UserManager::clearCurrentUser()
{
    m_currentUser = UserInfo();
    // 同步到配置文件
    saveUserInfo(nullptr);
}

void UserManager::loadUserInfo()
{
    const QString& path = iniFilePath();
    const bool needInit = !QFile::exists(path);

    QSettings ini(path, QSettings::IniFormat);
    ini.setIniCodec("UTF-8");

    if (needInit)
    {
        // 初始化默认密码
        ini.beginGroup("Users");
        ini.setValue("Operator", "123");
        ini.setValue("Engineer", "456");
        ini.setValue("Admin", "888");
        ini.endGroup();

        // 初始化当前用户为未登录
        ini.beginGroup("Current");
        ini.setValue("Role", static_cast<int>(UserRole::None));
        ini.setValue("Password", "");
        ini.endGroup();

        ini.sync();
    }

    //// 读取当前用户
    //ini.beginGroup("Current");
    //const int roleInt = ini.value("Role", static_cast<int>(UserRole::None)).toInt();
    //const QString& curPwd = ini.value("Password", "").toString();
    //ini.endGroup();

    //m_currentUser.userRole = roleFromInt(roleInt);
    //m_currentUser.password = curPwd;
}

void UserManager::saveUserInfo(const UserInfo* userInfo)
{
    const QString& path = iniFilePath();
    QSettings ini(path, QSettings::IniFormat);
    ini.setIniCodec("UTF-8");

    if (userInfo)
    {
        // 可选：更新对应角色的密码配置
        if (userInfo->userRole == UserRole::Operator ||
            userInfo->userRole == UserRole::Engineer ||
            userInfo->userRole == UserRole::Admin)
        {
            ini.beginGroup("Users");
            ini.setValue(roleKey(userInfo->userRole), userInfo->password);
            ini.endGroup();
        }

        // 更新当前用户
        m_currentUser = *userInfo;
    }

    ini.beginGroup("Current");
    ini.setValue("Role", static_cast<int>(m_currentUser.userRole));
    ini.setValue("Password", m_currentUser.password);
    ini.endGroup();

    ini.sync();
}

bool UserManager::checkPassword(UserRole userRole, const QString& password) const
{
    // 从配置读取对应角色密码进行校验
    QSettings ini(iniFilePath(), QSettings::IniFormat);
    ini.setIniCodec("UTF-8");

    ini.beginGroup("Users");
    const QString& expected = ini.value(roleKey(userRole), "").toString();
    ini.endGroup();

    if (expected.isEmpty())
        return false;

    return password == expected;
}

QString getUserName(UserRole userRole)
{
    switch (userRole)
    {
    case UserRole::Operator:
        return "操作员";
    case UserRole::Engineer:
        return "工程师";
    case UserRole::Admin:
        return "管理员";
    default:
        return "未登录";
    }
}