/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file qle/models/crossassetmodelimpliedswaptionvoltermstructure.hpp
    \brief dynamic Swpation volatility term structure
    \ingroup crossassetmodel
*/

#ifndef quantext_crossassetmodel_implied_swaption_volatility_termstructure_hpp
#define quantext_crossassetmodel_implied_swaption_volatility_termstructure_hpp

#include <qle/models/crossassetmodel.hpp>

#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/instruments/vanillaswap.hpp>

namespace QuantExt {
using namespace QuantLib;

class AnalyticLgmSwaptionEngine;

//! Cross Asset Model Implied Swaption Vol Term Structure
/*!
  \ingroup crossassetmodel
*/

class CrossAssetModelImpliedSwaptionVolTermStructure : public SwaptionVolatilityStructure {
public:
    CrossAssetModelImpliedSwaptionVolTermStructure(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
						   const ext::shared_ptr<YieldTermStructure>& impliedDiscountCurve,
						   const std::vector<QuantLib::ext::shared_ptr<IborIndex>>& impliedIborIndices,
                                                   const ext::shared_ptr<SwapIndex>& swapIndex,
                                                   const ext::shared_ptr<SwapIndex>& shortSwapIndex,
						   BusinessDayConvention bdc = Following,
						   const DayCounter& dc = DayCounter(),
                                                   const bool purelyTimeBased = false);

    void referenceDate(const Date& d);
    void referenceTime(const Time t);
    void state(const Real z);
    // pass model implied discount and index curves here
    void move(const Date& d, Real z);
    void move(const Time t, Real z);

    /* SwaptionVolatilityStructure interface */
    const Period& maxSwapTenor() const override;
    ext::shared_ptr<SmileSection> smileSectionImpl(Time optionTime, Time swapLength) const override;
    Volatility volatilityImpl(Time optionTime, Time swapLength, Rate strike) const override;

    // VolatilityTermStructure interface
    Real minStrike() const override;
    Real maxStrike() const override;
    // TermStructure interface
    Date maxDate() const override;
    Time maxTime() const override;
    const Date& referenceDate() const override;
    /* Observer interface */
    void update() override;

    Size ccyIndex() const { return ccyIndex_; }
private:
    const QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    const Size ccyIndex_; // position in the scenario's and model's vector of currencies
    ext::shared_ptr<YieldTermStructure> impliedDiscountCurve_; 
    std::vector<ext::shared_ptr<IborIndex>> impliedIborIndices_; 
    ext::shared_ptr<SwapIndex> swapIndex_;
    ext::shared_ptr<SwapIndex> shortSwapIndex_;
    const bool purelyTimeBased_;
    const QuantLib::ext::shared_ptr<AnalyticLgmSwaptionEngine> engine_;
    Date referenceDate_;
    Real relativeTime_;
    Real state_;
    Period maxSwapTenor_ = Period();
};

} // namespace QuantExt

#endif
