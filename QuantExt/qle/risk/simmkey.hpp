/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

/*! \file simmkey.hpp
    \brief SIMM risk factor key
*/

#include <qle/risk/simmconfiguration.hpp>

namespace QuantExt {

    class SimmKey {
    public:
        SimmKey() {}
        SimmKey(const RiskType type, const string& qualifier, const string& bucket, const string& label1,
                const string& label2, const Real amount, const string& amountCurrency, const Real amountUSD,
                const ProductClass productClass, const string& tradeID, const string& portfolioID);

        const RiskClass& type() const { return type_; }
        const string& qualifier() const { return qualifier_; }
        const string& bucket() const { return bucket_; }
        const string &label1() const { return label1_; }
        const string &label2() const { return label2_; }
        const Real &amount() const { return amount_; }
        const string &amountCurrency() const { return amountCurrency_; }
        const Real &amountUSD() const { return amountUSD_; }
        const ProductClass &productClass() const { return productClass_; }
        const string &tradeID() const { return tradeID_; }
        const string &portfolioID() const { return portfolioID_; }

        RiskClass& type() { return type_; }
        string& qualifier() { return qualifier_; }
        string bucket() { return bucket_; }
        string &label1() { return label1_; }
        string &label2() { return label2_; }
        Real &amount() { return amount_; }
        string &amountCurrency() { return amountCurrency_; }
        Real &amountUSD() { return amountUSD_; }
        ProductClass &productClass() { return productClass_; }
        string &tradeID() { return tradeID_; }
        string &portfolioID() { return portfolioID_; }

    private:
        const RiskType type_;
        const ProductClass productClass_;
        const string qualifier_, label1_, label2_, amountCurrency_, tradeID_, portfolioID_;
    };

} // namespace QuantExt
