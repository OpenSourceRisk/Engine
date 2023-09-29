/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/models/dummymodel.hpp
    \brief dummy model implementation
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/model.hpp>

namespace ore {
namespace data {

class DummyModel : public Model {
public:
    DummyModel(const Size n) : Model(n) {
        dummyResult_ = RandomVariable(n, 0.0);
        dummyResult_.set(0, 1.0); // make the result stochastic
    }
    Type type() const override { return Type::MC; }
    RandomVariable pay(const RandomVariable& amount, const Date& obsdate, const Date& paydate,
                       const std::string& currency) const override {
        return dummyResult_;
    }
    RandomVariable discount(const Date& obsdate, const Date& paydate, const std::string& currency) const override {
        return dummyResult_;
    }
    RandomVariable npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                       const boost::optional<long>& memSlot, const RandomVariable& addRegressor1,
                       const RandomVariable& addRegressor2) const override {
        return dummyResult_;
    }
    RandomVariable eval(const std::string& index, const Date& obsdate, const Date& fwdDate,
                        const bool returnMissingFixingAsNull, const bool ignoreTodaysFixing) const override {
        return dummyResult_;
    }
    virtual RandomVariable fwdCompAvg(const bool isAvg, const std::string& index, const Date& obsdate,
                                      const Date& start, const Date& end, const Real spread, const Real gearing,
                                      const Integer lookback, const Natural rateCutoff, const Natural fixingDays,
                                      const bool includeSpread, const Real cap, const Real floor,
                                      const bool nakedOption, const bool localCapFloor) const override {
        return dummyResult_;
    }
    RandomVariable barrierProbability(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                      const RandomVariable& barrier, const bool above) const override {
        return dummyResult_;
    }
    Real fxSpotT0(const std::string& forCcy, const std::string& domCcy) const override { return 1.0; }
    Real extractT0Result(const RandomVariable& result) const override { return 0.0; }
    const Date& referenceDate() const override {
        static const Date d;
        return d;
    }
    const std::string& baseCcy() const override {
        static const std::string b = "EUR";
        return b;
    }

private:
    RandomVariable dummyResult_;
};

} // namespace data
} // namespace ore
