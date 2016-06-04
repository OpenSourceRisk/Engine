/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file simmconfiguration.hpp
    \brief Configuration of simm
*/

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
        enum RiskType {
            Risk_Commodity = 0,
            Risk_CommodityVol = 1,
            Risk_CreditNonQ = 2,
            Risk_CreditQ = 3,
            Risk_CreditVol = 4,
            Risk_Equity = 5,
            Risk_EquityVol = 6,
            Risk_FX = 7,
            Risk_FXVol = 8,
            Risk_Inflation = 9,
            Risk_IRCurve = 10,
            Risk_IRVol = 11
        };

        enum ProductClass { All, RatesFX, Credit, Equity, Commodity };

        virtual const string& name() const = 0;
        virtual const std::vector<string>& buckets(RiskType t) const = 0;
        virtual const std::vector<string>& labels1(RiskType t) const = 0;
        virtual const std::vector<string>& labels2(RiskType t) const = 0;
    };

} // namespace QuantExt
