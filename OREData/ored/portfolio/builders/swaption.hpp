/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file portfolio/builders/swap.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/utilities/log.hpp>
#include <boost/make_shared.hpp>

namespace openriskengine {
namespace data {

//! European Swaption Engine Builder
/*! European Swaptions are priced with Black or Bachelier pricing engines,
 *  depending on the volatility type provided by Market (if it is normal, this
 *  builder returns a Bachelier engine, if it is lognormal (or lognormal shiffed)
 *  it will be a Black engine.
 *
 *  Engines are cached based on currency

    \ingroup portfolio
 */
class EuropeanSwaptionEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&> {
public:
    EuropeanSwaptionEngineBuilder() : CachingEngineBuilder("BlackBachelier", "BlackBachelierSwaptionEngine") {}

protected:
    virtual string keyImpl(const Currency& ccy) override { return ccy.code(); }

    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy) override;
};

//! Abstract BermudanSwaptionEngineBuilder class
/*! This defines the interface for Bermudan Swaption Builders
 *  Pricing Engines are not cached by this builder, but subclasses can employ
 *  a cache

    \ingroup portfolio
 */
class BermudanSwaptionEngineBuilder : public EngineBuilder {
public:
    BermudanSwaptionEngineBuilder(const string& model, const string& engine) : EngineBuilder(model, engine) {}

    virtual boost::shared_ptr<PricingEngine> engine( //! a unique (trade) id, for caching
        const string& id,
        //! is this a standard swaption
        bool isNonStandard,
        //! the currency
        const string& ccy,
        //! Excercise dates
        const std::vector<Date>& dates,
        //! Final maturity (for physical this may be greater than the last exercise date)
        const Date& maturity,
        //! Fixed rate
        Real fixedRate) = 0;
};

//! Implementation of BermudanSwaptionEngineBuilder using LGM Grid pricer
/*! \ingroup portfolio
*/
class LGMGridBermudanSwaptionEngineBuilder : public BermudanSwaptionEngineBuilder {
public:
    LGMGridBermudanSwaptionEngineBuilder() : BermudanSwaptionEngineBuilder("LGM", "Grid") {}

    boost::shared_ptr<PricingEngine> engine(const string& id, bool isNonStandard, const string& ccy,
                                            const std::vector<Date>& dates, const Date& maturity,
                                            Real fixedRate) override;
};

} // namespace data
} // namespace openriskengine
