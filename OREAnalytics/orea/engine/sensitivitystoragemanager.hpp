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

/*! \file engine/sensitivitystoragemanager.hpp
    \brief class helping to manage the storage of sensitivties in a cube
    \ingroup engine
*/

#pragma once

#include <orea/cube/npvcube.hpp>

#include <ored/marketdata/market.hpp>
#include <ored/portfolio/trade.hpp>

#include <boost/any.hpp>

namespace ore {
namespace analytics {

class SensitivityStorageManager {
public:
    virtual ~SensitivityStorageManager() {}

    /*! Get required size (i.e. number of floats / reals) to store sensitivities in cube. */
    virtual QuantLib::Size getRequiredSize() const = 0;

    /*! Add to the sensitivities for the given trade to the cube. The cube must contain an id matching the trade's
      netting set. The sensitivities are then added to this id. If dateIndex and sampleIndex are null, the T0 slice of
      the cube is populated, otherwise the cells for the given date and sample. The depth of the cube must at least be
      what getRequiredSize() returns. */
    virtual void addSensitivities(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube,
                                  const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                                  const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                                  const QuantLib::Size dateIndex = QuantLib::Null<QuantLib::Size>(),
                                  const QuantLib::Size sampleIndex = QuantLib::Null<QuantLib::Size>()) const = 0;

    /*! Get the stored sensitivities for a netting set for T0 (dateIndex = sampleIndex = null) or the given date and
      sample. The return value varies for each concrete implementation.  */
    virtual boost::any getSensitivities(const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube,
                                        const std::string& nettingSetId,
                                        const QuantLib::Size dateIndex = QuantLib::Null<QuantLib::Size>(),
                                        const QuantLib::Size sampleIndex = QuantLib::Null<QuantLib::Size>()) const = 0;
};

class CamSensitivityStorageManager : public SensitivityStorageManager {
public:
    /*! Assuming IR-FX coverage only with LGM1F and FXBS model components, for this the cam currencies list is
      sufficient to store the sensitivities in a suitable layout */
    explicit CamSensitivityStorageManager(const std::vector<std::string>& camCurrencies,
                                          const QuantLib::Size nCurveSensitivities,
                                          const QuantLib::Size nVegaOptSensitivities,
                                          const QuantLib::Size nVegaUndSensitivities,
                                          const QuantLib::Size nFxVegaSensitivities,
                                          const QuantLib::Size firstCubeIndexToUse,
                                          const bool use2ndOrderSensitivities);

    QuantLib::Size getRequiredSize() const override;

    void addSensitivities(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube,
                          const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                          const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                          const QuantLib::Size dateIndex = QuantLib::Null<QuantLib::Size>(),
                          const QuantLib::Size sampleIndex = QuantLib::Null<QuantLib::Size>()) const override;

    /*! Return delta, gamma, theta as tuple<Array, Matrix, Real> containing a delta vector, gamma matrix and theta
       scalar. The coordinates of the delta and gamma entries are

         ccy_1        irDelta_1
                      ...
                      irDelta_nCurveSensitivities

         ccy_2        irDelta_1
                      ...
                      irDelta_nCurveSensitivities

         ...

         ccy_nCamCcys irDelta_1
                      ...
                      irDelta_nCurveSensitivities

         log(fx)-Delta_1
         ...
         log(fx)-Delta_(nCamCcys-1)

         theta

         which means the number of components is nCurveSensitivities * nCamCurrencies + (nCamCurrencies - 1).
         All entries are in base ccy (= first ccy in camCurrencies), the fx deltas against base ccy.
    */

    boost::any getSensitivities(const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube, const std::string& nettingSetId,
                                const QuantLib::Size dateIndex = QuantLib::Null<QuantLib::Size>(),
                                const QuantLib::Size sampleIndex = QuantLib::Null<QuantLib::Size>()) const override;

private:
    QuantLib::Size getCcyIndex(const std::string& ccy) const;

    std::tuple<QuantLib::Array, QuantLib::Matrix, QuantLib::Real>
    processSwapSwaption(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube,
                        const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                        const QuantLib::ext::shared_ptr<ore::data::Market>& market) const;

    std::tuple<QuantLib::Array, QuantLib::Matrix, QuantLib::Real>
    processFxOption(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube, const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                    const QuantLib::ext::shared_ptr<ore::data::Market>& market) const;

    std::tuple<QuantLib::Array, QuantLib::Matrix, QuantLib::Real>
    processFxForward(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube, const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                     const QuantLib::ext::shared_ptr<ore::data::Market>& market) const;

    const std::vector<std::string> camCurrencies_;
    const QuantLib::Size nCurveSensitivities_;
    const QuantLib::Size firstCubeIndexToUse_;
    const bool use2ndOrderSensitivities_;

    QuantLib::Size N_;
};

// TODO add more storage managers here, e.g. one that stores sensitivities suitable for a SIMM calculation
// class SIMMSensitivityStorageManager : public SensitivityStorageManager {};

} // namespace analytics
} // namespace ore
