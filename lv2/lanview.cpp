#include "lanview.h"
#include "lv2data.h"
#include "srvdata.h"
#include "lv2daterr.h"
#include "lv2user.h"
#include "lv2service.h"
#include "guidata.h"
#include <QCoreApplication>

#if   (0-REVISION-1)==1
# define REVISION X
#endif

#define VERSION_MAJOR   1
#define VERSION_MINOR   00
#define VERSION_STR     _STR(VERSION_MAJOR) "." _STR(VERSION_MINOR) "(" _STR(REVISION) ")"

#define DB_VERSION_MAJOR 1
#define DB_VERSION_MINOR 30

// ****************************************************************************************************************
int findArg(char __c, const char * __s, int argc, char * argv[])
{
    for (int i = 1; i < argc; ++i) {
        const char * arg = argv[i];
        size_t s = strlen(arg);
        if (s < 2) continue;    // 2 betünél kisebb kapcsoló nem lehet
        if (arg[0] == '-') {    // A kapcsoló '-'-al kezdődik
            if (arg[1] == '-') {// Ha '--' -al kezdődik
                if (s == 2) {       // a '--' az argumentumok vége
                    if (0 == strcmp(__s, __sMinusMinus)) return -1; // épp ezt kerestük
                    break;                                          // ha vége, akkor vége
                }
                if (0 == strcmp(__s, arg + 2)) {
                    return i;    // "hosszú" kapcsoló"
                }
            }
            else {              // Csak egy '-' -al kezdődik
                if (__c == arg[1]) {
                    return i;    // egy betűs kapcsoló
                }
            }
        }
    }
    return -1;
}

int findArg(const QChar& __c, const QString& __s, const QStringList &args)
{
    //PDEB(PARSEARG) << "findArg('" << __c << "', \"" << __s << "\", args)" << endl;
    for (int i = 1; i < args.count(); ++i) {
        QString arg(args[i]);
        if (arg.indexOf(_sMinusMinus)  == 0) {
            if (__s == arg.mid(2)) {
                //PDEB(PARSEARG) << QObject::tr("Long switch found, return %1").arg(i) << endl;
                return i;
            }
        }
        else if (arg[0] == QChar('-') && arg.count() > 1) {
            if (QChar(__c) == arg[1]) {
                //PDEB(PARSEARG) << QObject::tr("Short switch found, return %1").arg(i) << endl;
                return i;
            }
        }
    }
    // PDEB(PARSEARG) << "Program switch not found. return -1" << endl;
    return -1;
}

// ****************************************************************************************************************

#ifdef Q_OS_WIN
#define DEFDEB 0
#else
#define DEFDEB cDebug::DERROR | cDebug::WARNING | cDebug::INFO | cDebug::VERBOSE | cDebug::MODMASK
#endif

eInternalStat lanView::appStat = IS_INIT;
/// Default debug level
qlonglong lanView::debugDefault = DEFDEB;
lanView  *lanView::instance     = nullptr;
#if defined(Q_OS_UNIX) || defined(Q_OS_LINUX)
bool      lanView::snmpNeeded   = true;
#else
bool      lanView::snmpNeeded   = false;
#endif
eSqlNeed  lanView::sqlNeeded    = SN_SQL_NEED;
bool      lanView::gui          = false;
const QString lanView::orgName(ORGNAME);
const QString lanView::orgDomain(ORGDOMAIN);
const short lanView::libVersionMajor = VERSION_MAJOR;
const short lanView::libVersionMinor = VERSION_MINOR;
const QString lanView::libVersion(VERSION_STR);
QString lanView::homeDefault(".");
QString    lanView::appHelp;
QString    lanView::appName;
short      lanView::appVersionMinor;
short      lanView::appVersionMajor;
QString    lanView::appVersion;
QString    lanView::testSelfName;
QString    lanView::sDateTimeForm("yyyy-MM-dd HH:mm:ss");
eIPV4Pol   lanView::ipv4Pol = IPV4_STRICT;
eIPV6Pol   lanView::ipv6Pol = IPV6_PERMISSIVE;
bool       lanView::setupTransactionFlag = false;

qlonglong sendError(const cError *pe, lanView *_instance)
{
    // Ha nem is volt hiba, akkor kész.
    if (pe == nullptr || pe->mErrorCode == eError::EOK) {
        return NULL_ID;
    }
    // Ha van nyitott adatbázis, csinálunk egy hiba rekordot
    lanView * pInst = _instance == nullptr ? lanView::instance : _instance;
    if (pInst == nullptr || pInst->pDb == nullptr || !pInst->pDb->isOpen()) {
        return NULL_ID;
    }
    QSqlQuery   q(*pInst->pDb);
    // sqlRollback(q, false);
    // sqlBegin(q);
    cNamedList  fields;
    fields.add(_sAppName,       lanView::appName);
    if (pInst->pSelfNode != nullptr && !pInst->pSelfNode->isNullId()) fields.add(_sNodeId, pInst->pSelfNode->getId());
    fields.add(_sPid,           QCoreApplication::applicationPid());
    fields.add(_sAppVer,        lanView::appVersion);
    fields.add(_sLibVer,        lanView::libVersion);
    if (pInst->pUser != nullptr && !pInst->pUser->isNullId()) fields.add(_sUserId, pInst->user().getId());
    if (pInst->pSelfService != nullptr && !pInst->pSelfService->isNullId()) fields.add(_sServiceId, pInst->pSelfService->getId());
    fields.add(_sFuncName,      pe->mFuncName);
    fields.add(_sSrcName,       pe->mSrcName);
    fields.add(_sSrcLine,       pe->mSrcLine);
    fields.add("err_code",      pe->mErrorCode);
    fields.add("err_name",      pe->errorMsg());
    fields.add("err_subcode",   pe->mErrorSubCode);
    fields.add("err_syscode",   pe->mErrorSysCode);
    fields.add("err_submsg",    pe->mErrorSubMsg);
    fields.add(_sThreadName,    pe->mThreadName);
    fields.add("sql_err_code",  pe->mSqlErrCode);
    fields.add("sql_err_type",  pe->mSqlErrType);
    fields.add("sql_driver_text",pe->mSqlErrDrText);
    fields.add("sql_db_text",   pe->mSqlErrDbText);
    fields.add("sql_query",     pe->mSqlQuery);
    fields.add("sql_bounds",    pe->mSqlBounds);
    fields.add("data_line",     pe->mDataLine);
    fields.add("data_pos",      pe->mDataPos);
    fields.add("data_msg",      pe->mDataMsg);
    fields.add("data_name",     pe->mDataName);
    fields.add("back_stack",    pe->slBackTrace.join('\n'));

    QString sql ="INSERT INTO app_errs (" + fields.names().join(", ") + ") VALUES (" + fields.quotes() + ") RETURNING applog_id";
    if (! q.prepare(sql)) {
        return NULL_ID;
    }
    int n = fields.size();
    for (int i = 0; i < n; ++i) q.bindValue(i, fields.value(i));
    if (!q.exec()) {
        return NULL_ID;
    }
    qlonglong   eid = NULL_ID;
    if (q.first()) eid = variantToId(q.value(0));
    // sqlCommit(q);
    return eid;
}

// ****************************************************************************************************************

void settingIntParameter(QSqlQuery& q, const QString& pname)
{
    qlonglong i = cSysParam::getIntegerSysParam(q, pname);
    if (i > 0) {
        QString sql = QString("SET %1 TO %2").arg(pname).arg(i);
        EXECSQL(q, sql)
    }
}

void dbOpenPost(QSqlQuery& q, const QString& _tn)
{
    QString nn = lanView::appName;
    if (!_tn.isEmpty()) {
        nn += _sUnderline + _tn;
    }
    if (lanView::getInstance()->pUser != nullptr) {
        // Ez mintha nem működne, vagy nem úgy, ahogy gondoltam
        EXECSQL(q, QString("SELECT set_user_id(%1)").arg(lanView::getInstance()->pUser->getId()));
    }
    EXECSQL(q, QString("SET application_name TO '%1'").arg(nn))
    settingIntParameter(q, "lock_timeout");
    settingIntParameter(q, "statement_timeout");
    lanView::sDateTimeForm = cSysParam::getTextSysParam(q, "date_time_form", lanView::sDateTimeForm);
}

lanView::lanView()
    : QObject(), libName(LIBNAME), debFile("-"), homeDir(), binPath(), args(), dbThreadMap(), threadMutex()
{
    // DBGFN();
    appStat     = IS_INIT;
    debug       = debugDefault;
    pSet        = nullptr;
    lastError   = nullptr;
    nonFatal    = nullptr;
    pDb         = nullptr;
#ifdef MUST_USIGNAL
    unixSignals = nullptr;
#endif // MUST_USIGNAL
    libTranslator= nullptr;
    appTranslator = nullptr;
    pSelfNode = nullptr;
    pSelfService = nullptr;
    pSelfHostService = nullptr;
    pUser = nullptr;
    setSelfStateF = false;
    pSelfInspector = nullptr;
    selfHostServiceId = NULL_ID;
    forcedSelfHostService = false;
    pQuery = nullptr;
    pLanguage = nullptr;
    pLocale   = nullptr;
#if   defined (Q_OS_LINUX)
    QString home = getEnvVar("HOME");
#elif defined (Q_OS_WIN)
    QString home = getEnvVar("USERPROFILE");
#endif
    homeDefault = home;

    try {
        cError::init(); // Set error strings
        initUserMetaTypes();

        if (testSelfName.isEmpty()) {
            testSelfName = getEnvVar("LV2TEST_SET_SELF_NAME");
        }
        // Program settings
        pSet = new QSettings(orgName, libName);
        if (pSet->allKeys().isEmpty()) {    // no settings yet
            getInitialSettings();
        }
        // Beállítjuk a DEBUG/LOG paramétereket
        debug   = pSet->value(_sDebugLevel, QVariant(debugDefault)).toLongLong();
        debFile = pSet->value(_sLogFile, QVariant(_sStdErr)).toString();
        if (testSelfName.isEmpty()) {   // Csak ha még nincs beállítva
            testSelfName = pSet->value(_sLv2testSetSelfNname).toString();
        }
        homeDir = pSet->value(_sHomeDir, QVariant(homeDefault)).toString();
        ipv4Pol = eIPV4Pol(IPV4Pol(pSet->value(_sIPV4Pol, QVariant(_sStrict)).toString()));
        ipv6Pol = eIPV6Pol(IPV6Pol(pSet->value(_sIPV6Pol, QVariant(_sPermissive)).toString()));
        QDir    d(homeDir);
        if (!QDir::setCurrent(d.path())) {
            DERR() << tr("Nincs beállítva, vagy nem létezik a program home könyvtár.") << endl;
            if (pSet->value(_sHomeDir).isNull()) {
                d.setPath(QDir::homePath());
                homeDir = d.path();
            }
            else {
                // You should not crashed, rather setup
                nonFatal = NEWCERROR(ENODIR, 0, d.path());
            }
        }
        binPath = d.filePath(_sBin);
        binPath = pSet->value(_sBinDir,  QVariant(binPath)).toString();
        // Inicializáljuk a cDebug objektumot
        cDebug::init(debFile, debug);
        // Feldolgozzuk a program paramétereket
        args = QCoreApplication::arguments();
        parseArg();
        // cDebug változások
        if (cDebug::fName() != cDebug::fNameCnv(debFile)) cDebug::init(debFile, debug);
        else cDebug::set(debug);
        // Unix Signals
#ifdef MUST_USIGNAL
        unixSignals = new cXSignal(this);
        cXSignal::setupUnixSignalHandlers();
        if (!connect(unixSignals, SIGNAL(qsignal(int)), this, SLOT(uSigSlot(int)))) {
            EXCEPTION(EPROGFAIL);
        }
#endif // MUST_USIGNAL
        if (instance != nullptr) EXCEPTION(EPROGFAIL,-1,QObject::tr("A lanView objektumból csak egy példány lehet."));
        instance = this;
        // Connect database, if need
        if (sqlNeeded != SN_NO_SQL) {
            if (openDatabase(bool2ex(sqlNeeded == SN_SQL_NEED ? EX_ERROR : EX_IGNORE))) {
                pQuery = newQuery();
                try {
                    dbOpenPost(*pQuery);
                    setSelfObjects();
                    // A reasons típus nem tartozik olyan táblához amihez osztály is van definiálva, külön csekkoljuk
                    cColEnumType::checkEnum(*pQuery, "reasons", reasons, reasons);
                    // Tábla használja, de nics objektumként ledefiniálva
                    cColEnumType::checkEnum(*pQuery, "errtype", errtype, errtype);
                } CATCHS(nonFatal)
                if (nonFatal != nullptr) {
                    pDelete(pQuery);
                    pDb->close();
                }
            }
        }
        // Language
        bool lok;
        languageId = pSet->value(_sLangId).toInt(&lok);
        if (!lok) languageId = NULL_IX;
        pLocale   = new QLocale;
        if (pQuery != nullptr) {
            pLanguage = new cLanguage;
            if (languageId != NULL_IX) {
                if (pLanguage->fetchById(*pQuery, languageId)) {
                    *pLocale = pLanguage->locale();
                    QLocale::setDefault(*pLocale);
                    setLanguage(*pQuery, lanView::getInstance()->languageId);
                }
                else {
                    pLanguage->setByLocale(*pQuery, *pLocale);
                    languageId = NULL_IX;
                }
            }
        }
        else {
            pLanguage = nullptr;
        }

        libTranslator = new QTranslator(this);
        if (libTranslator->load(*pLocale, libName, _sUnderline, homeDir)
         || libTranslator->load(*pLocale, libName, _sUnderline, binPath)) {
            QCoreApplication::installTranslator(libTranslator);
        }
        else {
            delete libTranslator;
            libTranslator = nullptr;
        }
        instAppTransl();
        // SNMP init, ha kell
        if (snmpNeeded) {
#ifdef SNMP_IS_EXISTS
            netSnmp::init();
#else //  SNMP_IS_EXISTS
            EXCEPTION(ENOTSUPP, -1, QObject::tr("SNMP not supported."));
#endif //  SNMP_IS_EXISTS
        }
        PDEB(VVERBOSE) << "End " << __PRETTY_FUNCTION__ << " try block." << endl;
    } CATCHS(lastError)
    DBGFNL();
}

QStringList * lanView::getTransactioMapAndCondLock()
{
    if (isMainThread()) {
        return &getInstance()->mainTrasactions;
    }
    else {
        getInstance()->threadMutex.lock();
        QMap<QString, QStringList>&  map = lanView::getInstance()->trasactionsThreadMap;
        QString thread = currentThreadName();
        QMap<QString, QStringList>::iterator i = map.find(thread);
        if (i == map.end()) {
            lanView::getInstance()->threadMutex.unlock();
            EXCEPTION(EPROGFAIL, 0, thread);
        }
        return &i.value();
    }
}

static void rollback_all(const QString& n, QSqlDatabase * pdb, QStringList& l)
{
    if (!l.isEmpty()) {
        DWAR() << QObject::tr("Rolback %1 all transactions : %2").arg(n,l.join(", ")) << endl;
        QSqlQuery q(*pdb);
        q.exec("ROLLBACK");
        l.clear();
    }
}

lanView::~lanView()
{
    appStat = IS_EXIT;
    instance = nullptr;    // "Kifelé" már nincs objektum
    PDEB(OBJECT) << QObject::tr("delete (lanView *)%1").arg(qulonglong(this), 0, 16) << endl;
    // fő szál tranzakciói (nem kéne lennie, ha mégis, akkor rolback mindegyikre)
    rollback_all("main", pDb, mainTrasactions);
    // Ha volt hiba objektumunk, töröljük. Elötte kiírjuk a hibaüzenetet, ha tényleg hiba volt
    if (lastError != nullptr && lastError->mErrorCode != eError::EOK) {    // Hiba volt
        PDEB(DERROR) << lastError->msg() << endl;         // A Hiba üzenet
        sendError(lastError, this);
    }
    pDelete(lastError);
    int en = cError::errCount();
    if (en) {   // Ennek illene üresnek lennie
        DERR() << tr("A hiba objektum konténer nem üres! %1 db kezeletlen objektum.").arg(en) << endl;
        do {
            cError *pe = cError::errorList.first();
            sendError(pe, this);
            delete pe;
        } while (cError::errCount());
    }
    // Lelőjük az összes Thread-et ...? Azoknak elvileg már nem szabadna mennie, sőt a konténernek is illik üresnek lennie
    // A thread-ek adatbázis objektumait töröljük.
    PDEB(INFO) << QObject::tr("Lock by threadMutex, in lanView destructor ...") << endl;
    threadMutex.lock();
    for (QMap<QString, QSqlDatabase *>::iterator i = dbThreadMap.begin(); i != dbThreadMap.end(); i++) {
        QSqlDatabase * pdb = i.value();
        if (pdb != nullptr){
            if (pdb->isValid()) {
                if (pdb->isOpen()) {
                    rollback_all(i.key(), pdb, trasactionsThreadMap[i.key()]);
                    pdb->close();
                    PDEB(SQL) << QObject::tr("Close database.") << endl;
                }
            }
            delete pdb;
            DWAR() << QObject::tr("pdb for %1 thread deleted by lanView destructor.").arg(i.key()) << endl;
        }
        else {
            DERR() << QObject::tr("pdb is NULL, for %1 thread.").arg(i.key()) << endl;
        }
    }
    dbThreadMap.clear();
    trasactionsThreadMap.clear();
    threadMutex.unlock();
    // Töröljük az adatbázis objektumot, ha volt.
    pDelete(pQuery);
    closeDatabase();
    // Töröljük a QSettings objektumunkat
    if (pSet != nullptr) {
        pSet->sync();
        delete pSet;
        pSet = nullptr;
    }
#ifdef SNMP_IS_EXISTS
    netSnmp::down();
#endif // SNMP_IS_EXISTS
    if (appTranslator != nullptr) {
        QCoreApplication::removeTranslator(appTranslator);
        delete appTranslator;
    }
    if (libTranslator != nullptr) {
        QCoreApplication::removeTranslator(libTranslator);
        delete libTranslator;
    }
    DBGFNL();
}

bool checkDbVersion(QSqlQuery& q, QString& msg)
{
    // A cSysParam objektumot nem használhatjuk !!
    bool ok1, ok2;
    int vmajor = int(execSqlIntFunction(q, &ok1, "get_int_sys_param", "version_major"));
    int vminor = int(execSqlIntFunction(q, &ok2, "get_int_sys_param", "version_minor"));
    if (!(ok1 && ok2)) {
        msg = QObject::tr("The database version numbers wrong format or missing.");
        DERR() << msg << endl;
        return false;
    }
    if (DB_VERSION_MAJOR != vmajor || DB_VERSION_MINOR != vminor) {
        msg = QObject::tr("Incorrect database version.\n"
                              "The expected version is %1.%2\n"
                              "The current database version is %3.%4")
                .arg(DB_VERSION_MAJOR).arg(DB_VERSION_MINOR).arg(vmajor).arg(vminor);
        DERR() << msg << endl;
        return false;
    }
    return true;
}

bool lanView::openDatabase(eEx __ex)
{
    closeDatabase();
    PDEB(VERBOSE) << "Open database ..." << endl;
    pDb = new  QSqlDatabase(QSqlDatabase::addDatabase(_sQPSql));
    if (!pDb->isValid()) SQLOERR(*pDb)
    pDb->setHostName(pSet->value(_sSqlHost).toString());
    pDb->setPort(pSet->value(_sSqlPort).toInt());
    pDb->setUserName(scramble(pSet->value(_sSqlUser).toString()));
    pDb->setPassword(scramble(pSet->value(_sSqlPass).toString()));
    pDb->setDatabaseName(pSet->value(_sDbName).toString());
    bool r = pDb->open();
    cError *pe;
    if (r) {
        QSqlQuery q(*pDb);
        QString msg;
        r = checkDbVersion(q, msg);
        if (!r) {
            pe = NEWCERROR(ESQLOPEN, 0, msg);
            if (__ex == EX_IGNORE) nonFatal = pe;
            else pe->exception();
            closeDatabase();
        }
    }
    else {
        QSqlError le = (*pDb).lastError();
        _sql_err_deb_(le, __FILE__, __LINE__, __PRETTY_FUNCTION__);
        pe = NEWCERROR(ESQLOPEN, 0, le.text());
        if (__ex == EX_IGNORE) nonFatal = pe;
        else pe->exception();
    }
    return r;
}

void lanView::closeDatabase()
{
    if (pDb != nullptr){
        if (pDb->isValid()) {
            if (pDb->isOpen()) {
                pDb->close();
                PDEB(SQL) << QObject::tr("Close database.") << endl;
            }
            delete pDb;
            pDb = nullptr;
            QSqlDatabase::removeDatabase(_sQPSql);
            PDEB(SQL) << QObject::tr("Remove database object.") << endl;
        }
        else {
            delete pDb;
            pDb = nullptr;
        }
        PDEB(SQL) << QObject::tr("pDb deleted.") << endl;
    }
}

void lanView::setSelfObjects()
{
    if (pDb != nullptr && pDb->isOpen()) {
        if (!isMainThread()) EXCEPTION(EPROGFAIL);
        pSelfNode = new cNode;
        if (selfHostServiceId != NULL_ID) {                     // ID set by command parameter
            pSelfHostService = new cHostService();
            pSelfHostService->setById(*pQuery, selfHostServiceId);
            pSelfNode->setById(*pQuery, pSelfHostService->getId(_sNodeId));
            pSelfService = cService::service(*pQuery, pSelfHostService->getId(_sServiceId));
            setSelfStateF = true;
        }
        else if (pSelfNode->fetchSelf(*pQuery, EX_IGNORE)) {    // Service name == app name
            pSelfService = cService::service(*pQuery, appName, EX_IGNORE);
            if (pSelfService != nullptr) {
                pSelfHostService = new cHostService();
                pSelfHostService->setId(_sNodeId,    pSelfNode->getId());
                pSelfHostService->setId(_sServiceId, pSelfService->getId());
                int n = pSelfHostService->completion(*pQuery);
                if (n > 1) {       // ambiguous, and if proto and prime = nil ?
                    pSelfHostService->clear();
                    pSelfHostService->setId(_sNodeId,    pSelfNode->getId());
                    pSelfHostService->setId(_sServiceId, pSelfService->getId());
                    pSelfHostService->setId(_sProtoServiceId, NIL_SERVICE_ID);
                    pSelfHostService->setId(_sPrimeServiceId, NIL_SERVICE_ID);
                    n = pSelfHostService->completion(*pQuery);
                }
                if (1 == n) {
                    setSelfStateF = true;
                    selfHostServiceId = pSelfHostService->getId();
                }
                else {                                  // not found or unclear
                    pDelete(pSelfHostService);
                }
            }
        }
        else {                                                  // unknown node
            pDelete(pSelfNode);
        }
    }
}

void lanView::parseArg(void)
{
    DBGFN();
    int i;
    if (ONDB(PARSEARG)) {
        cDebug::cout() << head << QObject::tr("arguments :");
        foreach (QString arg, args) {
             cDebug::cout() << QChar(' ') << arg;
        }
        cDebug::cout() << endl;
    }
    if (0 < findArg(QChar('h'), _sHelp, args)) {
        QTextStream QStdOut(stdout);
        if (appHelp.isEmpty() == false) QStdOut << appHelp << endl;
        QStdOut << tr("-d|--debug-level <level>    Set debug level") << endl;
        QStdOut << tr("-L|--log-file <file name>   Set log file name") << endl;
        QStdOut << tr("-V|--lib-version            Print lib version") << endl;
        QStdOut << tr("-S|--test-self-name         Test option") << endl;
        QStdOut << tr("-R|--host-service-id        Root host-service id") << endl;
        QStdOut << tr("-h|--help                   Print help") << endl;
        EXCEPTION(EOK, RS_STAT_SETTED); // Exit program
    }
    if (0 < findArg(QChar('V'), _sLibVersion, args)) {
        QTextStream QStdOut(stdout);
        QStdOut << QString(tr("LanView2 lib version ")).arg(libVersion) << endl;
        EXCEPTION(EOK, RS_STAT_SETTED); // Exit program
    }
    if (0 < findArg(QChar('v'), _sVersion, args)) {
        QTextStream QStdOut(stdout);
        QStdOut << QString(tr("%1 version %2")).arg(appName).arg(appVersion) << endl;
        EXCEPTION(EOK, RS_STAT_SETTED); // Exit program
    }
    if (0 < (i = findArg(QChar('d'), _sDebugLevel, args))
     && (i + 1) < args.count()) {
        bool ok;
        debug = args[i + 1].toLongLong(&ok, 0);
        if (!ok) DERR() << tr("Invalid numeric argument parameter : %1 %2").arg(args[i]).arg(args[i + 1])  << endl;
        args.removeAt(i);
        args.removeAt(i);
    }
    if (0 < (i = findArg(QChar('L'), _sLogFile, args))
     && (i + 1) < args.count()) {
        debFile = args[i + 1];
        args.removeAt(i);
        args.removeAt(i);
    }
    if (0 < (i = findArg(QChar('S'), _sLv2testSetSelfNname, args))
     && (i + 1) < args.count()) {
        testSelfName = args[i + 1];
        args.removeAt(i);
        args.removeAt(i);
    }
    if (0 < (i = findArg(QChar('R'), QString(_sHostServiceId).replace(QChar('_'), QChar('-')), args))
     && (i + 1) < args.count()) {
        bool ok;
        qlonglong id = args[i + 1].toLongLong(&ok, 0);
        if (ok) {
            selfHostServiceId = id;
            forcedSelfHostService = true;
        }
        else {
            DERR() << tr("Invalid numeric argument parameter : %1 %2").arg(args[i]).arg(args[i + 1])  << endl;
        }
        args.removeAt(i);
        args.removeAt(i);
    }
    DBGFNL();
}

void lanView::instAppTransl()
{
    if (pLocale == nullptr) {
        appTranslator = nullptr;
        DERR() << QObject::tr("Application language file not loaded, locale object pointer is NULL.") << endl;
        return;
    }
    appTranslator = new QTranslator(this);
    if (appTranslator->load(*pLocale, appName, _sUndeline, homeDir)
     || appTranslator->load(*pLocale, appName, _sUndeline, binPath)) {
        QCoreApplication::installTranslator(appTranslator);
    }
    else {
        DERR() << QObject::tr("Application language file not loaded : %1/%2").arg(appName).arg(pLocale->name()) << endl;
        delete appTranslator;
        appTranslator = nullptr;
    }
}

// Az sqlPass így nem OK, javítani!!!

void lanView::getInitialSettings()
{
    QDir dir = QDir::current();
    QFileInfo fi(dir, "LanView2First.ini");
    if (!fi.isReadable()) {
        PDEB(INFO) << tr("First ini file ") << fi.filePath() << tr(" is not found, or not readable (current dir)") << endl;
        dir.setPath(QCoreApplication::applicationDirPath());
        fi = QFileInfo(dir, "LanView2First.ini");
        if (!fi.isReadable()) {
            PDEB(INFO) << tr("First ini file ") << fi.filePath() << tr(" is not found, or not readable (app. dir)") << endl;
            return;
        }
    }
    PDEB(INFO) << tr("Read first ini file ") << fi.filePath() << endl;
    QFile f(fi.filePath());
    if (f.open(QIODevice::ReadOnly)) {
        bool fl = false;
        QTextStream inf(&f);
        QString s;
        QStringList scrambled = { "sqlUser", "sqlPass" };
        while (!(s = inf.readLine()).isEmpty()) {
            s = s.trimmed();
            QStringList eq = s.split(QChar('='));
            if (eq.size() == 2) {
                QString key = eq.first().simplified();
                QString val = eq.at(1).simplified();
                bool ok;
                qlonglong num = val.toLongLong(&ok);
                if (ok) {
                    pSet->setValue(key, num);
                }
                else {
                    if (scrambled.contains(key)){
                        val = scramble(val);
                    }
                    pSet->setValue(key, val);
                }
                fl = true;
            }
        }
        if (fl) pSet->sync();
    }
    else {
        PDEB(INFO) << tr("Open errorfirst ini file ") << fi.filePath() << endl;
    }
}

#ifdef MUST_USIGNAL
bool lanView::uSigRecv(int __i)
{
    if (__i == SIGHUP) {
        PDEB(INFO) << tr("Esemény : SIGHUP; reset ...") << endl;
        emitReset();
        return false;
    }
    return true;
}
#endif

void lanView::reSet()
{
    PDEB(INFO) << endl;
    try {
        down();
        appStat = IS_RESTART;
        resetCacheData();
        setSelfObjects();
        setup();
    } CATCHS(lastError)
    if (lastError != nullptr) {
        QCoreApplication::exit(lastError->mErrorCode);
        printf(" -- reSet(): EXIT %d\n", int(lastError->mErrorCode));
    }
}

void lanView::setup(eTristate _tr)
{
    tSetup<cInspector>(_tr);
}

void lanView::down()
{
    appStat = IS_DOWN;
    PDEB(INFO) << "Self inspector :" << (pSelfInspector != nullptr ? pSelfInspector->name() : _sNULL) << endl;
    if (pSelfInspector != nullptr) pSelfInspector->stop();
    pDelete(pSelfInspector);
    pDelete(pSelfHostService);
    pDelete(pSelfNode);
    pSelfService = nullptr;    // cache!
}

#ifdef MUST_USIGNAL
void lanView::uSigSlot(int __i)
{
    if (uSigRecv(__i)) switch (__i) {
        case SIGINT:
            PDEB(INFO) << QObject::tr("Signal SIGINT, call exit.") << endl;
            QCoreApplication::exit(0);
            break;
        default:
            PDEB(WARNING) << QObject::tr("Signal #%1 ignored.").arg(__i) << endl;
            break;
    }
}
#endif

static void subSrvDbNotif(cInspector * _pi, const QList<qlonglong>& hsids, const QString& cmd)
{
    if (_pi == nullptr) return;
    if (hsids.contains(_pi->hostServiceId())) _pi->dbNotif(cmd);
    if (_pi->pSubordinates == nullptr) return;
    foreach (cInspector * pi, *_pi->pSubordinates) {
        subSrvDbNotif(pi, hsids, cmd);
    }
}

void    lanView::dbNotif(const QString& name, QSqlDriver::NotificationSource source, const QVariant &payload)
{
    if (ONDB(INFO)) {
        QString src;
        switch (source) {
        case QSqlDriver::SelfSource:    src = _sSelf;       break;
        case QSqlDriver::OtherSource:   src = _sOther;      break;
        case QSqlDriver::UnknownSource:
     /* default:  */                    src = _sUnknown;    break;
        }
        cDebug::cout() << HEAD() << QObject::tr("Database notifycation : %1, source %2, payload :").arg(name).arg(src) << debVariantToString(payload) << endl;
    }
    QString sPayload = payload.toString();
    if (sPayload.isNull()) return;
    QStringList shsids = sPayload.split(QRegExp("[\\s\\(\\),;/]+"));
    QString     cmd = shsids.takeFirst();
    if (shsids.isEmpty()) return; // No specified any host_service_id
    QList<qlonglong> hsids;
    foreach (QString shsid, shsids) {
        bool ok;
        qlonglong hsid = shsid.toInt(&ok);
        if (ok) hsids << hsid;
    }
    if (hsids.isEmpty()) return; // No specified any valid host_service_id
    if (hsids.contains(selfHostServiceId)) {
        if (0 == _sReset.compare(cmd,  Qt::CaseInsensitive)) {
            PDEB(WARNING) << tr("NOTIFY %1  %2; reset ...").arg(name, sPayload) << endl;
            reSet();
            return;
        }
        else if (0 == QString(_sExit).compare(cmd,  Qt::CaseInsensitive)) {
            PDEB(WARNING) << tr("NOTIFY %1  %2; exit ...").arg(name, sPayload) << endl;
            down();
            printf(" -- dbNotif(...): EXIT 0\n");
            QCoreApplication::exit(0);
            return;
        }
    }
    // command to cInspector object, sub services ?
    subSrvDbNotif(pSelfInspector, hsids, cmd);

}

void lanView::insertStart(QSqlQuery& q)
{
    DBGFN();
    /* if (!q.exec(QString("SELECT error('Start', %1, '%2')").arg(QCoreApplication::applicationPid()).arg(appName)))
        SQLQUERYERR(q);*/
    cDbErr::insertNew(q, cDbErrType::_sStart, appName, QCoreApplication::applicationPid(), _sNil, _sNil);
}

void lanView::insertReStart(QSqlQuery& q) {
    DBGFN();
    /*if (!q.exec(QString("SELECT error('ReStart', %1, '%2')").arg(QCoreApplication::applicationPid()).arg(appName)))
        SQLQUERYERR(q);*/
    cDbErr::insertNew(q, cDbErrType::_sReStart, appName, QCoreApplication::applicationPid(), _sNil, _sNil);
}

bool lanView::subsDbNotif(const QString& __n, eEx __ex)
{
    static bool first = true;
    QString e;
    if (pDb != nullptr && pDb->driver()->hasFeature(QSqlDriver::EventNotifications)) {
        QString name = __n.isEmpty() ? appName : __n;
        if (!first && pDb->driver()->subscribedToNotifications().contains(name)) {
            PDEB(INFO) << "Already subscribed NOTIF " << name << endl;
            return true;
        }
        PDEB(INFO) << "Subscribe NOTIF " << name << endl;
        if (pDb->driver()->subscribeToNotification(name)) {
            if (first) {
                PDEB(VVERBOSE) << "Connect to notification(QString)" << endl;
                connect(pDb->driver(),
                        SIGNAL(notification(const QString&, QSqlDriver::NotificationSource, const QVariant&)),
                               SLOT(dbNotif(const QString&, QSqlDriver::NotificationSource, const QVariant&)));
                first = false;
            }
            return true;    // O.K.
        }
        e = tr("Database notification subscribe is unsuccessful.");
    }
    else {
        e = tr("Database notification is nothing supported.");
    }
    if (__ex) EXCEPTION(ENOTSUPP, -1, e);
    DERR() << e << endl;
    return false;
}

const cUser *lanView::setUser(const QString& un, const QString& pw, eEx __ex)
{
    if (instance == nullptr) EXCEPTION(EPROGFAIL);
    QSqlQuery q = getQuery();
    if (instance->pUser != nullptr) pDelete(instance->pUser);
    instance->pUser = new cUser();
    if (!instance->pUser->checkPasswordAndFetch(q, pw, un)) {
        pDelete(instance->pUser);
        if (__ex)  EXCEPTION(EFOUND,-1,tr("Ismeretlen felhasználó, vagy nem megfelelő jelszó"));
        return nullptr;
    }
    if (!q.exec(QString("SELECT set_user_id(%1)").arg(instance->pUser->getId()))) SQLQUERYERR(q)
    return instance->pUser;
}
const cUser *lanView::setUser(const QString& un, eEx __ex)
{
    if (instance == nullptr) EXCEPTION(EPROGFAIL);
    QSqlQuery q = getQuery();
    if (instance->pUser != nullptr) pDelete(instance->pUser);
    instance->pUser = new cUser();
    if (!instance->pUser->fetchByName(q, un)) {
        pDelete(instance->pUser);
        if (__ex)  EXCEPTION(EFOUND,-1,tr("Ismeretlen felhasználó : %1").arg(un));
        return nullptr;
    }
    if (q.exec(QString("SELECT set_user_id(%1)").arg(instance->pUser->getId()))) SQLQUERYERR(q)
    return instance->pUser;
}

const cUser *lanView::setUser(qlonglong uid, eEx __ex)
{
    if (instance == nullptr) EXCEPTION(EPROGFAIL);
    QSqlQuery q = getQuery();
    if (instance->pUser != nullptr) pDelete(instance->pUser);
    instance->pUser = new cUser();
    if (!instance->pUser->fetchById(q,uid)) {
        pDelete(instance->pUser);
        if (__ex)  EXCEPTION(EFOUND, uid, tr("Ismeretlen felhasználó azonosító"));
        return nullptr;
    }
    if (!q.exec(QString("SELECT set_user_id(%1)").arg(uid))) SQLQUERYERR(q)
    instance->pUser->getRights(q);
    return instance->pUser;
}

const cUser *lanView::setUser(cUser *__pUser)
{
    if (instance == nullptr) EXCEPTION(EPROGFAIL);
    if (!(__pUser->_stat < 0 || __pUser->_stat & ES_COMPLETE)) EXCEPTION(EDATA,__pUser->_stat, "Invalid __pUser object :" + __pUser->toString());
    if (instance->pUser != nullptr) pDelete(instance->pUser);
    instance->pUser = __pUser;
    return instance->pUser;
}


const cUser& lanView::user()
{
    if (instance == nullptr || instance->pUser == nullptr) EXCEPTION(EPROGFAIL);
    return *instance->pUser;
}

qlonglong lanView::getUserId(eEx __ex)
{
    if (instance == nullptr) EXCEPTION(EPROGFAIL);
    if (instance->pUser == nullptr) {
        if (__ex != EX_IGNORE) EXCEPTION(EFOUND);
        return NULL_ID;
    }
    return instance->pUser->getId();
}

bool lanView::isAuthorized(enum ePrivilegeLevel pl)
{
    enum ePrivilegeLevel act = user().privilegeLevel();
    return act  >= pl;
}

bool lanView::isAuthorized(const QString& pl)
{
    return isAuthorized(ePrivilegeLevel(privilegeLevel(pl)));
}



void lanView::resetCacheData()
{
    if (!exist()) EXCEPTION(EPROGFAIL);
    QSqlQuery q = getQuery();
    cIfType::fetchIfTypes(q);
    cService::resetCacheData();
    cTableShape::resetCacheData();
    if (instance->pUser != nullptr) {
        instance->pUser->setById(q);
        instance->pUser->getRights(q);
    }
}

const QString& IPV4Pol(int e, eEx __ex)
{
    switch(e) {
    case IPV4_IGNORED:      return _sIgnored;
    case IPV4_PERMISSIVE:   return _sPermissive;
    case IPV4_STRICT:       return _sStrict;
    }
    if (__ex) EXCEPTION(EDATA);
    return _sNul;
}

int IPV4Pol(const QString& n, eEx __ex)
{
    if (n.compare(_sIgnored, Qt::CaseInsensitive)) return IPV4_IGNORED;
    if (n.compare(_sPermissive, Qt::CaseInsensitive)) return IPV4_PERMISSIVE;
    if (n.compare(_sStrict, Qt::CaseInsensitive)) return IPV4_STRICT;
    if (__ex) EXCEPTION(EDATA);
    return IPV4_UNKNOWN;
}

const QString& IPV6Pol(int e, eEx __ex)
{
    switch(e) {
    case IPV6_IGNORED:      return _sIgnored;
    case IPV6_PERMISSIVE:   return _sPermissive;
    case IPV6_STRICT:       return _sStrict;
    }
    if (__ex) EXCEPTION(EDATA);
    return _sNul;
}

int IPV6Pol(const QString& n, eEx __ex)
{
    if (n.compare(_sIgnored, Qt::CaseInsensitive)) return IPV6_IGNORED;
    if (n.compare(_sPermissive, Qt::CaseInsensitive)) return IPV6_PERMISSIVE;
    if (n.compare(_sStrict, Qt::CaseInsensitive)) return IPV6_STRICT;
    if (__ex) EXCEPTION(EDATA);
    return IPV6_UNKNOWN;
}

cLv2QApp::cLv2QApp(int& argc, char ** argv) : QCoreApplication(argc, argv)
{
    lanView::gui = false;
}


cLv2QApp::~cLv2QApp()
{

}

bool cLv2QApp::notify(QObject * receiver, QEvent * event)
{
    static cError *lastError = nullptr;
    try {
        return QCoreApplication::notify(receiver, event);
    }
    catch(no_init_ *) { // Már letiltottuk a cError dobálást
        PDEB(VERBOSE) << "Dropped cError..." << endl;
        return false;
    }
    CATCHS(lastError)
/*    PDEB(DERROR) << "Error in "
                 << __PRETTY_FUNCTION__
                 << endl;
    PDEB(DERROR) << "Receiver : "
                 << receiver->objectName()
                 << "::"
                 << typeid(*receiver).name()
                 << endl;
    PDEB(DERROR) << "Event : "
                 << typeid(*event).name()
                 << endl; Ettől kiakad :-O ! */
    cError::mDropAll = true;                    // A továbbiakban nem *cError-al dobja a hibákat, hanem no_init_ *-el
    lanView::getInstance()->lastError = lastError;
    DERR() << lastError->msg() << endl;
    printf(" -- notify(): EXIT %d\n", int(lastError->mErrorCode));
    QCoreApplication::exit(lastError->mErrorCode);  // kilépünk.
    return false;
}

QString scramble(const QString& _s)
{
    QString r;
    qsrand(32572345U);
    foreach (QChar c, _s) {
        ushort u = c.unicode();
        u ^= ushort(qrand());
        r += QChar(u);
    }
    return r;
}

enum eImportParserStat importParserStat = IPS_READY;

bool callFromInterpreter()
{
    bool r = false;
    switch (importParserStat) {
    case IPS_READY:                     // Nem fut az interpreter
        break;
    case IPS_RUN:                       // A fő szálban fut az interpreter
        if (isMainThread()) r = true;   // Ez a fő szál
        break;
    case IPS_THREAD:                        // Egy thread-ban fut az interpreter
        if (isMainThread()) r = false;      // De ez a fő szál
        else if (QThread::currentThread()->objectName() == _sImportParser) r = true;
        break;
//  default:
//      EXCEPTION(EPROGFAIL);
    }
    return r;
}

#define IMPQUEUEMAXWAIT 1000

cExportQueue *cExportQueue::_pForParser = nullptr;
QMutex cExportQueue::staticMutex;
QMap<QString, cExportQueue *>    cExportQueue::intMap;

cExportQueue *cExportQueue::_pInstanceByThread()
{
    QString tn = currentThreadName();
    QMap<QString, cExportQueue *>::iterator it = intMap.find(tn);
    if (it == intMap.end()) return nullptr;
    return it.value();
}

cExportQueue *cExportQueue::pInstance()
{
    cExportQueue *r = nullptr;
    if (!staticMutex.tryLock(IMPQUEUEMAXWAIT)) EXCEPTION(ETO);
    if (callFromInterpreter()) {
        if (_pForParser == nullptr) _pForParser = new cExportQueue;
        r = _pForParser;
    }
    else {
        r = _pInstanceByThread();
    }
    staticMutex.unlock();
    return r;
}

cExportQueue::cExportQueue()
    : QObject(), QQueue<QString>(), QMutex()
{

}

cExportQueue::~cExportQueue()
{

}

/// Ha nem az iterpreterből lett hivva a metódus nem csinál semmit, lásd callFromInterpreter() -t.
/// Egyébként egy sor vagy paragrafus elhelyezése a queue-ban.
/// A szöveg elhelyezése után kivált egy ready() szignált.
/// A queue-hoz való hozzáféréshez egy belső mutex objektumot használ,
/// időtullépés esetén kizárást dob.
void cExportQueue::push(const QString &s)
{
    cExportQueue *r = pInstance();
    if (r == nullptr) return;
    if (!r->tryLock(IMPQUEUEMAXWAIT)) EXCEPTION(ETO);
    r->enqueue(s);
    r->unlock();
    r->ready();
}

void cExportQueue::push(const QStringList &sl)
{
    cExportQueue *r = pInstance();
    if (r == nullptr) return;
    foreach (QString s, sl) {
        if (!r->tryLock(IMPQUEUEMAXWAIT)) EXCEPTION(ETO);
        r->enqueue(s);
        r->unlock();
        r->ready();
    }
}

/// Kivesz egy sort vagy paragrafust a queue-ból.
/// Ha a queue üres, akkor egy null stringgel tér vissza.
/// A queue-hoz való hozzáféréshez egy belső mutex objektumot használ,
/// időtullépés esetén kizárást dob.
QString cExportQueue::pop()
{
    if (!tryLock(IMPQUEUEMAXWAIT)) EXCEPTION(ETO);
    QString s;
    if (!isEmpty()) s = dequeue();
    unlock();
    return s;
}

/// Kiüríti a queue-t, az eredmény stringben a queue elemei közötti szeparátor a soremelés karakter.
/// A queue-hoz való hozzáféréshez egy belső mutex objektumot használ,
/// időtullépés esetén kizárást dob.
/// ...
QString cExportQueue::toText(bool fromInterpret, bool _clr)
{
    cExportQueue *p;
    if (!staticMutex.tryLock(IMPQUEUEMAXWAIT)) EXCEPTION(ETO);
    if (fromInterpret) {
        p = _pForParser;
    }
    else {
        p = _pInstanceByThread();
    }
    staticMutex.unlock();

    QString s;
    if (p == nullptr) {
        DERR() << "Export Queue object is NULL" << endl;
    }
    else {
        if (!p->tryLock(IMPQUEUEMAXWAIT)) EXCEPTION(ETO);
        s = p->join("\n");
        if (!fromInterpret && _clr) {
            QString tn = currentThreadName();
            p->unlock();
            delete p;
            intMap.remove(tn);
            return s;
        }
        else {
            p->clear();
        }
        p->unlock();
    }
    return s;
}

cExportQueue *cExportQueue::init(bool fromInterpret)
{
    cExportQueue *p;
    if (!staticMutex.tryLock(IMPQUEUEMAXWAIT)) EXCEPTION(ETO);
    if (fromInterpret) {
        if (_pForParser != nullptr) {
            delete _pForParser;
        }
        _pForParser = p = new cExportQueue;
    }
    else {
        QThread *th = QThread::currentThread();
        QString  tn = threadName(th);
        QMap<QString, cExportQueue *>::iterator it = intMap.find(tn);
        if (it == intMap.end()) {
            intMap[tn] = p = new cExportQueue;
        }
        else {
            delete it.value();
            it.value() = p = new cExportQueue;
        }
    }
    staticMutex.unlock();
    return p;
}

void cExportQueue::drop(bool fromInterpret)
{
    if (!staticMutex.tryLock(IMPQUEUEMAXWAIT)) EXCEPTION(ETO);
    if (fromInterpret) {
        if (_pForParser != nullptr) {
            delete _pForParser;
        }
        _pForParser = nullptr;
    }
    else {
        QThread *th = QThread::currentThread();
        QString  tn = threadName(th);
        QMap<QString, cExportQueue *>::iterator it = intMap.find(tn);
        if (it != intMap.end()) {
            delete it.value();
            intMap.remove(tn);
        }
    }
    staticMutex.unlock();
}

const QString sET_       = "errtype";
const QString sET_Fatal  = "Fatal";
const QString sET_Error  = "Error";
const QString sET_Warning= "Warning";
const QString sET_Ok     = "Ok";
const QString sET_Info   = "Info";

/// Hiba típus konverziók
/// A string konstansok nem kerültek a szótárba,
/// mert nagy kezdőbetűvel lettek létrehozva (még a project elején)
/// A konvenció szerinti modosításuk túl sok PLSQL függvényt érintene,
/// így viszont ütköznek a nevek más nem nagy betüs szavakkal a szótárban.
int errtype(const QString& s, eEx __ex)
{
    if (0 == s.compare(sET_Fatal,   Qt::CaseInsensitive)) return ET_FATAL;
    if (0 == s.compare(sET_Error,   Qt::CaseInsensitive)) return ET_ERROR;
    if (0 == s.compare(sET_Warning, Qt::CaseInsensitive)) return ET_WARNING;
    if (0 == s.compare(sET_Ok,      Qt::CaseInsensitive)) return ET_OK;
    if (0 == s.compare(sET_Info,    Qt::CaseInsensitive)) return ET_INFO;
    if (__ex != EX_IGNORE) EXCEPTION(EENUMVAL, -1, s);
    return ENUM_INVALID;
}

const QString& errtype(int e, eEx __ex)
{
    switch (e) {
    case ET_FATAL:      return sET_Fatal;
    case ET_ERROR:      return sET_Error;
    case ET_WARNING:    return sET_Warning;
    case ET_OK:         return sET_Ok;
    case ET_INFO:       return sET_Info;
    default:    if (__ex != EX_IGNORE) EXCEPTION(EENUMVAL, e);
    }
    return _sNul;
}

/* ***************************************************************************************************************** */

#if   defined(Q_CC_MSVC) || defined(Q_OS_WIN)
# include <Windows.h>
# define SECURITY_WIN32
# include <security.h>
# include <secext.h>
#else
# include <unistd.h>
# include <sys/types.h>
# include <pwd.h>
#endif

cUser * getOsUser(QString& domain, QString& user)
{
    domain.clear();
    user.clear();
#if   defined(Q_CC_MSVC) || defined(Q_OS_WIN)
#define USER_NAME_MAXSIZE   64
    char charUserName[USER_NAME_MAXSIZE];
    ULONG userNameSize = USER_NAME_MAXSIZE;
    if (GetUserNameExA(NameDnsDomain, (LPSTR)charUserName, &userNameSize)) {
        QString du = QString(charUserName);
        QStringList sdu = du.split('\\');
        if (sdu.size() == 2) {
            domain = sdu.at(0);
            user   = sdu.at(1);
        }
    }
#else
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        user = pw->pw_name;
        if (lanView::getInstance()->pSelfNode != nullptr) {
            domain = lanView::getInstance()->pSelfNode->getName();
        }
    }
#endif
    cUser *r = nullptr;
    if (!(domain.isEmpty() || user.isEmpty())) {
        QString ud = user + "@" + domain;
        QString sql = "SELECT * FROM users WHERE ? = ANY(domain_users)";
        QSqlQuery q = getQuery();
        if (execSql(q, sql, ud)) {
            r = new cUser();
            r->set(q);
        }
    }
    return r;
}

bool parentIsLv2d()
{
    bool r = false;
#if   defined(Q_CC_MSVC) || defined(Q_OS_WIN)
#else
    pid_t ppid = getppid();
    QFile comm(QString("/proc/%1/comm").arg(int(ppid)));
    QString parenName;
    if (comm.open(QIODevice::ReadOnly)) {
        parenName = QString::fromUtf8(comm.readAll()).trimmed();
        r = 0 == parenName.compare(_sLv2d);
    }
#endif
    return r;
}
