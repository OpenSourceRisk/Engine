/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <ored/scripting/models/heston.hpp>
#include <sstream>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

void Heston::performModelCalculations() const {
    if (type_ == Model::Type::MC)
        performCalculationsMc();
    else if (type_ == Model::Type::FD)
        performCalculationsFd();
}

Real Heston::initialValue(const Size indexNo) const {
    return model_->hestonProcesses()[indexNo]->s0()->value();
}

Real Heston::atmForward(const Size indexNo, const Real t) const {
    return ore::data::atmForward(model_->hestonProcesses()[indexNo]->s0()->value(),
                                 model_->hestonProcesses()[indexNo]->riskFreeRate(),
                                 model_->hestonProcesses()[indexNo]->dividendYield(), t);
}

Real Heston::compoundingFactor(const Size indexNo, const Date& d1, const Date& d2) const {
    const auto& p = model_->hestonProcesses().at(indexNo);
    return p->dividendYield()->discount(d1) / p->dividendYield()->discount(d2) /
           (p->riskFreeRate()->discount(d1) / p->riskFreeRate()->discount(d2));
}

void Heston::performCalculationsMc() const {
    initUnderlyingPathsMc();
    setReferenceDateValuesMc();
    if (effectiveSimulationDates_.size() == 1)
        return;
    generatePaths();
}

void Heston::performCalculationsFd() const {
    // TODO, see localvol.cpp for a template
    QL_FAIL("Heston::performCalculationsFd() not implemented");
}

void Heston::generatePaths() const {

    // Single asset, no training paths, no foreign currency index drift adjustment
    // if (indices_.size() == 1) 
    //     return generateSingleAssetPaths();

    if (indices_.size() == 1) {
        DLOG("Heston::generatePaths() called for single index:");
        DLOG(indices_.front());
    } else {
        DLOG("Heston::generatePaths() called for " << indices_.size() << " indices:");
        for (auto i : indices_) {
            DLOG(i);
	}
    }

    Matrix C = getCorrelation();

    /*
       Expand the spot-spot correlation matrix following
          Dimitroff, Lorenz, Szimayer: A Parsimonious Multi-Asset Heston Model,
          https://ssrn.com/abstract=1435199, Eqn (16)
       to twice the size, including the variance factors, i.e. including
       - spot-variance correlation within each Heston "sub-system",
       - spot-variance correlation across different sub-systems
       - variance-variance correlations across different sub-systems
       with block matrices C_i along the diagonal and C_ij off diagonal
       
       Eqn (16):
       
             (1,    r_i )                  ( 1,     r_j       )
       C_i = (          )    C_ij = r_ij * (                  )
             (r_i,  1   )                  ( r_i,   r_i * r_j )
    */

    Size n = indices_.size();
    Matrix correlation(2 * n, 2 * n, 0.0);
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < n; ++j) {
            Real ri = model_->hestonProcesses()[i]->rho();
            if (i == j) {
                correlation[2 * i][2 * i] = 1.0;
                correlation[2 * i][2 * i + 1] = ri;
                correlation[2 * i + 1][2 * i] = ri;
                correlation[2 * i + 1][2 * i + 1] = 1.0;
            } else {
	        Real rij = C[i][j];
		Real rj = model_->hestonProcesses()[j]->rho();
                correlation[2 * i][2 * j] = rij;
                correlation[2 * i][2 * j + 1] = rij * rj;
                correlation[2 * i + 1][2 * j] = rij * ri;
                correlation[2 * i + 1][2 * j + 1] = rij * ri * rj;
            }
	    DLOG("C("<< i << "," << j<< ") = " << C[i][j]);
        }
    }

    for (Size i = 0; i < 2*n; ++i) {
        for (Size j = 0; j < 2*n; ++j) {
	    DLOG("corr("<< i << "," << j<< ") = " << correlation[i][j]);
	}
    }

    Matrix sqrtCorr = pseudoSqrt(correlation, params_.salvagingAlgorithm);

    Matrix sqrtCorrSquared = sqrtCorr * transpose(sqrtCorr);
    for (Size i = 0; i < 2 * n; ++i) {
        for (Size j = 0; j < 2 * n; ++j) {
            if (fabs(correlation[i][j] - correlation[j][i]) > 1e-9) {
                ALOG("correlation matrix not symmetric for i=" << i << ", j=" << j << ": " << std::setprecision(6)
                                                               << correlation[i][j] << " vs " << correlation[j][i]);
            }
            if (fabs(correlation[i][j] - sqrtCorrSquared[i][j]) > 1e-6) {
                ALOG("correlation matrix square root problem for i=" << i << ", j=" << j << ": " << std::setprecision(6)
                                                                     << correlation[i][j] << " vs "
                                                                     << sqrtCorrSquared[i][j]);
            }
        }
    }

    // Precompute index for drift adjustment for eq / com indices that are not in base ccy
    std::vector<Size> eqComIdx(indices_.size());
    for (Size j = 0; j < indices_.size(); ++j) {
        Size idx = Null<Size>();
        if (!indices_[j].isFx()) {
            // do we have an fx index with the desired currency?
            for (Size jj = 0; jj < indices_.size(); ++jj) {
                if (indices_[jj].isFx()) {
                    if (indexCurrencies_[jj] == indexCurrencies_[j])
                        idx = jj;
                }
            }
        }
        eqComIdx[j] = idx;
    }
    
    populatePathValues(size(), underlyingPaths_,
                       makeMultiPathVariateGenerator(params_.sequenceType, 2 * n, timeGrid_.size() - 1, params_.seed,
                                                     params_.sobolOrdering, params_.sobolDirectionIntegers),
                       correlation, sqrtCorr, eqComIdx);

    if (trainingSamples() != Null<Size>()) {
        populatePathValues(trainingSamples(), underlyingPathsTraining_,
                           makeMultiPathVariateGenerator(params_.trainingSequenceType, 2 * n, timeGrid_.size() - 1,
                                                         params_.trainingSeed, params_.sobolOrdering,
                                                         params_.sobolDirectionIntegers),
                           correlation, sqrtCorr, eqComIdx);
    }

    setAdditionalResults();

    DLOG("Heston::generatePaths() done");
}

void Heston::generateSingleAssetPaths() const {
    DLOG("Heston::generateSingleAssetPaths() called");

    QL_REQUIRE(indices_.size() == 1, "only a single index is covered so far");
    QL_REQUIRE(model_->hestonProcesses().size() == 1, "only a single heston process is covered so far");

    auto process = model_->hestonProcesses().front();

    // precompute index for drift adjustment for eq / com indices that are not in base ccy
    std::vector<Size> eqComIdx(indices_.size());
    for (Size j = 0; j < indices_.size(); ++j) {
        Size idx = Null<Size>();
        if (!indices_[j].isFx()) {
            // do we have an fx index with the desired currency?
            for (Size jj = 0; jj < indices_.size(); ++jj) {
                if (indices_[jj].isFx()) {
                    if (indexCurrencies_[jj] == indexCurrencies_[j])
                        idx = jj;
                }
            }
        }
        eqComIdx[j] = idx;
    }
    
    std::vector<std::vector<RandomVariable*>> rvs(indices_.size(),
                                                  std::vector<RandomVariable*>(effectiveSimulationDates_.size() - 1));
    auto date = effectiveSimulationDates_.begin();
    for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
        ++date;
        for (Size j = 0; j < indices_.size(); ++j) {
            rvs[j][i] = &underlyingPaths_[*date][j];
            rvs[j][i]->expand();
        }
    }

    // single asset: dimension = 2
    auto gen = makeMultiPathVariateGenerator(params_.sequenceType, indices_.size() * 2,
					     timeGrid_.size() - 1, params_.seed,
					     params_.sobolOrdering, params_.sobolDirectionIntegers);

    // single asset
    Array state(2), state0(2);
    state0[0] = model_->hestonProcesses()[0]->s0()->value();
    state0[1] = model_->hestonProcesses()[0]->v0();
    Array dW(2);

    for (Size path = 0; path < size(); ++path) {
        auto p = gen->next();
        state = state0;
        std::size_t date = 0;
        auto pos = positionInTimeGrid_.begin();
        ++pos;

	// evolve the process on the refined time grid
        for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
            Real t0 = timeGrid_[i];
            Real dt = timeGrid_[i+1] - t0;

	    // 2-d array of independent Wiener increments
            dW = p.value[i];

            // Heston process in QL expects independent increments dW and them into correlated increments
            state = process->evolve(t0, state, dt, dW);

	    // TODO: drift adjustment for foreign cururrency indices

            // on the effective simulation dates populate the underlying paths
            if (i + 1 == *pos) {
                rvs[0][date]->data()[path] = state[0];
                ++date;
                ++pos;
            }
        }
    }

    DLOG("Heston::generateSingleAssetPaths() done");
}

void Heston::populatePathValues(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                                const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                                const Matrix& correlation, const Matrix& sqrtCorr,
                                const std::vector<Size>& eqComIdx) const {
    
    std::vector<std::vector<RandomVariable*>> rvs(indices_.size(),
                                                  std::vector<RandomVariable*>(effectiveSimulationDates_.size() - 1));
    auto date = effectiveSimulationDates_.begin();
    for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
        ++date;
        for (Size j = 0; j < indices_.size(); ++j) {
            rvs[j][i] = &paths[*date][j];
            rvs[j][i]->expand();
        }
    }

    // precompute initial state arrays and decorrelation matrices 
    std::vector<Array> state(indices_.size(), Array(2));
    std::vector<Array> state0(indices_.size(), Array(2));
    std::vector<Matrix> Qinv(indices_.size(), Matrix(2,2));
    Array dw(2);
    for (Size j = 0; j < indices_.size(); ++j) {
        state0[j][0] = model_->hestonProcesses()[j]->s0()->value();
        state0[j][1] = model_->hestonProcesses()[j]->v0();

	// QuantLib's Heston::evolve uses dW(correlated) = Q * dW(independent)
	// with matrix Q = [ [ 1, 0 ], [ sqrt(1 - r^2), r ] ]
	// to convert independent variates into correlated variates.
	Real r = model_->hestonProcesses()[j]->rho();
	Matrix Q(2, 2, 0.0);
	Q[0][0] = 1.0; 
	Q[0][1] = 0.0;
	Q[1][0] = r;
	Q[1][1] = std::sqrt(1.0 - r * r);

	// Since we generate correlated variates for the multi-asset Heston model
	// we need to decorrelate each Heston subsystem before passing variates
	// into the Heston::evolve method, i.e. using the inverse of Q,
	//    Qinv = [ [ 1, 0 ], [ sqrt(1 - r^2)/r,  1/r ] ]
	//    dW(independent) = Qinv * dW(correlated)
	// preservng the spot process variate.

        // analytical inversion
        Qinv[j][0][0] = 1.0;
        Qinv[j][0][1] = 0.0;
        Qinv[j][1][0] = -r / std::sqrt(1.0 - r * r);
        Qinv[j][1][1] = 1.0 / std::sqrt(1.0 - r * r);

        // // double check with numerical inversion
        // Matrix Qi = inverse(Q);

        // for (Size ii = 0; ii < 2; ++ii) {
        //     for (Size jj = 0; jj < 2; ++jj) {
        //         QL_REQUIRE(fabs(Qi[ii][jj] - Qinv[j][ii][jj]) < 1e-6, "error inverting matrix Q");
        //     }
        // }

        // std::cout << "Qi(" << j << "):" << std::endl << Qi << std::endl;
        // std::cout << "Qinv(" << j << "):" << std::endl << Qinv[j] << std::endl;
    }

    // Empirical covariances at each time step
    std::vector<Matrix> cov(timeGrid_.size() - 1, Matrix(correlation.rows(), correlation.columns()));

    // Empirical Heston correlation for each index at each time grid point, before and after decorrelation
    std::vector<std::vector<Real>> cov1(indices_.size(), std::vector<Real>(timeGrid_.size() - 1, 0.0));
    std::vector<std::vector<Real>> cov2(indices_.size(), std::vector<Real>(timeGrid_.size() - 1, 0.0));

    // Cache variance processes along entire path to check proximity to zero, effect of a relaxed Feller constraint
    std::vector<std::vector<Real>> varianceState(indices_.size(), std::vector<Real>(timeGrid_.size() - 1, 0.0));

    Size countStuck = 0;
    for (Size path = 0; path < size(); ++path) {
        auto p = gen->next();
        state = state0;
        std::size_t date = 0;
        auto pos = positionInTimeGrid_.begin();
        ++pos;

	// Evolve the process on the refined time grid
        for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
            Real t0 = timeGrid_[i];
            Real dt = timeGrid_[i+1] - t0;

	    // Array of independent increments, size 2*n

	    Array dWind = p.value[i];
	    
	    // Array of correlated increments, size 2*n
	    // This is necessary to preserve the desired spot-spot correlations, 
	    // includes the calibrated spot-varianace correlations within each Heston sub-system,
	    // and includes spot-variance and variance-variance correlations across sub-systems,
	    // constructed with the partial independence assumption
	    
	    Array dWcor = sqrtCorr * p.value[i];

            if (debug_) {
                for (Size j1 = 0; j1 < dWcor.size(); j1++) {
                    for (Size j2 = 0; j2 < dWcor.size(); j2++)
                        cov[i][j1][j2] += dWcor[j1] * dWcor[j2];
                }
            }

            // Evolve each index, pick relevant part of dWcor
            for (Size j = 0; j < indices_.size(); ++j) {

	        auto process = model_->hestonProcesses()[j];
	      
	        // 2-d array of correlated Wiener increments for the Heston subsystem j	      
	        dw[0] = dWcor[2*j];
	        dw[1] = dWcor[2*j+1];
		if (debug_)
		    cov1[j][i] += dw[0] * dw[1];
		
	        // Decorrelate the subsystem, 2-d array of independent Wiener increments
		Array dw_decorrelated = Qinv[j] * dw;
		if (debug_)
		    cov2[j][i] += dw_decorrelated[0] * dw_decorrelated[1];
		
		// The Heston process in QL expects the latter independent increments as input
		// in order to apply the correlated increments dw internally.
		// Alternatively we could have modified Heston::evolve to also accept correlated
		// input, but that works only with some of the simple Heston discretization schemes
		// and not QE. To use the Heston implementation as is we take the simple
		// decorrelation step.
		state[j] = process->evolve(t0, state[j], dt, dw_decorrelated);
		
		// Keep track of the variance states to analyse below
		varianceState[j][i] = state[j][1];
	    }

	    // FIXME: Validate Heston drift adjustment(s) with a Martingale test
	    // Loop over all indices again and add drift for eq / com indices that are not in base ccy
            for (Size j = 0; j < indices_.size(); ++j) {
		Size j0 = eqComIdx[j];
                if (j0 != Null<Size>()) {
		    // Spot process adjustment
                    Real volIdx = std::sqrt(state[j0][1]);
                    Real volj = std::sqrt(state[j][1]);
                    Real sDrift = - correlation[2*j0][2*j] * volIdx * volj * dt; 
		    state[j][0] *= std::exp(sDrift);
		    // Variance process adjustment
		    Real sigma = model_->hestonProcesses()[j]->sigma();
		    Real vDrift = - correlation[2*j0][2*j+1] * sigma * volIdx * volj * dt; 
		    // Avoid measure drift to push the variance across the boundary.
		    // This might happen especially when we allow Feller < 1.
		    // FIXME ?
		    if (state[j][1] + vDrift > 0)
		        state[j][1] += vDrift; 
		}
            }

            // on the effective simulation dates populate the underlying paths
            if (i + 1 == *pos) {
	        for (Size j = 0; j < indices_.size(); ++j)
		    rvs[j][date]->data()[path] = state[j][0];
                ++date;
                ++pos;
            }
	}

        // Check variance process
        if (debug_) {
            Real tol = 1e-8; // FIXME
            int iMax = int(timeGrid_.size()) - 2;
            for (Size j = 0; j < indices_.size(); ++j) {
                Size iStuck = timeGrid_.size();
                for (int i = iMax - 1; i >= 0; --i) {
                    if (varianceState[j][i] < tol && varianceState[j][i + 1] < tol)
                        iStuck = i;
                }
                if (iStuck <= iMax - 1) {
		    countStuck++;
		    // ALOG("Heston path " << path << " for index " << indices_[j].name()
                    //                     << " seems to get stuck at time step " << iStuck << "/" << iMax - 1);
                }
            }
        }
    }

    if (debug_) {
        if (countStuck > 0) {
            ALOG("Variance process stuck across " << size() * indices_.size() << " paths: " << countStuck);
        }

        // The accuracy of the empirical correlation depends on number of samples and sampling method.
        // So use a generous tolerance here.
        Real tol = 5.0 / std::sqrt(size());
        for (Size j = 0; j < indices_.size(); ++j) {
            Real r = model_->hestonProcesses()[j]->rho();
            for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
                // We assume unit variances, skip their check
                Real r1 = cov1[j][i] / size();
                Real r2 = cov2[j][i] / size();
                QL_REQUIRE(fabs(r1 - r) < tol, "empirical correlation " << r1 << " does not match calibrated " << r);
                QL_REQUIRE(fabs(r2) < tol, "correlation " << r2 << " should be zero");
            }
        }

        for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
            for (Size j1 = 0; j1 < correlation.rows(); j1++) {
                for (Size j2 = 0; j2 < correlation.columns(); j2++) {
                    Real c = cov[i][j1][j2] / size();
                    DLOG("empiricalCorrelation[" << i << "][" << j1 << "][" << j2 << "] = " << std::setprecision(4)
                                                 << std::fixed << std::showpos << c);
                    QL_REQUIRE(fabs(c - correlation[j1][j2]) < tol,
                               "empirical correlation " << c << " does not match expected " << correlation[j1][j2]);
                }
            }
        }
    }
}

void Heston::setAdditionalResults() const {

    Matrix correlation = getCorrelation();
    
    for (Size i = 0; i < indices_.size(); ++i) {
        for (Size j = 0; j < i; ++j) {
            additionalResults_["Heston.Correlation_" + indices_[i].name() + "_" + indices_[j].name()] =
                correlation(i, j);
        }
    }

    std::vector<Real> calibrationStrikes = getCalibrationStrikes();

    for (Size i = 0; i < calibrationStrikes.size(); ++i) {
        additionalResults_["Heston.CalibrationStrike_" + indices_[i].name()] =
            (calibrationStrikes[i] == Null<Real>() ? "ATMF" : std::to_string(calibrationStrikes[i]));
    }

    for (Size i = 0; i < currencies_.size(); ++i) {
        if (i > 0) {
	    std::ostringstream o;
            o << "fxSpot." << currencies_[i];
            additionalResults_[o.str()] = fxSpots_[i - 1]->value();
        }
        for (auto const& d : effectiveSimulationDates_) {
	    std::ostringstream o;
            o << "discount." << currencies_[i] << "." << io::iso_date(d);
            additionalResults_[o.str()] = curves_[i]->discount(d);
        }
    }

    for (Size i = 0; i < indices_.size(); ++i) {
        Size timeStep = 0;
        for (auto const& d : effectiveSimulationDates_) {
            Real t = timeGrid_[positionInTimeGrid_[timeStep]];
            Real forward = atmForward(i, t);
            additionalResults_["Heston.Forward_" + indices_[i].name() + "_" + ore::data::to_string(d)] = forward;
            ++timeStep;
        }
    }

    for (Size i = 0; i < indices_.size(); ++i) {
        additionalResults_["Heston.theta_" + indices_[i].name()] = model_->hestonProcesses()[i]->theta();
        additionalResults_["Heston.kappa_" + indices_[i].name()] = model_->hestonProcesses()[i]->kappa();
        additionalResults_["Heston.sigma_" + indices_[i].name()] = model_->hestonProcesses()[i]->sigma();
        additionalResults_["Heston.rho_" + indices_[i].name()] = model_->hestonProcesses()[i]->rho();
        additionalResults_["Heston.v0_" + indices_[i].name()] = model_->hestonProcesses()[i]->v0();
    }
    
    for (Size i = 0; i < indices_.size(); ++i)
        additionalResults_["Heston.Index[" + to_string(i) + "]"] = indices_[i].name();

    if (model_->calibration().size() > 0)
        additionalResults_["Heston.calibration"] = model_->calibration();

    if (debug_) {
        // copy path data
        MultiAssetHestonPaths paths;
        paths.samples = size();
        for (auto i : indices_)
            paths.indexNames.push_back(i.name());
        for (auto d : effectiveSimulationDates_) {
            paths.dates.push_back(d);
            paths.data[d] = std::vector<std::vector<Real>>(indices_.size(), std::vector<Real>(size(), 0.0));
            for (Size i = 0; i < indices_.size(); ++i) {
                for (Size j = 0; j < size(); ++j) {
                    paths.data[d][i][j] = underlyingPaths_[d][i][j];
                }
            }
        }
        additionalResults_["Heston.paths"] = paths;
    }
}

} // namespace data
} // namespace ore
