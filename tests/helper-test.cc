
#include <gtest/gtest.h>

extern "C" {
#include "../helpers.h"
}

class HelperTest : public ::testing::Test
{
	private:

	protected:
		virtual void SetUp() {
			return;
		}
};

TEST_F(HelperTest, AppIdTest)
{
	ASSERT_TRUE(app_id_to_triplet("com.ubuntu.test_test_123", NULL, NULL, NULL));
	return;
}
