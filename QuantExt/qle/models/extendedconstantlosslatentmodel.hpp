/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#ifndef quantext_extended_constantloss_latentmodel_hpp
#define quantext_extended_constantloss_latentmodel_hpp

//#include <ql/experimental/credit/defaultprobabilitylatentmodel.hpp>
#include <qle/models/defaultprobabilitylatentmodel.hpp>
// take the loss model to a different file and avoid this inclusion
//#include <ql/experimental/credit/defaultlossmodel.hpp>
#include <qle/models/defaultlossmodel.hpp>

namespace QuantExt {
using namespace QuantLib;

/*! Constant deterministic loss amount default latent model, extended to cover a discrete 
  distribution of recovery rates following Krekel (2008), https://ssrn.com/abstract=11340228. 
  For each obligor we pass a vector of J recovery probabilities p_1, ..., p_J and 
  recovery rates in decreasing order r_1 > r_2 > ... > r_J contional on default. 
  If this data is empty, the extended model will fall back on the ConstantLossLatentModel.
*/
template <class copulaPolicy> class ExtendedConstantLossLatentModel : public DefaultLatentModel<copulaPolicy> {
private:
    const std::vector<Real> recoveries_;
    typedef typename copulaPolicy::initTraits initTraits;

public:
    ExtendedConstantLossLatentModel(const std::vector<std::vector<Real>>& factorWeights, const std::vector<Real>& recoveries,
                                    const std::vector<std::vector<Real>>& recoveryProbabilities,
                                    const std::vector<std::vector<Real>>& recoveryRates,
                                    LatentModelIntegrationType::LatentModelIntegrationType integralType,
                                    const initTraits& ini = initTraits())
        : DefaultLatentModel<copulaPolicy>(factorWeights, integralType, ini), recoveries_(recoveries),
          recoveryProbabilities_(recoveryProbabilities), recoveryRates_(recoveryRates) {
        QL_REQUIRE(recoveries.size() == factorWeights.size(), "Incompatible factors and recovery sizes.");
        checkStochasticRecoveries();
    }

    ExtendedConstantLossLatentModel(const Handle<Quote>& mktCorrel, const std::vector<Real>& recoveries,
                                    const std::vector<std::vector<Real>>& recoveryProbabilities,
                                    const std::vector<std::vector<Real>>& recoveryRates,
                                    LatentModelIntegrationType::LatentModelIntegrationType integralType, Size nVariables,
                                    const initTraits& ini = initTraits())
        : DefaultLatentModel<copulaPolicy>(mktCorrel, nVariables, integralType, ini), recoveries_(recoveries),
          recoveryProbabilities_(recoveryProbabilities), recoveryRates_(recoveryRates) {
        // actually one could define the other and get rid of the variable
        // here and in other similar models
        QL_REQUIRE(recoveries.size() == nVariables, "Incompatible model and recovery sizes.");
        checkStochasticRecoveries(); 
    }

    // Check vector sizes and that expected recovery matches market quoted recovery for each obligor
    void checkStochasticRecoveries() {
        QL_REQUIRE(recoveryProbabilities_.size() == recoveryRates_.size(), "number of recovery probability vectors and market recovery rates differ");
        if (recoveryProbabilities_.size() == 0)
            return;
        QL_REQUIRE(recoveryProbabilities_.size() == recoveries_.size(), "number of recovery rates and recovery probablity vectors differ");
        for (Size i = 0; i < recoveries_.size(); ++i) {
            QL_REQUIRE(recoveryProbabilities_[i].size() == recoveryRates_[i].size(), "recovery and probability vector size mismatch for obligor " << i);
            Real expectedRecovery = 0.0;
            for (Size j = 0; j < recoveryProbabilities_[i].size(); ++j)
                expectedRecovery += recoveryProbabilities_[i][j] * recoveryRates_[i][j];
            QL_REQUIRE(QuantLib::close_enough(expectedRecovery, recoveries_[i]), "expected recovery does not match market recovery rate for obligor " << i);
        }
    }

    Real conditionalRecovery(const Date& d, Size iName, const std::vector<Real>& mktFactors) const {
        return recoveries_[iName];
    }

    Real conditionalRecovery(Probability uncondDefP, Size iName, const std::vector<Real>& mktFactors) const {
        return recoveries_[iName];
    }

    Real conditionalRecoveryInvP(Real invUncondDefP, Size iName, const std::vector<Real>& mktFactors) const {
        return recoveries_[iName];
    }

    Real conditionalRecovery(Real latentVarSample, Size iName, const Date& d) const { return recoveries_[iName]; }

    const std::vector<Real>& recoveries() const { return recoveries_; }

    // this is really an interface to rr models even if not imposed. Default
    // loss models do have an interface for this one. Enforced only through
    // duck typing.
    Real expectedRecovery(const Date& d, Size iName, const DefaultProbKey& defKeys) const { return recoveries_[iName]; }

    const std::vector<std::vector<Real>>& recoveryProbabilities() { return recoveryProbabilities_; }
    const std::vector<std::vector<Real>>& recoveryRateGrids() { return recoveryRates_; }
    
private:
    std::vector<std::vector<Real>> recoveryProbabilities_;
    std::vector<std::vector<Real>> recoveryRates_;
};

typedef ExtendedConstantLossLatentModel<GaussianCopulaPolicy> ExtendedGaussianConstantLossLM;

/*! ExtendedConstantLossLatentModel interface for loss models.
  While it does not provide distribution type losses (e.g. expected tranche
  losses) because it lacks an integration algorithm it serves to allow
  pricing of digital type products like NTDs.

  Alternatively fuse with the aboves class.
*/
template <class copulaPolicy>
class ExtendedConstantLossModel : public virtual ExtendedConstantLossLatentModel<copulaPolicy>, public virtual DefaultLossModel {
public:
    ExtendedConstantLossModel(const std::vector<std::vector<Real>>& factorWeights, const std::vector<Real>& recoveries,
                              const std::vector<std::vector<Real>>& recoveryProbabilities,
                              const std::vector<std::vector<Real>>& recoveryRates,
                              LatentModelIntegrationType::LatentModelIntegrationType integralType,
                              const typename copulaPolicy::initTraits& ini = copulaPolicy::initTraits())
        : ExtendedConstantLossLatentModel<copulaPolicy>(factorWeights, recoveries, recoveryProbabilities, recoveryRates, integralType, ini) {}

    ExtendedConstantLossModel(const Handle<Quote>& mktCorrel, const std::vector<Real>& recoveries,
                              const std::vector<std::vector<Real>>& recoveryProbabilities,
                              const std::vector<std::vector<Real>>& recoveryRates,
                              LatentModelIntegrationType::LatentModelIntegrationType integralType, Size nVariables,
                              const typename copulaPolicy::initTraits& ini = copulaPolicy::initTraits())
        : ExtendedConstantLossLatentModel<copulaPolicy>(mktCorrel, recoveries, recoveryProbabilities, recoveryRates, integralType, nVariables, ini) {}

protected:
    // Disposable<std::vector<Probability> > probsBeingNthEvent(
    //    Size n, const Date& d) const {
    //    return
    //      ConstantLossLatentmodel<copulaPolicy>::probsBeingNthEvent(n, d);
    //}
    Real defaultCorrelation(const Date& d, Size iName, Size jName) const {
        return ExtendedConstantLossLatentModel<copulaPolicy>::defaultCorrelation(d, iName, jName);
    }
    Probability probAtLeastNEvents(Size n, const Date& d) const {
        return ExtendedConstantLossLatentModel<copulaPolicy>::probAtLeastNEvents(n, d);
    }
    Real expectedRecovery(const Date& d, Size iName, const DefaultProbKey& k) const {
        return ExtendedConstantLossLatentModel<copulaPolicy>::expectedRecovery(d, iName, k);
    }

private:
    virtual void resetModel() {
        // update the default latent model we derive from
        DefaultLatentModel<copulaPolicy>::resetBasket(DefaultLossModel::basket_.currentLink()); // forces interface
    }
};

} // namespace QuantExt

#endif
