#pragma once

// Vector kernels for signal and display data, through VOLK (the Vector
// Optimized Library of Kernels) when the build found it
// (RESONANT_HAVE_VOLK), and plain loops otherwise. VOLK chooses the best
// SIMD implementation for the running CPU (SSE, AVX, AVX-512, NEON).
//
// VOLK's kernels are almost all single precision: they suit measured
// samples, display conversions and spectra, not the solvers, whose
// reference arithmetic is double precision (ADR-000212). The `scalar`
// namespace holds the plain implementations, used for validation.
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>

namespace resonant::compute {

bool volkAvailable();
std::string volkVersion();
std::string volkMachine();  // the kernel family VOLK selected for this CPU

void multiply(const float* a, const float* b, float* out, size_t n);           // out = a b
void add(const float* a, const float* b, float* out, size_t n);                // out = a + b
void scale(const float* in, float factor, float* out, size_t n);               // out = factor in
void squareRoot(const float* in, float* out, size_t n);
void magnitudeSquared(const std::complex<float>* in, float* out, size_t n);   // |in|^2
void magnitude(const std::complex<float>* in, float* out, size_t n);          // |in|
void multiplyConjugate(const std::complex<float>* a, const std::complex<float>* b, std::complex<float>* out, size_t n);  // a conj(b)
std::complex<float> dotProduct(const std::complex<float>* a, const std::complex<float>* b, size_t n);                    // sum a b
void int16ToFloat(const std::int16_t* in, float fullScale, float* out, size_t n);  // in / fullScale
void int32ToFloat(const std::int32_t* in, float fullScale, float* out, size_t n);
void floatToDouble(const float* in, double* out, size_t n);
void doubleToFloat(const double* in, float* out, size_t n);
void multiply(const double* a, const double* b, double* out, size_t n);

namespace scalar {
void multiply(const float* a, const float* b, float* out, size_t n);
void add(const float* a, const float* b, float* out, size_t n);
void scale(const float* in, float factor, float* out, size_t n);
void squareRoot(const float* in, float* out, size_t n);
void magnitudeSquared(const std::complex<float>* in, float* out, size_t n);
void magnitude(const std::complex<float>* in, float* out, size_t n);
void multiplyConjugate(const std::complex<float>* a, const std::complex<float>* b, std::complex<float>* out, size_t n);
std::complex<float> dotProduct(const std::complex<float>* a, const std::complex<float>* b, size_t n);
void int16ToFloat(const std::int16_t* in, float fullScale, float* out, size_t n);
void int32ToFloat(const std::int32_t* in, float fullScale, float* out, size_t n);
void floatToDouble(const float* in, double* out, size_t n);
void doubleToFloat(const double* in, float* out, size_t n);
void multiply(const double* a, const double* b, double* out, size_t n);
}  // namespace scalar

}  // namespace resonant::compute
