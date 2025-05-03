#include "Greeter.h"
#include "gtest/gtest.h"

TEST(GreeterTest, hello_world)
{
    auto greeter = speaker::Greeter();
    // Expect two strings not to be equal.
    EXPECT_STRNE(greeter.SayHello().c_str(), "Hello World!");
}