/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file simmdata.hpp
    \brief Collection of SIMM risk factor keys
*/

#ifndef quantext_simm_data_hpp
#define quantext_simm_data_hpp

#include <qle/risk/simmconfigurationisdav315.hpp>
#include <qle/risk/simmkey.hpp>

#include <boost/make_shared.hpp>

#include <map>

namespace QuantExt {

    class SimmData {
    public:
        typedef SimmConfiguration::RiskClass RiskClass;
        typedef SimmConfiguration::RiskType RiskType;
        typedef SimmConfiguration::MarginType MarginType;
        typedef SimmConfiguration::ProductClass ProductClass;

        SimmData(const boost::shared_ptr<SimmConfiguration>& config = boost::make_shared<SimmConfiguration_ISDA_V315>(),
                 const bool useProductClasses = false);

        Size numberOfQualifiers(const RiskType t) const { return numberOfQualifiers_.at(t); }
        Size bucket(const RiskType t, const Size qualifier) {
            const std::vector<Size>& b = buckets_.at(t);
            QL_REQUIRE(qualifier < b.size(), "qualifier with index " << qualifier << " out of range 0..."
                                                                     << b.size() - 1);
            return b[qualifier];
        }
        bool useProductClasses() const { return useProductClasses_; }
        Size numberOfProductClasses() const { return numberOfProductClasses_; }
        const Real& amount(const RiskType t, const ProductClass productClass, const Size qualifier, const Size label1,
                           const Size label2) const;
        Real& amount(const RiskType t, const ProductClass productClass, const Size bucket, const Size qualifier,
                     const Size label1, const Size label2);

        const boost::shared_ptr<SimmConfiguration> configuration() { return config_; }

    protected:
        void check(const RiskType t, const ProductClass productClass, const Size label1, const Size label2) const;
        // label1, label2, qualifer => amount
        typedef std::vector<std::vector<std::vector<Real> > > RiskTypeData;
        // productClass, riskType => riskTypeData
        typedef std::map<std::pair<RiskType, ProductClass>, RiskTypeData> ProductClassData;

        const boost::shared_ptr<SimmConfiguration> config_;
        const bool useProductClasses_;
        const Size numberOfProductClasses_;
        std::map<RiskType, Size> numberOfQualifiers_;
        // qualifier => buckets
        std::map<RiskType, std::vector<Size> > buckets_;
        ProductClassData data_;
    };

    class SimmDataByKey : public SimmData {
    public:
        SimmDataByKey(
            const boost::shared_ptr<SimmConfiguration>& config = boost::make_shared<SimmConfiguration_ISDA_V315>(),
            bool useProductClasses = false);

        void addKey(const SimmKey& key);

    private:
        // translate index to string
        std::map<RiskType, std::vector<string> > qualifiers_, buckets_, labels1_, labels2_;
        std::string reportingCurrency_;
    };

} // namespace QuantExt

#endif
