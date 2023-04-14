/*
 Copyright (C) 2016-2023 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/forwardrateagreement.hpp
    \brief Engine builder for FRAs
    \ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <qle/models/crossassetmodel.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for Forward Rate Agreements
/*! Pricing engines are cached by currency
    \ingroup builders
*/
class FraEngineBuilderBase : public CachingPricingEngineBuilder<string, const Currency&> {
public:
    FraEngineBuilderBase(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"ForwardRateAgreement"}) {}

protected:
    virtual string keyImpl(const Currency& ccy) override { return ccy.code(); }
};

//! Implementation of FraEngineBuilderBase using MC pricer for external cam / AMC
/*! \ingroup portfolio
 */
class LgmAmcFraEngineBuilder : public FraEngineBuilderBase {
public:
    LgmAmcFraEngineBuilder(const boost::shared_ptr<QuantExt::CrossAssetModel>& cam,
                           const std::vector<Date>& simulationDates)
        : FraEngineBuilderBase("LGM", "AMC"), cam_(cam), simulationDates_(simulationDates) {}

protected:
    // the pricing engine depends on the ccy only, can use the caching from FraEngineBuilderBase
    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy) override;

private:
    boost::shared_ptr<PricingEngine> buildMcEngine(const boost::shared_ptr<QuantExt::LGM>& lgm,
                                                   const Handle<YieldTermStructure>& discountCurve,
                                                   const std::vector<Date>& simulationDates,
                                                   const std::vector<Size>& externalModelIndices);
    const boost::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
};

} // namespace data
} // namespace ore
