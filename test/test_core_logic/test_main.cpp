#include <unity.h>

#include <cmath>

#include "core_logic.h"

void setUp()
{
}

void tearDown()
{
}

void test_tank_nominal_reading()
{
    const auto result =
        CoreLogic::calculateTankReading(54.0f, 100.0f, 12.0f);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 58.0f, result.waterLevelCm);
    TEST_ASSERT_EQUAL_INT(57, result.fillPercent);
}

void test_tank_clamps_full_and_empty()
{
    const auto full =
        CoreLogic::calculateTankReading(0.0f, 100.0f, 12.0f);
    const auto empty =
        CoreLogic::calculateTankReading(150.0f, 100.0f, 12.0f);
    TEST_ASSERT_TRUE(full.valid);
    TEST_ASSERT_EQUAL_INT(100, full.fillPercent);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, full.waterLevelCm);
    TEST_ASSERT_TRUE(empty.valid);
    TEST_ASSERT_EQUAL_INT(0, empty.fillPercent);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, empty.waterLevelCm);
}

void test_tank_rejects_invalid_inputs()
{
    TEST_ASSERT_FALSE(
        CoreLogic::calculateTankReading(-1.0f, 100.0f, 12.0f).valid
    );
    TEST_ASSERT_FALSE(
        CoreLogic::calculateTankReading(10.0f, 0.0f, 12.0f).valid
    );
    TEST_ASSERT_FALSE(
        CoreLogic::calculateTankReading(10.0f, 100.0f, -1.0f).valid
    );
    TEST_ASSERT_FALSE(
        CoreLogic::calculateTankReading(NAN, 100.0f, 12.0f).valid
    );
}

void test_battery_percentage_boundaries()
{
    TEST_ASSERT_EQUAL_INT(
        0,
        CoreLogic::calculateBatteryPercentage(3.0f, 3.2f, 4.2f)
    );
    TEST_ASSERT_EQUAL_INT(
        100,
        CoreLogic::calculateBatteryPercentage(4.3f, 3.2f, 4.2f)
    );
    TEST_ASSERT_EQUAL_INT(
        50,
        CoreLogic::calculateBatteryPercentage(3.7f, 3.2f, 4.2f)
    );
}

void test_battery_rejects_invalid_configuration()
{
    TEST_ASSERT_EQUAL_INT(
        -1,
        CoreLogic::calculateBatteryPercentage(3.7f, 4.2f, 4.2f)
    );
    TEST_ASSERT_EQUAL_INT(
        -1,
        CoreLogic::calculateBatteryPercentage(3.7f, 4.3f, 4.2f)
    );
    TEST_ASSERT_EQUAL_INT(
        -1,
        CoreLogic::calculateBatteryPercentage(NAN, 3.2f, 4.2f)
    );
}

void test_deep_sleep_decisions()
{
    using CoreLogic::RuntimeMode;
    TEST_ASSERT_TRUE(CoreLogic::shouldEnterDeepSleep(
        true, false, false, RuntimeMode::Running
    ));
    TEST_ASSERT_FALSE(CoreLogic::shouldEnterDeepSleep(
        false, false, false, RuntimeMode::Running
    ));
    TEST_ASSERT_TRUE(CoreLogic::shouldEnterDeepSleep(
        false, true, false, RuntimeMode::Running
    ));
    TEST_ASSERT_FALSE(CoreLogic::shouldEnterDeepSleep(
        true, false, true, RuntimeMode::Running
    ));
}

void test_sleep_preparation_blocks_normal_work()
{
    using CoreLogic::RuntimeMode;
    TEST_ASSERT_TRUE(CoreLogic::canStartNormalWork(RuntimeMode::Running));
    TEST_ASSERT_FALSE(
        CoreLogic::canStartNormalWork(RuntimeMode::PreparingSleep)
    );
    TEST_ASSERT_FALSE(CoreLogic::canStartNormalWork(RuntimeMode::Sleeping));
}

void test_awake_measurement_scheduler()
{
    TEST_ASSERT_FALSE(CoreLogic::isMeasurementDue(999UL, 0UL, 1000UL));
    TEST_ASSERT_TRUE(CoreLogic::isMeasurementDue(1000UL, 0UL, 1000UL));
    TEST_ASSERT_FALSE(CoreLogic::isMeasurementDue(1000UL, 0UL, 0UL));
    TEST_ASSERT_TRUE(CoreLogic::isMeasurementDue(
        20UL,
        0xFFFFFFF0UL,
        30UL
    ));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_tank_nominal_reading);
    RUN_TEST(test_tank_clamps_full_and_empty);
    RUN_TEST(test_tank_rejects_invalid_inputs);
    RUN_TEST(test_battery_percentage_boundaries);
    RUN_TEST(test_battery_rejects_invalid_configuration);
    RUN_TEST(test_deep_sleep_decisions);
    RUN_TEST(test_sleep_preparation_blocks_normal_work);
    RUN_TEST(test_awake_measurement_scheduler);
    return UNITY_END();
}
