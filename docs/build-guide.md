# AOIProject 编译指南

本文面向首次接手工程的开发者，说明如何在 Windows 上从源码生成、测试并启动
`Shrimp`。日常开发优先使用 Git Bash 下的脚本；需要调试 CMake 或 Visual Studio
时再使用后文的手动命令。

> 本工程不是提交原生 `.sln` 文件的工程。CMake 在配置后为 Ninja 或 Visual Studio
> 生成构建文件；所有生成物都位于 `out/`，不应提交。

## 1. 构建约定

所有会链接在一起的目标必须使用下列一致的 ABI：

| 项目 | 约定 |
|---|---|
| 平台 | Windows x64 |
| 编译器 | Visual Studio 2022、MSVC v143（194） |
| 语言标准 | C++20 |
| 运行库 | 动态 CRT：Debug `/MDd`，Release `/MD` |
| 构建系统 | CMake + Ninja（默认）或 CMake 的 VS2022 生成器 |
| 依赖管理 | Conan 2，依赖版本由 `conan.lock` 固定 |

`TaskControl`、`Runtime`、`Shrimp` 共享同一套 Conan 依赖。Qt 6.8.3 是
`TaskControl` 的必需依赖；`Shrimp` 额外依赖公司私有的
`vtk/9.5.0@aoi/stable`。

## 2. 首次准备

1. 克隆仓库并进入根目录。

   ```bash
   git clone <仓库地址> AOIProject
   cd AOIProject
   ```

2. 安装 Visual Studio 2022 的“使用 C++ 的桌面开发”工作负载，并确认已安装 MSVC
   v143、Windows 10/11 SDK、CMake 和 Ninja。

3. 安装 Python 及 Conan 2，并确认命令可用。

   ```powershell
   python -m pip install "conan>=2,<3"
   conan --version
   cmake --version
   ninja --version
   ```

4. 配置公司的 Conan remote（首次需要登录；账号凭据请向项目管理员获取）。

   ```powershell
   conan remote list
   conan remote login <公司-remote> <用户名>
   ```

   remote 中必须有锁文件指定的私有 VTK **recipe revision**，并且同时有符合
   当前 ABI 的 Debug 或 Release 二进制包。不要为了“找得到 VTK”而修改
   `conan.lock`。

5. 使用 **Git Bash** 执行 `scripts/*.sh`。脚本会设置 VS2022 和 Windows SDK 环境；
   不需要、也不建议手工运行 `vcvarsall.bat`。

## 3. 最短的 Debug 编译流程

新机器需要从 remote 获取依赖时，使用在线模式：

```bash
AOI_OFFLINE=0 bash scripts/build-debug.sh deps configure build test run
```

命令依次完成：

1. `conan install` 到 `out/conan/debug`；
2. 使用 `debug` CMake preset 配置到 `out/build/debug`；
3. 用 Ninja 构建；
4. 执行 `TaskControl.CoreArchitecture` 和 `Runtime.Scheduler`；
5. 运行 `Shrimp --selftest`。

若本机没有私有 VTK 二进制包、且确认需要在本机重建它，才在前面加上 `vtk` 阶段：

```bash
AOI_OFFLINE=0 bash scripts/build-debug.sh vtk deps configure build test run
```

VTK 的首次构建耗时很长；正常开发应优先从公司 Conan remote 或离线缓存取得它。
阶段可单独重跑，例如：

```bash
bash scripts/build-debug.sh configure build test run
bash scripts/build-debug.sh test
```

成功后，程序及完整运行时目录在：

```text
out/build/debug/bin/Shrimp.exe
out/build/debug/bin/Themes/
```

CMake 会自动部署 Qt 插件、Qt/OpenCV/PCL/VTK DLL，以及整个 `Themes` 文件夹。
运行时不要只复制 `Shrimp.exe`，必须保留其所在的 `bin` 目录结构。

## 4. 使用转移或离线 Conan 缓存

如果已经拿到缓存包并通过 `conan cache restore` 恢复到独立目录，把该目录以 Git
Bash 路径设置给 `AOI_CONAN_HOME`：

```bash
AOI_CONAN_HOME=/e/cache/conan-home-debug-194-clean \
  bash scripts/build-debug.sh deps configure build test run
```

脚本会把 `/e/...` 自动转换为 Conan 需要的 Windows 路径。缓存完整时默认离线模式
即可工作；缺少 recipe、源码或二进制包时改用 `AOI_OFFLINE=0`，不要混用 Debug 和
Release 包。

要生成或验证可交付的缓存包，使用：

```bash
bash scripts/package-cache.sh
bash scripts/verify-bundle.sh
```

Release 包使用相同脚本，并设置 `AOI_CACHE_CONFIG=release`。私有包上传和缓存
核查的入口见 [`scripts/README.md`](../scripts/README.md)。

## 5. 手动构建 Debug / Release

脚本适合日常工作；下面的命令适合定位依赖图或 CI。必须先运行 Conan install，再
运行对应的 CMake preset，且两者的输出目录不能混用。

### Debug

```powershell
conan install . --lockfile=conan.lock --build=missing `
  -pr:h=conan/profiles/windows-msvc-v143-x64 `
  -pr:b=conan/profiles/windows-msvc-v143-x64 `
  -s:h build_type=Debug -o "&:with_shrimp=True" `
  --output-folder=out/conan/debug

cmake --preset debug -DAOI_BUILD_TASKCONTROL_TESTS=ON
cmake --build --preset build-debug
ctest --test-dir out/build/debug --output-on-failure
```

### Release

```powershell
conan install . --lockfile=conan.lock --build=missing `
  -pr:h=conan/profiles/windows-msvc-v143-x64 `
  -pr:b=conan/profiles/windows-msvc-v143-x64 `
  -s:h build_type=Release -o "&:with_shrimp=True" `
  --output-folder=out/conan/release

cmake --preset release -DAOI_BUILD_TASKCONTROL_TESTS=ON
cmake --build --preset build-release
ctest --test-dir out/build/release --output-on-failure -C Release
```

Release 可执行文件位于 `out/build/release/bin/Shrimp.exe`。`RelWithDebInfo`
复用 Release Conan 依赖，使用 `relwithdebuginfo` / `build-relwithdebuginfo`
preset。

`--output-folder` 请始终使用长选项。命令中同时存在 `-o` 时，短写法 `-of` 可能被
Conan 解析为另一个 option。

## 6. 在 Visual Studio 中打开

Visual Studio 不需要仓库中预生成的 `.sln`。先为 VS preset 生成依赖：

```powershell
conan install . --lockfile=conan.lock --build=missing `
  -pr:h=conan/profiles/windows-msvc-v143-x64 `
  -pr:b=conan/profiles/windows-msvc-v143-x64 `
  -s:h build_type=Debug -o "&:with_shrimp=True" `
  --output-folder=out/conan/vs2022-debug

cmake --preset vs2022-debug
```

随后可直接打开 `out/build/vs2022-debug/AOIProject.sln`，或在 Visual Studio 中
“打开本地文件夹”选择仓库根目录。生成的 `.sln`、`.vs` 与 `out/` 都是本机状态，
不提交到 Git。Release 对应 `vs2022-release` preset 和
`out/conan/vs2022-release` 输出目录。

Ninja 与 Visual Studio 使用的是同一套 MSVC `cl.exe`；区别只在 CMake 生成器和
构建文件形式。不要让二者共用同一个 `out/build/...` 目录。

## 7. 常见问题

| 现象 | 处理方式 |
|---|---|
| `conan_toolchain.cmake` 找不到 | 先执行与 preset 对应的 `conan install`；检查 `--output-folder` 与 `CMakePresets.json` 中的路径一致。 |
| 找不到 VTK 或 lock 指纹不匹配 | 不要删除 `conan.lock`。确认 remote 或离线缓存具有锁定 revision 的、与 Debug/Release 匹配的 VTK 包。 |
| `Duplicate preset: conan-debug` | 重新执行 `bash scripts/build-debug.sh deps`；脚本会重建 git-ignore 的 `CMakeUserPresets.json`。 |
| 测试启动即报 `0xC0000135` | 重新执行构建。测试和 Shrimp 的 DLL 依赖由 CMake 部署到各自的运行目录；不要从输出目录外单独启动测试 exe。 |
| `Shrimp.exe` 启动缺 DLL / Qt plugin | 从 `out/build/<配置>/bin` 启动，并完整保留 DLL、Qt 插件子目录和 `Themes` 文件夹。 |
| 文件被终端安全软件替换为不可读内容 | 先执行 `python scripts/restore_plaintext_from_git.py --check`，确认后不带 `--check` 恢复 Git 已跟踪的 UTF-8 文本。不要把异常内容提交。 |

## 8. 提交前检查

```bash
bash scripts/build-debug.sh configure build test run
git status
```

不要提交：`out/`、`CMakeUserPresets.json`、`.vs/`、用户级 `.suo` / `.user` 文件或
Conan cache。提交依赖升级时应同时提交 `conanfile.py` 与重新生成的 `conan.lock`，并
确认 Debug 和 Release 的私有 VTK 包均已可从公司 remote 获取。

更深入的缓存格式、VTK recipe revision 及故障原因说明见
[`build.md`](build.md)。
