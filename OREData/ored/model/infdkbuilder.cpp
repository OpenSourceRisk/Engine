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

#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/infdkparametrization.hpp>

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
    //  QL_REQUIRE(data_->aParamType() == ParamType::Piecewise, "DkBuilder, only piecewise volatility is supported
    //  currently");

    if (data_->aParamType() == ParamType::Constant) {
        QL_REQUIRE(data_->aTimes().size() == 0, "empty alpha times expected");
        QL_REQUIRE(data_->aValues().size() == 1, "initial alpha array should have size 1");
    } else if (data_->aParamType() == ParamType::Piecewise) {
        if (data_->calibrateA() && data_->calibrationType() == CalibrationType::Bootstrap) { // override
            if (data_->aTimes().size() > 0) {
                LOG("overriding alpha time grid with option expiries");
            }
            QL_REQUIRE(optionExpiries_.size() > 0, "empty option expiries");
            aTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
            alpha = Array(aTimes.size() + 1, data_->aValues()[0]);
        } else { // use input time grid and input alpha array otherwise
            QL_REQUIRE(alpha.size() == aTimes.size() + 1, "alpha grids do not match");
        }
    } else
        QL_FAIL("Alpha type case not covered");

    if (data_->hParamType() == ParamType::Constant) {
        QL_REQUIRE(data_->hValues().size() == 1, "reversion grid size 1 expected");
        QL_REQUIRE(data_->hTimes().size() == 0, "empty reversion time grid expected");
    } else if (data_->hParamType() == ParamType::Piecewise) {
        if (data_->calibrateH() && data_->calibrationType() == CalibrationType::Bootstrap) { // override
            if (data_->hTimes().size() > 0) {
                LOG("overriding H time grid with option expiries");
            }
            QL_REQUIRE(optionExpiries_.size() > 0, "empty option expiries");
            hTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
            h = Array(hTimes.size() + 1, data_->hValues()[0]);
        } else { // use input time grid and input alpha array otherwise
            QL_REQUIRE(h.size() == hTimes.size() + 1, "H grids do not match");
        }
    } else
        QL_FAIL("H type case not covered");

    LOG("before calibration: alpha times = " << aTimes << " values = " << alpha);
    LOG("before calibration:     h times = " << hTimes << " values = " << h)

    if (data_->reversionType() == LgmData::ReversionType::HullWhite &&
        data_->volatilityType() == LgmData::VolatilityType::HullWhite) {
        LOG("INF parametrization: InfDkPiecewiseConstantHullWhiteAdaptor");
        parametrization_ = boost::make_shared<InfDkPiecewiseConstantHullWhiteAdaptor>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h,
            data_->infIndex());
    } else if (data_->reversionType() == LgmData::ReversionType::HullWhite) {
        LOG("INF parametrization for " << data_->infIndex() << ": InfDkPiecewiseConstant");
        parametrization_ = boost::make_shared<InfDkPiecewiseConstantParametrization>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h,
            data_->infIndex());
    } else {
        parametrization_ = boost::make_shared<InfDkPiecewiseLinearParametrization>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h,
            data_->infIndex());
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
        Period expiry = parsePeriod(expiryString);
        Date expiryDate = inflationIndex_->fixingCalendar().advance(today, expiry);

        QL_REQUIRE(expiryDate > today, "expired calibration option expiry " << QuantLib::io::iso_date(expiryDate));
        Strike strike = parseStrike(data_->optionStrikes()[j]);

        Real strikeValue;
        QL_REQUIRE(strike.type == Strike::Type::Absolute,
                   "DkBuilder: only fixed strikes supported, got " << data_->optionStrikes()[j]);
        strikeValue = strike.value;

        Handle<ZeroInflationIndex> zInfIndex = market_->zeroInflationIndex(infIndex, configuration_);
        Calendar fixCalendar = zInfIndex->fixingCalendar();
        Date baseDate = zInfIndex->zeroInflationTermStructure()->baseDate();
        Real baseCPI = zInfIndex->fixing(baseDate);

        Option::Type capfloor;
        Real marketPrem;
        if (data_->capFloor() == "Cap") {
            capfloor = Option::Type::Call;
            marketPrem = infVol->capPrice(expiry, strikeValue);
        } else {
            capfloor = Option::Type::Put;
            marketPrem = infVol->floorPrice(expiry, strikeValue);
        }

        boost::shared_ptr<QuantExt::CpiCapFloorHelper> helper = boost::make_shared<QuantExt::CpiCapFloorHelper>(
            capfloor, baseCPI, expiryDate, fixCalendar, infVol->businessDayConvention(), fixCalendar,
            infVol->businessDayConvention(), strikeValue, zInfIndex, infVol->observationLag(), marketPrem);

        optionBasket_.push_back(helper);
        helper->performCalculations();
        expiryTimes[j] = inflationYearFraction(zInfIndex->frequency(), zInfIndex->interpolated(),
                                               zInfIndex->zeroInflationTermStructure()->dayCounter(), baseDate,
                                               helper->instrument()->fixingDate());
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
