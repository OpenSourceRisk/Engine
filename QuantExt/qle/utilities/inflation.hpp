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

/*! \file qle/utilities/inflation.hpp
    \brief some inflation related utilities.
*/

#ifndef quantext_inflation_hpp
#define quantext_inflation_hpp

#include <ql/indexes/inflationindex.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/time/period.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/zeroinflationmodeltermstructure.hpp>

namespace QuantExt {

/*! Utility function for calculating the time to a given \p date based on a given inflation index,
    \p inflationIndex, and a given inflation term structure, \p inflationTs. An optional \p dayCounter can be 
    provided to use instead of the inflation term structure day counter.

    \ingroup utilities
*/
QuantLib::Time inflationTime(const QuantLib::Date& date,
    const QuantLib::ext::shared_ptr<QuantLib::InflationTermStructure>& inflationTs,
    bool indexIsInterpolated,
    const QuantLib::DayCounter& dayCounter = QuantLib::DayCounter());

/*! Utility for calculating the ratio \f$ \frac{P_r(0, t)}{P_n(0, t)} \f$ where \f$ P_r(0, t) \f$ is the real zero
    coupon bond price at time zero for maturity \f$ t \f$ and \f$ P_n(0, t) \f$ is the nominal zero coupon bond price.
*/
QuantLib::Real inflationGrowth(const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts, QuantLib::Time t,
                               const std::optional<QuantLib::DayCounter>& dc, bool indexIsInterpolated);

/*! Utility for calculating the ratio \f$ \frac{P_r(0, t)}{P_n(0, t)} \f$ where \f$ P_r(0, t) \f$ is the real zero
    coupon bond price at time zero for maturity \f$ t \f$ and \f$ P_n(0, t) \f$ is the nominal zero coupon bond price.
*/
QuantLib::Real inflationGrowth(const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts,
    QuantLib::Time t, bool indexIsInterpolated);

int simulationLag(const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts);

int simulationLag(const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationTermStructure>& ts);

double simulationLagTime(const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts,
                         const std::optional<QuantLib::DayCounter>& dc = std::nullopt);

double simulationLagTime(const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationTermStructure>& ts,
                         const std::optional<QuantLib::DayCounter>& dc = std::nullopt);

/*! Compute a seasonality-adjusted zero rate for a given observation date.
    It takes the time tau between the term structure base date and the observation date as
    given and dont adjust it internally based on the frequency of the inflation curve.
    \param baseDate inflation base date
    \param observationDate date at which the seasonality factor is evaluated
    \param unadjustedZeroRate the zero inflation rate before seasonality
    \param tau the year fraction between the term structure base date and the observation date
    \param ts the zero inflation term structure that may contain seasonality
    \return the seasonality-adjusted zero inflation rate
*/
QuantLib::Real continuousSeasonalityAdjustment(const QuantLib::Date& baseDate, const QuantLib::Date& observationDate, QuantLib::Rate unadjustedZeroRate,
                                     QuantLib::Time tau,
                                     const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationTermStructure>& ts);

/*! Applies seasonality adjustment to a given CPI value assuming the CPI doesnt include the seasonality
    This method is used to seasonalize the CPI fixings for the Cross-AssetModel, where the inflation is modelled without
   seasonality, and the seasonality is applied on top of the model.
*/
QuantLib::Real seasonalizeCPI(const QuantLib::Date& observationDate, const QuantLib::Real CPI,
                    const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts);

QuantLib::Real seasonalizeCPI(const QuantLib::Date& observationDate, const QuantLib::Real CPI,
                      const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationTermStructure>& ts);

QuantLib::Real deseasonalizeCPI(const QuantLib::Date& observationDate, const QuantLib::Real CPI,
                      const QuantLib::Handle<QuantLib::ZeroInflationTermStructure>& ts);

QuantLib::Real deseasonalizeCPI(const QuantLib::Date& observationDate, const QuantLib::Real CPI,
                      const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationTermStructure>& ts);

double scenarioInflationZeroRateFromModelTs(const Date& simDate, const Period tenor, const Period observationLag,
                                            const QuantLib::ext::shared_ptr<ZeroInflationIndex> index,
                                            const QuantLib::ext::shared_ptr<ZeroInflationModelTermStructure> modelTs,
                                            const CrossAssetModel::ModelType modelType, const DayCounter simulationDc);

double scenarioBaseCpi(const Real y, const Real z, const QuantLib::Date& date,
                       const QuantLib::ext::shared_ptr<CrossAssetModel>& model, const Size modelIdx,
                       const DayCounter& simulationDc, const QuantLib::ext::shared_ptr<ZeroInflationIndex>& index);

/*! Calculate the Compound Factor to compute the nominal price from the real price
   I(t_s)/I(t_0) with I(t_s) the CPI at settlement date and I(t_0) the bond's base CPI
*/
QuantLib::Real inflationLinkedBondQuoteFactor(const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond);

/*! Iterates over all bond cashflows, and extract all inflation underlyings */
std::map<std::tuple<std::string, QuantLib::CPI::InterpolationType, QuantLib::Frequency, QuantLib::Period>,
         QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>>
extractAllInflationUnderlyingFromBond(const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond);

namespace ZeroInflation {

//! Check if today - availabilityLag is already known, otherwise return the fixingDate of the previous fixing
QuantLib::Date lastAvailableFixing(const QuantLib::InflationIndex& index, const QuantLib::Date& asof);

//! Computes a CPI fixing giving an zeroIndex, with interpolation if needed 
QuantLib::Rate cpiFixing(const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index, const QuantLib::Date& maturity,
                         const QuantLib::Period& obsLag, bool interpolated);


//! derives the zero inflation curve base date based on the useLastKnownFixing rule
QuantLib::Date curveBaseDate(const bool baseDateLastKnownFixing, const QuantLib::Date& refDate,
                             const QuantLib::Period obsLagCurve, const QuantLib::Frequency curveFreq,
                             const QuantLib::ext::shared_ptr<QuantLib::InflationIndex>& index);

//! computes the fixingDate for ZC CPI Swap following the rule
//! for an interpolated index it is d - obsLag but 
//! for an interpolated index the fixing date is per definition on the start of the 
//! inflation period in which d - obsLag falls
QuantLib::Date fixingDate(const QuantLib::Date& d, const QuantLib::Period obsLag,
                                        const QuantLib::Frequency,
                                        bool interpolated);


/*! Computes the base rate for curve construction so that zero inflation rate is constant up to the first pillar
* Accounts for the acctual accrued inflation between the ZCIIS base date and the curve base date (e.g. last published fixing date)
* If curve base date and ZCIIS are the same, then the base rate is the ZCIIS rate
*/
QuantLib::Rate guessCurveBaseRate(const bool baseDateLastKnownFixing, const QuantLib::Date& swapStart,
                                  const QuantLib::Date& asof,
                                  const QuantLib::Period& swapTenor, const QuantLib::DayCounter& swapZCLegDayCounter,
                                  const QuantLib::Period& swapObsLag, const QuantLib::Rate zeroCouponRate, 
                                  const QuantLib::Period& curveObsLag, const QuantLib::DayCounter& curveDayCounter,
                                  const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index, const bool interpolated,
                                  const QuantLib::ext::shared_ptr<QuantLib::Seasonality>& seasonality = nullptr);


//! checks if the vols are normal or lognormal 
//! if the volsurface is not derived from QuantExt::CPIVolatilitySurface we default to lognormal vols
bool isCPIVolSurfaceLogNormal(const QuantLib::ext::shared_ptr<QuantLib::CPIVolatilitySurface>& surface);


}


} // namespace QuantExt

#endif
