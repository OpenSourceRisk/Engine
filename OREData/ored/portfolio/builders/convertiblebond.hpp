/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/convertiblebond.hpp */

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <ql/time/date.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {
using ore::data::CachingPricingEngineBuilder;

class ConvertibleBondEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<
          std::string, const std::string&, const std::string&, const std::string&, const bool, const std::string&,
          const std::string&, const bool, QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>,
          const QuantLib::ext::shared_ptr<QuantExt::FxIndex>&, const std::string&, const QuantLib::Date&,
          const QuantLib::Date&> {
protected:
    ConvertibleBondEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"ConvertibleBond"}) {}

    std::string keyImpl(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
                        const bool hasCreditRisk, const std::string& securityId, const std::string& referenceCurveId,
                        const bool isExchangeable, QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equity,
                        const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fx, const std::string& equityCreditCurveId,
                        const QuantLib::Date& startDate, const QuantLib::Date& maturityDate) override;
};

class ConvertibleBondFDDefaultableEquityJumpDiffusionEngineBuilder : public ConvertibleBondEngineBuilder {
public:
    ConvertibleBondFDDefaultableEquityJumpDiffusionEngineBuilder()
        : ConvertibleBondEngineBuilder("DefaultableEquityJumpDiffusion", "FD") {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::PricingEngine>
    engineImpl(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
               const bool hasCreditRisk, const std::string& securityId, const std::string& referenceCurveId,
               const bool isExchangeable, QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equity,
               const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fx, const std::string& equityCreditCurveId,
               const QuantLib::Date& startDate, const QuantLib::Date& maturityDate) override;
};

} // namespace data
} // namespace ore
