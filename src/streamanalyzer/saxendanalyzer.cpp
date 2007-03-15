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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "jstreamsconfig.h"
#include "saxendanalyzer.h"
#include "streamanalyzer.h"
#include "inputstreamreader.h"
#include "indexwriter.h"
#include "analysisresult.h"
#include "fieldtypes.h"
#include "textutils.h"
#include <libxml/parser.h>
#include <ctype.h>
using namespace jstreams;
using namespace Strigi;
using namespace std;

const cnstr SaxEndAnalyzerFactory::titleFieldName("title");
const cnstr SaxEndAnalyzerFactory::encodingFieldName("encoding");
const cnstr SaxEndAnalyzerFactory::rootFieldName("root");

void
SaxEndAnalyzerFactory::registerFields(FieldRegister& reg) {
    titleField = reg.registerField(titleFieldName, FieldRegister::stringType,
        -1, 0);
    encodingField = reg.registerField(encodingFieldName,
        FieldRegister::stringType, 1, 0);
    rootField = reg.registerField(rootFieldName, FieldRegister::stringType,
        1, 0);
}

class SaxEndAnalyzer::Private {
public:
    enum FieldType { NONE, TEXT, TITLE };
    string fieldvalue;
    FieldType fieldtype;
    xmlParserCtxtPtr ctxt;
    xmlSAXHandler handler;
    AnalysisResult* idx;
    const SaxEndAnalyzerFactory* factory;
    bool error;
    bool stop;
    string rootelement;
    int32_t chars;

    static void charactersSAXFunc(void* ctx, const xmlChar * ch, int len);
    static void errorSAXFunc(void* ctx, const char * msg, ...);
    static void startElementSAXFunc(void * ctx, const xmlChar * name,
        const xmlChar ** atts);
    static void endElementSAXFunc(void * ctx, const xmlChar * name);
    static void startElementNsSAX2Func(void * ctx,
        const xmlChar* localname, const xmlChar* prefix, const xmlChar* URI,
        int nb_namespaces, const xmlChar ** namespaces, int nb_attributes,
        int nb_defaulted, const xmlChar ** attributes);

    Private() {
        ctxt = 0;
        memset(&handler, 0, sizeof(xmlSAXHandler));
        handler.characters = charactersSAXFunc;
        handler.error = errorSAXFunc;
        handler.startElement = startElementSAXFunc;
        handler.endElement = endElementSAXFunc;
        handler.startElementNs = startElementNsSAX2Func;
        fieldtype = TEXT;
    }
    ~Private() {
        reset();
    }
    void reset() {
        if (ctxt) {
            xmlFreeParserCtxt(ctxt);
            ctxt = 0;
        }
        error = false;
        stop = false;
        chars = 0;
        rootelement = "";
    }
    void init(AnalysisResult*i, const char* data, int32_t len) {
        reset();
        int initlen = (1024 > len) ?len :1024;
        idx = i;
        const char* name = 0;
        if (i) name = i->fileName().c_str();
        xmlKeepBlanksDefault(0);
        ctxt = xmlCreatePushParserCtxt(&handler, this, data, initlen, name);
        if (ctxt == 0) {
            error = true;
            stop = true;
        } else {
//            ctxt->sax2 = 1;
            // we need to call push once to do validation
            push(data+initlen, len-initlen);
        }
    }
    void push(const char* data, int32_t len) {
        xmlParseChunk(ctxt, data, len, 0);
    }
    void finish() {
        xmlParseChunk(ctxt, 0, 0, 1);
    }
};
void
SaxEndAnalyzer::Private::charactersSAXFunc(void* ctx, const xmlChar * ch,
        int len) {
    Private* p = (Private*)ctx;

    // skip whitespace
    const char* end = (const char*)ch+len;
    const char* c = (const char*)ch;
    while (c < end && isspace(*c)) c++;
    if (c == end) return;

    if (p->idx && p->fieldtype != NONE && checkUtf8((const char*)c, end-c)) {
        if (p->fieldtype == TEXT) {
            p->idx->addText((const char*)c, end-c);
        } else {
            p->fieldvalue += string((const char*)c, end-c);
        }
    }
    p->chars += end-c;
    if (p->chars > 1000000) {
        p->stop = true;
    }
}
#include <iostream>
void
SaxEndAnalyzer::Private::errorSAXFunc(void* ctx, const char* msg, ...) {
    Private* p = (Private*)ctx;
    p->stop = p->error = true;
    string e;

    va_list args;
    va_start(args, msg);
    e += string(" ")+va_arg(args,char*);
    va_end(args);
//    fprintf(stderr, "%s", e.c_str());
}
void
SaxEndAnalyzer::Private::startElementNsSAX2Func(void * ctx,
        const xmlChar* localname, const xmlChar* prefix, const xmlChar* URI,
        int nb_namespaces, const xmlChar ** namespaces, int nb_attributes,
        int nb_defaulted, const xmlChar ** attributes) {
    Private* p = (Private*)ctx;
    if(URI && p->rootelement.size() == 0) {
        p->rootelement = (const char*)URI;
    }
}
void
SaxEndAnalyzer::Private::startElementSAXFunc(void* ctx, const xmlChar* name,
        const xmlChar** atts) {
    Private* p = (Private*)ctx;
    if(name && p->rootelement.size() == 0) {
        p->rootelement = (const char*)name;
    }
    if (strcasecmp((const char*)name, "title") == 0) {
        p->fieldtype = TITLE;
        p->fieldvalue = "";
    }
}
void
SaxEndAnalyzer::Private::endElementSAXFunc(void* ctx, const xmlChar* name) {
    Private* p = (Private*)ctx;
    if (p->idx && p->fieldtype == TITLE && p->fieldvalue.size()) {
        p->idx->setField(p->factory->titleField, p->fieldvalue);
        p->fieldvalue = "";
    }
    p->fieldtype = TEXT;
}
SaxEndAnalyzer::SaxEndAnalyzer(const SaxEndAnalyzerFactory* f) {
    p = new Private();
    p->factory = f;
}
SaxEndAnalyzer::~SaxEndAnalyzer() {
    delete p;
}

bool
SaxEndAnalyzer::checkHeader(const char* header, int32_t headersize) const {
    p->init(0, header, headersize);
    return !p->error;
}

char
SaxEndAnalyzer::analyze(AnalysisResult& idx, InputStream* in) {
    const char* b;
    int32_t nread = in->read(b, 4, 0);
    if (nread >= 4) {
        p->init(&idx, b, nread);
        nread = in->read(b, 1, 0);
    }
    while (nread > 0 && !p->stop) {
        p->push(b, nread);
        nread = in->read(b, 1, 0);
    }
    p->finish();
    if (p->ctxt->encoding) {
        idx.setField(p->factory->encodingField, (const char*)p->ctxt->encoding);
    }
    idx.setMimeType("text/xml");
    idx.setField(p->factory->rootField, p->rootelement);
    if (in->getStatus() != Eof) {
        error = in->getError();
        return -1;
    }
    return 0;
}