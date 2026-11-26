#include "foundation/Id.h"
#include "foundation/Log.h"
#include "foundation/Transform.h"
#include "foundation/Units.h"

#include <gtest/gtest.h>

#include <set>

using namespace resonant;

TEST(Id, GeneratedIdsAreUniqueAndWellFormed)
{
    std::set<std::string> seen;
    for (int i = 0; i < 2000; ++i) {
        const Id id = Id::generate();
        ASSERT_FALSE(id.isNull());
        ASSERT_EQ(id.str().size(), 36u);
        ASSERT_EQ(id.str()[14], '4');  // UUID version 4
        ASSERT_TRUE(seen.insert(id.str()).second);
    }
}

TEST(Id, DefaultIdIsNullAndComparesByValue)
{
    EXPECT_TRUE(Id().isNull());
    EXPECT_EQ(Id("a"), Id("a"));
    EXPECT_NE(Id("a"), Id("b"));
}

TEST(Units, ConversionsRoundTripAndUseSi)
{
    using namespace units;
    EXPECT_DOUBLE_EQ(toSI(25.4, LengthUnit::Millimeter), 0.0254);
    EXPECT_DOUBLE_EQ(toSI(1.0, LengthUnit::Inch), 0.0254);
    EXPECT_DOUBLE_EQ(fromSI(toSI(3.7, LengthUnit::Centimeter), LengthUnit::Centimeter), 3.7);
    EXPECT_DOUBLE_EQ(toSI(40.0, FrequencyUnit::Kilohertz), 40000.0);
    EXPECT_DOUBLE_EQ(toSI(250.0, TimeUnit::Microsecond), 250e-6);
    EXPECT_NEAR(degreesToRadians(180.0), pi, 1e-15);
    EXPECT_NEAR(radiansToDegrees(pi / 2.0), 90.0, 1e-12);
}

TEST(Transform, IdentityByDefault)
{
    EXPECT_TRUE(Transform().matrix().isApprox(Mat4::Identity()));
}

TEST(Transform, AppliesScaleThenRotationThenTranslation)
{
    Transform t;
    t.position = Vec3(10.0, 0.0, 0.0);
    t.rotation = Vec3(0.0, 0.0, units::pi / 2.0);  // yaw 90 degrees about Z
    t.scale = Vec3(2.0, 1.0, 1.0);
    // (1,0,0) -> scaled (2,0,0) -> yawed (0,2,0) -> translated (10,2,0)
    EXPECT_TRUE(transformPoint(t.matrix(), Vec3(1.0, 0.0, 0.0)).isApprox(Vec3(10.0, 2.0, 0.0), 1e-12));
    // Directions ignore translation.
    EXPECT_TRUE(transformDirection(t.matrix(), Vec3(1.0, 0.0, 0.0)).isApprox(Vec3(0.0, 2.0, 0.0), 1e-12));
}

TEST(Transform, EulerOrderIsYawPitchRollAboutFixedAxes)
{
    // R = Rz(yaw) * Ry(pitch) * Rx(roll): roll is applied first.
    Transform t;
    t.rotation = Vec3(units::pi / 2.0, 0.0, units::pi / 2.0);
    // Y axis: rolled 90 deg about X -> +Z; yaw about Z leaves Z alone.
    EXPECT_TRUE((t.rotationMatrix() * Vec3::UnitY()).isApprox(Vec3::UnitZ(), 1e-12));
    // X axis: unaffected by roll; yawed 90 deg about Z -> +Y.
    EXPECT_TRUE((t.rotationMatrix() * Vec3::UnitX()).isApprox(Vec3::UnitY(), 1e-12));
    // Pitch about Y by +90 deg takes +Z to +X (right-handed).
    Transform pitch;
    pitch.rotation = Vec3(0.0, units::pi / 2.0, 0.0);
    EXPECT_TRUE((pitch.rotationMatrix() * Vec3::UnitZ()).isApprox(Vec3::UnitX(), 1e-12));
}

TEST(Log, SinksReceiveEntriesAndCanBeRemoved)
{
    Logger logger;
    std::vector<LogEntry> received;
    const int handle = logger.addSink([&](const LogEntry& entry) { received.push_back(entry); });
    logger.log(LogLevel::Warning, "test", "first");
    logger.removeSink(handle);
    logger.log(LogLevel::Error, "test", "second");

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].level, LogLevel::Warning);
    EXPECT_EQ(received[0].category, "test");
    EXPECT_EQ(received[0].message, "first");
    EXPECT_EQ(logger.history().size(), 2u);
    EXPECT_NE(formatLogEntry(received[0]).find("[warning] test: first"), std::string::npos);
}
