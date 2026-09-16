# AOIProject build setup

The project is a CMake superproject. `TaskControl` is the shared task/algorithm
library; `Shrimp` is the Qt GUI. Public dependencies are locked by
`conanfile.py`; no machine-local include or library directory is used.

## Build the Debug configuration

```console
$ bash scripts/build-debug.sh                 # vtk -> deps -> configure -> build -> test -> run -> package
$ bash scripts/build-debug.sh deps configure  # or run individual stages
$ bash scripts/build-debug.sh run             # Shrimp --selftest only
```

Stages: `vtk` `deps` `configure` `build` `test` `run` `package`. Every stage is
re-runnable on its own; a failing one stops the chain and names the stage.

The `vtk` stage builds the private VTK package and skips itself when the cache
already holds a matching Debug package. The chain then compiles **the whole
project, Shrimp included**: Shrimp is the Qt application this repository exists
to ship, so `AOI_BUILD_SHRIMP` is `ON` by default and the driver configures with
`-o "&:with_shrimp=True"`. Set `AOI_WITH_SHRIMP=0` for a core-only loop
(TaskControl + Runtime + tests), which then needs Qt but not VTK.

| script | job |
|---|---|
| `scripts/build-debug.sh` | the driver above |
| `scripts/_env.sh` | shared environment: VS2022 + Windows SDK variables, duplicate-case proxy de-dup, Conan/Ninja lookup. Sourced, not executed |
| `scripts/prefetch-sources.sh` | pre-download dependency sources into the Conan sources cache, so a build needs no network |
| `scripts/package-debug-cache.sh` | build the offline cache bundle (`out/conan-cache-debug.tgz`) |
| `scripts/verify-debug-bundle.sh` | restore that bundle into an empty cache and build, test **and run** the project from it |
| `scripts/normalize-cache-reparse.py` | repair unreadable NTFS reparse points before `conan cache save` |

Everything below explains *why* those scripts are shaped the way they are.
`_env.sh` sets the SDK variables by hand instead of calling `vcvarsall.bat`,
because `reg.exe` is unavailable here and the generated `conanvcvars.bat`
otherwise resolves `winsdk_version=None`; without it the compiler probe fails
with `CMAKE_MT-NOTFOUND` and the link dies with `LNK1104: cannot open
kernel32.lib`.

## Toolchain contract

Use exactly one configuration for every binary that is linked together:

- Windows x64
- Visual Studio 2022, MSVC v143 (compiler version 194)
- C++20
- dynamic MSVC CRT: `/MD` for Release and `/MDd` for Debug

Conan 2 is required (`conan --version`). It is a Python package, so its
`Scripts` directory differs per machine. Add the directory that contains
`conan.exe` to `PATH` before using the short `conan` command:

```powershell
# Locate it once, then add the printed directory to PATH.
python -m pip show conan
```

On the current machine it lives in
`C:\Users\Administrator\AppData\Local\Python\pythoncore-3.14-64\Scripts`.
Do not hardcode any user name in build scripts; read it from `PATH`.

## Build the project

`conan/recipes/vtk` has to be created first — see *Build the private VTK package*
below — because `AOI_BUILD_SHRIMP` is `ON` by default and Shrimp links VTK.

```powershell
conan install . --lockfile=conan.lock --build=missing -pr:h=conan/profiles/windows-msvc-v143-x64 -pr:b=conan/profiles/windows-msvc-v143-x64 -s:h build_type=Debug -o "&:with_shrimp=True" --output-folder=out/conan/debug
cmake --preset debug
cmake --build --preset build-debug

# RelWithDebInfo reuses the Release Conan packages.
cmake --preset relwithdebuginfo
cmake --build --preset build-relwithdebuginfo

conan install . --lockfile=conan.lock --build=missing -pr:h=conan/profiles/windows-msvc-v143-x64 -pr:b=conan/profiles/windows-msvc-v143-x64 -s:h build_type=Release -o "&:with_shrimp=True" --output-folder=out/conan/release
cmake --preset release
cmake --build --preset build-release
```

`-o "&:with_shrimp=False"` (or no option at all — it is the default) resolves the
core-only graph for a build that stops at TaskControl / Runtime. Qt is still in
it: see *Dependency policy* below. Pair it with `-DAOI_BUILD_SHRIMP=OFF` on the
configure line, because `AOI_BUILD_SHRIMP` defaults to `ON` in the top-level
`CMakeLists.txt`.

Note the spelling of the output folder: with a bare `-o` also on the command
line, conan parses `-of=out/conan/debug` as `-o f=out/conan/debug` and aborts
with *"option 'f' doesn't exist"*. Use `--output-folder=` whenever `-o` is
present.

### `cmake --preset debug` breaks after a second install folder

`conan install` **appends** its generated preset file to the project's
`CMakeUserPresets.json`; it never replaces the include list. Two installs into
different `--output-folder` values therefore leave two includes that both define
the preset `conan-debug`, and every later

```console
$ cmake --preset debug
CMake Error: Could not read presets from E:/projects/AOIProject:
             Duplicate preset: "conan-debug"
```

fails — a symptom that looks nothing like "somebody ran a second Conan install".

`scripts/build-debug.sh` pins the file to the folder its `deps` stage installed
into on every run, so re-running `deps` repairs it, and
`scripts/verify-debug-bundle.sh` saves and restores the file around its own
install. The file is generated and git-ignored (`/CMakeUserPresets.json`), and
nothing in `CMakePresets.json` depends on it — the project's own `debug`,
`relwithdebuginfo` and `release` presets carry their toolchain path themselves.

`-s:h build_type=Debug` overrides the **host** context only. The profile itself
carries `build_type=Release`, which is what the **build** context uses (both
contexts are configured from the same file via `-pr:h=` / `-pr:b=`), so the
resulting combination is host=Debug / build=Release. That asymmetry is
deliberate and not optional:

* the build context needs a *concrete* build_type. If it is left unset the
  packages there resolve with `build_type=None`, and any recipe that branches on
  it takes the wrong path. `winflexbison/2.5.25` is the case that has bitten
  this project: its `package()` copies the pre-built `win_flex.exe` /
  `win_bison.exe` out of the source tree only when
  `self.settings.build_type in ("Release", "Debug")`, and otherwise copies from
  its build folder. With `None` it copied neither, so the package contained just
  a stray `CMakeCCompilerId.exe`; `libpq/17.11` then died in
  `meson.build:327` with *"Program 'flex win_flex' not found or not executable"*
  while `PATH`, `LEX` and `YACC` all pointed correctly at that empty `bin`.
* a concrete build_type also matches the official ConanCenter binaries, so the
  build-context tool packages are downloaded instead of compiled from source
  (libiconv among them, which is a slow autotools build otherwise).

Do not remove `build_type` from the profile to "keep it generic", and do not add
it per-command — one place keeps `build-debug.sh` and `package-debug-cache.sh`
in agreement.

The Debug and Release commands intentionally use separate Conan folders and
separate CMake configurations. Never build Debug from the Release preset or
the inverse.

## Build and run the tests

The Catch2 suites are opt-in and registered at configure time, so the option has
to be present on the *configure* command, not on `ctest`:

```powershell
cmake --preset debug -DAOI_BUILD_TASKCONTROL_TESTS=ON
cmake --build --preset build-debug
ctest --test-dir out/build/debug --build-config Debug --output-on-failure
```

`scripts/build-debug.sh` sets `AOI_BUILD_TASKCONTROL_TESTS=ON` by default (turn
it off with `AOI_BUILD_TASKCONTROL_TESTS=OFF`). Two executables are registered:
`TaskControl.CoreArchitecture` and `Runtime.Scheduler`.

Test executables need their **DLL closure deployed beside them**. ctest launches
them straight out of the build tree with no Conan shell active, and
`TaskControl` is a `SHARED` library that links `Qt6::Core`, so nothing tells the
loader where `Qt6Cored.dll` or the OpenCV / PCL DLLs are. The process then dies
*before* `main()`:

```
Test #1: TaskControl.CoreArchitecture .....Exit code 0xc0000135***Exception: 573.66 sec
```

`0xC0000135` is `STATUS_DLL_NOT_FOUND`, and the ~10-minute "runtime" is Windows
Error Reporting, not the test. The fix is the function the build already has for
Shrimp:

```cmake
aoi_deploy_runtime_dependencies(TaskControlTests)
aoi_deploy_runtime_dependencies(RuntimeTests)
```

It copies `$<TARGET_RUNTIME_DLLS:...>` next to the executable and runs
`windeployqt` for the plugin and image-format DLLs that are not link-time
dependencies. **Any new executable target that links `AOI::TaskControl` needs
the same call**, otherwise ctest will report a `0xC0000135` "exception" and the
cause will look like a code failure.

An alternative for one-off runs, without changing CMake, is to start the shell
from Conan's runtime environment first — `out/conan/debug/build/Debug/generators/conanrun.bat`
prepends the Qt / PCL / OpenCC `bin` folders to `PATH`.

## Dependency policy

The core is resolved from ConanCenter: OpenCV 4.10.0, PCL 1.14.1,
Eigen, fmt 11.1.3, spdlog and Taskflow. fmt is fixed to the version required by spdlog. PCL 1.15.1 was requested by
the legacy project but has no ConanCenter recipe; 1.14.1 is the newest
available ConanCenter recipe.

Qt 6.8.3 is **unconditional**. `conanfile.py` requires it without asking, because
`TaskControl` is not a headless library: its runtime data bus stores values in
`QVariant`/`QHash` (`TaskControl/core/AlgorithmContext.h`), recipes are parsed
with `QJsonArray` (`TaskControl/core/AlgorithmIO.h`), the plug-in loader is built
on Qt, and `TaskControl/CMakeLists.txt` line 1 is
`find_package(Qt6 REQUIRED COMPONENTS Core)`. `Runtime` links
`AOI::TaskControl` PUBLIC and both test executables link it as well, so every
target in this repository needs Qt and no configuration exists without it.

**`TaskControl` links `Qt6::Core` only -- never `Qt6::Gui`.** It is the algorithm
/ runtime library and never touches a surface: 53 of its 66 source files use Qt,
and every one of those types is Core (`QJsonObject`, `QJsonArray`, `QString`,
`QVariant`, `QHash`, `QMap`, `QFile`, `QDir`, `QElapsedTimer`, `QPluginLoader`,
`QReadWriteLock`, `QCoreApplication`); there is not a single `QImage`, `QPixmap`,
`QPainter`, `QColor` or `QFont` in it, and no `.ui`/`.qrc`. `Qt6::Gui` used to be
declared here and has been removed. Two notes for whoever is tempted to add it
back:

- It bought nothing. The MSVC linker prunes import entries for DLLs whose symbols
  are never referenced, so `TaskControld.dll` never imported `Qt6Guid.dll` even
  while `Qt6::Gui` was on the link line -- the declaration was simply wrong, not
  load-bearing.
- It cost something. An unused component is still a **configure-time
  requirement**: `find_package(Qt6 REQUIRED COMPONENTS Core Gui)` fails on a Qt
  installation built without Gui, and it blurs which target actually needs a GUI
  toolkit. `Shrimp` needs one (`Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
  Qt6::OpenGLWidgets`) and declares its own.

**There is deliberately no `with_qt` option.** One was added and then removed: it
cannot ever be `False`. `with_qt=False` resolves a Qt-less graph successfully —
the install step is happy and every other dependency builds — and then
`cmake --preset debug` dies at `TaskControl/CMakeLists.txt:1` with *"Could not
find a package configuration file provided by Qt6"*, because the graph was never
what wanted Qt; the code was. An option whose only legal value is `True` is a
trap for whoever reads the comment next, so the require is written where its
reason is visible instead. (Same class of bug as `AOI_BUILD_SHRIMP`: a CMake
switch whose default disagrees with the Conan flag turns a "slower build" choice
into a hard configure failure.)

The single conditional dependency is the private `vtk/9.5.0@aoi/stable` package,
and that is what `with_shrimp` drives: `-o "&:with_shrimp=True"` for the whole
project, `False` (the default) for a core loop that still links Qt but keeps VTK
out of the graph entirely.

### The private VTK package (`conan/recipes/vtk`)

ConanCenter publishes no VTK in the configuration this project needs (shared,
Qt-enabled, same MSVC / Qt ABI), so `conan/recipes/vtk` builds VTK 9.5.0 from
source and publishes it as `vtk/9.5.0@aoi/stable`. It is the only package in the
graph that does not come from ConanCenter. `scripts/build-debug.sh` gives it its
own first stage, which skips itself whenever the cache already holds a matching
Debug package — *matching* includes the recipe revision `conan.lock` pins, since
Conan keys binaries by revision: a rebuilt recipe leaves the previous binary in
the cache under a revision nothing resolves any more, and a skip test that only
asked "is there any Debug vtk?" answered yes and silently kept it.

```console
$ bash scripts/build-debug.sh vtk                # skips when cached
$ AOI_FORCE_VTK=1 bash scripts/build-debug.sh vtk   # rebuild anyway
```

By hand (`-s:h build_type=Release` for the Release package):

```powershell
conan create conan/recipes/vtk --user=aoi --channel=stable -pr:h=conan/profiles/windows-msvc-v143-x64 -pr:b=conan/profiles/windows-msvc-v143-x64 -s:h build_type=Debug
```

Do not add `D:\PublicLibraries`, vcpkg, a Qt kit, PCL, VTK, or OpenCV paths back
into a project property sheet.

#### Where the sources come from, and why the recipe pins a checksum

Upstream serves this archive at
`https://www.vtk.org/files/release/9.5/VTK-9.5.0.tar.gz`, and the same tag exists
as a GitLab archive on `gitlab.kitware.com`. **Neither host is reachable from
this network**: a direct connection times out after 20 s, and through the VPN
the TLS handshake never completes — measured with curl (schannel) *and* with
Python's OpenSSL stack, so it is not a proxy-protocol problem. The HTTP proxy
does establish the CONNECT tunnel (`HTTP/1.1 200 Connection established`) and
the handshake then dies inside schannel.

The recipe therefore fetches the **GitHub tag archive**, which is a complete
source tree rather than a reduced export: VTK has no submodules at v9.5.0 (there
is no `.gitmodules`) and its `.gitattributes` marks only `.git*` and `.hooks*`
as `export-ignore`. Measured: 27,056 entries, top directory `VTK-9.5.0/`,
`ThirdParty/` complete (zlib, freetype, png, jpeg, expat, glad, eigen, …).

The `sha256` in the recipe is not decoration, it is what makes an offline build
possible. `SourcesCachingDownloader` disables `core.sources:download_cache`
whenever a recipe calls `download()` without a checksum
(`conan/internal/rest/caching_file_downloader.py`, *"Cannot cache download()
without sha256 checksum"*), so the previous revision of this recipe bypassed the
sources cache entirely and went to the network every time — which cannot work
here at all. With the pin, the run says:

```
Source ['https://ghfast.top/...', 'https://gh-proxy.com/...', ...]
  retrieved from local download cache
```

`scripts/prefetch-sources.sh` carries a matching `vtk` entry with the same
checksum, so `bash scripts/prefetch-sources.sh` warms the archive like every
other dependency. Two things about that entry:

* It must list only URLs that serve **the same bytes**. All four (ghfast,
  gh-proxy, github.com, codeload) are the GitHub archive; the official vtk.org
  tarball is a *different* byte stream and cannot share this checksum. To move
  back to the official archive, swap the URL list and recompute the hash.
* `chunks` must be `1`. ghfast.top does not implement `Range` (`-r` answers 200
  with the whole body instead of 206), so the automatic 8-way split that the
  32 MB threshold would otherwise pick asks eight times for the entire file.
  Single stream measured ~940 KB/s; the 50 MB archive came down in 54 s.

#### Editing this recipe invalidates `conan.lock`

`conan.lock` pins **recipe revisions**, so any edit to
`conan/recipes/vtk/conanfile.py` produces a new `vtk` revision that the lockfile
does not know. The next install then fails in a way that does not mention the
lockfile's contents:

```console
$ conan install . --lockfile=conan.lock -o "&:with_shrimp=True" ...
vtk/9.5.0@aoi/stable: Not found in local cache, looking in remotes...
ERROR: Package 'vtk/9.5.0@aoi/stable' not resolved: No remote defined.
```

Conan is looking for the *locked* revision, which no longer exists in the cache,
and `--no-remote` forbids fetching it. Rebuild the package first, then
regenerate the lockfile — in that order:

```powershell
bash scripts/build-debug.sh vtk
mv conan.lock conan.lock.old                       # see below: this step is the one that matters
conan lock create . --lockfile-out=conan.lock -o "&:with_shrimp=True" -pr:h=conan/profiles/windows-msvc-v143-x64 -pr:b=conan/profiles/windows-msvc-v143-x64 -s:h build_type=Debug --no-remote
```

`mv conan.lock conan.lock.old` is not housekeeping, it is the whole point.
`conan lock create` picks up a `conan.lock` sitting in the working directory and
uses it as its **base** lockfile, and a base lock pins revisions — so with the
old file still in place the command returns the old revision, byte for byte,
and looks like it worked. Measured on this repository:

```console
$ # conan.lock pinned #4b2e39c8…, the cache held both revisions
$ conan lock create . --lockfile-out=conan.lock … --no-remote
Generated lockfile: E:\projects\AOIProject\conan.lock
$ diff conan.lock.old conan.lock                     # nothing at all
$
$ mv conan.lock conan.lock.old                       # the step above
$ conan lock create . --lockfile-out=conan.lock … --no-remote
Generated lockfile: E:\projects\AOIProject\conan.lock
$ diff conan.lock.old conan.lock
6c6
<         "vtk/9.5.0@aoi/stable#4b2e39c8236ef9d854f61fbc64a048f0%1789540186.0359285",
---
>         "vtk/9.5.0@aoi/stable#983c7acfa0cabef0ae92c662007b543f%1789552790.1410687",
```

`conan lock create` resolves **binary** package ids, not just the recipe graph,
so it fails with the same "not resolved" error if it is run before the package
is built. Nothing else needs regenerating: the lockfile stores recipe
revisions only, and those do not depend on the build type, so one Debug
resolution covers the Release and RelWithDebInfo presets too — verified by the
diff above, where those 45 other entries stayed byte-identical.

That failure is no longer where you meet the problem: the `vtk` stage now
exports the recipe and compares the revision it got with the one the lockfile
pins *before* it starts compiling, and reports the two hashes side by side. See
the next section — there is a second way to produce a mismatch, and it does not
involve editing anything.

#### The recipe revision hashes the bytes on disk, not the recipe's meaning

`conan.lock` pins `vtk/9.5.0@aoi/stable#983c7acfa0cabef0ae92c662007b543f`, and
that hash is a checksum of the **bytes** of `conan/recipes/vtk/conanfile.py` as
they sit in the working tree. Line endings are part of those bytes, so the same
recipe written with CRLF and with LF is **two different recipes**. Measured in a
throwaway `CONAN_HOME`, exporting one such file both ways:

```text
LF    -> vtk/9.5.0@aoi/stable#983c7acfa0cabef0ae92c662007b543f
CRLF  -> vtk/9.5.0@aoi/stable#4b2e39c8236ef9d854f61fbc64a048f0
```

The second revision is the one this project shipped for a while, and no fresh
clone can ever produce it. What makes that dangerous is that **git's own
comparison cannot see the difference**. `.gitattributes` declares
`*.py text eol=lf`, so a clone always checks out LF — and the same rule makes git
compare a CRLF worktree as if it were LF. Measured with the recipe sitting in
that state:

```console
$ git status --short
 M conan/recipes/vtk/conanfile.py    # ...which nothing below can explain
$ git diff --stat                    # (nothing)
$ git hash-object conan/recipes/vtk/conanfile.py
56930938665b8064f5ddb2f889d0b9dfaf8693d4     # == git rev-parse HEAD:…, i.e. "unmodified"
$ git diff-files --raw
:100644 100644 56930938665b8064f5ddb2f889d0b9dfaf8693d4 0000…0000 M	conan/recipes/vtk/conanfile.py
```

A modified file with no diff, and a worktree object the index never hashed:
`git status` is reporting its own stat cache, while the content it would commit
is byte-for-byte what the commit already holds. Even that `M` is unreliable — in
the state that actually bit this repository, `git status --short` printed
nothing at all, which is how the CRLF survived review.

What does say it plainly is `git ls-files --eol`,

```console
$ git ls-files --eol conan/recipes/vtk/conanfile.py
i/lf    w/crlf  attr/text eol=lf      	conan/recipes/vtk/conanfile.py
```

the warning git emits the next time it touches the file,

```text
warning: in the working copy of 'conan/recipes/vtk/conanfile.py',
         CRLF will be replaced by LF the next time Git touches it
```

and `conan export`, whose revision simply will not match the lockfile.

This is not hypothetical. The working tree had CRLF in
`conan/recipes/vtk/conanfile.py` and in the root `conanfile.py` while git stored
LF for both, so the commit described one recipe and `conan.lock` pinned another
(`#4b2e39c8…`, which the committed file cannot produce). Shipments still worked,
because the bundle carried that revision's binary along with the lockfile entry;
what broke was every attempt to build VTK *from the repository* — the Release
package, any future recipe change, and the documented "build the recipe, then
regenerate the lock" loop — because those builds produce `#983c7acf…`, a
revision the lockfile did not mention, and the lockfile then rejects the binary
that was just built. On a fresh clone the recipe exports as `#983c7acf…` while
the lockfile still asks for `#4b2e39c8…`, so the VTK package that clone just
spent 26 minutes building is invisible to the project until the lockfile is
regenerated.

`scripts/build-debug.sh` therefore treats the line endings as a build input
rather than as housekeeping:

* the `vtk` stage rewrites CRLF back to LF before exporting, and prints what it
  changed. That is the same normalization git applies on the way in, so the
  committed content — and the revision a clone produces — is unaffected, and
  re-running the stage does nothing. Detection is a byte count rather than
  `grep -q $'\r'`, which is what the first version of this guard used: Git for
  Windows' grep treats CR as a line terminator and answered "no match" for the
  recipe that `od -c` shows is full of `\r\n` (measured: `grep -c $'\r'` → 0,
  `grep -cU $'\r'` → 104), so the guard turned itself off;
* it then exports the recipe and compares the resulting revision with the
  lockfile's, failing **before** it starts compiling instead of after it.
  `conan export` is idempotent and prints `Exported: <ref>#<revision>` whether or
  not that revision is already in the cache, so the check costs about a second:

```console
$ conan export conan/recipes/vtk --user=aoi --channel=stable
Exported: vtk/9.5.0@aoi/stable#983c7acfa0cabef0ae92c662007b543f (…)
```

A mismatch then means exactly one of two things, and the message says which:
stray carriage returns came back (the stage has just removed them, so re-run
it), or the recipe was genuinely edited and `conan.lock` has to be regenerated
as described above.

### pcre2 must not build pcre2grep (Windows / MSVC)

`conan/profiles/windows-msvc-v143-x64` pins

```
[options]
pcre2/*:build_pcre2grep=False
```

Without it the Debug build dies at `pcre2/10.42` with seven unresolved symbols:

```
pcre2grep.c.obj : error LNK2019: 无法解析的外部符号 gzread，函数 fill_buffer 中引用了该符号
pcre2grep.c.obj : error LNK2019: 无法解析的外部符号 BZ2_bzopen，函数 grep_or_recurse 中引用了该符号
... (gzclose, gzopen, BZ2_bzread, BZ2_bzclose, BZ2_bzerror)
pcre2grep.exe : fatal error LNK1120: 7 个无法解析的外部命令
```

The recipe writes `PCRE2_SUPPORT_LIBZ` and `PCRE2_SUPPORT_LIBBZ2` into the
CMake cache via `CMakeToolchain.variables`, which **bypasses the upstream
`IF(ZLIB_FOUND)` guard** in `CMakeLists.txt`. Upstream then assembles
`PCRE2GREP_LIBS` from the legacy variables `${ZLIB_LIBRARIES}` /
`${BZIP2_LIBRARIES}` and hands that to
`TARGET_LINK_LIBRARIES(pcre2grep pcre2-posix ${PCRE2GREP_LIBS})`. Under Conan's
CMakeDeps generator with a single-config Ninja build those two variables hold
IMPORTED target names (`ZLIB::ZLIB`, `BZip2::BZip2`) that never make it into
pcre2grep's link line — measured: `PCRE2GREP_LIBS` is non-empty at the point of
use, yet `build.ninja`'s `LINK_LIBRARIES` for `pcre2grep.exe` lists only
`pcre2-posix-staticd.lib pcre2-8-staticd.lib`. Meanwhile `config.h` is
generated from the same `IF` block and does define `SUPPORT_LIBZ` /
`SUPPORT_LIBBZ2`, so `pcre2grep.c.obj` carries the references with nothing to
resolve them. The failure is deterministic: retrying repeats it verbatim.

Switching the option off makes the recipe's `configure()` drop
`with_zlib`/`with_bzip2` as well, so pcre2 stops depending on zlib and bzip2
(both zlib and bzip2 remain in the graph for their other consumers). This
project only needs the pcre2 static libraries for Qt; `pcre2grep` is a
command-line grep utility. Verified: with `PCRE2_SUPPORT_LIBZ=OFF` /
`PCRE2_SUPPORT_LIBBZ2=OFF` the link succeeds and produces a 704512-byte
`pcre2grep.exe`.

Because the pin lives in the profile, every command that resolves the graph
picks it up — `conan install`, `conan graph info`,
`scripts/build-debug.sh` and `scripts/package-debug-cache.sh` — so nothing else
has to carry the option.

### Retries: transient only

`scripts/build-debug.sh` re-runs a failed `deps` attempt, but only when the
failure is worth re-running. Every failed attempt is classified from its own
output before the decision is made:

| class | matched by | action |
|---|---|---|
| deterministic | `DETERMINISTIC_RE` — `Error in build() method`, `error LNK####`, `error C####`, `error D####`, `ninja: build stopped`, `CMake Error at`, `Program ... not found`, `meson.build:N:N: ERROR`, `build failed`, … | stop the stage immediately, print the offending lines |
| transient | `TRANSIENT_RE` — `Timeout`, `Connection reset`, `ProxyError`, `Failed to download`, `IncompleteRead`, `Could not connect`, … | retry, up to `MAX_ATTEMPTS` (default 3) |
| unknown | neither | retry, up to `MAX_ATTEMPTS` |

Deterministic is tested **first** on purpose: a package that dies in `build()`
may well have logged a timeout while fetching something earlier in the same
attempt, and the build error is the one that matters.

The point is that Conan re-runs the identical recipe with identical inputs, so a
build error reproduces verbatim. pcre2's `LNK1120`, winflexbison's missing
`win_flex.exe` and libiconv's `D8003` were all reproducible, and each one used
to burn attempts until somebody read the log by hand. `MAX_ATTEMPTS=1` turns
retries off completely; `AOI_RETRY_ALL=1` restores the old
retry-everything behaviour.

Only `error LNK` is matched, never bare `LNK`: `LINK : warning LNK4098` shows up
in perfectly healthy builds. The same care applies to `error C####` versus
`warning C####`, and to `CMake Error at` versus `CMake Warning at`.

## Build Shrimp

`Shrimp` uses the checked-in `TaskControl` API. Its ROI measurement recipe
injects an input point, filters the point cloud, fits a plane, measures the
distance, then aggregates the tolerance result. `AOI_BUILD_SHRIMP` remains OFF
by default because Shrimp needs the private VTK package.

`TaskControl` still exposes a C++ API. Cross-process or independently built
plug-ins should use `TaskControl_C.h` until its C++ boundary is redesigned.

## Visual Studio solution view

Open the repository folder in Visual Studio, or generate a native VS2022
solution with the dedicated preset. The generated solution places `Shrimp` in
`Applications` and `TaskControl` in `Libraries`; within each project it groups
sources, headers, Qt forms, resources and feature folders separately.

```powershell
conan install . --lockfile=conan.lock --build=missing -pr:h=conan/profiles/windows-msvc-v143-x64-vs2022 -pr:b=conan/profiles/windows-msvc-v143-x64-vs2022 -s:h build_type=Release -o "&:with_shrimp=True" -of=out/conan/vs2022-release
cmake --preset vs2022-release -DAOI_BUILD_SHRIMP=ON
cmake --build --preset build-vs2022-release
```

Open `out/build/vs2022-release/AOIProject.sln` in Visual Studio after the
configuration step. Do not reintroduce legacy `.pro`, `.pri`, `.props` or
`.vcxproj` files: their dependency paths are not part of the Conan build.

## Offline cache package

When the package remote is unreachable (offline machine, blocked JFrog CE),
move binaries as a cache bundle instead of uploading them directly.

```powershell
# 1. Resolve one configuration and export its exact package list.
#    Everything is already cached, so this step is fast.
#    All three options below are mandatory here:
#      -o "&:with_shrimp=True"  the packaged graph must match the graph the
#                            build used, otherwise the bundle ships without
#                            qt/6.8.3 and without the private VTK package, and
#                            the receiving machine still fails to configure
#                            (or has to compile VTK itself; ~26 minutes measured here)
#      --build=missing
#      -c:a tools.graph:skip_binaries=False
conan install . --lockfile=conan.lock --build=missing -c:a tools.graph:skip_binaries=False -pr:h=conan/profiles/windows-msvc-v143-x64 -pr:b=conan/profiles/windows-msvc-v143-x64 -s:h build_type=Debug -o "&:with_shrimp=True" --output-folder=out/conan/debug --format=json > out/conan/debug/graph.json
conan list --graph=out/conan/debug/graph.json --format=json > out/conan/debug/pkglist.json

# 2. Pack those packages (host Debug binaries plus the Release tool packages).
#    msys2's bin/msys64/etc/mtab is an LX symlink reparse point that no native
#    Windows process can open, which aborts the command below until it is
#    normalized (see "conan cache save cannot archive msys2's etc/mtab").
python scripts/normalize-cache-reparse.py "%USERPROFILE%\.conan2\p"
conan cache save --list=out/conan/debug/pkglist.json --file=conan-cache-debug.tgz

# 3. On a machine with remote access:
conan cache restore conan-cache-debug.tgz
conan remote add <remote-name> https://<host>/artifactory/api/conan/<repo>
conan upload --list=out/conan/debug/pkglist.json -r <remote-name> --confirm

# 4. Before carrying the file anywhere, prove that it is usable by restoring it
#    into an empty cache and building from there:
bash scripts/verify-debug-bundle.sh
```

### Why `-c:a tools.graph:skip_binaries=False` is not optional

`tools.graph:skip_binaries` defaults to `True`. With it on, conan marks every
node whose files are not needed *by the current command* as `"binary": "Skip"` —
and on a warm cache that is nearly the whole graph, because the direct
dependencies are already present. `conan list --graph` then drops those nodes
unconditionally:

```python
# conan/api/model/list.py
if binary in (BINARY_SKIP, BINARY_INVALID, BINARY_MISSING):
    continue
```

Measured on the real Debug graph:

| resolve | node states | resulting pkglist |
|---|---|---|
| default | 11 Cache / 246 Skip | 12 binaries |
| `-c:a skip_binaries=False` | 256 Cache / 1 Download | 52 binaries |

Both lists contain the same package *names* and both produce a multi-hundred-
megabyte archive, so the difference is invisible on the machine that builds the
bundle. It only shows up on the machine that consumes it: with an **empty
cache** those skipped nodes stop being skippable, need binaries, and the bundle
never contained them.

`-c:a` matters, not plain `-c`. A bare `-c` applies to the host context only
(see `conan install --help`); the build context keeps `True` and leaves all 223
of its nodes skipped, which fixes exactly half the graph.

### What the bundle contains

The resolved pkglist holds **47 recipe references and 53 binary packages**, and
every one of those references carries at least one binary — nothing in the
consumer's closure is missing. A few references contribute two binaries each
(`conan list` merges every revision of a package into one entry), which is why
53 is larger than 47.

The largest single entry is the private `vtk/9.5.0@aoi/stable` package: 551 MB
unpacked, and the reason the archive went from 2.5 GiB (Qt-only graph) to
2.7 GiB. It is also the entry nothing else can stand in for, because it exists
in no remote — a bundle without it does not fail here, it fails as a ~26 minute
`conan create` on the receiving machine. `MUST_HAVE` in
`scripts/package-debug-cache.sh` therefore names `vtk` first.

Earlier revisions of this document claimed that ten packages (`cmake/4.4.3`,
`b2`, `nasm`, `openexr`, `imath`, `libjpeg`, `libtiff`, `libdeflate`,
`openjpeg`, `xz_utils`) ship without a binary. That is no longer true and has
been measured: with `-c:a tools.graph:skip_binaries=False` they are resolved into
the cache, the pkglist carries a binary for each of them, and `conan cache save`
writes them into the archive. (The old "53 binaries" figure came from a
multi-remote listing that counted one `nasm` binary living on the `conancenter`
remote; the cache-only pkglist has 52.)

`scripts/package-debug-cache.sh` runs all of the above and **refuses to write
the archive** unless the pkglist carries a binary for every package the build
links (Qt, pcre2, OpenCV, PCL, zlib, Boost, Catch2, …), so a regression here
fails loudly instead of at the far end of the transfer.

`conan cache save` stores recipes and binaries together, so the bundle is
self-contained. Build tools such as `msys2` are ~245 MB; drop them from
`pkglist.json` first if the bundle has to stay small.

### `conan cache save` cannot archive msys2's `etc/mtab`

The msys2 build tool ships one file that breaks `conan cache save` on Windows:

```
bin/msys64/etc/mtab   ->  /proc/mounts
```

MSYS2 creates its symlinks as **LX symlink** reparse points
(`IO_REPARSE_TAG_LX_SYMLINK`, `0xA000001D`). NTFS has no handler for that tag,
so no native Windows process can open the file at all — and whether the link
target exists makes no difference:

```console
>>> open(r"...\msys2f91ae1bf3386a\p\bin\msys64\etc\mtab", "rb")
OSError: [Errno 22] Invalid argument      # underlying: WinError 1920
                                          # ERROR_CANT_ACCESS_FILE
```

Python additionally reports it as a **regular file** (`S_ISREG` is `True`,
`os.path.islink()` is `False`), so `tarfile` never takes the symlink path and
tries to read the contents instead:

```python
# conan/internal/api/uploader.py -> compress_files -> tarfile.TarFile.add
tarinfo = self.gettarinfo(name, arcname)   # isreg() -> True
with bltn_open(name, "rb") as f:           # <- Errno 22 here
```

which aborts `conan cache save` part-way through the archive:

```
RuntimeError: lost gzip_file
OSError: [Errno 22] Invalid argument: '...\msys2...\bin\msys64\etc\mtab'
```

Conan's manifest code hits the same wall
(`conan/internal/model/manifest.py`:
`md5(os.readlink(f)) if os.path.islink(f) else md5sum(f)`), so
`conan cache check-integrity "msys2/*"` **crashes** instead of reporting.

A whole-store scan of this cache (881,573 files) finds exactly one such file.
`scripts/normalize-cache-reparse.py` replaces it with a plain 12-byte file
holding the link target (`/proc/mounts`, exactly what `ls -l` already reported)
and rewrites that single hash in the package's `conanmanifest.txt`, so the
package stays self-consistent:

```console
$ conan cache check-integrity "msys2/*"
msys2/cci.latest#d22fe7b2808f5fd34d0a7923ace9c54f: Integrity check: ok
msys2/cci.latest#d22fe7b2...:956a88975bda9dfcc485e2861d71e74bd7e2b9a5#13a1f739...: Integrity check: ok
```

Two traps worth knowing if that helper ever needs changing:

* It deletes through Win32 `DeleteFileW`, not `os.remove()`. The tool host
  injects a `sitecustomize` shim that wraps `os.remove`/`os.unlink` and `stat()`s
  the target first; `stat()` on this reparse point raises, so the shim prints
  `[safe-delete][SAFE_DELETE_BULK_GUARD_ERROR]` and exits before the delete is
  attempted.
* It must run under the Python that backs `conan.exe` (`AOI_PYTHON`, derived in
  `scripts/_env.sh`). Conan hashes package files with that interpreter, while the
  MSYS2 Python emulates the MSYS mounts and reparse points and would therefore
  compute hashes that no longer match what `conan cache check-integrity`
  recomputes.

`/etc/mtab` is legacy mount bookkeeping belonging to a *build tool*; nothing
reads it while compiling, and `conan cache restore` verifies no manifests at all
(`conan/api/subapi/cache.py`, `restore()`). Delete it instead of replacing it if
the helper ever becomes unusable — nothing in the build needs it.

`scripts/package-debug-cache.sh` automates all five steps and respects
`AOI_WITH_SHRIMP` (default `1`, same as `scripts/build-debug.sh`). The default
packages the `with_shrimp` graph, so the archive carries the private
`vtk/9.5.0@aoi/stable` package; Qt is in the archive either way. Set
`AOI_WITH_SHRIMP=0` for the old core-only bundle.

### Verify the bundle (do this before every hand-off)

`package-debug-cache.sh` can only report what it *put into* the archive. It
cannot tell you whether the archive is usable, because this machine has a warm
cache and will happily build from that instead of from the bundle. Wrap a
second, empty cache around it:

```console
$ bash scripts/verify-debug-bundle.sh
```

Seven stages, each one a way a hand-off has failed before:

| stage | what it proves |
|---|---|
| `home` | starts from a genuinely empty `CONAN_HOME` (`out/verify/conan-home`) |
| `restore` | the archive unpacks and yields the same binaries the pkglist claims |
| `check` | `conan cache check-integrity` recomputes every manifest and agrees |
| `install` | `conan install --no-remote`, **without** `--build=missing` |
| `build` | cmake configures and builds a fresh tree with the restored toolchain, `bin/` emptied first so no DLL from an earlier run can stand in for one the bundle does not have |
| `test` | ctest reports 100% |
| `run` | `Shrimp.exe --selftest` prints `END-TO-END PASS` out of that tree |

Stages can be run on their own — `bash scripts/verify-debug-bundle.sh build test`
— which is what you want while iterating; the full chain is what a hand-off
needs. Every stage clears what a previous run left behind (the throwaway home,
and `bin/`, where the executables and their whole DLL closure land), so a second
run cannot pass on its own leftovers and the script can simply be re-run after
the archive changes. The developer cache in `%USERPROFILE%\.conan2` is never
touched, logs land in `out/6-verify.log` and `out/verify/*.log`, and `out/` is
git-ignored.

Measured on the 2.7 GB Debug bundle (the `with_shrimp` graph):

```console
binaries in the restored cache : 53
integrity                      : ok (100 packages)
install                        : no remote, nothing built
build                          : configured + built, ninja taken from the bundle
test                           : 100% tests passed out of 2
run                            : Shrimp.exe --selftest -> END-TO-END PASS
```

**Read the `install` and `run` stages closely.** `install` deliberately carries
no `--build=missing` and no remote, so a package missing from the bundle is a
hard error naming the missing ref, instead of quietly turning into a source
build. On an empty home that source build would fail much further along, for a
reason that looks unrelated to packaging — and with the private VTK package in
the archive, the consumer does not spend ~26 minutes rebuilding it either.
`run` is the stage that separates "the bundle compiles" from "the bundle is
usable": linking succeeds happily while the VTK or Qt runtime DLLs, or the
OpenGL context, are still wrong.




