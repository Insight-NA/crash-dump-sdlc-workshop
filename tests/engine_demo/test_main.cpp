// Shared GoogleTest entry point.
//
// We deliberately provide our own main() instead of linking GTest::gtest_main.
// vcpkg's *shared* gtest_main.dll embeds a private copy of the test-registration
// machinery while importing the testing::UnitTest singleton from gtest.dll. That
// splits the registry: static TEST() registrars land in one UnitTest instance and
// RUN_ALL_TESTS() reads another, so every run reports "0 tests" (a vacuous pass).
// Linking only GTest::gtest and owning main() keeps registration and execution on
// the same registry, independent of static vs. shared GTest linkage.
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
