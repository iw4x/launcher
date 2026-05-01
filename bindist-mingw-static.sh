usage="Usage: $0 [-h|--help] [<options>] <package-spec>"

bdir="cfg-mingw-static"
odir="dist-output"
pkg_spec=

yes=
jobs=

owd="$(pwd)"
prog="$0"

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
      diag "  --build-dir|-b    Directory for the MinGW build configuration ('$bdir' by default)."
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

pkg_name="${pkg_spec%%@*}"

check_cmd bpkg "is the build2 toolchain installed and in your PATH?"
check_cmd x86_64-w64-mingw32-g++ "is the MinGW-w64 compiler toolchain installed?"
check_cmd unzip "is the unzip utility installed?"
check_cmd zip "is the zip utility installed?"

diag
diag "-------------------------------------------------------------------------"
diag
diag "About to generate MinGW static binary distribution."
diag
diag "Package Spec:     $pkg_spec"
diag "Package Name:     $pkg_name"
diag "Build directory:  $owd/$bdir/"
diag "Output directory: $owd/$odir/"
if test -n "$jobs"; then
diag "Parallel jobs:    $jobs"
fi
diag
if test -d "$bdir"; then
diag "WARNING: $bdir/ already exists and will be used as-is."
diag "  info: If you need to apply new compiler flags, delete $bdir/ first."
diag
fi
prompt_continue

if ! test -d "$bdir"; then
  run bpkg create -d "$bdir" cc       \
    config.cxx=x86_64-w64-mingw32-g++ \
    config.c=x86_64-w64-mingw32-gcc   \
    "config.cxx.coptions=-O3"         \
    "config.c.coptions=-O3"           \
    config.install.relocatable=true   \
    config.bin.lib=static             \
    config.c.loptions="-static -static-libgcc" \
    config.cxx.loptions="-static -static-libgcc -static-libstdc++"
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
run bpkg bindist -d "$bdir"              \
  --distribution=archive                 \
  --architecture=x86_64-w64-mingw32      \
  --os-release-id=windows                \
  --os-release-name=Windows              \
  --archive-install-root /               \
  --archive-lang cc=mingw_w64_gcc_static \
  -o "$odir"                             \
  "$pkg_name"

# The install module is quite rigid about maintaining the standard Unix
# directory hierarchy (bin/, share/, etc.), even when we try to coerce it into
# producing a minimal Windows distribution. So instead of fighting it, we
# append a post-processing phase at the end to extracts the generated archive
# into a temporary workspace, fishes out the .exe, and repackages it into the
# root of a new archive.

dist_zip=
for f in "$odir"/*.zip; do
  if [ -f "$f" ]; then
    dist_zip="$f"
    break
  fi
done

if test -z "$dist_zip"; then
  error "could not find the generated .zip archive in $odir/"
fi

tmp_workspace="$(mktemp -d)"

diag
diag "-------------------------------------------------------------------------"
diag
diag "Performing post-processing to flatten the archive hierarchy."
diag
diag "Target Archive:   $owd/$dist_zip"
diag "Workspace:        $tmp_workspace/"
diag

run unzip -q "$dist_zip" -d "$tmp_workspace/extracted"
run mkdir -p "$tmp_workspace/flat"

find "$tmp_workspace/extracted" -type f -name "*.exe" -exec cp {} "$tmp_workspace/flat/" \;

run rm "$dist_zip"
run cd "$tmp_workspace/flat"

run zip -q -r "$owd/$dist_zip" .

run cd "$owd"
rm -rf "$tmp_workspace"

diag
diag "-------------------------------------------------------------------------"
diag
diag "Generated binary distribution."
diag
diag "Output Archive: $owd/$dist_zip"
diag
