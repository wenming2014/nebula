/* Copyright (c) 2018 - present, VE Software Inc. All rights reserved
 *
 * This source code is licensed under Apache 2.0 License
 *  (found in the LICENSE.Apache file in the root directory)
 */

#include "base/Base.h"
#include "dataman/RowSetReader.h"
#include "dataman/ResultSchemaProvider.h"
#include "dataman/RowReader.h"

namespace nebula {

/***********************************
 *
 * RowSetReader::Iterator class
 *
 **********************************/
RowSetReader::Iterator::Iterator(SchemaProviderIf* schema,
                                 const folly::StringPiece& data,
                                 int64_t offset)
        : schema_(schema)
        , data_(data)
        , offset_(offset) {
    len_ = prepareReader();
}


int32_t RowSetReader::Iterator::prepareReader() {
    if (offset_ < static_cast<int64_t>(data_.size())) {
        try {
            auto begin = reinterpret_cast<const uint8_t*>(data_.begin());
            folly::ByteRange range(begin + offset_, 10);
            int32_t rowLen = folly::decodeVarint(range);
            int32_t lenBytes = range.begin() - begin - offset_;
            reader_.reset(
                new RowReader(schema_,
                              data_.subpiece(offset_ + lenBytes, rowLen)));
            return lenBytes + rowLen;
        } catch (const std::exception& ex) {
            LOG(ERROR) << "Failed to read the row length";
            offset_ = data_.size();
            reader_.reset();
        }
    }

    return 0;
}


const RowReader& RowSetReader::Iterator::operator*() const noexcept {
    return *reader_;
}


const RowReader* RowSetReader::Iterator::operator->() const noexcept {
    return reader_.get();
}


RowSetReader::Iterator& RowSetReader::Iterator::operator++() noexcept {
    offset_ += len_;
    len_ = prepareReader();
    return *this;
}


RowSetReader::Iterator::operator bool() const noexcept {
    return offset_ < static_cast<int64_t>(data_.size());
}


bool RowSetReader::Iterator::operator==(const Iterator& rhs) {
    return schema_ == rhs.schema_ &&
           data_ == rhs.data_ &&
           offset_ == rhs.offset_;
}


/***********************************
 *
 * RowSetReader class
 *
 **********************************/
RowSetReader::RowSetReader(storage::cpp2::QueryResponse& resp)
        : takeOwnership_(true) {
    auto schema = resp.get_schema();
    if (schema) {
        // There is a schema provided in the response
        schema_.reset(new ResultSchemaProvider(std::move(*schema)));
    }

    // If no schema, we cannot decode the data
    if (schema_ && resp.get_data()) {
        dataStore_ = std::move(*resp.get_data());
        data_ = dataStore_;
    }
}


RowSetReader::RowSetReader(SchemaProviderIf* schema,
                           folly::StringPiece data)
        : schema_(schema)
        , takeOwnership_(false)
        , data_(data) {
}


RowSetReader::~RowSetReader() {
    if (!takeOwnership_) {
        // If not taking the ownership of the schame, let's release it
        schema_.release();
    }
}


RowSetReader::Iterator RowSetReader::begin() const noexcept {
    return Iterator(schema_.get(), data_, 0);
}


RowSetReader::Iterator RowSetReader::end() const noexcept {
    return Iterator(schema_.get(), data_, data_.size());
}

}  // namespace nebula
