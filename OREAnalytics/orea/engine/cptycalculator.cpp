/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file engine/cptycalculator.hpp
    \brief The counterparty cube calculator interface
    \ingroup simulation
*/

#include <orea/engine/cptycalculator.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace analytics {

void SurvivalProbabilityCalculator::calculate(const std::string& name, Size nameIndex,
                                              const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                                              const Date& date, Size dateIndex, Size sample, bool isCloseOut) {
    if (!isCloseOut)
        outputCube->set(survProb(name, simMarket, date), nameIndex, dateIndex, sample, index_);
}

void SurvivalProbabilityCalculator::calculateT0(const std::string& name, Size nameIndex,
                                                const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                                                QuantLib::ext::shared_ptr<NPVCube>& outputCube) {
    outputCube->setT0(survProb(name, simMarket), nameIndex, index_);
}

Real SurvivalProbabilityCalculator::survProb(const std::string& name,
                                             const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                                             const Date& date) {
    Real survivalProb = 1.0;

    try {
        Handle<DefaultProbabilityTermStructure> dts = simMarket->defaultCurve(name, configuration_)->curve();
        QL_REQUIRE(!dts.empty(), "Default curve missing for counterparty " << name);
        survivalProb = dts->survivalProbability(date == Date() ? dts->referenceDate() : date);
    } catch (std::exception& e) {
        ALOG("Failed to calculate surv prob of counterparty " << name << " : " << e.what());
        survivalProb = 1.0;
    } catch (...) {
        ALOG("Failed to calculate surv prob of counterparty " << name << " : Unhandled Exception");
        survivalProb = 1.0;
    }
    return survivalProb;
}
} // namespace analytics
} // namespace ore
