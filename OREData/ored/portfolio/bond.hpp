/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file portfolio/bond.hpp
 \brief Bond trade data model and serialization
 \ingroup tradedata
 */
#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable BondData
/*!
\ingroup tradedata
*/
class BondData : public XMLSerializable {
public:
    //! Default Contructor
    BondData() : faceAmount_(0.0), zeroBond_(false), bondNotional_(1.0) {}

    //! Constructor to set up a bond from reference data
    BondData(string securityId, Real bondNotional)
        : securityId_(securityId), faceAmount_(0.0), zeroBond_(false), bondNotional_(bondNotional) {}

    //! Constructor for coupon bonds
    BondData(string issuerId, string creditCurveId, string securityId, string referenceCurveId, string settlementDays,
             string calendar, string issueDate, LegData& coupons)
        : issuerId_(issuerId), creditCurveId_(creditCurveId), securityId_(securityId),
          referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
          issueDate_(issueDate), coupons_(std::vector<LegData>{coupons}), faceAmount_(0), zeroBond_(false),
          bondNotional_(1.0) {}

    //! Constructor for coupon bonds with multiple phases (represented as legs)
    BondData(string issuerId, string creditCurveId, string securityId, string referenceCurveId, string settlementDays,
             string calendar, string issueDate, const std::vector<LegData>& coupons)
        : issuerId_(issuerId), creditCurveId_(creditCurveId), securityId_(securityId),
          referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
          issueDate_(issueDate), coupons_(coupons), faceAmount_(0), zeroBond_(false), bondNotional_(1.0) {}

    //! Constructor for zero bonds, FIXME these can only be set up via this ctor, not via fromXML()
    BondData(string issuerId, string creditCurveId, string securityId, string referenceCurveId, string settlementDays,
             string calendar, Real faceAmount, string maturityDate, string currency, string issueDate)
        : issuerId_(issuerId), creditCurveId_(creditCurveId), securityId_(securityId),
          referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
          issueDate_(issueDate), coupons_(), faceAmount_(faceAmount), maturityDate_(maturityDate), currency_(currency),
          zeroBond_(true), bondNotional_(1.0) {}

    //! Inspectors
    const string& issuerId() const { return issuerId_; }
    const string& creditCurveId() const { return creditCurveId_; }
    const string& securityId() const { return securityId_; }
    const string& referenceCurveId() const { return referenceCurveId_; }
    const string& incomeCurveId() const { return incomeCurveId_; }
    const string& volatilityCurveId() const { return volatilityCurveId_; }
    const string& settlementDays() const { return settlementDays_; }
    const string& calendar() const { return calendar_; }
    const string& issueDate() const { return issueDate_; }
    const std::vector<LegData>& coupons() const { return coupons_; }
    Real faceAmount() const { return faceAmount_; }
    const string& maturityDate() const { return maturityDate_; }
    const string& currency() const { return currency_; }
    bool zeroBond() const { return zeroBond_; }
    Real bondNotional() const { return bondNotional_; }

    //! XMLSerializable interface
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;

    //! populate data from reference data
    void populateFromBondReferenceData(const boost::shared_ptr<ReferenceDataManager>& referenceData);

private:
    string issuerId_;
    string creditCurveId_;
    string securityId_;
    string referenceCurveId_;
    string incomeCurveId_;     // only used for bond derivatives
    string volatilityCurveId_; // only used for bond derivatives
    string settlementDays_;
    string calendar_;
    string issueDate_;
    std::vector<LegData> coupons_;
    Real faceAmount_;     // only used for zero bonds
    string maturityDate_; // only used for for zero bonds
    string currency_;     // only used for for zero bonds
    bool zeroBond_;
    Real bondNotional_;
};

//! Serializable Bond
/*!
\ingroup tradedata
*/
class Bond : public Trade {
public:
    //! Default Contructor
    explicit Bond() : Trade("Bond") {}

    //! Constructor taking an envelope and bond data
    Bond(Envelope env, const BondData& bondData) : Trade("Bond", env), bondData_(bondData) {}

    //! Trade interface
    virtual void build(const boost::shared_ptr<EngineFactory>&) override;

    //! inspectors
    const BondData bondData() const { return bondData_; }
    const string& currency() const { return currency_; }
    // FIXE remove this, replace by bondData()->creditCurveId()
    const string& creditCurveId() const { return bondData_.creditCurveId(); }

    //! XMLSerializable interface
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;

private:
    BondData bondData_;
    string currency_;
};

} // namespace data
} // namespace ore
