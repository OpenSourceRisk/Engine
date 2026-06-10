/*
 Copyright (C) 2026 AcadiaSoft, Inc.
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/
#include "onindexcouponutils.hpp"
#include <boost/test/unit_test.hpp>
#include <oret/util/fileutilities.hpp>
#include <ql/quotes/simplequote.hpp>
#include <qle/termstructures/interpolateddiscountcurve2.hpp>
#include <iomanip>
#include <fstream>

namespace QuantExt {

using namespace std;
using filesystem::path;
using namespace QuantLib;

namespace OnIndexCouponTest {

// clang-format off

TestCouponData::TestCouponData() {
    // When it comes to coupon calculations, a flat forward curve may hide issues with dates so we use a curve
    // with a slope. Only need discount factors out to a month or so for the tests below.
    Handle<YieldTermStructure> sofrCurve(makeDiscCurve());
    sofr = ext::make_shared<Sofr>(sofrCurve);

    start = Date(1, Nov, 2025); // Saturday
    end = Date(30, Nov, 2025);  // Sunday
    pmt = Date(1, Dec, 2025);
    notional = 1;
}

void loadFixingsUpToDate(const Date& d, ext::shared_ptr<OvernightIndex> index, bool includeDate) {

    // Initialise SOFR data
    static map<Date, Real> sofrRates = {
        {Date(2, Dec, 2025), 0.0401},  {Date(1, Dec, 2025), 0.0412},  {Date(28, Nov, 2025), 0.0412},
        {Date(26, Nov, 2025), 0.0405}, {Date(25, Nov, 2025), 0.0401}, {Date(24, Nov, 2025), 0.0396},
        {Date(21, Nov, 2025), 0.0393}, {Date(20, Nov, 2025), 0.0391}, {Date(19, Nov, 2025), 0.0391},
        {Date(18, Nov, 2025), 0.0394}, {Date(17, Nov, 2025), 0.0400}, {Date(14, Nov, 2025), 0.0395},
        {Date(13, Nov, 2025), 0.0400}, {Date(12, Nov, 2025), 0.0398}, {Date(10, Nov, 2025), 0.0395},
        {Date(7, Nov, 2025), 0.0393},  {Date(6, Nov, 2025), 0.0392},  {Date(5, Nov, 2025), 0.0391},
        {Date(4, Nov, 2025), 0.0400},  {Date(3, Nov, 2025), 0.0413},  {Date(31, Oct, 2025), 0.0422},
        {Date(30, Oct, 2025), 0.0404}, {Date(29, Oct, 2025), 0.0427}, {Date(28, Oct, 2025), 0.0431},
        {Date(27, Oct, 2025), 0.0427}, {Date(24, Oct, 2025), 0.0424}, {Date(24, Oct, 2025), 0.0424}};

    // Load fixings
    Date anchorDate = includeDate ? d : d - 1;
    auto it = sofrRates.cbegin();
    while (it != sofrRates.cend() && it->first <= anchorDate) {
        index->addFixing(it->first, it->second);
        ++it;
    }
}

// Make discount curve for tests.
ext::shared_ptr<YieldTermStructure> makeDiscCurve() {
    static map<Size, DiscountFactor> dateDfs = {
        {0, 1.000000000},  {1, 0.999969863},  {2, 0.999937353},  {3, 0.999903139},  {4, 0.999864017},
        {5, 0.999824880},  {6, 0.999784156},  {7, 0.999739477},  {8, 0.999688193},  {9, 0.999632828},
        {10, 0.999576202}, {11, 0.999519599}, {12, 0.999462613}, {13, 0.999405507}, {14, 0.999335271},
        {15, 0.999266198}, {16, 0.999203719}, {17, 0.999134111}, {18, 0.999065005}, {19, 0.998996331},
        {20, 0.998915909}, {21, 0.998823996}, {22, 0.998726216}, {23, 0.998637407}, {24, 0.998545094},
        {25, 0.998455406}, {26, 0.998367992}, {27, 0.998267186}, {28, 0.998175836}, {29, 0.998065381},
        {30, 0.997956526}, {31, 0.997860354}, {32, 0.997758343}, {33, 0.997643778}, {34, 0.997544354},
        {35, 0.997435933}, {36, 0.997333220}, {37, 0.997189585}, {38, 0.996938409}, {39, 0.996571583},
        {40, 0.996081047}};

    vector<Time> times;
    vector<Handle<Quote>> dfs;
    times.reserve(dateDfs.size());
    dfs.reserve(dateDfs.size());
    for (const auto& [numDays, df] : dateDfs) {
        times.push_back(numDays / 365.0);
        dfs.push_back(Handle<Quote>(ext::make_shared<SimpleQuote>(df)));
    }

    return ext::make_shared<InterpolatedDiscountCurve2>(times, dfs, Actual365Fixed());
}

void runCpnAccrualTest(const OvernightIndexedCouponBase& cpn, const path& outFilePath, const path& expFilePath) {
    // For each evaluation date from before the coupon accrual start date until after the coupon payment date, check
    // the accrued amount for each of those dates.
    const Date& cpnAccStart = cpn.accrualStartDate();
    const Date& cpnPmt = cpn.date();
    Date startAccDate = cpnAccStart - 1;
    Date startEvalDate = cpn.fixingDates().front() - 1;
    Date stopDate = cpnPmt + 1;

    // Open output file for the calculated results.
    ofstream outFile(outFilePath);
    BOOST_REQUIRE_MESSAGE(outFile.is_open(), "runCpnAccrualTest: failed to open file at: " << outFilePath);

    // Write header.
    outFile << "eval / acc";
    for (Date accDate = startAccDate; accDate <= stopDate; ++accDate)
        outFile << "," << io::iso_date(accDate);
    outFile << ",full_coupon";
    outFile << endl;

    // Calculate and write the test results.
    outFile << std::fixed << std::setprecision(4);
    for (Date evalDate = startEvalDate; evalDate <= stopDate; ++evalDate) {
        Settings::instance().evaluationDate() = evalDate;
        outFile << io::iso_date(evalDate);

        // Load fixings up to but not equal to evalDate. This is not always what will happen in practice. For example,
        // 11 Nov 2025 is Veteran's Day holiday in the US but is a good business day in Europe. Valuing on 11 Nov 2025,
        // we would not get the fixing for 10 Nov 2025 until 08:00 ET on 12 Nov 2025 (the next good SOFR business day
        // after the 10 Nov 2025). The valuation would throw if a fixing is not entered for 10 Nov 2025 and this is
        // handled outside of ORE / QuantLib.
        loadFixingsUpToDate(evalDate, cpn.overnightIndex());

        for (Date accDate = startAccDate; accDate <= stopDate; ++accDate) {
            outFile << "," << cpn.accruedAmount(accDate);
        }

        // Tag on the coupon amount as well.
        outFile << "," << cpn.amount();

        outFile << endl;
    }
    outFile.close();

    // Compare against expected results. The results have been validated in Excel.
    BOOST_CHECK(compareFiles(outFilePath.string(), expFilePath.string()));
}
// clang-format on

}
}

