#include "Version.hpp"

#include <gtest/gtest.h>

TEST(Version, IsNonEmpty) { EXPECT_FALSE(listless::version().empty()); }
