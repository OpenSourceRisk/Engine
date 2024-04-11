/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/cashsettledeuropeanoption.hpp
    \brief cash settled european vanilla option.
    \ingroup instruments
*/

#ifndef quantext_cash_settled_european_option_hpp
#define quantext_cash_settled_european_option_hpp

#include <ql/index.hpp>
#include <ql/instruments/vanillaoption.hpp>

namespace QuantExt {

/*! Vanilla cash settled European options allowing for deferred payment and automatic exercise.

    \ingroup instruments
*/
class CashSettledEuropeanOption : public QuantLib::VanillaOption {
public:
    class arguments;
    class engine;

    //! Constructor for cash settled vanilla European option.
    CashSettledEuropeanOption(QuantLib::Option::Type type, QuantLib::Real strike, const QuantLib::Date& expiryDate,
                              const QuantLib::Date& paymentDate, bool automaticExercise,
                              const QuantLib::ext::shared_ptr<QuantLib::Index>& underlying = nullptr, bool exercised = false,
                              QuantLib::Real priceAtExercise = QuantLib::Null<QuantLib::Real>());

    //! Constructor for cash settled vanilla European option.
    CashSettledEuropeanOption(QuantLib::Option::Type type, QuantLib::Real strike, const QuantLib::Date& expiryDate,
                              QuantLib::Natural paymentLag, const QuantLib::Calendar& paymentCalendar,
                              QuantLib::BusinessDayConvention paymentConvention, bool automaticExercise,
                              const QuantLib::ext::shared_ptr<QuantLib::Index>& underlying = nullptr, bool exercised = false,
                              QuantLib::Real priceAtExercise = QuantLib::Null<QuantLib::Real>());

    //! Constructor for cash settled vanilla European option with digital payoff.
    CashSettledEuropeanOption(QuantLib::Option::Type type, QuantLib::Real strike, QuantLib::Real cashPayoff,
                              const QuantLib::Date& expiryDate, const QuantLib::Date& paymentDate,
                              bool automaticExercise, const QuantLib::ext::shared_ptr<QuantLib::Index>& underlying = nullptr,
                              bool exercised = false,
                              QuantLib::Real priceAtExercise = QuantLib::Null<QuantLib::Real>());

    //! Constructor for cash settled vanilla European option with digital payoff.
    CashSettledEuropeanOption(QuantLib::Option::Type type, QuantLib::Real strike, QuantLib::Real cashPayoff,
                              const QuantLib::Date& expiryDate, QuantLib::Natural paymentLag,
                              const QuantLib::Calendar& paymentCalendar,
                              QuantLib::BusinessDayConvention paymentConvention, bool automaticExercise,
                              const QuantLib::ext::shared_ptr<QuantLib::Index>& underlying = nullptr, bool exercised = false,
                              QuantLib::Real priceAtExercise = QuantLib::Null<QuantLib::Real>());

    //! \name Instrument interface
    //@{
    //! Account for cash settled European options not being expired until payment is made.
    bool isExpired() const override;

    //! Set up the extra arguments.
    void setupArguments(QuantLib::PricingEngine::arguments* args) const override;
    //@}

    //! Mark option as manually exercised at the given \p priceAtExercise.
    void exercise(QuantLib::Real priceAtExercise);

    //! \name Inspectors
    //@{
    const QuantLib::Date& paymentDate() const;
    bool automaticExercise() const;
    const QuantLib::ext::shared_ptr<QuantLib::Index>& underlying() const;
    bool exercised() const;
    QuantLib::Real priceAtExercise() const;
    //@}

private:
    QuantLib::Date paymentDate_;
    bool automaticExercise_;
    QuantLib::ext::shared_ptr<QuantLib::Index> underlying_;
    bool exercised_;
    QuantLib::Real priceAtExercise_;

    //! Shared initialisation
    void init(bool exercised, QuantLib::Real priceAtExercise);
};

class CashSettledEuropeanOption::arguments : public QuantLib::VanillaOption::arguments {
public:
    arguments() {}
    void validate() const override;

    QuantLib::Date paymentDate;
    bool automaticExercise;
    QuantLib::ext::shared_ptr<QuantLib::Index> underlying;
    bool exercised;
    QuantLib::Real priceAtExercise;
};

//! Engine
class CashSettledEuropeanOption::engine
    : public QuantLib::GenericEngine<CashSettledEuropeanOption::arguments, CashSettledEuropeanOption::results> {};

} // namespace QuantExt

#endif
