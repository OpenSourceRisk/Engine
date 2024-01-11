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

/*! \file orea/engine/sensitivityfilestream.hpp
    \brief Class for streaming SensitivityRecords from file
 */

#pragma once

#include <orea/engine/sensitivitystream.hpp>

#include <fstream>
#include <string>

namespace ore {
namespace analytics {

//! Class for streaming SensitivityRecords from csv file
class SensitivityInputStream : public SensitivityStream {
public:
    //! Constructor
    SensitivityInputStream(char delim = ',', const std::string& comment = "#") 
        : delim_(delim), comment_(comment), lineNo_(0) {}
    virtual ~SensitivityInputStream() {} // Declare virtual destructor
        
    /*! Set stream for function */
    void setStream(std::istream* stream);
    //! Returns the next SensitivityRecord in the stream
    SensitivityRecord next() override;
    //! Resets the stream so that SensitivityRecord objects can be streamed again
    void reset() override;

private:
    //! Handle on the stram
    std::istream* stream_;
    //! Csv file delimiter
    char delim_;
    //! Csv file comment string
    std::string comment_;
    //! Keep track of line number for messages
    QuantLib::Size lineNo_;

    //! Create a record from a collection of strings
    SensitivityRecord processRecord(const std::vector<std::string>& entries) const;
};

class SensitivityFileStream : public SensitivityInputStream {
public:
    //! Constructor providing path to csv file \p fileName
    SensitivityFileStream(const std::string& fileName, char delim = ',', const std::string& comment = "#");

    ~SensitivityFileStream() override;
    
private:
    std::ifstream* file_;
};

class SensitivityBufferStream : public SensitivityInputStream {
public:
    //! Constructor providing path to csv file \p fileName
    SensitivityBufferStream(const std::string& buffer, char delim = ',', const std::string& comment = "#");
};

} // namespace analytics
} // namespace ore
