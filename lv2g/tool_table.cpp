﻿#include "tool_table.h"
#include "popupreport.h"
#include <QtWebEngineWidgets>

cToolTable::cToolTable(cTableShape *pts, bool _isDialog, cRecordsViewBase *_upper, QWidget * par)
    : cRecordTable(pts, _isDialog, _upper, par)
{
    if (pts->getName(_sTableName).compare(_sToolObjects) == 0) {
        if (pUpper == nullptr) {
            subType = ST_TOOL_OBJ_ALL;
            // Edit, insert disabled
            pTableShape->clear(_sEditRights);
            pTableShape->clear(_sInsertRights);
        }
        else {
            subType = ST_TOOL_OBJ_CHILD;
            pTableShape->setFieldFlags(_sObjectId, ENUM2SET(FF_READ_ONLY));
            pTableShape->setFieldFlags(_sObjectName, ENUM2SET(FF_READ_ONLY));
            buttons << DBT_SPACER << DBT_EXEC;
            sProd = QSysInfo::productType();
            sKern = QSysInfo::kernelType();
        }
        return;
    }
    EXCEPTION(EDATA, -1, tr("Invalid or not supported shape type."));
}

cRecordViewModelBase * cToolTable::newModel()
{
    return new cToolTableModel(*this);
}

int cToolTable::ixToForeignKey()
{
    int ix = recDescr().toIndex(_sObjectId);
    return ix;
}

QStringList cToolTable::where(QVariantList& qParams)
{
    DBGFN();
    QStringList wl;
    if (flags & (RTF_IGROUP | RTF_NGROUP | RTF_IMEMBER | RTF_NMEMBER)) {
        EXCEPTION(ECONTEXT, flags, tr("Invalid table shape type."));
    }
    switch (subType) {
    case ST_TOOL_OBJ_ALL:
        break;
    case ST_TOOL_OBJ_CHILD:
        if (owner_id == NULL_ID) {  // A tulajdonos/tag rekord nincs kiválasztva
            wl << _sFalse;      // Ezzel jelezzük, hogy egy üres táblát kell megjeleníteni
            return wl;          // Ez egy üres tábla lessz!!
        }
        int ofix = ixToForeignKey();
        cRecord *pActOwner = pUpper->actRecord();
        if (pActOwner == nullptr) EXCEPTION(EPROGFAIL);
        QString ooname = pActOwner->tableName();
        wl << dQuoted(_sObjectName) + " = " + quoted(ooname);       // Object (owner) type filter
        wl << dQuoted(recDescr().columnName(ofix)) + " = " + QString::number(owner_id); // Owner id filter
        break;
    }
    wl << filterWhere(qParams);
    wl << refineWhere(qParams);

    DBGFNL();
    return wl;
}


void cToolTable::buttonPressed(int id)
{
    if (id == DBT_EXEC) {
        QString msg;
        if (!execute(msg) && !msg.isEmpty()) {
            cMsgBox::error(msg);
        }
    }
    else cRecordsViewBase::buttonPressed(id);
}

void cToolTable::insert(bool _similar)
{
    if (pUpper == nullptr) EXCEPTION(EPROGFAIL);
    if (owner_id == NULL_ID) return;
    cRecord *pActOwner = pUpper->actRecord();
    if (pActOwner == nullptr) EXCEPTION(EPROGFAIL);
    // Dialógus a leíró szerint összeállítva
    // A dialógusban megjelenítendő nyomógombok.
    qlonglong buttons = enum2set(DBT_OK, DBT_INSERT, DBT_CANCEL);
    cRecordDialog   rd(*pTableShape, buttons, true, nullptr, this, pWidget());  // A rekord szerkesztő dialógus
    pRecordDialog = &rd;
    cRecord *pRec = actRecord();    // pointer az aktuális rekordra, a beolvasott/megjelenített rekord listában
    if (_similar && pRec != nullptr) {
        cRecord *pR = pRec->dup();
        QBitArray c = pR->autoIncrement();
        pR->clear(c);
        rd.restore(pR);
        delete pR;
    }
    else {
        cRecordAny *pR = new cRecordAny(&recDescr());
        pR->set(_sObjectId,  pActOwner->getId());
        pR->set(_sObjectName,pActOwner->tableName());
        rd.restore(pR);
        delete pR;
    }
    while (1) {
        int r = rd.exec();
        if (r == DBT_INSERT || r == DBT_OK) {   // Csak az OK, és Insert gombra csinálunk valamit (egyébként: break)
            bool ok = rd.accept();
            if (ok) {
                cRecord *pRec = rd.record().dup();
                ok = pModel->insertRec(pRec);   // A pointer a pModel tulajdona lesz, ha OK
                if (!ok) pDelete(pRec);
            }
            if (ok) {
                if (r == DBT_OK) break; // Ha OK-t nyomott becsukjuk az dialóg-ot
                continue;               // Ha Insert-et, akkor folytathatja a következővel
            }
            else {
                //QMessageBox::warning(pWidget(), tr("Az új rekord beszúrása sikertelen."), rd.errMsg());
                continue;
            }
        }
        break;  // while
    }
    pRecordDialog = nullptr;
}

void cToolTable::modify(eEx)
{
    QModelIndex index = actIndex();
    if (index.isValid() == false) return;
    if (pRecordDialog != nullptr) {
        // ESC-el lépett ki, Kideríteni mi ez, becsukja az ablakot, de nem lép ki a loop-bol.
        pDelete(pRecordDialog);
        // EXCEPTION(EPROGFAIL);
    }

    cRecord *pRec = actRecord(index);    // pointer az aktuális rekordra, a beolvasott/megjelenített rekord listában
    if (pRec == nullptr) return;
    pRec = pRec->dup();           // Saját másolat
    qlonglong buttons;
    if (isReadOnly) {
        buttons = enum2set(DBT_CANCEL, DBT_NEXT, DBT_PREV);
    }
    else {
        buttons = enum2set(DBT_OK, DBT_CANCEL, DBT_NEXT, DBT_PREV);
    }
    // Létrehozzuk a dialogot
    pRecordDialog = new cRecordDialog(*pTableShape, buttons, true, nullptr, this, pWidget());

    int id = DBT_NEXT;
    while (1) {
        pRecordDialog->restore(pRec);     // lemásolja a dialogus saját adatterületére
        id =  pRecordDialog->exec(false);
        if (pRecordDialog->disabled()) id = DBT_CANCEL;
        // Ellenörzés, következő/előző, vagy kilép
        int updateResult = 0;
        switch(id) {
        case DBT_OK:
        case DBT_NEXT:
        case DBT_PREV: {
            updateResult = 1;
            if (isReadOnly) {
                pDelete(pRec);
            }
            else {
                // Update DB
                bool r = pRecordDialog->accept(); // Bevitt adatok rendben?
                if (!r) {
                    continue;
                }
                else {
                    // Leadminisztráljuk kiírjuk. Ha hiba van, azt a hívott metódus kiírja, és újrázunk.
                    pRec->copy(pRecordDialog->record());
                    updateResult = pModel->updateRec(index, pRec);
                    if (!updateResult) {
                        continue;
                    }
                    // Nincs felszabadítva, de már nem a mienk (betettük a rekord listába, régi elem felszabadítva)
                    pRec = nullptr;
                }
            }
            if (id == DBT_OK) {
                // A row-select-et helyreállítjuk (updateRow elszúrhatta
                if (index.isValid()) {
                    selectRow(index);
                }
                break;    // OK: vége
            }
            // A nexRow prevRow egy allokált objektumot ad vissza a bemásolt adattartalommal.
            if (id == DBT_NEXT) {       // A következő szerkesztése
                pRec = nextRow(&index, updateResult);
            }
            else {                      // Az előző szerkesztése
                pRec = prevRow(&index, updateResult);
            }
            if (pRec == nullptr) break;    // Nem volt előző/következő
            continue;
        }
        case DBT_CANCEL:
            pDelete(pRec);
            break;
        default:
            EXCEPTION(EPROGFAIL);
        }
        break;
    }
    pDelete(pRec);
    pRecordDialog = nullptr;
}

void cToolTable::setEditButtons()
{
    switch (subType) {
    case ST_TOOL_OBJ_ALL:
        break;
    case ST_TOOL_OBJ_CHILD:
        cRecord *pAct = actRecord();
        bool enable = pAct != nullptr;
        if (enable) {
            qlonglong id = pAct->getId(_sToolId);
            enable = execSqlBoolFunction(*pq, "check_platform", id, sKern, sProd);
        }
        pButtons->button(DBT_EXEC)->setEnabled(enable);
        break;
    }
    cRecordTable::setEditButtons();
}

bool cToolTable::execute(QString& msg)
{
    QString key;
    // Actual tool_objects record
    pToolObj = actRecord();
    if (pToolObj == nullptr) return false;
    qlonglong toolId = pToolObj->getId(_sToolId);
    // Tools record
    tool.setType(_sTools);
    tool.setById(*pq, toolId);
    // Object record
    object.setType(pToolObj->getName(_sObjectName));
    object.setById(*pq, owner_id);
    // Features
    cFeatures f;
    features.clear();
    features.split(tool.getName(_sFeatures));
    f.split(pToolObj->getName(_sFeatures));
    features.merge(f);
    if (object.isIndex(_sFeatures)) {
        f.clear();
        f.split(object.getName(_sFeatures));
        key = mCat("tool", tool.getName());
        features.merge(f, key);
    }
    QString type = tool.getName(_sToolType);
    QString stmt = tool.getName(_sToolStmt);
    // substitute $
    eMsg.clear();
    stmt = substitute(*pq, this, stmt,
        [] (const QString& key, QSqlQuery& q, void *_p)
            {
                QString r;
                cToolTable *p = static_cast<cToolTable *>(_p);
                r = p->getParValue(q, key);
                return r;
            });
    /****/
    if (!eMsg.isEmpty()) {
        msg = eMsg;
        eMsg.clear();
        return false;
    }
    if (!features.contains(_sWait)) features[_sWait] = tool.getName(_sWait);
#if defined(Q_CC_GNU)
    if (0 == type.compare(_sTerminal)) {
        return exec_term(stmt, features, msg);
    }
#endif
    if (0 == type.compare(_sCommand)) {
        return exec_command(stmt, features, msg);
    }
    if (0 == type.compare(_sUrl)) {
        return exec_url(stmt, features, msg);
    }
    return false;
}
QString cToolTable::getParValue(QSqlQuery& q, const QString& key)
{
    if (object.isIndex(key))    return object.getName(key);
    if (pToolObj->isIndex(key)) return pToolObj->getName(key);
    if (tool.isIndex(key))      return tool.getName(key);
    QString val = features.value(key);
    if (val.isEmpty()) val = cSysParam::getTextSysParam(q, key);
    if (!val.isEmpty()) {
        if (val.startsWith("select", Qt::CaseInsensitive)) {
            // 0: "select"
            // 1: selected field name
            // 2: JOIN ...
            // 3: WHILE <act. record> AND ...   / Optional
            // 4: ORDER ...                     / Optional
            QStringList sl = cFeatures::value2list(val);
            if (sl.size() < 3) {
                eMsg = tr("Hiányos lekérdezés paraméterek.");
                return _sNul;
            }
            QString sql;
            sql = "SELECT " + sl.at(1) + " FROM " + object.tableName();
            sql += " JOIN " + sl.at(2);
            sql += " WHERE " + object.idName() + " = " + QString::number(owner_id);
            if (isContIx(sl, 3) && !sl.at(3).isEmpty()) {
                sql += " AND " + sl.at(3);
            }
            if (isContIx(sl, 4) && !sl.at(4).isEmpty()) {
                sql += " ORDER BY " + sl.at(4);
            }
            sql += " LIMIT 1";
            if (!q.exec(sql)) {
                eMsg = tr("Hibás lekérdezés : %1").arg(sql);
                return _sNul;
            }
            if (!q.first()) {
                eMsg = tr("A lekérdezés nem adott vissza értéket : %1").arg(sql);
                return _sNul;
            }
            return q.value(0).toString();
        }
        else {
            return val;
        }
    }
    eMsg = tr("Behelyettesítési hiba. Ismeretlen név : %1").arg(key);
    return _sNul;
}

bool cToolTable::exec_command(const QString& stmt, cFeatures& __f, QString& msg)
{
    _DBGFN() << VDEB(stmt) << endl;
    bool wait = str2bool(__f.value(_sWait), EX_IGNORE);
    if (wait) {
        ulong strt_to = getTimeout(__f, _sStartTimeout, PROCESS_START_TO);
        ulong stop_to = getTimeout(__f, _sStopTimeout,  PROCESS_STOP_TO);
        PDEB(INFO) << "Start and wait : " << dQuoted(stmt) << VDEB(strt_to) << VDEB(stop_to) << endl;
        int r = startProcessAndWait(stmt, QStringList(), &msg, strt_to, stop_to);
        return r == 0;
    }
    else {
        PDEB(INFO) << "Start dateched : " << dQuoted(stmt) << endl;
        bool r = QProcess::startDetached(stmt);
        return  r;
    }
}

bool cToolTable::exec_url(const QString& _url, cFeatures& __f, QString& msg)
{
    (void)__f;
    // bool wait = str2bool(__f.value(_sWait), EX_IGNORE);
    QUrl url(_url);
    if (url.isEmpty()) {
        msg += tr("Invalid URL: %1").arg(_url);
        return false;
    }
    QWebEngineView *pView = new QWebEngineView();
    QMdiSubWindow *pSubWindow = new QMdiSubWindow;
    pSubWindow->setWidget(pView);
    pSubWindow->setAttribute(Qt::WA_DeleteOnClose);
    lv2g::pMdiArea()->addSubWindow(pSubWindow);
    pView->load(url);
    pView->show();
    QString title = object.getName() + " / " + tool.getName() + " (" + _url + ")";
    pView->setWindowTitle(title);
    return true;
}

#if defined(Q_CC_GNU)

cTerminal::cTerminal(const QString& stmt, cFeatures& __f, QString& msg)
    : QWidget(), pUi(new Ui::Terminal), pTerminal(new QTermWidget(0))
{
    (void)__f;
    (void)msg;
    pUi->setupUi(this);
    QFont font = QApplication::font();
    font.setFamily("Monospace");
    font.setPointSize(12);
    pTerminal->setTerminalFont(font);
    pTerminal->setArgs(QStringList() << "-c" << stmt);
    pTerminal->setShellProgram("bash");

    pTerminal->setAutoClose(true);
    pTerminal->startShellProgram();
    pUi->verticalLayout->insertWidget(0, pTerminal);

    connect(pUi->toolButtonZoomIn, SIGNAL(clicked()), pTerminal, SLOT(zoomIn()));
    connect(pUi->toolButtonZoomOut,SIGNAL(clicked()), pTerminal, SLOT(zoomOut()));
    connect(pUi->toolButtonClear,  SIGNAL(clicked()), pTerminal, SLOT(clear()));
    connect(pTerminal, SIGNAL(receivedData(QString)), this, SLOT(appendBuffer(QString)));
    connect(pUi->pushButtonSaveLog, SIGNAL(clicked()), this, SLOT(save()));
    connect(pUi->toolButtonClearLog, SIGNAL(clicked()), this, SLOT(clearLog()));

    pTerminal->show();
}

void cTerminal::appendBuffer(const QString& text)
{
    if (pUi->checkBoxLog->isChecked()) {
        buffer += text;
        pUi->pushButtonSaveLog->setDisabled(buffer.isEmpty());
    }
}

void cTerminal::save()
{
    static QString fileName;
    cFileDialog::textToFile(fileName, buffer, this);
}

void cTerminal::clearLog()
{
    buffer.clear();
    pUi->pushButtonSaveLog->setDisabled(true);
}

bool cToolTable::exec_term(const QString& stmt, cFeatures& __f, QString& msg)
{

    (void)__f;
    (void)msg;
    cTerminal *pTerminal = new cTerminal(stmt, __f, msg);

    QMdiSubWindow *pSubWindow = new QMdiSubWindow;
    lv2g::pMdiArea()->addSubWindow(pSubWindow);
    pSubWindow->setWidget(pTerminal);
    pSubWindow->setAttribute(Qt::WA_DeleteOnClose);

    connect(pTerminal->pTerminal, SIGNAL(finished()), pSubWindow, SLOT(close()));

    QString title = object.getName() + " / " + tool.getName() + " (terminal)";
    pSubWindow->setWindowTitle(title);
    pSubWindow->show();
    lv2g::pMdiArea()->setActiveSubWindow(pSubWindow);
    return true;
}
#endif

QVariant cToolTableModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();      // Sor index a táblázatban
    if (row >= _records.size()) return QVariant();
    const cRecord *pr = _records.at(row);
    int col = index.column();   // oszlop index a táblázatban
    if (col >= _col2field.size())   return QVariant();
    int fix = _col2field[col];  // Mező index a (fő)rekordban
    cToolTable& par = static_cast<cToolTable&>(recordView);
    switch (par.subType) {
    case cToolTable::ST_TOOL_OBJ_ALL:
        if (fix == ixObjectId) {
            cRecordAny o(pr->getName(ixObjectName));
            qlonglong oid = pr->getId(ixObjectId);
            QString n = o.getNameById(*pq, oid, EX_IGNORE);
            if (role == Qt::DisplayRole) {
                return n.isEmpty() ? QString("#%1").arg(oid) : n;
            }
            else {
                return dcRole(n.isEmpty() ? DC_ERROR : DC_DERIVED, role);
            }
        }
        break;
    case cToolTable::ST_TOOL_OBJ_CHILD:
        if (role == Qt::FontRole) {
            qlonglong tid = pr->getId(_sToolId);
            QString sKern = par.sKern;
            QString sProd = par.sProd;
            bool enable = execSqlBoolFunction(*pq, "check_platform", tid, sKern, sProd);
            if (!enable) return dcRole(DC_NOT_PERMIT, Qt::FontRole);
        }
        else {
            if (fix == ixObjectId) {
                if (role == Qt::DisplayRole) {
                    cRecordAny o(pr->getName(ixObjectName));
                    return o.getNameById(*pq, pr->getId(ixObjectId));
                }
                else {
                    return dcRole(DC_DERIVED, role);
                }
            }
        }
    }
    return cRecordTableModel::data(index, role);
}
