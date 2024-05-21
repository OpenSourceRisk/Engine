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

/*! \file portfolio/creditdefaultswapdata.hpp
    \brief A class to hold credit default swap data
    \ingroup tradedata
 */

#pragma once

#include <boost/optional.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>

#include <ql/instruments/creditdefaultswap.hpp>

namespace ore {
namespace data {

//! CDS debt tier enumeration
enum class CdsTier { SNRFOR, SUBLT2, SNRLAC, SECDOM, JRSUBUT2, PREFT1, LIEN1, LIEN2, LIEN3 };
CdsTier parseCdsTier(const std::string& s);
std::ostream& operator<<(std::ostream& out, const CdsTier& cdsTier);

//! CDS documentation clause enumeration
enum class CdsDocClause { CR, MM, MR, XR, CR14, MM14, MR14, XR14 };
CdsDocClause parseCdsDocClause(const std::string& s);
std::ostream& operator<<(std::ostream& out, const CdsDocClause& cdsDocClause);

// TODO refactor to creditevents.hpp
//! ISDA CDS documentation rules set enumeration
enum class IsdaRulesDefinitions { y2003 = 2003, y2014 = 2014 };
IsdaRulesDefinitions parseIsdaRulesDefinitions(const std::string& s);
std::ostream& operator<<(std::ostream& out, const CdsDocClause& cdsDocClause);
IsdaRulesDefinitions isdaRulesDefinitionsFromDocClause(const CdsDocClause& cdsDocClause);

//! ISDA credit event types enumeration
enum class CreditEventType {
    BANKRUPTCY,
    FAILURE_TO_PAY,
    RESTRUCTURING,
    OBLIGATION_ACCELERATION,
    OBLIGATION_DEFAULT,
    REPUDIATION_MORATORIUM,
    GOVERNMENTAL_INTERVENTION
};
CreditEventType parseCreditEventType(const std::string& s);
std::ostream& operator<<(std::ostream& out, const CreditEventType& creditEventType);
bool isTriggeredDocClause(CdsDocClause contractDocClause, CreditEventType creditEventType);

//! ISDA credit event seniority sets enumeration
enum class CreditEventTiers { SNR, SUB, SNRLAC, SNR_SUB, SNR_SNRLAC, SUB_SNRLAC, SNR_SUB_SNRLAC };
CreditEventTiers parseCreditEventTiers(const std::string& s);
std::ostream& operator<<(std::ostream& out, const CreditEventTiers& creditEventTiers);
bool isAuctionedSeniority(CdsTier contractTier, CreditEventTiers creditEventTiers);
// end TODO refactor to creditevents.hpp

/*! Serializable reference information
    \ingroup tradedata
*/
class CdsReferenceInformation : public XMLSerializable {
public:
    //! Default constructor
    CdsReferenceInformation();

    //! Detailed constructor
    CdsReferenceInformation(const std::string& referenceEntityId, CdsTier tier, const QuantLib::Currency& currency,
                            boost::optional<CdsDocClause> docClause = boost::none);

    //! \name XMLSerializable interface
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const std::string& referenceEntityId() const { return referenceEntityId_; }
    CdsTier tier() const { return tier_; }
    const QuantLib::Currency& currency() const { return currency_; }
    bool hasDocClause() const;
    CdsDocClause docClause() const;
    //@}

    /*! Give back the ID for the CdsReferenceInformation object. The id is the concatenation of the string
        representation of the object's members using the \c | character as a delimiter.
    */
    const std::string& id() const { return id_; }

private:
    std::string referenceEntityId_;
    CdsTier tier_;
    QuantLib::Currency currency_;
    boost::optional<CdsDocClause> docClause_;
    std::string id_;

    //! Populate the \c id_ member
    void populateId();
};

/*! Attempt to parse string to CdsReferenceInformation
    \param[in]  strInfo The string we wish to convert to CdsReferenceInformation
    \param[out] cdsInfo The resulting CdsReferenceInformation if the parsing was successful.

    \return \c true if the parsing was successful and \c false if not.

    If the function receives a \p strInfo of the form `ID|TIER|CCY|DOCCLAUSE` with `CCY` being a valid ISO currency
    code, `TIER` being a valid CDS debt tier and `DOCCLAUSE` being a valid CDS documentation clause, the parsing
    should be successful. Here, DOCCLAUSE is optional.

    \ingroup utilities
*/
bool tryParseCdsInformation(std::string strInfo, CdsReferenceInformation& cdsInfo);

/*! Serializable credit default swap data
    \ingroup tradedata
*/
class CreditDefaultSwapData : public XMLSerializable {
public:
    //! Default constructor
    CreditDefaultSwapData();

    using PPT = QuantLib::CreditDefaultSwap::ProtectionPaymentTime;

    //! Constructor that takes an explicit \p creditCurveId
    CreditDefaultSwapData(const string& issuerId, const string& creditCurveId, const LegData& leg,
                          const bool settlesAccrual = true,
                          const PPT protectionPaymentTime = PPT::atDefault,
                          const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                          const Real upfrontFee = Null<Real>(),
                          QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>(),
                          const std::string& referenceObligation = "",
                          const Date& tradeDate = Date(),
                          const std::string& cashSettlementDays = "",
			  const bool rebatesAccrual = true);

    //! Constructor that takes a \p referenceInformation object
    CreditDefaultSwapData(const std::string& issuerId, const CdsReferenceInformation& referenceInformation,
                          const LegData& leg, bool settlesAccrual = true,
                          const PPT protectionPaymentTime = PPT::atDefault,
                          const QuantLib::Date& protectionStart = QuantLib::Date(),
                          const QuantLib::Date& upfrontDate = QuantLib::Date(),
                          QuantLib::Real upfrontFee = QuantLib::Null<QuantLib::Real>(),
                          QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>(),
                          const std::string& referenceObligation = "",
                          const Date& tradeDate = Date(),
                          const std::string& cashSettlementDays = "",
			  const bool rebatesAccrual = true);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    const string& issuerId() const { return issuerId_; }
    const string& creditCurveId() const;
    const LegData& leg() const { return leg_; }
    bool settlesAccrual() const { return settlesAccrual_; }
    PPT protectionPaymentTime() const { return protectionPaymentTime_; }
    const Date& protectionStart() const { return protectionStart_; }
    const Date& upfrontDate() const { return upfrontDate_; }
    Real upfrontFee() const { return upfrontFee_; }
    bool rebatesAccrual() const { return rebatesAccrual_; }

    /*! If the CDS is a fixed recovery CDS, this returns the recovery rate.
        For a standard CDS, it returns Null<Real>().
    */
    QuantLib::Real recoveryRate() const { return recoveryRate_; }

    //! CDS Reference Obligation
    const std::string& referenceObligation() const { return referenceObligation_; }

    const QuantLib::Date& tradeDate() const { return tradeDate_; }
    QuantLib::Natural cashSettlementDays() const { return cashSettlementDays_; }

    /*! CDS reference information. This will be empty if an explicit credit curve ID has been used.
     */
    const boost::optional<CdsReferenceInformation>& referenceInformation() const { return referenceInformation_; }

protected:
    virtual void check(XMLNode* node) const;
    virtual XMLNode* alloc(XMLDocument& doc) const;

private:
    std::string issuerId_;
    std::string creditCurveId_;
    LegData leg_;
    bool settlesAccrual_;
    PPT protectionPaymentTime_;
    QuantLib::Date protectionStart_;
    QuantLib::Date upfrontDate_;
    QuantLib::Real upfrontFee_;
    bool rebatesAccrual_;

    //! Populated if the CDS is a fixed recovery rate CDS, otherwise \c Null<Real>()
    QuantLib::Real recoveryRate_;

    std::string referenceObligation_;
    QuantLib::Date tradeDate_;
    std::string strCashSettlementDays_;
    QuantLib::Natural cashSettlementDays_;

    boost::optional<CdsReferenceInformation> referenceInformation_;
};

} // namespace data
} // namespace ore
