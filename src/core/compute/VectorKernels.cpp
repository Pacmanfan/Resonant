#include "compute/VectorKernels.h"

#include <algorithm>
#include <climits>
#include <cmath>

#ifdef RESONANT_HAVE_VOLK
#include <volk/volk.h>
#include <volk/volk_version.h>
#endif

namespace resonant::compute {

namespace scalar {
void multiply(const float* a, const float* b, float* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = a[i] * b[i]; }
void add(const float* a, const float* b, float* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = a[i] + b[i]; }
void scale(const float* in, float f, float* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = f * in[i]; }
void squareRoot(const float* in, float* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = std::sqrt(in[i]); }
void magnitudeSquared(const std::complex<float>* in, float* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = std::norm(in[i]); }
void magnitude(const std::complex<float>* in, float* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = std::sqrt(std::norm(in[i])); }
void multiplyConjugate(const std::complex<float>* a, const std::complex<float>* b, std::complex<float>* out, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        out[i] = a[i] * std::conj(b[i]);
}
std::complex<float> dotProduct(const std::complex<float>* a, const std::complex<float>* b, size_t n)
{
    std::complex<double> sum = 0.0;
    for (size_t i = 0; i < n; ++i)
        sum += std::complex<double>(a[i]) * std::complex<double>(b[i]);
    return std::complex<float>(sum);
}
void int16ToFloat(const std::int16_t* in, float s, float* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = static_cast<float>(in[i]) / s; }
void int32ToFloat(const std::int32_t* in, float s, float* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = static_cast<float>(in[i]) / s; }
void floatToDouble(const float* in, double* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = in[i]; }
void doubleToFloat(const double* in, float* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = static_cast<float>(in[i]); }
void multiply(const double* a, const double* b, double* out, size_t n) { for (size_t i = 0; i < n; ++i) out[i] = a[i] * b[i]; }
}  // namespace scalar

#ifdef RESONANT_HAVE_VOLK

namespace {
// VOLK counts in unsigned int: longer arrays go in chunks.
template <typename F>
void chunked(size_t n, F&& f)
{
    constexpr size_t chunk = static_cast<size_t>(UINT_MAX / 2);
    for (size_t at = 0; at < n; at += chunk)
        f(at, static_cast<unsigned int>(std::min(chunk, n - at)));
}
}  // namespace

bool volkAvailable() { return true; }
std::string volkVersion()
{
    return std::to_string(VOLK_VERSION_MAJOR) + "." + std::to_string(VOLK_VERSION_MINOR) + "." + std::to_string(VOLK_VERSION_MAINT);
}
std::string volkMachine() { return volk_get_machine(); }

void multiply(const float* a, const float* b, float* out, size_t n) { chunked(n, [&](size_t o, unsigned m) { volk_32f_x2_multiply_32f(out + o, a + o, b + o, m); }); }
void add(const float* a, const float* b, float* out, size_t n) { chunked(n, [&](size_t o, unsigned m) { volk_32f_x2_add_32f(out + o, a + o, b + o, m); }); }
void scale(const float* in, float f, float* out, size_t n) { chunked(n, [&](size_t o, unsigned m) { volk_32f_s32f_multiply_32f(out + o, in + o, f, m); }); }
void squareRoot(const float* in, float* out, size_t n) { chunked(n, [&](size_t o, unsigned m) { volk_32f_sqrt_32f(out + o, in + o, m); }); }
void magnitudeSquared(const std::complex<float>* in, float* out, size_t n)
{
    chunked(n, [&](size_t o, unsigned m) { volk_32fc_magnitude_squared_32f(out + o, in + o, m); });
}
void magnitude(const std::complex<float>* in, float* out, size_t n)
{
    chunked(n, [&](size_t o, unsigned m) { volk_32fc_magnitude_32f(out + o, in + o, m); });
}
void multiplyConjugate(const std::complex<float>* a, const std::complex<float>* b, std::complex<float>* out, size_t n)
{
    chunked(n, [&](size_t o, unsigned m) { volk_32fc_x2_multiply_conjugate_32fc(out + o, a + o, b + o, m); });
}
std::complex<float> dotProduct(const std::complex<float>* a, const std::complex<float>* b, size_t n)
{
    std::complex<float> total = 0.0f;
    chunked(n, [&](size_t o, unsigned m) {
        std::complex<float> part = 0.0f;
        volk_32fc_x2_dot_prod_32fc(&part, a + o, b + o, m);
        total += part;
    });
    return total;
}
void int16ToFloat(const std::int16_t* in, float s, float* out, size_t n) { chunked(n, [&](size_t o, unsigned m) { volk_16i_s32f_convert_32f(out + o, in + o, s, m); }); }
void int32ToFloat(const std::int32_t* in, float s, float* out, size_t n) { chunked(n, [&](size_t o, unsigned m) { volk_32i_s32f_convert_32f(out + o, in + o, s, m); }); }
void floatToDouble(const float* in, double* out, size_t n) { chunked(n, [&](size_t o, unsigned m) { volk_32f_convert_64f(out + o, in + o, m); }); }
void doubleToFloat(const double* in, float* out, size_t n) { chunked(n, [&](size_t o, unsigned m) { volk_64f_convert_32f(out + o, in + o, m); }); }
void multiply(const double* a, const double* b, double* out, size_t n) { chunked(n, [&](size_t o, unsigned m) { volk_64f_x2_multiply_64f(out + o, a + o, b + o, m); }); }

#else

bool volkAvailable() { return false; }
std::string volkVersion() { return {}; }
std::string volkMachine() { return "none (built without VOLK)"; }
void multiply(const float* a, const float* b, float* out, size_t n) { scalar::multiply(a, b, out, n); }
void add(const float* a, const float* b, float* out, size_t n) { scalar::add(a, b, out, n); }
void scale(const float* in, float f, float* out, size_t n) { scalar::scale(in, f, out, n); }
void squareRoot(const float* in, float* out, size_t n) { scalar::squareRoot(in, out, n); }
void magnitudeSquared(const std::complex<float>* in, float* out, size_t n) { scalar::magnitudeSquared(in, out, n); }
void magnitude(const std::complex<float>* in, float* out, size_t n) { scalar::magnitude(in, out, n); }
void multiplyConjugate(const std::complex<float>* a, const std::complex<float>* b, std::complex<float>* out, size_t n) { scalar::multiplyConjugate(a, b, out, n); }
std::complex<float> dotProduct(const std::complex<float>* a, const std::complex<float>* b, size_t n) { return scalar::dotProduct(a, b, n); }
void int16ToFloat(const std::int16_t* in, float s, float* out, size_t n) { scalar::int16ToFloat(in, s, out, n); }
void int32ToFloat(const std::int32_t* in, float s, float* out, size_t n) { scalar::int32ToFloat(in, s, out, n); }
void floatToDouble(const float* in, double* out, size_t n) { scalar::floatToDouble(in, out, n); }
void doubleToFloat(const double* in, float* out, size_t n) { scalar::doubleToFloat(in, out, n); }
void multiply(const double* a, const double* b, double* out, size_t n) { scalar::multiply(a, b, out, n); }

#endif

}  // namespace resonant::compute
