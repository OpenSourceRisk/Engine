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
        SimmData(const boost::shared_ptr<SimmConfiguration>& config = boost::make_shared<SimmConfiguration_ISDA_V315>(),
                 const bool useProductClasses = false);

        Size numberOfQualifiers(const SimmConfiguration::RiskType t) const { return numberOfQualifiers_.at(t); }
        Size numberOfBuckets(const SimmConfiguration::RiskType t) const { return numberOfBuckets_.at(t); }
        Size numberOfLabels1(const SimmConfiguration::RiskType t) const { return numberOfLabels1_.at(t); }
        Size numberOfLabels2(const SimmConfiguration::RiskType t) const { return numberOfLabels2_.at(t); }

        bool useProductClasses() const { return useProductClasses_; }
        Size numberOfProductClasses() const { return numberOfProductClasses_; }

        const Real& amount(const SimmConfiguration::RiskType t, const SimmConfiguration::ProductClass productClass,
                           const Size qualifier, const Size bucket, const Size label1, const Size label2) const;
        Real& amount(const SimmConfiguration::RiskType t, const SimmConfiguration::ProductClass productClass,
                     const Size qualifier, const Size bucket, const Size label1, const Size label2);

    protected:
        void check(const SimmConfiguration::RiskType t, const SimmConfiguration::ProductClass productClass,
                   const Size bucket, const Size label1, const Size label2) const;
        // bucket, label1, label2, qualifer => amount
        typedef std::vector<std::vector<std::vector<std::vector<Real> > > > RiskTypeData;
        // productClass, riskType => riskTypeData
        typedef std::map<std::pair<SimmConfiguration::RiskType, SimmConfiguration::ProductClass>, RiskTypeData>
            ProductClassData;

        const boost::shared_ptr<SimmConfiguration> config_;
        const bool useProductClasses_;
        const Size numberOfProductClasses_;
        std::map<SimmConfiguration::RiskType, Size> numberOfQualifiers_, numberOfBuckets_, numberOfLabels1_,
            numberOfLabels2_;
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
        std::map<SimmConfiguration::RiskType, std::vector<string> > qualifiers_, buckets_, labels1_, labels2_;
        std::string reportingCurrency_;
    };

} // namespace QuantExt

#endif
