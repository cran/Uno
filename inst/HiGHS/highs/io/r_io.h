/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**@file io/r_io.h
 * @brief Console-I/O redirection for the R-package build of HiGHS.
 *
 * This header is GUARDED by the macro HIGHS_R_PRINT:
 *
 *   - When HIGHS_R_PRINT is NOT defined (every upstream / standalone HiGHS
 *     build), the header is a thin pass-through: HIGHS_COUT == std::cout,
 *     HIGHS_CERR == std::cerr, and the standard `printf` / `stdout` are left
 *     untouched.  Behavior is byte-identical to upstream, so the fork stays
 *     trivially mergeable.
 *
 *   - When HIGHS_R_PRINT IS defined (the CVXR `Uno` R-package build, which
 *     passes -DHIGHS_R_PRINT and force-includes this header via the compiler
 *     `-include` flag for BOTH C and C++ TUs), every console write is routed to
 *     R's console through Rprintf / REprintf, and the libc symbols `stdout`
 *     (___stdoutp), `printf` (_printf / _puts / _putchar) and `std::cout` /
 *     `std::cerr` (the C++ stream objects) are never referenced.  This is what
 *     lets `R CMD check`'s "checking compiled code" pass for a bundled HiGHS.
 *
 * `printf` and `stdout` are redirected with macros (HiGHS has 600+ debug
 * `printf` sites and the `stdout` token is both a write target and a console
 * sentinel, so editing every site is impractical and error-prone).  The C++
 * `std::cout` / `std::cerr` cannot be macro-replaced (qualified names), so the
 * two compiled files that use them (`ipm/ipx/control.cc`, `test_kkt/DevKkt.cpp`)
 * are edited to use HIGHS_COUT / HIGHS_CERR instead.
 *
 * `stdout` REDIRECTION -- two portable strategies, selected per platform:
 *
 *   (A) funopen / fopencookie (macOS, *BSD, glibc): `stdout` is remapped to a
 *       REAL FILE* that forwards every write to Rprintf.  Because it is a real
 *       stream, every use stays correct without further work: console writes
 *       route to R, `file == stdout` sentinel comparisons are consistent (the
 *       FILE* is a cached singleton), and unguarded writers such as
 *       `writeRangingFile(stdout, ...)` or the MPS/LP writers (which receive a
 *       `file` that may equal `stdout` from `openWriteFile`) just work.
 *
 *   (B) sentinel + wrapped write functions (Windows / MinGW / UCRT, and any
 *       platform lacking funopen AND fopencookie): there is no portable way to
 *       fabricate a custom FILE* on UCRT, so instead `stdout` is remapped to a
 *       distinguished SENTINEL FILE* (the address of a private static byte that
 *       is NEVER handed to the C runtime), and the small, closed set of stream
 *       WRITE functions that HiGHS ever applies to a stdout-valued FILE* --
 *       `fprintf`, `vfprintf`, `fflush` -- are wrapped so that, when their
 *       FILE* argument is the sentinel, they route to Rprintf instead of
 *       touching the CRT.  An exhaustive grep of the compiled sources confirms
 *       these three are the ONLY write/flush functions ever reached with a
 *       stdout-valued stream (no fputs/fputc/fwrite/putc on stdout/file exist).
 *       Sentinel comparisons (`file == stdout`, `file != stdout`) and the
 *       `if (file != stdout) fclose(file)` guards remain correct because the
 *       sentinel is a stable, unique pointer that no writer ever dereferences.
 *
 * `abort` / `exit` REDIRECTION: both are remapped (function-like macros) to
 * R's error path `Rf_error`, which longjmps and is `noreturn` -- matching the
 * `noreturn` contract of `abort`/`exit` so no spurious "control reaches end of
 * non-void function" warnings arise.  Function-like macro form (`abort()`,
 * `exit(code)`) means the unrelated local variable `bool abort` in
 * HighsTransformedLp.cpp (used without parentheses) is untouched.  See the
 * inventory comment at the macro definitions for every live call site.
 *
 * This header is included by both C and C++ translation units, so everything
 * outside `#ifdef __cplusplus` must be valid C.
 */
#ifndef HIGHS_IO_R_IO_H_
#define HIGHS_IO_R_IO_H_

#if defined(HIGHS_R_PRINT)

/* R's <R_ext/Error.h> / <R_ext/Print.h> install unprefixed aliases
 * `error`/`warning`/`print*` for the `Rf_`-prefixed entry points UNLESS
 * R_NO_REMAP is set.  Those bare names collide with C++ standard-library
 * identifiers (e.g. libc++ <fstream> uses `codecvt_base::error`).  We use the
 * `Rf_`-prefixed forms exclusively, so suppress the remap.  Must be defined
 * before any R header is included. */
#ifndef R_NO_REMAP
#define R_NO_REMAP
#endif

/* fopencookie (glibc) needs _GNU_SOURCE before any libc header.  This header is
 * force-included first in the R build, so defining it here is in time. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif

/* Pull in the standard headers BEFORE redefining `printf` / `stdout` /
 * `fprintf` / `abort` / `exit`, so the libc / libc++ / libstdc++ headers are
 * parsed with the real definitions. */
#ifdef __cplusplus
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#else
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif
#if defined(__linux__)
#include <sys/types.h> /* ssize_t for the fopencookie write callback */
#endif

#include <R_ext/Error.h> /* Rf_error (noreturn) -- for abort / exit redirect */
#include <R_ext/Print.h> /* Rprintf, REprintf (C linkage) */

#ifdef __cplusplus
extern "C" {
#endif

/* ----- stdout replacement: a real FILE* whose writes go to Rprintf ----------
 * Using a real FILE* (rather than a fake sentinel) keeps every use correct:
 * console writes are routed, `file == stdout` comparisons stay consistent, and
 * unguarded writers do not crash.  The libc `stdout` symbol is never named.
 * Declared `static inline` so it is shareable from both C and C++ TUs without a
 * separate definition, and does not warn when a TU includes the header but only
 * uses `printf` (not `stdout`). */
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__)

static int highs_r_write_fn(void* cookie, const char* buf, int n) {
  (void)cookie;
  if (n > 0) Rprintf("%.*s", n, buf);
  return n;
}
static inline FILE* highs_r_rconsole(void) {
  static FILE* f = NULL;
  if (!f) {
    f = funopen(NULL, NULL, &highs_r_write_fn, NULL, NULL);
    if (f) setvbuf(f, NULL, _IONBF, 0);
  }
  return f;
}

#elif defined(__GLIBC__)

static ssize_t highs_r_write_fn(void* cookie, const char* buf, size_t n) {
  (void)cookie;
  if (n > 0) Rprintf("%.*s", (int)n, buf);
  return (ssize_t)n;
}
static inline FILE* highs_r_rconsole(void) {
  static FILE* f = NULL;
  if (!f) {
    cookie_io_functions_t io;
    memset(&io, 0, sizeof(io));
    io.write = &highs_r_write_fn;
    f = fopencookie(NULL, "w", io);
    if (f) setvbuf(f, NULL, _IONBF, 0);
  }
  return f;
}

#else /* no funopen / fopencookie (Windows / MinGW / UCRT): sentinel + wrappers
       * ----------------------------------------------------------------------
       * UCRT's FILE is opaque and there is no portable way to install a custom
       * write callback, so we cannot fabricate a routing FILE*.  Instead use a
       * distinguished SENTINEL FILE* -- the address of a private static byte --
       * as the value of `stdout`.  The sentinel is never passed to the CRT; the
       * wrapped write functions below detect it and route to Rprintf.  Using a
       * private object's address (not the real `stdout`) guarantees the libc
       * `stdout` symbol is never referenced, so it cannot appear in the .a. */

/* One private static byte per TU; its address is the unique sentinel.  Cast
 * through void* to silence the (harmless) aliasing of the byte as a FILE*; the
 * pointer is only ever compared, never dereferenced. */
static char highs_r_stdout_sentinel_storage_ = 0;
static inline FILE* highs_r_rconsole(void) {
  return (FILE*)(void*)&highs_r_stdout_sentinel_storage_;
}

/* Variadic forwarder: when `stream` is our sentinel, route to Rprintf;
 * otherwise fall through to the real fprintf.  Marked with the printf
 * format attribute under GCC/Clang so format-string checking still works. */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
static inline int
highs_r_fprintf(FILE* stream, const char* format, ...) {
  va_list ap;
  int ret;
  va_start(ap, format);
  if (stream == highs_r_rconsole()) {
    char buf[4096];
    ret = vsnprintf(buf, sizeof(buf), format, ap);
    if (ret > 0) Rprintf("%s", buf);
  } else {
    ret = vfprintf(stream, format, ap);
  }
  va_end(ap);
  return ret;
}

static inline int highs_r_vfprintf(FILE* stream, const char* format,
                                   va_list ap) {
  if (stream == highs_r_rconsole()) {
    char buf[4096];
    int ret = vsnprintf(buf, sizeof(buf), format, ap);
    if (ret > 0) Rprintf("%s", buf);
    return ret;
  }
  return vfprintf(stream, format, ap);
}

static inline int highs_r_fflush(FILE* stream) {
  /* The R console is unbuffered from HiGHS's point of view; flushing the
   * sentinel is a no-op.  A NULL flush (flush all) still flushes real streams. */
  if (stream == highs_r_rconsole()) return 0;
  return fflush(stream);
}

#endif /* funopen / fopencookie vs sentinel */

#ifdef __cplusplus
}  /* extern "C" */
#endif

/* ----- std::cout / std::cerr replacement: a streambuf over Rprintf/REprintf -
 * C++ only; macro-replacement of qualified `std::cout` is impossible, so the
 * two files that use the C++ streams call HIGHS_COUT / HIGHS_CERR instead. */
#ifdef __cplusplus
#include <iostream>
#include <ostream>
#include <streambuf>

namespace highs_r {

class RStreamBuf : public std::streambuf {
 public:
  explicit RStreamBuf(bool to_stderr) : to_stderr_(to_stderr) {}

 protected:
  std::streamsize xsputn(const char* s, std::streamsize n) override {
    if (to_stderr_)
      REprintf("%.*s", static_cast<int>(n), s);
    else
      Rprintf("%.*s", static_cast<int>(n), s);
    return n;
  }
  int overflow(int c) override {
    if (c != EOF) {
      const char ch = static_cast<char>(c);
      if (to_stderr_)
        REprintf("%c", ch);
      else
        Rprintf("%c", ch);
    }
    return c;
  }

 private:
  bool to_stderr_;
};

inline std::ostream& rcout() {
  static RStreamBuf buf(false);
  static std::ostream os(&buf);
  return os;
}
inline std::ostream& rcerr() {
  static RStreamBuf buf(true);
  static std::ostream os(&buf);
  return os;
}

}  // namespace highs_r

#define HIGHS_COUT (::highs_r::rcout())
#define HIGHS_CERR (::highs_r::rcerr())
#endif /* __cplusplus */

/* Redirect the standard `printf` and `stdout` (done AFTER the system headers
 * above).  HiGHS has no qualified (`std::printf`) or return-value uses of
 * `printf`, so the function-like macro form is safe. */
#undef printf
#define printf(...) Rprintf(__VA_ARGS__)

#undef stdout
#define stdout (highs_r_rconsole())

/* On the sentinel platforms (no funopen / fopencookie), also wrap the three
 * stream WRITE/flush functions that HiGHS ever applies to a stdout-valued
 * FILE*, so a write through the sentinel routes to Rprintf instead of the CRT.
 * On funopen / fopencookie platforms these are left untouched: the routing
 * FILE* handles everything natively.  HiGHS has no qualified (`std::fprintf`),
 * function-pointer, or return-value-into-state uses of these, so the
 * function-like macro form is safe. */
#if !(defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
      defined(__OpenBSD__) || defined(__DragonFly__) || defined(__GLIBC__))
/* C++ standard-library stream headers call `std::fflush` / `std::fprintf`
 * internally (e.g. libc++ / libstdc++ <fstream>'s basic_filebuf::sync).  A
 * function-like macro `#define fflush ...` would still expand the `fflush`
 * token in `std::fflush(...)`, mangling those internal calls.  So pre-include
 * the C++ stream headers HERE -- with the real definitions still in scope --
 * before installing the macros, exactly as the C stdio headers are pre-included
 * at the top.  After this point HiGHS itself only uses the unqualified forms,
 * which the macros below intercept correctly. */
#ifdef __cplusplus
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <streambuf>
#endif
#undef fprintf
#define fprintf highs_r_fprintf
#undef vfprintf
#define vfprintf highs_r_vfprintf
#undef fflush
#define fflush highs_r_fflush
#endif

/* Redirect the fatal-exit primitives to R's error path.  `Rf_error` longjmps
 * and is noreturn, matching the noreturn contract of `abort` / `exit` (so no
 * "control reaches end of non-void function" warnings).  Function-like macro
 * form leaves non-call identifiers (e.g. the local `bool abort;` in
 * HighsTransformedLp.cpp) untouched.  HiGHS's own sources use no qualified
 * `std::abort` / `std::exit`, function-pointer, or non-call uses.
 *
 * Caveat handled below: the C++ standard library DOES use `std::abort`
 * internally (e.g. libc++/libstdc++ <exception>'s exception_ptr machinery, and
 * the standard <cassert>).  As with fflush above, a function-like `abort()`
 * macro would mangle `std::abort()` inside those headers.  So pre-include the
 * standard headers that reference `std::abort` / `std::exit` HERE -- with the
 * real declarations in scope -- before installing the macros.
 *
 * Live call sites eliminated (verified by nm: _abort x1, _exit x4):
 *   abort()  : util/HighsUtils.cpp:1241        (highsAssert, NDEBUG path)
 *   exit(.)  : mip/HighsCliqueTable.cpp:1717    (separateCliques guard)
 *              pdlp/cupdlp/cupdlp_linalg.c      (CPU error paths)
 *              pdlp/cupdlp/cupdlp_scaling.c:104,191
 *              pdlp/cupdlp/cupdlp_step.c:212
 * plus dead/dev-report exit() sites (HighsLpRelaxation, HighsMipSolverData)
 * defensively neutralized whether or not the optimizer keeps them. */
#ifdef __cplusplus
#include <cassert>
#include <cstdlib>
#include <exception>
#include <new>
#include <stdexcept>
#endif
#undef abort
#define abort() Rf_error("HiGHS: internal abort()")
#undef exit
#define exit(code) Rf_error("HiGHS: internal exit(%d)", (int)(code))

#else /* !HIGHS_R_PRINT -- upstream build: byte-identical pass-through */

#ifdef __cplusplus
#include <iostream>
#define HIGHS_COUT std::cout
#define HIGHS_CERR std::cerr
#endif

#endif /* HIGHS_R_PRINT */

#endif /* HIGHS_IO_R_IO_H_ */
