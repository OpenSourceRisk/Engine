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

/*! \file ored/model/inflation/infjybuilder.hpp
    \brief Builder for a Jarrow Yildrim inflation model component
    \ingroup models
*/

#pragma once

#include <ored/marketdata/market.hpp>
#include <ored/model/inflation/infjydata.hpp>
#include <qle/models/marketobserver.hpp>
#include <qle/models/modelbuilder.hpp>
#include <qle/models/infjyparameterization.hpp>

namespace ore {
namespace data {

/*! Builder for a Jarrow Yildrim inflation model component
    
    This class is a utility to turn a Jarrow Yildrim inflation model component description into an inflation model 
    parameterization which can be used to instantiate a CrossAssetModel.

    \ingroup models
*/
class InfJyBuilder : public QuantExt::ModelBuilder {
public:
    /*! Constructor
        \param market                   Market object
        \param data                     Jarrow Yildrim inflation model description
        \param configuration            Market configuration to use
        \param referenceCalibrationGrid The reference calibration grid
    */
    InfJyBuilder(const QuantLib::ext::shared_ptr<Market>& market, const QuantLib::ext::shared_ptr<InfJyData>& data,
                 const std::string& configuration = Market::defaultConfiguration,
                 const std::string& referenceCalibrationGrid = "", const bool donCalibrate = false);

    using Helpers = std::vector<QuantLib::ext::shared_ptr<QuantLib::CalibrationHelper>>;
    
    //! \name Inspectors
    //@{
    std::string inflationIndex() const;
    QuantLib::ext::shared_ptr<QuantExt::InfJyParameterization> parameterization() const;
    Helpers realRateBasket() const;
    Helpers indexBasket() const;
    //@}

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

    void setCalibrationDone() const;

private:
    QuantLib::ext::shared_ptr<Market> market_;
    std::string configuration_;
    QuantLib::ext::shared_ptr<InfJyData> data_;
    std::string referenceCalibrationGrid_;
    bool dontCalibrate_;
    
    QuantLib::ext::shared_ptr<QuantExt::InfJyParameterization> parameterization_;
    QuantLib::ext::shared_ptr<QuantExt::MarketObserver> marketObserver_;

    // The rate curve to use
    Handle<YieldTermStructure> rateCurve_;

    // We always need a ZeroInflationIndex to build the JY model.
    QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> zeroInflationIndex_;

    // We may need these depending on the calibration instrument types.
    QuantLib::Handle<QuantLib::CPIVolatilitySurface> cpiVolatility_;
    QuantLib::ext::shared_ptr<QuantLib::YoYInflationIndex> yoyInflationIndex_;
    QuantLib::Handle<QuantLib::YoYOptionletVolatilitySurface> yoyVolatility_;

    // Helper flag used in the forceRecalculate() method.
    bool forceCalibration_ = false;

    /*! Calibration instruments to use for calibrating the real rate portion of the JY model. The basket is
        empty if we are not calibrating the real rate portion of the JY model. Depending on the calibration 
        configuration, either the real rate reversion parameter or the real rate volatility parameter will be 
        adjusted in order to match these instruments.
    */
    mutable Helpers realRateBasket_;
    mutable std::vector<bool> rrInstActive_;
    mutable QuantLib::Array rrInstExpiries_;

    /*! Calibration instruments to use for calibrating the inflation index portion of the JY model. The basket is 
        empty if we are not calibrating the inflation index portion of the JY model.
    */
    mutable Helpers indexBasket_;
    mutable std::vector<bool> indexInstActive_;
    mutable QuantLib::Array indexInstExpiries_;

    //! Cache the prices of all of the active calibration helper instruments.
    mutable std::vector<QuantLib::Real> priceCache_;

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    //! Build any calibration baskets requested by the configuration i.e. via the \c data_ member.
    void buildCalibrationBaskets() const;

    //! Build the calibration basket.
    Helpers buildCalibrationBasket(const CalibrationBasket& cb, std::vector<bool>& active,
        QuantLib::Array& expiries, bool forRealRateReversion = false) const;

    //! Build a CPI cap floor calibration basket.
    Helpers buildCpiCapFloorBasket(const CalibrationBasket& cb, std::vector<bool>& active,
        QuantLib::Array& expiries) const;

    //! Build a YoY cap floor calibration basket.
    Helpers buildYoYCapFloorBasket(const CalibrationBasket& cb, std::vector<bool>& active,
        QuantLib::Array& expiries) const;

    //! Build a YoY swap calibration basket.
    Helpers buildYoYSwapBasket(const CalibrationBasket& cb, std::vector<bool>& active,
        QuantLib::Array& expiries, bool forRealRateReversion = false) const;

    //! Find calibration basket with parameter value equal to \p parameter
    const CalibrationBasket& calibrationBasket(const std::string& parameter) const;

    //! Create the real rate parameterisation.
    QuantLib::ext::shared_ptr<QuantExt::Lgm1fParametrization<ZeroInflationTermStructure>> createRealRateParam() const;

    //! Create the inflation index parameterisation.
    QuantLib::ext::shared_ptr<QuantExt::FxBsParametrization> createIndexParam() const;

    /*! Perform checks and possibly adjust the \p times and \p values array depending on calibration configuration.
    */
    void setupParams(const ModelParameter& param, QuantLib::Array& times, QuantLib::Array& values, 
        const QuantLib::Array& expiries, const std::string& paramName) const;

    //! Create the reference calibration dates.
    std::vector<QuantLib::Date> referenceCalibrationDates() const;

    //! Attempt to initialise market data members that may be needed for building calibration instruments.
    void initialiseMarket();

    /*! Returns \c true if the market value of any of the calibration helpers has changed. If \p updateCache is 
        \c true, the cached prices are updated if they have changed.
    */
    bool pricesChanged(bool updateCache) const;

    //! Return the market value of the given calibration helper
    QuantLib::Real marketPrice(const QuantLib::ext::shared_ptr<QuantLib::CalibrationHelper>& helper) const;
};

}
}
