/*
 Copyright (C) 2023 AcadiaSoft Inc.
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

/*! \file orea/simm/crif.hpp
    \brief Struct for holding CRIF records
*/

#pragma once

#include <orea/simm/crifrecord.hpp>
#include <ored/report/report.hpp>

namespace ore {
namespace analytics {

class Crif {
public:
    enum class CrifType { Empty, Frtb, Simm };
    Crif() = default;

    void addRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies = false);
    void addRecords(const Crif& crif, bool aggregateDifferentAmountCurrencies = false);

    void clear() { records_.clear(); }

    std::set<CrifRecord>::const_iterator begin() { return records_.cbegin(); }
    std::set<CrifRecord>::const_iterator end() { return records_.cend(); }

    //! Simm methods
    //! Give back the set of portfolio IDs that have been loaded
    
    const std::set<std::string>& portfolioIds() const;
    const std::set<ore::data::NettingSetDetails>& nettingSetDetails() const;

    Crif simmParameters() const {
        Crif simmParam;
        for (const auto& r : records_) {
            if (r.isSimmParameter()) {
                simmParam.addRecord(r);
            }
        }
        return simmParam;
    }

    Crif exlcudeSimmParameters() const {
        Crif simmParam;
        for (const auto& r : records_) {
            if (!r.isSimmParameter()) {
                simmParam.addRecord(r);
            }
        }
        return simmParam;
    }
    //! For each CRIF record checks if amountCurrency and amount are 
    //! defined and uses these to populate the record's amountUsd
    void fillAmountUsd(const boost::shared_ptr<ore::data::Market> market);
    //! Check if netting set details are used anywhere, instead of just the netting set ID
    bool hasNettingSetDetails() const;

    //! Aggregate all existing records
    Crif aggregate() const;

private:
    std::set<CrifRecord> records_;

    //SIMM members
    //! Set of portfolio IDs that have been loaded
    std::set<std::string> portfolioIds_;
    std::set<ore::data::NettingSetDetails> nettingSetDetails_;
    
};
} // namespace analytics
} // namespace ore