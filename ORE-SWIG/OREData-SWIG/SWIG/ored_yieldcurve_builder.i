/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef ored_yieldcurve_builder_i
#define ored_yieldcurve_builder_i

%include ored_curvespec.i
%include ored_curveconfigurations.i
%include ored_loader.i
%include ored_iborfallbackconfig.i
%include ored_referencedatamanager.i

%{
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/marketdata/defaultcurve.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ored/marketdata/structuredcurveerror.hpp>
#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>
using ore::data::YieldCurveCalibrationInfo;
using ore::data::FittedBondCurveCalibrationInfo;
using ore::data::YieldCurve;
using ore::data::DefaultCurve;
using ore::data::StructuredCurveErrorMessage;
using ore::data::StructuredCurveWarningMessage;
%}

// ---------------------------------------------------------------------------
// YieldCurveCalibrationInfo
// ---------------------------------------------------------------------------

// Suppress complex nested container members SWIG cannot handle
%ignore ore::data::YieldCurveCalibrationInfo::rateHelperPillarDates;
%ignore ore::data::YieldCurveCalibrationInfo::rateHelperCashflows;
%ignore ore::data::YieldCurveCalibrationInfo::defaultPeriods;

%shared_ptr(ore::data::YieldCurveCalibrationInfo)
%shared_ptr(ore::data::FittedBondCurveCalibrationInfo)

namespace ore {
namespace data {

struct YieldCurveCalibrationInfo {
    std::string dayCounter;
    std::string currency;
    std::vector<QuantLib::Date> pillarDates;
    std::vector<double> zeroRates;
    std::vector<double> discountFactors;
    std::vector<double> times;
    std::vector<std::string> mdQuoteLabels;
    std::vector<double> mdQuoteValues;
    std::vector<std::string> rateHelperTypes;
    std::vector<double> rateHelperQuoteErrors;

    %extend {
        static QuantLib::ext::shared_ptr<ore::data::FittedBondCurveCalibrationInfo>
        getFullView(const QuantLib::ext::shared_ptr<ore::data::YieldCurveCalibrationInfo>& base) {
            return QuantLib::ext::dynamic_pointer_cast<ore::data::FittedBondCurveCalibrationInfo>(base);
        }
    }
};

struct FittedBondCurveCalibrationInfo : public ore::data::YieldCurveCalibrationInfo {
    std::string fittingMethod;
    std::vector<double> solution;
    int iterations;
    double costValue;
    double tolerance;
    std::vector<std::string> securities;
    std::vector<QuantLib::Date> securityMaturityDates;
    std::vector<double> marketPrices;
    std::vector<double> modelPrices;
    std::vector<double> marketYields;
    std::vector<double> modelYields;
};

} // namespace data
} // namespace ore

// ---------------------------------------------------------------------------
// StructuredCurveErrorMessage / StructuredCurveWarningMessage
// (exposed as standalone diagnostic types; base class hierarchy is not wrapped)
// ---------------------------------------------------------------------------

namespace ore {
namespace data {

class StructuredCurveErrorMessage {
public:
    StructuredCurveErrorMessage(const std::string& curveId,
                                const std::string& exceptionType,
                                const std::string& exceptionWhat);
};

class StructuredCurveWarningMessage {
public:
    StructuredCurveWarningMessage(const std::string& curveId,
                                  const std::string& exceptionType,
                                  const std::string& exceptionWhat);
};

} // namespace data
} // namespace ore

// ---------------------------------------------------------------------------
// YieldCurve – curve builder / bootstrapper
// ---------------------------------------------------------------------------

%shared_ptr(ore::data::YieldCurve)
%shared_ptr(ore::data::DefaultCurve)

%template(YieldCurveSpecVector) std::vector<QuantLib::ext::shared_ptr<ore::data::YieldCurveSpec>>;
%template(YieldCurveMap) std::map<std::string, QuantLib::ext::shared_ptr<ore::data::YieldCurve>>;
%template(DefaultCurveMap) std::map<std::string, QuantLib::ext::shared_ptr<ore::data::DefaultCurve>>;

%nodefaultctor ore::data::YieldCurve;

namespace ore {
namespace data {

class YieldCurve {
public:
    %extend {
        //! Python-friendly constructor. Builds a yield curve from a list of YieldCurveSpec
        //! objects, the curve configurations, and a market data loader.
        //! Dependent yield and default curves are not supported via this interface;
        //! use TodaysMarket for multi-curve scenarios.
        YieldCurve(
            const QuantLib::Date& asof,
            const std::vector<QuantLib::ext::shared_ptr<ore::data::YieldCurveSpec>>& curveSpecs,
            const ore::data::CurveConfigurations& curveConfigs,
            const ore::data::Loader& loader,
            const QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig =
                QuantLib::ext::make_shared<ore::data::IborFallbackConfig>(
                    ore::data::IborFallbackConfig::defaultConfig()),
            bool buildCalibrationInfo = true) {
            return new ore::data::YieldCurve(
                asof, curveSpecs, curveConfigs, loader,
                /*requiredYieldCurves=*/ {},
                /*requiredDefaultCurves=*/ {},
                /*fxTriangulation=*/ ore::data::FXTriangulation(),
                /*referenceData=*/ nullptr,
                iborFallbackConfig,
                /*preserveQuoteLinkage=*/ false,
                buildCalibrationInfo,
                /*market=*/ nullptr,
                /*useAtParCoupons=*/ true);
        }
    }

    const QuantLib::Date& asofDate() const;
    const QuantLib::Handle<QuantLib::YieldTermStructure>&
        handle(const std::string& specName = std::string()) const;
    QuantLib::ext::shared_ptr<ore::data::YieldCurveCalibrationInfo>
        calibrationInfo(const std::string& specName = std::string()) const;
};

// ---------------------------------------------------------------------------
// DefaultCurve – hazard-rate / CDS-spread bootstrapper
// ---------------------------------------------------------------------------

class DefaultCurve {
public:
    //! Default (empty) constructor
    DefaultCurve();

    %extend {
        //! Python-friendly constructor. Builds a default curve from market data.
        //! Passes empty yield-curve and default-curve dependency maps; supply these
        //! via TodaysMarket for multi-curve scenarios.
        DefaultCurve(
            const QuantLib::Date& asof,
            const ore::data::DefaultCurveSpec& spec,
            const ore::data::Loader& loader,
            const ore::data::CurveConfigurations& curveConfigs,
            const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr) {
            std::map<std::string, QuantLib::ext::shared_ptr<ore::data::YieldCurve>> yieldCurves;
            std::map<std::string, QuantLib::ext::shared_ptr<ore::data::DefaultCurve>> defaultCurves;
            return new ore::data::DefaultCurve(asof, spec, loader, curveConfigs,
                                               yieldCurves, defaultCurves, referenceData);
        }
    }

    const ore::data::DefaultCurveSpec& spec() const;
    const QuantLib::ext::shared_ptr<QuantExt::CreditCurve>& creditCurve() const;
    QuantLib::Real recoveryRate();
};

} // namespace data
} // namespace ore

#endif
