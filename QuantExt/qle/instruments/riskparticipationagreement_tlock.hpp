/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file riskparticipationagreement_tlock.hpp
    \brief RPA instrument for tlock underlyings
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/instrument.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/position.hpp>
#include <ql/pricingengine.hpp>

namespace QuantExt {

using namespace QuantLib;

class RiskParticipationAgreementTLock : public QuantLib::Instrument {
public:
    class arguments;
    class results;
    class engine;
    /*! The bond must be a fixed rate bond, i.e. it may only contain FixedCoupons.
        The udnerlying payout is (referenceRate - bond yield) * DV01 if payer = false, otherwise multiplied by -1.
        As in the swap RPA, protectionFeepayer = true means protection is received, protection fee is paid. */
    RiskParticipationAgreementTLock(const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond, Real bondNotional, bool payer,
                                    Real referenceRate, const DayCounter& dayCounter, const Date& terminationDate,
                                    const Date& paymentDate, const std::vector<Leg>& protectionFee,
                                    const bool protectionFeePayer, const std::vector<std::string>& protectionFeeCcys,
                                    const Real participationRate, const Date& protectionStart,
                                    const Date& protectionEnd, const bool settlesAccrual,
                                    const Real fixedRecoveryRate = Null<Real>());

    //! Instrument interface
    bool isExpired() const override;

    //! Inspectors
    const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond() const { return bond_; }
    bool payer() { return payer_; }
    Real referenceRate() const { return referenceRate_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const Date& terminationDate() const { return terminationDate_; }
    const Date& paymentDate() const { return paymentDate_; }

    const std::vector<Leg>& protectionFee() const { return protectionFee_; }
    bool protectionFeePayer() const { return protectionFeePayer_; }
    const std::vector<std::string>& protectionFeeCcys() const { return protectionFeeCcys_; }
    Real participationRate() const { return participationRate_; }
    const Date& protectionStart() const { return protectionStart_; }
    const Date& protectionEnd() const { return protectionEnd_; }
    bool settlesAccrual() const { return settlesAccrual_; }
    Real fixedRecoveryRate() const { return fixedRecoveryRate_; }

    // maturity = max protection end, last prot fee pay date
    const Date& maturity() const { return maturity_; }

private:
    void setupArguments(QuantLib::PricingEngine::arguments*) const override;
    void setupExpired() const override;
    void fetchResults(const QuantLib::PricingEngine::results*) const override;

    // underlying
    QuantLib::ext::shared_ptr<QuantLib::Bond> bond_;
    Real bondNotional_;
    bool payer_;
    Real referenceRate_;
    DayCounter dayCounter_;
    Date terminationDate_;
    Date paymentDate_;

    // protection data
    std::vector<Leg> protectionFee_;
    bool protectionFeePayer_;
    std::vector<std::string> protectionFeeCcys_;
    Real participationRate_;
    Date protectionStart_, protectionEnd_;
    bool settlesAccrual_;
    Real fixedRecoveryRate_;

    Date maturity_;
};

class RiskParticipationAgreementTLock::arguments : public PricingEngine::arguments {
public:
    arguments()
        : payer(false), referenceRate(0.0), protectionFeePayer(false), participationRate(0.0), settlesAccrual(false),
          fixedRecoveryRate(0.0) {}
    void validate() const override {}

    // underlying
    QuantLib::ext::shared_ptr<QuantLib::Bond> bond;
    Real bondNotional;
    bool payer;
    Real referenceRate;
    DayCounter dayCounter;
    Date terminationDate;
    Date paymentDate;

    // protection data
    std::vector<Leg> protectionFee;
    bool protectionFeePayer;
    std::vector<std::string> protectionFeeCcys;
    Real participationRate;
    Date protectionStart, protectionEnd;
    bool settlesAccrual;
    Real fixedRecoveryRate;

    // maturity
    Date maturity;
};

class RiskParticipationAgreementTLock::results : public Instrument::results {
public:
    void reset() override { Instrument::results::reset(); }
};

class RiskParticipationAgreementTLock::engine
    : public GenericEngine<RiskParticipationAgreementTLock::arguments, RiskParticipationAgreementTLock::results> {};

} // namespace QuantExt
