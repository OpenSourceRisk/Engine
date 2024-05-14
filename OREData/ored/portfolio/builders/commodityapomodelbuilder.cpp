/*
  Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/commodityapomodelbuilder.hpp>

#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

namespace ore {
namespace data {

CommodityApoModelBuilder::CommodityApoModelBuilder(const Handle<YieldTermStructure>& curve,
                                                   const QuantLib::Handle<QuantLib::BlackVolTermStructure>& vol,
                                                   const QuantLib::ext::shared_ptr<QuantExt::CommodityAveragePriceOption>& apo,
                                                   const bool dontCalibrate)
    : BlackScholesModelBuilderBase(curve, QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
                                              Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0)),
                                              Handle<YieldTermStructure>(QuantLib::ext::make_shared<QuantLib::FlatForward>(
                                                  0, NullCalendar(), 0.0, Actual365Fixed())),
                                              Handle<YieldTermStructure>(QuantLib::ext::make_shared<QuantLib::FlatForward>(
                                                  0, NullCalendar(), 0.0, Actual365Fixed())),
                                              vol)),
      apo_(apo), dontCalibrate_(dontCalibrate) {}

void CommodityApoModelBuilder::setupDatesAndTimes() const {
    // we don't need the simulation dates or timegrid and we populate the curve / vol times directly below
    return;
}

std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>
CommodityApoModelBuilder::getCalibratedProcesses() const {
    // nothing to do, return original processes
    return processes_;
}

std::vector<std::vector<Real>> CommodityApoModelBuilder::getCurveTimes() const {
    // collect times relevant on the discount curve
    std::vector<Real> times;
    if (dontCalibrate_)
        return {times};
    if (apo_->underlyingFlow()->date() > curves_.front()->referenceDate())
        times.push_back(curves_.front()->timeFromReference(apo_->underlyingFlow()->date()));
    return {times};
}

std::vector<std::vector<std::pair<Real, Real>>> CommodityApoModelBuilder::getVolTimesStrikes() const {
    // collect times relevant on the vol surface
    std::vector<std::pair<Real, Real>> result;
    if (dontCalibrate_)
        return {result};
    auto vol = processes_.front()->blackVolatility();
    std::set<QuantLib::Date> expiries;
    Real effectiveStrike = apo_->effectiveStrike();
    try {
        effectiveStrike -= apo_->accrued(curves_.front()->referenceDate());
    } catch (...) {
        // The accrued calculation might fail due to missing fixings and this will cause an error in the
        // instrument pricing. We don't throw an error here since the apo might actually be expired so
        // that no pricing is required at all.
    }
    for (auto const& p : apo_->underlyingFlow()->indices()) {
        if (p.first > curves_.front()->referenceDate()) {
            if (apo_->underlyingFlow()->useFuturePrice()) {
                Date expiry = p.second->expiryDate();
                if (expiries.find(expiry) == expiries.end()) {
                    result.push_back(std::make_pair(vol->timeFromReference(expiry), effectiveStrike));
                    expiries.insert(expiry);
                }
            } else {
                if (expiries.find(p.first) == expiries.end()) {
                    result.push_back(std::make_pair(vol->timeFromReference(p.first), effectiveStrike));
                    expiries.insert(p.first);
                }
            }
        }
    }
    return {result};
}

} // namespace data
} // namespace ore
