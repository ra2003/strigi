/* This file is part of Strigi Desktop Search
 *
 * Copyright (C) 2006 Jos van den Oever <jos@vandenoever.info>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifndef INDEXREADER_H
#define INDEXREADER_H

#include "indexeddocument.h"
#include <vector>

namespace jstreams {
class Query;
class IndexReader {
public:
    virtual ~IndexReader() {}
    virtual int32_t countHits(const Query&) = 0;
    virtual std::vector<IndexedDocument> query(const Query&) = 0;
    virtual std::map<std::string, time_t> getFiles(char depth) = 0;
    virtual int32_t countDocuments() { return -1; }
    virtual int32_t countWords() { return -1; }
    virtual int64_t getIndexSize() { return -1; }
    virtual int64_t getDocumentId(const std::string& uri) = 0;
    virtual time_t getMTime(int64_t docid) = 0;
};

}

#endif
