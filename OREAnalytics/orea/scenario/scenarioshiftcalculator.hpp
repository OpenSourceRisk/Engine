/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/scenario/scenarioshiftcalculator.hpp
    \brief Class for calculating the shift multiple between two scenarios for a given key
*/

#pragma once

#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>

namespace ore {
namespace analytics {

/*! Class for calculating the shift multiple between two scenarios for a given risk factor key.

    The shift value returned is a value that is consistent with the SensitivityScenarioData
    and ScenarioSimMarketParameters passed in during construction. In other words, multiplying
    the result of the shift method with sensitivities generated using the SensitivityScenarioData
    configuration will give a valid estimate of the P&L move associated with moving from one
    scenario to another.
*/
class ScenarioShiftCalculator {
public:
    /*! Constructor
        \param sensitivityConfig sensitivity configuration that will determine the result
                                 returned by the shift method
        \param simMarketConfig   simulation market configuration for the scenarios that will
                                 be fed to the shift method
        \param simMarket         simulation market that will be used if provided
    */
    ScenarioShiftCalculator(const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityConfig,
                            const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketConfig,
			    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket =
			    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>());
    /*! Calculate the shift in the risk factor \p key implied by going from scenario \p s_1 to
        scenario \p s_2.
    */
    QuantLib::Real shift(const ore::analytics::RiskFactorKey& key, const ore::analytics::Scenario& s_1,
                         const ore::analytics::Scenario& s_2) const;

private:
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensitivityConfig_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketConfig_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket> simMarket_;
  
    /*! For some risk factors, the sensitivty is understood to be to a transform
        of the quantity that appears in the scenario and this transform can generally
        require the time to expiry of the factor.

        For example, the SensitivityScenarioData expresses shifts for IR in terms of
        zero rates and the scenarios hold discount factors so to convert the scenario
        value from \f$df_t\f$ to \f$z_t\f$, you need to know the year fraction until
        maturity i.e. \f$\tau(0, t)\f$ and then the transformed value is:
        \f[
            z_t = - \frac{\ln(df_t)}{\tau(0, t)}
        \f]
    */
    QuantLib::Real transform(const ore::analytics::RiskFactorKey& key, QuantLib::Real value,
                             const QuantLib::Date& asof) const;
};

} // namespace analytics
} // namespace ore
