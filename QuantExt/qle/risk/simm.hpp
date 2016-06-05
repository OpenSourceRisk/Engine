/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file simmdata.hpp
    \brief Collection of SIMM risk factor keys
*/

#ifndef quantext_simm_hpp
#define quantext_simm_hpp

#include <qle/risk/simmdata.hpp>

#incldue < boost / tuple / tuple.hpp >

namespace QuantExt {

    class Simm {
    public:
        enum RiskClass {
            InterestRate = 0,
            CreditQualifying = 1,
            CreditNonQualifying = 2,
            Equity = 3,
            Commodity = 4,
            FX = 5
        };
        enum MarginType {
            Delta = 0,
            Vega = 1,
            Curvature = 2
        }

        Simm(const boost::shared_ptr<SimmData>& data) : data_(data) {
            calculate();
        }

        Real initialMargin() { return totalInitialMargin_; }
        Real initialMargin(const RiskClass c, const MarginType m) { return initialMargin_.at(std::make_pair(c, m)); }

    private:
        void calculate();
        const boost::shared_ptr<SimmData> data_;
        std::map<boost::tuple<RiskClass, SimmConfiguration::ProductClass, MarginType>, Real> initialMargin_;
        Real totalInitialMargin_;
    };

} // namespace QuantExt

#endif
