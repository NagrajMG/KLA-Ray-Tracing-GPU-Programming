#pragma once

#include <cfloat>
#include <cmath>
#include <type_traits>

#ifdef __CUDACC__
#define HD __host__ __device__
#else
#define HD
#endif

#if defined(RT_FP16_COLORS) || defined(RT_FP16_GEOMETRY)
#include <cuda_fp16.h>

// Explicit operators keep basic arithmetic in FP16 on the GPU. Host-side scene
// construction uses FP32 arithmetic followed by round-to-nearest FP16 conversion.
struct Half {
    __half value;

    Half() = default;
    HD Half(float x) : value(__float2half_rn(x)) {}
    HD operator float() const { return __half2float(value); }

    HD static Half from_native(__half x) {
        Half result;
        result.value = x;
        return result;
    }
};

static_assert(sizeof(Half) == 2, "FP16 storage must occupy two bytes");
static_assert(std::is_trivially_copyable<Half>::value, "FP16 must support cudaMemcpy");

HD Half operator+(Half a, Half b) {
#ifdef __CUDA_ARCH__
    return Half::from_native(__hadd(a.value, b.value));
#else
    return Half(float(a) + float(b));
#endif
}

HD Half operator-(Half a, Half b) {
#ifdef __CUDA_ARCH__
    return Half::from_native(__hsub(a.value, b.value));
#else
    return Half(float(a) - float(b));
#endif
}

HD Half operator*(Half a, Half b) {
#ifdef __CUDA_ARCH__
    return Half::from_native(__hmul(a.value, b.value));
#else
    return Half(float(a) * float(b));
#endif
}

HD Half operator/(Half a, Half b) {
#ifdef __CUDA_ARCH__
    return Half::from_native(__hdiv(a.value, b.value));
#else
    return Half(float(a) / float(b));
#endif
}

HD Half operator-(Half a) { return Half(-float(a)); }

// Without these overloads, a literal such as 2.0f would promote the calculation
// to FP32 (or make overload resolution ambiguous).
#define HALF_MIXED_OPERATOR(op)                                                        \
    template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value>>     \
    HD Half operator op(Half a, T b) { return a op Half(float(b)); }                    \
    template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value>>     \
    HD Half operator op(T a, Half b) { return Half(float(a)) op b; }

HALF_MIXED_OPERATOR(+)
HALF_MIXED_OPERATOR(-)
HALF_MIXED_OPERATOR(*)
HALF_MIXED_OPERATOR(/)
#undef HALF_MIXED_OPERATOR

HD Half &operator+=(Half &a, Half b) { a = a + b; return a; }
HD Half &operator*=(Half &a, Half b) { a = a * b; return a; }
#endif

#ifdef RT_FP16_GEOMETRY
using Real = Half;
constexpr float RealEpsilon = 0.0009765625f;
constexpr float RealMax = 65504.0f;
#else
using Real = float;
constexpr float RealEpsilon = FLT_EPSILON;
constexpr float RealMax = FLT_MAX;
#endif

#ifdef RT_FP16_COLORS
using ColorScalar = Half;
#else
using ColorScalar = float;
#endif

// CUDA has no native FP16 counterparts for every function used by the quartic
// solver. Evaluate these functions in FP32 and round their results to Real.
HD Real real_sqrt(Real x) { return Real(sqrtf(float(x))); }
HD Real real_cbrt(Real x) { return Real(cbrtf(float(x))); }
HD Real real_cos(Real x) { return Real(cosf(float(x))); }
HD Real real_acos(Real x) { return Real(acosf(float(x))); }
HD Real real_abs(Real x) { return Real(fabsf(float(x))); }
HD Real real_min(Real a, Real b) { return Real(fminf(float(a), float(b))); }
HD Real real_max(Real a, Real b) { return Real(fmaxf(float(a), float(b))); }
HD Real real_copysign(Real a, Real b) { return Real(copysignf(float(a), float(b))); }
