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

/*! \file riskparticipationagreement.hpp
    \brief RPA instrument
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/exercise.hpp>
#include <ql/instrument.hpp>
#include <ql/position.hpp>
#include <ql/pricingengine.hpp>

namespace QuantExt {

using namespace QuantLib;

class RiskParticipationAgreement : public QuantLib::Instrument {
public:
    class arguments;
    class results;
    class engine;
    RiskParticipationAgreement(const std::vector<Leg>& underlying, const std::vector<bool>& underlyingPayer,
                               const std::vector<std::string>& underlyingCcys, const std::vector<Leg>& protectionFee,
                               const bool protectionFeePayer, const std::vector<std::string>& protectionFeeCcys,
                               const Real participationRate, const Date& protectionStart, const Date& protectionEnd,
                               const bool settlesAccrual, const Real fixedRecoveryRate = Null<Real>(),
                               const QuantLib::ext::shared_ptr<QuantLib::Exercise>& exercise = nullptr,
                               const bool exerciseIsLong = false, const std::vector<QuantLib::ext::shared_ptr<CashFlow>>& premium = std::vector<QuantLib::ext::shared_ptr<CashFlow>>(),
                               const bool nakedOption = false);
    //! Instrument interface
    bool isExpired() const override;

    //! Inspectors
    const std::vector<Leg>& underlying() const { return underlying_; }
    const std::vector<bool>& underlyingPayer() const { return underlyingPayer_; }
    const std::vector<std::string>& underlyingCcys() const { return underlyingCcys_; }
    const std::vector<Leg>& protectionFee() const { return protectionFee_; }
    bool protectionFeePayer() const { return protectionFeePayer_; }
    const std::vector<std::string>& protectionFeeCcys() const { return protectionFeeCcys_; }
    Real participationRate() const { return participationRate_; }
    const Date& protectionStart() const { return protectionStart_; }
    const Date& protectionEnd() const { return protectionEnd_; }
    bool settlesAccrual() const { return settlesAccrual_; }
    Real fixedRecoveryRate() const { return fixedRecoveryRate_; }
    const QuantLib::ext::shared_ptr<Exercise>& exercise() const { return exercise_; }
    const bool nakedOption() const { return nakedOption_; }
    //
    const Date& maturity() const { return maturity_; }
    const Date& underlyingMaturity() const { return underlyingMaturity_; }

private:
    void setupArguments(QuantLib::PricingEngine::arguments*) const override;
    void setupExpired() const override;
    void fetchResults(const QuantLib::PricingEngine::results*) const override;
    //
    const std::vector<Leg> underlying_;
    const std::vector<bool> underlyingPayer_;
    const std::vector<std::string> underlyingCcys_;
    const std::vector<Leg> protectionFee_;
    const bool protectionFeePayer_;
    const std::vector<std::string> protectionFeeCcys_;
    const Real participationRate_;
    const Date protectionStart_, protectionEnd_;
    const bool settlesAccrual_;
    const Real fixedRecoveryRate_;
    const QuantLib::ext::shared_ptr<Exercise> exercise_;
    const bool exerciseIsLong_;
    const std::vector<QuantLib::ext::shared_ptr<CashFlow>> premium_;
    const bool nakedOption_;

    //
    Date maturity_, underlyingMaturity_;
    // internal additional results that are passed back to the engine via the arguments
    // this might or might not be populated / used by an engine
    mutable Date optionRepresentationReferenceDate_;
    mutable std::vector<std::tuple<Date, Date, Date>> optionRepresentationPeriods_;
    mutable std::vector<QuantLib::ext::shared_ptr<Instrument>> optionRepresentation_;
    mutable std::vector<Real> optionMultiplier_;
};

class RiskParticipationAgreement::arguments : public PricingEngine::arguments {
public:
    arguments() : protectionFeePayer(false), participationRate(0.0), settlesAccrual(false), fixedRecoveryRate(0.0) {}
    void validate() const override {}
    std::vector<Leg> underlying;
    std::vector<bool> underlyingPayer;
    std::vector<std::string> underlyingCcys;
    std::vector<Leg> protectionFee;
    bool protectionFeePayer;
    std::vector<std::string> protectionFeeCcys;
    Real participationRate;
    Date protectionStart, protectionEnd, underlyingMaturity;
    bool settlesAccrual;
    Real fixedRecoveryRate;
    QuantLib::ext::shared_ptr<Exercise> exercise;
    bool exerciseIsLong;
    std::vector<QuantLib::ext::shared_ptr<CashFlow>> premium;
    bool nakedOption;
    std::vector<QuantLib::ext::shared_ptr<Instrument>> optionRepresentation;
    std::vector<Real> optionMultiplier;
    std::vector<std::tuple<Date, Date, Date>> optionRepresentationPeriods;
    Date optionRepresentationReferenceDate;
};

class RiskParticipationAgreement::results : public Instrument::results {
public:
    std::vector<QuantLib::ext::shared_ptr<Instrument>> optionRepresentation;
    std::vector<Real> optionMultiplier;
    std::vector<std::tuple<Date, Date, Date>> optionRepresentationPeriods;
    Date optionRepresentationReferenceDate;
    void reset() override {
        Instrument::results::reset();
        optionRepresentation.clear();
        optionMultiplier.clear();
        optionRepresentationPeriods.clear();
        optionRepresentationReferenceDate = Date();
    }
};

class RiskParticipationAgreement::engine
    : public GenericEngine<RiskParticipationAgreement::arguments, RiskParticipationAgreement::results> {};

} // namespace QuantExt
