#ifndef INDEXWRITER_H
#define INDEXWRITER_H

#include <string>

namespace jstreams {

template <class T>
class StreamBase;

/*
- create indexwriter
for all streams {
 - create an indexable
 - add the indexwriter to it
 - add a stream to the indexable (optional)
 - add fields to indexable (optional)
 - delete the indexable
}
- delete the indexwriter
*/
class Indexable;
class IndexWriter {
friend class Indexable;
protected:
    virtual void startIndexable(Indexable*) = 0;
    virtual void addStream(const Indexable*, const std::string& fieldname,
        jstreams::StreamBase<wchar_t>* datastream) = 0;
    virtual void addField(const Indexable*, const std::string &fieldname,
        const std::string& value) = 0;
    virtual void setField(const Indexable*, const std::string &fieldname,
        int64_t value) = 0;
    virtual void finishIndexable(const Indexable*) = 0;
    void setIndexed(Indexable* idx, bool indexed);
public:
    virtual ~IndexWriter() {}
    virtual void commit() { return; }
    virtual void deleteEntry(const std::string& entry) = 0;
    virtual int itemsInCache() { return 0; };
};

class Indexable {
friend class IndexWriter;
private:
    int64_t id;
    const int64_t mtime;
    const std::string& name;
    IndexWriter* writer;
    char depth;
    bool wasindexed;
public:
    Indexable(const std::string& n, int64_t mt, IndexWriter* w, char d)
            :mtime(mt), name(n), writer(w), depth(d) {
        wasindexed = true; // don't index per default
        w->startIndexable(this);
    }
    ~Indexable() { writer->finishIndexable(this); }
    void addStream(const std::string& fieldname,
            jstreams::StreamBase<wchar_t>* datastream) {
        writer->addStream(this, fieldname, datastream);
    }
    void addField(const std::string &fieldname,
            const std::string &value) {
        writer->addField(this, fieldname, value);
    }
    const std::string& getName() const { return name; }
    int64_t getMTime() const { return mtime; }
    void setId(int64_t i) { id = i; }
    int64_t getId() const { return id; }
    bool wasIndexed() const { return wasindexed; }
    char getDepth() const { return depth; }
};
inline void
IndexWriter::setIndexed(Indexable* idx, bool indexed) {
    idx->wasindexed = indexed;
}

}

#endif
