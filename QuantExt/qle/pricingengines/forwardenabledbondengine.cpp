/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/forwardenabledbondengine.hpp>

#include <ql/math/solvers1d/newtonsafe.hpp>
#include <ql/pricingengines/bond/bondfunctions.hpp>

namespace QuantExt {

std::pair<QuantLib::Real, QuantLib::Real>
forwardPrice(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& instrument, const QuantLib::Date& forwardDate,
             const QuantLib::Date& settlementDate, const bool conditionalOnSurvival,
             std::vector<CashFlowResults>* cfResults, QuantLib::Leg* const expectedCashflows) {
    instrument->recalculate();
    auto fwdEngine = QuantLib::ext::dynamic_pointer_cast<ForwardEnabledBondEngine>(instrument->pricingEngine());
    QL_REQUIRE(fwdEngine, "QuantExt::forwardPrice(): engine can not be cast to ForwardEnabledBondEngine");
    return fwdEngine->forwardPrice(forwardDate, settlementDate, conditionalOnSurvival, cfResults, expectedCashflows);
}

QuantLib::Real yield(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& instrument, QuantLib::Real price,
                     const QuantLib::DayCounter& dayCounter, QuantLib::Compounding compounding,
                     QuantLib::Frequency frequency, QuantLib::Date forwardDate, QuantLib::Date settlementDate,
                     QuantLib::Real accuracy, QuantLib::Size maxIterations, QuantLib::Rate guess,
                     QuantLib::Bond::Price::Type priceType) {

    instrument->recalculate();
    auto fwdEngine = QuantLib::ext::dynamic_pointer_cast<ForwardEnabledBondEngine>(instrument->pricingEngine());
    QL_REQUIRE(fwdEngine, "forwardPrice(): engine can not be cast to ForwardEnabledBondEngine");

    auto bond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(instrument);
    QL_REQUIRE(bond, "QuantExt::yield(): instrument can not be cast to Bond");

    QuantLib::Date today = QuantLib::Settings::instance().evaluationDate();

    if (forwardDate == QuantLib::Date())
        forwardDate = today;

    if (settlementDate == QuantLib::Date())
        settlementDate = bond->settlementDate(forwardDate);

    QL_REQUIRE(QuantLib::BondFunctions::isTradable(*bond, settlementDate), "QuantExt::yield(): non tradable at "
                                                                               << settlementDate << " (maturity being "
                                                                               << bond->maturityDate() << ")");

    if (priceType == QuantLib::Bond::Price::Clean)
        price += bond->accruedAmount(settlementDate);

    price /= 100.0 / bond->notional(settlementDate);

    QuantLib::Leg expectedCashflows;
    fwdEngine->forwardPrice(forwardDate, settlementDate, true, nullptr, &expectedCashflows);

    QuantLib::NewtonSafe solver;
    solver.setMaxEvaluations(maxIterations);
    return QuantLib::CashFlows::yield<QuantLib::NewtonSafe>(solver, expectedCashflows, price, dayCounter, compounding,
                                                            frequency, false, settlementDate, settlementDate, accuracy,
                                                            guess);
}

QuantLib::Real duration(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& instrument, QuantLib::Rate yield,
                        const QuantLib::DayCounter& dayCounter, QuantLib::Compounding compounding,
                        QuantLib::Frequency frequency, QuantLib::Duration::Type type, QuantLib::Date forwardDate,
                        QuantLib::Date settlementDate) {

    instrument->recalculate();
    auto fwdEngine = QuantLib::ext::dynamic_pointer_cast<ForwardEnabledBondEngine>(instrument->pricingEngine());
    QL_REQUIRE(fwdEngine, "forwardPrice(): engine can not be cast to ForwardEnabledBondEngine");

    auto bond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(instrument);
    QL_REQUIRE(bond, "QuantExt::yield(): instrument can not be cast to Bond");

    QuantLib::Date today = QuantLib::Settings::instance().evaluationDate();

    if (forwardDate == QuantLib::Date())
        forwardDate = today;

    if (settlementDate == QuantLib::Date())
        settlementDate = bond->settlementDate(forwardDate);

    QL_REQUIRE(QuantLib::BondFunctions::isTradable(*bond, settlementDate), "QuantExt::duration(): non tradable at "
                                                                               << settlementDate << " (maturity being "
                                                                               << bond->maturityDate() << ")");

    QuantLib::Leg expectedCashflows;
    fwdEngine->forwardPrice(forwardDate, settlementDate, true, nullptr, &expectedCashflows);

    QuantLib::InterestRate y(yield, dayCounter, compounding, frequency);
    return QuantLib::CashFlows::duration(expectedCashflows, y, type, false, settlementDate);
}

} // namespace QuantExt
