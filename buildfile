# Copyright (c) the IW4x authors (see the AUTHORS file).
# SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

# Glue buildfile that "pulls" all the packages in the project.
#
import pkgs = [dir_paths] $process.run_regex(\
  cat $src_root/packages.manifest, '\s*location\s*:\s*(\S+)\s*', '\1')

./: $pkgs
