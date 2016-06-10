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

        Simm(const boost::shared_ptr<SimmData>& data) : data_(data), phiInv995Sq_(6.634896601021214) { calculate(); }

        Real initialMargin(const ProductClass p, const RiskClass c, const MarginType m) const;
        Real initialMargin(const ProductClass p, const RiskClass c) const;
        Real initialMargin(const ProductClass p) const;
        Real initialMargin() const;

        //! refresh results after data has changed
        void calculate();

        const boost::shared_ptr<SimmData> data() const { return data_; }

    private:
        Real marginIR(RiskType t, ProductClass p) const;
        Real curvatureMarginIR(ProductClass p) const;
        Real marginGeneric(RiskType t, ProductClass p) const;
        Real curvatureMarginGeneric(RiskType t, ProductClass p) const;
        const boost::shared_ptr<SimmData> data_;
        const Real phiInv995Sq_;
        std::map<boost::tuple<ProductClass, RiskClass, MarginType>, Real> initialMargin_;
    };

} // namespace QuantExt

#endif
