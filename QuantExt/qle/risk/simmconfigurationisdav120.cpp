/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file simmconfigurationisdav120.hpp
    \brief Configuration of simm according to the ISDA spec v1.20 (7 April 2016)
*/

#include <qle/risk/simmconfigurationisdav120.hpp>

#include <boost/assign/std/vector.hpp>

using namespace boost::assign;

namespace QuantExt {

    SimmConfiguration_ISDA_V120::SimmConfiguration_ISDA_V120() : name_("SIMM ISDA V120 (7 April 2016)") {
        // buckets
        std::vector<string> bucketsIRCurve, bucketsCreditQ, bucketsCreditNonQ, bucketsEquity, bucketsCommodity;
        bucketsIRCurve += "1", "2", "3";
        bucketsCreditQ += "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual";
        bucketsCreditNonQ += "1", "2", "Residual";
        bucketsEquity += "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "Residual";
        bucketsCommodity += "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16",
            "Residual";
        buckets_[Risk_IRCurve] = bucketsIRCurve;
        buckets_[Risk_CreditQ] = bucketsCreditQ;
        buckets_[Risk_CreditVol] = bucketsCreditQ;
        buckets_[Risk_CreditNonQ] = bucketsCreditNonQ;
        buckets_[Risk_Equity] = bucketsEquity;
        buckets_[Risk_EquityVol] = bucketsEquity;
        buckets_[Risk_Commodity] = bucketsCommodity;
        buckets_[Risk_CommodityVol] = bucketsCommodity;
        buckets_[Risk_IRVol] = std::vector<string>();
        buckets_[Risk_Inflation] = std::vector<string>();
        buckets_[Risk_FX] = std::vector<string>();
        buckets_[Risk_FXVol] = std::vector<string>();

        // label1
        std::vector<string> irTenor, crTenor, volTenor;
        irTenor += "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y";
        crTenor += "1y", "2y", "3y", "5y", "10y";
        volTenor += "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y";
        labels1_[Risk_IRCurve] = irTenor;
        labels1_[Risk_IRVol] = irTenor;
        labels1_[Risk_CreditQ] = crTenor;
        labels1_[Risk_CreditVol] = crTenor;
        labels1_[Risk_CreditNonQ] = crTenor;
        labels1_[Risk_Equity] = std::vector<string>();
        labels1_[Risk_EquityVol] = volTenor;
        labels1_[Risk_Commodity] = std::vector<string>();
        labels1_[Risk_CommodityVol] = volTenor;
        labels1_[Risk_Inflation] = std::vector<string>();
        labels1_[Risk_FX] = std::vector<string>();
        labels1_[Risk_FXVol] = volTenor;

        // label2
        std::vector<string> subcurve, sec;
        subcurve += "OIS", "Libor1m", "Libor3m", "Libor6m", "Libor12m", "Prime";
        sec += "Sec";
        labels2_[Risk_IRCurve] = subcurve;
        labels2_[Risk_IRVol] = std::vector<string>();
        labels2_[Risk_CreditQ] = sec;
        labels2_[Risk_CreditVol] = std::vector<string>();
        labels2_[Risk_CreditNonQ] = std::vector<string>();
        labels2_[Risk_Equity] = std::vector<string>();
        labels2_[Risk_EquityVol] = std::vector<string>();
        labels2_[Risk_Commodity] = std::vector<string>();
        labels2_[Risk_CommodityVol] = std::vector<string>();
        labels2_[Risk_Inflation] = std::vector<string>();
        labels2_[Risk_FX] = std::vector<string>();
        labels2_[Risk_FXVol] = std::vector<string>();
    }

} // namespace QuantExt
