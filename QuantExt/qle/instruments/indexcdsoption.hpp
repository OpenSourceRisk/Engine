/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/indexcdsoption.hpp
    \brief Index CDS option instrument
*/

#ifndef quantext_index_cds_option_hpp
#define quantext_index_cds_option_hpp

#include <qle/instruments/indexcreditdefaultswap.hpp>
#include <qle/instruments/cdsoption.hpp>

#include <ql/instruments/swaption.hpp>
#include <ql/option.hpp>

namespace QuantExt {

//! Index CDS option instrument
class IndexCdsOption : public QuantLib::Option {
public:
    class arguments;
    class results;
    class engine;

    IndexCdsOption(const QuantLib::ext::shared_ptr<IndexCreditDefaultSwap>& swap,
                   const QuantLib::ext::shared_ptr<QuantLib::Exercise>& exercise, QuantLib::Real strike,
                   CdsOption::StrikeType strikeType_ = CdsOption::Spread,
                   Settlement::Type settlementType = Settlement::Cash,
                   QuantLib::Real tradeDateNtl = QuantLib::Null<QuantLib::Real>(),
                   QuantLib::Real realisedFep = QuantLib::Null<QuantLib::Real>(),
                   const QuantLib::Period& indexTerm = 5 * Years);

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(QuantLib::PricingEngine::arguments* args) const override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<IndexCreditDefaultSwap>& underlyingSwap() const { return swap_; }
    //@}

    //! \name Calculations
    //@{
    QuantLib::Rate atmRate() const;
    QuantLib::Real riskyAnnuity() const;
    QuantLib::Volatility
    impliedVolatility(QuantLib::Real price,
                      const QuantLib::Handle<QuantLib::YieldTermStructure>& termStructureSwapCurrency,
                      const QuantLib::Handle<QuantLib::YieldTermStructure>& termStructureTradeCollateral,
                      const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>&, QuantLib::Real recoveryRate,
                      QuantLib::Real accuracy = 1.e-4, QuantLib::Size maxEvaluations = 100,
                      QuantLib::Volatility minVol = 1.0e-7, QuantLib::Volatility maxVol = 4.0) const;
    //@}

private:
    QuantLib::ext::shared_ptr<IndexCreditDefaultSwap> swap_;
    QuantLib::Real strike_;
    CdsOption::StrikeType strikeType_;
    Settlement::Type settlementType_;
    QuantLib::Real tradeDateNtl_;
    QuantLib::Real realisedFep_;
    QuantLib::Period indexTerm_;

    mutable QuantLib::Real riskyAnnuity_;

    //! \name Instrument interface
    //@{
    void setupExpired() const override;
    void fetchResults(const QuantLib::PricingEngine::results* results) const override;
    //@}
};

//! %Arguments for index CDS option calculation
class IndexCdsOption::arguments : public IndexCreditDefaultSwap::arguments, public Option::arguments {
public:
    arguments()
        : strike(QuantLib::Null<QuantLib::Real>()), strikeType(CdsOption::Spread), settlementType(Settlement::Cash),
          tradeDateNtl(QuantLib::Null<QuantLib::Real>()), realisedFep(QuantLib::Null<QuantLib::Real>()) {}

    QuantLib::ext::shared_ptr<IndexCreditDefaultSwap> swap;
    QuantLib::Real strike;
    CdsOption::StrikeType strikeType;
    Settlement::Type settlementType;
    QuantLib::Real tradeDateNtl;
    QuantLib::Real realisedFep;
    QuantLib::Period indexTerm;

    void validate() const override;
};

//! %Results from index CDS option calculation
class IndexCdsOption::results : public CdsOption::results {
public:
    QuantLib::Real riskyAnnuity;

    void reset() override;
};

//! Base class for index CDS option engines
class IndexCdsOption::engine : public QuantLib::GenericEngine<IndexCdsOption::arguments, IndexCdsOption::results> {};

}

#endif
