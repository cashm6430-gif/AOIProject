;--------------------------------
; setup.nsi
;--------------------------------

!include "FileFunc.nsh"
!include "MUI2.nsh"

Unicode true
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!define APP_NAME      "Shrimp"
!define APP_EXE       "Shrimp.exe"
!define APP_EXE_PATH  "${__FILEDIR__}\\${APP_EXE}"

!getdllversion "${APP_EXE_PATH}" APP_VER_
!define APP_VERSION "${APP_VER_1}.${APP_VER_2}.${APP_VER_3}.${APP_VER_4}"
; 若主程序不在同目录，可临时改为手动版本：
; !define APP_VERSION "1.0.0.520"

Name "Shrimp"
OutFile "${APP_NAME}_${APP_VERSION}_x64_Setup.exe"

; 64位默认安装目录（不带 x86）
InstallDir "D:\Shrimp"
InstallDirRegKey HKLM "Software\Shrimp" "InstallDir"

!define APP_PUBLISHER "Shrimp"
!define UNINST_KEY    "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shrimp"

BrandingText "${APP_NAME} Setup"
ShowInstDetails show
ShowUninstDetails show

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\nsis3-metro.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\nsis3-metro.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Header\nsis3-metro.bmp"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "SimpChinese"

Section "Install"

  ; 必须在 Section 内
  SetRegView 64

  ; 保留目录结构打包当前目录
  SetOutPath "$INSTDIR"
  File /r /x "*.lib" /x "*.exp" /x "*.ilk" /x "*.ipdb" /x "*.iobj" "*.*"

  ; 记录安装目录
  WriteRegStr HKLM "Software\Shrimp" "InstallDir" "$INSTDIR"

  ; 写卸载器
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; 快捷方式
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\卸载 ${APP_NAME}.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0

  ; 应用和功能
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1

  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" "$0"

  ; 可选：安装 VC 运行库（存在才执行）
  IfFileExists "$INSTDIR\VC_redist.x64.exe" 0 +3
    ExecWait '"$INSTDIR\VC_redist.x64.exe" /install /quiet /norestart' $1
    DetailPrint "VC Runtime return code: $1"

SectionEnd

Section "Uninstall"

  ; 必须在 Section 内
  SetRegView 64

  Delete "$DESKTOP\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\卸载 ${APP_NAME}.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"

  DeleteRegKey HKLM "${UNINST_KEY}"
  DeleteRegKey HKLM "Software\Shrimp"

  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR"

SectionEnd
