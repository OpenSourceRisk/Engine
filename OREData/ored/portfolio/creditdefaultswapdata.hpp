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

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>
#include <boost/optional.hpp>

namespace ore {
namespace data {

//! CDS debt tier enumeration
enum class CdsTier {
    SNRFOR,
    SUBLT2,
    SNRLAC,
    SECDOM,
    JRSUBUT2,
    PREFT1
};
CdsTier parseCdsTier(const std::string& s);
std::ostream& operator<<(std::ostream& out, const CdsTier& cdsTier);

//! CDS documentation clause enumeration
enum class CdsDocClause {
    CR,
    MM,
    MR,
    XR,
    CR14,
    MM14,
    MR14,
    XR14
};
CdsDocClause parseCdsDocClause(const std::string& s);
std::ostream& operator<<(std::ostream& out, const CdsDocClause& cdsDocClause);

/*! Serializable reference information
    \ingroup tradedata
*/
class CdsReferenceInformation : public XMLSerializable {
public:
    //! Default constructor
    CdsReferenceInformation() {}

    //! Detailed constructor
    CdsReferenceInformation(const std::string& referenceEntityId,
        CdsTier tier,
        const QuantLib::Currency& currency,
        CdsDocClause docClause);

    //! \name XMLSerializable interface
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const std::string& referenceEntityId() const { return referenceEntityId_; }
    CdsTier tier() const { return tier_; }
    const QuantLib::Currency& currency() const { return currency_; }
    CdsDocClause docClause() const { return docClause_; }
    //@}

    /*! Give back the ID for the CdsReferenceInformation object. The id is the concatenation of the string 
        representation of the object's members using the \c | character as a delimiter.
    */
    const std::string& id() const { return id_; }

private:
    std::string referenceEntityId_;
    CdsTier tier_;
    QuantLib::Currency currency_;
    CdsDocClause docClause_;
    std::string id_;

    //! Populate the \c id_ member
    void populateId();
};

/*! Serializable credit default swap data
    \ingroup tradedata
*/
class CreditDefaultSwapData : public XMLSerializable {
public:
    //! Default constructor
    CreditDefaultSwapData() {}

    //! Constructor that takes an explicit \p creditCurveId
    CreditDefaultSwapData(const string& issuerId, const string& creditCurveId, const LegData& leg,
                          const bool settlesAccrual = true, const bool paysAtDefaultTime = true,
                          const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                          const Real upfrontFee = Null<Real>(),
                          QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>(),
                          const std::string& referenceObligation = "")
        : issuerId_(issuerId), creditCurveId_(creditCurveId), leg_(leg), settlesAccrual_(settlesAccrual),
          paysAtDefaultTime_(paysAtDefaultTime), protectionStart_(protectionStart), upfrontDate_(upfrontDate),
          upfrontFee_(upfrontFee), recoveryRate_(recoveryRate), referenceObligation_(referenceObligation) {}

    //! Constructor that takes a \p referenceInformation object
    CreditDefaultSwapData(const std::string& issuerId,
        const CdsReferenceInformation& referenceInformation,
        const LegData& leg,
        bool settlesAccrual = true,
        bool paysAtDefaultTime = true,
        const QuantLib::Date& protectionStart = QuantLib::Date(),
        const QuantLib::Date& upfrontDate = QuantLib::Date(),
        QuantLib::Real upfrontFee = QuantLib::Null<QuantLib::Real>(),
        QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>(),
        const std::string& referenceObligation = "");

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;

    const string& issuerId() const { return issuerId_; }
    const string& creditCurveId() const;
    const LegData& leg() const { return leg_; }
    bool settlesAccrual() const { return settlesAccrual_; }
    bool paysAtDefaultTime() const { return paysAtDefaultTime_; }
    const Date& protectionStart() const { return protectionStart_; }
    const Date& upfrontDate() const { return upfrontDate_; }
    Real upfrontFee() const { return upfrontFee_; }
    
    /*! If the CDS is a fixed recovery CDS, this returns the recovery rate. 
        For a standard CDS, it returns Null<Real>().
    */
    QuantLib::Real recoveryRate() const { return recoveryRate_; }

    //! CDS Reference Obligation
    const std::string& referenceObligation() const { return referenceObligation_; }

    /*! CDS reference information. This will be empty if an explicit credit curve ID has been used.
    */
    const boost::optional<CdsReferenceInformation>& referenceInformation() const { return referenceInformation_; }

private:
    string issuerId_;
    string creditCurveId_;
    LegData leg_;
    bool settlesAccrual_, paysAtDefaultTime_;
    Date protectionStart_, upfrontDate_;
    Real upfrontFee_;
    
    //! Populated if the CDS is a fixed recovery rate CDS, otherwise \c Null<Real>()
    QuantLib::Real recoveryRate_;

    std::string referenceObligation_;

    boost::optional<CdsReferenceInformation> referenceInformation_;
};

} // namespace data
} // namespace ore
