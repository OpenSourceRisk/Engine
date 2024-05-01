/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file marketdata/bondspreadimply.hpp
    \brief bond spread imply utility
    \ingroup marketdata
*/

#pragma once

#include <ored/portfolio/referencedata.hpp>

#include <ored/marketdata/security.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/enginefactory.hpp>

namespace ore {
namespace data {

class BondSpreadImply {
public:
    /*! Determine the securities that require a spread imply and return a map securityID => security containing them.
        If excludeRegex is non-empty security ids that match excludeRegex are excluded from the returned list. */
    static std::map<std::string, QuantLib::ext::shared_ptr<Security>>
    requiredSecurities(const Date& asof, const QuantLib::ext::shared_ptr<TodaysMarketParameters>& params,
                       const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs, const Loader& loader,
                       const bool continueOnError = false, const std::string& excludeRegex = std::string());

    /*! Imply bond spreads and add them to the loader. */
    static QuantLib::ext::shared_ptr<Loader>
    implyBondSpreads(const std::map<std::string, QuantLib::ext::shared_ptr<Security>>& securities,
                     const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager,
                     const QuantLib::ext::shared_ptr<Market>& market, const QuantLib::ext::shared_ptr<EngineData>& engineData,
                     const std::string& configuration, const IborFallbackConfig& iborFallbackConfig);

private:
    //! helper function that computes a single implied spread for a bond
    static Real implySpread(const std::string& securityId, const Real cleanPrice,
                            const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager,
                            const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                            const QuantLib::ext::shared_ptr<SimpleQuote>& spreadQuote, const std::string& configuration);
};

} // namespace data
} // namespace ore
