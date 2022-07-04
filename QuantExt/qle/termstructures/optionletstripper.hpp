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

/*! \file qle/termstructures/optionletstripper.hpp
    \brief optionlet (caplet/floorlet) volatility stripper
*/

#ifndef quantext_optionletstripper_hpp
#define quantext_optionletstripper_hpp

#include <ql/termstructures/volatility/optionlet/strippedoptionletbase.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/termstructures/capfloortermvolsurface.hpp>

namespace QuantLib {
class IborIndex;
}

namespace QuantExt {
using namespace QuantLib;

/*! Copy of the QL class that uses an QuantExt::CapFloorTermVolSurface
    to support BiLinearInterpolation
*/
class OptionletStripper : public StrippedOptionletBase {
public:
    //! \name StrippedOptionletBase interface
    //@{
    const std::vector<Rate>& optionletStrikes(Size i) const override;
    const std::vector<Volatility>& optionletVolatilities(Size i) const override;

    const std::vector<Date>& optionletFixingDates() const override;
    const std::vector<Time>& optionletFixingTimes() const override;
    Size optionletMaturities() const override;

    const std::vector<Rate>& atmOptionletRates() const override;

    DayCounter dayCounter() const override;
    Calendar calendar() const override;
    Natural settlementDays() const override;
    BusinessDayConvention businessDayConvention() const override;
    //@}

    const std::vector<Period>& optionletFixingTenors() const;
    const std::vector<Date>& optionletPaymentDates() const;
    const std::vector<Time>& optionletAccrualPeriods() const;
    ext::shared_ptr<CapFloorTermVolSurface> termVolSurface() const;
    ext::shared_ptr<IborIndex> index() const;
    Real displacement() const override;
    VolatilityType volatilityType() const override;

    const Period& rateComputationPeriod() const;

protected:
    // if index is OIS, rateComputationPeriod must be provided, for Ibor it is derived from the index tenor
    OptionletStripper(const ext::shared_ptr<QuantExt::CapFloorTermVolSurface>&, const ext::shared_ptr<IborIndex>& index,
                      const Handle<YieldTermStructure>& discount = Handle<YieldTermStructure>(),
                      const VolatilityType type = ShiftedLognormal, const Real displacement = 0.0,
                      const Period& rateComputationPeriod = 0 * Days, const Size onCapSettlementDays = 0);

    //! Method to populate the dates, times and accruals that can be overridden in derived classes
    virtual void populateDates() const;

    ext::shared_ptr<CapFloorTermVolSurface> termVolSurface_;
    ext::shared_ptr<IborIndex> index_;
    Handle<YieldTermStructure> discount_;
    Size nStrikes_;
    Size nOptionletTenors_;

    mutable std::vector<std::vector<Rate> > optionletStrikes_;
    mutable std::vector<std::vector<Volatility>> optionletVolatilities_;

    mutable std::vector<Time> optionletTimes_;
    mutable std::vector<Date> optionletDates_;
    std::vector<Period> optionletTenors_;
    mutable std::vector<Rate> atmOptionletRate_;
    mutable std::vector<Date> optionletPaymentDates_;
    mutable std::vector<Time> optionletAccrualPeriods_;

    std::vector<Period> capFloorLengths_;
    const VolatilityType volatilityType_;
    const Real displacement_;
    const Period rateComputationPeriod_;
    const Size onCapSettlementDays_;
};

} // namespace QuantExt

#endif
