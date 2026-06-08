// Copyright (c) 2018-2026 Charlie Vanaret
// Licensed under the MIT license. See LICENSE file in the project directory for details.

/**@file tools/r_io.h
 * @brief Console-I/O redirection for the R-package build of Uno.
 *
 * This header is GUARDED by the macro UNO_R_PRINT:
 *
 *   - When UNO_R_PRINT is NOT defined (every upstream / standalone Uno build,
 *     including the AMPL and Python interfaces), the header is a thin
 *     pass-through: UNO_COUT == std::cout and UNO_CERR == std::cerr.  The
 *     output of `#include "tools/r_io.h"` is then byte-identical to including
 *     <iostream> and writing std::cout / std::cerr, so the fork stays trivially
 *     mergeable with upstream.
 *
 *   - When UNO_R_PRINT IS defined (the CVXR `Uno` R-package build, which passes
 *     -DUNO_R_PRINT on CXXFLAGS), every console write that Uno performs is
 *     routed to R's console through Rprintf / REprintf, and the C++ stream
 *     objects std::cout (__ZNSt3__14coutE) and std::cerr (__ZNSt3__14cerrE) are
 *     never referenced by Uno's object files.  This is what lets `R CMD check`'s
 *     "checking compiled code" step pass for a bundled Uno.
 *
 * Why no `printf` / `stdout` redirection (unlike HiGHS's io/r_io.h)?
 *   Uno's compiled C++ sources do not use the C `printf` family nor the C
 *   `stdout` FILE* token at all -- every console write goes through the C++
 *   `std::cout` / `std::cerr` ostreams (chiefly via tools/Logger.hpp, whose
 *   Logger::stream defaults to &std::cout).  Because there is no use of the C
 *   `stdout` FILE*, there is nothing here that would need funopen (macOS/BSD) or
 *   fopencookie (glibc), and therefore NO platform on which a non-portable
 *   route is required.  In particular, the Windows (MinGW/UCRT) build -- which
 *   lacks both funopen and fopencookie -- is handled by exactly the same
 *   streambuf-over-Rprintf mechanism as macOS and Linux, with no real-stdout
 *   fallback and hence no leaked C `stdout` symbol.
 *
 * `assert()` is compiled out by -DNDEBUG (CMakeLists sets
 * CMAKE_CXX_FLAGS_RELEASE = "-O3 -DNDEBUG"), so the __assert_rtn / abort symbol
 * that assert would otherwise pull in never appears.
 */
#ifndef UNO_TOOLS_R_IO_H_
#define UNO_TOOLS_R_IO_H_

#ifdef __cplusplus

#if defined(UNO_R_PRINT)

// Pull in the standard headers BEFORE introducing the R-backed streams, so the
// libc++ / libstdc++ headers are parsed normally.
#include <iostream>
#include <ostream>
#include <streambuf>

#include <R_ext/Print.h> // Rprintf, REprintf (declared with C linkage)

namespace uno {
namespace r_io {

// A std::streambuf whose writes are forwarded to R's console.  Routing through
// the streambuf (rather than macro-replacing the qualified name std::cout,
// which is impossible) means the few Uno sites that use the C++ streams call
// UNO_COUT / UNO_CERR instead and never name std::cout / std::cerr.
class RStreamBuf : public std::streambuf {
public:
   explicit RStreamBuf(bool to_stderr) : to_stderr_(to_stderr) {}

protected:
   std::streamsize xsputn(const char* s, std::streamsize n) override {
      if (this->to_stderr_) {
         REprintf("%.*s", static_cast<int>(n), s);
      }
      else {
         Rprintf("%.*s", static_cast<int>(n), s);
      }
      return n;
   }
   int overflow(int c) override {
      if (c != EOF) {
         const char ch = static_cast<char>(c);
         if (this->to_stderr_) {
            REprintf("%c", ch);
         }
         else {
            Rprintf("%c", ch);
         }
      }
      return c;
   }

private:
   bool to_stderr_;
};

// Function-local statics give the streambuf and the ostream program lifetime
// (constructed on first use, never destroyed), so handing &rcout() to
// Logger::stream is safe for the whole run.
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

} // namespace r_io
} // namespace uno

#define UNO_COUT (::uno::r_io::rcout())
#define UNO_CERR (::uno::r_io::rcerr())

#else // !UNO_R_PRINT -- upstream build: byte-identical pass-through

#include <iostream>
#define UNO_COUT std::cout
#define UNO_CERR std::cerr

#endif // UNO_R_PRINT

#endif // __cplusplus

#endif // UNO_TOOLS_R_IO_H_
