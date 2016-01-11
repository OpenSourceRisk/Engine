/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include "xassetmodel2.hpp"
#include "utilities.hpp"

#include <qle/methods/multipathgenerator.hpp>
#include <qle/models/all.hpp>
#include <qle/pricingengines/all.hpp>

#include <ql/currencies/all.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/math/statistics/incrementalstatistics.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/methods/montecarlo/multipathgenerator.hpp>
#include <ql/methods/montecarlo/pathgenerator.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/quotes/simplequote.hpp>

#include <boost/make_shared.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/covariance.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variates/covariate.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;
using namespace boost::accumulators;

void XAssetModelTest2::testFMSCase() {

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(18, Dec, 2015);

    Date refDate = Date(18, Dec, 2015);

    // ========================================================
    // correlation matrix (13 ccy, 12 fx, 3 inf pairs (CPI,RR))
    // ========================================================

    // note that we have a mapping from the original indices to the new indices
    // because we are setting up Inlflation RR as IR and Inflation CPI as FX
    // the mapping is as follows (original => here)
    // 0-12 => 0-12
    // 13-24 => 16-27
    // 25 => 28
    // 26 => 13
    // 27 => 29
    // 28 => 14
    // 29 => 30
    // 30 => 15
    Size mapping[31] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                        11, 12, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                        25, 26, 27, 28, 13, 29, 14, 30, 15};
    Size invMapping[31];
    for (Size i = 0; i < 31; ++i) {
        invMapping[mapping[i]] = i;
    }

    Real c[31][31] = {
        {1, 0.3, 0.3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -0, -0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0.5, 0.95, 0.15, 0.285, 0, 0},
        {0.3, 1, 0.3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -0, -0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0.15, 0.285, 0.5, 0.95, 0, 0},
        {0.3, 0.3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -0, -0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0.15, 0.285, 0.15, 0.285, 0, 0},
        {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {-0, -0, -0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0},
        {-0, -0, -0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
         0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         1, 0, 0, 0, 0, 0, 0},
        {0.5, 0.15, 0.15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 1, 0.475, 0.075, 0.1425, 0, 0},
        {0.95, 0.285, 0.285, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0.475, 1, 0.1425, 0.27075, 0, 0},
        {0.15, 0.5, 0.15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0.075, 0.1425, 1, 0.475, 0, 0},
        {0.285, 0.95, 0.285, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0.1425, 0.27075, 0.475, 1, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 1, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 1}};

    Matrix rho(31, 31);
    for (Size i = 0; i < 31; ++i) {
        for (Size j = 0; j < 31; ++j) {
            rho[mapping[i]][mapping[j]] = c[i][j];
        }
    }

    // check eigenvalues of rho (this check is done in the model constructor
    // as well, we cross check this here)

    SymmetricSchurDecomposition ssd(rho);
    for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
        if (ssd.eigenvalues()[i] < 0.0) {
            BOOST_ERROR("negative eigenvalue in input matrix (#"
                        << i << ", " << ssd.eigenvalues()[i]);
        }
    }

    // =========================================
    // IR components
    // =========================================

    Period irTen[] = {
        3 * Months, 6 * Months, 1 * Years,  4 * Years,  6 * Years,  8 * Years,
        10 * Years, 12 * Years, 14 * Years, 16 * Years, 24 * Years, 28 * Years,
        30 * Years, 35 * Years, 40 * Years, 45 * Years, 50 * Years};

    Array irTimes(LENGTH(irTen));
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        irTimes[i] = Actual365Fixed().yearFraction(
            refDate, TARGET().advance(refDate, irTen[i]));
    }
    // for parametrization set up (without last time)
    Array irTimes2(irTimes.begin(), irTimes.end() - 1);

    boost::shared_ptr<IrLgm1fParametrization> tmpIr;

    // dummy yts (we check covariances here for which the yts does not matter)
    Handle<YieldTermStructure> yts(
        boost::make_shared<FlatForward>(refDate, 0.02, Actual365Fixed()));

    Array alpha(LENGTH(irTen)), lambda(LENGTH(irTen));

    std::vector<boost::shared_ptr<Parametrization> > parametrizations;

    // IR #1 EUR
    Real H_EUR[] = {0.24842958, 0.49885636, 0.99512038, 3.9210561, 5.8235466,
                    7.6883654,  9.5162582,  11.307956,  13.064176, 14.785621,
                    21.337214,  24.421626,  25.918178,  29.531191, 32.967995,
                    36.237252,  39.346934};
    Real zeta_EUR[] = {9.0701982e-06, 1.0608339e-05, 1.1620875e-05,
                       0.00015177493, 0.00031122807, 0.00046892033,
                       0.00061743761, 0.00075125711, 0.00089906517,
                       0.001101485,   0.0016151376,  0.0018395256,
                       0.0020329435,  0.0026210991,  0.0032963998,
                       0.004125193,   0.0045825323};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_EUR[i] - (i == 0 ? 0.0 : zeta_EUR[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
        lambda[i] = std::sqrt((H_EUR[i] - (i == 0 ? 0.0 : H_EUR[i - 1])) /
                              (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        EURCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #2 GBP (H is shared between all currencies)
    Real zeta_GBP[] = {5.3989367e-06, 9.8331458e-06, 4.6388054e-05,
                       0.00040863507, 0.00062437912, 0.00076368123,
                       0.00083880053, 0.00097274237, 0.0011247902,
                       0.0011807996,  0.0016212442,  0.0016897153,
                       0.0019250142,  0.0023629707,  0.0028444473,
                       0.0033775638,  0.0035846398};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_GBP[i] - (i == 0 ? 0.0 : zeta_GBP[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        GBPCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #3 USD
    Real zeta_USD[] = {2.3553293e-07, 4.7232546e-07, 1.6760723e-06,
                       2.7562061e-05, 7.5069821e-05, 0.00016052372,
                       0.00032273232, 0.00070327448, 0.00082690882,
                       0.0014144869,  0.0019465284,  0.0019707646,
                       0.00219778,    0.0026321698,  0.0030966507,
                       0.0035997538,  0.0037455693};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_USD[i] - (i == 0 ? 0.0 : zeta_USD[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        USDCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #4 JPY
    Real zeta_JPY[] = {2.3553293e-07, 4.7232546e-07, 1.6760723e-06,
                       2.7562061e-05, 7.5069821e-05, 0.00016052372,
                       0.00032273232, 0.00070327448, 0.00082690882,
                       0.0014144869,  0.0019465284,  0.0019707646,
                       0.00219778,    0.0026321698,  0.0030966507,
                       0.0035997538,  0.0037455693};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_JPY[i] - (i == 0 ? 0.0 : zeta_JPY[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        JPYCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #5 AUD
    Real zeta_AUD[] = {7.7367295e-06, 1.3455117e-05, 3.6148337e-05,
                       0.00035739546, 0.0006449375,  0.0010106854,
                       0.0014263234,  0.001839049,   0.0021976553,
                       0.0027602048,  0.0038615771,  0.0038724338,
                       0.0043593179,  0.0054144983,  0.0065917297,
                       0.0079097947,  0.0086340945};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_AUD[i] - (i == 0 ? 0.0 : zeta_AUD[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        AUDCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #6 CAD
    Real zeta_CAD[] = {7.1764671e-06, 1.199943e-05,  3.0336942e-05,
                       0.00023188566, 0.00038850625, 0.00050854554,
                       0.0005611467,  0.00071498642, 0.0008629815,
                       0.00087117906, 0.0010263932,  0.0011534502,
                       0.0013161557,  0.0016666467,  0.0020675357,
                       0.0025277164,  0.00275934};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_CAD[i] - (i == 0 ? 0.0 : zeta_CAD[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        CADCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #7 CHF
    Real zeta_CHF[] = {2.0285111e-06, 1.1611047e-05, 1.4434095e-05,
                       4.6520687e-05, 0.00031520268, 0.00067093245,
                       0.00078748667, 0.0010554702,  0.0011654964,
                       0.0014978801,  0.0018047495,  0.0018047762,
                       0.0019504756,  0.0022601499,  0.0025871501,
                       0.0029619175,  0.0031895455};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_CHF[i] - (i == 0 ? 0.0 : zeta_CHF[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        CHFCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #8 DKK
    Real zeta_DKK[] = {3.95942e-06,   1.6524019e-05, 2.7177507e-05,
                       0.00029766543, 0.00065437464, 0.001221066,
                       0.0017487336,  0.0021895397,  0.0025464983,
                       0.0027541051,  0.0027541403,  0.0028892292,
                       0.0031707705,  0.0037313519,  0.0043215627,
                       0.0049612987,  0.0052460193};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_DKK[i] - (i == 0 ? 0.0 : zeta_DKK[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        DKKCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #9 NOK
    Real zeta_NOK[] = {2.1747207e-05, 4.2144995e-05, 4.2145974e-05,
                       0.00036357391, 0.00054458124, 0.00074627758,
                       0.00081604641, 0.00092208188, 0.0011002273,
                       0.0012189063,  0.0018979681,  0.0019753582,
                       0.0022190637,  0.0027605153,  0.0033563053,
                       0.0040315714,  0.0044332994};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_NOK[i] - (i == 0 ? 0.0 : zeta_NOK[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        NOKCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #10 PLN
    Real zeta_PLN[] = {9.0701982e-06, 1.0608339e-05, 1.1620875e-05,
                       0.00015177493, 0.00031122807, 0.00046892033,
                       0.00061743761, 0.00075125711, 0.00089906517,
                       0.001101485,   0.0016151376,  0.0018395256,
                       0.0020329435,  0.0026210991,  0.0032963998,
                       0.004125193,   0.0045825323};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_PLN[i] - (i == 0 ? 0.0 : zeta_PLN[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        PLNCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #11 SEK
    Real zeta_SEK[] = {6.330515e-06,  7.5521582e-06, 9.9440922e-06,
                       0.00032860183, 0.0005331322,  0.00071660054,
                       0.00086542894, 0.0011098021,  0.0013293011,
                       0.0017246094,  0.0027609916,  0.0027611132,
                       0.0030808796,  0.0038392582,  0.0046789792,
                       0.005628121,   0.0063423051};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_SEK[i] - (i == 0 ? 0.0 : zeta_SEK[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        SEKCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #12 SGD
    Real zeta_SGD[] = {9.0701982e-06, 1.0608339e-05, 1.1620875e-05,
                       0.00015177493, 0.00031122807, 0.00046892033,
                       0.00061743761, 0.00075125711, 0.00089906517,
                       0.001101485,   0.0016151376,  0.0018395256,
                       0.0020329435,  0.0026210991,  0.0032963998,
                       0.004125193,   0.0045825323};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_SGD[i] - (i == 0 ? 0.0 : zeta_SGD[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        SGDCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // IR #13 INR
    Real zeta_INR[] = {9.0701982e-06, 1.0608339e-05, 1.1620875e-05,
                       0.00015177493, 0.00031122807, 0.00046892033,
                       0.00061743761, 0.00075125711, 0.00089906517,
                       0.001101485,   0.0016151376,  0.0018395256,
                       0.0020329435,  0.0026210991,  0.0032963998,
                       0.004125193,   0.0045825323};
    for (Size i = 0; i < LENGTH(irTen); ++i) {
        alpha[i] = std::sqrt((zeta_INR[i] - (i == 0 ? 0.0 : zeta_INR[i - 1])) /
                             (irTimes[i] - (i == 0 ? 0.0 : irTimes[i - 1])));
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        INRCurrency(), yts, irTimes2, alpha, irTimes2, lambda);
    parametrizations.push_back(tmpIr);

    // =========================================
    // Inflation RR (as IR component here)
    // =========================================

    Period inflTen[] = {1 * Years,  2 * Years,  3 * Years,
                        5 * Years,  7 * Years,  12 * Years,
                        15 * Years, 20 * Years, 30 * Years};

    Array inflTimes(LENGTH(inflTen));
    for (Size i = 0; i < LENGTH(inflTen); ++i) {
        inflTimes[i] = Actual365Fixed().yearFraction(
            refDate, TARGET().advance(refDate, inflTen[i]));
    }
    // for parametrization set up (without last time)
    Array inflTimes2(inflTimes.begin(), inflTimes.end() - 1);
    Array alphaInfl(LENGTH(inflTen)), lambdaInfl(LENGTH(inflTen));

    // IR #14 BGL = RR INFL EUR

    Real H_BGL[] = {0.473128, 1.068300, 1.555252, 2.527081, 3.611487,
                    6.076270, 7.369295, 9.429210, 13.319564};
    Real alpha_BGL[] = {0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02};
    for (Size i = 0; i < LENGTH(inflTen); ++i) {
        lambdaInfl[i] =
            std::sqrt((H_BGL[i] - (i == 0 ? 0.0 : H_BGL[i - 1])) /
                      (inflTimes[i] - (i == 0 ? 0.0 : inflTimes[i - 1])));
        alphaInfl[i] = alpha_BGL[i];
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        BGLCurrency(), yts, inflTimes2, alphaInfl, inflTimes2, lambdaInfl);
    parametrizations.push_back(tmpIr);

    // IR #16 BYR = RR INFL UK

    Real H_BYR[] = {1.062214,  2.161263,  3.073939,  4.861583, 6.515747,
                    10.324476, 12.390876, 15.568734, 21.145007};
    Real alpha_BYR[] = {0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02};
    for (Size i = 0; i < LENGTH(inflTen); ++i) {
        lambdaInfl[i] =
            std::sqrt((H_BYR[i] - (i == 0 ? 0.0 : H_BYR[i - 1])) /
                      (inflTimes[i] - (i == 0 ? 0.0 : inflTimes[i - 1])));
        alphaInfl[i] = alpha_BYR[i];
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        BYRCurrency(), yts, inflTimes2, alphaInfl, inflTimes2, lambdaInfl);
    parametrizations.push_back(tmpIr);

    // IR #17 BYR = RR INFL FR

    Real H_CZK[] = {1.024666, 1.290138, 1.655453, 2.250962, 2.843277,
                    3.684875, 3.842543, 4.000118, 4.000213};
    Real alpha_CZK[] = {0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02};
    for (Size i = 0; i < LENGTH(inflTen); ++i) {
        lambdaInfl[i] =
            std::sqrt((H_CZK[i] - (i == 0 ? 0.0 : H_CZK[i - 1])) /
                      (inflTimes[i] - (i == 0 ? 0.0 : inflTimes[i - 1])));
        alphaInfl[i] = alpha_CZK[i];
    }
    tmpIr = boost::make_shared<IrLgm1fPiecewiseLinearParametrization>(
        CZKCurrency(), yts, inflTimes2, alphaInfl, inflTimes2, lambdaInfl);
    parametrizations.push_back(tmpIr);

    // =========================================
    // FX components
    // =========================================

    Period fxTen[] = {3 * Months,  6 * Months,  9 * Months, 12 * Months,
                      15 * Months, 18 * Months, 2 * Years,  3 * Years};

    Array fxTimes(LENGTH(fxTen));
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        fxTimes[i] = Actual365Fixed().yearFraction(
            refDate, TARGET().advance(refDate, fxTen[i]));
    }
    // for parametrization set up (without last time)
    Array fxTimes2(fxTimes.begin(), fxTimes.end() - 1);

    boost::shared_ptr<FxBsParametrization> tmpFx;
    Array sigma(LENGTH(fxTen));

    Handle<Quote> dummyFxSpot(boost::make_shared<SimpleQuote>(1.0));

    // FX #1 GBP

    Real sigma_GBP[] = {0.080781, 0.088930, 0.098602, 0.105432,
                        0.100682, 0.100443, 0.100033, 0.103945};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_GBP[i];
    }
    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        GBPCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #2 USD

    Real sigma_USD[] = {0.100489, 0.100483, 0.101916, 0.100875,
                        0.099272, 0.099088, 0.098720, 0.105264};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_USD[i];
    }
    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        USDCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #3 JPY

    Real sigma_JPY[] = {0.088486, 0.100977, 0.109587, 0.112013,
                        0.115858, 0.115846, 0.115711, 0.122524};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_JPY[i];
    }
    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        JPYCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #4 AUD

    Real sigma_AUD[] = {0.125030, 0.123755, 0.123786, 0.122953,
                        0.123691, 0.123537, 0.123154, 0.121826};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_AUD[i];
    }
    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        AUDCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #5 CAD

    Real sigma_CAD[] = {0.113583, 0.109568, 0.108982, 0.109527,
                        0.110234, 0.110095, 0.109754, 0.108610};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_CAD[i];
    }
    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        CADCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #6 CHF

    Real sigma_CHF[] = {0.066449, 0.074224, 0.080625, 0.083341,
                        0.092719, 0.092715, 0.092488, 0.108220};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_CHF[i];
    }
    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        CHFCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #7 DKK

    Real sigma_DKK[] = {0.012913, 0.013110, 0.012621, 0.015782,
                        0.024053, 0.023408, 0.021574, 0.000000};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_DKK[i];
    }
    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        DKKCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #8 NOK

    Real sigma_NOK[] = {0.099987, 0.099916, 0.099795, 0.099668,
                        0.099532, 0.099321, 0.098811, 0.097166};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_NOK[i];
    }
    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        NOKCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #9 PLN

    Real sigma_PLN[] = {0.065094, 0.069539, 0.072197, 0.073313,
                        0.069963, 0.069777, 0.069391, 0.068027};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_PLN[i];
    }

    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        PLNCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #10 SEK

    Real sigma_SEK[] = {0.068977, 0.078492, 0.082604, 0.085282,
                        0.084029, 0.083851, 0.083398, 0.082871};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_SEK[i];
    }

    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        SEKCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #11 SGD

    Real sigma_SGD[] = {0.149995, 0.149970, 0.149935, 0.149903,
                        0.149861, 0.149791, 0.149611, 0.148984};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_SGD[i];
    }

    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        SGDCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #12 INR

    Real sigma_INR[] = {0.100486, 0.100462, 0.101885, 0.100864,
                        0.099298, 0.099177, 0.098906, 0.105704};
    for (Size i = 0; i < LENGTH(fxTen); ++i) {
        sigma[i] = sigma_INR[i];
    }

    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        INRCurrency(), dummyFxSpot, fxTimes2, sigma);
    parametrizations.push_back(tmpFx);

    // FX #13, 14, 15 Inflation CPI EUR, UK, FR

    Array notimes(0);
    Array sigma_CPI(1, 0.0075);

    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        BGLCurrency(), dummyFxSpot, notimes, sigma_CPI);
    parametrizations.push_back(tmpFx);
    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        BYRCurrency(), dummyFxSpot, notimes, sigma_CPI);
    parametrizations.push_back(tmpFx);
    tmpFx = boost::make_shared<FxBsPiecewiseConstantParametrization>(
        CZKCurrency(), dummyFxSpot, notimes, sigma_CPI);
    parametrizations.push_back(tmpFx);

    // =========================================
    // time grid for RFE
    // =========================================

    std::vector<Time> simTimes_;
    simTimes_.push_back(0.0);
    for (Size i = 1; i <= 118; ++i) {
        Date tmp = TARGET().advance(refDate, i * Months);
        simTimes_.push_back(Actual365Fixed().yearFraction(refDate, tmp));
    }
    for (Size i = 1; i <= 40; ++i) {
        Date tmp = TARGET().advance(refDate, (117 + 3 * i) * Months);
        simTimes_.push_back(Actual365Fixed().yearFraction(refDate, tmp));
    }
    for (Size i = 1; i <= 31; ++i) {
        Date tmp = TARGET().advance(refDate, (19 + i) * Years);
        simTimes_.push_back(Actual365Fixed().yearFraction(refDate, tmp));
    }
    for (Size i = 1; i <= 10; ++i) {
        Date tmp = TARGET().advance(refDate, (50 + i * 5) * Years);
        simTimes_.push_back(Actual365Fixed().yearFraction(refDate, tmp));
    }

    // =========================================
    // XAsset model
    // =========================================

    boost::shared_ptr<XAssetModel> xmodel = boost::make_shared<XAssetModel>(
        parametrizations, rho, SalvagingAlgorithm::None);

    boost::shared_ptr<StochasticProcess> p_exact =
        xmodel->stateProcess(XAssetStateProcess::exact);
    boost::shared_ptr<StochasticProcess> p_euler =
        xmodel->stateProcess(XAssetStateProcess::euler);

    // check that covariance matrices are positive semidefinite

    Array x0 = p_exact->initialValues();
    for (Size i = 1; i < simTimes_.size(); ++i) {
        // x0 does not matter, since covariance does not depend on it
        Matrix cov = p_exact->covariance(simTimes_[i - 1], x0,
                                         simTimes_[i] - simTimes_[i - 1]);
        SymmetricSchurDecomposition ssd(cov);
        for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
            if (ssd.eigenvalues()[i] < 0.0) {
                BOOST_ERROR("negative eigenvalue at "
                            << i
                            << " in covariance matrix at t=" << simTimes_[i]
                            << " (" << ssd.eigenvalues()[i] << ")");
            }
        }
        // if(simTimes_[i-1]>10.0 && simTimes_[i-1]<11.0) {
        // std::clog << "covariance(" << simTimes_[i - 1] << "," << simTimes_[i]
        //           << "): (eigenvalues=" << ssd.eigenvalues().front() << " ... "
        //           << ssd.eigenvalues().back() << std::endl;
        //     for(Size i=0;i<31;++i) {
        //         std::clog << "| ";
        //         for(Size j=0;j<31;++j) {
        //             std::clog << cov[invMapping[i]][invMapping[j]] << " ";
        //         }
        //         std::clog << " |" << std::endl;
        //     }
        //     std::clog << std::endl;
        // }
    }

    // one super-large step
    Matrix cov = p_exact->covariance(0.0, x0, 50.0/*simTimes_.back()*/);
    SymmetricSchurDecomposition ssd2(cov);
    // std::clog << "covariance(" << 0.0 << "," << simTimes_.back()
    //           << "): (eigenvalues=" << ssd2.eigenvalues().front() << " ... "
    //           << ssd2.eigenvalues().back() << ")" << std::endl;
    for (Size i = 0; i < ssd2.eigenvalues().size(); ++i) {
        if (ssd2.eigenvalues()[i] < 0.0) {
            BOOST_ERROR("negative eigenvalue at "
                        << i << " in covariance matrix at t=0.0 for dt=100.0"
                        << " (" << ssd2.eigenvalues()[i] << ")");
        }
    }

} // testFMSCase

test_suite *XAssetModelTest2::suite() {
    test_suite *suite = BOOST_TEST_SUITE("XAsset model tests 2");
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest2::testFMSCase));
    return suite;
}
