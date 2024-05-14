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

/*! \file ored/portfolio/riskparticipationagreement.hpp
    \brief risk participation agreement data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/tlockdata.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>

#include <ql/instruments/bond.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

//! Serializable risk participation agreement
/*!
  \ingroup tradedata
*/
class RiskParticipationAgreement : public ore::data::Trade {
public:
    RiskParticipationAgreement() : Trade("RiskParticipationAgreement") {}
    //! Leg-based constructur, i.e. with Swap underyling
    RiskParticipationAgreement(const ore::data::Envelope& env, const std::vector<ore::data::LegData>& underlying,
                               const std::vector<ore::data::LegData>& protectionFee, const Real participationRate,
                               const Date& protectionStart, const Date& protectionEnd, const std::string& creditCurveId,
                               const std::string& issuerId = "", const bool settlesAccrual = true,
                               const Real fixedRecoveryRate = Null<Real>(),
                               const boost::optional<ore::data::OptionData>& optionData = boost::none)
        : Trade("RiskParticipationAgreement", env), underlying_(underlying), tlockData_(ore::data::TreasuryLockData()),
          protectionFee_(protectionFee), participationRate_(participationRate), protectionStart_(protectionStart),
          protectionEnd_(protectionEnd), creditCurveId_(creditCurveId), issuerId_(issuerId),
          settlesAccrual_(settlesAccrual), fixedRecoveryRate_(fixedRecoveryRate), optionData_(optionData) {}
    // Constructor with T-Lock underlying
    RiskParticipationAgreement(const ore::data::Envelope& env, const ore::data::TreasuryLockData& tlockData,
                               const std::vector<ore::data::LegData>& protectionFee, const Real participationRate,
                               const Date& protectionStart, const Date& protectionEnd, const std::string& creditCurveId,
                               const std::string& issuerId = "", const bool settlesAccrual = true,
                               const Real fixedRecoveryRate = Null<Real>())
        : Trade("RiskParticipationAgreement", env), tlockData_(tlockData), protectionFee_(protectionFee),
          participationRate_(participationRate), protectionStart_(protectionStart), protectionEnd_(protectionEnd),
          creditCurveId_(creditCurveId), issuerId_(issuerId), settlesAccrual_(settlesAccrual),
          fixedRecoveryRate_(fixedRecoveryRate) {}

    //! \name Inspectors
    //@{
    const std::vector<ore::data::LegData>& underlying() const { return underlying_; }
    const boost::optional<ore::data::OptionData>& optionData() const { return optionData_; }
    const ore::data::TreasuryLockData& tlockData() const { return tlockData_; }
    const std::vector<ore::data::LegData>& protectionFee() const { return protectionFee_; }
    Real participationRate() const { return participationRate_; }
    const Date& protectionStart() const { return protectionStart_; }
    const Date& protectionEnd() const { return protectionEnd_; }
    const std::string& creditCurveId() const { return creditCurveId_; }
    const std::string& issuerId() const { return issuerId_; }
    bool settlesAccrual() const { return settlesAccrual_; }
    Real fixedRecoveryRate() const { return fixedRecoveryRate_; }
    //@}

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

private:
    void buildWithSwapUnderlying(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory);
    void buildWithTlockUnderlying(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory);

    std::vector<ore::data::LegData> underlying_;
    ore::data::TreasuryLockData tlockData_;
    std::vector<ore::data::LegData> protectionFee_;
    Real participationRate_;
    Date protectionStart_, protectionEnd_;
    std::string creditCurveId_, issuerId_;
    bool settlesAccrual_;
    Real fixedRecoveryRate_;
    boost::optional<ore::data::OptionData> optionData_;
    bool nakedOption_;
};

} // namespace data
} // namespace ore
