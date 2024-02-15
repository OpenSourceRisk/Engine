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
    \brief Engine builders for Swaptions
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <qle/models/lgm.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/pricingengines/mclgmswaptionengine.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! European Swaption Engine Builder
/*! European Swaptions are priced with Black or Bachelier pricing engines,
 *  depending on the volatility type provided by Market (if it is normal, this
 *  builder returns a Bachelier engine, if it is lognormal (or lognormal shifted)
 *  it will be a Black engine.
 *
 *  Engines are cached based on currency

    \ingroup builders
 */
class EuropeanSwaptionEngineBuilder : public CachingPricingEngineBuilder<string, const string&> {
public:
    EuropeanSwaptionEngineBuilder()
        : CachingEngineBuilder("BlackBachelier", "BlackBachelierSwaptionEngine", {"EuropeanSwaption"}) {}

protected:
    virtual string keyImpl(const string& key) override { return key; }

    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& key) override;
};

//! Abstract BermudanAmericanSwaptionEngineBuilder class
/*! This defines the interface for Bermudan/American Swaption Builders
 *  Pricing Engines are cached by trade id

    \ingroup builders
 */
class BermudanAmericanSwaptionEngineBuilder
    : public CachingPricingEngineBuilder<string, const string&, const string&, const std::vector<Date>&, const Date&,
                                         const std::vector<Real>&, const bool> {
public:
    BermudanAmericanSwaptionEngineBuilder(const string& model, const string& engine)
        : CachingEngineBuilder(model, engine, {"BermudanSwaption", "AmericanSwaption"}) {}

protected:
    virtual string keyImpl(const string& id, const string& key, const std::vector<Date>& dates, const Date& maturity,
                           const std::vector<Real>& strikes, const bool isAmerican) override {
        return id;
    }
};

//! Abstract LGMBermudanAmericanSwaptionEngineBuilder class
/*! This defines the interface for LGM BermudanAmerican Swaption Builders

\ingroup builders
*/
class LGMBermudanAmericanSwaptionEngineBuilder : public BermudanAmericanSwaptionEngineBuilder {
public:
    LGMBermudanAmericanSwaptionEngineBuilder(const string& engine) : BermudanAmericanSwaptionEngineBuilder("LGM", engine) {}

protected:
    boost::shared_ptr<QuantExt::LGM> model(const string& id, const string& key, const std::vector<Date>& dates,
                                           const Date& maturity, const std::vector<Real>& strikes,
                                           const bool isAmerican);
};

//! Implementation of BermudanAmericanSwaptionEngineBuilder using LGM Grid pricer
/*! \ingroup builders
 */
class LGMGridBermudanAmericanSwaptionEngineBuilder : public LGMBermudanAmericanSwaptionEngineBuilder {
public:
    LGMGridBermudanAmericanSwaptionEngineBuilder() : LGMBermudanAmericanSwaptionEngineBuilder("Grid") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(
        //! a unique (trade) id, for caching
        const string& id,
        //! the key (index or ccy)
        const string& key,
        //! Excercise dates
        const std::vector<Date>& dates,
        //! maturity of the underlying
        const Date& maturity,
        //! Fixed rate (null means ATM)
        const std::vector<Real>& strikes, const bool isAmerican) override;
};

//! Implementation of BermudanAmericanSwaptionEngineBuilder using LGM FD pricer
/*! \ingroup builders
 */
class LGMFDBermudanAmericanSwaptionEngineBuilder : public LGMBermudanAmericanSwaptionEngineBuilder {
public:
    LGMFDBermudanAmericanSwaptionEngineBuilder() : LGMBermudanAmericanSwaptionEngineBuilder("FD") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(
        //! a unique (trade) id, for caching
        const string& id,
        //! the key (index or ccy)
        const string& key,
        //! Excercise dates
        const std::vector<Date>& dates,
        //! maturity of the underlying
        const Date& maturity,
        //! Fixed rate (null means ATM)
        const std::vector<Real>& strikes, const bool isAmerican) override;
};

//! Implementation of LGMBermudanAmericanSwaptionEngineBuilder using MC pricer
/*! \ingroup portfolio
 */
class LgmMcBermudanAmericanSwaptionEngineBuilder : public LGMBermudanAmericanSwaptionEngineBuilder {
public:
    LgmMcBermudanAmericanSwaptionEngineBuilder() : LGMBermudanAmericanSwaptionEngineBuilder("MC") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(
        //! a unique (trade) id, for caching
        const string& id,
        //! the currency
        const string& key,
        //! Excercise dates
        const std::vector<Date>& dates,
        //! maturity of the underlying
        const Date& maturity,
        //! Fixed rate (null means ATM)
        const std::vector<Real>& strikes, const bool isAmerican) override;
};

// Implementation of BermudanAmericanSwaptionEngineBuilder for external cam, with additional simulation dates (AMC)
class LgmAmcBermudanAmericanSwaptionEngineBuilder : public BermudanAmericanSwaptionEngineBuilder {
public:
    LgmAmcBermudanAmericanSwaptionEngineBuilder(const boost::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                        const std::vector<Date>& simulationDates)
        : BermudanAmericanSwaptionEngineBuilder("LGM", "AMC"), cam_(cam), simulationDates_(simulationDates) {}

protected:
    // the pricing engine depends on the ccy only
    virtual string keyImpl(const string& id, const string& ccy, const std::vector<Date>& dates, const Date& maturity,
                           const std::vector<Real>& strikes, const bool isAmerican) override {
        return ccy + "_" + std::to_string(isAmerican);
    }
    virtual boost::shared_ptr<PricingEngine> engineImpl(
        //! a unique (trade) id, for caching
        const string& id,
        //! the currency
        const string& key,
        //! Excercise dates
        const std::vector<Date>& dates,
        //! maturity of the underlying
        const Date& maturity,
        //! Fixed rate (null means ATM)
        const std::vector<Real>& strikes, const bool isAmerican) override;

    const boost::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
};
    
} // namespace data
} // namespace ore
