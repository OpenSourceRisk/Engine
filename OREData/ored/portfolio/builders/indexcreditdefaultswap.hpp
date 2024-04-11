/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/indexcreditdefaultswap.hpp
\brief
\ingroup portfolio
*/

#pragma once

#include <ored/utilities/parsers.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for Index Credit Default Swaps
/*! Pricing engines are cached by the index CDS trade's currency, the index CDS constituent credit curve IDs and the
    index CDS credit curve ID.

    \ingroup portfolio
*/

class IndexCreditDefaultSwapEngineBuilder
    : public CachingPricingEngineBuilder<vector<string>, const Currency&, const string&, const vector<string>&,
                                         const boost::optional<string>&, Real, bool> {

public:
    CreditPortfolioSensitivityDecomposition sensitivityDecomposition();

protected:
    IndexCreditDefaultSwapEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"IndexCreditDefaultSwap"}) {}

    vector<string> keyImpl(const Currency& ccy, const string& creditCurveId, const vector<string>& creditCurveIds,
                           const boost::optional<string>& overrideCurve, Real recoveryRate = Null<Real>(),
                           const bool inCcyDiscountCurve = false) override;
};

//! Midpoint Engine Builder class for IndexCreditDefaultSwaps
/*! This class creates a MidPointCdsEngine
    \ingroup portfolio
*/

class MidPointIndexCdsEngineBuilder : public IndexCreditDefaultSwapEngineBuilder {
public:
    MidPointIndexCdsEngineBuilder()
        : IndexCreditDefaultSwapEngineBuilder("DiscountedCashflows", "MidPointIndexCdsEngine") {}

protected:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency& ccy, const string& creditCurveId,
                                                const vector<string>& creditCurveIds,
                                                const boost::optional<string>& overrideCurve,
                                                Real recoveryRate = Null<Real>(),
                                                const bool inCcyDiscountCurve = false) override;
};

} // namespace data
} // namespace ore
