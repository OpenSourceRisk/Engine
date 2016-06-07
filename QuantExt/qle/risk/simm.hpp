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

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <map>

namespace QuantExt {

    class Simm {
    public:
        typedef SimmConfiguration::RiskClass RiskClass;
        typedef SimmConfiguration::RiskType RiskType;
        typedef SimmConfiguration::MarginType MarginType;
        typedef SimmConfiguration::ProductClass ProductClass;

        Simm(const boost::shared_ptr<SimmData>& data) : data_(data) { calculate(); }

        Real initialMargin(const RiskClass c, const MarginType m, const ProductClass p) {
            boost::tuple<RiskClass, MarginType, ProductClass> key = boost::make_tuple(c, m, p);
            return initialMargin_.at(key);
        }

        //! refresh results after data has changed
        void calculate();

        const boost::shared_ptr<SimmData> data() const { return data_; }

    private:
        Real deltaMarginIR(ProductClass p);
        const boost::shared_ptr<SimmData> data_;
        std::map<boost::tuple<RiskClass, MarginType, ProductClass>, Real> initialMargin_;
    };

} // namespace QuantExt

#endif
