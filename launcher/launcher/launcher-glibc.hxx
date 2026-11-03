// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

// glibc symbol version bindings for the glibc floor (see launcher-glibc.cxx).
//
// By default a glibc symbol reference binds to the latest version of the
// symbol in the glibc used for the build, which can exceed the glibc on the
// host that runs the launcher. Every translation unit of the launcher except
// launcher-glibc.cxx includes this header first (-include), which binds each
// symbol to its latest version at the floor. The versions differ between
// architectures, so we generate the bindings for each of them (see
// etc/private/glibc-symbols).
//
#pragma once

#if defined(__x86_64__)
#  include <launcher/launcher-glibc-x86_64.hxx>
#else
#  error no glibc floor bindings for this architecture
#endif
