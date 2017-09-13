/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/swap.hpp
    \brief
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <qle/models/lgm.hpp>

#include <boost/make_shared.hpp>

namespace ore {
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
    EuropeanSwaptionEngineBuilder()
        : CachingEngineBuilder("BlackBachelier", "BlackBachelierSwaptionEngine", {"EuropeanSwaption"}) {}

protected:
    virtual string keyImpl(const Currency& ccy) override { return ccy.code(); }

    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy) override;
};

//! Abstract BermudanSwaptionEngineBuilder class
/*! This defines the interface for Bermudan Swaption Builders
 *  Pricing Engines are cached by trade id

    \ingroup portfolio
 */
class BermudanSwaptionEngineBuilder
    : public CachingPricingEngineBuilder<string, const string&, const bool, const string&, const std::vector<Date>&,
                                         const Date&, const Real> {
public:
    BermudanSwaptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"BermudanSwaption"}) {}

protected:
    virtual string keyImpl(const string& id, const bool isNonStandard, const string& ccy,
                           const std::vector<Date>& dates, const Date& maturity, const Real fixedRate) override {
        return id;
    }
};

class LGMBermudanSwaptionEngineBuilder : public BermudanSwaptionEngineBuilder {
public:
    LGMBermudanSwaptionEngineBuilder(const string& engine) : BermudanSwaptionEngineBuilder("LGM", engine) {}

protected:
    boost::shared_ptr<QuantExt::LGM> model(const string& id, bool isNonStandard, const string& ccy,
                                           const std::vector<Date>& dates, const Date& maturity, const Real fixedRate);
};

//! Implementation of BermudanSwaptionEngineBuilder using LGM Grid pricer
/*! \ingroup portfolio
 */
class LGMGridBermudanSwaptionEngineBuilder : public LGMBermudanSwaptionEngineBuilder {
public:
    LGMGridBermudanSwaptionEngineBuilder() : LGMBermudanSwaptionEngineBuilder("Grid") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(
        //! a unique (trade) id, for caching
        const string& id,
        //! is this a standard swaption
        const bool isNonStandard,
        //! the currency
        const string& ccy,
        //! Excercise dates
        const std::vector<Date>& dates,
        //! maturity of the underlying
        const Date& maturity,
        //! Fixed rate (null means ATM)
        const Real fixedRate) override;
};

} // namespace data
} // namespace ore
