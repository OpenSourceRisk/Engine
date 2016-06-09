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

    /*! Base data class for SIMM, not that no FX risk in reporting currency should be added here,
      although it is present in the data file usually. SimmDataByKey handles this automatically.
      Note that the risk type Risk_Inflation is considered to be part of Risk_IRCurve throughout
      this class, i.e. the parameter RiskType in the methods of this class should always be Risk_IRCurve
      for inflation. */
    class SimmData {
    public:
        typedef SimmConfiguration::RiskClass RiskClass;
        typedef SimmConfiguration::RiskType RiskType;
        typedef SimmConfiguration::MarginType MarginType;
        typedef SimmConfiguration::ProductClass ProductClass;

        SimmData(const boost::shared_ptr<SimmConfiguration>& config = boost::make_shared<SimmConfiguration_ISDA_V315>(),
                 const bool useProductClasses = false);

        Size numberOfQualifiers(const RiskType t, const ProductClass p) const {
            return numberOfQualifiers_.at(std::make_pair(t, p));
        }
        Size bucket(const RiskType t, const ProductClass p, const Size qualifier) {
            const std::vector<Size>& b = buckets_.at(std::make_pair(t, p));
            QL_REQUIRE(qualifier < b.size(), "qualifier with index " << qualifier << " out of range 0..."
                                                                     << b.size() - 1);
            // if only inflation is present, bucket mght be still null
            return b[qualifier] == Null<Size>() ? 0 : b[qualifier];
        }
        bool useProductClasses() const { return useProductClasses_; }
        Size numberOfProductClasses() const { return numberOfProductClasses_; }
        // retrieve amount
        Real amount(const RiskType t, const ProductClass p, const Size qualifier, const Size label1,
                           const Size label2) const;
        // write amount
        Real& amount(const RiskType t, const ProductClass p, const Size bucket, const Size qualifier,
                     const Size label1, const Size label2);

        const boost::shared_ptr<SimmConfiguration> configuration() { return config_; }

    protected:
        void check(const RiskType t, const ProductClass p, const Size label1, const Size label2) const;
        // label1, label2, qualifer => amount
        typedef std::vector<std::vector<std::vector<Real> > > RiskTypeData;
        // productClass, riskType => riskTypeData
        typedef std::map<std::pair<RiskType, ProductClass>, RiskTypeData> ProductClassData;

        const boost::shared_ptr<SimmConfiguration> config_;
        const bool useProductClasses_;
        const Size numberOfProductClasses_;
        std::map<std::pair<RiskType, ProductClass>, Size> numberOfQualifiers_;
        // qualifier => buckets
        std::map<std::pair<RiskType, ProductClass>, std::vector<Size> > buckets_;
        ProductClassData data_;
    };

    /*! Data class for SIMM based on keys. FX risk in reporting currency is filtered out (i.e. not added)
      automatically.  Note that the risk type Risk_Inflation is considered to be part of Risk_IRCUrve
      as above */
    class SimmDataByKey : public SimmData {
    public:
        SimmDataByKey(
            const boost::shared_ptr<SimmConfiguration>& config = boost::make_shared<SimmConfiguration_ISDA_V315>(),
            bool useProductClasses = false);

        void addKey(const SimmKey& key);
        const std::vector<string>& qualifiers(const RiskType t, const ProductClass p) const {
            return qualifiers_.at(std::make_pair(t, p));
        }

    private:
        // translate index to string
        std::map<std::pair<RiskType, ProductClass>, std::vector<string> > qualifiers_;
        std::string reportingCurrency_;
    };

} // namespace QuantExt

#endif
