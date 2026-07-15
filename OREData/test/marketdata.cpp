#include <boost/test/unit_test.hpp>
#include <chrono>
#include <ored/utilities/marketdata.hpp>

using namespace ore::data;

BOOST_AUTO_TEST_SUITE(NormaliseDeliveryCodeTests)

BOOST_AUTO_TEST_CASE(testValidTwoDigitYear) {
    // Test standard format: single letter month code + 2-digit year
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("F24"), "2024-01");
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("H23"), "2023-03");
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("M25"), "2025-06");
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("Z26"), "2026-12");
}

BOOST_AUTO_TEST_CASE(testValidSingleDigitYear) {
    // Test single digit year - should use current decade
    auto today = std::chrono::year_month_day(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
    int currentYear = static_cast<int>(today.year());
    int currentYearDecade = (currentYear / 10) * 10;

    std::string expectedYear5 = std::to_string(currentYearDecade + 5);
    std::string expectedYear9 = std::to_string(currentYearDecade + 9);

    BOOST_CHECK_EQUAL(normaliseDeliveryCode("F5"), expectedYear5 + "-02");
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("Z9"), expectedYear9 + "-12");
}

BOOST_AUTO_TEST_CASE(testValidFourDigitYear) {
    // Test 4-digit year format
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("F2024"), "2024-02");
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("M2025"), "2025-06");
}

BOOST_AUTO_TEST_CASE(testAllMonthCodes) {
    // Test all valid month codes
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("F24"), "2024-01"); // January
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("G24"), "2024-02"); // February
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("H24"), "2024-03"); // March
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("J24"), "2024-04"); // April
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("K24"), "2024-05"); // May
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("M24"), "2024-06"); // June
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("N24"), "2024-07"); // July
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("Q24"), "2024-08"); // August
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("U24"), "2024-09"); // September
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("V24"), "2024-10"); // October
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("X24"), "2024-11"); // November
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("Z24"), "2024-12"); // December
}

BOOST_AUTO_TEST_CASE(testInvalidMonthCode) {
    // Test invalid month codes - should return original string
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("A24"), "A24");
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("B24"), "B24");
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("W24"), "W24");
}

BOOST_AUTO_TEST_CASE(testEmptyString) {
    // Test empty input
    BOOST_CHECK_EQUAL(normaliseDeliveryCode(""), "");
}

BOOST_AUTO_TEST_CASE(testInvalidFormat) {
    // Test various invalid formats - should return original string
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("24"), "24");   // No month code
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("F"), "F");     // No year (will throw in stoi)
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("ABC"), "ABC"); // Invalid month code
}

BOOST_AUTO_TEST_CASE(testLowercaseMonthCode) {
    // Test lowercase month codes - should not match (case-sensitive)
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("f24"), "f24");
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("m24"), "m24");
}

BOOST_AUTO_TEST_CASE(testMonthPadding) {
    // Verify month is zero-padded to 2 digits
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("G24"), "2024-02");
    BOOST_CHECK_EQUAL(normaliseDeliveryCode("V24"), "2024-10");
}

BOOST_AUTO_TEST_SUITE_END()