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

#include <ored/portfolio/nettingsetdetails.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <string>
#include <variant>

namespace ore {
namespace analytics {

using ore::data::NettingSetDetails;
/*! A container for holding single CRIF records or aggregated CRIF records.
    A CRIF record is a row of the CRIF file outlined in the document:
    <em>ISDA SIMM Methodology, Risk Data Standards. Version 1.36: 1 February 2017.</em>
    or an updated version thereof.
*/
struct CrifRecord {

    enum class RecordType {
        SIMM,
        FRTB,
        Generic
    };

     /*! Risk types plus an All type for convenience
        Internal methods rely on the last element being 'All'
        Note that the risk type inflation has to be treated as an additional, single
        tenor bucket in IRCurve
    */
    enum class RiskType {
        // Empty for null, misisng field
        Empty,
        // SIMM Risk Types
        Commodity,
        CommodityVol,
        CreditNonQ,
        CreditQ,
        CreditVol,
        CreditVolNonQ,
        Equity,
        EquityVol,
        FX,
        FXVol,
        Inflation,
        IRCurve,
        IRVol,
        InflationVol,
        BaseCorr,
        XCcyBasis,
        ProductClassMultiplier,
        AddOnNotionalFactor,
        Notional,
        AddOnFixedAmount,
        PV, // IM Schedule
        // FRTB Risk Types
        GIRR_DELTA,
        GIRR_VEGA,
        GIRR_CURV,
        CSR_NS_DELTA,
        CSR_NS_VEGA,
        CSR_NS_CURV,
        CSR_SNC_DELTA,
        CSR_SNC_VEGA,
        CSR_SNC_CURV,
        CSR_SC_DELTA,
        CSR_SC_VEGA,
        CSR_SC_CURV,
        EQ_DELTA,
        EQ_VEGA,
        EQ_CURV,
        COMM_DELTA,
        COMM_VEGA,
        COMM_CURV,
        FX_DELTA,
        FX_VEGA,
        FX_CURV,
        DRC_NS,
        DRC_SNC,
        DRC_SC,
        RRAO_1_PERCENT,
        RRAO_01_PERCENT,
        // All type for aggreggation purposes
        All
    };
    
    //! Product class types in SIMM plus an All type for convenience
    //! Internal methods rely on the last element being 'All'
    enum class ProductClass {
        RatesFX,
        Rates, // extension for IM Schedule
        FX,    // extension for IM Schedule
        Credit,
        Equity,
        Commodity,
        Empty,
        Other, // extension for IM Schedule
        AddOnNotionalFactor, // extension for additional IM
        AddOnFixedAmount,    // extension for additional IM
        All
    };

    //! There are two entries for curvature risk in frtb, a up and down shift
    enum class CurvatureScenario { Empty, Up, Down };

    // clang-format off
    //                 trade ID                                                qualifier    bucket       label1       label2       collect regs post regs
    typedef std::tuple<std::string, NettingSetDetails, ProductClass, RiskType, std::string, std::string, std::string, std::string, std::string, std::string> SimmAmountCcyKey;
    // clang-format on

    // required data
    std::string tradeId;
    std::string portfolioId;
    ProductClass productClass = ProductClass::Empty;
    RiskType riskType = RiskType::Notional;
    std::string qualifier;
    std::string bucket;
    std::string label1;
    std::string label2;
    std::string amountCurrency;
    mutable QuantLib::Real amount = QuantLib::Null<QuantLib::Real>();
    mutable QuantLib::Real amountUsd = QuantLib::Null<QuantLib::Real>();
    
    // additional fields used exclusively by the SIMM calculator for handling amounts converted in a given result ccy
    std::string resultCurrency;
    mutable QuantLib::Real amountResultCcy = QuantLib::Null<QuantLib::Real>();

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

    // frtb fields
    std::string label3;
    std::string creditQuality;
    std::string longShortInd;
    std::string coveredBondInd;
    std::string trancheThickness;
    std::string bb_rw;

    // additional data
    std::map<std::string, std::variant<std::string, double, bool>> additionalFields;

    // Default Constructor
    CrifRecord() {}

    CrifRecord(std::string tradeId, std::string tradeType, NettingSetDetails nettingSetDetails,
               ProductClass productClass, RiskType riskType,
               std::string qualifier, std::string bucket, std::string label1, std::string label2,
               std::string amountCurrency, QuantLib::Real amount, QuantLib::Real amountUsd, std::string imModel = "",
               std::string collectRegulations = "", std::string postRegulations = "", std::string endDate = "",
               std::map<std::string, std::string> extraFields = {})
        : tradeId(tradeId), portfolioId(nettingSetDetails.nettingSetId()),
          productClass(productClass), riskType(riskType), qualifier(qualifier),
          bucket(bucket), label1(label1), label2(label2), amountCurrency(amountCurrency), amount(amount),
          amountUsd(amountUsd), tradeType(tradeType), nettingSetDetails(nettingSetDetails), imModel(imModel),
          collectRegulations(collectRegulations), postRegulations(postRegulations), endDate(endDate) {
        for (const auto& [key, value] : extraFields) {
            additionalFields[key] = value;
        }
    }

    RecordType type() const;

    bool hasAmountCcy() const { return !amountCurrency.empty(); }
    bool hasAmount() const { return amount != QuantLib::Null<QuantLib::Real>(); }
    bool hasAmountUsd() const { return amountUsd != QuantLib::Null<QuantLib::Real>(); }
    bool hasResultCcy() const { return !resultCurrency.empty(); }
    bool hasAmountResultCcy() const { return amountResultCcy != QuantLib::Null<QuantLib::Real>(); }

    // We use (and require) amountUsd for all risk types except for SIMM parameters AddOnNotionalFactor and
    // ProductClassMultiplier as these are multipliers and not amounts denominated in the amountCurrency
    bool requiresAmountUsd() const {
        return riskType != RiskType::AddOnNotionalFactor &&
               riskType != RiskType::ProductClassMultiplier;
    }

    bool isSimmParameter() const {
        return riskType == RiskType::AddOnFixedAmount ||
               riskType == RiskType::AddOnNotionalFactor ||
               riskType == RiskType::ProductClassMultiplier;
    }

    bool isEmpty() const { return riskType == RiskType::Empty; }

    
    bool isFrtbCurvatureRisk() const {
        switch (riskType) {
        case RiskType::GIRR_CURV:
        case RiskType::CSR_NS_CURV:
        case RiskType::CSR_SNC_CURV:
        case RiskType::CSR_SC_CURV:
        case RiskType::EQ_CURV:
        case RiskType::COMM_CURV:
        case RiskType::FX_CURV:
            return true;
        default:
            return false;
        }
    }

    CurvatureScenario frtbCurveatureScenario() const {
        double shift = 0.0;
        if (isFrtbCurvatureRisk() && ore::data::tryParseReal(label1, shift) && shift < 0.0) {
            return CurvatureScenario::Down;
        } else if (isFrtbCurvatureRisk()) {
            return CurvatureScenario::Up;
        } else {
            return CurvatureScenario::Empty;
        }
    }

    std::string getAdditionalFieldAsStr(const std::string& fieldName) const {
        auto it = additionalFields.find(fieldName);
        std::string value;
        if (it != additionalFields.end()) {
            if (const std::string* vstr = std::get_if<std::string>(&it->second)) {
                value = *vstr;
            } else if (const double* vdouble = std::get_if<double>(&it->second)) {
                value = ore::data::to_string(*vdouble);
            } else {
                const bool* vbool = std::get_if<bool>(&it->second);
                value = ore::data::to_string(*vbool);
            }
        }
        return value;
    }

    double getAdditionalFieldAsDouble(const std::string& fieldName) const {
        auto it = additionalFields.find(fieldName);
        double value = QuantLib::Null<double>();
        if (it != additionalFields.end()) {
            if (const double* vdouble = std::get_if<double>(&it->second)) {
                value = *vdouble;
            } else if (const std::string* vstr = std::get_if<std::string>(&it->second)) {
                value = ore::data::parseReal(*vstr);
            }
        }
        return value;
    }

    bool getAdditionalFieldAsBool(const std::string& fieldName) const {
        auto it = additionalFields.find(fieldName);
        bool value = false;
        if (it != additionalFields.end()) {
            if (const bool* vbool = std::get_if<bool>(&it->second)) {
                value = *vbool;
            } else if (const std::string* vstr = std::get_if<std::string>(&it->second)) {
                value = ore::data::parseBool(*vstr);
            }
        }
        return value;
    }

    const SimmAmountCcyKey getSimmAmountCcyKey() const {
        return std::make_tuple(tradeId, nettingSetDetails, productClass, riskType, qualifier, bucket, label1, label2,
                               collectRegulations, postRegulations);
    }

    //! Define how CRIF records are compared
    bool operator<(const CrifRecord& cr) const {
        if (type() == RecordType::FRTB || cr.type() == RecordType::FRTB) {
            return std::tie(tradeId, nettingSetDetails, productClass, riskType, qualifier, bucket, label1, label2,
                            label3, endDate, creditQuality, longShortInd, coveredBondInd, trancheThickness, bb_rw,
                            amountCurrency, collectRegulations, postRegulations) <
                   std::tie(cr.tradeId, cr.nettingSetDetails, cr.productClass, cr.riskType, cr.qualifier, cr.bucket,
                            cr.label1, cr.label2, cr.label3, cr.endDate, cr.creditQuality, cr.longShortInd,
                            cr.coveredBondInd, cr.trancheThickness, cr.bb_rw, cr.amountCurrency, cr.collectRegulations,
                            cr.postRegulations);
        } else {
            return std::tie(tradeId, nettingSetDetails, productClass, riskType, qualifier, bucket, label1, label2,
                            amountCurrency, collectRegulations, postRegulations) <
                   std::tie(cr.tradeId, cr.nettingSetDetails, cr.productClass, cr.riskType, cr.qualifier, cr.bucket,
                            cr.label1, cr.label2, cr.amountCurrency, cr.collectRegulations, cr.postRegulations);
        }
    }

    static bool amountCcyLTCompare(const CrifRecord& cr1, const CrifRecord& cr2) {
        if (cr1.type() == RecordType::FRTB || cr2.type() == RecordType::FRTB) {
            return std::tie(cr1.tradeId, cr1.nettingSetDetails, cr1.productClass, cr1.riskType, cr1.qualifier,
                            cr1.bucket, cr1.label1, cr1.label2, cr1.label3, cr1.endDate, cr1.creditQuality,
                            cr1.longShortInd, cr1.coveredBondInd, cr1.trancheThickness, cr1.bb_rw,
                            cr1.collectRegulations, cr1.postRegulations) <
                   std::tie(cr2.tradeId, cr2.nettingSetDetails, cr2.productClass, cr2.riskType, cr2.qualifier,
                            cr2.bucket, cr2.label1, cr2.label2, cr2.label3, cr2.endDate, cr2.creditQuality,
                            cr2.longShortInd, cr2.coveredBondInd, cr2.trancheThickness, cr2.bb_rw,
                            cr2.collectRegulations, cr2.postRegulations);
        } else {
            return std::tie(cr1.tradeId, cr1.nettingSetDetails, cr1.productClass, cr1.riskType, cr1.qualifier,
                            cr1.bucket, cr1.label1, cr1.label2, cr1.collectRegulations, cr1.postRegulations) <
                   std::tie(cr2.tradeId, cr2.nettingSetDetails, cr2.productClass, cr2.riskType, cr2.qualifier,
                            cr2.bucket, cr2.label1, cr2.label2, cr2.collectRegulations, cr2.postRegulations);
        }
    }

    bool operator==(const CrifRecord& cr) const {
        if (type() == RecordType::FRTB || cr.type() == RecordType::FRTB) {
            return std::tie(tradeId, nettingSetDetails, productClass, riskType, qualifier, bucket, label1, label2,
                            label3, endDate, creditQuality, longShortInd, coveredBondInd, trancheThickness, bb_rw,
                            amountCurrency, collectRegulations, postRegulations) ==
                   std::tie(cr.tradeId, cr.nettingSetDetails, cr.productClass, cr.riskType, cr.qualifier, cr.bucket,
                            cr.label1, cr.label2, cr.label3, cr.endDate, cr.creditQuality, cr.longShortInd,
                            cr.coveredBondInd, cr.trancheThickness, cr.bb_rw, cr.amountCurrency, cr.collectRegulations,
                            cr.postRegulations);
        } else {
            return std::tie(tradeId, nettingSetDetails, productClass, riskType, qualifier, bucket, label1, label2,
                            amountCurrency, collectRegulations, postRegulations) ==
                   std::tie(cr.tradeId, cr.nettingSetDetails, cr.productClass, cr.riskType, cr.qualifier, cr.bucket,
                            cr.label1, cr.label2, cr.amountCurrency, cr.collectRegulations, cr.postRegulations);
        }
    }
    static bool amountCcyEQCompare(const CrifRecord& cr1, const CrifRecord& cr2) {
        if (cr1.type() == RecordType::FRTB || cr2.type() == RecordType::FRTB) {
            return std::tie(cr1.tradeId, cr1.nettingSetDetails, cr1.productClass, cr1.riskType, cr1.qualifier,
                            cr1.bucket, cr1.label1, cr1.label2, cr1.label3, cr1.endDate, cr1.creditQuality,
                            cr1.longShortInd, cr1.coveredBondInd, cr1.trancheThickness, cr1.bb_rw,
                            cr1.collectRegulations, cr1.postRegulations) ==
                   std::tie(cr2.tradeId, cr2.nettingSetDetails, cr2.productClass, cr2.riskType, cr2.qualifier,
                            cr2.bucket, cr2.label1, cr2.label2, cr2.label3, cr2.endDate, cr2.creditQuality,
                            cr2.longShortInd, cr2.coveredBondInd, cr2.trancheThickness, cr2.bb_rw,
                            cr2.collectRegulations, cr2.postRegulations);
        } else {
            return std::tie(cr1.tradeId, cr1.nettingSetDetails, cr1.productClass, cr1.riskType, cr1.qualifier,
                            cr1.bucket, cr1.label1, cr1.label2, cr1.collectRegulations, cr1.postRegulations) ==
                   std::tie(cr2.tradeId, cr2.nettingSetDetails, cr2.productClass, cr2.riskType, cr2.qualifier,
                            cr2.bucket, cr2.label1, cr2.label2, cr2.collectRegulations, cr2.postRegulations);
        }
    }

    static std::vector<std::set<std::string>> additionalHeaders;
};

//! Enable writing of a CrifRecord
std::ostream& operator<<(std::ostream& out, const CrifRecord& cr);

std::ostream& operator<<(std::ostream& out, const CrifRecord::RiskType& rt);

std::ostream& operator<<(std::ostream& out, const CrifRecord::ProductClass& pc);

std::ostream& operator<<(std::ostream& out, const CrifRecord::CurvatureScenario& scenario);

CrifRecord::RiskType parseRiskType(const std::string& rt);

CrifRecord::ProductClass parseProductClass(const std::string& pc);

CrifRecord::CurvatureScenario parseFrtbCurvatureScenario(const std::string& scenario);

} // namespace analytics
} // namespace ore
