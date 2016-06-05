/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file simmkey.hpp
    \brief SIMM risk factor key
*/

#ifndef quantext_simm_key_hpp
#define quantext_simm_key_hpp

#include <qle/risk/simmconfiguration.hpp>

namespace QuantExt {

    class SimmKey {
    public:
        SimmKey(const SimmConfiguration::RiskType type, const SimmConfiguration::ProductClass productClass,
                const string& qualifier, const string& bucket, const string& label1, const string& label2,
                const Real amount, const string& amountCurrency, const string& tradeID)
            : type_(type), productClass_(productClass), qualifier_(qualifier), bucket_(bucket), label1_(label1),
              label2_(label2), amountCurrency_(amountCurrency), tradeID_(tradeID), amount_(amount) {}

        const SimmConfiguration::RiskType& riskType() const { return type_; }
        const string& qualifier() const { return qualifier_; }
        const string& bucket() const { return bucket_; }
        const string& label1() const { return label1_; }
        const string& label2() const { return label2_; }
        const Real& amount() const { return amount_; }
        const string& amountCurrency() const { return amountCurrency_; }
        const SimmConfiguration::ProductClass& productClass() const { return productClass_; }
        const string& tradeID() const { return tradeID_; }

    private:
        const SimmConfiguration::RiskType type_;
        const SimmConfiguration::ProductClass productClass_;
        const string qualifier_, bucket_, label1_, label2_, amountCurrency_, tradeID_;
        const Real amount_;
    };

} // namespace QuantExt

#endif
