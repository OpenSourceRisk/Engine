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

/*! \file portfolio/bondfuture.hpp
 \brief Bond trade data model and serialization
 \ingroup tradedata
 */
#pragma once

#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ql/instruments/bond.hpp>

namespace ore {
namespace data {

enum FutureType { ShortTenor, LongTenor };

void populateFromBondFutureReferenceData(string& currency, string& contractMonth, string& deliverableGrade,
                                         string& fairPrice, string& settlement, string& settlementDirty,
                                         string& rootDate, string& expiryBasis, string& settlementBasis,
                                         string& expiryLag, string& settlementLag, string& lastTrading,
                                         string& lastDelivery, vector<string>& secList,
                                         const ext::shared_ptr<BondFutureReferenceDatum>& bondFutureRefData);

class BondFuture : public Trade {
public:
    //! Default constructor
    BondFuture() : Trade("BondFuture") {}

    //! Constructor to set up a bondfuture from reference data
    BondFuture(const string& contractName, Real contractNotional, const string& longShort = "Long",
               Envelope env = Envelope())
        : Trade("BondFuture", env), contractName_(contractName), contractNotional_(contractNotional),
          longShort_(longShort) {}

    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    //! Add underlying Bond names
    // std::map<AssetClass, std::set<std::string>>
    // underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const
    // override;

    //! inspectors
    const BondBuilder::Result& ctdUnderlying() const { return ctdUnderlying_; }
    const string& ctdId() const { return ctdId_; }
    const string& currency() const { return currency_; }
    const bool fairPrice() const { return fairPriceBool_; }

    void populateFromBondFutureReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData);

protected:
    void checkData();

    void deduceDates(QuantLib::Date& expiry, QuantLib::Date& settlement);

    FutureType selectTypeUS(const std::string& value);

    void checkDates(const QuantLib::Date& expiry, const QuantLib::Date& settlement);

    std::pair<std::string, double> identifyCtdBond(const ext::shared_ptr<EngineFactory>& engineFactory,
                                                   const Date& expiry);

    const double getSettlementPriceFuture(const ext::shared_ptr<EngineFactory>& engineFactory) const;

    double conversionfactor_usd(double coupon, const FutureType& type, const Date& bondMaturity,
                                const Date& futureExpiry);

private:
    // mandatory information
    std::string contractName_;
    std::string currency_;
    double contractNotional_;
    std::string longShort_;

    // can be in reference data
    std::vector<std::string> secList_; // list of DeliveryBasket securities
    std::string deliverableGrade_;     // futureType differentiating the underlying -> USD conversion factor derivation
    std::string lastTrading_;          // expiry
    std::string lastDelivery_;         // settlement date

    // flags with fallbacks
    std::string fairPrice_; // indicates whether strike = 0 (false) or settlement price (true)
    bool fairPriceBool_;
    std::string settlement_;      // Cash or Physical
    std::string settlementDirty_; // true (dirty) or false (clean)

    // bond future date conventions to derive lastTrading and lastDelivery
    std::string contractMonth_;
    std::string rootDate_;        // first, end, nth weekday (e.g. 'Monday,3') taken
    std::string expiryBasis_;     // ROOT, SETTLEMENT taken
    std::string settlementBasis_; // ROOT, EXPIRY taken
    std::string expiryLag_;       // periods taken
    std::string settlementLag_;   // periods taken

    BondBuilder::Result ctdUnderlying_;
    std::string ctdId_;
};

struct BondFutureBuilder : public BondBuilder {
    virtual Result build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                         const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                         const std::string& contractName) const override;

    void modifyToForwardBond(const Date& expiry, QuantLib::ext::shared_ptr<QuantLib::Bond>& bond,
                             const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                             const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                             const std::string& securityId) const override;
};

} // namespace data
} // namespace ore
