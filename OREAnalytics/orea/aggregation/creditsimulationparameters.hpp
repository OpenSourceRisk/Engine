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

/*! \file aggregation/creditsimulationparameters.hpp
    \brief Credit simulation parameter class
    \ingroup engine
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>

#include <ql/math/matrix.hpp>

using QuantLib::Array;
using QuantLib::Matrix;
using QuantLib::Size;
using std::string;

namespace ore {
using namespace data;
namespace analytics {

//! Credit simulation description
/*! \ingroup engine
 */
class CreditSimulationParameters : public XMLSerializable {
public:
    //! Default constructor
    CreditSimulationParameters(){};

    //! \name Inspectors
    //@
    const std::map<string, Matrix>& transitionMatrix() const { return transitionMatrix_; }
    const std::vector<string>& entities() const { return entities_; }
    const std::vector<Array>& factorLoadings() const { return factorLoadings_; }
    const std::vector<string>& transitionMatrices() const { return transitionMatrices_; }
    const std::vector<Size>& initialStates() const { return initialStates_; }
    bool marketRisk() const { return marketRisk_; }
    bool creditRisk() const { return creditRisk_; }
    bool zeroMarketPnl() const { return zeroMarketPnl_; }
    const string& evaluation() const { return evaluation_; }
    bool doubleDefault() const { return doubleDefault_; }
    Size seed() const { return seed_; }
    Size paths() const { return paths_; }
    const std::string& creditMode() const { return creditMode_; }
    const std::string& loanExposureMode() const { return loanExposureMode_; }
    const std::vector<string>& nettingSetIds() const { return nettingSetIds_; }
    //@}

    //! \name Setters
    //@{
    std::map<string, Matrix>& transitionMatrix() { return transitionMatrix_; }
    std::vector<string>& entities() { return entities_; }
    std::vector<Array>& factorLoadings() { return factorLoadings_; }
    std::vector<string>& transitionMatrices() { return transitionMatrices_; }
    std::vector<Size>& initialStates() { return initialStates_; }
    bool& marketRisk() { return marketRisk_; }
    bool& creditRisk() { return creditRisk_; }
    bool& zeroMarketPnl() { return zeroMarketPnl_; }
    string& evaluation() { return evaluation_; }
    bool& doubleDefault() { return doubleDefault_; }
    Size& seed() { return seed_; }
    Size& paths() { return paths_; }
    std::string& creditMode() { return creditMode_; }
    std::string& loanExposureMode() { return loanExposureMode_; }
    std::vector<string>& nettingSetIds() { return nettingSetIds_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc)const ;
    //@}

    //! \Equality Operators
    //@{
    // bool operator==(const CreditSimulationParameters& rhs);
    // bool operator!=(const CreditSimulationParameters& rhs);
    //@}

private:
    std::map<string, Matrix> transitionMatrix_;
    std::vector<string> entities_;
    std::vector<Array> factorLoadings_;
    std::vector<string> transitionMatrices_;
    std::vector<Size> initialStates_;
    bool marketRisk_;
    bool creditRisk_;
    bool zeroMarketPnl_;
    string evaluation_;
    bool doubleDefault_;
    Size seed_, paths_;
    string creditMode_;
    string loanExposureMode_;
    std::vector<string> nettingSetIds_;
};
} // namespace analytics
} // namespace ore
