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

/*! \file orepsimm/orea/crifmarket.hpp
    \brief Market used when generating CRIF file
*/

#pragma once

#include <ored/marketdata/marketimpl.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>

namespace ore {
namespace analytics {

/*! Market providing access to market data values needed during CRIF generation.
*/
class CrifMarket : public ore::data::MarketImpl {
public:
    //! Constructor of an empty market.
    CrifMarket(const QuantLib::Date& asof);

    //! Constructor that attempts to populate the relevant portions of the market.
    CrifMarket(const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& ssm,
               const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& ssd,
               const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs);

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket() const;
    const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiData() const;
    //@}

private:
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket> ssm_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> ssd_;
    QuantLib::ext::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;

    /*! Populate optionlet volatilities with ATM optionlet curves from the ScenarioSimMarket member \c ssm_. The 
        SensitivityScenarioData member \c ssd_ is used to determine the pillars of the ATM optionlet curves that are 
        created.
    */
    void addAtmOptionletVolatilities();

    /*! Populate swaption volatilities with ATM swaption surfaces from the ScenarioSimMarket member \c ssm_. The 
        SensitivityScenarioData member \c ssd_ is used to determine the expiries and underlying swap tenors of the 
        ATM swaption surfaces that are created.
    */
    void addAtmSwaptionVolatilities();
};

}
}
