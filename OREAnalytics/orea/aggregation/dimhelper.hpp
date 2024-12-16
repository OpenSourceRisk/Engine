/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file oreap/dim/dimhelper.hpp
    \brief Dynamic initial margin helper

    \ingroup dim
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orepbase/orea/engine/sensitivitystoragemanager.hpp>

#include <ored/portfolio/trade.hpp>
#include <ored/utilities/log.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ql/math/matrix.hpp>

namespace oreplus {
namespace analytics {
using namespace ore::data;
using namespace ore::analytics;

using QuantExt::CrossAssetModel;
using QuantLib::Matrix;
using QuantLib::Time;

//! Helper for cam model implied VaR calculation
/*! delta or delta-gamma (normal) based estimation of VaR */
class DimHelper {
public:
    /*! Construct the dim helper based on
        - the cam model used for simulation
        - the cube which stores the sensitivities on a netting set level
        - the sensitivity storage manager
        - the grid on which the curve sensitivities are computed
        - the dim horizon expressed in calendar days
        Notice that we assume that the model has IR LGM1F and FX BS components only */
    DimHelper(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, const QuantLib::ext::shared_ptr<NPVCube>& cube,
              const QuantLib::ext::shared_ptr<SensitivityStorageManager>& sensitivityStorageManager,
              const std::vector<Time>& curveSensitivityGrid, const Size dimHorizonCalendarDays);

    /*! Returns the VaR for a
        - netting set id
        - an order
           order = 1: delta (order 1)
           order = 2: delta-gamma, with normal distribution assumption ("order 1.5")
           order = 3: delta-gamma (order 2)
        - a quantile
        - a theta factor, the netting set theta times this factor is added to the result
        - a date and sample index, if both are null, the VaR is computed for the T0 slice of the cube */
    Real var(const std::string& nettingSetId, const Size order, const Real quantile, const Real thetaFactor = 0.0,
             const Size dateIndex = Null<Size>(), const Size sampleIndex = Null<Size>());

private:
    const QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    const QuantLib::ext::shared_ptr<NPVCube> cube_;
    const QuantLib::ext::shared_ptr<SensitivityStorageManager> sensitivityStorageManager_;

    std::vector<Matrix> covariances_;
};

} // namespace analytics
} // namespace oreplus
