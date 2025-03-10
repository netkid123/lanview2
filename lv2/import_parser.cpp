#include <math.h>
#include "lanview.h"
#include "others.h"
#undef  __MODUL_NAME__
#define __MODUL_NAME__  PARSER
#include "import_parser.h"

QString      importFileNm;
unsigned int importLineNo = 0;
QTextStream* pImportInputStream = nullptr;

bool importSrcOpen(QFile& f)
{
    if (f.exists()) return f.open(QIODevice::ReadOnly);
    QString fn = f.fileName();
    QDir    d(lanView::getInstance()->homeDir);
    if (!d.exists(fn)) {
        EXCEPTION(ENOTFILE, -1, fn);
    }
    f.setFileName(d.filePath(fn));
    return f.open(QIODevice::ReadOnly);
}

int importParseText(QString text)
{
    importFileNm = "[stream]";
    pImportInputStream = new QTextStream(&text);
    PDEB(INFO) << QObject::tr("Start parser ...") << endl;
    initImportParser();
    int r = importParse();
    downImportParser();
    PDEB(INFO) << QObject::tr("End parser.") << endl;
    pDelete(pImportInputStream);
    return r;
}

int importParseFile(const QString& fn)
{
    QFile in(fn);
    importFileNm = fn;
    if (fn == QChar('-') || fn == _sStdin) {
        importFileNm = _sStdin;
        pImportInputStream = new QTextStream(stdin, QIODevice::ReadOnly);
    }
    else {
        if (!importSrcOpen(in)) EXCEPTION(EFOPEN, -1, in.fileName());
        pImportInputStream = new QTextStream(&in);
        importFileNm = in.fileName();
    }
    PDEB(INFO) << QObject::tr("Start parser ...") << endl;
    initImportParser();
    int r = importParse();
    downImportParser();
    PDEB(INFO) << QObject::tr("End parser.") << endl;
    pDelete(pImportInputStream);
    return r;
}

cError *importGetLastError() {
    cError *r = pImportLastError;
    pImportLastError = nullptr;
    return r;
}

cImportParseThread *cImportParseThread::pInstance = nullptr;

cImportParseThread::cImportParseThread(const QString& _inicmd, QObject *par)
    : QThread(par)
    , queueAccess(1)    // A queue szabad
    , dataReady(0)      // A Queue üres
    , parseReady(0)     // A parser szabad, de nincs adat
    , queue()
    , iniCmd(_inicmd)
{
    DBGOBJ();
    srcType = ST_QUEUE;
    if (pInstance != nullptr) EXCEPTION(EPROGFAIL);
    pInstance = this;
    setObjectName(_sImportParser);
}

cImportParseThread::~cImportParseThread()
{
    DBGOBJ();
    stopParser();
    pInstance = nullptr;
}

void cImportParseThread::reset()
{
    DBGFN();
    queue.clear();
    if (0 != dataReady.available()) {
        if (!dataReady.tryAcquire(dataReady.available())) EXCEPTION(ESEM);
    }
    switch (queueAccess.available()) {
    case 1:                          break;
    case 0: queueAccess.release();   break;
    default: EXCEPTION(ESEM);
    }
    if (0 != parseReady.available()) {
        if (!parseReady.tryAcquire(parseReady.available())) EXCEPTION(ESEM);
    }
}

void	cImportParseThread::run()
{
    // printf("*********************\n");
    DBGFN();
    // Alaphelyzetbe állítjuk a sorpuffert, és a szemforokat
    reset();
    if (importParserStat != IPS_READY || pImportInputStream != nullptr) {
        if (pImportLastError == nullptr) pImportLastError = NEWCERROR(ESTAT);
        DERR() << VEDEB(importParserStat, ipsName) << _sCommaSp << VDEBPTR(pImportInputStream) << endl;
        return;
    }
    PDEB(INFO) << QObject::tr("Start parser (thread) ...") << endl;
    initImportParser();
    switch (srcType) {
    case ST_SRING:
        importFileNm = "[stream]";
        pImportInputStream = new QTextStream(&src);
        PDEB(VERBOSE) << QObject::tr("Stop parser (thread/stream) ...") << endl;
        importParse(IPS_THREAD);
        break;
    case ST_QUEUE:
        importFileNm = "[queue]";
        importParse(IPS_THREAD);
        PDEB(VERBOSE) << QObject::tr("Stop parser (thread/queue) ...") << endl;
        if (parseReady.available() == 0) parseReady.release();
        else EXCEPTION(EPROGFAIL);
        break;
    }
    downImportParser();
    PDEB(INFO) << QObject::tr("End parser ((thread)).") << endl;
    pDelete(pImportInputStream);
    DBGFNL();
}

/// Forrás szöveg küldése a parsernek.
/// @param src A lefordítandó szöveg.
/// @param pe Hiba objektum pointer referencia. Hiba esetén a keletkezett hiba objektum pointere kerül bele.
/// @return a hiba típusa. Ha nem REASON_OK, akkor a hiba objektum pointerét a pe-ben kapjuk vissza, amit a hívónak kell felszabadítani.
int cImportParseThread::push(const QString& src, cError *& pe, int _to)
{
    _DBGFN() << src << endl;
    int r;
    if (isRunning()) {
        if (parseReady.available() > 0) parseReady.acquire(parseReady.available());
        queueAccess.acquire();              // puffer hazzáférés foglalt
        queue.enqueue(src);                 // küld
        dataReady.release();                // pufferben adat
        queueAccess.release();              // puffer hazzáférés szabad
        PDEB(VVERBOSE) << "parseReady.tryAcquire() ..." << endl;
        bool b = parseReady.tryAcquire(1, _to);
        PDEB(VVERBOSE) << "parseReady.tryAcquire() return " << b << endl;
        if (b) {
            pe = importGetLastError();
            r = pe == nullptr ? REASON_OK : R_ERROR;
        }
        else {
            pe = importGetLastError();                      // Időtúllépés
            if (nullptr == pe) pe = NEWCERROR(ETO);
            r = R_TIMEOUT;
        }
    }
    else {
        pe = importGetLastError();
        if (pe == nullptr) pe = NEWCERROR(EUNKNOWN, -1, tr("A parser szál nem fut."));
        r = R_DISCARD;
    }
    if (REASON_OK != r) {
        cError *rpe = nullptr;
        reStartParser(rpe);
        if (rpe != nullptr) {
            DERR() << rpe->msg() << endl;
            if (pe != nullptr) {
                rpe->pErrorNested = pe;
            }
            pe = rpe;
        }
    }
    if (pe == nullptr) {
        _DBGFNL() << r << endl;
    }
    else {
        _DBGFNL() << "LAST ERROR : " << pe->msg() << endl;
    }
    return r;
}

QString cImportParseThread::pop()
{
    QString r;
    DBGFN();
    if (parseReady.available() == 0) {
        PDEB(VERBOSE) << "parseReady.release() ..." << endl;
        parseReady.release();
    }
    dataReady.acquire();
    queueAccess.acquire();
    if (!queue.isEmpty()) r = queue.dequeue();
    queueAccess.release();
    _DBGFNL() << _sSpace << quotedString(r) << endl;
    return r;
}

int cImportParseThread::startParser(cError *&pe, const QString *pSrc)
{
    PDEB(INFO) << "Start parser ... pSrc = " << (pSrc == nullptr ? _sNULL : quotedString(*pSrc)) << endl;
    pe = nullptr;
    if (isRunning()) {
        pe = NEWCERROR(ECONTEXT);
        return R_ERROR;
    }
    if (pSrc == nullptr) {
        srcType = ST_QUEUE;
    }
    else {
        srcType = ST_SRING;
        if (pSrc->isEmpty()) src = *pSrc;
        else                 src = iniCmd + "\n" + *pSrc;
    }
    int r = R_IN_PROGRESS;
    start();
    yieldCurrentThread();   // Help debug
    switch (srcType) {
    case ST_QUEUE:
        if (!iniCmd.isEmpty()) {
            int rr = push(iniCmd, pe);
            if (rr != REASON_OK) r = rr;
        }
        break;
    case ST_SRING:
        break;
    }
    DBGFNL();
    return r;
}

int cImportParseThread::reStartParser(cError *&pe)
{
    PDEB(INFO) << "Restart parser ..." << endl;
    stopParser();
    return startParser(pe);
}

void cImportParseThread::stopParser()
{
    DBGFN();
    if (isRunning()) {
        if (!queueAccess.tryAcquire(1, IPT_SHORT_WAIT)) {
            DERR() << "Ignore blocked access semaphor." << endl;
        }
        queue.clear();
        queueAccess.release();
        dataReady.release();
        if (!wait(IPT_LONG_WAIT)) {
            setTerminationEnabled();
            DERR() << "Import parser thread, nothing exited." << endl;
            terminate();
            if (!wait(IPT_LONG_WAIT)) {
                DERR() << "Import parser thread, nothing terminated." << endl;
            }
            downImportParser();
        }
    }
    DBGFNL();
}

