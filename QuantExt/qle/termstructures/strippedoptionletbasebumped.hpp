/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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

/**
 * \file qle/termstructures/strippedoptionletbasebumped.hpp
 * \brief take an existing `StrippedOptionletBase` and allow it to be bumped
 * \ingroup termstructures
 */

#pragma once
#include <ql/handle.hpp>
#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/math/matrix.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletbase.hpp>
#include <vector>

namespace QuantExt {

class StrippedOptionletBaseBumped : public QuantLib::StrippedOptionletBase {
public:
    using QuoteCurve = std::vector<QuantLib::Handle<QuantLib::Quote>>;
    using QuoteRow = QuoteCurve;
    using QuoteGrid = std::vector<QuoteRow>;

    StrippedOptionletBaseBumped(
        QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase> sob,
        QuoteCurve bumpQuotes,
        std::vector<QuantLib::Time> bumpTimes);
    StrippedOptionletBaseBumped(
        QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase> sob,
        QuoteGrid bumpQuotes,
        std::vector<QuantLib::Time> bumpTimes,
        std::vector<QuantLib::Rate> bumpStrikes);

    //! \name StrippedOptionletBase interface
    //@{
    const std::vector<QuantLib::Rate>& optionletStrikes(QuantLib::Size i) const override;
    const std::vector<QuantLib::Volatility>& optionletVolatilities(QuantLib::Size i) const override;
    const std::vector<QuantLib::Date>& optionletFixingDates() const override;
    const std::vector<QuantLib::Time>& optionletFixingTimes() const override;
    QuantLib::Size optionletMaturities() const override;
    const std::vector<QuantLib::Rate>& atmOptionletRates() const override;
    QuantLib::DayCounter dayCounter() const override;
    QuantLib::Calendar calendar() const override;
    QuantLib::Natural settlementDays() const override;
    QuantLib::BusinessDayConvention businessDayConvention() const override;
    //@}

    QuantLib::VolatilityType volatilityType() const override;
    QuantLib::Real displacement() const override;
    bool useEffectiveVolatility() const override;

protected:
    void performCalculations() const override;

private:
    QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase> sob_;
    QuoteGrid bumpQuotes_;
    std::vector<QuantLib::Time> bumpTimes_;
    std::vector<QuantLib::Rate> bumpStrikes_;

    // Store the volatilities to be returned.
    mutable std::vector<std::vector<QuantLib::Volatility>> volatilities_;

    // Store the bumps to be applied to the underlying volatilities.
    mutable QuantLib::Matrix bumpMatrix_;
    QuantLib::Interpolation2D bumpInterp_;

    // Helper to convert the a QuoteCurve to a QuoteGrid.
    static QuoteGrid makeGrid(QuoteCurve quoteCurve);

    // Initialise and perform checks.
    void init();
};

} // namespace QuantExt
