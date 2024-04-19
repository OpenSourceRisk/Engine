/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/models/blackscholescg.hpp
    \brief black scholes model for n underlyings (fx, equity or commodity)
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/blackscholescgbase.hpp>

namespace ore {
namespace data {

class BlackScholesCG : public BlackScholesCGBase {
public:
    /* ctor for multiple underlyings, see BlackScholesBase, plus:
       - processes: hold spot, rate and div ts and vol for each given index
       - we assume that the given correlations are constant and read the value only at t = 0
       - calibration strikes are given as a map indexName => strike, if an index is missing in this map, the calibration
         strike will be atmf */
    BlackScholesCG(
        const Size paths, const std::vector<std::string>& currencies,
        const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
        const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
        const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
        const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
        const Handle<BlackScholesModelWrapper>& model,
        const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
        const std::set<Date>& simulationDates,
        const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
        const std::string& calibration = "ATM",
        const std::map<std::string, std::vector<Real>>& calibrationStrikes = {});

    // ctor for one underlying
    BlackScholesCG(const Size paths, const std::string& currency, const Handle<YieldTermStructure>& curve,
                   const std::string& index, const std::string& indexCurrency,
                   const Handle<BlackScholesModelWrapper>& model, const std::set<Date>& simulationDates,
                   const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                   const std::string& calibration = "ATM", const std::vector<Real>& calibrationStrikes = {});

private:
    // ModelImpl interface implementation
    std::size_t getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                     const std::size_t barrier, const bool above) const override;

    // BlackScholesBase interface implementation
    void performCalculations() const override;

    // The calibration to use, ATM or Deal
    const std::string calibration_;

    // map indexName => calibration strike (for missing indices we'll assume atmf)
    const std::map<std::string, std::vector<Real>> calibrationStrikes_;
};

} // namespace data
} // namespace ore
