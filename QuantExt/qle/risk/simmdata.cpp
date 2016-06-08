/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/risk/simmdata.hpp>

#include <iostream>

namespace QuantExt {

    SimmData::SimmData(const boost::shared_ptr<SimmConfiguration>& config, const bool useProductClasses)
        : config_(config), useProductClasses_(useProductClasses),
          numberOfProductClasses_(useProductClasses ? config->numberOfProductClasses : 1) {

        for (Size t = 0; t < config_->numberOfRiskTypes; ++t) {
            RiskType rt = RiskType(t);
            for (Size p = 0; p < numberOfProductClasses_; ++p) {
                ProductClass pc = ProductClass(p);
                buckets_[std::make_pair(rt, pc)] = std::vector<Size>(0);
                numberOfQualifiers_[std::make_pair(rt, pc)] = 0;
                RiskTypeData tmp;
                tmp.resize(config_->labels1(rt).size());
                QL_REQUIRE(tmp.size() > 0, "risk type " << rt << " labels1 has size 0");
                for (Size i = 0; i < tmp.size(); ++i) {
                    tmp[i].resize(config_->labels2(rt).size());
                    QL_REQUIRE(tmp[i].size() > 0, "risk type " << rt << " labels2 has size 0");
                    for (Size k = 0; k < tmp[i].size(); ++k) {
                        // TODO revisit this, hardcoded for the moment ...
                        tmp[i][k].reserve(100);
                    }
                }
                data_[std::make_pair(rt, pc)] = tmp;
            }
        }
    }

    void SimmData::check(const RiskType t, const ProductClass p, const Size label1, const Size label2) const {
        QL_REQUIRE(p < numberOfProductClasses(), "ProductClass << " << p << " out of range 0..."
                                                                    << numberOfProductClasses());
        QL_REQUIRE(label1 < config_->labels1(t).size(),
                   "RiskType " << t << ", label1 (" << label1 << ") out of range 0..." << config_->labels1(t).size());
        QL_REQUIRE(label2 < config_->labels2(t).size(),
                   "RiskType " << t << ", label2 (" << label1 << ") out of range 0..." << config_->labels2(t).size());
    }

    Real SimmData::amount(const RiskType t, const ProductClass p, const Size qualifier, const Size label1,
                          const Size label2) const {
        check(t, p, label1, label2);
        QL_REQUIRE(qualifier < numberOfQualifiers(t, p), "RiskType " << t << ", ProductClass " << p << ", qualifier ("
                                                                     << qualifier << ") out of range 0..."
                                                                     << numberOfQualifiers(t, p));
        const std::vector<Real>& v = data_.at(std::make_pair(t, p))[label1][label2];
        if (qualifier >= v.size())
            return 0.0;
        else
            return data_.at(std::make_pair(t, p))[label1][label2][qualifier];
    }

    Real& SimmData::amount(const RiskType t, const ProductClass p, const Size bucket, const Size qualifier,
                           const Size label1, const Size label2) {
        check(t, p, label1, label2);
        std::vector<Real>& v = data_.at(std::make_pair(t, p))[label1][label2];
        Size s = v.size();
        if (qualifier >= s) {
            v.resize(qualifier + 1);
            for (Size i = s; i < v.size(); ++i) {
                v[i] = 0.0;
            }
            numberOfQualifiers_[std::make_pair(t, p)] = qualifier + 1;
        }
        QL_REQUIRE(bucket < config_->buckets(t).size(), "bucket with index " << bucket << " out of range 0..."
                                                                             << config_->buckets(t).size());
        std::vector<Size>& b = buckets_.at(std::make_pair(t, p));
        s = b.size();
        if (qualifier >= s) {
            b.resize(qualifier + 1);
            for (Size i = s; i < b.size(); ++i) {
                b[i] = Null<Size>();
            }
        }
        QL_REQUIRE(b[qualifier] == Null<Size>() || b[qualifier] == bucket,
                   "two different buckets (" << b[qualifier] << ", " << bucket << ") for qualifier " << qualifier);
        b[qualifier] = bucket;
        return v[qualifier];
    }

    SimmDataByKey::SimmDataByKey(const boost::shared_ptr<SimmConfiguration>& config, bool useProductClasses)
        : SimmData(config, useProductClasses), reportingCurrency_("") {}

    void SimmDataByKey::addKey(const SimmKey& key) {
        std::vector<string>::const_iterator it;
        RiskType t = key.riskType();
        ProductClass p = useProductClasses() ? key.productClass() : ProductClass(0);
        std::pair<RiskType, ProductClass> tp(t, p);
        it = std::find(config_->buckets(t).begin(), config_->buckets(t).end(), key.bucket());
        QL_REQUIRE(it != config_->buckets(t).end(), "bucket \"" << key.bucket()
                                                                << "\" not found, key can not be added");
        Size bucket = it - config_->buckets(t).begin();
        it = std::find(config_->labels1(t).begin(), config_->labels2(t).end(), key.label1());
        QL_REQUIRE(it != config_->labels1(t).end(), "label1 \"" << key.label1()
                                                                << "\" not found, key can not be added");
        Size label1 = it - config_->labels1(t).begin();
        it = std::find(config_->labels2(t).begin(), config_->labels2(t).end(), key.label2());
        QL_REQUIRE(it != config_->labels2(t).end(), "label2 \"" << key.label2()
                                                                << "\" not found, key can not be added");
        Size label2 = it - config_->labels2(t).begin();
        it = std::find(qualifiers_[tp].begin(), qualifiers_[tp].end(), key.qualifier());
        Size qualifier = it - qualifiers_[tp].begin();
        if (it == qualifiers_[tp].end()) {
            qualifiers_[tp].push_back(key.qualifier());
        }
        if (reportingCurrency_ == "")
            reportingCurrency_ = key.amountCurrency();
        else
            QL_REQUIRE(reportingCurrency_ == key.amountCurrency(),
                       "key has reporting currency " << key.amountCurrency() << ", but deduced " << reportingCurrency_
                                                     << " from first key added, this key is not added ");
        amount(t, p, bucket, qualifier, label1, label2) += key.amount();
    }

} // namespace QuantExt
