/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file portfolio/creditlinkedswap.hpp
    \brief credit linked swap data model
    \ingroup portfolio
*/

#pragma once

#include <qle/instruments/creditlinkedswap.hpp>

#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {
using ore::data::XMLSerializable;
using std::string;
using std::vector;

class CreditLinkedSwap : public Trade {
public:
    CreditLinkedSwap() : Trade("CreditLinkedSwap") {}
    CreditLinkedSwap(const std::string& creditCurveId, const bool settlesAccrual, const Real fixedRecoveryRate,
                     const QuantExt::CreditDefaultSwap::ProtectionPaymentTime& defaultPaymentTime,
                     const std::vector<LegData>& independentPayments, const std::vector<LegData>& contingentPayments,
                     const std::vector<LegData>& defaultPayments, const std::vector<LegData>& recoveryPayments)
        : Trade("CreditLinkedSwap"), creditCurveId_(creditCurveId), settlesAccrual_(settlesAccrual),
          fixedRecoveryRate_(fixedRecoveryRate), defaultPaymentTime_(defaultPaymentTime),
          independentPayments_(independentPayments), contingentPayments_(contingentPayments),
          defaultPayments_(defaultPayments), recoveryPayments_(recoveryPayments) {}

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    void build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) override;

    QuantLib::Real notional() const override;

private:
    std::string creditCurveId_;
    bool settlesAccrual_;
    double fixedRecoveryRate_;
    QuantExt::CreditDefaultSwap::ProtectionPaymentTime defaultPaymentTime_;
    std::vector<LegData> independentPayments_;
    std::vector<LegData> contingentPayments_;
    std::vector<LegData> defaultPayments_;
    std::vector<LegData> recoveryPayments_;
};

} // namespace data
} // namespace ore
