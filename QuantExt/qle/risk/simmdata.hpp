/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file simmkey.hpp
    \brief Collection of SIMM risk factor keys
*/

#include <qle/risk/simmkey.hpp>

namespace QuantExt {

    class SimmData {
    public:
        SimmData(const SimmConfiguration& config) : config_(config) {}
        virtual addKey(const SimmKey& key) = 0;

    private:
        const SimmConfiguration config_;
    };

    class SimmDataPortfolio : public SimmData {
    public:
        SimmDataPortfolio(const SimmConfiguration& config = SimmConfiguration_ISDA_V120(),
                          bool useProductClasses = false)
            : SimmDataPortfolio(config), useProductClasses_(useProductClasses) {}

        addKey(const SimmKey& key);

    private:
        const bool useProductClasses_;
        // translate index to string
        std::map<RiskType, std::vector<string> > qualifier_, bucket_, label1_, label2_;
        // bucket, label1, label2, qualifer
        typedef RiskTypeData std::vector<std::vector<std::vector<std::vector<Real> > > >;
        typedef ProductClassData std::map<std::pair<ProductClass, RiskType>, RiskTypeData>;
        typedef PortfolioProductClassData std::vector<ProductClassData>;
    };

} // namespace QuantExt
