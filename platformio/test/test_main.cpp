#include <Arduino.h>
#include <unity.h>
#include <RndisInterface.h>

// This function is called before each test
void setUp(void) {
    // No setup needed for these tests
}

// This function is called after each test
void tearDown(void) {
    // No teardown needed for these tests
}

// Test that the MAC address is configured with the specific, expected value.
// A correct MAC address is crucial for network functionality.
void test_mac_address_is_correct() {
    uint8_t expected_mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_mac, tud_network_mac_address, 6);
}

// Smoke test to ensure the main setup function can be called without crashing.
// This provides a basic check that the initialization logic is syntactically correct.
void test_setup_function_runs() {
    rndis_setup();
}

// Smoke test to ensure the main loop function can be called without crashing.
// This provides a basic check that the background task processing is syntactically correct.
void test_loop_function_runs() {
    rndis_loop();
}

void setup() {
    // Delay to allow the test runner to connect
    delay(2000);

    UNITY_BEGIN();

    // Run the on-device unit tests
    // NOTE: These tests verify on-device logic only. Testing host-side device
    // recognition requires manual testing with a physical device, as outlined
    // in TESTING.md.
    RUN_TEST(test_mac_address_is_correct);
    RUN_TEST(test_setup_function_runs);
    RUN_TEST(test_loop_function_runs);

    UNITY_END();
}

void loop() {
    // The tests run only once in setup()
}
