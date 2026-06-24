/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/models/modelcg.hpp
    \brief interface for model against which a script can be run
    \ingroup utilities
*/

#pragma once

#include <qle/ad/computationgraph.hpp>
#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_ops.hpp>

#include <ql/patterns/lazyobject.hpp>
#include <ql/settings.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/timegrid.hpp>

#include <ql/any.hpp>

#include <optional>

namespace QuantExt {
class ComputationGraph;
}

namespace ore {
namespace data {

using QuantLib::Date;
using QuantLib::Integer;
using QuantLib::Natural;
using QuantLib::Real;
using QuantLib::Size;

/*! The local base ccy functionality only makes sense with model type MC, it can not be used with FD 

    There are two versions of this functionality, of which only v1 is currently implemented in derived classes:

    v1: for local base ccy != global base ccy, only underlying indices with index ccy = local base ccy and payments
        in local base ccy can be processed

    v2: no restrictions, all underlying indices and payment currencies can be processed

    Derived classes are required to provide suitable checks, i.e., throw an error when v2 functionality is required
    but only v1 is implemented.
*/
class ModelCG : public QuantLib::LazyObject {
public:
    enum class Type { MC, FD };

    class ModelParameter {
    public:
        /* '*derived*' model parameters depend on other, more fundamental nodes and parameters and are
           only there to cache results in a convenient way, i.e. we could use a separate class for them */
        enum class Type {
            none,                    // type not set (= model param is not initialized)
            fix,                     // fixing, historical (non-derived param) or projected (*derived*)
            complexRate,             // complex ir rate (on, cms, ...), only gcam, *derived*
            dsc,                     // T0 ir discount
            fwdCompAvg,              // T0 compounded / avg ir rate (only bs)
            div,                     // T0 div yield dsc factor (only bs)
            rfr,                     // T0 rfr       dsc factor (only bs)
            defaultProb,             // T0 default prob
            lgm_H,                   // lgm1f parameters (only gcam)
            lgm_Hprime,              // ...
            lgm_zeta,                // ...
            fxbs_sigma,              // fxbs vol parameter (only gcam)
            logX0,                   // stoch process log initial value (only bs)
            logFxSpot,               // log fx spot (initial value from T0)
            sqrtCorr,                // model sqrt correlation
            sqrtCov,                 // model sqrt covariance
            corr,                    // model correlation
            cov,                     // model cov
            cam_corrzz,              // gcam ir-ir corr
            cam_corrzx,              // gcam ir-fx corr
            lgm_numeraire,           // lgm1f parameters (only gcam), *derived*
            lgm_discountBond,        // dito, *derived*
            lgm_reducedDiscountBond, // dito, *derived*
            interpolated_undpath,    // interpolated underlying path value (only gcam), *derived*
            interpolated_irstate,    // interpolated ir state value (only gcam), *derived*
            pay,                     // pay function cache (ModelCGImpl), *derived*
            eval,                    // eval function cache (ModelCGImpl), *derived*
            fxSpotT0,                // possibly triangulated fx spot t0, *derived*
            fxRate,                  // fx rate (to base ccy) at an obs date, *derived*
            convertToBaseCcy         // for convertToBaseCcy(), *derived*
        };

        ~ModelParameter() = default;
        ModelParameter() = default;
        ModelParameter(const Type type, const std::string& qualifier, const std::string& qualifier2 = {},
                       const QuantLib::Date& date = {}, const QuantLib::Date& date2 = {},
                       const QuantLib::Date& date3 = {}, const std::size_t index = 0, const std::size_t index2 = 0,
                       const std::size_t hash = 0, const double time = 0.0);
        ModelParameter(const ModelParameter& p) = default;
        ModelParameter(ModelParameter&& p) = default;
        ModelParameter& operator=(const ModelParameter& p) = default;
        ModelParameter& operator=(ModelParameter&& p) = default;

        friend bool operator==(const ModelParameter& x, const ModelParameter& y);
        friend bool operator<(const ModelParameter& x, const ModelParameter& y);

        Type type() const { return type_; }
        const std::string& qualifier() const { return qualifier_; }
        const std::string& qualifier2() const { return qualifier2_; }
        const QuantLib::Date& date() const { return date_; }
        const QuantLib::Date& date2() const { return date2_; }
        const QuantLib::Date& date3() const { return date3_; }
        double time() const { return time_; }
        std::size_t index() const { return index_; }
        std::size_t index2() const { return index2_; }
        std::size_t hash() const { return hash_; }
        double eval() const { return functor_ ? functor_() : 0.0; }
        std::size_t node() const { return node_; }

        void setFunctor(const std::function<double(void)>& f) const { functor_ = f; }
        void setNode(const std::size_t node) const { node_ = node; }

    private:
        // key, diferent types use a different subset of fields
        Type type_ = Type::none;
        std::string qualifier_;
        std::string qualifier2_;
        QuantLib::Date date_;
        QuantLib::Date date2_;
        QuantLib::Date date3_;
        std::size_t index_;
        std::size_t index2_;
        std::size_t hash_;
        double time_;
        // functor, only filled for primary model parameters, not filled for cached parameters
        mutable std::function<double(void)> functor_;
        // node in cg, this is filled for primary model parameters and cached parameters
        mutable std::size_t node_ = QuantExt::ComputationGraph::nan;
    };

    ModelCG(const QuantLib::Size n, const bool enableCgOptimization);
    virtual ~ModelCG() {}

    // computation graph
    QuantLib::ext::shared_ptr<QuantExt::ComputationGraph> computationGraph() { return g_; }

    // model type
    virtual ModelCG::Type type() const = 0;

    // number of paths
    virtual QuantLib::Size size() const { return n_; }

    // DEAD at the moment - if not null, this model uses a separate mc training phase for NPV() calcs
    virtual Size trainingSamples() const { return QuantLib::Null<Size>(); }

    /* DEAD at the moment - enable / disable the usage of the training paths (if trainingPaths() is not null)
       the model should be using training paths only temporarily and reset to normal model via RAII */
    virtual void toggleTrainingPaths() const {}

    // if true use sticky close out date impmlied market for all subsequent calls;
    virtual void useStickyCloseOutDates(const bool b) const;

    // the current ref date of the model
    const Date& referenceDate() const { return referenceDate_; };

    // the (actual) time from reference measured in the model
    virtual Real actualTimeFromReference(const Date& d) const = 0;

    // the (global) base ccy of the model
    virtual const std::string& baseCurrency() const = 0;

    // the list of supported model currencies
    virtual const std::vector<std::string>& currencies() const = 0;

    // time between two dates d1 <= d2, default actact should be overriden in derived claases if appropriate
    virtual std::size_t dt(const Date& d1, const Date& d2) const;

    // result must be as of max(refdate, obsdate); refdate < paydate and obsdate <= paydate required
    // localBaseCurrency determines both the result ccy and the paths used for fx, discount and numeraire
    virtual std::size_t pay(const std::size_t amount, const Date& obsdate, const Date& paydate,
                            const std::string& currency,
                            const std::string& localBaseCurrency = {}) const = 0;

    // refdate <= obsdate <= paydate required, in currency, localBaseCurrency determines the paths
    virtual std::size_t discount(const Date& obsdate, const Date& paydate, const std::string& currency,
                                 const std::string& localBaseCurrency = {}) const = 0;

    // fx rate at date obsdate, currency vs. baseCcy resp. localBaseCcy, if given, 
    // on paths in baseCcy reps. localBaseCcy. refdate <= obsdate required
    virtual std::size_t fxRate(const Date& obsdate, const std::string& currency,
                               const std::string& localBaseCurrency = {}) const = 0;

    /* refdate <= obsdate required
      - automatic mode (no overwrite regressors are given):
        the whole model state (in global base ccy) plus addRegressors are used to calculate the conditional expectation
      - overwriteRegressors are given (only for MC):
        these replace the whole regressor set and must also include any additional regressors (i.e., addRegressors are
      ignored) these can also be used to use regressors from a local base ccy
      - evaluationRegressors are given (only for MC):
        these determine the final variables on which the regression model is evaluated, the number of regressors must be
      consistent with those used for training, usually this is used in combination with overwriteRegressor, but it is
      also possible to use with automatic mode */
    virtual std::size_t npv(const std::size_t amount, const Date& obsdate, const std::size_t filter,
                            const std::optional<long>& memSlot, const std::set<std::size_t> addRegressors,
                            const std::optional<std::set<std::size_t>>& overwriteRegressors,
                            const std::optional<std::set<std::size_t>>& evaluationRegressors = {}) const = 0;

    /* regressors used in npv()
      - relevantCurrencies - if not none - restrict the set of currencies for which regressors are generated
      - localBaseCurrency is used to determine the relevant fx indices in the regressor set
      - localBaseCurrencyPaths determines the paths w.r.t. which the regressors are built
      - both localBaseCurrency and localBaseCurrencyPaths default to the global model base ccy, if not given
      - relevantCurrencies must contain all relevant currencies, i.e. localBaseCurrency (or localBaseCurrencyPaths)
        will _not_ be added to this set of currencies */
    virtual std::set<std::size_t> npvRegressors(const Date& obsdate,
                                                const std::optional<std::set<std::string>>& relevantCurrencies,
                                                const std::string& localBaseCurrency = {},
                                                const std::string& localBaseCurrencyPaths = {}) const = 0;

    /* eval index at (past or future) obsdate:
       - if fwddate != null, fwddate > obsdate is required. A check must be implemented that the obsdate allows for
         the index projection. For non-inflation indices this check is simply obsdate >= refdate. For zero inflation
         indices the is is obsdate >= basedate where the basedate is the one from the zero inflation ts asociated
         to the index.
       - if a historical fixing is required and missing, the behaviour depends on returnMissingFixingAsNull, if this
         flag is true, an exception is thrown, if false, the node "nan" is returned
       - for non-inflation indices, if ignoreTodaysFixing is true, always return the market spot for obsdate =
         referencedate, even if  a historical fixing is available; for inflation indices, ignore this flag
    */
    virtual std::size_t eval(const std::string& index, const Date& obsdate, const Date& fwddate,
                             const bool returnMissingFixingAsNull = false, const bool ignoreTodaysFixing = false,
                             const std::string& localBaseCurrency = {}) const = 0;

    /* get numeraire N(s) for s >= referenceDate in currency on paths determined by localBaseCurrency */
    virtual std::size_t numeraire(const Date& s, const std::string& currency = {},
                                  const std::string& localBaseCurrency = {}) const = 0;

    /* get conversion to base factor FX_local-global(t) * N_local(t) / N_global(t)
       note: the paths are in global base ccy paths always */
    virtual std::size_t convertToBaseCcy(const Date& s, const std::string& localBaseCurrency) const = 0;

    // forward looking daily comp/avg, obsdate <= start < end required, result must be as of max(refdate, obsdate)
    virtual std::size_t fwdCompAvg(const bool isAvg, const std::string& index, const Date& obsdate, const Date& start,
                                   const Date& end, const Real spread, const Real gearing, const Integer lookback,
                                   const Natural rateCutoff, const Natural fixingDays, const bool includeSpread,
                                   const Real cap, const Real floor, const bool nakedOption, const bool localCapFloor,
                                   const std::string& localBaseCurrency) const = 0;

    // barrier hit probability, obsdate1 <= obsdate2 required, but refdate can lie anywhere w.r.t. obsdate1, 2
    virtual std::size_t barrierProbability(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                           const std::size_t barrier, const bool above,
                                           const std::string& localBaseCurrency) const = 0;

    // get T0 fx spot
    virtual std::size_t fxSpotT0(const std::string& forCcy, const std::string& domCcy) const = 0;

    // extract T0 result from random variable
    virtual Real extractT0Result(const QuantExt::RandomVariable& value) const = 0;

    // reset stored NPV() regression coefficients (if applicable)
    virtual void resetNPVMem() {}

    // additional results provided by the model
    const std::map<std::string, QuantLib::ext::any>& additionalResults() const {
        if (additionalResults_.empty())
            populateAdditionalResults();
        return additionalResults_;
    }

    // path level additional results provided by the model
    const std::map<std::string, QuantLib::ext::any>& additionalResultsPathLevel() const {
        if (additionalResultsPathLevel_.empty())
            populateAdditionalResultsPathLevel();
        return additionalResultsPathLevel_;
    }

    // CG / AD part of the interface
    virtual std::size_t cgVersion() const = 0;
    virtual const std::vector<std::vector<std::size_t>>& randomVariates() const = 0; // dim / steps

    // get fx spot as of today directly, i.e. bypassing the cg
    virtual Real getDirectFxSpotT0(const std::string& forCcy, const std::string& domCcy) const = 0;

    // get discount as of today directly, i.e. bypassing the cg
    virtual Real getDirectDiscountT0(const Date& paydate, const std::string& currency) const = 0;

    // calculate the model
    void calculate() const override { LazyObject::calculate(); }

    // get model parameters
    std::set<ModelCG::ModelParameter>& modelParameters() const { return modelParameters_; };

    // get cached parameters
    std::set<ModelCG::ModelParameter>& cachedParameters() const { return cachedParameters_; };

    // add a model parameer if not yet present, return node in any case
    std::size_t addModelParameter(const ModelCG::ModelParameter& p, const std::function<double(void)>& f) const;

    // linear interpolation helper method, d must not lie outside knownDates resp. knownTimes
    std::tuple<QuantLib::Date, QuantLib::Date, std::size_t, std::size_t>
    getInterpolationWeights(const QuantLib::Date& d, const std::set<Date>& knownDates) const;
    std::tuple<std::size_t, std::size_t, std::size_t, std::size_t>
    getInterpolationWeights(const double t, const QuantLib::TimeGrid& knownTimes) const;

protected:
    // map with additional results provided by this model instance
    mutable std::map<std::string, QuantLib::ext::any> additionalResults_;
    mutable std::map<std::string, QuantLib::ext::any> additionalResultsPathLevel_;

    // the underlying computation graph
    QuantLib::ext::shared_ptr<QuantExt::ComputationGraph> g_;

    // the current reference date of the model
    mutable Date referenceDate_;

    // container holding model parameters and cached parameters
    mutable std::set<ModelCG::ModelParameter> modelParameters_;
    mutable std::set<ModelCG::ModelParameter> cachedParameters_;

private:
    void performCalculations() const override {}

    // populate additional results on demand
    virtual void populateAdditionalResults() const {}
    virtual void populateAdditionalResultsPathLevel() const {}

    // size of random variables within model
    QuantLib::Size n_;
    // whether to enable optimization in cg building
    bool enableCgOptimization_;
};

// standalone version of ModelCG::addModelParameters()
std::size_t addModelParameter(QuantExt::ComputationGraph& g, std::set<ModelCG::ModelParameter>& modelParameters,
                              const ModelCG::ModelParameter& p, const std::function<double(void)>& f);

bool operator==(const ModelCG::ModelParameter& x, const ModelCG::ModelParameter& y);
bool operator<(const ModelCG::ModelParameter& x, const ModelCG::ModelParameter& y);

// output model parameter
std::ostream& operator<<(std::ostream& o, const ModelCG::ModelParameter::Type& t);
std::ostream& operator<<(std::ostream& o, const ModelCG::ModelParameter& p);

} // namespace data
} // namespace ore
