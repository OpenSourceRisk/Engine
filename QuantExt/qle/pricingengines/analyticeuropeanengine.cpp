/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ql/exercise.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <qle/pricingengines/analyticeuropeanengine.hpp>
#include <utility>

using std::vector;
using std::string;

namespace QuantExt {

using namespace QuantLib;

void AnalyticEuropeanEngine::calculate() const {

    QuantLib::AnalyticEuropeanEngine::calculate();

    if (flipResults_) {

        // Invert strike, spot, forward

        auto resToInvert = vector<string>({"spot", "forward", "strike"});
        for (const string& res : resToInvert) {
            auto it = results_.additionalResults.find(res);
            if (it != results_.additionalResults.end())
                it->second = 1. / boost::any_cast<Real>(it->second);
        }

        // Swap riskFreeDiscount and dividendDiscount, discountFactor stays what it is

        Real rfDiscount = Null<Real>();
        Real divDiscount = Null<Real>();

        if (auto tmp = results_.additionalResults.find("riskFreeDiscount"); tmp != results_.additionalResults.end())
            rfDiscount = boost::any_cast<Real>(tmp->second);
        if (auto tmp = results_.additionalResults.find("dividendDiscount"); tmp != results_.additionalResults.end())
            divDiscount = boost::any_cast<Real>(tmp->second);

       results_.additionalResults["riskFreeDiscount"] = divDiscount;
       results_.additionalResults["dividendDiscount"] = rfDiscount;

    }
}

} // namespace QuantExt
