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

#include <variant>

namespace ore {
namespace data {

using SwaptionModel = std::variant<std::monostate, Handle<CrossAssetModel>, ext::shared_ptr<QuantExt::LGM>>;

/*! Swaption engine builder base class. This is for the general xccy case, i.e. keys and strikes have one element per
    supported underlying currency. FX strikes are against base ccy, where base ccy is the first currenc resp. the base
    ccy of an external CAM (AMC builders). For the single currency case, keys and strikes have one entry and fxStrikes
   is empty. */
class SwaptionEngineBuilder
    : public CachingPricingEngineBuilder<string, const string&, const std::vector<string>&, const std::vector<Date>&,
                                         const std::vector<Date>&, const std::vector<std::vector<Real>>&,
                                         const std::vector<std::vector<Real>>&, const bool, const string&,
                                         const string&, const SwaptionModel&> {
public:
    SwaptionEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes,
                          const bool idBasedKey = true)
        : CachingEngineBuilder(model, engine, tradeTypes), idBasedKey_(idBasedKey) {}

    SwaptionModel model(const string& id, const std::vector<string>& keys, const std::vector<Date>& expiries,
                        const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
                        const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican) const;

protected:
    CrossAssetModel::Discretization discretization_ = CrossAssetModel::Discretization::Exact;

private:
    string keyImpl(const string& id, const std::vector<string>& keys, const std::vector<Date>& dates,
                   const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
                   const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican,
                   const std::string& discountCurve, const std::string& securitySpread, const SwaptionModel&) override;
    bool idBasedKey_ = true;
};

//! European Swaption Engine Builder
/*! European Swaptions are priced with Black or Bachelier pricing engines,
    depending on the volatility type provided by Market */
class EuropeanSwaptionEngineBuilder final : public SwaptionEngineBuilder {
public:
    EuropeanSwaptionEngineBuilder()
        : SwaptionEngineBuilder("BlackBachelier", "BlackBachelierSwaptionEngine",
                                {"EuropeanSwaption", "EuropeanSwaption_NonStandard"}) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const string& id, const std::vector<string>& keys, const std::vector<Date>& dates,
               const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
               const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican, const std::string& discountCurve,
               const std::string& securitySpread, const SwaptionModel&) override;
};

//! LGM based builders, base class
class LGMSwaptionEngineBuilder : public SwaptionEngineBuilder {
public:
    LGMSwaptionEngineBuilder(const string& engine)
        : SwaptionEngineBuilder("LGM", engine,
                                {"EuropeanSwaption", "EuropeanSwaption_NonStandard", "BermudanSwaption",
                                 "BermudanSwaption_NonStandard", "AmericanSwaption", "AmericanSwaption_NonStandard"}) {}
};

//! LGM Grid engine
class LGMGridSwaptionEngineBuilder final : public LGMSwaptionEngineBuilder {
public:
    LGMGridSwaptionEngineBuilder() : LGMSwaptionEngineBuilder("Grid") {}

private:
    QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const string& id, const std::vector<string>& keys, const std::vector<Date>& dates,
               const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
               const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican, const std::string& discountCurve,
               const std::string& securitySpread, const SwaptionModel&) override;
};

//! LGM FD engine
class LGMFDSwaptionEngineBuilder final : public LGMSwaptionEngineBuilder {
public:
    LGMFDSwaptionEngineBuilder() : LGMSwaptionEngineBuilder("FD") {}

private:
    QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const string& id, const std::vector<string>& keys, const std::vector<Date>& dates,
               const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
               const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican, const std::string& discountCurve,
               const std::string& securitySpread, const SwaptionModel&) override;
};

//!

//! CAM based engines, base class
class CamSwaptionEngineBuilder : public SwaptionEngineBuilder {
public:
    explicit CamSwaptionEngineBuilder(const std::string& engine, const bool idBasedKey = true)
        : SwaptionEngineBuilder("LGM", engine,
                                {"EuropeanSwaption", "EuropeanSwaption_NonStandard", "BermudanSwaption",
                                 "BermudanSwaption_NonStandard", "AmericanSwaption", "AmericanSwaption_NonStandard",
                                 "EuropeanSwaption_XCcy", "EuropeanSwaption_NonStandard_XCcy", "BermudanSwaption_XCcy",
                                 "BermudanSwaption_NonStandard_XCcy", "AmericanSwaption_XCcy",
                                 "AmericanSwaption_NonStandard_XCcy"},
                                idBasedKey) {}
};

// CAM MC engine
class CamMCSwaptionEngineBuilder final : public CamSwaptionEngineBuilder {
public:
    CamMCSwaptionEngineBuilder() : CamSwaptionEngineBuilder("MC") {}

private:
    QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const string& id, const std::vector<string>& keys, const std::vector<Date>& dates,
               const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
               const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican, const std::string& discountCurve,
               const std::string& securitySpread, const SwaptionModel&) override;
};

//! CAM AMC engine
class AmcSwaptionEngineBuilder final : public CamSwaptionEngineBuilder {
public:
    AmcSwaptionEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                             const std::vector<Date>& simulationDates, const std::vector<Date>& stickyCloseOutDates)
        : CamSwaptionEngineBuilder("AMC", false), cam_(cam), simulationDates_(simulationDates),
          stickyCloseOutDates_(stickyCloseOutDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const string& id, const std::vector<string>& keys, const std::vector<Date>& dates,
               const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
               const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican, const std::string& discountCurve,
               const std::string& securitySpread, const SwaptionModel&) override;

    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
    const std::vector<Date> stickyCloseOutDates_;
};

// CAM MCCG engine
class CamMCCgSwaptionEngineBuilder final : public CamSwaptionEngineBuilder {
public:
    CamMCCgSwaptionEngineBuilder() : CamSwaptionEngineBuilder("MCCG") {
        discretization_ = CrossAssetModel::Discretization::Euler;
    }

private:
    QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const string& id, const std::vector<string>& keys, const std::vector<Date>& dates,
               const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
               const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican, const std::string& discountCurve,
               const std::string& securitySpread, const SwaptionModel&) override;
};

//! CAM AMC-CG engine
class AmcCgSwaptionEngineBuilder final : public CamSwaptionEngineBuilder {
public:
    AmcCgSwaptionEngineBuilder(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& modelCg,
                               const std::vector<Date>& simulationDates)
        : CamSwaptionEngineBuilder("AMCCG", false), modelCg_(modelCg), simulationDates_(simulationDates) {}

private:
    QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const string& id, const std::vector<string>& keys, const std::vector<Date>& dates,
               const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
               const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican, const std::string& discountCurve,
               const std::string& securitySpread, const SwaptionModel&) override;

    const QuantLib::ext::shared_ptr<ore::data::ModelCG> modelCg_;
    const std::vector<Date> simulationDates_;
};

} // namespace data
} // namespace ore
