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

#include <qle/models/eqbsconstantparametrization.hpp>
#include <qle/models/eqbspiecewiseconstantparametrization.hpp>
//#include <qle/pricingengines/analyticcclgmeqoptionengine.hpp>
#include <qle/models/fxoptionhelper.hpp>

#include <ored/model/eqbsbuilder.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

EqBsBuilder::EqBsBuilder(const boost::shared_ptr<ore::data::Market>& market, 
                         const boost::shared_ptr<EqBsData>& data,
                         const QuantLib::Currency& baseCcy,
                         const std::string& configuration)
    : market_(market), configuration_(configuration), 
    baseCcy_(baseCcy), data_(data) {

    QuantLib::Currency ccy = ore::data::parseCurrency(data->currency());
    string eqName = data->eqName();

    if (data->calibrateSigma())
        buildOptionBasket();

    Array sigmaTimes, sigma;
    if (data->sigmaParamType() == ParamType::Constant) {
        QL_REQUIRE(data->sigmaTimes().size() == 0, "empty sigma tme grid expected");
        QL_REQUIRE(data->sigmaValues().size() == 1, "initial sigma grid size 1 expected");
        sigmaTimes = Array(0);
        sigma = Array(data_->sigmaValues().begin(), data_->sigmaValues().end());
    } else {
        if (data->calibrateSigma()) { // override
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
    Handle<Quote> eqSpot = market_->equitySpot(eqName, configuration_);
    string ccyPair = ccy.code() + baseCcy.code();
    Handle<Quote> fxSpot = market_->fxSpot(ccyPair, configuration_);

    if (data->sigmaParamType() == ParamType::Piecewise)
        parametrization_ =
            boost::make_shared<QuantExt::EqBsPiecewiseConstantParametrization>(ccy, eqName, eqSpot, fxSpot, sigmaTimes, sigma);
    else if (data->sigmaParamType() == ParamType::Constant)
        parametrization_ = boost::make_shared<QuantExt::EqBsConstantParametrization>(ccy, eqName, eqSpot, fxSpot, sigma[0]);
    else
        QL_FAIL("interpolation type not supported for Equity");
}

// void FxBsBuilder::update() {
//     // nothing to do here
// }

void EqBsBuilder::buildOptionBasket() {
    QL_REQUIRE(data_->optionExpiries().size() == data_->optionStrikes().size(), "Eq option vector size mismatch");
    Date today = Settings::instance().evaluationDate();
    std::string eqCcy = data_->currency();
    std::string fxCcyPair = eqCcy + baseCcy_.code();
    std::string eqName = data_->eqName();
    Handle<Quote> eqSpot = market_->equitySpot(eqName, configuration_);
    QL_REQUIRE(!eqSpot.empty(), "Eq spot quote not found");
    Handle<Quote> fxSpot = market_->fxSpot(fxCcyPair, configuration_);
    QL_REQUIRE(!fxSpot.empty(), "fx spot quote not found");
    // using the "discount curve" here instead of the equityReferenceRateCurve?
    Handle<YieldTermStructure> ytsRate = market_->discountCurve(eqCcy, configuration_);
    QL_REQUIRE(!ytsRate.empty(), "equity IR termstructure not found for " << eqCcy);
    Handle<YieldTermStructure> ytsDiv = market_->equityDividendCurve(eqName, configuration_);
    QL_REQUIRE(!ytsDiv.empty(), "dividend yield termstructure not found for " << eqName);
    Handle<BlackVolTermStructure> eqVol = market_->equityVol(eqName, configuration_);
    QL_REQUIRE(!eqVol.empty(), "Eq vol termstructure not found for " << eqName);
    std::vector<Time> expiryTimes(data_->optionExpiries().size());
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        std::string expiryString = data_->optionExpiries()[j];
        // may wish to calibrate against specific futures expiry dates...
        Period expiry;
        Date expiryDate;
        bool isDate;
        parseDateOrPeriod(expiryString, expiryDate, expiry, isDate);
        if (!isDate)
            expiryDate = today + expiry;
        QL_REQUIRE(expiryDate > today, 
            "expired calibration option expiry " 
            << QuantLib::io::iso_date(expiryDate));
        ore::data::Strike strike = ore::data::parseStrike(data_->optionStrikes()[j]);
        Real strikeValue;
        // TODO: Extend strike type coverage
        if (strike.type == ore::data::Strike::Type::ATMF)
            strikeValue = Null<Real>();
        else if (strike.type == ore::data::Strike::Type::Absolute)
            strikeValue = strike.value;
        else
            QL_FAIL("strike type ATMF or Absolute expected");
        Handle<Quote> volQuote(boost::make_shared<SimpleQuote>(eqVol->blackVol(expiryDate, strikeValue)));
        boost::shared_ptr<QuantExt::FxOptionHelper> helper =
            boost::make_shared<QuantExt::FxOptionHelper>(expiryDate, strikeValue, eqSpot, volQuote, ytsRate, ytsDiv);
        optionBasket_.push_back(helper);
        helper->performCalculations();
        expiryTimes[j] = ytsRate->timeFromReference(helper->option()->exercise()->date(0));
        LOG("Added EquityOptionHelper " << eqName << " " << expiry << " " << volQuote->value());
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
