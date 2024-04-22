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

/*!
    \file ored/marketdata/dummymarket.hpp
    \brief Dummy Market class returning empty handles, used in tests
    \ingroup curves
*/

#pragma once

#include <ored/marketdata/market.hpp>
#include <ored/utilities/indexparser.hpp>

namespace ore {
namespace analytics {
using namespace std;
using namespace QuantLib;
using namespace ore::data;

//! DummyMarket
class DummyMarket : public Market {
public:
    DummyMarket() : Market(false) {}

    //! Get the asof Date
    Date asofDate() const override { return Date(); }

    Handle<YieldTermStructure> discountCurveImpl(const string& key, const string& config) const override {
        return Handle<YieldTermStructure>();
    }
    Handle<YieldTermStructure> yieldCurve(const ore::data::YieldCurveType& type, const string&,
                                          const string&) const override {
        return Handle<YieldTermStructure>();
    }
    Handle<YieldTermStructure> yieldCurve(const string&, const string&) const override {
        return Handle<YieldTermStructure>();
    }
    Handle<IborIndex> iborIndex(const string&, const string&) const override {
        return Handle<IborIndex>(ore::data::parseIborIndex("EUR-EONIA")); // ugly, but some people check index.empty()
    }
    Handle<SwapIndex> swapIndex(const string&, const string&) const override { return Handle<SwapIndex>(); }
    Handle<SwaptionVolatilityStructure> swaptionVol(const string&, const string&) const override {
        return Handle<SwaptionVolatilityStructure>();
    }
    string shortSwapIndexBase(const string&, const string&) const override { return string(); }
    string swapIndexBase(const string&, const string&) const override { return string(); }

    Handle<SwaptionVolatilityStructure> yieldVol(const string&, const string&) const override {
        return Handle<SwaptionVolatilityStructure>();
    }

    Handle<QuantExt::FxIndex> fxIndexImpl(const string& index, const string&) const override {
        if (isFxIndex(index))
            return Handle<QuantExt::FxIndex>(parseFxIndex(index));
        return Handle<QuantExt::FxIndex>();
    };
    Handle<Quote> fxSpotImpl(const string&, const string&) const override { return Handle<Quote>(); }
    Handle<Quote> fxRateImpl(const string&, const string&) const override { return Handle<Quote>(); }
    Handle<BlackVolTermStructure> fxVolImpl(const string&, const string&) const override {
        return Handle<BlackVolTermStructure>();
    }

    Handle<QuantExt::CreditCurve> defaultCurve(const string&, const string&) const override {
        return Handle<QuantExt::CreditCurve>(
            QuantLib::ext::make_shared<QuantExt::CreditCurve>(Handle<DefaultProbabilityTermStructure>()));
    }
    Handle<Quote> recoveryRate(const string&, const string&) const override { return Handle<Quote>(); }

    Handle<QuantExt::CreditVolCurve> cdsVol(const string&, const string&) const override {
        return Handle<QuantExt::CreditVolCurve>();
    }

    Handle<QuantExt::BaseCorrelationTermStructure> baseCorrelation(const string&, const string&) const override {
        return Handle<QuantExt::BaseCorrelationTermStructure>();
    }

    Handle<OptionletVolatilityStructure> capFloorVol(const string&, const string&) const override {
        return Handle<OptionletVolatilityStructure>();
    }

    std::pair<string, QuantLib::Period> capFloorVolIndexBase(const string&, const string&) const override {
        return std::make_pair(string(), 0 * Days);
    }

    Handle<ZeroInflationIndex> zeroInflationIndex(const string&, const string&) const override {
        return Handle<ZeroInflationIndex>();
    }
    Handle<YoYInflationIndex> yoyInflationIndex(const string&, const string&) const override {
        return Handle<YoYInflationIndex>();
    }

    Handle<QuantExt::YoYOptionletVolatilitySurface> yoyCapFloorVol(const string&, const string&) const override {
        return Handle<QuantExt::YoYOptionletVolatilitySurface>();
    }

    Handle<QuantLib::CPIVolatilitySurface> cpiInflationCapFloorVolatilitySurface(const string&, const string&) const override {
        return Handle<QuantLib::CPIVolatilitySurface>();
    }

    Handle<Quote> equitySpot(const string&, const string&) const override { return Handle<Quote>(); }
    Handle<YieldTermStructure> equityDividendCurve(const string&, const string&) const override {
        return Handle<YieldTermStructure>();
    }
    Handle<YieldTermStructure> equityForecastCurve(const string&, const string&) const override {
        return Handle<YieldTermStructure>();
    }

    Handle<QuantExt::EquityIndex2> equityCurve(const string& eqName, const string&) const override {
        return Handle<QuantExt::EquityIndex2>();
    }

    Handle<BlackVolTermStructure> equityVol(const string&, const string&) const override {
        return Handle<BlackVolTermStructure>();
    }

    Handle<Quote> securitySpread(const string&, const string&) const override { return Handle<Quote>(); }

    QuantLib::Handle<QuantExt::PriceTermStructure> commodityPriceCurve(const std::string&,
                                                                       const std::string&) const override {
        return QuantLib::Handle<QuantExt::PriceTermStructure>();
    }

    QuantLib::Handle<QuantExt::CommodityIndex> commodityIndex(const std::string&, const std::string&) const override {
        return QuantLib::Handle<QuantExt::CommodityIndex>();
    }

    QuantLib::Handle<QuantLib::BlackVolTermStructure> commodityVolatility(const std::string&,
                                                                          const std::string&) const override {
        return Handle<BlackVolTermStructure>();
    }

    QuantLib::Handle<QuantLib::Quote> cpr(const string&, const string&) const override {
        return QuantLib::Handle<QuantLib::Quote>();
    }

    QuantLib::Handle<QuantExt::CorrelationTermStructure> correlationCurve(const std::string&, const std::string&,
                                                                          const std::string&) const override {
        return QuantLib::Handle<QuantExt::CorrelationTermStructure>();
    }
};

} // namespace analytics
} // namespace ore
