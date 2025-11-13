#include <gtest/gtest.h>
#include "../src/wta/core/types.hpp"

class TypesTest : public ::testing::Test {
protected:
    using namespace wta::types;
};

TEST_F(TypesTest, Vec2Construction) {
    wta::types::Vec2 v{1.0f, 2.0f};
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
}

TEST_F(TypesTest, Vec2DefaultConstruction) {
    wta::types::Vec2 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
}

TEST_F(TypesTest, Vec3Construction) {
    wta::types::Vec3 v{1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST_F(TypesTest, PlatformStateConstruction) {
    wta::types::PlatformState p;
    EXPECT_EQ(p.id, 0);
    EXPECT_EQ(p.role, wta::types::PlatformRole::Unknown);
    EXPECT_FLOAT_EQ(p.hit_prob, 0.0f);
    EXPECT_TRUE(p.alive);
}

TEST_F(TypesTest, TargetStateConstruction) {
    wta::types::TargetState t;
    EXPECT_EQ(t.id, 0);
    EXPECT_EQ(t.kind, wta::types::TargetKind::Other);
    EXPECT_EQ(t.tier, 0);
    EXPECT_FLOAT_EQ(t.value, 0.0f);
    EXPECT_TRUE(t.alive);
}

TEST_F(TypesTest, AmmoStateConstruction) {
    wta::types::AmmoState a;
    EXPECT_EQ(a.missile, 0);
    EXPECT_EQ(a.bomb, 0);
    EXPECT_EQ(a.rocket, 0);
}

TEST_F(TypesTest, IdxRowMajor) {
    size_t n_targets = 5;
    EXPECT_EQ(wta::types::idx_row_major(0, 0, n_targets), 0);
    EXPECT_EQ(wta::types::idx_row_major(0, 4, n_targets), 4);
    EXPECT_EQ(wta::types::idx_row_major(1, 0, n_targets), 5);
    EXPECT_EQ(wta::types::idx_row_major(2, 3, n_targets), 13);
}
