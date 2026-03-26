/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef qle_credit_default_swap_i
#define qle_credit_default_swap_i

%include instruments.i
%include credit.i
%include termstructures.i
%include bonds.i
%include null.i
%include creditdefaultswap.i

%{
static QuantLib::ext::shared_ptr<QuantLib::Exercise> qleCloneExercise(const QuantLib::Exercise& exercise) {
  switch (exercise.type()) {
  case QuantLib::Exercise::American: {
    const auto& dates = exercise.dates();
    if (dates.size() == 1) {
      return QuantLib::ext::make_shared<QuantLib::AmericanExercise>(dates.back());
    }
    return QuantLib::ext::make_shared<QuantLib::AmericanExercise>(dates.front(), dates.back());
  }
  case QuantLib::Exercise::Bermudan:
    return QuantLib::ext::make_shared<QuantLib::BermudanExercise>(exercise.dates());
  case QuantLib::Exercise::European:
    return QuantLib::ext::make_shared<QuantLib::EuropeanExercise>(exercise.lastDate());
  default:
    throw std::runtime_error("Unsupported exercise type in qleCloneExercise");
  }
}
%}

%rename(QLECdsOption) QuantExt::CdsOption;
%ignore QuantExt::CdsOption::underlyingSwap;
%shared_ptr(QuantExt::CdsOption)
namespace QuantExt {
class CdsOption : public Instrument {
  public:
    enum StrikeType { Price, Spread };
    CdsOption(const QuantLib::ext::shared_ptr<QuantLib::CreditDefaultSwap> swap,
	      const QuantLib::ext::shared_ptr<QuantLib::Exercise>& exercise,
	      bool knocksOut = true, const QuantLib::Real strike = Null<Real>(),
              const QuantExt::CdsOption::StrikeType strikeType = QuantExt::CdsOption::StrikeType::Spread);
    const QuantLib::ext::shared_ptr<QuantLib::CreditDefaultSwap> underlyingSwap() const;
    QuantLib::Rate atmRate() const;
    QuantLib::Real riskyAnnuity() const;
    QuantLib::Volatility impliedVolatility(QuantLib::Real price,
                                           const QuantLib::Handle<QuantLib::YieldTermStructure>& termStructureSwapCurrency,
                                           const QuantLib::Handle<QuantLib::YieldTermStructure>& termStructureTradeCollateral,
                                           const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& probability,
                                           QuantLib::Real recoveryRate,
                                           QuantLib::Real accuracy = 1.e-4,
                                           QuantLib::Size maxEvaluations = 100,
                                           QuantLib::Volatility minVol = 1.0e-7,
                                           QuantLib::Volatility maxVol = 4.0) const;
};
}

%inline %{
const QuantLib::CreditDefaultSwap& qleCdsOptionUnderlyingSwap(const QuantExt::CdsOption& option) {
  return *option.underlyingSwap();
}

QuantExt::CdsOption* qleMakeCdsOptionFromObjects(const QuantLib::CreditDefaultSwap& swap,
                         const QuantLib::Exercise& exercise) {
  return new QuantExt::CdsOption(QuantLib::ext::make_shared<QuantLib::CreditDefaultSwap>(swap),
                   qleCloneExercise(exercise));
}
%}

%pythoncode %{
def _qle_cds_option_init(self, *args):
  _ore_module = globals().get("_ORE", globals().get("_OREP"))
  if len(args) == 2:
    try:
      _ore_module.QLECdsOption_swiginit(self, _ore_module.qleMakeCdsOptionFromObjects(args[0], args[1]))
      return
    except TypeError:
      pass
  _ore_module.QLECdsOption_swiginit(self, _ore_module.new_QLECdsOption(*args))

def _qle_cds_option_underlying_swap(self):
  _ore_module = globals().get("_ORE", globals().get("_OREP"))
  return _ore_module.qleCdsOptionUnderlyingSwap(self)

QLECdsOption.__init__ = _qle_cds_option_init
QLECdsOption.underlyingSwap = _qle_cds_option_underlying_swap
%}

%rename(QLEBlackCdsOptionEngine) QuantExt::BlackCdsOptionEngine;
%shared_ptr(QuantExt::BlackCdsOptionEngine)
namespace QuantExt {
class BlackCdsOptionEngine : public PricingEngine {
  public:
    BlackCdsOptionEngine(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& probability,
                         QuantLib::Real recovery,
                         const QuantLib::Handle<QuantLib::YieldTermStructure>& discountSwapCurrency,
                         const QuantLib::Handle<QuantLib::YieldTermStructure>& discountTradeCollateral,
                         const QuantLib::Handle<QuantExt::CreditVolCurve>& volatility);
};
}

#endif
