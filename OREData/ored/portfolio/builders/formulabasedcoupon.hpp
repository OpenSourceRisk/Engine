/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ql/cashflows/couponpricer.hpp>

namespace ore {
namespace data {

using namespace QuantExt;
using namespace QuantLib;

class FormulaBasedCouponPricerBuilder
    : public ore::data::CachingCouponPricerBuilder<
          std::string, const std::string&, const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::IborCouponPricer>>&,
          const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::CmsCouponPricer>>&,
          const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>&> {
public:
    FormulaBasedCouponPricerBuilder() : CachingEngineBuilder("BrigoMercurio", "MC", {"FormulaBasedCoupon"}) {}

protected:
    virtual std::string
    keyImpl(const std::string& paymentCcy, const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::IborCouponPricer>>&,
            const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::CmsCouponPricer>>&,
            const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexMaps) override {
        std::string key = paymentCcy;
        for (auto const& i : indexMaps) {
            key += ":" + i.first;
        }
        return key;
    }
    virtual QuantLib::ext::shared_ptr<FloatingRateCouponPricer>
    engineImpl(const std::string& paymentCcy,
               const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::IborCouponPricer>>& iborPricers,
               const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::CmsCouponPricer>>& cmsPricers,
               const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexMaps) override;
};

} // namespace data
} // namespace ore
