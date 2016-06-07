/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file simmconfiguration.hpp
    \brief Configuration of simm
*/

#ifndef quantext_simm_configuration_hpp
#define quantext_simm_configuration_hpp

#include <ql/errors.hpp>
#include <ql/types.hpp>

#include <algorithm>
#include <string>
#include <vector>

using std::string;

using namespace QuantLib;

namespace QuantExt {

    class SimmConfiguration {
    public:
        enum RiskClass {
            RiskClass_InterestRate = 0,
            RiskClass_CreditQualifying = 1,
            RiskClass_CreditNonQualifying = 2,
            RiskClass_Equity = 3,
            RiskClass_Commodity = 4,
            RiskClass_FX = 5
        };
        enum RiskType {
            Risk_Commodity = 0,
            Risk_CommodityVol = 1,
            Risk_CreditNonQ = 2,
            Risk_CreditQ = 3,
            Risk_CreditVol = 4,
            Risk_CreditVolNonQ = 5,
            Risk_Equity = 6,
            Risk_EquityVol = 7,
            Risk_FX = 8,
            Risk_FXVol = 9,
            Risk_Inflation = 10,
            Risk_IRCurve = 11,
            Risk_IRVol = 12
        };
        enum MarginType { Delta = 0, Vega = 1, Curvature = 2 };
        enum ProductClass {
            ProductClass_RatesFX = 0,
            ProductClass_Credit = 1,
            ProductClass_Equity = 2,
            ProductClass_Commodity = 3
        };

        static const Size numberOfRiskClasses = 6;
        static const Size numberOfRiskTypes = 13;
        static const Size numberOfMarginTypes = 3;
        static const Size numberOfProductClasses = 4;

        virtual const string& name() const = 0;
        virtual const std::vector<string>& buckets(const RiskType t) const = 0;
        virtual Size residualBucket(const RiskType t) const = 0;
        virtual const std::vector<string>& labels1(const RiskType t) const = 0;
        virtual const std::vector<string>& labels2(const RiskType t) const = 0;

        virtual Real weight(const RiskType t, const Size bucketIdx, const Size label1Idx) const = 0;
        virtual Real correlationLabels1(const RiskType t, const Size i, const Size j) const = 0;
        virtual Real correlationLabels2(const RiskType t, const Size i, const Size j) const = 0;
        virtual Real correlationBuckets(const RiskType t, const Size i, const Size j) const = 0;
        virtual Real correlationQualifiers(const RiskType t) const = 0;
        virtual Real correlationWithinBucket(const RiskType t, const Size i) const = 0;
        virtual Real correlationRiskClasses(const RiskClass c, const RiskClass d) const = 0;

        // TODO, revisit later, but as of V315 there are no threasholds defined anyway,
        // so we keep the interface general for the moment
        virtual Real concentrationThreshold() const { return QL_MAX_REAL; }
    };

    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::RiskClass x);
    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::RiskType x);
    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::MarginType x);
    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::ProductClass x);

} // namespace QuantExt

#endif
