#!/usr/bin/python3

header = r"""/*
 * GLIBC compatibility header
 *
 * Copyright 2021 Bill Zissimopoulos
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall NOT be removed
 * from this file.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define GLIBC_COMPAT_STR__(x)           GLIBC_COMPAT_STR___(x)
#define GLIBC_COMPAT_STR___(x)          #x
#if defined(__ASSEMBLER__)
#define GLIBC_COMPAT_SYMVER__(a,b)      .symver a,b
#else
#define GLIBC_COMPAT_SYMVER__(a,b)      __asm__(".symver " GLIBC_COMPAT_STR__(a) "," GLIBC_COMPAT_STR__(b))
#endif
"""

footer = r"""
#undef GLIBC_COMPAT_SYMVER__
#undef GLIBC_COMPAT_STR__
#undef GLIBC_COMPAT_STR___
"""

import re
import sys

symtab = {}
for line in sys.stdin:
    line = line.rstrip()
    part = line.split("\t", 2)
    if 2 != len(part):
        continue
    part = part[1].split()
    if 3 != len(part) or "GLIBC_PRIVATE" in part[1] or part[2].startswith("GLIBC_"):
        continue
    sym = part[2]
    ver = part[1]
    symtab.setdefault(sym, []).append(ver)

print(header)
for sym in sorted(symtab.keys()):
    ver = symtab[sym]
    ver = sorted(ver, key=lambda v: [int(p) if p.isdigit() else p.lower() for p in re.split(r'(\d+)', v)])
    ver = ver[-1].lstrip("(").rstrip(")")
    print("GLIBC_COMPAT_SYMVER__(%s,%s@%s);" % (sym, sym, ver))
print(footer)
