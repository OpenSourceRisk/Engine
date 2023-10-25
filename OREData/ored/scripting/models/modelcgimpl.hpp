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

/*! \file ored/scripting/models/modelcgimpl.hpp
    \brief basis implementation for a script engine model
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/modelcg.hpp>
#include <ored/scripting/utilities.hpp>

#include <qle/termstructures/correlationtermstructure.hpp>

#include <ql/indexes/interestrateindex.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/timegrid.hpp>

namespace ore {
namespace data {

/* This class provides an implementation of the model interface. Derived classes have to implement
   - ModelCG::referenceDate()
   - ModelCG::npv()
   - ModelCG::fwdCompAvg()
   - ModelCG::getDirectFxSpotT0()
   - ModelCG::getDirectDiscountT0()
   and the interface defined by this class (the pure virtual methods defined below) */
class ModelCGImpl : public ModelCG {
public:
    /* Constructor arguments:
       - dayCounter: the convention to convert dates to times
       - size: the dimension of the randomvariables used by the model
       - currencies: the supported currencies, the first one is the model's base ccy
       - irIndices: pairs of ORE labels and QL indices, linked to T0 curves
       - infIndices: pairs of ORE labels and QL indices, linked to T0 curves
       - indices: eq, fx, comm index names following the ORE naming conventions; fx indices must have a domestic
         currency equal to the model's base ccy; fx indices should have the tag GENERIC here (historical fixings
         will still be handled using the original tag); we do not require an fx index for each non-base currency
         supported, we fall back on a zero vol conversion instead
       - indexCurrencies: index ccy for eq, comm, ir and the foreign ccy for fx indices
       - conventions: currently needed to resolve comm indices only
       - simulationDates: currently needed to resolve comm indices only
       - the conversion of a payment ccy != base ccy uses the fx index value (if existent), otherwise the zero vol
         version (i.e. the given fx spot and curves)
       - new and inverse currency pairs are implied from the existing ones in eval() for non-historical fixings
       - historical fixings are retrieved in eval() only; there they override today's spot if given
     */
    ModelCGImpl(const DayCounter& dayCounter, const Size size, const std::vector<std::string>& currencies,
                const std::vector<std::pair<std::string, boost::shared_ptr<InterestRateIndex>>>& irIndices,
                const std::vector<std::pair<std::string, boost::shared_ptr<ZeroInflationIndex>>>& infIndices,
                const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
                const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig);

    // Model interface implementation (partial)
    const std::string& baseCcy() const override { return currencies_.front(); }
    std::size_t dt(const Date& d1, const Date& d2) const override {
        return cg_const(*g_, dayCounter_.yearFraction(d1, d2));
    }
    std::size_t pay(const std::size_t amount, const Date& obsdate, const Date& paydate,
                    const std::string& currency) const override;
    std::size_t discount(const Date& obsdate, const Date& paydate, const std::string& currency) const override;
    std::size_t eval(const std::string& index, const Date& obsdate, const Date& fwddate,
                     const bool returnMissingMissingAsNull = false,
                     const bool ignoreTodaysFixing = false) const override;
    std::size_t fxSpotT0(const std::string& forCcy, const std::string& domCcy) const override;
    std::size_t barrierProbability(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                   const std::size_t barrier, const bool above) const override;

    // provide default implementation for MC type models (taking a simple expectation)
    Real extractT0Result(const RandomVariable& value) const override;

    // CG / AD part of the interface
    std::size_t cgVersion() const override;
    const std::vector<std::vector<std::size_t>>& randomVariates() const override; // dim / steps
    std::vector<std::pair<std::size_t, double>> modelParameters() const override;

protected:
    // get (non-ir) index (forward) value for index[indexNo] for (fwd >=) d >= reference date
    virtual std::size_t getIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const = 0;
    // get projection for irIndices[indexNo] for (fwd >=) d >= reference date, this should also return a value
    // if d (resp. fwd if given) is not a valid fixing date for the index
    virtual std::size_t getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const = 0;
    // get projection for infIndices[indexNo] for fwd >= d >= base date; fwd will always be given and be a first day of
    // an inflation period (this function is called twice, interpolation will be handled in the ModelCGImpl class then)
    virtual std::size_t getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd) const = 0;
    // get discount factor P(s,t) for ccy currencies[idx], t > s >= referenceDate
    virtual std::size_t getDiscount(const Size idx, const Date& s, const Date& t) const = 0;
    // get numeraire N(s) for ccy curencies[idx], s >= referenceDate
    virtual std::size_t getNumeraire(const Date& s) const = 0;
    // get fx spot for currencies[idx] vs. currencies[0], as of the referenceDate, should be 1 for idx=0
    virtual std::size_t getFxSpot(const Size idx) const = 0;
    // get barrier probability for refDate <= obsdate1 <= obsdate2, the case obsdate1 < refDate is handled in this class
    virtual std::size_t getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                             const std::size_t barrier, const bool above) const = 0;

    const DayCounter dayCounter_;
    const std::vector<std::string> currencies_;
    const std::vector<std::string> indexCurrencies_;
    const std::set<Date> simulationDates_;
    const IborFallbackConfig iborFallbackConfig_;

    std::vector<std::pair<IndexInfo, boost::shared_ptr<InterestRateIndex>>> irIndices_;
    std::vector<std::pair<IndexInfo, boost::shared_ptr<ZeroInflationIndex>>> infIndices_;
    std::vector<IndexInfo> indices_;

    // to be populated by derived classes when building the computation graph
    mutable std::vector<std::vector<size_t>> randomVariates_;
    mutable std::vector<std::pair<std::size_t, std::function<double(void)>>> modelParameters_;

    // convenience function to add model parameters
    void addModelParameter(const std::string& id, std::function<double(void)> f) const;

    // manages cg version and triggers recalculations of random variate / model parameter nodes
    void performCalculations() const override;

private:
    mutable std::size_t cgVersion_ = 0;
    mutable QuantLib::Date cgEvalDate_ = Date();

    // helper method to handle inflation fixings and their interpolation
    std::size_t getInflationIndexFixing(const bool returnMissingFixingAsNull, const std::string& indexInput,
                                        const boost::shared_ptr<ZeroInflationIndex>& infIndex, const Size indexNo,
                                        const Date& limDate, const Date& obsdate, const Date& fwddate,
                                        const Date& baseDate) const;
};

} // namespace data
} // namespace ore