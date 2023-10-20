/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/oibasisswaphelper.hpp
    \brief Overnight Indexed vs Overnight Indexed Basis Swap rate helpers
    \ingroup termstructures
*/

#ifndef quantlib_doubleoisbasisswaphelper_hpp
#define quantlib_doubleoisbasisswaphelper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>
#include <qle/instruments/doubleoibasisswap.hpp>

namespace QuantExt {

//! Rate helper for bootstrapping over Overnight Indexed Basis Swap Spreads
/*! \ingroup termstructures
 */
class DoubleOIBSHelper : public RelativeDateRateHelper {
public:
    DoubleOIBSHelper(Natural settlementDays,
               const Period& swapTenor, // swap maturity
               const Handle<Quote>& spread, const boost::shared_ptr<OvernightIndex>& payIndex,
               const boost::shared_ptr<OvernightIndex>& recIndex,
               const Handle<YieldTermStructure>& discount = Handle<YieldTermStructure>(),
               const bool spreadOnPayLeg = false, const Period& shortPayTenor = Period(),
               const Period& longPayTenor = Period(), const bool telescopicValueDates = false);
    //! \name RateHelper interface
    //@{
    Real impliedQuote() const override;
    void setTermStructure(YieldTermStructure*) override;
    //@}
    //! \name inspectors
    //@{
    boost::shared_ptr<DoubleOvernightIndexedBasisSwap> swap() const { return swap_; }
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}
protected:
    void initializeDates() override;

    Natural settlementDays_;
    Period swapTenor_;
    boost::shared_ptr<OvernightIndex> payIndex_;
    boost::shared_ptr<OvernightIndex> recIndex_;
    Handle<YieldTermStructure> discount_;
    bool spreadOnPayLeg_;
    Period shortPayTenor_;
    Period longPayTenor_;
    bool telescopicValueDates_;

    boost::shared_ptr<DoubleOvernightIndexedBasisSwap> swap_;
    RelinkableHandle<YieldTermStructure> termStructureHandle_;
    RelinkableHandle<YieldTermStructure> discountRelinkableHandle_;
};
} // namespace QuantExt

#endif
