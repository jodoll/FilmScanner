#include <unity.h>

void setUp()
{
}

void tearDown()
{
}

void test_native_environment_builds()
{
  TEST_ASSERT_TRUE(true);
}

int main(int, char**)
{
  UNITY_BEGIN();
  RUN_TEST(test_native_environment_builds);
  return UNITY_END();
}
