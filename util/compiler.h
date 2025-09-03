// Copyright (c) 2025, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0, as
// published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms, as
// designated in a particular file or component or in included license
// documentation. The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// Without limiting anything contained in the foregoing, this file,
// which is part of Connector/ODBC, is also subject to the
// Universal FOSS Exception, version 1.0, a copy of which can be found at
// https://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA


#ifndef _COMPILER_H
#define _COMPILER_H

#include <inttypes.h>

/*
  Macros used to disable warnings for fragments of code.
*/

#if defined __GNUC__ || defined __clang__

#define PRAGMA(X) _Pragma(#X)
#define DISABLE_WARNING(W) PRAGMA(GCC diagnostic ignored #W)

#if defined __clang__ || __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#define DIAGNOSTIC_PUSH PRAGMA(GCC diagnostic push)
#define DIAGNOSTIC_POP  PRAGMA(GCC diagnostic pop)
#else
#define DIAGNOSTIC_PUSH
#define DIAGNOSTIC_POP
#endif

#elif defined _MSC_VER


#define PRAGMA(X) __pragma(X)
#define DISABLE_WARNING(W) PRAGMA(warning (disable:W))

#define DIAGNOSTIC_PUSH  PRAGMA(warning (push))
#define DIAGNOSTIC_POP   PRAGMA(warning (pop))

#else

#define PRAGMA(X)
#define DISABLE_WARNING(W)

#define DIAGNOSTIC_PUSH
#define DIAGNOSTIC_POP

#endif


/*
  NO_WARNINGS_PUSH/POP macros are intended to locally disable warnings
  for example when including 3-rd party headers (however, it should not be used
  for std library headers -- we should ensure they are used in a way that does
  not generate any warnings)
*/

#if defined _MSC_VER

#define NO_WARNINGS_PUSH  PRAGMA(warning (push,0))
#define NO_WARNINGS_POP   PRAGMA(warning (pop))

#define NO_FORMAT_WARNINGS_PUSH
#define NO_FORMAT_WARNINGS_POP

#else

#define NO_WARNINGS_BASE \
  DISABLE_WARNING(-Wall) DISABLE_WARNING(-Wextra) DISABLE_WARNING(-Wcpp) \
  DISABLE_WARNING(-Wdeprecated-declarations) \

#ifdef HAVE_DEPRECATED_BUILTINS
#define NO_WARNINGS NO_WARNINGS_BASE DISABLE_WARNING(-Wdeprecated-builtins)
#else
#define NO_WARNINGS NO_WARNINGS_BASE
#endif

#define NO_WARNINGS_PUSH  DIAGNOSTIC_PUSH NO_WARNINGS
#define NO_WARNINGS_POP   DIAGNOSTIC_POP


#define NO_FORMAT_WARNINGS_PUSH DIAGNOSTIC_PUSH \
  DISABLE_WARNING(-Wformat) DISABLE_WARNING(-Wformat-truncation)
#define NO_FORMAT_WARNINGS_POP  DIAGNOSTIC_POP

#endif

#endif /* _COMPILER_H */

