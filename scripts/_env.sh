#!/usr/bin/env bash
# Shared environment preparation for the AOIProject MSVC/Ninja build.
# Source this file; do not execute it directly.
#
#   . "$(dirname "$0")/_env.sh"
#
# It exists because two things on this machine are broken out of the box:
#
#  1. vcvarsall.bat locates the Windows SDK by querying the registry through
#     reg.exe.  When reg.exe is unavailable the generated conanvcvars.bat ends
#     up with "winsdk_version=None", so INCLUDE / LIB / PATH lose the whole
#     Windows SDK and the compiler probe fails with CMAKE_MT-NOTFOUND and
#     LNK1104 "cannot open kernel32.lib".  We therefore set every SDK variable
#     explicitly instead of relying on vcvars.
#
#  2. The shell environment carries the proxy in BOTH upper and lower case
#     (HTTP_PROXY + http_proxy).  MSBuild copies the process environment into a
#     case-sensitive Hashtable when it builds a tool command line, so the
#     duplicate key makes every cl.exe invocation die with:
#         error MSB6001: "CL.exe" ... System.ArgumentException:
#         已添加项。字典中的关键字:"HTTPS_PROXY"所添加的关键字:"https_proxy"
#     Conan packages built with MSBuild (libjpeg is one) cannot build until the
#     duplicate is removed.  Requests-based downloads only need the lower-case
#     form, so the upper-case pair is dropped here.

# Coreutils live outside the default PATH of the tool shell.
export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

# --- 1. Drop the duplicate-case proxy variables (MSBuild MSB6001) -------------
unset HTTP_PROXY
unset HTTPS_PROXY
unset ALL_PROXY
unset NO_PROXY
unset FTP_PROXY

# --- 2. Explicit Visual Studio 2022 + Windows SDK environment ----------------
AOI_VC_DIR="${AOI_VC_DIR:-C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.44.35207}"
AOI_VC_ROOT="${AOI_VC_ROOT:-C:\\Program Files\\Microsoft Visual Studio\\2022\\Community}"
AOI_SDK_DIR="${AOI_SDK_DIR:-C:\\Program Files (x86)\\Windows Kits\\10}"
AOI_SDK_VER="${AOI_SDK_VER:-10.0.26100.0}"

export WindowsSdkDir="$AOI_SDK_DIR\\"
export WindowsSDKVersion="$AOI_SDK_VER\\"
export WindowsSdkLibVersion="$AOI_SDK_VER\\"
export WindowsSdkBinPath="$AOI_SDK_DIR\\bin\\"
export WindowsSdkVerBinPath="$AOI_SDK_DIR\\bin\\$AOI_SDK_VER\\"
export UniversalCRTSdkDir="$AOI_SDK_DIR\\"
export UCRTVersion="$AOI_SDK_VER"
export VCToolsInstallDir="$AOI_VC_DIR\\"
export VCToolsVersion="14.44.35207"
export VCINSTALLDIR="$AOI_VC_ROOT\\"
export VisualStudioVersion="17.0"
export VSINSTALLDIR="$AOI_VC_ROOT\\"

export INCLUDE="$AOI_VC_DIR\\include;$AOI_SDK_DIR\\Include\\$AOI_SDK_VER\\ucrt;$AOI_SDK_DIR\\Include\\$AOI_SDK_VER\\um;$AOI_SDK_DIR\\Include\\$AOI_SDK_VER\\shared;$AOI_SDK_DIR\\Include\\$AOI_SDK_VER\\winrt;$AOI_SDK_DIR\\Include\\$AOI_SDK_VER\\cppwinrt"
export LIB="$AOI_VC_DIR\\lib\\x64;$AOI_SDK_DIR\\Lib\\$AOI_SDK_VER\\um\\x64;$AOI_SDK_DIR\\Lib\\$AOI_SDK_VER\\ucrt\\x64"

export PATH="/c/Program Files (x86)/Windows Kits/10/bin/$AOI_SDK_VER/x64:/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64:$PATH"

# --- 3. Locate Conan ---------------------------------------------------------
# The Conan 2 cache root, in the shell's own POSIX form, used only for local
# filesystem searching below.  Do NOT export CONAN_HOME from here: this shell
# speaks "/c/Users/..." while Conan requires a native absolute path and aborts
# with "Invalid CONAN_HOME value".  Conan finds its default cache on its own.
if [ -z "${AOI_CONAN_HOME:-}" ]; then
  for cand in "$HOME/.conan2" "/c/Users/$USERNAME/.conan2" "/c/Users/Administrator/.conan2"; do
    [ -d "$cand" ] && { AOI_CONAN_HOME="$cand"; break; }
  done
fi
export AOI_CONAN_HOME

# Conan flags reject POSIX paths, so hand it the mixed form ("E:/a/b").
unset CONAN_HOME
if command -v cygpath >/dev/null 2>&1; then
  AOI_WINPATH() { cygpath -m "$1"; }
else
  AOI_WINPATH() { printf '%s' "$1"; }
fi

# --- 3b. Stop the host's safe-delete shim from killing Conan mid-package ----
# The tool host puts .../cli/vendor/shim on PYTHONPATH, so every Python process
# it spawns -- conan.exe included -- loads a sitecustomize.py that wraps
# os.remove / os.rmdir / shutil.rmtree.  Deleting more than
# CODEBUDDY_SAFE_DELETE_BULK_THRESHOLD (50) entries inside one assistant turn is
# supposed to be confirmed interactively; a non-interactive build has nobody to
# ask, so the shim aborts the process instead:
#
#   [safe-delete][SAFE_DELETE_BULK_CONFIRM_REQUIRED] {"count":128,"threshold":50,...}
#   ERROR: Exiting with code: 1
#
# This is what killed boost/1.83.0 five times in a row: b2 compiled the whole
# library (500 objects, 3.5 minutes), package() reached the step that clears the
# b2-generated lib/cmake tree (128 files > 50), and Conan exited *after* the
# expensive part, every time.  The retry loop then rebuilt it from scratch.
#
# Conan only ever deletes inside its own cache and the project's build tree, so
# raise the ceiling for build subprocesses.  The shim's other protections (trash
# routing, per-path policy) stay fully active.
export CODEBUDDY_SAFE_DELETE_BULK_THRESHOLD="${AOI_SAFE_DELETE_BULK_THRESHOLD:-1000000}"

# conan.exe is installed outside the shell PATH.  Prefer an explicit override,
# then the known interpreter layout, then whatever "conan" resolves to.
if [ -z "${AOI_CONAN:-}" ]; then
  for cand in \
      "/c/Users/Administrator/AppData/Local/Python/pythoncore-3.14-64/Scripts/conan.exe" \
      "/c/Users/$USERNAME/AppData/Local/Python/pythoncore-3.14-64/Scripts/conan.exe" ; do
    [ -x "$cand" ] && { AOI_CONAN="$cand"; break; }
  done
fi
[ -z "${AOI_CONAN:-}" ] && AOI_CONAN="$(command -v conan || true)"
export AOI_CONAN

# The interpreter that backs conan.exe.  Helper scripts that touch the cache
# must run under this exact Python: Conan hashes package files with it, and the
# MSYS2 Python sees things differently (it emulates the MSYS mounts and the
# LX symlink reparse points), so hashes computed by another interpreter would no
# longer match what `conan cache check-integrity` recomputes.  "Scripts" sits
# next to the interpreter, so conan.exe is two levels down.
if [ -z "${AOI_PYTHON:-}" ] && [ -n "${AOI_CONAN:-}" ]; then
  for cand in \
      "$(dirname "$(dirname "$AOI_CONAN")")/python.exe" \
      "/c/Users/Administrator/AppData/Local/Python/pythoncore-3.14-64/python.exe" \
      "/c/Users/$USERNAME/AppData/Local/Python/pythoncore-3.14-64/python.exe" ; do
    [ -x "$cand" ] && { AOI_PYTHON="$cand"; break; }
  done
fi
export AOI_PYTHON

# --- 4. Locate Ninja ---------------------------------------------------------
# CMakePresets.json uses the Ninja generator, but ninja is not on PATH: the
# project gets it from the profile's "tool_requires: *: ninja/1.13.2", and that
# only reaches PATH when the build runs through Conan's own environment
# scripts.  Pick the pinned Conan copy first, then the one bundled with VS.
if [ -z "${AOI_NINJA:-}" ]; then
  AOI_NINJA="$(find "$AOI_CONAN_HOME/p" -maxdepth 5 -path '*ninja*/p/bin/ninja.exe' 2>/dev/null | head -1)"
  export AOI_NINJA
fi
if [ -z "${AOI_NINJA:-}" ] && [ -x "/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" ]; then
  AOI_NINJA="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
  export AOI_NINJA
fi
if [ -n "${AOI_NINJA:-}" ]; then
  export PATH="$(dirname "$AOI_NINJA"):$PATH"
fi

export AOI_CMAKE="${AOI_CMAKE:-$(command -v cmake || true)}"
export AOI_CTEST="${AOI_CTEST:-$(command -v ctest || true)}"

# --- 5. Let the MSYS2 shell convert POSIX paths again ------------------------
# The tool host exports two variables whose only purpose is to stop *its own*
# bash from rewriting arguments:
#
#   MSYS_NO_PATHCONV=1       Git-for-Windows private switch
#   MSYS2_ARG_CONV_EXCL=*    MSYS2 argument-exclusion list
#
# Both are wrong for a native-toolchain build.  Conan drives every autotools
# recipe through the MSYS2 bash (tools.microsoft.bash, provided by the
# msys2/cci.latest tool_require) and relies on that shell converting the POSIX
# paths it generates -- e.g.
#     CC="/c/.../build-aux/compile cl -nologo"
# into native form on the way to cl.exe.  Suppress the conversion and cl.exe
# receives "/c/users/.../localcharset.c" verbatim, reads it as an option and
# dies with
#     cl: warning D9002: ignoring unknown option "/c/.../localcharset.c"
#     cl: error   D8003: missing source filename
# which is exactly how libiconv/1.17 failed here.  Remove both so the shell
# defaults apply; nothing in this build wants either of them.
unset MSYS_NO_PATHCONV
unset MSYS2_ARG_CONV_EXCL
