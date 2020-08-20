/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file portfolio/additionalresults.hpp
    \brief AdditionalResults class
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <vector>

namespace ore {
namespace data {

//! Serializable additional result
/*!
  \ingroup portfolio
*/
class AdditionalResult {
public:
    //! Default constructor
    AdditionalResult(const boost::shared_ptr<Trade>& trade);

    //! Load from XML Node
    XMLNode* toXML(XMLDocument& doc);

private:
    std::string tradeId_;
    std::map<std::string, std::vector<QuantLib::Real>> vectorResults_;
    std::map<std::string, QuantLib::Matrix> matrixResults_;
    std::map<std::string, QuantLib::Real> doubleResults_; 
    std::map<std::string, int> intResults_; 
    std::map<std::string, std::string> stringResults_; 
};

//! Serializable additional results
/*!
  \ingroup portfolio
*/

class AdditionalResults {
public:
    //! Default constructor
    AdditionalResults(const boost::shared_ptr<Portfolio>& portfolio);

    //! Load from XML Node
    void toXML(XMLDocument& doc);

    //! Save AdditionalResults to an XML file
    void save(const std::string& fileName) const;

private: 
    boost::shared_ptr<Portfolio> portfolio_;
    std::vector<boost::shared_ptr<AdditionalResult>> additionalResults_;
};

} // namespace data
} // namespace ore
