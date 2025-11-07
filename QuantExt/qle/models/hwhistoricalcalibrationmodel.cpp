/*
  Copyright (C) 2021 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <qle/models/hwhistoricalcalibrationmodel.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/make_shared.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <math.h>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/math/optimization/costfunction.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/haltonrsg.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>

#include <iostream>

using namespace QuantLib;

namespace QuantExt {

HwHistoricalCalibrationModel::HwHistoricalCalibrationModel(
    const Date& asOfDate, const std::vector<Period>& curveTenor, const Real& lambda, const bool& useForwardRate,
    const std::map<std::string, std::map<Date, std::vector<Real>>>& dataIR,
    const std::map<std::string, std::map<Date, Real>>& dataFX)
    : asOfDate_(asOfDate), curveTenor_(curveTenor), lambda_(lambda), useForwardRate_(useForwardRate), dataIR_(dataIR),
      dataFX_(dataFX) {}

HwHistoricalCalibrationModel::HwHistoricalCalibrationModel(const Date& asOfDate, const std::vector<Period>& curveTenor,
                                                           const bool& useForwardRate,
                                                           const std::map<std::string, Size>& principalComponent,
                                                           const std::map<std::string, Array>& eigenValue,
                                                           const std::map<std::string, Matrix>& eigenVector)
    : asOfDate_(asOfDate), curveTenor_(curveTenor), useForwardRate_(useForwardRate),
      principalComponent_(principalComponent), eigenValue_(eigenValue), eigenVector_(eigenVector) {}

void HwHistoricalCalibrationModel::pcaCalibration(const Real& varianceRetained) {
    computeFxLogReturn();
    computeIrAbsoluteReturn();
    pca(varianceRetained);
    computeCorrelation();
}

void HwHistoricalCalibrationModel::computeFxLogReturn() {
    for (auto const& outer : dataFX_) {
        bool firstDate = true;
        Real prev;
        Array logReturn(outer.second.size() - 1, 0.0);
        Size i = 0;
        for (auto const& inner : outer.second) {
            if (firstDate) {
                prev = inner.second;
                firstDate = false;
            } else {
                logReturn[i] = log(inner.second / prev);
                prev = inner.second;
                ++i;
            }
        }

        // Subtract mean
        Real avg = 0.0;
        for (Size i = 0; i < logReturn.size(); ++i) {
            avg += logReturn[i];
        }
        avg /= static_cast<double>(logReturn.size());
        logReturn = logReturn - avg;

        // Add exponential-weight to return matrix
        Array ewma(logReturn.size(), 0.0);
        Real ewmaSum = 0.0;
        Real logReturnSum = 0.0;
        for (Size i = 0; i < ewma.size(); ++i) {
            ewma[i] = std::pow(lambda_, 0.5 * (ewma.size() - i - 1));
            ewmaSum += ewma[i] * ewma[i];
        }
        for (Size i = 0; i < logReturn.size(); ++i) {
            logReturn[i] *= ewma[i];
            logReturnSum += logReturn[i] * logReturn[i];
        }
        Real variance = 252 * logReturnSum / ewmaSum;
        fxLogReturn_[outer.first] = logReturn;
        fxVariance_[outer.first] = variance;
        fxSigma_[outer.first] = sqrt(variance);
    }
}

void HwHistoricalCalibrationModel::computeIrAbsoluteReturn() {
    Size datesCount = dataIR_.begin()->second.size();
    Real diffTenor;
    std::vector<Real> prev, next;
    for (auto const& outer : dataIR_) {

        bool firstDate = true;
        Matrix irAbsoluteReturn(datesCount - 1, curveTenor_.size(), 0.0);
        Size i = 0;

        for (auto const& inner : outer.second) {
            // Compute zero rate or forward rate
            std::vector<Real> v;
            if (useForwardRate_) {
                diffTenor = Actual365Fixed().yearFraction(inner.first, inner.first + curveTenor_[0]);
                Real tenorSum = diffTenor;
                v.push_back(-log(inner.second[0]) / diffTenor);
                for (Size i = 1; i < curveTenor_.size(); ++i) {
                    diffTenor = Actual365Fixed().yearFraction(inner.first, inner.first + curveTenor_[i]) - tenorSum;
                    tenorSum = tenorSum + diffTenor;
                    v.push_back(-log(inner.second[i] / inner.second[i - 1]) / diffTenor);
                }
            } else {
                for (Size i = 0; i < curveTenor_.size(); ++i) {
                    diffTenor = Actual365Fixed().yearFraction(inner.first, inner.first + curveTenor_[i]);
                    v.push_back(-log(inner.second[i]) / diffTenor);
                }
            }

            // compute absolute return
            if (firstDate) {
                prev = v;
                firstDate = false;
            } else {
                next = v;
                std::transform(next.begin(), next.end(), prev.begin(), next.begin(), std::minus<Real>());
                for (Size j = 0; j < next.size(); ++j) {
                    /*if (std::abs(next[j]) > returnThreshold_) {
                        WLOG("Absolute return " << next[j] << " at date " << outer.first << " tenor " << curveTenor_[j]
                                                << " above threshold.")
                    }*/
                    irAbsoluteReturn[i][j] = next[j];
                }
                i++;
                prev = v;
            }
        }

        // Subtract mean
        for (Size j = 0; j < curveTenor_.size(); ++j) {
            Real avg = 0.0;
            for (Size i = 0; i < irAbsoluteReturn.rows(); ++i) {
                avg += irAbsoluteReturn[i][j];
            }
            avg /= static_cast<double>(irAbsoluteReturn.rows());
            /*if (std::abs(avg) > averageThreshold_) {
                ALOG("Average " << avg << " for tenor " << curveTenor_[j] << " above threshold");
            }*/
            for (Size i = 0; i < irAbsoluteReturn.rows(); ++i) {
                irAbsoluteReturn[i][j] -= avg;
            }
        }

        // Add exponential-weight to return matrix
        Array ewma(datesCount - 1);
        Real ewmaSum = 0;
        for (Size i = 0; i < datesCount - 1; ++i) {
            ewma[i] = std::pow(lambda_, 0.5 * (datesCount - i - 2));
            ewmaSum += ewma[i] * ewma[i];
        }
        for (Size i = 0; i < irAbsoluteReturn.rows(); ++i) {
            for (Size j = 0; j < irAbsoluteReturn.columns(); ++j) {
                irAbsoluteReturn[i][j] *= ewma[i];
            }
        }
        irCovariance_[outer.first] = 252 * transpose(irAbsoluteReturn) * irAbsoluteReturn / ewmaSum;
        irAbsoluteReturn_[outer.first] = irAbsoluteReturn;
    }
}

void HwHistoricalCalibrationModel::pca(const Real& varianceRetained) {
    for (auto const& ccyMatrix : irCovariance_) {
        //LOG("Perform PCA for currency: " << ccyMatrix.first);
        SymmetricSchurDecomposition decomp(ccyMatrix.second);
        eigenValue_[ccyMatrix.first] = decomp.eigenvalues();
        eigenVector_[ccyMatrix.first] = decomp.eigenvectors();
        Real sum = 0;
        Size pc = 0;
        Real variance = 0;
        for (Size i = 0; i < decomp.eigenvalues().size(); ++i) {
            //LOG("EigenValues[" << i << "] = " << std::scientific << decomp.eigenvalues()[i]);
            sum += decomp.eigenvalues()[i];
        }
        while (variance / sum < varianceRetained) {
            variance += decomp.eigenvalues()[pc];
            ++pc;
        }
        principalComponent_[ccyMatrix.first] = pc;
        //LOG("PrincipalComponent: " << principalComponent_.find(ccyMatrix.first)->second);
        //LOG("Variance Retained: " << std::fixed << variance / sum);

        // transfrom absolute return matrix into pca eigenvector adjusted
        Matrix adjMatrix(decomp.eigenvalues().size(), pc);
        for (Size i = 0; i < decomp.eigenvalues().size(); ++i) {
            for (Size j = 0; j < pc; ++j) {
                adjMatrix[i][j] = decomp.eigenvectors()[i][j];
            }
        }
        irAbsoluteReturnAdjusted_[ccyMatrix.first] = irAbsoluteReturn_.find(ccyMatrix.first)->second * adjMatrix;
    }
}

struct StatModelTargetFunction : public CostFunction {
    StatModelTargetFunction(const Array& eigenVector, const std::vector<Real>& curveTenorReal,
                            const Size basisFunctionNumber, const Real kappaUpperBound, const bool useForwardRate)
        : eigenVector_(eigenVector), curveTenorReal_(curveTenorReal), basisFunctionNumber_(basisFunctionNumber),
          kappaUpperBound_(kappaUpperBound), useForwardRate_(useForwardRate) {}

    Array values(const Array& x) const override {
        Array res(curveTenorReal_.size());
        for (Size i = 0; i < curveTenorReal_.size(); ++i) {
            Real t = curveTenorReal_[i];
            Real sum = 0.0;
            for (Size j = 0; j < basisFunctionNumber_; ++j) {
                Real v = x[j];
                Real kappa;
                if (close_enough(kappaUpperBound_, 0.0))
                    kappa = x[basisFunctionNumber_ + j];
                else
                    kappa = kappaUpperBound_ / (std::acos(-1) / 2) * std::atan(x[basisFunctionNumber_ + j]);
                Real t0 = i == 0 || !useForwardRate_ ? 0.0 : curveTenorReal_[i - 1];
                if (std::abs(kappa) < 1E-6) {
                    sum += v;
                } else {
                    sum += v / (t - t0) / kappa * (std::exp(-kappa * t0) - std::exp(-kappa * t));
                }
            }
            res[i] = sum - eigenVector_[i];
        }
        return res;
    }

    const Array& eigenVector_;
    const std::vector<Real>& curveTenorReal_;
    const Size basisFunctionNumber_;
    const Real kappaUpperBound_;
    const bool useForwardRate_;
};

void HwHistoricalCalibrationModel::meanReversionCalibration(const Size& basisFunctionNumber, const Real& kappaUpperBound,
                                       const Size& maxGuess) {
    QL_REQUIRE(maxGuess >= 0, "Max guess for Halton Sequence should be a positive integer.");
    basisFunctionNumber_ = basisFunctionNumber;
    for (Size i = 0; i < curveTenor_.size(); ++i) {
        curveTenorReal_.push_back(Actual365Fixed().yearFraction(asOfDate_, asOfDate_ + curveTenor_[i]));
    }
    for (auto const& ccyMatrix : eigenVector_) {
        //LOG("Perform mean reversion calibration for currency " << ccyMatrix.first);
        Size principalComponent = principalComponent_.find(ccyMatrix.first)->second;
        Matrix v(principalComponent, basisFunctionNumber_);
        Matrix kappa(principalComponent, basisFunctionNumber_);
        // run optimization for each principal component
        for (Size i = 0; i < principalComponent; ++i) {
            Array eigenVector(ccyMatrix.second.rows());
            for (Size j = 0; j < ccyMatrix.second.rows(); j++) {
                eigenVector[j] = ccyMatrix.second[j][i];
            }
            NoConstraint noConstraint;
            LevenbergMarquardt lm;
            EndCriteria endCriteria(100, 10, 1e-8, 1e-8, 1e-8);
            Array guess(2 * basisFunctionNumber_);
            Real bestFunctionValue = QL_MAX_REAL;
            Array solution;
            HaltonRsg halton(2 * basisFunctionNumber_, 42);
            for (Size nGuess = 0; nGuess < maxGuess; ++nGuess) {
                auto seq = halton.nextSequence();
                for (Size j = 0; j < basisFunctionNumber_; ++j) {
                    guess[j] = seq.value[j] * 10.0 - 5.0;
                    guess[basisFunctionNumber_ + j] = seq.value[j] * 1.0 - 0.50;
                }
                StatModelTargetFunction targetFunction(eigenVector, curveTenorReal_, basisFunctionNumber_,
                                                       kappaUpperBound, useForwardRate_);
                Problem problem(targetFunction, noConstraint, guess);
                lm.minimize(problem, endCriteria);

                if (problem.functionValue() < bestFunctionValue) {
                    solution = problem.currentValue();
                    bestFunctionValue = problem.functionValue();
                    //LOG("Current best solution is " << problem.currentValue() << ", cost function = "
                    //                                << bestFunctionValue << " at trial " << nGuess);
                }
            }
            for (Size j = 0; j < basisFunctionNumber_; ++j) {
                v[i][j] = solution[j];
                if (close_enough(kappaUpperBound, 0.0))
                    kappa[i][j] = solution[basisFunctionNumber_ + j];
                else
                    kappa[i][j] = kappaUpperBound / (std::acos(-1) / 2) * std::atan(solution[basisFunctionNumber_ + j]);
            }

            //LOG("Solution for Principal Component " << i << " is " << solution);
            //LOG("time,pc,model,diff");
            for (Size k = 0; k < curveTenorReal_.size(); ++k) {
                Real t = curveTenorReal_[k];
                Real sum = 0.0;
                for (Size j = 0; j < basisFunctionNumber_; ++j) {
                    Real vVal = solution[j];
                    Real kappaVal;
                    if (close_enough(kappaUpperBound, 0.0))
                        kappaVal = solution[basisFunctionNumber_ + j];
                    else
                        kappaVal = kappaUpperBound / (std::acos(-1) / 2) * std::atan(solution[basisFunctionNumber_ + j]);
                    Real t0 = k == 0 || !useForwardRate_ ? 0.0 : curveTenorReal_[k - 1];
                    if (std::abs(kappaVal) < 1E-6) {
                        sum += vVal;
                    } else {
                        sum += vVal / (t - t0) / kappaVal * (std::exp(-kappaVal * t0) - std::exp(-kappaVal * t));
                    }
                }
                //LOG(curveTenorReal_[k] << "," << eigenVector[k] << "," << sum << "," << sum - eigenVector[k]);
            }
        }
        v_[ccyMatrix.first] = v;
        kappa_[ccyMatrix.first] = kappa;
    }
    // Format Output
    formatIrKappa();
    formatIrSigma();
}

void HwHistoricalCalibrationModel::computeCorrelation() {
    Size returnLength = dataIR_.begin()->second.size() - 1;
    Array arr1(returnLength, 0.0), arr2(returnLength, 0.0);
    // IR-IR correlation
    for (auto ir1 = irAbsoluteReturnAdjusted_.begin(); ir1 != irAbsoluteReturnAdjusted_.end(); ++ir1) {
        for (auto ir2 = next(ir1); ir2 != irAbsoluteReturnAdjusted_.end(); ++ir2) {
            Matrix correlationMatrix(ir1->second.columns(), ir2->second.columns(), 0.0);
            for (Size i = 0; i < ir1->second.columns(); ++i) {
                for (Size k = 0; k < returnLength; ++k) {
                    arr1[k] = ir1->second[k][i];
                }
                for (Size j = 0; j < ir2->second.columns(); ++j) {
                    for (Size k = 0; k < returnLength; ++k) {
                        arr2[k] = ir2->second[k][j];
                    }
                    correlationMatrix[i][j] = correlation(arr1, arr2);
                }
            }
            correlationMatrix_[std::make_pair("IR:" + ir1->first, "IR:" + ir2->first)] = correlationMatrix;
        }
    }
    // IR-FX correlation
    for (auto const& ir : irAbsoluteReturnAdjusted_) {
        for (auto const& fx : fxLogReturn_) {
            Matrix correlationMatrix(ir.second.columns(), 1, 0.0);
            for (Size i = 0; i < ir.second.columns(); ++i) {
                for (Size k = 0; k < returnLength; ++k) {
                    arr1[k] = ir.second[k][i];
                }
                correlationMatrix[i][0] = correlation(arr1, fx.second);
            }
            correlationMatrix_[std::make_pair("IR:" + ir.first, "FX:" + fx.first)] = correlationMatrix;
        }
    }
    // FX-FX correlation
    for (auto fx1 = fxLogReturn_.begin(); fx1 != fxLogReturn_.end(); ++fx1) {
        for (auto fx2 = next(fx1); fx2 != fxLogReturn_.end(); ++fx2) {
            Matrix correlationMatrix(1, 1, 0.0);
            correlationMatrix[0][0] = correlation(fx1->second, fx2->second);
            correlationMatrix_[std::make_pair("FX:" + fx1->first, "FX:" + fx2->first)] = correlationMatrix;
        }
    }
}

Real HwHistoricalCalibrationModel::correlation(Array arr1, Array arr2) {
    QL_REQUIRE(arr1.size() == arr2.size(), "Size of the 2 arrays used to calculate correlation must be the same.");
    Real sum_1 = 0.0, sum_2 = 0.0, sum_12 = 0.0, sum_11 = 0.0, sum_22 = 0.0;
    for (Size i = 0; i < arr1.size(); ++i) {
        sum_1 += arr1[i];
        sum_2 += arr2[i];
        sum_12 += arr1[i] * arr2[i];
        sum_11 += arr1[i] * arr1[i];
        sum_22 += arr2[i] * arr2[i];
    }
    Real corr = (arr1.size() * sum_12 - sum_1 * sum_2) /
                sqrt((arr1.size() * sum_11 - sum_1 * sum_1) * (arr1.size() * sum_22 - sum_2 * sum_2));
    return corr;
}

void HwHistoricalCalibrationModel::formatIrKappa() {
    for (auto const& ccyMatrix : kappa_) {
        Size n = principalComponent_.find(ccyMatrix.first)->second * basisFunctionNumber_;
        Array kappaFormatted(n, 0.0);
        for (Size i = 0; i < n; i++) {
            kappaFormatted[i] = ccyMatrix.second[i / basisFunctionNumber_][i % basisFunctionNumber_];
        }
        kappaFormatted_[ccyMatrix.first] = kappaFormatted;
    }
}

void HwHistoricalCalibrationModel::formatIrSigma() {
    for (auto const& ccyMatrix : v_) {
        Size n = principalComponent_.find(ccyMatrix.first)->second * basisFunctionNumber_;
        Matrix sigmaFormatted(principalComponent_.find(ccyMatrix.first)->second, n, 0.0);
        for (Size i = 0; i < n; i++) {
            sigmaFormatted[i / basisFunctionNumber_][i] =
                sqrt(eigenValue_.find(ccyMatrix.first)->second[i / basisFunctionNumber_]) *
                ccyMatrix.second[i / basisFunctionNumber_][i % basisFunctionNumber_];
        }
        sigmaFormatted_[ccyMatrix.first] = sigmaFormatted;
    }
}

} // namespace quantext
