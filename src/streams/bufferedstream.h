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
#ifndef BUFFEREDSTREAM_H
#define BUFFEREDSTREAM_H

#include "streambase.h"
#include "inputstreambuffer.h"
#include <cassert>

/**
 * The classes in this namespace are meant to support the classes in the Strigi namespace.
 * The namespace jstreams still exists because CLucene uses the same headers.
 */
namespace jstreams {

/**
 * @brief Abstract class providing a buffered input stream.
 */
template <class T>
class BufferedInputStream : public StreamBase<T> {
private:
    bool finishedWritingToBuffer;
    InputStreamBuffer<T> buffer;

    void writeToBuffer(int32_t minsize, int32_t maxsize);
protected:
    /**
     * @brief Fill the buffer with the provided data
     *
     * This function should be implemented by subclasses.
     * It should write up to @p space characters from the
     * stream to the buffer position pointed to by @p start.
     *
     * If the end of the stream is encountered, -1 should be
     * returned.
     *
     * If an error offurs, the status should be set to Error,
     * an error message should be set and -1 should be returned.
     *
     * You should @em not call this function yourself.
     *
     * @param start where the data should be written to
     * @param space the maximum amount of data to write
     * @return Number of characters written, or -1 on error
     **/
    virtual int32_t fillBuffer(T* start, int32_t space) = 0;
    /**
     * @brief Resets the buffer, allowing it to be used again
     *
     * If imlemented, this function will reset the buffer, allowing
     * it to be re-used.
     */
    void resetBuffer() {printf("implement 'resetBuffer'\n");}
    /**
     * @brief Sets the minimum size of the buffer
     */
    void setMinBufSize(int32_t s) {
        buffer.makeSpace(s);
    }
    /** Default constructor just initialises members */
    BufferedInputStream<T>();
public:
    int32_t read(const T*& start, int32_t min, int32_t max);
    int64_t reset(int64_t);
    virtual int64_t skip(int64_t ntoskip);
};

template <class T>
BufferedInputStream<T>::BufferedInputStream() {
    finishedWritingToBuffer = false;
}

template <class T>
void
BufferedInputStream<T>::writeToBuffer(int32_t ntoread, int32_t maxread) {
    int32_t missing = ntoread - buffer.avail;
    int32_t nwritten = 0;
    while (missing > 0 && nwritten >= 0) {
        int32_t space;
        space = buffer.makeSpace(missing);
        if (maxread >= ntoread && space > maxread) {
             space = maxread;
        }
        T* start = buffer.readPos + buffer.avail;
        nwritten = fillBuffer(start, space);
        assert(StreamBase<T>::status != Eof);
        if (nwritten > 0) {
            buffer.avail += nwritten;
            missing = ntoread - buffer.avail;
        }
    }
    if (nwritten < 0) {
        finishedWritingToBuffer = true;
    }
}
template <class T>
int32_t
BufferedInputStream<T>::read(const T*& start, int32_t min, int32_t max) {
    if (StreamBase<T>::status == Error) return -2;
    if (StreamBase<T>::status == Eof) return -1;

    // do we need to read data into the buffer?
    if (min > max) max = 0;
    if (!finishedWritingToBuffer && min > buffer.avail) {
        // do we have enough space in the buffer?
        writeToBuffer(min, max);
        if (StreamBase<T>::status == Error) return -2;
    }

    int32_t nread = buffer.read(start, max);

    StreamBase<T>::position += nread;
    if (StreamBase<T>::position > StreamBase<T>::size
        && StreamBase<T>::size > 0) {
        // error: we read more than was specified in size
        // this is an error because all dependent code might have been labouring
        // under a misapprehension
        StreamBase<T>::status = Error;
        StreamBase<T>::error = "Stream is longer than specified.";
        nread = -2;
    } else if (StreamBase<T>::status == Ok && buffer.avail == 0
            && finishedWritingToBuffer) {
        StreamBase<T>::status = Eof;
        if (StreamBase<T>::size == -1) {
            StreamBase<T>::size = StreamBase<T>::position;
        }
        // save one call to read() by already returning -1 if no data is there
        if (nread == 0) nread = -1;
    }
    return nread;
}
template <class T>
int64_t
BufferedInputStream<T>::reset(int64_t newpos) {
    assert(newpos >= 0);
    if (StreamBase<T>::status == Error) return -2;
    // check to see if we have this position
    int64_t d = StreamBase<T>::position - newpos;
    if (buffer.readPos - d >= buffer.start && -d < buffer.avail) {
        StreamBase<T>::position -= d;
        buffer.avail += (int32_t)d;
        buffer.readPos -= d;
        StreamBase<T>::status = Ok;
    }
    return StreamBase<T>::position;
}
template <class T>
int64_t
BufferedInputStream<T>::skip(int64_t ntoskip) {
    const T *begin;
    int32_t nread;
    int64_t skipped = 0;
    while (ntoskip) {
        int32_t step = (int32_t)((ntoskip > buffer.size) ?buffer.size :ntoskip);
        nread = read(begin, 1, step);
        if (nread <= 0) {
            return skipped;
        }
        ntoskip -= nread;
        skipped += nread;
    }
    return skipped;
}
}

#endif
