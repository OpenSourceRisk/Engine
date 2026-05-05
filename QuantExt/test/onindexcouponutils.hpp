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
#pragma once

#include <filesystem>
#include <ql/indexes/ibor/sofr.hpp>
#include <ql/shared_ptr.hpp>
#include <qle/cashflows/overnightindexedcouponbase.hpp>

namespace QuantExt {
namespace OnIndexCouponTest {

// Hold test data for coupon creation.
struct TestCouponData {
    TestCouponData();

    QuantLib::ext::shared_ptr<QuantLib::Sofr> sofr;
    QuantLib::Date start;
    QuantLib::Date end;
    QuantLib::Date pmt;
    QuantLib::Real notional;
};

// Load fixings for `index` up to, including or excluding, date `d`.
void loadFixingsUpToDate(const QuantLib::Date& d,
    QuantLib::ext::shared_ptr<QuantLib::OvernightIndex> index,
    bool includeDate = false);

// Create a simple discount curve for testing purposes.
QuantLib::ext::shared_ptr<QuantLib::YieldTermStructure> makeDiscCurve();

// Write out accrual amounts for `cpn` under `outFilePath` and compare to expected results at `expFilePath`.
void runCpnAccrualTest(const OvernightIndexedCouponBase& cpn, const std::filesystem::path& outFilePath,
    const std::filesystem::path& expFilePath);

}
}
