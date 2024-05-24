/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/correlationmatrix.hpp
    \brief configuration class for building correlation matrices
    \ingroup utilities
*/

#pragma once

#include <map>
#include <qle/models/crossassetmodel.hpp>
#include <ql/math/matrix.hpp>
#include <ql/quote.hpp>

namespace ore {
namespace data {

/*! Struct for holding information on a factor in the correlation matrix. For example <code>{ IR, "EUR", 0 }</code> 
    is the first factor in the EUR interest rate process.
*/
struct CorrelationFactor {
    QuantExt::CrossAssetModel::AssetType type;
    std::string name;
    QuantLib::Size index;
};

//! \name Compare <code>CorrelationFactor</code>s.
//@{
bool operator<(const CorrelationFactor& lhs, const CorrelationFactor& rhs);
bool operator==(const CorrelationFactor& lhs, const CorrelationFactor& rhs);
bool operator!=(const CorrelationFactor& lhs, const CorrelationFactor& rhs);
//@}

//! Allow <code>CorrelationFactor</code>s to be written.
std::ostream& operator<<(std::ostream& out, const CorrelationFactor& f);

/*! Parse a correlation factor \p name. For example, a \p name like \c IR:EUR is parsed to a \c CorrelationFactor with 
    \c type, \c name and \c index set to \c IR, \c EUR and \c 0 respectively. Note that the name is of the form 
    \c type:name and the index is always set to 0 initially. The actual index is set separately.
*/
CorrelationFactor parseCorrelationFactor(const std::string& name, const char separator = ':');

/*! The key for storing the correlation data is the pair of factors.
*/
typedef std::pair<CorrelationFactor, CorrelationFactor> CorrelationKey;

/*! Correlation matrix builder class
    
    Can be loaded with sets of individual correlations as pairs and will build a required correlation matrix.

    \ingroup utilities
*/
class CorrelationMatrixBuilder {
public:
    CorrelationMatrixBuilder() {}

    //! Clear all data
    void reset();

    //! \name Add Correlations
    //@{
    /*! Method to add a correlation between \p factor1 and \p factor2. The factor string is of the form
        \c type:name and it is assumed that the factor belongs to a process driven by one factor. For example,
        \c IR:EUR would refer to the single factor driving the EUR interest rate process.
        
        For processes driven by more than one factor, use the \c addCorrelation methods below that take a 
        \c CorrelationFactor.
    */
    void addCorrelation(const std::string& factor1, const std::string& factor2, QuantLib::Real correlation);
    
    //! \copydoc CorrelationMatrixBuilder::addCorrelation(const std::string&,const std::string&,QuantLib::Real)
    void addCorrelation(const std::string& factor1, const std::string& factor2,
        const QuantLib::Handle<QuantLib::Quote>& correlation);

    //! Add correlation between factor \p f_1 and \p f_2.
    void addCorrelation(const CorrelationFactor& f_1, const CorrelationFactor& f_2, QuantLib::Real correlation);

    //! Add correlation quote between factor \p f_1 and \p f_2.
    void addCorrelation(const CorrelationFactor& f_1, const CorrelationFactor& f_2,
        const QuantLib::Handle<QuantLib::Quote>& correlation);
    //@}

    //! \name Correlation Matrix
    /*! For any of the correlation matrices returned below, if it is asked for a correlation between two factors 
        that has not been added to the matrix builder, it will return 0.0 and not raise an exception.
    */
    //@{
    /*! Return a \f$2n-1\f$ square matrix for an IR/FX model, where \f$n\f$ is the number of currencies in the 
        \p ccys argument. This assumes that \c ccys[0] is the base currency.
    */
    QuantLib::Matrix correlationMatrix(const std::vector<std::string>& ccys);

    /*! Return a \f$2n-1+m\f$ square matrix for an IR/FX/INF model, where \f$n\f$ is the number of currencies in the
        \p ccys argument and \f$m\f$ is the number of inflation indices in the \p infIndices argument. This assumes 
        that \c ccys[0] is the base currency.
    */
    QuantLib::Matrix correlationMatrix(const std::vector<std::string>& ccys,
        const std::vector<std::string>& infIndices);

    /*! Return a \f$2n-1+m+k\f$ square matrix for an IR/FX/INF/CR model, where \f$n\f$ is the number of currencies in 
        the \p ccys argument, \f$m\f$ is the number of inflation indices in the \p infIndices argument and \f$k\f$ is 
        the number of credit names in the \p names argument. This assumes that \c ccys[0] is the base currency.
    */
    QuantLib::Matrix correlationMatrix(const std::vector<std::string>& ccys,
        const std::vector<std::string>& infIndices,
        const std::vector<std::string>& names);

    /*! Return a \f$2n-1+m+k+p\f$ square matrix for an IR/FX/INF/CR/EQ model, where \f$n\f$ is the number of 
        currencies in the \p ccys argument, \f$m\f$ is the number of inflation indices in the \p infIndices argument, 
        \f$k\f$ is the number of credit names in the \p names argument and \f$p\f$ is the number of equity names in 
        the \p equities argument. This assumes that \c ccys[0] is the base currency.
    */
    QuantLib::Matrix correlationMatrix(const std::vector<std::string>& ccys,
        const std::vector<std::string>& infIndices,
        const std::vector<std::string>& names,
        const std::vector<std::string>& equities);

    // TODO: Add commodity

    /*! Build the correlation matrix according to the information provided in \p processInfo. The ProcessInfo map uses 
        the CrossAssetModel asset type as the outer map key and therefore has the correct ordering i.e. IR, FX, etc. 
        For each CrossAssetModel asset type, there is a vector of pairs where the first element in the pair is the 
        name of the factor being modeled and the second element in the pair is the number of factors used in 
        modeling the name. In most cases, the number of factors is 1. The first element in the vector for asset type 
        IR is assumed to be the base currency.

    */
    typedef std::map<QuantExt::CrossAssetModel::AssetType, std::vector<std::pair<std::string, QuantLib::Size>>>
        ProcessInfo;
    QuantLib::Matrix correlationMatrix(const ProcessInfo& processInfo);
    //@}

    //! Get the correlation between two factors
    QuantLib::Handle<QuantLib::Quote> lookup(const std::string& f1, const std::string& f2);

    //! Get the correlation between the factor \p f_1 and \p f_2.
    QuantLib::Handle<QuantLib::Quote> getCorrelation(const CorrelationFactor& f_1, const CorrelationFactor& f_2) const;

    //! Get the raw correlation data
    const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& correlations();

private:
    /*! Create the process information for each of the factors. Legacy method where each process is assumed to be 
        driven by one factor. Used to support the legacy \c correlationMatrix methods above that accept vectors of 
        strings.
    */
    ProcessInfo createProcessInfo(const std::vector<std::string>& ccys,
        const std::vector<std::string>& inflationIndices = {},
        const std::vector<std::string>& creditNames = {},
        const std::vector<std::string>& equityNames = {});

    //! Perform some basic checks on the factor names.
    void checkFactor(const CorrelationFactor& f) const;

    /*! The pair of factors used as the key will always have the first element less than the second element. We use 
        \c createKey to ensure this ordering when adding elements to \c corrs_ and when looking up elements in 
        \c corrs_.
    */
    CorrelationKey createKey(const CorrelationFactor& f_1, const CorrelationFactor& f_2) const;

    //! Store the correlation between two factors.
    std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>> corrs_;
};

} // namespace data
} // namespace ore
