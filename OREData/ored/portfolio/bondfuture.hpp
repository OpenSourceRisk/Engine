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

namespace ore {
namespace data {

enum FutureType { ShortTenor, LongTenor };

void populateFromBondFutureReferenceData(std::string& currency, std::string& contractMonth,
                                         std::string& deliverableGrade, std::string& rootDate, std::string& expiryBasis,
                                         std::string& settlementBasis, std::string& expiryLag,
                                         std::string& settlementLag, std::string& lastTrading,
                                         std::string& lastDelivery, std::vector<std::string>& secList,
                                         const ext::shared_ptr<BondFutureReferenceDatum>& bondFutureRefData);

class BondFuture : public Trade {
public:
    //! Default constructor
    BondFuture() : Trade("BondFuture") {}

    //! Constructor taking an envelope
    BondFuture(Envelope env, const std::vector<std::string>& secList) : Trade("BondFuture", env), secList_(secList) {}

    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    //! Add underlying Bond names
    // std::map<AssetClass, std::set<std::string>>
    // underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const
    // override;

    //! inspectors
    // const std::vector<std::string>& secList() const { return secList_; }

protected:

    void checkData();

    void deduceDates(QuantLib::Date& expiry, QuantLib::Date& settlement);

    FutureType selectTypeUS(const std::string& value);

    void checkDates(const QuantLib::Date& expiry, const QuantLib::Date& settlement);

    std::string identifyCtdBond(const ext::shared_ptr<EngineFactory>& engineFactory);

    void populateFromBondFutureReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData);

private:
    // mandatory first tier information
    std::string contractName_; // can be used to identify tier 2/3 info
    std::vector<std::string> secList_;
    std::string currency_;
    double contractNotional_;
    std::string longShort_;

    // second tier information - both can be used in conjunction to identify tier 3 info
    std::string contractMonth_;
    std::string deliverableGrade_; // futureType differentiating the underlying
    // bond future date conventions
    std::string rootDate_;   // first, end, nth weekday (e.g. 'Monday,3') taken
    std::string expiryBasis_;     // root, expiry, settle taken
    std::string settlementBasis_; // root, expiry, settle taken
    std::string expiryLag_;       // periods taken
    std::string settlementLag_;   // periods taken

    // thirdt tier information
    std::string lastTrading_;  // expiry
    std::string lastDelivery_; // settlement date
};
} // namespace data
} // namespace ore
