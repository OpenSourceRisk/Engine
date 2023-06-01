/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/simm/crifrecord.hpp
    \brief Struct for holding a CRIF record
*/

#pragma once

#include <orea/simm/simmconfiguration.hpp>
#include <ored/portfolio/nettingsetdetails.hpp>

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <string>

namespace ore {
namespace analytics {

using ore::data::NettingSetDetails;

/*! A container for holding single CRIF records or aggregated CRIF records.
    A CRIF record is a row of the CRIF file outlined in the document:
    <em>ISDA SIMM Methodology, Risk Data Standards. Version 1.36: 1 February 2017.</em>
    or an updated version thereof.
*/
struct CrifRecord {

    // required data
    std::string tradeId;
    std::string portfolioId;
    SimmConfiguration::ProductClass productClass = SimmConfiguration::ProductClass::Empty;
    SimmConfiguration::RiskType riskType = SimmConfiguration::RiskType::Notional;
    std::string qualifier;
    std::string bucket;
    std::string label1;
    std::string label2;
    std::string amountCurrency;
    mutable QuantLib::Real amount = QuantLib::Null<QuantLib::Real>();
    mutable QuantLib::Real amountUsd = QuantLib::Null<QuantLib::Real>();

    // optional data
    std::string tradeType;
    std::string agreementType;
    std::string callType;
    std::string initialMarginType;
    std::string legalEntityId;
    NettingSetDetails nettingSetDetails; // consists of the above: agreementType ... legalEntityId
    mutable std::string imModel;
    mutable std::string collectRegulations;
    mutable std::string postRegulations;
    std::string endDate;

    // additional data
    std::map<std::string, std::string> additionalFields;

    // Default Constructor
    CrifRecord() {}

    CrifRecord(std::string tradeId, std::string tradeType, NettingSetDetails nettingSetDetails,
               SimmConfiguration::ProductClass productClass, SimmConfiguration::RiskType riskType,
               std::string qualifier, std::string bucket, std::string label1, std::string label2,
               std::string amountCurrency, QuantLib::Real amount, QuantLib::Real amountUsd, std::string imModel = "",
               std::string collectRegulations = "", std::string postRegulations = "", std::string endDate = "",
               std::map<std::string, std::string> additionalFields = {})
        : tradeId(tradeId), portfolioId(nettingSetDetails.nettingSetId()),
          productClass(productClass), riskType(riskType), qualifier(qualifier),
          bucket(bucket), label1(label1), label2(label2), amountCurrency(amountCurrency), amount(amount),
          amountUsd(amountUsd), tradeType(tradeType), nettingSetDetails(nettingSetDetails), imModel(imModel),
          collectRegulations(collectRegulations), postRegulations(postRegulations), endDate(endDate),
          additionalFields(additionalFields) {}

    CrifRecord(std::string tradeId, std::string tradeType, std::string portfolioId,
               SimmConfiguration::ProductClass productClass, SimmConfiguration::RiskType riskType,
               std::string qualifier, std::string bucket, std::string label1, std::string label2,
               std::string amountCurrency, QuantLib::Real amount, QuantLib::Real amountUsd, std::string imModel = "",
               std::string collectRegulations = "", std::string postRegulations = "", std::string endDate = "",
               std::map<std::string, std::string> additionalFields = {})
        : CrifRecord(tradeId, tradeType, NettingSetDetails(portfolioId), productClass, riskType, qualifier,
                     bucket, label1, label2, amountCurrency, amount, amountUsd, imModel,
                     collectRegulations, postRegulations, endDate, additionalFields) {}

    bool hasAmountCcy() const { return !amountCurrency.empty(); }
    bool hasAmount() const { return amount != QuantLib::Null<QuantLib::Real>(); }
    bool hasAmountUsd() const { return amountUsd != QuantLib::Null<QuantLib::Real>(); }

    // We use (and require) amountUsd for all risk types except for SIMM parameters AddOnNotionalFactor and
    // ProductClassMultiplier as these are multipliers and not amounts denominated in the amountCurrency
    bool requiresAmountUsd() const {
        return riskType != SimmConfiguration::RiskType::AddOnNotionalFactor &&
               riskType != SimmConfiguration::RiskType::ProductClassMultiplier;
    }

    bool isSimmParameter() const {
        return riskType == SimmConfiguration::RiskType::AddOnFixedAmount ||
               riskType == SimmConfiguration::RiskType::AddOnNotionalFactor ||
               riskType == SimmConfiguration::RiskType::ProductClassMultiplier;
    }

    //! Define how CRIF records are compared
    bool operator<(const CrifRecord& cr) const {
        return std::tie(tradeId, nettingSetDetails, productClass, riskType, qualifier, bucket, label1, label2,
                        amountCurrency, collectRegulations, postRegulations) <
               std::tie(cr.tradeId, cr.nettingSetDetails, cr.productClass, cr.riskType, cr.qualifier, cr.bucket,
                        cr.label1, cr.label2, cr.amountCurrency, cr.collectRegulations, cr.postRegulations);
    }
    static bool amountCcyLTCompare(const CrifRecord& cr1, const CrifRecord& cr2) {
        return std::tie(cr1.tradeId, cr1.nettingSetDetails, cr1.productClass, cr1.riskType, cr1.qualifier, cr1.bucket,
                        cr1.label1, cr1.label2, cr1.collectRegulations, cr1.postRegulations) <
               std::tie(cr2.tradeId, cr2.nettingSetDetails, cr2.productClass, cr2.riskType, cr2.qualifier, cr2.bucket,
                        cr2.label1, cr2.label2, cr2.collectRegulations, cr2.postRegulations);
    }
    bool operator==(const CrifRecord& cr) const {
        return std::tie(tradeId, nettingSetDetails, productClass, riskType, qualifier, bucket, label1, label2,
                        amountCurrency, collectRegulations, postRegulations) ==
               std::tie(cr.tradeId, cr.nettingSetDetails, cr.productClass, cr.riskType, cr.qualifier, cr.bucket,
                        cr.label1, cr.label2, cr.amountCurrency, cr.collectRegulations, cr.postRegulations);
    }
    static bool amountCcyEQCompare(const CrifRecord& cr1, const CrifRecord& cr2) {
        return std::tie(cr1.tradeId, cr1.nettingSetDetails, cr1.productClass, cr1.riskType, cr1.qualifier, cr1.bucket,
                        cr1.label1, cr1.label2, cr1.collectRegulations, cr1.postRegulations) ==
               std::tie(cr2.tradeId, cr2.nettingSetDetails, cr2.productClass, cr2.riskType, cr2.qualifier, cr2.bucket,
                        cr2.label1, cr2.label2, cr2.collectRegulations, cr2.postRegulations);
    }

    static std::map<QuantLib::Size, std::set<std::string>> additionalHeaders;

};

//! Enable writing of a CrifRecord
std::ostream& operator<<(std::ostream& out, const CrifRecord& cr);

/*! Tag used in the boost multi-index structure below to indicate the index for the
    outer loop in a SIMM calculation i.e. on (Portfolio, Product Class)
*/
struct TradeIdTag {};
struct PortfolioTag {};
struct ProductClassTag {};
struct RiskTypeTag {};
struct BucketTag {};
struct BucketQualifierTag {};
struct QualifierTag {};

/*! A structure that we can use to aggregate CrifRecords across trades in a portfolio
    to provide the net sensitivities that we need to perform a downstream SIMM calculation.
*/
typedef boost::multi_index_container<
    CrifRecord,
    boost::multi_index::indexed_by<
        // The CRIF record itself and its '<' operator define a unique index
        boost::multi_index::ordered_unique<boost::multi_index::identity<CrifRecord>>,
        // Non-unique index in to the trade id level
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<TradeIdTag>,
            boost::multi_index::composite_key<
                CrifRecord, boost::multi_index::member<CrifRecord, std::string, &CrifRecord::tradeId>>>,
        // Non-unique index in to the portfolio level
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<PortfolioTag>,
            boost::multi_index::composite_key<
                CrifRecord, boost::multi_index::member<CrifRecord, NettingSetDetails, &CrifRecord::nettingSetDetails>>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<ProductClassTag>,
            boost::multi_index::composite_key<
                CrifRecord,
                boost::multi_index::member<CrifRecord, NettingSetDetails, &CrifRecord::nettingSetDetails>,
                boost::multi_index::member<CrifRecord, SimmConfiguration::ProductClass, &CrifRecord::productClass>>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<RiskTypeTag>,
            boost::multi_index::composite_key<
                CrifRecord,
                boost::multi_index::member<CrifRecord, NettingSetDetails, &CrifRecord::nettingSetDetails>,
                boost::multi_index::member<CrifRecord, SimmConfiguration::ProductClass, &CrifRecord::productClass>,
                boost::multi_index::member<CrifRecord, SimmConfiguration::RiskType, &CrifRecord::riskType>>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<QualifierTag>,
            boost::multi_index::composite_key<
                CrifRecord,
                boost::multi_index::member<CrifRecord, NettingSetDetails, &CrifRecord::nettingSetDetails>,
                boost::multi_index::member<CrifRecord, SimmConfiguration::ProductClass, &CrifRecord::productClass>,
                boost::multi_index::member<CrifRecord, SimmConfiguration::RiskType, &CrifRecord::riskType>,
                boost::multi_index::member<CrifRecord, std::string, &CrifRecord::qualifier>>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<BucketQualifierTag>,
            boost::multi_index::composite_key<
                CrifRecord,
                boost::multi_index::member<CrifRecord, NettingSetDetails, &CrifRecord::nettingSetDetails>,
                boost::multi_index::member<CrifRecord, SimmConfiguration::ProductClass, &CrifRecord::productClass>,
                boost::multi_index::member<CrifRecord, SimmConfiguration::RiskType, &CrifRecord::riskType>,
                boost::multi_index::member<CrifRecord, std::string, &CrifRecord::bucket>,
                boost::multi_index::member<CrifRecord, std::string, &CrifRecord::qualifier>>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<BucketTag>,
            boost::multi_index::composite_key<
                CrifRecord, boost::multi_index::member<CrifRecord, NettingSetDetails, &CrifRecord::nettingSetDetails>,
                boost::multi_index::member<CrifRecord, SimmConfiguration::ProductClass, &CrifRecord::productClass>,
                boost::multi_index::member<CrifRecord, SimmConfiguration::RiskType, &CrifRecord::riskType>,
                boost::multi_index::member<CrifRecord, std::string, &CrifRecord::bucket>>>>>
    SimmNetSensitivities;

} // namespace analytics
} // namespace ore
