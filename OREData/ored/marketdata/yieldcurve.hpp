/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
 Copyright (C) 2023 Oleg Kulkov
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

/*! \file ored/marketdata/yieldcurve.hpp
    \brief Wrapper class for QuantLib term structures.
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/configuration/yieldcurveconfig.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>
#include <ored/marketdata/yieldcurve.hpp>

#include <ql/termstructures/globalbootstrap.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using ore::data::Conventions;
using ore::data::CurveConfigurations;
using ore::data::YieldCurveConfig;
using ore::data::YieldCurveConfigMap;
using ore::data::YieldCurveSegment;

class DefaultCurve;
class ReferenceDataManager;

//! Wrapper class for building yield term structures
/*!
  Given yield curve specification and its configuration
  this class will actually build a QuantLib yield
  termstructure.

  \ingroup curves
*/
class YieldCurve {
public:
    //! Supported interpolation variables
    enum class InterpolationVariable { Zero, Discount, Forward };

    //! Supported interpolation methods
    enum class InterpolationMethod {
        Linear,
        LogLinear,
        NaturalCubic,
        FinancialCubic,
        ConvexMonotone,
        Quadratic,
        LogQuadratic,
        LogNaturalCubic,
        LogFinancialCubic,
        LogCubicSpline,
        MonotonicLogCubicSpline,
        Hermite,
        CubicSpline,
        DefaultLogMixedLinearCubic,
        MonotonicLogMixedLinearCubic,
        KrugerLogMixedLinearCubic,
        LogMixedLinearCubicNaturalSpline,
        BackwardFlat,       // backward-flat interpolation
        ExponentialSplines, // fitted bond curves only
        NelsonSiegel,       // fitted bond curves only
        Svensson            // fitted bond curves only
    };

    //! Constructor
    YieldCurve( //! Valuation date
        Date asof,
        //! Yield curve specifications
        const std::vector<QuantLib::ext::shared_ptr<YieldCurveSpec>>& curveSpec,
        //! Repository of yield curve configurations
        const CurveConfigurations& curveConfigs,
        // TODO shared_ptr or ref?
        //! Market data loader
        const Loader& loader,
        //! Map of underlying yield curves if required
        const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& requiredYieldCurves =
            map<string, QuantLib::ext::shared_ptr<YieldCurve>>(),
        //! Map of underlying default curves if required
        const map<string, QuantLib::ext::shared_ptr<DefaultCurve>>& requiredDefaultCurves =
            map<string, QuantLib::ext::shared_ptr<DefaultCurve>>(),
        //! FxTriangultion to get FX rate from cross if needed
        const FXTriangulation& fxTriangulation = FXTriangulation(),
        //! optional pointer to reference data, needed to build fitted bond curves
        const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
        //! ibor fallback config
        const IborFallbackConfig& iborfallbackConfig = IborFallbackConfig::defaultConfig(),
        //! if true keep qloader quotes linked to yield ts, otherwise detach them
        const bool preserveQuoteLinkage = false,
        //! build calibration info
        const bool buildCalibrationInfo = true,
        //! market object to look up external discount curves
        const Market* market = nullptr);

    //! \name Inspectors
    //@{
    const Date& asofDate() const { return asofDate_; }
    const std::vector<Currency>& currencies() const { return currency_; }
    const std::vector<QuantLib::ext::shared_ptr<YieldCurveSpec>>& curveSpecs() const { return curveSpec_; }
    const Handle<YieldTermStructure>& handle(const std::string& specName = std::string()) const;
    // might return nullptr, if no info was produced for the specified curve
    QuantLib::ext::shared_ptr<YieldCurveCalibrationInfo>
    calibrationInfo(const std::string& specName = std::string()) const;
    //@}

private:
    Date asofDate_;

    std::vector<QuantLib::ext::shared_ptr<YieldCurveSpec>> curveSpec_;
    std::vector<Currency> currency_;
    std::vector<DayCounter> zeroDayCounter_;
    std::vector<bool> extrapolation_;
    std::vector<Handle<YieldTermStructure>> discountCurve_;
    std::vector<bool> discountCurveGiven_;
    std::vector<QuantLib::ext::shared_ptr<YieldCurveConfig>> curveConfig_;
    std::vector<vector<QuantLib::ext::shared_ptr<YieldCurveSegment>>> curveSegments_;
    std::vector<InterpolationVariable> interpolationVariable_;
    std::vector<InterpolationMethod> interpolationMethod_;

    const Loader& loader_; // only used in ctor
    std::vector<RelinkableHandle<YieldTermStructure>> h_;
    std::vector<QuantLib::ext::shared_ptr<YieldTermStructure>> p_;
    std::vector<QuantLib::ext::shared_ptr<YieldCurveCalibrationInfo>> calibrationInfo_;

    void buildBootstrappedCurve(const std::set<std::size_t>& indices);

    void buildDiscountCurve(const std::size_t index);
    void buildZeroCurve(const std::size_t index);
    void buildZeroSpreadedCurve(const std::size_t index);
    //! Build a yield curve that uses QuantExt::DiscountRatioModifiedCurve
    void buildDiscountRatioCurve(const std::size_t index);
    //! Build a yield curve that uses QuantLib::FittedBondCurve
    void buildFittedBondCurve(const std::size_t index);
    //! Build a yield curve that uses QuantExt::WeightedYieldTermStructure
    void buildWeightedAverageCurve(const std::size_t index);
    //! Build a yield curve that uses QuantExt::YieldPlusDefaultYieldTermStructure
    void buildYieldPlusDefaultCurve(const std::size_t index);
    //! Build a yield curve that uses QuantExt::IborFallbackCurve
    void buildIborFallbackCurve(const std::size_t index);
    //! Build a yield curve that uses QuantExt::bondYieldShiftedCurve
    void buildBondYieldShiftedCurve(const std::size_t index);

    //! Return the yield curve with the given \p id from the requiredYieldCurves_ map
    QuantLib::Handle<YieldTermStructure> getYieldCurve(const std::size_t index, const std::string& ccy,
                                                       const std::string& id) const;

    map<string, QuantLib::ext::shared_ptr<YieldCurve>> requiredYieldCurves_;
    map<string, QuantLib::ext::shared_ptr<DefaultCurve>> requiredDefaultCurves_;
    const FXTriangulation& fxTriangulation_;
    const QuantLib::ext::shared_ptr<ReferenceDataManager> referenceData_;
    IborFallbackConfig iborFallbackConfig_;
    const bool preserveQuoteLinkage_;
    bool buildCalibrationInfo_;
    const Market* market_;

    map<string, QuantLib::RelinkableHandle<YieldTermStructure>> requiredYieldCurveHandles_;

    std::pair<QuantLib::ext::shared_ptr<YieldTermStructure>, const MultiCurveBootstrapContributor*>
    buildPiecewiseCurve(const std::size_t index, const std::size_t mixedInterpolationSize,
                        const vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);

    QuantLib::ext::shared_ptr<YieldTermStructure>
    flattenPiecewiseCurve(const std::size_t index, const QuantLib::ext::shared_ptr<YieldTermStructure>& yieldts,
                          const std::size_t mixedInterpolationSize,
                          const vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);

    /* Functions to build RateHelpers from yield curve segments */
    void addDeposits(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                     vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addFutures(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                    vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addFras(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                 vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addOISs(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                 vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addSwaps(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                  vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addAverageOISs(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                        vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addTenorBasisSwaps(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                            vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addTenorBasisTwoSwaps(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                               vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addBMABasisSwaps(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                          vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addFXForwards(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                       vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addCrossCcyBasisSwaps(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                               vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);
    void addCrossCcyFixFloatSwaps(const std::size_t index, const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                                  vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments);

    // get the fx spot from the string provided
    QuantLib::ext::shared_ptr<FXSpotQuote> getFxSpotQuote(string spotId);

    // get the index of a spec name
    std::size_t index(const std::string& specName) const;
};

//! Helper function for parsing interpolation method
YieldCurve::InterpolationMethod parseYieldCurveInterpolationMethod(const string& s);
//! Helper function for parsing interpolation variable
YieldCurve::InterpolationVariable parseYieldCurveInterpolationVariable(const string& s);

//! Output operator for interpolation method
std::ostream& operator<<(std::ostream& out, const YieldCurve::InterpolationMethod m);

//! Templated function to build a YieldTermStructure and apply interpolation methods to it
template <template <class> class CurveType>
QuantLib::ext::shared_ptr<YieldTermStructure>
buildYieldCurve(const vector<Date>& dates, const vector<QuantLib::Real>& rates, const DayCounter& dayCounter,
                YieldCurve::InterpolationMethod interpolationMethod, Size n = 0);

//! Create a Interpolated Zero Curve and apply interpolators
QuantLib::ext::shared_ptr<YieldTermStructure> zerocurve(const vector<Date>& dates, const vector<Rate>& yields,
                                                        const DayCounter& dayCounter,
                                                        YieldCurve::InterpolationMethod interpolationMethod,
                                                        Size n = 0);

//! Create a Interpolated Discount Curve and apply interpolators
QuantLib::ext::shared_ptr<YieldTermStructure>
discountcurve(const vector<Date>& dates, const vector<DiscountFactor>& dfs, const DayCounter& dayCounter,
              YieldCurve::InterpolationMethod interpolationMethod, Size n = 0);

//! Create a Interpolated Forward Curve and apply interpolators
QuantLib::ext::shared_ptr<YieldTermStructure> forwardcurve(const vector<Date>& dates, const vector<Rate>& forwards,
                                                           const DayCounter& dayCounter,
                                                           YieldCurve::InterpolationMethod interpolationMethod,
                                                           Size n = 0);

} // namespace data
} // namespace ore
