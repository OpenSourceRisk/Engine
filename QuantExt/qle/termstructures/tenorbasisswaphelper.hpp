/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file tenorbasisswaphelper.hpp
    \brief Single currency tenor basis swap helper
    \ingroup termstructures
*/

#ifndef quantext_tenor_basis_swap_helper_hpp
#define quantext_tenor_basis_swap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/tenorbasisswap.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Rate helper for bootstrapping using Libor tenor basis swaps
/*! \ingroup termstructures
 */
class TenorBasisSwapHelper : public RelativeDateRateHelper {
public:
    TenorBasisSwapHelper(Handle<Quote> spread, const Period& swapTenor, const QuantLib::ext::shared_ptr<IborIndex> payIndex,
                         const QuantLib::ext::shared_ptr<IborIndex> receiveIndex,
                         const Handle<YieldTermStructure>& discountingCurve = Handle<YieldTermStructure>(),
                         bool spreadOnRec = true, bool includeSpread = false, const Period& payFrequency = Period(),
                         const Period& recFrequency = Period(), const bool telescopicValueDates = false,
                         QuantExt::SubPeriodsCoupon1::Type type = QuantExt::SubPeriodsCoupon1::Compounding);

    //! \name RateHelper interface
    //@{
    Real impliedQuote() const override;
    void setTermStructure(YieldTermStructure*) override;
    //@}
    //! \name TenorBasisSwapHelper inspectors
    //@{
    QuantLib::ext::shared_ptr<TenorBasisSwap> swap() const { return swap_; }
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}

protected:
    void initializeDates() override;

    Period swapTenor_;
    QuantLib::ext::shared_ptr<IborIndex> payIndex_;
    QuantLib::ext::shared_ptr<IborIndex> receiveIndex_;
    bool spreadOnRec_;
    bool includeSpread_;
    Period payFrequency_;
    Period recFrequency_;
    bool telescopicValueDates_;
    QuantExt::SubPeriodsCoupon1::Type type_;
    bool automaticDiscountRelinkableHandle_;

    QuantLib::ext::shared_ptr<TenorBasisSwap> swap_;
    RelinkableHandle<YieldTermStructure> termStructureHandle_;
    Handle<YieldTermStructure> discountHandle_;
    RelinkableHandle<YieldTermStructure> discountRelinkableHandle_;
};
} // namespace QuantExt

#endif
