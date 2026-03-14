/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file model/utilities.hpp
    \brief Shared utilities for model building and calibration
    \ingroup models
*/

#pragma once

#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/strike.hpp>

#include <qle/indexes/commodityindex.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fallbackiborindex.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/models/commodityschwartzparametrization.hpp>
#include <qle/models/eqbsparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/hwparametrization.hpp>
#include <qle/models/infdkparametrization.hpp>
#include <qle/models/infjyparameterization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/irmodelcalibrationinfo.hpp>

#include <ql/indexes/swapindex.hpp>
#include <ql/models/calibrationhelper.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>

#include <boost/variant.hpp>

#include <vector>

namespace ore {
namespace data {
using namespace QuantExt;
using namespace QuantLib;

template <typename Helper> Real getCalibrationError(const std::vector<QuantLib::ext::shared_ptr<Helper>>& basket) {
    Real rmse = 0.0;
    for (auto const& h : basket) {
        Real tmp = h->calibrationError();
        rmse += tmp * tmp;
    }
    return std::sqrt(rmse / static_cast<Real>(basket.size()));
}

std::string getCalibrationDetails(LgmCalibrationInfo& info,
                                  const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& parametrization =
                                      QuantLib::ext::shared_ptr<IrLgm1fParametrization>());

std::string getCalibrationDetails(HwCalibrationInfo& info,
                                  const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<IrHwParametrization>& parametrization =
                                      QuantLib::ext::shared_ptr<IrHwParametrization>());

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<FxBsParametrization>& parametrization =
                                      QuantLib::ext::shared_ptr<FxBsParametrization>(),
                                  const QuantLib::ext::shared_ptr<Parametrization>& domesticLgm =
                                      QuantLib::ext::shared_ptr<IrLgm1fParametrization>());

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<FxBsParametrization>& parametrization =
                                      QuantLib::ext::shared_ptr<FxBsParametrization>(),
                                  const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& domesticLgm =
                                      QuantLib::ext::shared_ptr<IrLgm1fParametrization>());

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<EqBsParametrization>& parametrization =
                                      QuantLib::ext::shared_ptr<EqBsParametrization>(),
                                  const QuantLib::ext::shared_ptr<Parametrization>& domesticLgm =
                                      QuantLib::ext::shared_ptr<IrLgm1fParametrization>());

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<EqBsParametrization>& parametrization =
                                      QuantLib::ext::shared_ptr<EqBsParametrization>(),
                                  const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& domesticLgm =
                                      QuantLib::ext::shared_ptr<IrLgm1fParametrization>());

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<InfDkParametrization>& parametrization =
                                      QuantLib::ext::shared_ptr<InfDkParametrization>(),
                                  bool indexIsInterpolated = true);

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<CommoditySchwartzParametrization>& parametrization =
                                      QuantLib::ext::shared_ptr<CommoditySchwartzParametrization>());

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& realRateBasket,
                                  const std::vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& indexBasket,
                                  const QuantLib::ext::shared_ptr<InfJyParameterization>& parameterization,
                                  bool calibrateRealRateVol = false);

std::string getCalibrationDetails(const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& parametrization);

//! Return an option's maturity date, given an explicit date or a period.
QuantLib::Date optionMaturity(const boost::variant<QuantLib::Date, QuantLib::Period>& maturity,
                              const QuantLib::Calendar& calendar,
                              const QuantLib::Date& referenceDate = Settings::instance().evaluationDate());

//! Return a cpi cap/floor strike value, the input strike can be of type absolute or atm forward
Real cpiCapFloorStrikeValue(const QuantLib::ext::shared_ptr<BaseStrike>& strike,
                            const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& inflationIndex,
                            const QuantLib::Handle<QuantLib::CPIVolatilitySurface>& volSurface,
                            const QuantLib::Date& optionMaturityDate, const bool dontCalibrate);

//! Return a yoy cap/floor strike value, the input strike can be of type absolute or atm forward
Real yoyCapFloorStrikeValue(const QuantLib::ext::shared_ptr<BaseStrike>& strike,
                            const QuantLib::ext::shared_ptr<YoYInflationTermStructure>& curve,
                            const QuantLib::Date& optionMaturityDate);

//! helper function that computes the atm forward
Real atmForward(const Real s0, const Handle<YieldTermStructure>& r, const Handle<YieldTermStructure>& q, const Real t);

/*! convert a IR / FX / EQ index name to a correlation label that is understood by the cam builder;
    return the tenor of the index too (or 0*Days if not applicable) */
std::pair<std::string, Period> convertIndexToCamCorrelationEntry(const std::string& i);

/*! - helper class that takes and index name string and provides the index type and a parsed version of
      the index with no market data attached
    - commodity indices can be of the extended form for scripting, see parseScriptedCommodityIndex for details
    - if a market is given, the class attempts to retrieve an eq index from the market, so that it has the
      correct business day calendar (market curve will be attached to the index too in this case)
*/
class IndexInfo {
public:
    // ctor taking the ORE name of an index and conventions
    explicit IndexInfo(const std::string& name, const QuantLib::ext::shared_ptr<Market>& market = nullptr);
    // the (ORE, i.e. Input) name of the index
    std::string name() const { return name_; }
    // type of index
    bool isFx() const { return isFx_; }
    bool isEq() const { return isEq_; }
    bool isComm() const { return isComm_; }
    bool isIr() const { return isIr_; }
    bool isIrIbor() const { return isIrIbor_; }
    bool isIrSwap() const { return isIrSwap_; }
    bool isInf() const { return isInf_; }
    bool isGeneric() const { return isGeneric_; }
    // get parsed version of index, or null if na
    QuantLib::ext::shared_ptr<FxIndex> fx() const { return fx_; }
    QuantLib::ext::shared_ptr<EquityIndex2> eq() const { return eq_; }
    QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>
    // requires obsDate + conventions for forms 3-6 below, throws otherwise
    comm(const Date& obsDate = Date()) const;
    QuantLib::ext::shared_ptr<InterestRateIndex> ir() const { return ir_; }
    QuantLib::ext::shared_ptr<IborIndex> irIbor() const { return irIbor_; }
    // nullptr if it is no ibor fallback index
    QuantLib::ext::shared_ptr<FallbackIborIndex>
    irIborFallback(const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig,
                   const Date& asof = QuantLib::Date::maxDate()) const;
    // nullptr if it is no overnight fallback index
    QuantLib::ext::shared_ptr<FallbackOvernightIndex>
    irOvernightFallback(const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig,
                        const Date& asof = QuantLib::Date::maxDate()) const;
    QuantLib::ext::shared_ptr<SwapIndex> irSwap() const { return irSwap_; }
    QuantLib::ext::shared_ptr<ZeroInflationIndex> inf() const { return inf_; }
    QuantLib::ext::shared_ptr<Index> generic() const { return generic_; }
    // get ptr to base class (comm forms 3-6 below require obsDate + conventions, will throw otherwise)
    QuantLib::ext::shared_ptr<Index> index(const Date& obsDate = Date()) const;
    // get comm underlying name NYMEX:CL (no COMM- prefix, no suffixes)
    std::string commName() const;
    // get inf name INF-EUHICP (i.e. without the #L, #F suffix)
    std::string infName() const;
    // get if inf fixing should be interpolated
    bool infIsInterpolated() const { return infIsInterpolated_; }
    // comparisons (based on the name)
    bool operator==(const IndexInfo& j) const { return name() == j.name(); }
    bool operator!=(const IndexInfo& j) const { return !(*this == j); }
    bool operator<(const IndexInfo& j) const { return name() < j.name(); }
    bool operator>(const IndexInfo& j) const { return j < (*this); }
    bool operator<=(const IndexInfo& j) const { return !((*this) > j); }
    bool operator>=(const IndexInfo& j) const { return !((*this) < j); }

private:
    std::string name_;
    QuantLib::ext::shared_ptr<Market> market_; // might be null
    bool isFx_, isEq_, isComm_, isIr_, isInf_, isIrIbor_, isIrSwap_, isGeneric_;
    QuantLib::ext::shared_ptr<FxIndex> fx_;
    QuantLib::ext::shared_ptr<EquityIndex2> eq_;
    QuantLib::ext::shared_ptr<InterestRateIndex> ir_;
    QuantLib::ext::shared_ptr<IborIndex> irIbor_;
    QuantLib::ext::shared_ptr<SwapIndex> irSwap_;
    QuantLib::ext::shared_ptr<ZeroInflationIndex> inf_;
    QuantLib::ext::shared_ptr<Index> generic_;
    std::string commName_, infName_;
    // Inflation indices are never interpolated but coupons can use an interpolated fixing
    // for convencience we define indexes which are will return interpolated fixings instead of flat fixings.
    bool infIsInterpolated_;
};

std::ostream& operator<<(std::ostream& o, const IndexInfo& i);

/*! This method tries to parse an commodity index name used in the scripting context

    0) COMM-name
    1) COMM-name-YYYY-MM-DD
    2) COMM-name-YYYY-MM
    ===
    3) CMMM-name\#N\#D\#Cal
    4) COMM-name\#N\#D
    5) COMM-name\#N
    ===
    6) COMM-name!N

    Here 0) - 2) are corresponding to the usual ORE conventions while 3) - 6) are specific to the scripting
    module: Expressions of the form 3) - 5) are resolved to one of the forms 1) and 2) using a given commodity
    future expiry calculator as follows:

    3) COMM-name\#N\#D\#Cal is resolved to the (N+1)th future with expiry greater than the given obsDate advanced by D
                         business days w.r.t. Calendar Cal, N >= 0
    4) as 3), Cal is taken as the commodity index's fixing calendar
    5) as 4), D is set to 0 if not given
    6) COMM-name!N is resolved to the future with month / year equal to the obsDate and monthOffst = N, N >=0

    Notice that the forms 1) and 2) can be parsed without an obsDate and a commodity future convention given. If no
    convention is given, the fixing calendar in the index is set to the NullCalendar. In case a commodity future
    convention is given for the name, the fixing calendar is set to the calendar from the convention.

    TODO if the form is COMM-name-YYYY-MM, the day of month of the expiry date will be set to 01, consistently
    with the ORE index parser, even if a convention is present, that would allow us to determine the correct
    expiry date. Should we use that latter date in the returned index?

    Forms 3) to 6) on the other hand require a commodity future convention in any case, and an obsDate.
*/

QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> parseScriptedCommodityIndex(const std::string& indexName,
                                                                                const QuantLib::Date& obsDate = Date());

/*! This method tries to parse an inflation index name used in the scripting context

  1) EUHICPXT
  2) EUHICPXT\#F
  3) EUHICPXT\#L

  Here 1)   is the original form used in ORE. This represents a non-interpolated index.
       2,3) is the extended form including a flag indicating the interpolation F (flat, =1) or L (linear)

  The function returns a ql inflation index accounting for the interpolation (but without ts attached),
  and the ORE index name without the \#F, \#L suffix.
*/
std::tuple<QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>, std::string, bool>
parseScriptedInflationIndex(const std::string& indexName);

} // namespace data
} // namespace ore
