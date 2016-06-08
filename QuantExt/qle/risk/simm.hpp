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
        typedef SimmConfiguration::ProductClass ProductClass;
        typedef SimmConfiguration::RiskClass RiskClass;
        typedef SimmConfiguration::MarginType MarginType;
        typedef SimmConfiguration::RiskType RiskType;

        Simm(const boost::shared_ptr<SimmData>& data) : data_(data) { calculate(); }

        Real initialMargin(const ProductClass p, const RiskClass c, const MarginType m) {
            try {
                boost::tuple<ProductClass, RiskClass, MarginType> key = boost::make_tuple(p, c, m);
                return initialMargin_.at(key);
            } catch (...) {
                return 0.0;
            }
        }

        //! refresh results after data has changed
        void calculate();

        const boost::shared_ptr<SimmData> data() const { return data_; }

    private:
        Real marginIR(RiskType t, ProductClass p);
        Real curvatureMarginIR(ProductClass p);
        const boost::shared_ptr<SimmData> data_;
        std::map<boost::tuple<ProductClass, RiskClass, MarginType>, Real> initialMargin_;
    };

} // namespace QuantExt

#endif
