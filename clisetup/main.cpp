#include <QCoreApplication>
#include <QtCore>
#include <QSqlDatabase>
#include "lanview.h"


#define APPNAME "clisetup"
#undef  __MODUL_NAME__
#define __MODUL_NAME__  APP

#define VERSION_MAJOR   0
#define VERSION_MINOR   01
#define VERSION_STR     _STR(VERSION_MAJOR) "." _STR(VERSION_MINOR)

const QString& setAppHelp()
{
    return _sNul;
}

static QTextStream stdo(stdout);
static QTextStream stdi(stdin);

static void settingQSetIValue(const QString& _key, const QString& _text, qlonglong _def)
{
    bool ok;
    qlonglong i;
    do {
        i = lanView::getInstance()->pSet->value(_key, _def).toLongLong();
        stdo << QObject::tr("%1 (%2) : ").arg(_text).arg(i) << endl;
        QString s = stdi.readLine();
        s = s.simplified();
        if (s.isEmpty()) break;
        i = s.toLongLong(&ok);
    } while (!ok);
    lanView::getInstance()->pSet->setValue(_key, i);
}


static void settingQSetSValue(const QString& _key, const QString& _text, const QString& _def)
{
    QString s = lanView::getInstance()->pSet->value(_key, _def).toString();
    stdo << QObject::tr("%1 (%2) : ").arg(_text, s) << endl;
    s = stdi.readLine();
    s = s.simplified();
    if (s.isEmpty()) s = _def;
    lanView::getInstance()->pSet->setValue(_key, s);
}

static void settingQSetXValue(const QString& _key, const QString& _text)
{
    stdo << QObject::tr("%1 : ").arg(_text) << endl;
    QString s = stdi.readLine();
    s = s.simplified();
    if (s.isEmpty()) return;
    s = scramble(s);
    lanView::getInstance()->pSet->setValue(_key, s);
}

int main(int argc, char *argv[])
{
    cLv2QApp app(argc, argv);

    SETAPP()
    lanView::snmpNeeded = false;
    lanView::sqlNeeded  = SN_NO_SQL;

    lanView   mo;

    if (mo.lastError) {  // Ha hiba volt, vagy vége
        return mo.lastError->mErrorCode; // a mo destruktora majd kiírja a hibaüzenetet.
    }

    cError *pe = nullptr;

    try {
        settingQSetSValue(_sHomeDir,   QObject::tr("Munka könyvtár path"), lanView::homeDefault);
        settingQSetSValue(_sSqlHost,   QObject::tr("SQL szerver címe"),    _sLocalHost);
        settingQSetIValue(_sSqlPort,   QObject::tr("SQL szerver port"),    5432);
        settingQSetXValue(_sSqlUser,   QObject::tr("SQL felhasználói név"));
        settingQSetXValue(_sSqlPass,   QObject::tr("SQL jelszó"));
        settingQSetSValue(_sDbName,    QObject::tr("Az adatbázis neve"),   _sLanView2);
        settingQSetSValue(_sLogFile,   QObject::tr("Log fájl"),            _sStdErr);
        settingQSetIValue(_sDebugLevel,QObject::tr("Nyomkövetési szint"),  lanView::debugDefault);
        settingQSetSValue(_sMibPath,   QObject::tr("MIB path lista"),      _sNul);
        lanView::getInstance()->pSet->sync();

        eTristate f = TS_NULL;
        do {
            stdo << QObject::tr("Kísérlet, az adatbázis megnyitására ? (I/n)") << endl;
            QString s = stdi.readLine();
            s = s.simplified();
            if (s.isEmpty() || s.compare("i", Qt::CaseInsensitive)) f = TS_TRUE;
            else if (s.compare("n", Qt::CaseInsensitive)) f = TS_FALSE;
        } while (f == TS_NULL);

        if (f == TS_TRUE) {
            QSqlDatabase *pDb = new  QSqlDatabase(QSqlDatabase::addDatabase(_sQPSql));
            if (!pDb->isValid()) {
                QSqlError le = pDb->lastError();
                stdo << QObject::tr("SQL DB ERROR #") + le.nativeErrorCode() << endl;
                stdo << "driverText   : " << le.driverText() << endl;
                stdo << "databaseText : " << le.databaseText() << endl;
            }
            else {  // OK
                pDb->setHostName(   lanView::getInstance()->pSet->value(_sSqlHost).toString());
                pDb->setPort(       lanView::getInstance()->pSet->value(_sSqlPort).toInt());
                pDb->setUserName(   scramble(lanView::getInstance()->pSet->value(_sSqlUser).toString()));
                pDb->setPassword(   scramble(lanView::getInstance()->pSet->value(_sSqlPass).toString()));
                pDb->setDatabaseName(lanView::getInstance()->pSet->value(_sDbName).toString());
                if (!pDb->open()) {
                    QSqlError le = pDb->lastError();
                    stdo << QObject::tr("SQL open ERROR #") + le.nativeErrorCode() << endl;
                    stdo << "driverText   : " << le.driverText() << endl;
                    stdo << "databaseText : " << le.databaseText() << endl;
                }
                else {
                    QSqlQuery q(*pDb);
                    QString msg;
                    if (!checkDbVersion(q, msg)) {
                        stdo << msg;
                    }
                }
            }
            delete pDb;
        }
    } CATCHS(pe)

    exit(pe == nullptr ? 0 : pe->mErrorCode);
}
