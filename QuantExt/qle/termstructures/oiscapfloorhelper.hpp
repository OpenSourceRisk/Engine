/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/oiscapfloorhelper.hpp
    \brief Helper for bootstrapping optionlet volatilties from ois cap floor volatilities
    \ingroup termstructures
*/

#pragma once

#include <qle/termstructures/capfloorhelper.hpp>

#include <ql/termstructures/bootstraphelper.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {

class OISCapFloorHelper : public QuantLib::RelativeDateBootstrapHelper<QuantLib::OptionletVolatilityStructure> {

public:
    //! OISCapFloorHelper, similar to CapFloorHelper
    OISCapFloorHelper(CapFloorHelper::Type type, const QuantLib::Period& tenor,
                      const QuantLib::Period& rateComputationPeriod, QuantLib::Rate strike,
                      const QuantLib::Handle<QuantLib::Quote>& quote,
                      const QuantLib::ext::shared_ptr<QuantLib::OvernightIndex>& index,
                      const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve, bool moving = true,
                      const QuantLib::Date& effectiveDate = QuantLib::Date(),
                      CapFloorHelper::QuoteType quoteType = CapFloorHelper::Premium,
                      QuantLib::VolatilityType quoteVolatilityType = QuantLib::Normal,
                      QuantLib::Real quoteDisplacement = 0.0);

    Leg capFloor() const { return capFloor_; }

    void setTermStructure(QuantLib::OptionletVolatilityStructure* ovts) override;
    QuantLib::Real impliedQuote() const override;
    void accept(QuantLib::AcyclicVisitor&) override;

private:
    void initializeDates() override;

    CapFloorHelper::Type type_;
    QuantLib::Period tenor_;
    QuantLib::Period rateComputationPeriod_;
    QuantLib::Rate strike_;
    QuantLib::ext::shared_ptr<QuantLib::OvernightIndex> index_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountHandle_;
    bool moving_;
    QuantLib::Date effectiveDate_;
    CapFloorHelper::QuoteType quoteType_;
    QuantLib::VolatilityType quoteVolatilityType_;
    QuantLib::Real quoteDisplacement_;
    QuantLib::Handle<QuantLib::Quote> rawQuote_;
    bool initialised_;

    Leg capFloor_;
    QuantLib::RelinkableHandle<QuantLib::OptionletVolatilityStructure> ovtsHandle_;
    Leg capFloorCopy_;

    //! A method to calculate the cap floor premium from a flat cap floor volatility value
    QuantLib::Real npv(QuantLib::Real quote);
};

} // namespace QuantExt
