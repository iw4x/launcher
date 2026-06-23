#!/bin/sh

usage="Usage: $0 [-h|--help] [<options>] <package-spec>"

bdir="cfg-gcc-static"
odir="dist-output"
pkg_spec=

yes=
jobs=

owd="$(pwd)"
prog="$0"

glibc_compat_version="2.41"
glibc_compat_header="$owd/etc/private/glibc-compat/glibc-$glibc_compat_version.h"
glibc_compat_poption="-include$glibc_compat_header"
static_c_loptions="-static-libgcc"
static_cxx_loptions="-static-libgcc -static-libstdc++"

fail ()
{
  cd "$owd"
  exit 1
}

diag ()
{
  echo "$*" 1>&2
}

error ()
{
  diag "error: $*"
  fail
}

run ()
{
  diag "+ $@"
  "$@"
  if test "$?" -ne "0"; then
    fail
  fi
}

check_cmd () # <cmd> [<hint>]
{
  if ! command -v "$1" >/dev/null 2>&1; then
    diag "error: unable to execute $1: command not found"
    if test -n "$2"; then
      diag "  info: $2"
    fi
    fail
  fi
}

check_file () # <file> <hint>
{
  if ! test -f "$1"; then
    diag "error: required file not found: $1"
    if test -n "$2"; then
      diag "  info: $2"
    fi
    fail
  fi
}

package_name () # <package-spec>
{
  case "$1" in
    *@*)
      echo "${1%%@*}"
      ;;
    */)
      d="${1%/}"
      if test -f "$d/manifest"; then
        n="$(sed -n 's/^name:[	 ]*//p' "$d/manifest" | sed -n '1p')"
        if test -n "$n"; then
          echo "$n"
          return
        fi
      fi
      basename "$d"
      ;;
    *)
      echo "$1"
      ;;
  esac
}

check_existing_build_config ()
{
  cfg="$bdir/build/config.build"

  if ! test -f "$cfg"; then
    error "existing build configuration is missing $cfg"
  fi

  if ! grep -F -- "$glibc_compat_poption" "$cfg" >/dev/null 2>&1; then
    diag "error: $bdir/ already exists without the required GLIBC compat include."
    diag "  info: Delete $bdir/ or recreate it with:"
    diag "  info:   config.cc.poptions=$glibc_compat_poption"
    fail
  fi

  if ! grep -F -- "config.c.loptions = $static_c_loptions" "$cfg" >/dev/null 2>&1 ||
     ! grep -F -- "config.cxx.loptions = $static_cxx_loptions" "$cfg" >/dev/null 2>&1; then
    diag "error: $bdir/ already exists without the required static GCC runtime link options."
    diag "  info: Delete $bdir/ or recreate it with:"
    diag "  info:   config.c.loptions=\"$static_c_loptions\""
    diag "  info:   config.cxx.loptions=\"$static_cxx_loptions\""
    fail
  fi
}

version_greater () # <version> <floor>
{
  lhs_major="${1%%.*}"
  lhs_minor="${1#*.}"
  lhs_minor="${lhs_minor%%.*}"

  rhs_major="${2%%.*}"
  rhs_minor="${2#*.}"
  rhs_minor="${rhs_minor%%.*}"

  if test "$lhs_major" -gt "$rhs_major"; then
    return 0
  fi

  if test "$lhs_major" -eq "$rhs_major" &&
     test "$lhs_minor" -gt "$rhs_minor"; then
    return 0
  fi

  return 1
}

verify_glibc_floor () # <binary>
{
  binary="$1"

  diag "Verifying GLIBC symbol floor for $binary"

  versions="$(readelf --version-info "$binary" 2>/dev/null | \
    sed -n 's/.*GLIBC_\([0-9][0-9]*\.[0-9][0-9]*\).*/\1/p' | \
    sort -u)"

  bad=
  for v in $versions; do
    if version_greater "$v" "$glibc_compat_version"; then
      bad="$v"
      break
    fi
  done

  if test -n "$bad"; then
    diag "error: $binary requires GLIBC_$bad; release floor is GLIBC_$glibc_compat_version"
    diag "  info: Check the build inputs or regenerate the compat header only after raising the supported distro floor."
    fail
  fi

  diag "  info: GLIBC symbol floor is compatible with GLIBC_$glibc_compat_version"
}

prompt_continue ()
{
  while test -z "$yes"; do
    printf "Continue? [y/n] " 1>&2
    read yes
    case "$yes" in
      y|Y) yes=true ;;
      n|N) fail     ;;
        *) yes=     ;;
    esac
  done
}

while test $# -ne 0; do
  case "$1" in
    -h|--help)
      diag
      diag "$usage"
      diag "Options:"
      diag "  --yes|-y          Do not ask for confirmation before starting."
      diag "  --jobs|-j <num>   Number of jobs to perform in parallel."
      diag "  --build-dir|-b    Directory for the GCC build configuration ('$bdir' by default)."
      diag "  --output-dir|-o   Directory to place the resulting archive ('$odir' by default)."
      diag
      diag "Example:"
      diag "  $0 launcher@https://github.com/iw4x/launcher.git#main"
      diag
      exit 0
      ;;
    --yes|-y)
      yes=true
      shift
      ;;
    -j|--jobs)
      shift
      if test $# -eq 0; then
        error "number of jobs expected after --jobs|-j; run $prog -h for details"
      fi
      jobs="$1"
      shift
      ;;
    -b|--build-dir)
      shift
      if test $# -eq 0; then
        error "directory expected after --build-dir|-b; run $prog -h for details"
      fi
      bdir="$1"
      shift
      ;;
    -o|--output-dir)
      shift
      if test $# -eq 0; then
        error "directory expected after --output-dir|-o; run $prog -h for details"
      fi
      odir="$1"
      shift
      ;;
    -*)
      diag "error: unknown option '$1'"
      diag "  info: run 'sh $prog -h' for usage"
      fail
      ;;
    *)
      if test -z "$pkg_spec"; then
        pkg_spec="$1"
        shift
      else
        diag "error: unexpected argument '$@'"
        diag "  info: options must come before the <package-spec> argument"
        fail
      fi
      break
      ;;
  esac
done

if test -z "$pkg_spec"; then
  error "package specification is required; run 'sh $prog -h' for usage"
fi

pkg_name="$(package_name "$pkg_spec")"

check_cmd bpkg "is the build2 toolchain installed and in your PATH?"
check_cmd g++ "is the native GNU C++ compiler installed?"
check_cmd tar "is the tar utility installed?"
check_cmd xz "is the xz compression utility installed?"
check_cmd readelf "is binutils installed and in your PATH?"
check_file "$glibc_compat_header" "run the repository setup from the project root or restore the vendored GLIBC compatibility header."

diag
diag "-------------------------------------------------------------------------"
diag
diag "About to generate flat native GCC static binary distribution (AVX-512 Optimized)."
diag
diag "Package Spec:     $pkg_spec"
diag "Package Name:     $pkg_name"
diag "Build directory:  $owd/$bdir/"
diag "Output directory: $owd/$odir/"
diag "GLIBC floor:      $glibc_compat_version"
diag "Compat header:    $glibc_compat_header"
if test -n "$jobs"; then
diag "Parallel jobs:    $jobs"
fi
diag
if test -d "$bdir"; then
diag "WARNING: $bdir/ already exists and will be used as-is."
diag "  info: Validating that it already contains the required compatibility flags."
diag
check_existing_build_config
fi
prompt_continue

if ! test -d "$bdir"; then
  run bpkg create -d "$bdir" cc \
    config.cxx=g++ \
    config.c=gcc \
    "config.cc.poptions=$glibc_compat_poption" \
    "config.cxx.coptions=-O3" \
    "config.c.coptions=-O3" \
    "config.c.loptions=$static_c_loptions" \
    "config.cxx.loptions=$static_cxx_loptions" \
    config.bin.lib=static \
    config.install.relocatable=true
else
  diag "info: using existing build configuration in $bdir/"
fi

# Build
#
jobs_opt=
if test -n "$jobs"; then
  jobs_opt="-j $jobs"
fi

run bpkg build -d "$bdir" $jobs_opt --yes "$pkg_spec"

# Bindist
#
run bpkg bindist -d "$bdir"            \
  --distribution=archive               \
  --archive-install-root /             \
  --archive-no-os                      \
  --archive-build-meta="+linux-glibc"  \
  --archive-lang cc=gcc_static         \
  -o "$odir"                           \
  "$pkg_name"

# The install module is quite rigid about maintaining the standard Unix
# directory hierarchy (bin/, share/, etc.), even when we try to coerce it into
# producing a minimal Linux distribution. So instead of fighting it, we append
# a post-processing phase at the end to extracts the generated archive into a
# temporary workspace, fishes out the executable, and repackages it into the
# root of a new archive.

dist_archive=
for f in "$odir"/*.tar.xz; do
  if [ -f "$f" ]; then
    dist_archive="$f"
    break
  fi
done

if test -z "$dist_archive"; then
  error "could not find the generated .tar.xz archive in $odir/"
fi

tmp_workspace="$(mktemp -d)"

diag
diag "-------------------------------------------------------------------------"
diag
diag "Performing post-processing to flatten the archive hierarchy."
diag
diag "Target Archive:   $owd/$dist_archive"
diag "Workspace:        $tmp_workspace/"
diag

run mkdir -p "$tmp_workspace/extracted"
run tar  -xf "$dist_archive" -C "$tmp_workspace/extracted"
run mkdir -p "$tmp_workspace/flat"

for d in "$tmp_workspace/extracted"/*; do
  if [ -d "$d/bin" ]; then
    run cp "$d/bin"/* "$tmp_workspace/flat/"
  fi
done

verified=
for f in "$tmp_workspace/flat"/*; do
  if [ -f "$f" ] && [ -x "$f" ]; then
    verify_glibc_floor "$f"
    verified=true
  fi
done

if test -z "$verified"; then
  error "could not find an executable to verify in the flattened archive"
fi

run rm "$dist_archive"
run cd "$tmp_workspace/flat"

run tar -cJf "$owd/$dist_archive" .

run cd "$owd"
rm -rf "$tmp_workspace"

diag
diag "-------------------------------------------------------------------------"
diag
diag "Generated binary distribution."
diag
diag "Output Archive: $owd/$dist_archive"
diag
