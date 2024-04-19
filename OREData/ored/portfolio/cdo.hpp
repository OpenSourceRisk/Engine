/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file portfolio/capfloor.hpp
    \brief Ibor cap, floor or collar trade data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/basketdata.hpp>

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable CDS Index Tranche (Synthetic CDO)
/*! \ingroup tradedata
 */


class SyntheticCDO : public Trade {
public:
  SyntheticCDO() : Trade("SyntheticCDO"),
		   attachmentPoint_(Null<Real>()), detachmentPoint_(Null<Real>()), settlesAccrual_(true),
		   protectionPaymentTime_(QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault),
		   upfrontFee_(Null<Real>()), rebatesAccrual_(true), recoveryRate_(Null<Real>()),
		   useSensitivitySimplification_(false) {}
    SyntheticCDO(
        const Envelope& env, const LegData& leg, const string& qualifier, const BasketData& basketData,
        double attachmentPoint, double detachmentPoint, const bool settlesAccrual = true,
        const QuantExt::CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
            QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault,
        const string& protectionStart = string(), const string& upfrontDate = string(),
        const Real upfrontFee = Null<Real>(), const bool rebatesAccrual = true, Real recoveryRate = Null<Real>())
        : Trade("SyntheticCDO", env), qualifier_(qualifier), legData_(leg), basketData_(basketData),
          attachmentPoint_(attachmentPoint), detachmentPoint_(detachmentPoint), settlesAccrual_(settlesAccrual),
          protectionPaymentTime_(protectionPaymentTime), protectionStart_(protectionStart), upfrontDate_(upfrontDate),
          upfrontFee_(upfrontFee), rebatesAccrual_(rebatesAccrual), recoveryRate_(recoveryRate),
	  useSensitivitySimplification_(false) {}

    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! Inspectors
    //@{
    const string& qualifier() const { return qualifier_; }
    const LegData& leg() const { return legData_; }
    const BasketData& basketData() const { return basketData_; }
    const double& attachmentPoint() const { return attachmentPoint_; }
    const double& detachmentPoint() const { return detachmentPoint_; }
    QuantExt::CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime() const { return protectionPaymentTime_; }
    const string& protectionStart() const { return protectionStart_; }
    const string& upfrontDate() const { return upfrontDate_; }
    const double& upfrontFee() const { return upfrontFee_; }
    bool settlesAccrual() const { return settlesAccrual_; }
    bool rebatesAccrual() const { return rebatesAccrual_; }
    const Real& recoveryRate() const { return recoveryRate_; }
    bool useSensitivitySimplification() const { return useSensitivitySimplification_; }
    const std::map<std::string, Real>& basketConstituents() const { return basketConstituents_; }
    //@}

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    static vector<Time>
    extractTimeGridDefaultCurve(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& dpts);
    QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure> static buildCalibratedConstiuentCurve(
        const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve,
        const QuantLib::ext::shared_ptr<SimpleQuote>& calibrationFactor);
    
    /*! Get credit curve id with term suffix "_5Y". If the creditCurveId contains such a suffix already
       this is used. Otherwise we try to imply it from the schedule. If that is not possible, the
       creditCurveId without tenor is returned. */
    std::string creditCurveIdWithTerm() const;

    /*! If set this is used to derive the term instead of the schedule start date. A concession to bad
        trade setups really, where the start date is not set to the index effective date */
    void setIndexStartDateHint(const QuantLib::Date& d) const { indexStartDateHint_ = d; }

    /*! Get the index start date hint, or null if it was never set */
    const QuantLib::Date& indexStartDateHint() const { return indexStartDateHint_; }

private:

    bool isIndexTranche() const { return qualifier_.size() == 13 && qualifier_.substr(0, 3) == "RED"; }

    string qualifier_;
    LegData legData_;
    BasketData basketData_;
    double attachmentPoint_;
    double detachmentPoint_;
    bool settlesAccrual_;
    QuantExt::CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime_;
    string protectionStart_;
    string upfrontDate_;
    double upfrontFee_;
    bool rebatesAccrual_;
    Real recoveryRate_;
    
    // Need these stored in case we use the simplification and need to recontribute the total sensitivities
    // to the underlyings in the basket.
    std::map<std::string, double> basketConstituents_;
    bool useSensitivitySimplification_;
    mutable QuantLib::Date indexStartDateHint_;
};
} // namespace data
} // namespace ore
