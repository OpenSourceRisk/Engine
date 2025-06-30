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

#include <orea/aggregation/simmhelper.hpp>
#include <orea/simm/simmbucketmapperbase.hpp>
#include <orea/simm/simmconfigurationisdav2_6_5.hpp>

#include <ored/portfolio/fxoption.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace analytics {

SimmHelper::SimmHelper(const std::vector<std::string>& currencies, const QuantLib::ext::shared_ptr<NPVCube>& cube,
                       const QuantLib::ext::shared_ptr<AggregationScenarioData>& marketCube,
                       const QuantLib::ext::shared_ptr<SensitivityStorageManager>& sensitivityStorageManager,
                       const QuantLib::ext::shared_ptr<ore::data::Market>& market)
    : referenceDate_(Settings::instance().evaluationDate()), dc_(ActualActual(ActualActual::ISDA)),
      currencies_(currencies), cube_(cube), marketCube_(marketCube), market_(market) {

    QL_REQUIRE(cube, "SimmHelper: cube is null");

    ssm_ = QuantLib::ext::dynamic_pointer_cast<SimmSensitivityStorageManager>(sensitivityStorageManager);
    QL_REQUIRE(ssm_, "SimmHelper: SimmSensitivityStorageManager is null, not set or wrong type");

    irDeltaInstruments_.clear();
    for (auto p : ssm_->irDeltaTerms()) {
        if (p < 1 * Years)
            irDeltaInstruments_.push_back(IrDeltaParConverter::InstrumentType::Deposit);
        else
            irDeltaInstruments_.push_back(IrDeltaParConverter::InstrumentType::Swap);
    }

    // dimension = 1, since we call the calculator for each sample individually
    imCalculator_ = QuantLib::ext::make_shared<SimpleDynamicSimm>(
        1, currencies_, ssm_->irDeltaTerms(), ssm_->irVegaTerms(), ssm_->fxVegaTerms(),
        QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_6_5>(QuantLib::ext::make_shared<SimmBucketMapperBase>(),
                                                                  10));
    irDeltaConverter_.resize(currencies_.size());
    std::string baseCcy = currencies_[0];
    for (Size ccyIndex = 0; ccyIndex < currencies_.size(); ++ccyIndex) {
        irDeltaConverter_[ccyIndex] = IrDeltaParConverter(
            ssm_->irDeltaTerms(), irDeltaInstruments_,
            *market_->swapIndex(market_->swapIndexBase(currencies_[ccyIndex])),
            [this](const Date& d) { return timeFromReference(d); });
    }
}

Real SimmHelper::timeFromReference(const Date& d) const {
    return dc_.yearFraction(referenceDate_, d);
}

Real SimmHelper::initialMargin(const std::string& nettingSetId, const Size dateIndex, const Size sampleIndex,
                               bool deltaMargin, bool vegaMargin, bool curvatureMargin,
			       bool IR, bool FX) {

    DLOG("SimmHelper::initialMargin called for date " << dateIndex << ", sample " << sampleIndex
                                                      << " for delta/vega/curvature components " << deltaMargin << "/"
                                                      << vegaMargin << "/" << curvatureMargin);

    QL_REQUIRE((dateIndex == Null<Size>() && sampleIndex == Null<Size>()) ||
                   (dateIndex != Null<Size>() && sampleIndex != Null<Size>()),
               "SimmHelper::initialMargin(): date and sample index must be both null (write to T0 "
               "slice) or both not null");

    auto r = ssm_->getSensitivities(cube_, nettingSetId, dateIndex, sampleIndex);
    QL_REQUIRE(r.type() == typeid(std::tuple<Array, std::vector<Array>, std::vector<Array>, Real>),
               "SimmHelper::initialMargin(): unexpected result type '" << r.type().name() << "' from SimmSensitivityStorageManager");

    LOG("SimmHelper got sensitivities");
     
    Array delta;
    std::vector<Array> swaptionVegaRisk;
    std::vector<Array> fxVega;
    Real theta;
    
    std::tie(delta, swaptionVegaRisk, fxVega, theta) = boost::any_cast<std::tuple<Array, vector<Array>, vector<Array>, Real>>(r);

    DLOG("SimmHelper delta size: " << delta.size());
    DLOG("SimmHelper swaptionVegaRisk size: " << swaptionVegaRisk.size());
    DLOG("SimmHelper fxVega size: " << fxVega.size());
    
    // Map delta array of Reals to irDelta matrix of RandomVariables, in order to utilize the SimpleDynamicSimm calculator
    auto irDeltaIM = std::vector<std::vector<QuantExt::RandomVariable>>(
        currencies_.size(), std::vector<RandomVariable>(ssm_->irDeltaTerms().size(), RandomVariable(1, 0.0)));
    Array tmpDelta(ssm_->irDeltaTerms().size(), 0.0);
    Size idx = 0;
    for (Size i = 0; i < currencies_.size(); ++i) {
        Size idxBackup = idx;
        for (Size j = 0; j < ssm_->irDeltaTerms().size(); ++j) {
            tmpDelta[j] = delta[idx];
            idx++;
        }
        Array parDelta = transpose(irDeltaConverter_[i].dzerodpar()) * tmpDelta;
	// The same, component-wise
        // Array parDelta(ssm_->irDeltaTerms().size(), 0.0);
        // for (std::size_t b = 0; b < tmpDelta.size(); ++b) {
        //     for (std::size_t z = 0; z < tmpDelta.size(); ++z) {
        //         parDelta[b] += irDeltaConverter_[i].dzerodpar()(z, b) * tmpDelta[z];
	//     }
	// }

	idx = idxBackup;
        for (Size j = 0; j < ssm_->irDeltaTerms().size(); ++j) {
            QL_REQUIRE(idx < delta.size(), "delta index " << idx << " out of range");
            irDeltaIM[i][j].set(0, parDelta[j] * 0.0001); // SIMM calculator expects shift size 1bp absolute
	    idx++;
        }
    }
    LOG("SimmHelper irDelta mapped");

    // Map delta array of Reals to fxDeltaIM vector of RandomVariables
    // calculator
    auto fxDeltaIM = std::vector<QuantExt::RandomVariable>(currencies_.size() - 1, RandomVariable(1, 0.0));
    std::string baseCcy = currencies_[0];
    for (Size i = 0; i < currencies_.size() - 1; ++i) {
        QL_REQUIRE(idx < delta.size(), "delta index " << idx << " out of range");
        std::string ccy = currencies_[i + 1];
	// The stored FX Delta is partial derivative w.r.t. ln(FX), let's call it delta_1,
	// delta_1 = \frac{\partial V}{\partial \ln(FX)}
	// We need
	// delta_2 = \frac{\partial V}{\partial FX}
	//         = \frac{\partial_V}{\partial \ln(FX)} \times \frac{\partial \ln(FX)}{\partial FX}
	//         = delta_1 * 1/FX 
	// The SIMM calculator expects shift size 1% relative, i.e. FX/100 absolute, so we need to feed the scaled sensitivity
	// delta_2 * FX/100 = delta_1 / 100
	// into the SIMM calculator, hence no need to get the FX rate from the simulated market.
	
	// Real fx = 1.0;
        // if (dateIndex == Null<Size>()) {
        //     try {
        //         fx = market_->fxSpot(ccy + baseCcy)->value();
        //     } catch (std::exception& e) {
        //         ALOG("Could not get fx rate from the t0 market for pair " << baseCcy + ccy << ": " << e.what());
        //     }
        // } else {
        //     try {
        //         fx = marketCube_->get(dateIndex, sampleIndex, AggregationScenarioDataType::FXSpot, ccy);
        //     } catch (std::exception& e) {
        //         ALOG("Could not get fx rate from the simulation market for pair " << baseCcy + ccy << ": " << e.what());
        //     }
        // }
        fxDeltaIM[i].set(0, delta[idx] * 0.01); 
        idx++;
    }
    LOG("SimmHelper fxDelta mapped");

    // Compress vector of vega matrices of Reals into vector of vega arrays of RandomVariables aggregating
    // across underlying term and scaling to SIMM's Swaption VegaRisk.
    auto irVegaIM = std::vector<std::vector<QuantExt::RandomVariable>>(
        currencies_.size(), std::vector<RandomVariable>(ssm_->irVegaTerms().size(), RandomVariable(1, 0.0)));
    for (Size i = 0; i < currencies_.size(); ++i) {
        std::string ccy = currencies_[i];
        for (Size j = 0; j < ssm_->irVegaTerms().size(); ++j)
            irVegaIM[i][j].set(0, swaptionVegaRisk[i][j]);
    }
    LOG("SimmHelper swaptionVega mapped");

    // Map vector of vega arrays of Reals into vector of vega arrays of RandomVariables.
    auto fxVegaIM = std::vector<std::vector<QuantExt::RandomVariable>>(
        currencies_.size() - 1, std::vector<RandomVariable>(ssm_->fxVegaTerms().size(), RandomVariable(1, 0.0)));
    for (Size i = 0; i < currencies_.size() - 1; ++i) {
      Real sum = 0;
      for (Size j = 0; j < ssm_->fxVegaTerms().size(); ++j) {
	    // The SIMM calculator expects shift size 0.01 absolute (!)
	    fxVegaIM[i][j].set(0, fxVega[i][j] * 0.01);
	    sum += fxVega[i][j] * 0.01;
        }
        LOG("ccy " << i << " date " << dateIndex << " sample " << sampleIndex << " FxVega " << sum << " buckets "
	    << to_string(fxVega[i]));
        if (dateIndex != Null<Size>() && i == 1) {
            Date d = cube_->dates()[dateIndex];
            Time t = timeFromReference(d);
            LOG(nettingSetId << "," << sampleIndex << "," << io::iso_date(d) << "," << t << "," << sum
                             << ",Ccy,FxVega,6"
                             << " " << to_string(fxVega[i]));
        }
    }

    RandomVariable res = imCalculator_->value(irDeltaIM, irVegaIM, fxDeltaIM, fxVegaIM);
    totalMargin_ = res.at(0);

    if (deltaMargin) {
        RandomVariable res = imCalculator_->value(irDeltaIM, irVegaIM, fxDeltaIM, fxVegaIM, true, false, false);
        deltaMargin_ = res.at(0);
    }

    if (vegaMargin) {
        RandomVariable res = imCalculator_->value(irDeltaIM, irVegaIM, fxDeltaIM, fxVegaIM, false, true, false);
        vegaMargin_ = res.at(0);
    }

    if (curvatureMargin) {
        RandomVariable res = imCalculator_->value(irDeltaIM, irVegaIM, fxDeltaIM, fxVegaIM, false, false, true);
        curvatureMargin_ = res.at(0);
    }

    if (deltaMargin && IR) {
        RandomVariable res =
            imCalculator_->value(irDeltaIM, irVegaIM, fxDeltaIM, fxVegaIM, true, false, false, true, false);
        irDeltaMargin_ = res.at(0);
    }

    if (deltaMargin && FX) {
        RandomVariable res =
            imCalculator_->value(irDeltaIM, irVegaIM, fxDeltaIM, fxVegaIM, true, false, false, false, true);
        fxDeltaMargin_ = res.at(0);
    }

    LOG("SimmHelper::initialMargin done for date " << dateIndex << ", sample " << sampleIndex);

    return totalMargin_;
    
} // var

} // namespace analytics
} // namespace ore
