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

#include <boost/range/adaptor/filtered.hpp>
#include <orea/simm/crifrecord.hpp>
#include <ored/report/report.hpp>

namespace ore {
namespace analytics {

struct crifRecordIsSimmParameter {
    bool operator()(const CrifRecord& x) { return x.isSimmParameter(); }
};

class Crif {
public:
    enum class CrifType { Empty, Frtb, Simm };
    Crif() = default;

    void addRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies = false);
    void addRecords(const Crif& crif, bool aggregateDifferentAmountCurrencies = false);

    void clear() { records_.clear(); }

    std::set<CrifRecord>::const_iterator begin() const { return records_.cbegin(); }
    std::set<CrifRecord>::const_iterator end() const { return records_.cend(); }
    std::set<CrifRecord>::const_iterator find(const CrifRecord& r) const { return records_.find(r); }
    //! 
    bool empty() const { return records_.empty(); }

    //! check if there are crif records beside simmParameters
    const bool hasCrifRecords() const;
    
    //! Simm methods
    //! Give back the set of portfolio IDs that have been loaded
    const std::set<std::string>& portfolioIds() const;
    const std::set<ore::data::NettingSetDetails>& nettingSetDetails() const;

    //! check if the Crif contains simmParameters
    const bool hasSimmParameter() const;

    //! returns a Crif containing only simmParameter entries
    Crif simmParameters() const;
    
    //! deletes all existing simmParameter and replaces them with the new one
    void setSimmParameters(const Crif& crif);

    //! For each CRIF record checks if amountCurrency and amount are 
    //! defined and uses these to populate the record's amountUsd
    void fillAmountUsd(const boost::shared_ptr<ore::data::Market> market);
    
    //! Check if netting set details are used anywhere, instead of just the netting set ID
    bool hasNettingSetDetails() const;

    //! Aggregate all existing records
    Crif aggregate() const;

private:
    void insertCrifRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies = false);
    void addFrtbCrifRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies = false);
    void addSimmCrifRecord(const CrifRecord& record, bool aggregateDifferentAmountCurrencies = false);
    void addSimmParameterRecord(const CrifRecord& record);
    void updateAmountExistingRecord(std::set<CrifRecord>::iterator& it, const CrifRecord& record);


    CrifType type_ = CrifType::Empty;
    std::set<CrifRecord> records_;

    //SIMM members
    //! Set of portfolio IDs that have been loaded
    std::set<std::string> portfolioIds_;
    std::set<ore::data::NettingSetDetails> nettingSetDetails_;
    
};
} // namespace analytics
} // namespace ore