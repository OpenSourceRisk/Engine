/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#pragma once

#include <ored/portfolio/basketdata.hpp>
#include <ored/portfolio/creditdefaultswapdata.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ql/instruments/creditdefaultswap.hpp>

namespace ore {
namespace data {

class IndexCreditDefaultSwapData : public ore::data::CreditDefaultSwapData {
public:
    //! Default constructor
    IndexCreditDefaultSwapData();

    using PPT = QuantExt::CreditDefaultSwap::ProtectionPaymentTime;

    //! Detailed constructor
    IndexCreditDefaultSwapData(const std::string& creditCurveId,
        const BasketData& basket,
        const ore::data::LegData& leg,
        const bool settlesAccrual = true,
        const PPT protectionPaymentTime = PPT::atDefault,
        const QuantLib::Date& protectionStart = QuantLib::Date(),
        const QuantLib::Date& upfrontDate = QuantLib::Date(),
        const QuantLib::Real upfrontFee = QuantLib::Null<QuantLib::Real>(),
        const QuantLib::Date& tradeDate = QuantLib::Date(),
        const std::string& cashSettlementDays = "",
        const bool rebatesAccrual = true);

    //! \name XMLSerializable interface
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const BasketData& basket() const { return basket_; }
    //@}

    /*! Get credit curve id with term suffix "_5Y". If the creditCurveId contains such a suffix already
        this is used. Otherwise we try to imply it from the schedule. If that is not possible, the
	creditCurveId without tenor is returned. */
    std::string creditCurveIdWithTerm() const;

    /*! If set this is used to derive the term instead of the schedule start date. A concession to bad
        trade setups really, where the start date is not set to the index effective date */
    void setIndexStartDateHint(const QuantLib::Date& d) const { indexStartDateHint_ = d; }

    /*! Get the index start date hint, or null if it was never set */
    const QuantLib::Date& indexStartDateHint() const { return indexStartDateHint_; }

protected:
    //! \name CreditDefaultSwapData interface
    //@{
    void check(ore::data::XMLNode* node) const override;
    ore::data::XMLNode* alloc(ore::data::XMLDocument& doc) const override;
    //@}

private:
    BasketData basket_;
    mutable QuantLib::Date indexStartDateHint_;
};

} // namespace data
} // namespace ore
