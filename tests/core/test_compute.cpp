// Compute backends: the thread pool, VOLK kernels against their plain
// implementations, capability detection and backend checks.
#include "compute/Compute.h"
#include "compute/ThreadPool.h"
#include "compute/VectorKernels.h"
#include "electromagnetics/EmStudy.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <cmath>
#include <numeric>
#include <random>

using namespace resonant;
using namespace resonant::compute;

TEST(ComputeThreads, PoolCoversEveryIndexOnceWithAFixedPartition)
{
    for (const int threads : {1, 2, 3, 8}) {
        ThreadPool pool(threads);
        EXPECT_EQ(pool.threads(), threads);
        for (const size_t count : {size_t(0), size_t(1), size_t(5), size_t(1000), size_t(12345)}) {
            std::vector<std::atomic<int>> hits(count);
            std::vector<std::pair<size_t, size_t>> ranges;
            std::mutex m;
            pool.run(count, [&](size_t b, size_t e) {
                for (size_t i = b; i < e; ++i)
                    hits[i]++;
                const std::lock_guard lock(m);
                ranges.emplace_back(b, e);
            });
            for (size_t i = 0; i < count; ++i)
                ASSERT_EQ(hits[i].load(), 1) << threads << " threads, count " << count << ", index " << i;
            std::sort(ranges.begin(), ranges.end());
            if (count > 0) {
                EXPECT_EQ(ranges.front().first, 0u);
                EXPECT_EQ(ranges.back().second, count);
                for (size_t r = 1; r < ranges.size(); ++r)
                    EXPECT_EQ(ranges[r].first, ranges[r - 1].second);  // contiguous, no overlap
            }
        }
        // Reused many times without deadlock.
        std::atomic<long> sum = 0;
        for (int r = 0; r < 2000; ++r)
            pool.run(64, [&](size_t b, size_t e) { sum += static_cast<long>(e - b); });
        EXPECT_EQ(sum.load(), 2000L * 64);
    }
    EXPECT_GE(hardwareThreads(), 1);
    EXPECT_EQ(resolveThreads(0), hardwareThreads());
    EXPECT_EQ(resolveThreads(1), 1);
    EXPECT_EQ(resolveThreads(100000), hardwareThreads());
}

TEST(ComputeKernels, VectorKernelsMatchThePlainLoops)
{
    std::mt19937 random(7);
    std::uniform_real_distribution<float> u(-2.0f, 2.0f);
    const size_t n = 1003;  // not a multiple of any SIMD width
    std::vector<float> a(n), b(n), x(n), y(n);
    std::vector<std::complex<float>> ca(n), cb(n), cx(n), cy(n);
    std::vector<std::int16_t> s16(n);
    std::vector<std::int32_t> s32(n);
    std::vector<double> da(n), db(n), dx(n), dy(n);
    for (size_t i = 0; i < n; ++i) {
        a[i] = u(random);
        b[i] = std::abs(u(random)) + 0.1f;
        ca[i] = {u(random), u(random)};
        cb[i] = {u(random), u(random)};
        s16[i] = static_cast<std::int16_t>(u(random) * 16000.0f);
        s32[i] = static_cast<std::int32_t>(u(random) * 1.0e9f);
        da[i] = u(random);
        db[i] = u(random);
    }
    const auto same = [&](const std::vector<float>& p, const std::vector<float>& q, float tolerance) {
        for (size_t i = 0; i < n; ++i)
            ASSERT_NEAR(p[i], q[i], tolerance * std::max(1.0f, std::abs(q[i]))) << i;
    };
    multiply(a.data(), b.data(), x.data(), n);
    scalar::multiply(a.data(), b.data(), y.data(), n);
    same(x, y, 1e-6f);
    add(a.data(), b.data(), x.data(), n);
    scalar::add(a.data(), b.data(), y.data(), n);
    same(x, y, 1e-6f);
    scale(a.data(), 3.5f, x.data(), n);
    scalar::scale(a.data(), 3.5f, y.data(), n);
    same(x, y, 1e-6f);
    squareRoot(b.data(), x.data(), n);
    scalar::squareRoot(b.data(), y.data(), n);
    same(x, y, 1e-6f);
    magnitudeSquared(ca.data(), x.data(), n);
    scalar::magnitudeSquared(ca.data(), y.data(), n);
    same(x, y, 1e-6f);
    magnitude(ca.data(), x.data(), n);
    scalar::magnitude(ca.data(), y.data(), n);
    same(x, y, 1e-5f);
    multiplyConjugate(ca.data(), cb.data(), cx.data(), n);
    scalar::multiplyConjugate(ca.data(), cb.data(), cy.data(), n);
    for (size_t i = 0; i < n; ++i)
        ASSERT_NEAR(std::abs(cx[i] - cy[i]), 0.0f, 1e-5f) << i;
    const std::complex<float> dot = dotProduct(ca.data(), cb.data(), n), reference = scalar::dotProduct(ca.data(), cb.data(), n);
    EXPECT_NEAR(std::abs(dot - reference), 0.0f, 1e-3f * std::sqrt(static_cast<float>(n)));
    int16ToFloat(s16.data(), 32768.0f, x.data(), n);
    scalar::int16ToFloat(s16.data(), 32768.0f, y.data(), n);
    same(x, y, 1e-7f);
    int32ToFloat(s32.data(), 2147483648.0f, x.data(), n);
    scalar::int32ToFloat(s32.data(), 2147483648.0f, y.data(), n);
    same(x, y, 1e-6f);
    multiply(da.data(), db.data(), dx.data(), n);
    scalar::multiply(da.data(), db.data(), dy.data(), n);
    for (size_t i = 0; i < n; ++i)
        ASSERT_DOUBLE_EQ(dx[i], dy[i]);
    doubleToFloat(da.data(), x.data(), n);
    scalar::doubleToFloat(da.data(), y.data(), n);
    same(x, y, 0.0f);
    floatToDouble(x.data(), dx.data(), n);
    for (size_t i = 0; i < n; ++i)
        ASSERT_EQ(dx[i], static_cast<double>(x[i]));
    // The build reports what it has.
#ifdef RESONANT_HAVE_VOLK
    EXPECT_TRUE(volkAvailable());
    EXPECT_FALSE(volkMachine().empty());
#else
    EXPECT_FALSE(volkAvailable());
#endif
}

TEST(ComputeBackends, OptionsCapabilitiesAndChecks)
{
    ComputeOptions o;
    EXPECT_EQ(o.backend, ComputeBackend::Cpu);
    o.backend = ComputeBackend::Cuda;
    o.threads = 4;
    auto back = computeOptionsFromJson(toJson(o));
    ASSERT_TRUE(back.ok()) << back.message();
    EXPECT_EQ(back.value(), o);
    EXPECT_FALSE(computeOptionsFromJson(nlohmann::json{{"backend", "opencl"}}).ok());
    EXPECT_FALSE(computeOptionsFromJson(nlohmann::json{{"threads", -1}}).ok());

    const ComputeCapabilities c = detectCompute();
    EXPECT_EQ(c.hardwareThreads, hardwareThreads());
    EXPECT_FALSE(c.summary().empty());
    std::printf("[ compute ] %s\n", c.summary().c_str());
    // The CPU backend always runs; CUDA runs only where a usable device has room.
    EXPECT_TRUE(checkBackend(ComputeOptions{}, size_t(1) << 40).ok());
    const Status cuda = checkBackend(o, 1024);
#ifndef RESONANT_HAVE_CUDA
    EXPECT_FALSE(cuda.ok());
    EXPECT_NE(cuda.message().find("no CUDA support"), std::string::npos);
#else
    bool usable = false;
    for (const CudaDeviceInfo& d : c.cudaDevices)
        usable = usable || (d.index == 0 && d.usable && d.freeBytes > (128u << 20));
    EXPECT_EQ(cuda.ok(), usable) << cuda.message();
    // Far more than any GPU has is refused with the numbers.
    if (usable) {
        EXPECT_NE(checkBackend(o, size_t(1) << 50).message().find("MiB free"), std::string::npos);
    }
#endif
}

TEST(ComputeKernels, FieldMagnitudeArrayMatchesThePerSampleValue)
{
    em::EmFieldResult r;
    r.grid.nu = 37;
    r.grid.nv = 11;
    std::mt19937 random(3);
    std::uniform_real_distribution<float> u(-5.0f, 5.0f);
    for (int c = 0; c < 3; ++c) {
        r.e[static_cast<size_t>(c)].resize(r.grid.count());
        r.h[static_cast<size_t>(c)].resize(r.grid.count());
        for (size_t i = 0; i < r.grid.count(); ++i) {
            r.e[static_cast<size_t>(c)][i] = {u(random), u(random)};
            r.h[static_cast<size_t>(c)][i] = {u(random), u(random)};
        }
    }
    for (const em::FieldComponent c : {em::FieldComponent::EMagnitude, em::FieldComponent::HMagnitude, em::FieldComponent::Ey, em::FieldComponent::PowerDensity}) {
        std::vector<std::complex<float>> all;
        r.componentArray(c, all);
        ASSERT_EQ(all.size(), r.grid.count());
        for (size_t i = 0; i < all.size(); ++i)
            ASSERT_NEAR(std::abs(all[i] - r.component(c, i)), 0.0f, 2e-6f * std::max(1.0f, std::abs(r.component(c, i)))) << static_cast<int>(c) << " " << i;
    }
}
