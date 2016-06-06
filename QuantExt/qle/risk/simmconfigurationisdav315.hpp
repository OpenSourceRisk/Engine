/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file simmconfigurationisdav120.hpp
    \brief Configuration of simm according to the ISDA spec v3.15 (7 April 2016)
*/
#ifndef quantext_simm_configuration_isdav315_hpp
#define quantext_simm_configuration_isdav315_hpp

#include <qle/risk/simmconfiguration.hpp>

#include <map>

namespace QuantExt {

    class SimmConfiguration_ISDA_V315 : public SimmConfiguration {
    public:
        SimmConfiguration_ISDA_V315();
        const std::string& name() const { return name_; }
        const std::vector<string>& buckets(RiskType t) const { return buckets_.at(t); }
        bool hasResidualBucket(RiskType t) const {
            if (t == Risk_FX || t == Risk_FXVol || t == Risk_Inflation)
                return false;
            else
                return true;
        }
        const std::vector<string>& labels1(RiskType t) const { return labels1_.at(t); }
        const std::vector<string>& labels2(RiskType t) const { return labels2_.at(t); }

    private:
        const std::string name_;
        std::map<RiskType, std::vector<string> > buckets_, labels1_, labels2_;
    };

} // namespace QuantExt

#endif
