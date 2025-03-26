/*
 Copyright (C) 2023 AcadiaSoft Inc.
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

/*! \file orea/simm/crif.hpp
    \brief Struct for holding CRIF records
*/

#pragma once
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <orea/simm/crifrecord.hpp>
#include <ored/report/report.hpp>
#include <ored/marketdata/market.hpp>
#include <boost/bimap.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <ored/portfolio/nettingsetdetails.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <string>
#include <variant>

namespace ore {
namespace analytics {

class Crif;

class SlimCrifRecord {

public:
    SlimCrifRecord() {}
    SlimCrifRecord(const QuantLib::ext::weak_ptr<Crif>& crif);
    SlimCrifRecord(const QuantLib::ext::weak_ptr<Crif>& crif, const CrifRecord& cr);
    SlimCrifRecord(const QuantLib::ext::weak_ptr<Crif>& crif, const SlimCrifRecord& cr);

    void updateFromCrifRecord(const CrifRecord& record);
    void updateFromSlimCrifRecord(const SlimCrifRecord& record);

    CrifRecord::RecordType type() const;

    QuantLib::ext::weak_ptr<Crif> crif_;

    const QuantLib::ext::weak_ptr<Crif>& crif() const { return crif_; }
    const std::string& getTradeId() const;
    const std::string& getTradeType() const;
    const ore::data::NettingSetDetails& getNettingSetDetails() const;
    const std::string& getQualifier() const;
    const std::string& getBucket() const;
    const std::string& getLabel1() const;
    const std::string& getLabel2() const;
    const std::string& getResultCurrency() const;
    const std::string& getEndDate() const;
    const std::string& getCurrency() const;

    const std::string& getLabel3() const;
    const std::string& getCreditQuality() const;
    const std::string& getLongShortInd() const;
    const std::string& getCoveredBondInd() const;
    const std::string& getTrancheThickness() const;
    const std::string& getBbRw() const;

    // Members whose values do not need to be managed by a Crif instance
    const std::set<CrifRecord::Regulation>& collectRegulations() const { return collectRegulations_; }
    const std::set<CrifRecord::Regulation>& postRegulations() const { return postRegulations_; }
    const CrifRecord::RiskType& riskType() const { return riskType_; }
    const CrifRecord::ProductClass& productClass() const { return productClass_; }
    // QuantLib::Real amount() const { return amount_; }
    // QuantLib::Real amountUsd() const { return amountUsd_; }
    // QuantLib::Real amountResultCurrency() const { return amountResultCcy_; }
    const CrifRecord::IMModel& imModel() const { return imModel_; }

    QuantLib::Real& amount() { return amount_; }
    QuantLib::Real& amountUsd() { return amountUsd_; }
    QuantLib::Real& amountResultCurrency() { return amountResultCcy_; }
    QuantLib::Real& additionalFields() { return amount_; }

    QuantLib::Real amount() const { return amount_; }
    QuantLib::Real amountUsd() const { return amountUsd_; }
    QuantLib::Real amountResultCurrency() const { return amountResultCcy_; }
    const std::map<std::string, std::variant<std::string, double, bool>>& additionalFields() const {
        return additionalFields_;
    }

    void setCollectRegulations(const std::set<CrifRecord::Regulation>& value) { collectRegulations_ = value; }
    void setPostRegulations(const std::set<CrifRecord::Regulation>& value) { postRegulations_ = value; }
    void setAmount(QuantLib::Real value) const { amount_ = value; }
    void setAmountUsd(QuantLib::Real value) const { amountUsd_ = value; }
    void setAmountResultCurrency(QuantLib::Real value) const { amountResultCcy_ = value; }
    void setImModel(const CrifRecord::IMModel& value) { imModel_ = value; }

    void setCrif(const QuantLib::ext::weak_ptr<Crif>& crif);
    void setTradeId(const std::string& value);
    void setTradeType(const std::string& value);
    void setNettingSetDetails(const ore::data::NettingSetDetails& value);
    void setQualifier(const std::string& value);
    void setBucket(const std::string& value);
    void setLabel1(const std::string& value);
    void setLabel2(const std::string& value);
    void setResultCurrency(const std::string& value) const;
    void setEndDate(const std::string& value);
    void setCurrency(const std::string& value);
    void setLabel3(const std::string& value);
    void setCreditQuality(const std::string& value);
    void setLongShortInd(const std::string& value);
    void setCoveredBondInd(const std::string& value);
    void setTrancheThickness(const std::string& value);
    void setBbRw(const std::string& value);

    bool hasAmountCcy() const { return !getCurrency().empty(); }
    bool hasAmount() const { return amount_ != QuantLib::Null<QuantLib::Real>(); }
    bool hasAmountUsd() const { return amountUsd_ != QuantLib::Null<QuantLib::Real>(); }
    bool hasResultCcy() const { return !getResultCurrency().empty(); }
    bool hasAmountResultCcy() const { return amountResultCcy_ != QuantLib::Null<QuantLib::Real>(); }

    // We use (and require) amountUsd for all risk types except for SIMM parameters AddOnNotionalFactor and
    // ProductClassMultiplier as these are multipliers and not amounts denominated in the currency
    bool requiresAmountUsd() const {
        return riskType_ != CrifRecord::RiskType::AddOnNotionalFactor &&
               riskType_ != CrifRecord::RiskType::ProductClassMultiplier;
    }

    bool isSimmParameter() const {
        return riskType_ == CrifRecord::RiskType::AddOnFixedAmount ||
               riskType_ == CrifRecord::RiskType::AddOnNotionalFactor ||
               riskType_ == CrifRecord::RiskType::ProductClassMultiplier;
    }

    bool isEmpty() const { return riskType() == CrifRecord::RiskType::Empty; }

    bool isFrtbCurvatureRisk() const {
        switch (riskType_) {
        case CrifRecord::RiskType::GIRR_CURV:
        case CrifRecord::RiskType::CSR_NS_CURV:
        case CrifRecord::RiskType::CSR_SNC_CURV:
        case CrifRecord::RiskType::CSR_SC_CURV:
        case CrifRecord::RiskType::EQ_CURV:
        case CrifRecord::RiskType::COMM_CURV:
        case CrifRecord::RiskType::FX_CURV:
            return true;
        default:
            return false;
        }
    }

    CrifRecord::CurvatureScenario frtbCurveatureScenario() const {
        double shift = 0.0;
        if (isFrtbCurvatureRisk() && ore::data::tryParseReal(getLabel1(), shift) && shift < 0.0) {
            return CrifRecord::CurvatureScenario::Down;
        } else if (isFrtbCurvatureRisk()) {
            return CrifRecord::CurvatureScenario::Up;
        } else {
            return CrifRecord::CurvatureScenario::Empty;
        }
    }

    std::string getAdditionalFieldAsStr(const std::string& fieldName) const {
        auto it = additionalFields_.find(fieldName);
        std::string value;
        if (it != additionalFields_.end()) {
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
        auto it = additionalFields_.find(fieldName);
        double value = QuantLib::Null<double>();
        if (it != additionalFields_.end()) {
            if (const double* vdouble = std::get_if<double>(&it->second)) {
                value = *vdouble;
            } else if (const std::string* vstr = std::get_if<std::string>(&it->second)) {
                value = ore::data::parseReal(*vstr);
            }
        }
        return value;
    }

    bool getAdditionalFieldAsBool(const std::string& fieldName) const {
        auto it = additionalFields_.find(fieldName);
        bool value = false;
        if (it != additionalFields_.end()) {
            if (const bool* vbool = std::get_if<bool>(&it->second)) {
                value = *vbool;
            } else if (const std::string* vstr = std::get_if<std::string>(&it->second)) {
                value = ore::data::parseBool(*vstr);
            }
        }
        return value;
    }

    //! Define how CRIF records are compared
    bool operator<(const SlimCrifRecord& cr) const {
        if (type() == CrifRecord::CrifRecord::RecordType::FRTB || cr.type() == CrifRecord::RecordType::FRTB) {
            return std::tie(tradeId(), nettingSetDetails(), productClass(), riskType(), qualifier(),
                            bucket(), label1(), label2(), label3(), endDate(), creditQuality(),
                            longShortInd(), coveredBondInd(), trancheThickness(), bbRw(), currency(),
                            collectRegulations(), postRegulations()) <
                   std::tie(cr.tradeId(), cr.nettingSetDetails(), cr.productClass(), cr.riskType(),
                            cr.qualifier(), cr.bucket(), cr.label1(), cr.label2(), cr.label3(),
                            cr.endDate(), cr.creditQuality(), cr.longShortInd(), cr.coveredBondInd(),
                            cr.trancheThickness(), cr.bbRw(), cr.currency(), cr.collectRegulations(),
                            cr.postRegulations());
        } else {
            return std::tie(tradeId(), nettingSetDetails(), productClass(), riskType(), qualifier(),
                            bucket(), label1(), label2(), currency(), collectRegulations(),
                            postRegulations()) <
                   std::tie(cr.tradeId(), cr.nettingSetDetails(), cr.productClass(),
                            cr.riskType(), cr.qualifier(), cr.bucket(),
                            cr.label1(), cr.label2(), cr.currency(),
                            cr.collectRegulations(), cr.postRegulations());
        }
    }

    inline static bool amountCcyLTCompare(const SlimCrifRecord& cr1, const SlimCrifRecord& cr2) {
        if (cr1.type() == CrifRecord::RecordType::FRTB || cr2.type() == CrifRecord::RecordType::FRTB) {
            return std::tie(cr1.tradeId(), cr1.nettingSetDetails(), cr1.productClass(), cr1.riskType(),
                            cr1.qualifier(), cr1.bucket(), cr1.label1(), cr1.label2(), cr1.label3(),
                            cr1.endDate(), cr1.creditQuality(), cr1.longShortInd(), cr1.coveredBondInd(),
                            cr1.trancheThickness(), cr1.bbRw(), cr1.collectRegulations(), cr1.postRegulations()) <
                   std::tie(cr2.tradeId(), cr2.nettingSetDetails(), cr2.productClass(), cr2.riskType(),
                            cr2.qualifier(), cr2.bucket(), cr2.label1(), cr2.label2(), cr2.label3(),
                            cr2.endDate(), cr2.creditQuality(), cr2.longShortInd(), cr2.coveredBondInd(),
                            cr2.trancheThickness(), cr2.bbRw(), cr2.collectRegulations(), cr2.postRegulations());
        } else {
            return std::tie(cr1.tradeId(), cr1.nettingSetDetails(), cr1.productClass(), cr1.riskType(),
                            cr1.qualifier(), cr1.bucket(), cr1.label1(), cr1.label2(),
                            cr1.collectRegulations(), cr1.postRegulations()) <
                   std::tie(cr2.tradeId(), cr2.nettingSetDetails(), cr2.productClass(), cr2.riskType(),
                            cr2.qualifier(), cr2.bucket(), cr2.label1(), cr2.label2(),
                            cr2.collectRegulations(), cr2.postRegulations());
        }
    }

    bool operator==(const SlimCrifRecord& cr) const {
        if (type() == CrifRecord::RecordType::FRTB || cr.type() == CrifRecord::RecordType::FRTB) {
            return std::tie(tradeId(), nettingSetDetails(), productClass(), riskType(), qualifier(),
                            bucket(), label1(), label2(), label3(), endDate(), creditQuality(),
                            longShortInd(), coveredBondInd(), trancheThickness(), bbRw(), currency(),
                            collectRegulations(), postRegulations()) ==
                   std::tie(cr.tradeId(), cr.nettingSetDetails(), cr.productClass(), cr.riskType(),
                            cr.qualifier(), cr.bucket(), cr.label1(), cr.label2(), cr.label3(),
                            cr.endDate(), cr.creditQuality(), cr.longShortInd(), cr.coveredBondInd(),
                            cr.trancheThickness(), cr.bbRw(), cr.currency(), cr.collectRegulations(),
                            cr.postRegulations());
        } else {
            return std::tie(tradeId(), nettingSetDetails(), productClass(), riskType(), qualifier(),
                            bucket(), label1(), label2(), currency(), collectRegulations(),
                            postRegulations()) ==
                   std::tie(cr.tradeId(), cr.nettingSetDetails(), cr.productClass(), cr.riskType(),
                            cr.qualifier(), cr.bucket(), cr.label1(), cr.label2(), cr.currency(),
                            cr.collectRegulations(), cr.postRegulations());
        }
    }
    inline static bool amountCcyEQCompare(const SlimCrifRecord& cr1, const SlimCrifRecord& cr2) {
        if (cr1.type() == CrifRecord::RecordType::FRTB || cr2.type() == CrifRecord::RecordType::FRTB) {
            return std::tie(cr1.tradeId(), cr1.nettingSetDetails(), cr1.productClass(), cr1.riskType(),
                            cr1.qualifier(), cr1.bucket(), cr1.label1(), cr1.label2(), cr1.label3(),
                            cr1.endDate(), cr1.creditQuality(), cr1.longShortInd(), cr1.coveredBondInd(),
                            cr1.trancheThickness(), cr1.bbRw(), cr1.collectRegulations(),
                            cr1.postRegulations()) ==
                   std::tie(cr2.tradeId(), cr2.nettingSetDetails(), cr2.productClass(), cr2.riskType(),
                            cr2.qualifier(), cr2.bucket(), cr2.label1(), cr2.label2(), cr2.label3(),
                            cr2.endDate(), cr2.creditQuality(), cr2.longShortInd(), cr2.coveredBondInd(),
                            cr2.trancheThickness(), cr2.bbRw(), cr2.collectRegulations(), cr2.postRegulations());
        } else {
            return std::tie(cr1.tradeId(), cr1.nettingSetDetails(), cr1.productClass(), cr1.riskType(),
                            cr1.qualifier(), cr1.bucket(), cr1.label1(), cr1.label2(),
                            cr1.collectRegulations(), cr1.postRegulations()) ==
                   std::tie(cr2.tradeId(), cr2.nettingSetDetails(), cr2.productClass(), cr2.riskType(),
                            cr2.qualifier(), cr2.bucket(), cr2.label1(), cr2.label2(),
                            cr2.collectRegulations(), cr2.postRegulations());
        }
    }

    CrifRecord toCrifRecord() const;

    const int& tradeId() const { return tradeId_; }
    const int& tradeType() const { return tradeType_; }
    const int& nettingSetDetails() const { return nettingSetDetails_; }
    const int& qualifier() const { return qualifier_; }
    const int& bucket() const { return bucket_; }
    const int& label1() const { return label1_; }
    const int& label2() const { return label2_; }
    const int& resultCurrency() const { return resultCurrency_; }
    const int& endDate() const { return endDate_; }
    const int& currency() const { return currency_; }
    const int& label3() const { return label3_; }
    const int& creditQuality() const { return creditQuality_; }
    const int& longShortInd() const { return longShortInd_; }
    const int& coveredBondInd() const { return coveredBondInd_; }
    const int& trancheThickness() const { return trancheThickness_; }
    const int& bbRw() const { return bb_rw_; }

    static std::vector<std::set<std::string>> additionalHeaders;

private:
    // required data
    CrifRecord::ProductClass productClass_ = CrifRecord::ProductClass::Empty;
    CrifRecord::RiskType riskType_ = CrifRecord::RiskType::Notional;
    int tradeId_;
    int tradeType_;
    int nettingSetDetails_;
    int qualifier_;
    int bucket_;
    int label1_;
    int label2_;
    int currency_;
    mutable QuantLib::Real amount_ = QuantLib::Null<QuantLib::Real>();
    mutable QuantLib::Real amountUsd_ = QuantLib::Null<QuantLib::Real>();

    // additional fields used exclusively by the SIMM calculator for handling amounts converted in a given result
    // ccy
    mutable int resultCurrency_;
    mutable QuantLib::Real amountResultCcy_ = QuantLib::Null<QuantLib::Real>();

    // optional data
    mutable CrifRecord::IMModel imModel_ = CrifRecord::IMModel::Empty;
    mutable std::set<CrifRecord::Regulation> collectRegulations_;
    mutable std::set<CrifRecord::Regulation> postRegulations_;
    int endDate_;

    // frtb fields
    int label3_;
    int creditQuality_;
    int longShortInd_;
    int coveredBondInd_;
    int trancheThickness_;
    int bb_rw_;

    // additional data
    std::map<std::string, std::variant<std::string, double, bool>> additionalFields_;
};

struct QualifierTag {};
struct BucketTag {};
struct QualifierBucketTag {};
struct RiskTypeTag {};

/*! A structure that we can use to aggregate CrifRecords across trades in a portfolio
    to provide the net sensitivities that we need to perform a downstream SIMM calculation.
*/
// clang-format off
typedef boost::multi_index_container<SlimCrifRecord,
    boost::multi_index::indexed_by<
        // 0 The (slim) CRIF record itself and its '<' operator define a unique index
        boost::multi_index::ordered_unique<boost::multi_index::identity<SlimCrifRecord>>,
        // 1
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<QualifierTag>,
            boost::multi_index::composite_key<
                SlimCrifRecord,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const int&, &SlimCrifRecord::nettingSetDetails>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const CrifRecord::ProductClass&, &SlimCrifRecord::productClass>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const CrifRecord::RiskType&, &SlimCrifRecord::riskType>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const int&, &SlimCrifRecord::qualifier>
            >
        >,
        // 2
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<BucketTag>,
            boost::multi_index::composite_key<
                SlimCrifRecord,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const int&, &SlimCrifRecord::nettingSetDetails>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const CrifRecord::ProductClass&, &SlimCrifRecord::productClass>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const CrifRecord::RiskType&, &SlimCrifRecord::riskType>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const int&, &SlimCrifRecord::bucket>
            >
        >,
        // 3
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<QualifierBucketTag>,
            boost::multi_index::composite_key<
                SlimCrifRecord,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const int&, &SlimCrifRecord::nettingSetDetails>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const CrifRecord::ProductClass&, &SlimCrifRecord::productClass>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const CrifRecord::RiskType&, &SlimCrifRecord::riskType>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const int&, &SlimCrifRecord::qualifier>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const int&, &SlimCrifRecord::bucket>
            >
        >,
        // 4
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<RiskTypeTag>,
            boost::multi_index::composite_key<
                SlimCrifRecord,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const int&, &SlimCrifRecord::nettingSetDetails>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const CrifRecord::ProductClass&, &SlimCrifRecord::productClass>,
                boost::multi_index::const_mem_fun<SlimCrifRecord, const CrifRecord::RiskType&, &SlimCrifRecord::riskType>
            >
        >
    >
>   SlimCrifRecordContainer;
// clang-format on

struct crifRecordIsSimmParameter {
    bool operator()(const SlimCrifRecord& x) { return x.isSimmParameter(); }
};

//! Enable writing of a CrifRecord
std::ostream& operator<<(std::ostream& out, const SlimCrifRecord& cr);

/*! A container for holding single CRIF records or aggregated CRIF records.
    A CRIF record is a row of the CRIF file outlined in the document:
    <em>ISDA SIMM Methodology, Risk Data Standards. Version 1.36: 1 February 2017.</em>
    or an updated version thereof.
*/
class Crif : public QuantLib::ext::enable_shared_from_this<Crif> {
public:
    enum class CrifType { Empty, Frtb, Simm };
    Crif() = default;

    CrifType type() const { return type_; }

    void addRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies = false,
                   bool sortFxVolQualifer = true);
    void addRecord(const SlimCrifRecord& record, bool aggregateDifferentAmountCurrencies = false,
                   bool sortFxVolQualifer = true);
    void addRecords(const Crif& crif, bool aggregateDifferentAmountCurrencies = false, bool sortFxVolQualfier = true);
    void addRecords(const QuantLib::ext::shared_ptr<Crif>& crif, bool aggregateDifferentAmountCurrencies = false, bool sortFxVolQualfier = true);

    void clear();

    void updateIndex(CrifRecord& record);

    SlimCrifRecordContainer::nth_index<0>::type::iterator begin() { return records_.begin(); }
    SlimCrifRecordContainer::nth_index<0>::type::iterator end() { return records_.end(); }
    SlimCrifRecordContainer::nth_index<0>::type::iterator find(const SlimCrifRecord& r) { return records_.find(r); }

    SlimCrifRecordContainer::nth_index<0>::type::const_iterator begin() const { return records_.cbegin(); }
    SlimCrifRecordContainer::nth_index<0>::type::const_iterator end() const { return records_.cend(); }
    SlimCrifRecordContainer::nth_index<0>::type::const_iterator find(const SlimCrifRecord& r) const { return records_.find(r); }

    //! Find first element
    std::pair<SlimCrifRecordContainer::nth_index<1>::type::const_iterator,
              SlimCrifRecordContainer::nth_index<1>::type::const_iterator>
    findBy(const NettingSetDetails nsd, CrifRecord::ProductClass pc, const CrifRecord::RiskType rt,
           const std::string& qualifier) const;

    bool empty() const { return records_.empty(); }

    size_t size() const { return records_.size(); }

    //! check if there are crif records beside simmParameters
    const bool hasCrifRecords() const;

    //! Simm methods
    //! Give back the set of portfolio IDs that have been loaded
    std::set<std::string> portfolioIds() const;
    const std::set<ore::data::NettingSetDetails>& nettingSetDetails() const { return nettingSetDetails_; }

    //! check if the Crif contains simmParameters
    const bool hasSimmParameters() const;

    //! returns a crif without zero amount records, FXRisk entries in currency *alwaysIncludeFxRiskCcy* are always
    //! included
    QuantLib::ext::shared_ptr<Crif> filterNonZeroAmount(double threshold = 0.0, std::string alwaysIncludeFxRiskCcy = "") const;

    //! returns a Crif containing only simmParameter entries
    QuantLib::ext::shared_ptr<Crif> simmParameters() const;

    //! deletes all existing simmParameter and replaces them with the new one
    void setSimmParameters(const QuantLib::ext::shared_ptr<Crif>& crif);

    //! deletes all existing simmParameter and replaces them with the new one
    void setCrifRecords(const QuantLib::ext::shared_ptr<Crif>& crif);

    //! For each CRIF record checks if currency and amount are
    //! defined and uses these to populate the record's amountUsd
    void fillAmountUsd(const QuantLib::ext::shared_ptr<ore::data::Market> market);

    //! Check if netting set details are used anywhere, instead of just the netting set ID
    bool hasNettingSetDetails() const;

    //! Aggregate all existing records
    QuantLib::ext::shared_ptr<Crif> aggregate(bool aggregateDifferentAmountCurrencies = false) const;
    void setAggregate(bool flag) { aggregate_ = flag; }

    size_t countMatching(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc, const CrifRecord::RiskType rt,
                         const std::string& qualifier) const;

    std::set<CrifRecord::ProductClass> ProductClassesByNettingSetDetails(const NettingSetDetails nsd) const;

    std::set<std::string> qualifiersBy(const NettingSetDetails nsd, CrifRecord::ProductClass pc,
                                       const CrifRecord::RiskType rt) const;

    std::pair<SlimCrifRecordContainer::nth_index<1>::type::const_iterator,
              SlimCrifRecordContainer::nth_index<1>::type::const_iterator>
    filterByQualifier(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc, const CrifRecord::RiskType rt,
                      const std::string& qualifier) const;

    std::pair<SlimCrifRecordContainer::nth_index<2>::type::const_iterator,
              SlimCrifRecordContainer::nth_index<2>::type::const_iterator>
    filterByBucket(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc, const CrifRecord::RiskType rt,
                   const std::string& bucket) const;

    std::pair<SlimCrifRecordContainer::nth_index<3>::type::const_iterator,
              SlimCrifRecordContainer::nth_index<3>::type::const_iterator>
    filterByQualifierAndBucket(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc,
                               const CrifRecord::RiskType rt, const std::string& qualifier,
                               const std::string& bucket) const;

    std::pair<SlimCrifRecordContainer::nth_index<4>::type::const_iterator,
              SlimCrifRecordContainer::nth_index<4>::type::const_iterator>
    filterBy(const NettingSetDetails& nsd, const CrifRecord::ProductClass pc,
                                     const CrifRecord::RiskType rt) const;
    std::vector<SlimCrifRecord> filterBy(const CrifRecord::RiskType rt) const;
    std::vector<SlimCrifRecord> filterByTradeId(const std::string& id) const;
    std::set<std::string> tradeIds() const;

    const std::string& getTradeId(int idx) const;
    const std::string& getTradeType(int idx) const;
    const ore::data::NettingSetDetails& getNettingSetDetails(int idx) const;
    const std::string& getQualifier(int idx) const;
    const std::string& getBucket(int idx) const;
    const std::string& getLabel1(int idx) const;
    const std::string& getLabel2(int idx) const;
    const std::string& getResultCurrency(int idx) const;
    const std::string& getEndDate(int idx) const;
    const std::string& getCurrency(int idx) const;

    const std::string& getLabel3(int idx) const;
    const std::string& getCreditQuality(int idx) const;
    const std::string& getLongShortInd(int idx) const;
    const std::string& getCoveredBondInd(int idx) const;
    const std::string& getTrancheThickness(int idx) const;
    const std::string& getBbRw(int idx) const;

    int updateTradeIdIndex(const std::string& value);
    int updateTradeTypeIndex(const std::string& value);
    int updateNettingSetDetailsIndex(const ore::data::NettingSetDetails& value);
    int updateQualifierIndex(const std::string& value);
    int updateBucketIndex(const std::string& value);
    int updateLabel1Index(const std::string& value);
    int updateLabel2Index(const std::string& value);
    int updateResultCurrencyIndex(const std::string& value);
    int updateEndDateIndex(const std::string& value);
    int updateCurrencyIndex(const std::string& value);
    int updateLabel3Index(const std::string& value);
    int updateCreditQualityIndex(const std::string& value);
    int updateLongShortIndIndex(const std::string& value);
    int updateCoveredBondIndIndex(const std::string& value);
    int updateTrancheThicknessIndex(const std::string& value);
    int updateBbRwIndex(const std::string& value);

private:
    void insertCrifRecord(const SlimCrifRecord& record, bool aggregateDifferentAmountCurrencies = false);
    void addFrtbCrifRecord(const SlimCrifRecord& record, bool aggregateDifferentAmountCurrencies = false,
                           bool sortFxVolQualifer = true);
    void addSimmCrifRecord(const SlimCrifRecord& record, bool aggregateDifferentAmountCurrencies = false,
                           bool sortFxVolQualifer = true);
    void addSimmParameterRecord(const SlimCrifRecord& record, bool aggregateDifferentAmountCurrencies = false);
    void updateAmountExistingRecord(SlimCrifRecordContainer::nth_index_iterator<0>::type it,
                                    const SlimCrifRecord& record);

    boost::bimap<int, std::string> tradeIdIndex_, tradeTypeIndex_, qualifierIndex_, bucketIndex_, label1Index_,
        label2Index_, currencyIndex_, resultCurrencyIndex_, endDateIndex_, label3Index_, creditQualityIndex_, longShortIndIndex_,
        coveredBondIndIndex_, trancheThicknessIndex_, bbRwIndex_;
    boost::bimap<int, ore::data::NettingSetDetails> nettingSetDetailsIndex_;

    CrifType type_ = CrifType::Empty;
    SlimCrifRecordContainer records_;

    // SIMM members
    //! Set of netting set IDs that have been loaded
    std::set<ore::data::NettingSetDetails> nettingSetDetails_;
    mutable bool aggregate_ = false;
};

} // namespace analytics
} // namespace ore
