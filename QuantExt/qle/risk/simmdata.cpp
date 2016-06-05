/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/risk/simmdata.hpp>

namespace QuantExt {

    SimmData::SimmData(const boost::shared_ptr<SimmConfiguration>& config, const bool useProductClasses)
        : config_(config), useProductClasses_(useProductClasses),
          numberOfProductClasses_(useProductClasses ? config->numberOfProductClasses : 1) {

        for (Size t = 0; t < config_->numberOfRiskTypes; ++t) {
            for (Size p = 0; p < numberOfProductClasses_; ++p) {
                SimmConfiguration::RiskType key = SimmConfiguration::RiskType(t);
                RiskTypeData tmp;
                numberOfBuckets_[key] = config_->buckets(key).size();
                tmp.resize(numberOfBuckets_[key]);
                for (Size i = 0; i < tmp.size(); ++i) {
                    numberOfLabels1_[key] = config_->labels1(key).size();
                    tmp[i].resize(numberOfLabels1_[key]);
                    for (Size j = 0; j < tmp[i].size(); ++j) {
                        numberOfLabels2_[key] = config_->labels2(key).size();
                        tmp[i][j].resize(numberOfLabels2_[key]);
                        for (Size k = 0; k < tmp[i][j].size(); ++k) {
                            // TODO revisit this, hardcoded for the moment ...
                            tmp[i][j][k].reserve(100);
                        }
                    }
                }
            }
        }
    }

    void SimmData::check(const SimmConfiguration::RiskType t, const SimmConfiguration::ProductClass productClass,
                         const Size bucket, const Size label1, const Size label2) const {
        QL_REQUIRE(productClass < numberOfProductClasses(), "ProductClass << " << productClass << " out of range 0..."
                                                                               << numberOfProductClasses());
        QL_REQUIRE(bucket < numberOfBuckets(t), "RiskType " << t << ", bucket (" << bucket << ") out of range 0..."
                                                            << numberOfBuckets(t));
        QL_REQUIRE(label1 < numberOfLabels1(t), "RiskType " << t << ", label1 (" << label1 << ") out of range 0..."
                                                            << numberOfLabels1(t));
        QL_REQUIRE(label2 < numberOfLabels2(t), "RiskType " << t << ", label2 (" << label1 << ") out of range 0..."
                                                            << numberOfLabels2(t));
    }

    const Real& SimmData::amount(const SimmConfiguration::RiskType t,
                                 const SimmConfiguration::ProductClass productClass, const Size qualifier,
                                 const Size bucket, const Size label1, const Size label2) const {
        check(t, productClass, bucket, label1, label2);
        QL_REQUIRE(qualifier < numberOfQualifiers(t), "RiskType " << t << ", qualifier (" << qualifier
                                                                  << ") out of range 0..." << numberOfQualifiers(t));
        return data_.at(std::make_pair(t, productClass))[bucket][label1][label2][qualifier];
    }

    Real& SimmData::amount(const SimmConfiguration::RiskType t, const SimmConfiguration::ProductClass productClass,
                           const Size qualifier, const Size bucket, const Size label1, const Size label2) {
        check(t, productClass, bucket, label1, label2);
        std::vector<Real>& v = data_.at(std::make_pair(t, productClass))[bucket][label1][label2];
        Size s = v.size();
        if (qualifier >= s) {
            v.resize(qualifier + 1);
            for (Size i = s; i < v.size(); ++i) {
                v[i] = 0.0;
            }
        }
        return v[qualifier];
    }

    SimmDataByKey::SimmDataByKey(const boost::shared_ptr<SimmConfiguration>& config, bool useProductClasses)
        : SimmData(config, useProductClasses), reportingCurrency_("") {
        for (Size t = 0; t < config_->numberOfRiskTypes; ++t) {
            SimmConfiguration::RiskType key = SimmConfiguration::RiskType(t);
            buckets_[key] = config_->buckets(key);
            labels1_[key] = config_->labels1(key);
            labels2_[key] = config_->labels2(key);
        }
    }

    void SimmDataByKey::addKey(const SimmKey& key) {
        std::vector<string>::const_iterator it;
        SimmConfiguration::RiskType t = key.riskType();
        it = std::find(buckets_[t].begin(), buckets_[t].end(), key.bucket());
        QL_REQUIRE(it != buckets_[t].end(), "bucket " << key.bucket() << " not found, key can not be added");
        Size bucket = it - buckets_[t].begin();
        it = std::find(labels1_[t].begin(), labels1_[t].end(), key.label1());
        QL_REQUIRE(it != buckets_[t].end(), "label1 " << key.label1() << " not found, key can not be added");
        Size label1 = it - labels1_[t].begin();
        it = std::find(labels2_[t].begin(), labels2_[t].end(), key.label2());
        QL_REQUIRE(it != labels2_[t].end(), "label2 " << key.label2() << " not found, key can not be added");
        Size label2 = it - labels2_[t].begin();
        it = std::find(qualifiers_[t].begin(), qualifiers_[t].end(), key.qualifier());
        Size qualifier = it - qualifiers_[t].begin();
        if (it == qualifiers_[t].end()) {
            qualifiers_[t].push_back(key.qualifier());
        }
        SimmConfiguration::ProductClass p =
            useProductClasses() ? key.productClass() : SimmConfiguration::ProductClass(0);
        if (reportingCurrency_ == "")
            reportingCurrency_ = key.amountCurrency();
        else
            QL_REQUIRE(reportingCurrency_ == key.amountCurrency(),
                       "key has reporting currency " << key.amountCurrency() << ", but deduced " << reportingCurrency_
                                                     << " from first key added, this key is not added ");
        amount(t, p, qualifier, bucket, label1, label2) = key.amount();
    }

} // namespace QuantExt
