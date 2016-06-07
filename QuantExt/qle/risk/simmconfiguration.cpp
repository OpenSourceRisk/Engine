/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/risk/simmconfiguration.hpp>

namespace QuantExt {

    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::RiskClass x) {
        switch (x) {
        case SimmConfiguration::RiskClass_InterestRate:
            return out << "InterestRate";
        case SimmConfiguration::RiskClass_CreditQualifying:
            return out << "CreditQualifying";
        case SimmConfiguration::RiskClass_CreditNonQualifying:
            return out << "CreditNonQualifying";
        case SimmConfiguration::RiskClass_Equity:
            return out << "Equity";
        case SimmConfiguration::RiskClass_Commodity:
            return out << "Commodity";
        case SimmConfiguration::RiskClass_FX:
            return out << "FX";
        default:
            return out << "Unknown simm risk class (" << x << ")";
        }
    }

    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::RiskType x) {
        switch (x) {
        case SimmConfiguration::Risk_Commodity:
            return out << "Risk_Commodity";
        case SimmConfiguration::Risk_CommodityVol:
            return out << "Risk_CommodityVol";
        case SimmConfiguration::Risk_CreditNonQ:
            return out << "Risk_CreditNonQ";
        case SimmConfiguration::Risk_CreditQ:
            return out << "Risk_CreditQ";
        case SimmConfiguration::Risk_CreditVol:
            return out << "Risk_CreditVol";
        case SimmConfiguration::Risk_CreditVolNonQ:
            return out << "Risk_CreditVolNonQ";
        case SimmConfiguration::Risk_Equity:
            return out << "Risk_Equity";
        case SimmConfiguration::Risk_EquityVol:
            return out << "Risk_EquityVol";
        case SimmConfiguration::Risk_FX:
            return out << "Risk_FX";
        case SimmConfiguration::Risk_FXVol:
            return out << "Risk_FXVol";
        case SimmConfiguration::Risk_Inflation:
            return out << "Risk_Inflation";
        case SimmConfiguration::Risk_IRCurve:
            return out << "Risk_IRCurve";
        case SimmConfiguration::Risk_IRVol:
            return out << "Risk_IRVol";
        default:
            return out << "Unkonwn simm risk type (" << x << ")";
        }
    }

    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::MarginType x) {
        switch (x) {
        case SimmConfiguration::Delta:
            return out << "Delta";
        case SimmConfiguration::Vega:
            return out << "Vega";
        case SimmConfiguration::Curvature:
            return out << "Curvature";
        default:
            return out << "Unknown simm margin type (" << x << ")";
        }
    }

    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::ProductClass x) {
        switch (x) {
        case SimmConfiguration::ProductClass_RatesFX:
            return out << "RateFX";
        case SimmConfiguration::ProductClass_Credit:
            return out << "Credit";
        case SimmConfiguration::ProductClass_Equity:
            return out << "Equity";
        case SimmConfiguration::ProductClass_Commodity:
            return out << "Commodity";
        default:
            return out << "Unknown simm product class (" << x << ")";
        }
    }

} // namespace QuantExt
