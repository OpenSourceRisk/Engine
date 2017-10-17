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

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/quotes/simplequote.hpp>

#include <qle/models/infdkparametrization.hpp>
#include <qle/models/cpicapfloorhelper.hpp>

#include <ored/model/infdkbuilder.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

InfDkBuilder::InfDkBuilder(const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<InfDkData>& data,
        const std::string& configuration)
        : market_(market), configuration_(configuration), data_(data) {

    LOG("DkBuilder for " << data_->infIndex());

    inflationIndex_ = boost::dynamic_pointer_cast<ZeroInflationIndex>(
        *market_->zeroInflationIndex(data_->infIndex(), configuration_));
    QL_REQUIRE(inflationIndex_, "DkBuilder: requires ZeroInflationIndex, got " << data_->infIndex());

    if (data_->calibrateA() || data_->calibrateH())
        buildCapFloorBasket();

    Array aTimes(data_->aTimes().begin(), data_->aTimes().end());
    Array hTimes(data_->hTimes().begin(), data_->hTimes().end());
    Array alpha(data_->aValues().begin(), data_->aValues().end());
    Array h(data_->hValues().begin(), data_->hValues().end());

    // TODO, support other param types
    QL_REQUIRE(data_->aParamType() == ParamType::Piecewise, "DkBuilder, only piecewise volatility is supported currently");
    QL_REQUIRE(data_->hParamType() == ParamType::Piecewise, "DkBuilder, only piecewise reversion is supported currently");

    if (data_->calibrateA()) { // override
        if (data_->aTimes().size() > 0) {
            LOG("overriding alpha time grid with option expiries");
        }
        QL_REQUIRE(optionExpiries_.size() > 0, "empty option expiries");
        aTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
        alpha = Array(aTimes.size() + 1, data_->aValues()[0]);
    }
    else { // use input time grid and input alpha array otherwise
        aTimes = Array(data_->aTimes().begin(), data_->aTimes().end());
        alpha = Array(data_->aValues().begin(), data_->aValues().end());
        QL_REQUIRE(alpha.size() == aTimes.size() + 1, "alpha grids do not match");
    }

    if (data_->calibrateH()) { // override
        if (data_->hTimes().size() > 0) {
            LOG("overriding H time grid with option expiries");
        }
        QL_REQUIRE(optionExpiries_.size() > 0, "empty option expiries");
        hTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
        h = Array(hTimes.size() + 1, data_->hValues()[0]);
    }
    else { // use input time grid and input alpha array otherwise
        hTimes = Array(data_->hTimes().begin(), data_->hTimes().end());
        h = Array(data_->hValues().begin(), data_->hValues().end());
        QL_REQUIRE(h.size() == hTimes.size() + 1, "H grids do not match");
    }

    QL_REQUIRE(!data_->calibrateA(), "DkBuilder, calibrateA not supported.");

    if (data_->reversionType() == LgmData::ReversionType::HullWhite &&
        data_->volatilityType() == LgmData::VolatilityType::HullWhite) {
        LOG("INF parametrization: InfDkPiecewiseConstantHullWhiteAdaptor");
        parametrization_ = boost::make_shared<InfDkPiecewiseConstantHullWhiteAdaptor>
            (inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h);
    }
    else if (data_->reversionType() == LgmData::ReversionType::HullWhite) {
        LOG("INF parametrization for " << data_->infIndex() << ": InfDkPiecewiseConstant");
        parametrization_ = boost::make_shared<InfDkPiecewiseConstantParametrization>
            (inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h);
    }
    else {
        parametrization_ = boost::make_shared<InfDkPiecewiseLinearParametrization>
            (inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h);
        LOG("INF parametrization for " << data_->infIndex() << ": InfDkPiecewiseLinear");
    }

    LOG("alpha times size: " << aTimes.size());
    LOG("lambda times size: " << hTimes.size());

    LOG("Apply shift horizon and scale");

    QL_REQUIRE(data_->shiftHorizon() >= 0.0, "shift horizon must be non negative");
    QL_REQUIRE(data_->scaling() > 0.0, "scaling must be positive");

    if (data_->shiftHorizon() > 0.0) {
        LOG("Apply shift horizon " << data_->shiftHorizon() << " to the " << data_->infIndex() << " DK model");
        parametrization_->shift() = data_->shiftHorizon();
    }

    if (data_->scaling() != 1.0) {
        LOG("Apply scaling " << data_->scaling() << " to the " << data_->infIndex() << " DK model");
        parametrization_->scaling() = data_->scaling();
    }

}

void InfDkBuilder::buildCapFloorBasket() {
    QL_REQUIRE(data_->optionExpiries().size() == data_->optionStrikes().size(), "Inf option vector size mismatch");
    Date today = Settings::instance().evaluationDate();
    std::string infIndex = data_->infIndex();

    Handle<CPICapFloorTermPriceSurface> infVol = market_->inflationCapFloorPriceSurface(infIndex, configuration_);
    QL_REQUIRE(!infVol.empty(), "Inf vol termstructure not found for " << infIndex);
        
    std::vector<Time> expiryTimes(data_->optionExpiries().size());
    optionBasket_.clear();
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        std::string expiryString = data_->optionExpiries()[j];
        // may wish to calibrate against specific futures expiry dates...
        Period expiry;
        Date expiryDate;
        bool isDate;
        parseDateOrPeriod(expiryString, expiryDate, expiry, isDate);
        if (!isDate)
            expiryDate = today + expiry;
        QL_REQUIRE(expiryDate > today, "expired calibration option expiry " << QuantLib::io::iso_date(expiryDate));
        Strike strike = parseStrike(data_->optionStrikes()[j]);

        Real strikeValue;
        QL_REQUIRE(strike.type == Strike::Type::Absolute, "DkBuilder: only fixed strikes supported, got "
            << data_->optionStrikes()[j]);
        strikeValue = strike.value;

        Real baseCPI = data_->baseCPI();
        Handle<ZeroInflationIndex> zInfIndex = market_->zeroInflationIndex(infIndex, configuration_);
        Calendar fixCalendar = zInfIndex->fixingCalendar();

        boost::shared_ptr<QuantExt::CpiCapFloorHelper> helper =
            boost::make_shared<QuantExt::CpiCapFloorHelper>(Option::Type::Call, baseCPI, expiryDate, fixCalendar,
                infVol->businessDayConvention(), fixCalendar, infVol->businessDayConvention(), strikeValue, zInfIndex,
                infVol->observationLag(), infVol->capPrice(expiry, strikeValue));

        optionBasket_.push_back(helper);
        helper->performCalculations();
        expiryTimes[j] = inflationYearFraction(zInfIndex->frequency(), zInfIndex->interpolated(),
            zInfIndex->zeroInflationTermStructure()->dayCounter(), zInfIndex->zeroInflationTermStructure()->baseDate(), 
            expiryDate);
        LOG("Added InflationOptionHelper " << infIndex << " " << expiry);
    }

    std::sort(expiryTimes.begin(), expiryTimes.end());
    auto itExpiryTime = unique(expiryTimes.begin(), expiryTimes.end());
    expiryTimes.resize(distance(expiryTimes.begin(), itExpiryTime));

    optionExpiries_ = Array(expiryTimes.size());
    for (Size j = 0; j < expiryTimes.size(); j++)
        optionExpiries_[j] = expiryTimes[j];
}
} // namespace data
} // namespace ore
