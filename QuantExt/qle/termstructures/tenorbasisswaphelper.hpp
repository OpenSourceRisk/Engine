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

/*! \file tenorbasisswap.hpp
    \brief Single currency tenor basis swap helper
    \ingroup termstructures
*/

#ifndef quantext_tenor_basis_swap_helper_hpp
#define quantext_tenor_basis_swap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/tenorbasisswap.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Rate helper for bootstrapping using Libor tenor basis swaps
/*! \ingroup termstructures
 */
class TenorBasisSwapHelper : public RelativeDateRateHelper {
public:
    TenorBasisSwapHelper(Handle<Quote> spread, const Period& swapTenor, const boost::shared_ptr<IborIndex> longIndex,
                         const boost::shared_ptr<IborIndex> shortIndex, const Period& shortPayTenor = Period(),
                         const Handle<YieldTermStructure>& discountingCurve = Handle<YieldTermStructure>(),
                         bool spreadOnShort = true, bool includeSpread = false,
                         SubPeriodsCoupon::Type type = SubPeriodsCoupon::Compounding);

    //! \name RateHelper interface
    //@{
    Real impliedQuote() const;
    void setTermStructure(YieldTermStructure*);
    //@}
    //! \name TenorBasisSwapHelper inspectors
    //@{
    boost::shared_ptr<TenorBasisSwap> swap() const { return swap_; }
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&);
    //@}

protected:
    void initializeDates();

    Period swapTenor_;
    boost::shared_ptr<IborIndex> longIndex_;
    boost::shared_ptr<IborIndex> shortIndex_;
    Period shortPayTenor_;
    bool spreadOnShort_;
    bool includeSpread_;
    SubPeriodsCoupon::Type type_;

    boost::shared_ptr<TenorBasisSwap> swap_;
    RelinkableHandle<YieldTermStructure> termStructureHandle_;
    Handle<YieldTermStructure> discountHandle_;
    RelinkableHandle<YieldTermStructure> discountRelinkableHandle_;
};
} // namespace QuantExt

#endif
