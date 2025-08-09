#ifndef RAYCAST_MACROS_INCLUDED
#define RAYCAST_MACROS_INCLUDED

#define M_MAX(x, y) (((x)>(y))?(x):(y))
#define M_MIN(x, y) (((x)<(y))?(x):(y))
#define M_CLAMP(N, L, U) M_MAX(M_MIN(U, N), L)
#define M_MOD(A, B) (((A) % (B) + (B)) % (B))
#define M_BIT(B) (1<<B)
#define M_UNUSED(x) (void)(x)
#define M_OPTIONAL(X) X
#define M_DISCARDABLE(X) X

#if defined(_MSC_VER)
// MSVC Compiler
#define M_INLINED static __forceinline
#define M_PACKED __pragma(pack(push, 1)) struct __pragma(pack(pop))
#elif defined(__GNUC__) || defined(__clang__)
// GCC or Clang
#define M_INLINED static inline __attribute__((always_inline))
#define M_PACKED __attribute__((__packed__))
#else
// Fallback for unknown compilers
#define M_INLINED static inline
#define M_PACKED
#endif

#ifdef RAYCASTER_DEBUG
  #define IF_DEBUG(S) S;
#else
  #define IF_DEBUG(S)
#endif

// https://stackoverflow.com/questions/2124339/c-preprocessor-va-args-number-of-arguments
// #define M_NARG(...) M__NARG_(_,##__VA_ARGS__,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
// #define M__NARG_(_,...) M__ARG_N(__VA_ARGS__)
// #define M__ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N

#define M_EXPAND(x) x
#define M_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9,_10,_11,_12,_13,_14,_15,_16, N, ...) N
#define M_NARG_(...) M_EXPAND(M_ARG_N(__VA_ARGS__))
#define M_NARG(...) M_NARG_(__VA_ARGS__, 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

#define M_ARRAY(TYPE, ...) M_NARG(__VA_ARGS__), (TYPE[]) { __VA_ARGS__ }

/* For creating some special floats that looks like NaNs but have some data encoded in the */
#define M_FLAGGED_NAN(F) ((union { uint32_t i; float f; }){ .i = 0x7FC00000 | ((F) & 0x007FFFFF) }).f
#define M_FLAGGED_NAN_CHECK(FLT, F) ((((union { float f; uint32_t i; }){ .f = (FLT) }).i & 0x7FFFFFFF) == (0x7FC00000 | ((F) & 0x007FFFFF)))

#endif
