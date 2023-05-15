/*
 Copyright (C) 2005, 2006 Theo Boafo
 Copyright (C) 2006, 2007 StatPro Italia srl
 Copyright (C) 2020 Quaternion Risk Managment Ltd

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

/*! \file qle/pricingengines/tflattice.hpp
    \brief Binomial Tsiveriotis-Fernandes tree model
*/

#pragma once

#include <qle/pricingengines/discretizedconvertible.hpp>

#include <ql/methods/lattices/bsmlattice.hpp>

namespace QuantExt {

//! Binomial lattice approximating the Tsiveriotis-Fernandes model
/*! \ingroup lattices */
template <class T> class TsiveriotisFernandesLattice : public BlackScholesLattice<T> {
public:
    TsiveriotisFernandesLattice(const ext::shared_ptr<T>& tree, Rate riskFreeRate, Time end, Size steps,
                                Spread creditSpread, Volatility volatility, Spread divYield);

    Spread creditSpread() const { return creditSpread_; };

protected:
    void stepback(Size i, const Array& values, const Array& conversionProbability, const Array& spreadAdjustedRate,
                  Array& newValues, Array& newConversionProbability, Array& newSpreadAdjustedRate) const;
    void rollback(DiscretizedAsset&, Time to) const override;
    void partialRollback(DiscretizedAsset&, Time to) const override;

private:
    Spread creditSpread_;
};

// template definitions

template <class T>
TsiveriotisFernandesLattice<T>::TsiveriotisFernandesLattice(const ext::shared_ptr<T>& tree, Rate riskFreeRate, Time end,
                                                            Size steps, Spread creditSpread, Volatility sigma,
                                                            Spread divYield)
    : BlackScholesLattice<T>(tree, riskFreeRate, end, steps), creditSpread_(creditSpread) {
    QL_REQUIRE(this->pu_ <= 1.0, "probability (" << this->pu_ << ") higher than one");
    QL_REQUIRE(this->pu_ >= 0.0, "negative (" << this->pu_ << ") probability");
}

template <class T>
void TsiveriotisFernandesLattice<T>::stepback(Size i, const Array& values, const Array& conversionProbability,
                                              const Array& spreadAdjustedRate, Array& newValues,
                                              Array& newConversionProbability, Array& newSpreadAdjustedRate) const {

    for (Size j = 0; j < this->size(i); j++) {

        // new conversion probability is calculated via backward
        // induction using up and down probabilities on tree on
        // previous conversion probabilities, ie weighted average
        // of previous probabilities.
        newConversionProbability[j] = this->pd_ * conversionProbability[j] + this->pu_ * conversionProbability[j + 1];

        // Use blended discounting rate
        newSpreadAdjustedRate[j] = newConversionProbability[j] * this->riskFreeRate_ +
                                   (1 - newConversionProbability[j]) * (this->riskFreeRate_ + creditSpread_);

        newValues[j] = (this->pd_ * values[j] / (1 + (spreadAdjustedRate[j] * this->dt_))) +
                       (this->pu_ * values[j + 1] / (1 + (spreadAdjustedRate[j + 1] * this->dt_)));
    }
}

template <class T> void TsiveriotisFernandesLattice<T>::rollback(DiscretizedAsset& asset, Time to) const {
    partialRollback(asset, to);
    asset.adjustValues();
}

template <class T> void TsiveriotisFernandesLattice<T>::partialRollback(DiscretizedAsset& asset, Time to) const {

    Time from = asset.time();

    if (close(from, to))
        return;

    QL_REQUIRE(from > to, "cannot roll the asset back to" << to << " (it is already at t = " << from << ")");

    QuantExt::DiscretizedConvertible& convertible = dynamic_cast<QuantExt::DiscretizedConvertible&>(asset);

    Integer iFrom = Integer(this->t_.index(from));
    Integer iTo = Integer(this->t_.index(to));

    for (Integer i = iFrom - 1; i >= iTo; --i) {

        Array newValues(this->size(i));
        Array newSpreadAdjustedRate(this->size(i));
        Array newConversionProbability(this->size(i));

        stepback(i, convertible.values(), convertible.conversionProbability(), convertible.spreadAdjustedRate(),
                 newValues, newConversionProbability, newSpreadAdjustedRate);

        convertible.time() = this->t_[i];
        convertible.values() = newValues;
        convertible.spreadAdjustedRate() = newSpreadAdjustedRate;
        convertible.conversionProbability() = newConversionProbability;

        // skip the very last adjustment
        if (i != iTo)
            convertible.adjustValues();
    }
}

} // namespace QuantExt
