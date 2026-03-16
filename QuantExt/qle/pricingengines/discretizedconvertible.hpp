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

/*! \file qle/pricingengines/discretizedconvertible.hpp
    \brief discretized convertible
*/

#pragma once

#include <qle/instruments/convertiblebond.hpp>

#include <ql/discretizedasset.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantExt {

class DiscretizedConvertible : public DiscretizedAsset {
public:
    DiscretizedConvertible(const ConvertibleBond::option::arguments& args,
                           const ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                           const Handle<Quote>& creditSpread, const TimeGrid& grid);

    void reset(Size size) override;

    const Array& conversionProbability() const { return conversionProbability_; }
    Array& conversionProbability() { return conversionProbability_; }

    const Array& spreadAdjustedRate() const { return spreadAdjustedRate_; }
    Array& spreadAdjustedRate() { return spreadAdjustedRate_; }

    const Array& dividendValues() const { return dividendValues_; }
    Array& dividendValues() { return dividendValues_; }

    std::vector<Time> mandatoryTimes() const override;

protected:
    void postAdjustValuesImpl() override;
    Array conversionProbability_, spreadAdjustedRate_, dividendValues_;

private:
    Array adjustedGrid() const;
    void applyConvertibility();
    void applyCallability(Size, bool convertible);
    void addCashflow(Size);
    ConvertibleBond::option::arguments arguments_;
    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
    Handle<Quote> creditSpread_;
    std::vector<Time> stoppingTimes_;
    std::vector<Time> callabilityTimes_;
    std::vector<Time> cashflowTimes_;
    std::vector<Time> dividendTimes_;

    // add PC
    Real getConversionRatio(const Real t) const;
    std::vector<Time> notionalTimes_;
};

} // namespace QuantExt
