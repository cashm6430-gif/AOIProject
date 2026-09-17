#!/usr/bin/env bash
# =============================================================================
# prefetch-sources.sh — 把 Conan 各依赖的源码包预先抓到本地 sources 缓存
# =============================================================================
# 为什么单独干这件事：
#   这台机器直连境外源只有 20~25 KB/s，而 Wi-Fi 本身带宽充足。实测瓶颈完全
#   在"走哪条通道"，不在带宽：
#
#     ghfast.top          + socks5h://127.0.0.1:7892   ~2600 KB/s   GitHub 类包
#     archives.boost.io   + http://127.0.0.1:7892      400~1200 KB/s  boost
#     sourceforge.net     直连                          ~174 KB/s
#     任意源              直连                           20~25 KB/s
#
#   7892 是用户的 VPN(pineappleCore)，它说的是 SOCKS5 不是 HTTP：用
#   `-x http://127.0.0.1:7892` 访问多数站点会立刻 rc=35 (SSL connect error)。
#   socks5h 的 h 表示 DNS 也交给代理解析，少了它同样 rc=35。
#
#   两条通道都会**间歇性** rc=35（大约 3 次里失败 1 次），所以本脚本把
#   "重试"当成正常路径而不是异常兜底。
#
#   单连接还被限速得很死。实测 6 路并发能把 boost 从 28 KB/s 拉到 1.7 MB/s，
#   所以大包一律分块并行。
#
# 抓下来的文件按 sha256 命名丢进 Conan 的 sources 缓存
# (core.sources:download_cache)，之后 `conan install --no-remote` 全程零网络。
#
# 用法:  bash scripts/prefetch-sources.sh
#        MAX_TRIES=40 CHUNK_COUNT=8 bash scripts/prefetch-sources.sh
# =============================================================================
set -u

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CACHE="${AOI_SOURCES_CACHE:-$ROOT/out/conan-sources-cache}"
DEST="$CACHE/s"
TMP="$CACHE/tmp"
mkdir -p "$DEST" "$TMP"

# curl 是原生 Windows 程序，不认 /e/projects/... 这种 MSYS 路径
WINP() { cygpath -m "$1" 2>/dev/null || printf '%s' "$1"; }

PROXY_SOCKS="${AOI_PROXY_SOCKS:-socks5h://127.0.0.1:7892}"
PROXY_HTTP="${AOI_PROXY_HTTP:-http://127.0.0.1:7892}"
MAX_TRIES="${MAX_TRIES:-40}"
CHUNK_THRESHOLD="${CHUNK_THRESHOLD:-33554432}"    # 超过 32MB 才考虑分块
CHUNK_COUNT="${CHUNK_COUNT:-8}"

# name|sha256|route|chunks|size|url1|url2...
#   route : socks5 = 走 SOCKS5 代理（GitHub 类，配 ghfast 前缀）
#           http   = 走 HTTP 代理（boost.io）
#           direct = 直连（sourceforge / osgeo 这类国内可达的）
#   chunks: 分块数；0 = 自动（按 size 是否超过阈值决定）
#   size  : 已知字节数；0 = 运行时探测。已知的一律写死 —— 这条链路对
#           HEAD/Content-Range 的请求有很高概率 rc=35，与其重试不如直接填。
#
# 每个 URL 可以单独覆盖路由，写法是 "<route>@<url>"。这是必须的：同一批
# 备选 URL 常常分布在不同 host 上，而"哪个 host 走哪条路"差别巨大
# （见文件头的实测表），共用一条路由会让备选 URL 形同虚设。
#
# 2026-09-16 复测（VPN 规则模式变化后，结论与文件头不同！）：
#   * GitHub 各路径走 SOCKS5 现在**基本全挂**（schannel handshake 失败），
#     但走**直连**加上 gh-proxy / ghfast 前缀反而稳定 200KB/s+：
#       gh-proxy.com   + direct  -> 200KB/s+ （releases/download 也支持）
#       ghfast.top     + direct  -> 200KB/s+ （releases/download 也支持）
#       codeload.github.com direct -> 62KB/s，3/3 成功，archive/tags 的兜底
#     github.com 本体直连只会返回 302，然后卡在 objects.githubusercontent.com。
#   * Qt 源码（950MB）走**国内镜像直连**最快（腾讯云 / 上交 / sau，均 400KB/s+），
#     download.qt.io 本体反而不稳。
#   * 其余 GNU/gnome/postgresql/sqlite/nasm 等直连即可；ftp.gnu.org 系走
#     HTTP 代理更快（直连只有 1.8KB/s）。
#
# vtk 这一条的背景（它和上面所有条目的取数通道都不同）：
#   * 官方 https://www.vtk.org/files/release/9.5/VTK-9.5.0.tar.gz 与
#     gitlab.kitware.com 的同 tag 归档在本机都不可达 —— 直连超时，走 VPN
#     连 TLS 握手都完不成（schannel 与 OpenSSL 一样）。
#   * 只有 GitHub tag 归档这一条路。它是**完整源码树**：v9.5.0 没有
#     .gitmodules（无子模块），.gitattributes 只把 .git*/.hooks* 标记为
#     export-ignore，实测 27056 条目、顶层目录 VTK-9.5.0/。
#   * chunks 必须写 1：ghfast 不实现 Range（-r 返回 200 而不是 206），而
#     size 超过 32MB 阈值，自动分块会并发去要 8 个块、每块都拿到整份文件。
#     单流实测 ~940KB/s，50MB 约 54 秒。
#   * 与 conan/recipes/vtk/conanfile.py 里的 sha256 必须一致；那边靠这个
#     校验和才能用上 core.sources:download_cache（不传 sha256 时 Conan 直接
#     绕过缓存走网络）。
#
# 注意 SOURCES 是用 `for line in $SOURCES` 按空白切分来遍历的：**条目里不能
# 出现空格**，包括注释。要写说明就写在这里，不要写在 SOURCES 里面。
SOURCES="
boost|6478edfe2f3305127cffe8caf73ea0176c53769f4bf1585be237eb30798c3b8e|http|8|122892751|https://archives.boost.io/release/1.83.0/source/boost_1_83_0.tar.bz2|https://sourceforge.net/projects/boost/files/boost/1.83.0/boost_1_83_0.tar.bz2/download
opencv|b2171af5be6b26f7a06b1229948bbb2bdaa74fcf5cd097e0af6378fce50a6eb9|socks5|0|94993429|https://ghfast.top/https://github.com/opencv/opencv/archive/refs/tags/4.10.0.tar.gz|https://codeload.github.com/opencv/opencv/tar.gz/refs/tags/4.10.0
pcl|5dc5e09509644f703de9a3fb76d99ab2cc67ef53eaf5637db2c6c8b933b28af6|socks5|0|68672885|https://ghfast.top/https://github.com/PointCloudLibrary/pcl/archive/refs/tags/pcl-1.14.1.tar.gz|https://codeload.github.com/PointCloudLibrary/pcl/tar.gz/refs/tags/pcl-1.14.1
openexr|f3f6c4165694d5c09e478a791eae69847cadb1333a2948ca222aa09f145eba63|socks5|0|18855303|https://ghfast.top/https://github.com/AcademySoftwareFoundation/openexr/archive/refs/tags/v3.2.3.tar.gz|https://codeload.github.com/AcademySoftwareFoundation/openexr/tar.gz/refs/tags/v3.2.3
xz_utils|fff1ffcf2b0da84d308a14de513a1aa23d4e9aa3464d17e64b9714bfdd0bbfb6|direct|0|1548064|https://sourceforge.net/projects/lzmautils/files/xz-5.8.3.tar.xz/download|https://download.osgeo.org/lzmautils/xz-5.8.3.tar.xz
libpng|28eb403f51f0f7405249132cecfe82ea5c0ef97f1b32c5a65828814ae0d34775|direct|0|1070096|https://download.sourceforge.net/libpng/libpng-1.6.58.tar.xz|
libtiff|88b3979e6d5c7e32b50d7ec72fb15af724f6ab2cbf7e10880c360a77e4b5d99a|direct|0|3584534|https://download.osgeo.org/libtiff/tiff-4.6.0.tar.gz|
qt|cdd3a69967208276bb01af7ace7dba0ba53e679f886a4cbe624225c60fb73f2c|direct|8|994812276|direct@https://mirrors.cloud.tencent.com/qt/archive/qt/6.8/6.8.3/single/qt-everywhere-src-6.8.3.tar.xz|direct@https://mirrors.sjtug.sjtu.edu.cn/qt/archive/qt/6.8/6.8.3/single/qt-everywhere-src-6.8.3.tar.xz|direct@https://mirrors.sau.edu.cn/qt/archive/qt/6.8/6.8.3/single/qt-everywhere-src-6.8.3.tar.xz|direct@https://download.qt.io/archive/qt/6.8/6.8.3/single/qt-everywhere-src-6.8.3.tar.xz
openssl|9bffaa1ad1e07b354c21bd3324ec02fa15579f45a7d0494b3e74bc449b7333ef|direct|0|0|direct@https://gh-proxy.com/https://github.com/openssl/openssl/releases/download/openssl-3.6.4/openssl-3.6.4.tar.gz|direct@https://ghfast.top/https://github.com/openssl/openssl/releases/download/openssl-3.6.4/openssl-3.6.4.tar.gz|direct@https://ghproxy.net/https://github.com/openssl/openssl/releases/download/openssl-3.6.4/openssl-3.6.4.tar.gz
pcre2|8d36cd8cb6ea2a4c2bb358ff6411b0c788633a2a45dabbf1aeb4b701d1b5e840|direct|0|0|direct@https://gh-proxy.com/https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.42/pcre2-10.42.tar.bz2|direct@https://ghfast.top/https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.42/pcre2-10.42.tar.bz2|direct@https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.42/pcre2-10.42.tar.bz2
harfbuzz|8660ebd3c27d9407fc8433b5d172bafba5f0317cb0bb4339f28e5370c93d42b7|direct|0|0|direct@https://gh-proxy.com/https://github.com/harfbuzz/harfbuzz/releases/download/12.3.0/harfbuzz-12.3.0.tar.xz|direct@https://ghfast.top/https://github.com/harfbuzz/harfbuzz/releases/download/12.3.0/harfbuzz-12.3.0.tar.xz|direct@https://github.com/harfbuzz/harfbuzz/releases/download/12.3.0/harfbuzz-12.3.0.tar.xz
libffi|bc9842a18898bfacb0ed1252c4febcc7e78fa139fd27fdc7a3e30d9d9356119b|direct|0|0|direct@https://gh-proxy.com/https://github.com/libffi/libffi/releases/download/v3.4.8/libffi-3.4.8.tar.gz|direct@https://ghfast.top/https://github.com/libffi/libffi/releases/download/v3.4.8/libffi-3.4.8.tar.gz|direct@https://github.com/libffi/libffi/releases/download/v3.4.8/libffi-3.4.8.tar.gz
brotli|e720a6ca29428b803f4ad165371771f5398faba397edf6778837a18599ea13ff|direct|0|0|direct@https://gh-proxy.com/https://github.com/google/brotli/archive/v1.1.0.tar.gz|direct@https://ghfast.top/https://github.com/google/brotli/archive/v1.1.0.tar.gz|direct@https://codeload.github.com/google/brotli/tar.gz/v1.1.0
double-conversion|04ec44461850abbf33824da84978043b22554896b552c5fd11a9c5ae4b4d296e|direct|0|0|direct@https://gh-proxy.com/https://github.com/google/double-conversion/archive/refs/tags/v3.3.0.tar.gz|direct@https://ghfast.top/https://github.com/google/double-conversion/archive/refs/tags/v3.3.0.tar.gz|direct@https://codeload.github.com/google/double-conversion/tar.gz/refs/tags/v3.3.0
lz4|0b0e3aa07c8c063ddf40b082bdf7e37a1562bda40a0ff5272957f3e987e0e54b|direct|0|0|direct@https://gh-proxy.com/https://github.com/lz4/lz4/archive/v1.9.4.tar.gz|direct@https://ghfast.top/https://github.com/lz4/lz4/archive/v1.9.4.tar.gz|direct@https://codeload.github.com/lz4/lz4/tar.gz/v1.9.4
md4c|55d0111d48fb11883aaee91465e642b8b640775a4d6993c2d0e7a8092758ef21|direct|0|0|direct@https://gh-proxy.com/https://github.com/mity/md4c/archive/refs/tags/release-0.5.2.tar.gz|direct@https://ghfast.top/https://github.com/mity/md4c/archive/refs/tags/release-0.5.2.tar.gz|direct@https://codeload.github.com/mity/md4c/tar.gz/refs/tags/release-0.5.2
winflexbison|8e1b71e037b524ba3f576babb0cf59182061df1f19cd86112f085a882560f60b|direct|0|0|direct@https://gh-proxy.com/https://github.com/lexxmark/winflexbison/archive/v2.5.25.tar.gz|direct@https://ghfast.top/https://github.com/lexxmark/winflexbison/archive/v2.5.25.tar.gz|direct@https://codeload.github.com/lexxmark/winflexbison/tar.gz/v2.5.25
glib|ce85a947bb8b3c0204dbeff79aec39bcb46371c6fafb64ba5b8726c71e038d5f|direct|0|0|direct@https://download.gnome.org/sources/glib/2.86/glib-2.86.5.tar.xz
libpq|dd27f2b3c59e73ed14aa3324901242bf69a032a6347805f274e6260322d42979|direct|0|0|direct@https://ftp.postgresql.org/pub/source/v17.11/postgresql-17.11.tar.bz2
m4|63aede5c6d33b6d9b13511cd0be2cac046f2e70fd0a07aa9573a04a82783af96|direct|0|0|direct@https://ftpmirror.gnu.org/gnu/m4/m4-1.4.19.tar.xz|http@https://ftp.gnu.org/gnu/m4/m4-1.4.19.tar.xz
libgettext|49f089be11b490170bbf09ed2f51e5f5177f55be4cc66504a5861820e0fb06ab|http|0|0|http@https://ftpmirror.gnu.org/gnu/gettext/gettext-0.22.tar.gz|http@https://ftp.gnu.org/gnu/gettext/gettext-0.22.tar.gz
libiconv|8f74213b56238c85a50a5329f77e06198771e70dd9a739779f4c02f65d971313|http|0|0|http@https://ftpmirror.gnu.org/gnu/libiconv/libiconv-1.17.tar.gz|http@https://ftp.gnu.org/gnu/libiconv/libiconv-1.17.tar.gz
nasm|c77745f4802375efeee2ec5c0ad6b7f037ea9c87c92b149a9637ff099f162558|direct|0|0|direct@https://www.nasm.us/pub/nasm/releasebuilds/2.16.01/nasm-2.16.01.tar.xz|socks5@https://www.nasm.us/pub/nasm/releasebuilds/2.16.01/nasm-2.16.01.tar.xz
pkgconf|cd05c9589b9f86ecf044c10a2269822bc9eb001eced2582cfffd658b0a50c243|direct|0|0|direct@https://distfiles.ariadne.space/pkgconf/pkgconf-2.5.1.tar.xz
sqlite3|1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d|direct|0|0|direct@https://sqlite.org/2026/sqlite-amalgamation-3530400.zip
vtk|3d311ff2608e971d40222ae01016d404fb07d746292f77edd86786912767a9c1|direct|1|50803879|direct@https://ghfast.top/https://github.com/Kitware/VTK/archive/refs/tags/v9.5.0.tar.gz|direct@https://gh-proxy.com/https://github.com/Kitware/VTK/archive/refs/tags/v9.5.0.tar.gz|direct@https://github.com/Kitware/VTK/archive/refs/tags/v9.5.0.tar.gz|direct@https://codeload.github.com/Kitware/VTK/tar.gz/refs/tags/v9.5.0
"

sha_of() { sha256sum "$1" 2>/dev/null | cut -d' ' -f1; }

# Conan keeps a "<sha256>.json" beside every blob in the sources cache, and
# `conan upload` walks that folder to decide which sources to push as *backup
# sources*.  It aborts the entire upload with
#     ERROR: Missing metadata file for backup source <path>
# the moment it meets one blob without it
# (conan/internal/rest/download_cache.py, get_backup_sources_files).
#
# A cache populated by hand -- this script, or curl -- has the blob and nothing
# beside it, so that failure lands on whoever uploads next: hours later, on
# another machine, and topically unrelated to fetching sources.  Measured here:
# the xz_utils entry below was fetched on 2026-09-15 and broke every
# `conan upload` until it was given a metadata file.
#
# The reference CANNOT be filled in from here: SOURCES carries the package
# *name* but not its version/channel, and get_backup_sources_files matches the
# "references" keys against the references being uploaded -- a guessed key would
# simply never match and the entry would be dropped anyway.  So the map is left
# empty on purpose: inert rather than mis-attributed.  Using the cache is
# unaffected either way, because Conan looks sources up by sha256, not by this
# file.  The only thing that changes is that this blob is not offered as a
# backup source for an upload.
write_metadata() {
  # Two separate `local` statements on purpose: in one statement
  # `local sha="$1" meta="$DEST/$sha.json"` the second assignment reads the
  # *new* local `sha`, which is still unset at that point, and under this
  # script's `set -u` that is fatal -- "sha: unbound variable" (bash 5.3).
  local sha="$1"
  local meta="$DEST/$sha.json"
  [ -f "$meta" ] && return 0
  printf '{"references": {}, "timestamp": %s}\n' "$(date +%s)" > "$meta"
}

# 按 route 生成 curl 的代理参数
proxy_args() {
  case "$1" in
    socks5) printf -- '-x %s' "$PROXY_SOCKS" ;;
    http)   printf -- '-x %s' "$PROXY_HTTP" ;;
    *)      printf -- '--noproxy *' ;;
  esac
}

# 各通道通用容错：僵死连接快速失败、断线自动重试
# --ssl-no-revoke 是本机必须的：schannel 会去查 CRL/OCSP，而吊销服务器在这里
# 不可达，curl 会直接以 rc=35 中止握手。requests(python) 不做吊销检查，所以
# conan 自己下载时没有这个问题 —— 只有 curl 需要这个开关。
CURL_COMMON=(--ssl-no-revoke --connect-timeout 15 --max-time 3600 --retry 2 --retry-delay 2
             --speed-limit 4096 --speed-time 30 -L -sS)

# 每个 URL 可以写成 "<route>@<url>" 来单独指定路由；没有前缀就用行默认路由。
# 结果放在 SPLIT_ROUTE / SPLIT_URL 两个全局变量里。
split_route() {
  case "$1" in
    socks5@*|http@*|direct@*) SPLIT_ROUTE="${1%%@*}"; SPLIT_URL="${1#*@}" ;;
    *)                        SPLIT_ROUTE="$2";      SPLIT_URL="$1" ;;
  esac
}

# 拿远端文件总长。三种办法按可靠性排序，实测在这个网络下差别巨大：
#   1. HEAD / Content-Length   —— archives.boost.io 的 CloudFront 压根不回
#   2. Range: 0-0 / Content-Range —— 有时也被挡
#   3. 416 技巧：请求一个远超文件长度的 range，服务器必回
#      `Content-Range: bytes */<TOTAL>` —— 实测一击命中
# 所以直接只用第 3 种。输出必须落盘：-o /dev/null 在原生 Windows curl 上
# 会报 "client returned ERROR on write"，连响应头都拿不到。
remote_size() {
  local url="$1" route="$2"
  local -a px=()
  read -r -a px <<< "$(proxy_args "$route")"
  curl "${px[@]}" --ssl-no-revoke -s -D - -o "$(WINP "$TMP/.size_probe")" -L -r 99999999999- \
       --connect-timeout 15 --max-time 30 "$url" 2>/dev/null \
    | tr -d '\r' \
    | awk 'tolower($1)=="content-range:"{ split($2,a,"/"); if (a[2] != "*") print a[2] }' \
    | tail -1
}

fetch_one() {
  local name="$1" sha="$2" route="$3" want_chunks="$4" want_size="$5"; shift 5
  local -a urls=("$@")
  local target="$DEST/$sha"
  local part="$TMP/$name.part"

  if [ -f "$target" ] && [ "$(sha_of "$target")" = "$sha" ]; then
    # Idempotent, and the only thing that repairs an entry fetched before
    # write_metadata() existed: re-running this script heals the old blobs
    # instead of leaving them to break somebody else's `conan upload`.
    write_metadata "$sha"
    printf '[skip] %-10s already cached\n' "$name"
    return 0
  fi
  [ -f "$target" ] && mv "$target" "$target.bad_$(date +%H%M%S)"

  # 决定分块数。长度优先用配置里写死的，探测只作兜底。
  local total nchunks per
  total="${want_size:-0}"
  if ! [ "$total" -gt 0 ] 2>/dev/null; then
    split_route "${urls[0]}" "$route"
    total="$(remote_size "$SPLIT_URL" "$SPLIT_ROUTE")"
  fi
  [ -z "$total" ] && total=0
  if [ "${want_chunks:-0}" -gt 0 ] 2>/dev/null; then
    nchunks="$want_chunks"
  elif [ "$total" -gt "$CHUNK_THRESHOLD" ]; then
    nchunks="$CHUNK_COUNT"
  else
    nchunks=1
  fi
  # 分块要靠 total 算字节范围，没有就只能退回单块
  if [ "$nchunks" -gt 1 ] && [ "$total" -eq 0 ]; then
    printf '[warn] %-10s size unknown, falling back to 1 chunk\n' "$name" >&2
    nchunks=1
  fi
  per=$(( total > 0 ? (total + nchunks - 1) / nchunks : 0 ))

  printf '[get ] %-10s total=%s route=%s chunks=%s\n' "$name" "${total:-?}" "$route" "$nchunks"

  local try i u from to fi rc uroute uurl
  local -a PX=() UPX=()
  for ((try = 1; try <= MAX_TRIES; try++)); do
    [ "$nchunks" -eq 1 ] && printf '[    ] %-10s try %d\n' "$name" "$try"

    for ((i = 0; i < nchunks; i++)); do
      if [ "$nchunks" -eq 1 ]; then
        fi="$part"; from=0; to=""
      else
        fi="$TMP/$name.chunk.$i"
        from=$(( i * per )); to=$(( from + per - 1 ))
        # 已经够长的块跳过
        if [ -f "$fi" ] && [ "$(stat -c %s "$fi")" -ge "$((to - from + 1))" ]; then continue; fi
      fi

      for u in "${urls[@]}"; do
        [ -z "$u" ] && continue
        split_route "$u" "$route"
        uroute="$SPLIT_ROUTE"; uurl="$SPLIT_URL"
        read -r -a UPX <<< "$(proxy_args "$uroute")"
        if [ "$nchunks" -eq 1 ]; then
          curl "${UPX[@]}" "${CURL_COMMON[@]}" -C - -o "$(WINP "$fi")" "$uurl" && break
        else
          # 分块必须用显式 -r。注意 -C - 不能和 -r 同时出现，否则 curl 直接
          # 报 "option -r: is badly used here"。块不大（这单 15MB/块），
          # 失败就整块重下，不必在块内做续传。
          curl "${UPX[@]}" "${CURL_COMMON[@]}" -r "$from-$to" \
               -o "$(WINP "$fi")" "$uurl" && break
        fi
      done
    done

    # 校验
    if [ "$nchunks" -eq 1 ]; then
      if [ -f "$part" ] && [ "$(sha_of "$part")" = "$sha" ]; then
        mv "$part" "$target"
        write_metadata "$sha"
        printf '[ok  ] %-10s %s (%s)\n' "$name" "$sha" "$(du -h "$target" | cut -f1)"
        return 0
      fi
    else
      local complete=1
      for ((i = 0; i < nchunks; i++)); do
        [ -f "$TMP/$name.chunk.$i" ] || { complete=0; break; }
      done
      if [ "$complete" -eq 1 ]; then
        cat "$TMP/$name.chunk."* > "$TMP/$name.assembled" 2>/dev/null
        if [ "$(sha_of "$TMP/$name.assembled")" = "$sha" ]; then
          mv "$TMP/$name.assembled" "$target"
          rm -f "$TMP/$name.chunk."*
          write_metadata "$sha"
          printf '[ok  ] %-10s %s (%s)\n' "$name" "$sha" "$(du -h "$target" | cut -f1)"
          return 0
        fi
        printf '[    ] %-10s assembled but sha mismatch, restarting\n' "$name"
        rm -f "$TMP/$name.chunk."*
      fi
    fi
    sleep 3
  done

  printf '[FAIL] %-10s gave up after %d tries\n' "$name" "$MAX_TRIES" >&2
  return 1
}

printf 'cache  : %s\n' "$CACHE"
printf 'socks5 : %s\n' "$PROXY_SOCKS"
printf 'http   : %s\n\n' "$PROXY_HTTP"

# 所有包一次性并行启动：已完成的会立刻 skip，慢包不会堵住其他包
for line in $SOURCES; do
  [ -z "$line" ] && continue
  IFS='|' read -r name sha route chunks size rest <<< "$line"
  IFS='|' read -r -a extra <<< "$rest"
  ( fetch_one "$name" "$sha" "$route" "$chunks" "$size" "${extra[@]}" ) > "$TMP/$name.log" 2>&1 &
done
wait

echo
for line in $SOURCES; do
  [ -z "$line" ] && continue
  IFS='|' read -r name rest <<< "$line"
  cat "$TMP/$name.log" 2>/dev/null
done

echo
ok=0; miss=0
for line in $SOURCES; do
  [ -z "$line" ] && continue
  IFS='|' read -r name sha rest <<< "$line"
  if [ -f "$DEST/$sha" ]; then ok=$((ok + 1)); else miss=$((miss + 1)); printf 'MISSING: %s\n' "$name"; fi
done
printf '\n%d/%d sources cached under %s\n' "$ok" "$((ok + miss))" "$DEST"
[ "$miss" -eq 0 ] || exit 1
