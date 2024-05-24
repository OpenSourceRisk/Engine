/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/formulabasedlegdata.hpp
    \brief leg data for formula based leg types
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <qle/indexes/formulabasedindex.hpp>

namespace ore {
namespace data {

class FormulaBasedLegData : public LegAdditionalData {
public:
    //! Default constructor
    FormulaBasedLegData() : LegAdditionalData("FormulaBased"), fixingDays_(0), isInArrears_(false) {}
    //! Constructor
    FormulaBasedLegData(const string& formulaBasedIndex, int fixingDays, bool isInArrears)
        : LegAdditionalData("FormulaBased"), formulaBasedIndex_(formulaBasedIndex), fixingDays_(fixingDays),
          isInArrears_(isInArrears) {
        initIndices();
    }

    //! \name Inspectors
    //@{
    const string& formulaBasedIndex() const { return formulaBasedIndex_; }
    int fixingDays() const { return fixingDays_; }
    const string& fixingCalendar() const { return fixingCalendar_; }
    bool isInArrears() const { return isInArrears_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    void initIndices();
    string formulaBasedIndex_;
    int fixingDays_;
    string fixingCalendar_;
    bool isInArrears_;
};

Leg makeFormulaBasedLeg(const LegData& data, const QuantLib::ext::shared_ptr<QuantExt::FormulaBasedIndex>& formulaBasedIndex,
                        const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                        const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexMaps,
                        const QuantLib::Date& openEndDateReplacement = Null<Date>());

} // namespace data
} // namespace ore
