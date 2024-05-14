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
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/lgm.hpp>
#include <qle/pricingengines/mclgmswaptionengine.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! Swaption engine builder base class
class SwaptionEngineBuilder
    : public CachingPricingEngineBuilder<string, const string&, const string&, const std::vector<Date>&, const Date&,
                                         const std::vector<Real>&, const bool, const string&, const string&> {
public:
    SwaptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes)
        : CachingEngineBuilder(model, engine, tradeTypes) {}

protected:
    string keyImpl(const string& id, const string& key, const std::vector<Date>& dates, const Date& maturity,
                   const std::vector<Real>& strikes, const bool isAmerican, const std::string& discountCurve,
                   const std::string& securitySpread) override {
        return id;
    }
};

//! European Swaption Engine Builder
/*! European Swaptions are priced with Black or Bachelier pricing engines,
    depending on the volatility type provided by Market */
class EuropeanSwaptionEngineBuilder final : public SwaptionEngineBuilder {
public:
    EuropeanSwaptionEngineBuilder()
        : SwaptionEngineBuilder("BlackBachelier", "BlackBachelierSwaptionEngine", {"EuropeanSwaption"}) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& id, const string& key, const std::vector<Date>& dates,
                                                const Date& maturity, const std::vector<Real>& strikes,
                                                const bool isAmerican, const std::string& discountCurve,
                                                const std::string& securitySpread) override;
};

//! Abstract LGMSwaptionEngineBuilder class
class LGMSwaptionEngineBuilder : public SwaptionEngineBuilder {
public:
    LGMSwaptionEngineBuilder(const string& engine)
        : SwaptionEngineBuilder("LGM", engine, {"EuropeanSwaption", "BermudanSwaption", "AmericanSwaption"}) {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::LGM> model(const string& id, const string& key, const std::vector<Date>& dates,
                                           const Date& maturity, const std::vector<Real>& strikes,
                                           const bool isAmerican);
};

//! Implementation of BermudanAmericanSwaptionEngineBuilder using LGM Grid pricer
class LGMGridSwaptionEngineBuilder final : public LGMSwaptionEngineBuilder {
public:
    LGMGridSwaptionEngineBuilder() : LGMSwaptionEngineBuilder("Grid") {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& id, const string& key, const std::vector<Date>& dates,
                                                const Date& maturity, const std::vector<Real>& strikes,
                                                const bool isAmerican, const std::string& discountCurve,
                                                const std::string& securitySpread) override;
};

//! Implementation of BermudanAmericanSwaptionEngineBuilder using LGM FD pricer
class LGMFDSwaptionEngineBuilder final : public LGMSwaptionEngineBuilder {
public:
    LGMFDSwaptionEngineBuilder() : LGMSwaptionEngineBuilder("FD") {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& id, const string& key, const std::vector<Date>& dates,
                                                const Date& maturity, const std::vector<Real>& strikes,
                                                const bool isAmerican, const std::string& discountCurve,
                                                const std::string& securitySpread) override;
};

//! Implementation of LGMBermudanAmericanSwaptionEngineBuilder using MC pricer
class LGMMCSwaptionEngineBuilder final : public LGMSwaptionEngineBuilder {
public:
    LGMMCSwaptionEngineBuilder() : LGMSwaptionEngineBuilder("MC") {}

private:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& id, const string& key, const std::vector<Date>& dates,
                                                const Date& maturity, const std::vector<Real>& strikes,
                                                const bool isAmerican, const std::string& discountCurve,
                                                const std::string& securitySpread) override;
};

// Implementation of BermudanAmericanSwaptionEngineBuilder for external cam, with additional simulation dates (AMC)
class LGMAmcSwaptionEngineBuilder final : public LGMSwaptionEngineBuilder {
public:
    LGMAmcSwaptionEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                const std::vector<Date>& simulationDates)
        : LGMSwaptionEngineBuilder("AMC"), cam_(cam), simulationDates_(simulationDates) {}

private:
    string keyImpl(const string& id, const string& ccy, const std::vector<Date>& dates, const Date& maturity,
                   const std::vector<Real>& strikes, const bool isAmerican, const std::string& discountCurve,
                   const std::string& securitySpread) override {
        return ccy + "_" + std::to_string(isAmerican) + discountCurve + securitySpread;
    }
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& id, const string& key, const std::vector<Date>& dates,
                                                const Date& maturity, const std::vector<Real>& strikes,
                                                const bool isAmerican, const std::string& discountCurve,
                                                const std::string& securitySpread) override;

    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
};

} // namespace data
} // namespace ore
