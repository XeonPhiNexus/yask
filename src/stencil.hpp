/*****************************************************************************

YASK: Yet Another Stencil Kernel
Copyright (c) 2014-2017, Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*****************************************************************************/

// This file defines functions, types, and macros needed for the stencil
// kernel.

#ifndef STENCIL_HPP
#define STENCIL_HPP

// Control assert() by turning on with DEBUG instead of turning off with
// NDEBUG. This makes it off by default.
#ifndef DEBUG
#define NDEBUG
#endif

#include <cstdint>
#include <iostream>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <functional>

#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32
#include <unistd.h>
#include <stdint.h>
#include <immintrin.h>
#endif

// safe integer divide and mod.
#include "idiv.hpp"

// macros for 1D<->nD transforms.
#include "layout_macros.hpp"

// Auto-generated macros from foldBuilder.
// It's important that this be included before realv.hpp
// to properly set the vector lengths.
#define DEFINE_MACROS
#include "stencil_code.hpp"
#undef DEFINE_MACROS

// Settings from makefile.
#include "stencil_macros.hpp"

// Define a folded vector of reals.
#include "realv.hpp"

// Other utilities.
#include "utils.hpp"

// macro for debug message.
#ifdef TRACE
#define TRACE_MSG0(os, msg) ((os) << "YASK: " << msg << std::endl << std::flush)
#else
#define TRACE_MSG0(os, msg) ((void)0)
#endif

// macro for debug message from a StencilContext method.
#define TRACE_MSG1(msg) TRACE_MSG0(get_ostr(), msg)
#define TRACE_MSG(msg) TRACE_MSG1(msg)
 
// macro for debug message when _context ptr is defined.
#define TRACE_MSG2(msg) TRACE_MSG0(_context->get_ostr(), msg)
 
// Macro for automatically adding W dimension's arg.
// TODO: make all args programmatic.
#if USING_DIM_W
#define ARG_W(w) w,
#else
#define ARG_W(w)
#endif

// Cluster sizes in vectors.
// This are defaults for those not defined by the stencil compiler.
#ifndef CLEN_T
#define CLEN_T (1)
#endif
#ifndef CLEN_W
#define CLEN_W (1)
#endif
#ifndef CLEN_X
#define CLEN_X (1)
#endif
#ifndef CLEN_Y
#define CLEN_Y (1)
#endif
#ifndef CLEN_Z
#define CLEN_Z (1)
#endif

// Cluster sizes in points.
// This is the number of scalar results calculated by each
// call to the calc_cluster function(s) generated by foldBuilder
// in stencil_code.hpp.
#define CPTS_T (CLEN_T * VLEN_T)
#define CPTS_W (CLEN_W * VLEN_W)
#define CPTS_X (CLEN_X * VLEN_X)
#define CPTS_Y (CLEN_Y * VLEN_Y)
#define CPTS_Z (CLEN_Z * VLEN_Z)
#define CPTS (CLEN_T * CPTS_W * CPTS_X * CPTS_Y * CPTS_Z)

// Default alignment and padding.
#define CACHELINE_BYTES  (64)
#define YASK_PAD (17) // cache-lines between data buffers.
#define YASK_ALIGNMENT (2 * 1024 * 1024) // 2MiB-page

// L1 and L2 hints
#define L1 _MM_HINT_T0
#define L2 _MM_HINT_T1

////// Default prefetch distances.
// These are only used if and when prefetch code is generated by
// gen-loops.pl.

// How far to prefetch ahead for L1.
#ifndef PFDL1
 #define PFDL1 1
#endif

// How far to prefetch ahead for L2.
#ifndef PFDL2
 #define PFDL2 2
#endif

// Set MODEL_CACHE to 1 or 2 to model L1 or L2.
#ifdef MODEL_CACHE
#include "cache_model.hpp"
extern yask::Cache cache_model;
 #if MODEL_CACHE==L1
  #warning Modeling L1 cache
 #elif MODEL_CACHE==L2
  #warning Modeling L2 cache
 #else
  #warning Modeling UNKNOWN cache
 #endif
#endif

// Memory-accessing code.
namespace yask {
#include "layouts.hpp"
}
#include "generic_grids.hpp"
#include "realv_grids.hpp"

// First/last index macros.
// These are relative to global problem, not rank.
#define FIRST_INDEX(dim) (0)
#define LAST_INDEX(dim) (_context->tot_ ## dim - 1)

namespace yask {

    // Default grid layouts.
    // Last number in 'Layout' name has unit stride, e.g.,
    // LAYOUT_WXYZ Layout_1234 => unit-stride in z.
    // LAYOUT_WXYZ Layout_1243 => unit-stride in y.
#ifndef LAYOUT_XYZ
#define LAYOUT_XYZ Layout_123
#endif
#ifndef LAYOUT_WXYZ
#define LAYOUT_WXYZ Layout_1234
#endif
#ifndef LAYOUT_TXYZ
#define LAYOUT_TXYZ Layout_1234
#endif
#ifndef LAYOUT_TWXYZ
#define LAYOUT_TWXYZ Layout_12345
#endif

    // RealVecGrids using layouts defined above.
    using Grid_XYZ = RealVecGrid_XYZ<LAYOUT_XYZ>;
    using Grid_WXYZ = RealVecGrid_WXYZ<LAYOUT_WXYZ>;
    template <idx_t tdim>
    using Grid_TXYZ = RealVecGrid_TXYZ<LAYOUT_TXYZ, tdim>;
    template <idx_t tdim>
    using Grid_TWXYZ = RealVecGrid_TWXYZ<LAYOUT_TWXYZ, tdim>;

    // RealGrids using traditional C layout.
    typedef GenericGrid3d<real_t, LAYOUT_XYZ> RealGrid_XYZ;
    typedef GenericGrid4d<real_t, LAYOUT_WXYZ> RealGrid_WXYZ;

} // namespace yask.

// Base types for stencil context, etc.
#include "stencil_calc.hpp"

// Auto-generated stencil code that extends base types.
#define DEFINE_CONTEXT
#include "stencil_code.hpp"
#undef DEFINE_CONTEXT

#endif
