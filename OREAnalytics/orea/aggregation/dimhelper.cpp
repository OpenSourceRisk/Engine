/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <orepbase/orea/dim/dimhelper.hpp>

#include <ored/portfolio/fxoption.hpp>

#include <qle/math/deltagammavar.hpp>
#include <qle/models/crossassetanalytics.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace QuantExt::CrossAssetAnalytics;

namespace oreplus {
namespace analytics {

DimHelper::DimHelper(const QuantLib::ext::shared_ptr<CrossAssetModel>& model, const QuantLib::ext::shared_ptr<NPVCube>& cube,
                     const QuantLib::ext::shared_ptr<SensitivityStorageManager>& sensitivityStorageManager,
                     const std::vector<Time>& curveSensitivityGrid, const Size dimHorizonCalendarDays)
    : model_(model), cube_(cube), sensitivityStorageManager_(sensitivityStorageManager) {

    QL_REQUIRE(cube, "DimHelper: cube is null");
    QL_REQUIRE(sensitivityStorageManager, "DimHelper: sensitivityStorageManager is null");
    QL_REQUIRE(QuantLib::ext::dynamic_pointer_cast<CamSensitivityStorageManager>(sensitivityStorageManager),
               "DimHelper: wrong sensitivity storage manager type, expected CamSensitivityStorageManager");

    // build model implied covariance matrices, the coordinates are organised as follows
    // z1(1), ..., zn(1), z1(2), ... zn(2), ... , z1(c), ... , zn(c), fx(2), fx(3), ... , fx(c)
    // where we have c currencies in the cross asset IR-FX model
    // note that this is one-curve, since in the model no stochastic basis is present
    // note also that there is no stochastic vol present, so no vol component, i.e. vega for
    // european swaptions is ignored

    LOG("DimHelper: Build model implied covariance matrices");
    std::vector<Date> dates(1, cube_->asof());
    dates.insert(dates.end(), cube_->dates().begin(), cube_->dates().end());
    Size n = curveSensitivityGrid.size();
    Size c = model_->components(CrossAssetModel::AssetType::IR);
    covariances_.resize(dates.size(), Matrix(n * c + (c - 1), n * c + (c - 1), 0.0));
    LOG("Building " << dates.size() << " matrices with rows = cols = " << covariances_.front().rows() << " (" << n
                    << " curve sensitivity grid points, " << c << " currencies)");
    for (Size i = 0; i < dates.size(); ++i) {
        Time t1 = model_->irlgm1f(0)->termStructure()->timeFromReference(dates[i]);
        Time t2 = model_->irlgm1f(0)->termStructure()->timeFromReference(dates[i] + dimHorizonCalendarDays);

        // IR-IR, TODO exploit symmetries here and below, for the moment keep it simple and readable
        for (Size cc = 0; cc < c; ++cc) {
            for (Size dd = 0; dd < c; ++dd) {
                for (Size ii = 0; ii < n; ++ii) {
                    for (Size jj = 0; jj < n; ++jj) {
                        Size idx1 = cc * n + ii;
                        Size idx2 = dd * n + jj;
                        if (idx1 > idx2)
                            continue;
                        Real cov = integral(*model_,
                                            P(HTtz(cc, curveSensitivityGrid[ii]), HTtz(dd, curveSensitivityGrid[jj]),
                                              az(cc), az(dd), rzz(cc, dd)),
                                            t1, t2) /
                                   (curveSensitivityGrid[ii] * curveSensitivityGrid[jj]);
                        covariances_[i][idx1][idx2] = covariances_[i][idx2][idx1] = cov;
                    }
                }
            }
        }
        // IR-FX
        for (Size cc = 0; cc < c; ++cc) {
            for (Size ii = 0; ii < n; ++ii) {
                for (Size dd = 0; dd < c - 1; ++dd) {
                    Size idx1 = c * n + dd;
                    Size idx2 = cc * n + ii;
                    if (idx1 > idx2)
                        continue;
                    Real cov =
                        integral(*model_, P(HTtz(cc, curveSensitivityGrid[ii]), az(cc), sx(dd), rzx(cc, dd)), t1, t2);
                    covariances_[i][idx1][idx2] = covariances_[i][idx2][idx1] = cov;
                }
            }
        }
        // FX-FX
        for (Size cc = 0; cc < c - 1; ++cc) {
            for (Size ii = 0; ii < n; ++ii) {
                for (Size dd = 0; dd < c - 1; ++dd) {
                    Size idx1 = c * n + cc;
                    Size idx2 = c * n + dd;
                    if (idx1 > idx2)
                        continue;
                    Real cov = integral(*model_, P(sx(cc), sx(dd), rxx(cc, dd)), t1, t2);
                    covariances_[i][idx1][idx2] = covariances_[i][idx2][idx1] = cov;
                }
            }
        }
        TLOG("Timestep " << i << " from " << t1 << " to " << t2 << ":");
        TLOGGERSTREAM(covariances_[i]);
    } // for dates
    LOG("Covariance matrix building finished.");
} // DimHelper()

Real DimHelper::var(const std::string& nettingSetId, const Size order, const Real quantile, const Real thetaFactor,
                    const Size dateIndex, const Size sampleIndex) {

    QL_REQUIRE(order == 1 || order == 2 || order == 3,
               "DimHelper: order (" << order << ") must be 1 (d), 2 (d-g-normal) or 3 (d-g)");

    QL_REQUIRE((dateIndex == Null<Size>() && sampleIndex == Null<Size>()) ||
                   (dateIndex != Null<Size>() && sampleIndex != Null<Size>()),
               "CamSensitivityStorageManager::addSensitivities(): date and sample index must be both null (write to T0 "
               "slice) or both not null");

    auto r = sensitivityStorageManager_->getSensitivities(cube_, nettingSetId, dateIndex, sampleIndex);
    QL_REQUIRE(r.type() == typeid(std::tuple<Array, Matrix, Real>),
               "DimHelper::var(): unexpected result type '" << r.type().name() << "' from sensitivityStorageManager");

    Array delta;
    Matrix gamma;
    Real theta;

    std::tie(delta, gamma, theta) = boost::any_cast<std::tuple<Array, Matrix, Real>>(r);
    const Matrix& cov = covariances_[dateIndex == Null<Size>() ? 0 : dateIndex + 1];

    // check if gamma is zero, then we can compute a delta var irrespective of the order that is given
    bool gammaIsZero = true;
    for (Size i = 0; i < gamma.rows(); ++i) {
        for (Size j = 0; j <= i; ++j) {
            if (!close_enough(gamma(i, j), 0.0))
                gammaIsZero = false;
        }
    }

    Real res;
    if (order == 1 || gammaIsZero) {
        res = deltaVar(cov, delta, quantile);
    } else if (order == 2) {
        res = deltaGammaVarNormal(cov, delta, gamma, quantile);
    } else if (order == 3) {
        // res = deltaGammaVarSaddlepoint(cov, delta, gamma, quantile);
        res = deltaGammaVarCornishFisher(cov, delta, gamma, quantile);
    } else {
        QL_FAIL("order (" << order << ") unexpected, must be 1, 2 or 3");
    }

    return res + theta * thetaFactor;

} // var

} // namespace analytics
} // namespace oreplus
