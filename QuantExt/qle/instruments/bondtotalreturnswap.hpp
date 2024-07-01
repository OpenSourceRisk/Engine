/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/indexes/bondindex.hpp>
#include <qle/indexes/fxindex.hpp>

#include <ql/handle.hpp>
#include <ql/instrument.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/interestrate.hpp>
#include <ql/position.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/types.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Bond TRS class
class BondTRS : public Instrument {
public:
    class arguments;
    using engine = GenericEngine<BondTRS::arguments, BondTRS::results>;
    using results = BondTRS::results;
    //! Constructor
    BondTRS(const QuantLib::ext::shared_ptr<QuantExt::BondIndex>& bondIndex, const Real bondNotional, const Real initialPrice,
            const std::vector<Leg>& fundingLeg, const bool payTotalReturnLeg, const std::vector<Date>& valuationDates,
            const std::vector<Date>& paymentDates, const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr,
            bool payBondCashFlowsImmediately = false, const Currency& fundingCurrency = Currency(),
            const Currency& bondCurrency = Currency());

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<QuantExt::BondIndex>& bondIndex() const { return bondIndex_; }
    const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex() const { return fxIndex_; }
    Real bondNotional() const { return bondNotional_; }
    const std::vector<Leg>& fundingLeg() const { return fundingLeg_; }
    Real initialPrice() const { return initialPrice_; }
    bool payTotalReturnLeg() const { return payTotalReturnLeg_; }
    const Leg& returnLeg() const { return returnLeg_; }
    bool payBondCashFlowsImmediately() const { return payBondCashFlowsImmediately_; }
    const std::vector<Date>& valuationDates() const { return valuationDates_; }
    const std::vector<Date>& paymentDates() const { return paymentDates_; }
    //@}

private:
    QuantLib::ext::shared_ptr<QuantExt::BondIndex> bondIndex_;
    Real bondNotional_;
    Real initialPrice_;
    std::vector<Leg> fundingLeg_;
    bool payTotalReturnLeg_;
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex_;
    bool payBondCashFlowsImmediately_;
    Currency fundingCurrency_, bondCurrency_;
    std::vector<Date> valuationDates_;
    std::vector<Date> paymentDates_;
    //
    Leg returnLeg_;
};

//! \ingroup instruments
class BondTRS::arguments : public virtual PricingEngine::arguments {
public:
    QuantLib::ext::shared_ptr<QuantExt::BondIndex> bondIndex;
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex;
    Real bondNotional;
    std::vector<Leg> fundingLeg;
    Leg returnLeg;
    bool payTotalReturnLeg;
    bool payBondCashFlowsImmediately;
    Currency fundingCurrency, bondCurrency;
    void validate() const override {}
    std::vector<Date> paymentDates;
    std::vector<Date> valuationDates;
};

} // namespace QuantExt
