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

/*! \file ored/portfolio/builders/riskparticipationagreement.hpp
    \brief
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/riskparticipationagreement.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

//! RPA base engine builder
class RiskParticipationAgreementEngineBuilderBase
    : public CachingPricingEngineBuilder<string, const std::string&, RiskParticipationAgreement*> {
public:
    RiskParticipationAgreementEngineBuilderBase(const std::string& model, const std::string& engine,
                                                const std::set<std::string>& tradeTypes)
        : CachingEngineBuilder(model, engine, tradeTypes) {}

protected:
    virtual std::string keyImpl(const std::string& id, RiskParticipationAgreement* rpa) override { return id; }
    std::map<std::string, Handle<YieldTermStructure>> getDiscountCurves(RiskParticipationAgreement* rpa);
    std::map<std::string, Handle<Quote>> getFxSpots(RiskParticipationAgreement* rpa);
};

//! RPA Black engine builder
class RiskParticipationAgreementBlackEngineBuilder : public RiskParticipationAgreementEngineBuilderBase {
public:
    RiskParticipationAgreementBlackEngineBuilder()
        : RiskParticipationAgreementEngineBuilderBase("Black", "Analytic", {"RiskParticipationAgreement_Vanilla"}) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& id,
                                                          RiskParticipationAgreement* rpa) override;
};

//! RPA XCcy Black engine builder
class RiskParticipationAgreementXCcyBlackEngineBuilder : public RiskParticipationAgreementEngineBuilderBase {
public:
    RiskParticipationAgreementXCcyBlackEngineBuilder()
        : RiskParticipationAgreementEngineBuilderBase("Black", "Analytic",
                                                      {"RiskParticipationAgreement_Vanilla_XCcy"}) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& id,
                                                          RiskParticipationAgreement* rpa) override;
};

//! RPA Numeric LGM base builder
class RiskParticipationAgreementLGMGridEngineBuilder : public RiskParticipationAgreementEngineBuilderBase {
public:
    explicit RiskParticipationAgreementLGMGridEngineBuilder(const std::set<std::string>& tradeTypes)
        : RiskParticipationAgreementEngineBuilderBase("LGM", "Grid", tradeTypes) {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::LGM> model(const string& id, const string& key, const std::vector<Date>& expiries,
                                           const Date& maturity, const std::vector<Real>& strikes);
};

//! RPA Numeric LGM engine builder for swap underlyings
class RiskParticipationAgreementSwapLGMGridEngineBuilder : public RiskParticipationAgreementLGMGridEngineBuilder {
public:
    RiskParticipationAgreementSwapLGMGridEngineBuilder()
        : RiskParticipationAgreementLGMGridEngineBuilder(
              {"RiskParticipationAgreement_Vanilla", "RiskParticipationAgreement_Structured"}) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& id,
                                                          RiskParticipationAgreement* rpa) override;
};

//! RPA Numeric LGM engine builder for tlock underlyings
class RiskParticipationAgreementTLockLGMGridEngineBuilder : public RiskParticipationAgreementLGMGridEngineBuilder {
public:
    RiskParticipationAgreementTLockLGMGridEngineBuilder()
        : RiskParticipationAgreementLGMGridEngineBuilder({"RiskParticipationAgreement_TLock"}) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& id,
                                                          RiskParticipationAgreement* rpa) override;
};

} // namespace data
} // namespace ore
