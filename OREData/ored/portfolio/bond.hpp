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
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradefactory.hpp>

#include <utility>

namespace ore {
namespace data {

/*! Serializable BondData
    FIXME zero bonds are only supported via the third constructor, but not in fromXML()
*/
class BondData : public XMLSerializable {
public:
    //! Default Contructor
    BondData() : hasCreditRisk_(true),
               faceAmount_(0.0), zeroBond_(false), bondNotional_(1.0), isPayer_(false), isInflationLinked_(false) {}

    //! Constructor to set up a bond from reference data
    BondData(string securityId, Real bondNotional, bool hasCreditRisk = true)
        : securityId_(securityId), hasCreditRisk_(hasCreditRisk), faceAmount_(0.0), zeroBond_(false),
          bondNotional_(bondNotional), isPayer_(false), isInflationLinked_(false) {}

    //! Constructor for coupon bonds
    BondData(string issuerId, string creditCurveId, string securityId, string referenceCurveId, string settlementDays,
             string calendar, string issueDate, LegData& coupons, bool hasCreditRisk = true)
        : issuerId_(issuerId), creditCurveId_(creditCurveId), securityId_(securityId),
          referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
          issueDate_(issueDate), coupons_(std::vector<LegData>{coupons}), hasCreditRisk_(hasCreditRisk), faceAmount_(0),
          zeroBond_(false), bondNotional_(1.0) {
        initialise();
    }

    //! Constructor for coupon bonds with multiple phases (represented as legs)
    BondData(string issuerId, string creditCurveId, string securityId, string referenceCurveId, string settlementDays,
             string calendar, string issueDate, const std::vector<LegData>& coupons, bool hasCreditRisk = true)
        : issuerId_(issuerId), creditCurveId_(creditCurveId), securityId_(securityId),
          referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
          issueDate_(issueDate), coupons_(coupons), hasCreditRisk_(hasCreditRisk), faceAmount_(0), zeroBond_(false),
          bondNotional_(1.0) {
        initialise();
    }

    //! Constructor for zero bonds, FIXME these can only be set up via this ctor, not via fromXML()
    BondData(string issuerId, string creditCurveId, string securityId, string referenceCurveId, string settlementDays,
             string calendar, Real faceAmount, string maturityDate, string currency, string issueDate,
             bool hasCreditRisk = true)
        : issuerId_(issuerId), creditCurveId_(creditCurveId), securityId_(securityId),
          referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
          issueDate_(issueDate), coupons_(), hasCreditRisk_(hasCreditRisk), faceAmount_(faceAmount),
          maturityDate_(maturityDate), currency_(currency), zeroBond_(true), bondNotional_(1.0) {
        initialise();
    }

    //! Inspectors
    const string& issuerId() const { return issuerId_; }
    const string& creditCurveId() const { return creditCurveId_; }
    const string& creditGroup() const { return creditGroup_; }
    const string& securityId() const { return securityId_; }
    const string& referenceCurveId() const { return referenceCurveId_; }
    const string& incomeCurveId() const { return incomeCurveId_; }
    const string& volatilityCurveId() const { return volatilityCurveId_; }
    const string& settlementDays() const { return settlementDays_; }
    const string& calendar() const { return calendar_; }
    const string& issueDate() const { return issueDate_; }
    QuantExt::BondIndex::PriceQuoteMethod priceQuoteMethod() const;
    Real priceQuoteBaseValue() const;
    const std::vector<LegData>& coupons() const { return coupons_; }
    const string& currency() const { return currency_; }
    Real bondNotional() const { return bondNotional_; }
    bool hasCreditRisk() const { return hasCreditRisk_; }
    bool isPayer() const { return isPayer_; }
    bool zeroBond() const { return zeroBond_; }
    bool isInflationLinked() const { return isInflationLinked_; }
    // only used for zero bonds
    Real faceAmount() const { return faceAmount_; }
    const string& maturityDate() const { return maturityDate_; }
    const string& subType() const { return subType_; }

    //! XMLSerializable interface
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    //! populate data from reference datum and check data for completeness
    void populateFromBondReferenceData(const QuantLib::ext::shared_ptr<BondReferenceDatum>& referenceDatum,
				       const std::string& startDate = "", const std::string& endDate = "");

    //! look up reference datum in ref data manager and populate, check data for completeness
    void populateFromBondReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
				       const std::string& startDate = "", const std::string& endDate = "");

    //! check data for completeness
    void checkData() const;

    //! return isda sub type "Single Name", "Index" or throw if sub type can not be mapped
    std::string isdaBaseProduct() const;

private:
    void initialise();
    string issuerId_;
    string creditCurveId_;
    string creditGroup_;
    string securityId_;
    string referenceCurveId_;
    string incomeCurveId_;     // only used for bond derivatives
    string volatilityCurveId_; // only used for bond derivatives
    string settlementDays_;
    string calendar_;
    string issueDate_;
    string priceQuoteMethod_;
    string priceQuoteBaseValue_;
    std::vector<LegData> coupons_;
    bool hasCreditRisk_;
    Real faceAmount_;     // only used for zero bonds
    string maturityDate_; // only used for for zero bonds
    string currency_;
    bool zeroBond_;
    Real bondNotional_;
    bool isPayer_;
    bool isInflationLinked_;
    string subType_;
};

//! Serializable Bond
/*!
\ingroup tradedata
*/
class Bond : public Trade {
public:
    //! Default Constructor
    explicit Bond() : Trade("Bond") {}

    //! Constructor taking an envelope and bond data
    Bond(Envelope env, const BondData& bondData)
        : Trade("Bond", env), originalBondData_(bondData), bondData_(bondData) {}

    //! Trade interface
    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! inspectors
    const BondData& bondData() const { return bondData_; }

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    //! XMLSerializable interface
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

private:
    BondData originalBondData_, bondData_;
};

//! Bond Factory that builds bonds from reference data

struct BondBuilder {
    struct Result {
        std::string builderLabel;
        QuantLib::ext::shared_ptr<QuantLib::Bond> bond;
        QuantLib::ext::shared_ptr<QuantExt::ModelBuilder> modelBuilder; // might be nullptr
        bool isInflationLinked = false;
        bool hasCreditRisk = true;
        std::string currency;
        std::string creditCurveId;
        std::string securityId;
        std::string creditGroup;
        QuantExt::BondIndex::PriceQuoteMethod priceQuoteMethod = QuantExt::BondIndex::PriceQuoteMethod::PercentageOfPar;
        double priceQuoteBaseValue = 1.0;

        double inflationFactor() const;
    };
    virtual ~BondBuilder() {}
    virtual Result build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                         const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                         const std::string& securityId) const = 0;
};

class BondFactory : public QuantLib::Singleton<BondFactory, std::integral_constant<bool, true>> {
    map<std::string, QuantLib::ext::shared_ptr<BondBuilder>> builders_;
    mutable boost::shared_mutex mutex_;

public:
    BondBuilder::Result build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                              const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                              const std::string& securityId) const;
    void addBuilder(const std::string& referenceDataType, const QuantLib::ext::shared_ptr<BondBuilder>& builder,
                    const bool allowOverwrite = false);
};

struct VanillaBondBuilder : public BondBuilder {
    virtual Result build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                         const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                         const std::string& securityId) const override;
};

} // namespace data
} // namespace ore
