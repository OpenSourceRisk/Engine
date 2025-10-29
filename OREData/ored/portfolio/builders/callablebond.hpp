/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file oredportfolio/builders/callablebond.hpp */

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <ql/time/date.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

class CallableBondEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<std::string, const std::string&, const std::string&,
                                                    const std::string&, const std::string&, const std::string&,
                                                    const std::string&, const QuantLib::Date&> {
protected:
    CallableBondEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"CallableBond"}) {}

    std::string keyImpl(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
                        const std::string& securityId, const std::string& referenceCurveId,
                        const std::string& incomeCurveId, const QuantLib::Date& maturityDate) override {
        return id;
    }
};

class CallableBondLgmEngineBuilder : public CallableBondEngineBuilder {
public:
    explicit CallableBondLgmEngineBuilder(const std::string& engine) : CallableBondEngineBuilder("LGM", engine) {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::IrModel> model(const std::string& id, const std::string& ccy,
                                                       const QuantLib::Date& maturityDate,
                                                       const bool generateAdditionalResults);

    // Args cover a list of FD resp. Grid engine parameters
    template <class... Args>
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    makeEngine(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
               const std::string& securityId, const std::string& referenceCurveId, const std::string& incomeCurveId,
               const QuantLib::Date& maturityDate, Args... args);
};

class CallableBondLgmFdEngineBuilder : public CallableBondLgmEngineBuilder {
public:
    CallableBondLgmFdEngineBuilder() : CallableBondLgmEngineBuilder("FD") {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::PricingEngine>
    engineImpl(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
               const std::string& securityId, const std::string& referenceCurveId, const std::string& incomeCurveId,
               const QuantLib::Date& maturityDate) override;
};

class CallableBondLgmGridEngineBuilder : public CallableBondLgmEngineBuilder {
public:
    CallableBondLgmGridEngineBuilder() : CallableBondLgmEngineBuilder("Grid") {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::PricingEngine>
    engineImpl(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
               const std::string& securityId, const std::string& referenceCurveId, const std::string& incomeCurveId,
               const QuantLib::Date& maturityDate) override;
};

} // namespace data
} // namespace ore
