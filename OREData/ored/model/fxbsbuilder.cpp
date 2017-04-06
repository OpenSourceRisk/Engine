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

#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/models/fxeqoptionhelper.hpp>

#include <ored/model/fxbsbuilder.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

FxBsBuilder::FxBsBuilder(const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<FxBsData>& data,
                         const std::string& configuration)
    : market_(market), configuration_(configuration), data_(data) {

    QuantLib::Currency ccy = ore::data::parseCurrency(data->foreignCcy());
    QuantLib::Currency domesticCcy = ore::data::parseCurrency(data->domesticCcy());

    if (data->calibrateSigma())
        buildOptionBasket();

    Array sigmaTimes, sigma;
    if (data->sigmaParamType() == ParamType::Constant) {
        QL_REQUIRE(data->sigmaTimes().size() == 0, "empty sigma tme grid expected");
        QL_REQUIRE(data->sigmaValues().size() == 1, "initial sigma grid size 1 expected");
        sigmaTimes = Array(0);
        sigma = Array(data_->sigmaValues().begin(), data_->sigmaValues().end());
    } else {
        if (data->calibrateSigma() && data->calibrationType() == CalibrationType::Bootstrap) { // override
            QL_REQUIRE(optionExpiries_.size() > 0, "optionExpiries is empty");
            sigmaTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
            sigma = Array(sigmaTimes.size() + 1, data->sigmaValues()[0]);
        } else {
            // use input time grid and input alpha array otherwise
            sigmaTimes = Array(data_->sigmaTimes().begin(), data_->sigmaTimes().end());
            sigma = Array(data_->sigmaValues().begin(), data_->sigmaValues().end());
            QL_REQUIRE(sigma.size() == sigmaTimes.size() + 1, "sigma grids do not match");
        }
    }

    // Quotation needs to be consistent with FX spot quotation in the FX calibration basket
    Handle<Quote> fxSpot = market_->fxSpot(ccy.code() + domesticCcy.code(), configuration_);

    if (data->sigmaParamType() == ParamType::Piecewise)
        parametrization_ =
            boost::make_shared<QuantExt::FxBsPiecewiseConstantParametrization>(ccy, fxSpot, sigmaTimes, sigma);
    else if (data->sigmaParamType() == ParamType::Constant)
        parametrization_ = boost::make_shared<QuantExt::FxBsConstantParametrization>(ccy, fxSpot, sigma[0]);
    else
        QL_FAIL("interpolation type not supported for FX");
}

void FxBsBuilder::update() {
    ;// nothing to do here
}

void FxBsBuilder::buildOptionBasket() {
    QL_REQUIRE(data_->optionExpiries().size() == data_->optionStrikes().size(), "fx option vector size mismatch");
    Date today = Settings::instance().evaluationDate();
    std::string domesticCcy = data_->domesticCcy();
    std::string ccy = data_->foreignCcy();
    std::string ccyPair = ccy + domesticCcy;
    Handle<Quote> fxSpot = market_->fxSpot(ccyPair, configuration_);
    QL_REQUIRE(!fxSpot.empty(), "FX spot quote not found");
    Handle<YieldTermStructure> ytsDom = market_->discountCurve(domesticCcy, configuration_);
    QL_REQUIRE(!ytsDom.empty(), "domestic yield termstructure not found");
    Handle<YieldTermStructure> ytsFor = market_->discountCurve(ccy, configuration_);
    QL_REQUIRE(!ytsFor.empty(), "foreign yield termstructure not found");
    Handle<BlackVolTermStructure> fxVol = market_->fxVol(ccyPair, configuration_);
    QL_REQUIRE(!fxVol.empty(), "fx vol termstructure not found");
    std::vector<Time> expiryTimes(data_->optionExpiries().size());
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        std::string expiryString = data_->optionExpiries()[j];
        Period expiry = ore::data::parsePeriod(expiryString);
        Date expiryDate = today + expiry;
        ore::data::Strike strike = ore::data::parseStrike(data_->optionStrikes()[j]);
        Real strikeValue;
        // TODO: Extend strike type coverage
        if (strike.type == ore::data::Strike::Type::ATMF)
            strikeValue = Null<Real>();
        else if (strike.type == ore::data::Strike::Type::Absolute)
            strikeValue = strike.value;
        else
            QL_FAIL("strike type ATMF or Absolute expected");
        Handle<Quote> quote(boost::make_shared<SimpleQuote>(fxVol->blackVol(expiryDate, strikeValue)));
        boost::shared_ptr<QuantExt::FxEqOptionHelper> helper =
            boost::make_shared<QuantExt::FxEqOptionHelper>(expiryDate, strikeValue, fxSpot, quote, ytsDom, ytsFor);
        optionBasket_.push_back(helper);
        helper->performCalculations();
        expiryTimes[j] = ytsDom->timeFromReference(helper->option()->exercise()->date(0));
        LOG("Added FxEqOptionHelper " << ccyPair << " " << expiry << " " << quote->value());
    }

    std::sort(expiryTimes.begin(), expiryTimes.end());
    auto itExpiryTime = unique(expiryTimes.begin(), expiryTimes.end());
    expiryTimes.resize(distance(expiryTimes.begin(), itExpiryTime));

    optionExpiries_ = Array(expiryTimes.size());
    for (Size j = 0; j < expiryTimes.size(); j++)
        optionExpiries_[j] = expiryTimes[j];
}
}
}
