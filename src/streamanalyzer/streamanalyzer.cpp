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
#include "jstreamsconfig.h"
#include "streamanalyzer.h"
#include "fileinputstream.h"
#include "streamendanalyzer.h"
#include "streamthroughanalyzer.h"
#include "streamlineanalyzer.h"
#include "streameventanalyzer.h"
#include "streamsaxanalyzer.h"
#include "bz2endanalyzer.h"
#include "bmpendanalyzer.h"
#include "textendanalyzer.h"
#include "saxendanalyzer.h"
#include "tarendanalyzer.h"
#include "arendanalyzer.h"
#include "zipendanalyzer.h"
#include "rpmendanalyzer.h"
#include "cpioendanalyzer.h"
#include "pngendanalyzer.h"
#include "gzipendanalyzer.h"
#include "mailendanalyzer.h"
#include "helperendanalyzer.h"
#include "dataeventinputstream.h"
#include "id3v2throughanalyzer.h"
#include "oggthroughanalyzer.h"
#include "digestthroughanalyzer.h"
#include "analysisresult.h"
#include "indexwriter.h"
#include "analyzerconfiguration.h"
#include "textutils.h"
#include "analyzerloader.h"
#include "eventthroughanalyzer.h"
#include "htmlsaxanalyzer.h"
#include <sys/stat.h>
#ifdef WIN32
 #include "ifilterendanalyzer.h"
#endif
#include <config.h>

using namespace std;
using namespace jstreams;
using namespace Strigi;

class StreamAnalyzer::Private {
public:
    AnalyzerConfiguration& conf;
    vector<StreamThroughAnalyzerFactory*> throughfactories;
    vector<StreamEndAnalyzerFactory*> endfactories;
    vector<StreamSaxAnalyzerFactory*> saxfactories;
    vector<StreamLineAnalyzerFactory*> linefactories;
    vector<StreamEventAnalyzerFactory*> eventfactories;
    vector<vector<StreamEndAnalyzer*> > end;
    vector<vector<StreamThroughAnalyzer*> > through;
    IndexWriter* writer;

    AnalyzerLoader* moduleLoader;
    const RegisteredField* sizefield;
    void initializeThroughFactories();
    void initializeEndFactories();
    void initializeSaxFactories();
    void initializeLineFactories();
    void initializeEventFactories();
    void addFactory(StreamThroughAnalyzerFactory* f);
    void addFactory(StreamEndAnalyzerFactory* f);
    void addFactory(StreamSaxAnalyzerFactory* f);
    void addFactory(StreamLineAnalyzerFactory* f);
    void addFactory(StreamEventAnalyzerFactory* f);
    void addThroughAnalyzers();
    void addEndAnalyzers();
    void removeIndexable(unsigned depth);
    char analyze(AnalysisResult& idx, StreamBase<char>* input);

    Private(AnalyzerConfiguration& c);
    ~Private();
};

StreamAnalyzer::Private::Private(AnalyzerConfiguration& c) :conf(c), writer(0) {
    moduleLoader = new AnalyzerLoader();
    sizefield = c.getFieldRegister().registerField("size",
        FieldRegister::integerType, 1, 0);

    moduleLoader->loadPlugins( LIBINSTALLDIR "/strigi");
    
    initializeThroughFactories();
    initializeEndFactories();
}
StreamAnalyzer::Private::~Private() {
    // delete all factories
    vector<StreamThroughAnalyzerFactory*>::iterator ta;
    for (ta = throughfactories.begin(); ta != throughfactories.end(); ++ta) {
        delete *ta;
    }
    vector<StreamEndAnalyzerFactory*>::iterator ea;
    for (ea = endfactories.begin(); ea != endfactories.end(); ++ea) {
        delete *ea;
    }
    vector<StreamSaxAnalyzerFactory*>::iterator sa;
    for (sa = saxfactories.begin(); sa != saxfactories.end(); ++ea) {
        delete *sa;
    }
    vector<StreamLineAnalyzerFactory*>::iterator la;
    for (la = linefactories.begin(); la != linefactories.end(); ++la) {
        delete *la;
    }
    vector<StreamEventAnalyzerFactory*>::iterator da;
    for (da = eventfactories.begin(); da != eventfactories.end(); ++da) {
        delete *da;
    }
    // delete the through analyzers and end analyzers
    vector<vector<StreamThroughAnalyzer*> >::iterator tIter;
    for (tIter = through.begin(); tIter != through.end(); ++tIter) {
        vector<StreamThroughAnalyzer*>::iterator t;
        for (t = tIter->begin(); t != tIter->end(); ++t) {
            delete *t;
        }
    }
    vector<vector<StreamEndAnalyzer*> >::iterator eIter;
    for (eIter = end.begin(); eIter != end.end(); ++eIter) {
        vector<StreamEndAnalyzer*>::iterator e;
        for (e = eIter->begin(); e != eIter->end(); ++e) {
            delete *e;
        }
    }
    delete moduleLoader;
    if (writer != 0) {
        writer->releaseWriterData(conf.getFieldRegister());
    }
}

StreamAnalyzer::StreamAnalyzer(AnalyzerConfiguration& c)
        :p(new Private(c)) {
}
StreamAnalyzer::~StreamAnalyzer() {
    delete p;
}
void
StreamAnalyzer::setIndexWriter(IndexWriter& w) {
    if (p->writer != 0) {
        p->writer->releaseWriterData(p->conf.getFieldRegister());
    }
    p->writer = &w;
    p->writer->initWriterData(p->conf.getFieldRegister());
}
char
StreamAnalyzer::indexFile(const char *filepath) {
    string path(filepath);
    return indexFile(path);
}
char
StreamAnalyzer::indexFile(const string& filepath) {
    if (!checkUtf8(filepath.c_str())) {
        return 1;
    }
    if (p->writer == 0) {
        return 1;
    }
    struct stat s;
    stat(filepath.c_str(), &s);
    // ensure a decent buffer size
    string name;
    AnalysisResult analysisresult(filepath, s.st_mtime, *p->writer, *this);
    FileInputStream file(filepath.c_str());
    if (file.getStatus() == Ok) {
        return analysisresult.index(&file);
    } else {
        return analysisresult.index(0);
    }
}
void
StreamAnalyzer::Private::addFactory(StreamThroughAnalyzerFactory* f) {
    f->registerFields(conf.getFieldRegister());
    if (conf.useFactory(f)) {
        throughfactories.push_back(f);
    } else {
        delete f;
    }
}
void
StreamAnalyzer::Private::initializeSaxFactories() {
    list<StreamSaxAnalyzerFactory*> plugins
        = moduleLoader->getStreamSaxAnalyzerFactories();
    list<StreamSaxAnalyzerFactory*>::iterator i;
    for (i = plugins.begin(); i != plugins.end(); ++i) {
        addFactory(*i);
    }
    addFactory(new HtmlSaxAnalyzerFactory());
}
void
StreamAnalyzer::Private::initializeThroughFactories() {
    list<StreamThroughAnalyzerFactory*> plugins
        = moduleLoader->getStreamThroughAnalyzerFactories();
    list<StreamThroughAnalyzerFactory*>::iterator i;
    for (i = plugins.begin(); i != plugins.end(); ++i) {
        addFactory(*i);
    }
    //addFactory(new DigestThroughAnalyzerFactory());
    addFactory(new ID3V2ThroughAnalyzerFactory());
    addFactory(new OggThroughAnalyzerFactory());
    addFactory(new EventThroughAnalyzerFactory(saxfactories, linefactories,
        eventfactories));
}
void
StreamAnalyzer::Private::addFactory(StreamSaxAnalyzerFactory* f) {
    f->registerFields(conf.getFieldRegister());
    if (conf.useFactory(f)) {
        saxfactories.push_back(f);
    } else {
        delete f;
    }
}
void
StreamAnalyzer::Private::addFactory(StreamEndAnalyzerFactory* f) {
    f->registerFields(conf.getFieldRegister());
    if (conf.useFactory(f)) {
        endfactories.push_back(f);
    } else {
        delete f;
    }
}
/**
 * Instantiate factories for all analyzers.
 **/
void
StreamAnalyzer::Private::initializeEndFactories() {
    list<StreamEndAnalyzerFactory*> plugins
        = moduleLoader->getStreamEndAnalyzerFactories();
    list<StreamEndAnalyzerFactory*>::iterator i;
    for (i = plugins.begin(); i != plugins.end(); ++i) {
        fprintf(stderr, "adding %s\n", (*i)->getName());
        addFactory(*i);
    }
    addFactory(new Bz2EndAnalyzerFactory());
    addFactory(new GZipEndAnalyzerFactory());
    addFactory(new TarEndAnalyzerFactory());
    addFactory(new ArEndAnalyzerFactory());
    addFactory(new MailEndAnalyzerFactory());
    addFactory(new ZipEndAnalyzerFactory());
    addFactory(new RpmEndAnalyzerFactory());
    addFactory(new CpioEndAnalyzerFactory());
    addFactory(new PngEndAnalyzerFactory());
    addFactory(new BmpEndAnalyzerFactory());
//    addFactory(new PdfEndAnalyzerFactory());
#ifdef WIN32
#ifdef __GNUC__
#warning FIXME - IFilterEndAnalyzerFactory is pure virtual!
//    addFactory(new IFilterEndAnalyzerFactory());
#endif
#else
	//temporary only, i just haven't got expat.h working yet
	addFactory(new SaxEndAnalyzerFactory());
#endif
    addFactory(new HelperEndAnalyzerFactory());
    addFactory(new TextEndAnalyzerFactory());
}
void
StreamAnalyzer::Private::addThroughAnalyzers() {
    through.resize(through.size()+1);
    vector<vector<StreamThroughAnalyzer*> >::reverse_iterator tIter;
    tIter = through.rbegin();
    vector<StreamThroughAnalyzerFactory*>::iterator ta;
    for (ta = throughfactories.begin(); ta != throughfactories.end(); ++ta) {
        tIter->push_back((*ta)->newInstance());
    }
}
void
StreamAnalyzer::Private::addEndAnalyzers() {
    end.resize(end.size()+1);
    vector<vector<StreamEndAnalyzer*> >::reverse_iterator eIter;
    eIter = end.rbegin();
    vector<StreamEndAnalyzerFactory*>::iterator ea;
    for (ea = endfactories.begin(); ea != endfactories.end(); ++ea) {
        eIter->push_back((*ea)->newInstance());
    }
}
char
StreamAnalyzer::analyze(AnalysisResult& idx, StreamBase<char>* input) {
    return p->analyze(idx, input);
}
char
StreamAnalyzer::Private::analyze(AnalysisResult& idx, StreamBase<char>* input) {
//    static int count = 1;
//    if (++count % 1000 == 0) {
//        fprintf(stderr, "file #%i: %s\n", count, path.c_str());
//    }
    //printf("depth #%i: %s\n", depth, path.c_str());

    // retrieve or construct the through analyzers and end analyzers
    vector<vector<StreamThroughAnalyzer*> >::iterator tIter;
    vector<vector<StreamEndAnalyzer*> >::iterator eIter;
    while ((int)through.size() <= idx.depth()) {
        addThroughAnalyzers();
        addEndAnalyzers();
    }
    tIter = through.begin() + idx.depth();
    eIter = end.begin() + idx.depth();

    // insert the through analyzers
    vector<StreamThroughAnalyzer*>::iterator ts;
    for (ts = tIter->begin(); ts != tIter->end(); ++ts) {
        (*ts)->setIndexable(&idx);
        input = (*ts)->connectInputStream(input);
    }
    bool finished = false;
    int32_t headersize = 1024;
    const char* header;
    if (input) {
        headersize = input->read(header, headersize, headersize);
        if (input->reset(0) != 0) {
            fprintf(stderr, "resetting is impossible!! pos: %lli status: %i\n",
                input->getPosition(), input->getStatus());
        }
        if (headersize < 0) finished = true;
    } else {
        // indicate that we have no data in the stream
        headersize = -1;
    }
    int es = 0, size = eIter->size();
    while (!finished && es != size) {
        StreamEndAnalyzer* sea = (*eIter)[es];
        if (sea->checkHeader(header, headersize)) {
            char ar = sea->analyze(idx, input);
            if (ar) {
                if (input) {
                    int64_t pos = input->reset(0);
                    if (pos != 0) { // could not reset
                        fprintf(stderr, "could not reset stream of %s from pos "
                            "%lli to 0 after reading with %s: %s\n",
                            idx.path().c_str(), input->getPosition(),
                            sea->getName(), sea->getError().c_str());
                        finished = true;
                    }
                }
            } else {
                finished = true;
            }
            eIter = end.begin() + idx.depth();
        }
        es++;
    }
    if (input) {
        // make sure the entire stream is read if the size is not known
        bool ready;
        tIter = through.begin() + idx.depth();
        do {
            ready = input->getSize() != -1;
            vector<StreamThroughAnalyzer*>::iterator ts;
            for (ts = tIter->begin(); ready && ts != tIter->end(); ++ts) {
                ready = (*ts)->isReadyWithStream();
            }
            if (!ready) {
                input->skip(1000000);
            }
        } while (!ready && input->getStatus() == Ok);
        if (input->getStatus() == Error) {
            fprintf(stderr, "Error: %s\n", input->getError());
            removeIndexable(idx.depth());
            return -2;
        }
    }

    // store the size of the stream
    if (input) {
        // TODO remove cast
        idx.setField(sizefield, (uint32_t)input->getSize());
    }

    // remove references to the analysisresult before it goes out of scope
    removeIndexable(idx.depth());
    return 0;
}
/**
 * Remove references to the analysisresult before it goes out of scope.
 **/
void
StreamAnalyzer::Private::removeIndexable(uint depth) {
    vector<vector<StreamThroughAnalyzer*> >::iterator tIter;
    vector<StreamThroughAnalyzer*>::iterator ts;
    tIter = through.begin() + depth;
    for (ts = tIter->begin(); ts != tIter->end(); ++ts) {
        // remove references to the analysisresult before it goes out of scope
        (*ts)->setIndexable(0);
    }
}
AnalyzerConfiguration&
StreamAnalyzer::getConfiguration() const {
    return p->conf;
}
