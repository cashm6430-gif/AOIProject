#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QString>

enum class UserRole
{
    None = -1,  // 未登录
    Operator,   // 操作员
    Engineer,   // 工程师
    Admin       // 管理员
};

struct UserInfo
{
    UserRole userRole = UserRole::None;  //用户角色
    QString password;   //密码
};

QString getUserName(UserRole userRole);

class UserManager final
{
public:
    static UserManager& instance();
    // 设置当前用户信息
    void setCurrentUser(const UserInfo& userInfo);
    // 获取当前用户信息
    UserInfo getCurrentUser() const { return m_currentUser; }
    // 清除当前用户信息
    void clearCurrentUser();
    // 加载用户配置信息
    void loadUserInfo();
    // 保存用户信息
    void saveUserInfo(const UserInfo* userInfo);
    // 验证用户信息
    bool checkPassword(UserRole userRole, const QString& password) const;

private:
    UserManager();
    UserManager(const UserManager&) = delete;
    UserManager& operator=(const UserManager&) = delete;
    UserManager(UserManager&&) = delete;
    UserManager& operator=(UserManager&&) = delete;

private:
    UserInfo m_currentUser;  //当前登录用户信息
};

#endif // USERMANAGER_H
