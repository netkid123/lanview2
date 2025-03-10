/***************************************************************************
 *   Copyright (C) 2008 by Csiki Ferenc   *
 *   csikfer@indasoft.hu   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "lanview.h"

const QString&  snmpIfStatus(int __i, eEx __ex)
{
    switch(__i) {
    case IF_UP:             return _sUp;
    case IF_DOWN:           return _sDown;
    case IF_TESTING:        return _sTesting;
    case IF_UNKNOWN:        return _sUnknown;
    case IF_DORMAT:         return _sDormant;
    case IF_NOTPRESENT:     return _sNotPresent;
    case IF_LOWERLAYERDOWN: return _sLowerLayerDown;
    default:                if (__ex) EXCEPTION(EDATA);
    }
    return _sNul;
}

int  snmpIfStatus(const QString& __s, eEx __ex)
{
    if (!_sUp.compare(__s, Qt::CaseInsensitive))            return IF_UP;
    if (!_sDown.compare(__s, Qt::CaseInsensitive))          return IF_DOWN;
    if (!_sTesting.compare(__s, Qt::CaseInsensitive))       return IF_TESTING;
    if (!_sUnknown.compare(__s, Qt::CaseInsensitive))       return IF_UNKNOWN;
    if (!_sDormant.compare(__s, Qt::CaseInsensitive))       return IF_DORMAT;
    if (!_sNotPresent.compare(__s, Qt::CaseInsensitive))    return IF_NOTPRESENT;
    if (!_sLowerLayerDown.compare(__s, Qt::CaseInsensitive))return IF_LOWERLAYERDOWN;
    if (__ex)  EXCEPTION(EDATA);
    return 0;
}

/* *********************************************************************************************** */

int cTable::getRowIndex(const QString &ixColName, const QVariant& ixValue, eEx __ex)
{
    // Index or values column is not found
    if (!contains(ixColName)) {
        if (__ex != EX_IGNORE) EXCEPTION(ENONAME, -1, QObject::tr("Invalid index column name : %1").arg(ixColName));
        return NULL_IX;
    }
    // Index column
    tVariantVector& indexColumn = (*this)[ixColName];
    int rowIndex;
    for (rowIndex = 0; rowIndex < indexColumn.size(); rowIndex++) {
        if (indexColumn[rowIndex] == ixValue) {
            return rowIndex;
        }
    }
    // If not found
    if (__ex != EX_IGNORE) EXCEPTION(EFOUND, -1, QObject::tr("Index value %1 not found in %2 index column.").arg(debVariantToString(ixValue), ixColName));
    return NULL_IX;
}

QVariant *cTable::getCellPtr(const QString& ixColName, const QVariant& ixValue, const QString& colName, eEx __ex)
{
    // Values column is not found
    if (!contains(colName)) {
        if (__ex != EX_IGNORE) EXCEPTION(ENONAME, -1, QObject::tr("Invalid column name : %1").arg(colName));
        return nullptr;
    }
    // Row index by index value
    int rowIndex = getRowIndex(ixColName, ixValue, __ex);
    if (rowIndex < 0) return nullptr;
    return &((*this)[colName][rowIndex]);
}

cTable& cTable::operator<<(const QString& __cn)
{
    if (contains(__cn)) EXCEPTION(EDATA);
    tVariantVector col(rows());
    insert(__cn, col);
    return *this;
}

QString cTable::toString(void) const
{
    QStringList keylst = keys();
    if (keylst.isEmpty()) return _sEmpty;
    int     nr = rows(), i;
    QString     r, key;
    foreach (key, keylst) {
        r += key + QChar(',');
    }
    r.chop(1);
    r += QChar('\n');
    for (i = 0; i < nr; i++) {
        foreach (key, keylst) {
            const tVariantVector&  vv = operator[](key);
            if (vv.size() <= i) EXCEPTION(EPROGFAIL);
            QVariant v = vv[i];
            if (v.type() == QVariant::ByteArray && v.toByteArray().contains((char)0)) {
                r += dump(v.toByteArray()) + QChar(',');
            }
            else r += v.toString() + QChar(',');
        }
        r.chop(1);
        r += QChar('\n');
    }
    return r;
}

/* *********************************************************************************************** */
#ifdef SNMP_IS_EXISTS

QBitArray   bitString2Array(u_char *__bs, size_t __os)
{
    QBitArray   r(__os * 8, false);
    int         i;
    for (i = 0; __os > 0; ++__bs, --__os) {
        u_char  b = *__bs;
        for (int ii = 8; ii > 0; --ii, ++i, b <<= 1) {
            if (b & 0x80) {
                r.setBit(i, true);
            }
        }
    }
    return r;
}

/* *********************************************************************************************** */

const char *netSnmp::type = "qsnmp";
int netSnmp::objCnt = 0;
int netSnmp::explicitInit = 0;
bool netSnmp::inited = false;

netSnmp::netSnmp()
{
    if (objCnt < 0) EXCEPTION(EPROGFAIL);
    objCnt++;
    implicitInit();
    status = STAT_SUCCESS;
    emsg.clear();
}
netSnmp::~netSnmp()
{
    if (objCnt <= 0) EXCEPTION(EPROGFAIL);
    objCnt--;
    implicitDown();
}

void    netSnmp::implicitInit(void)
{
    if (inited) return;
    PDEB(SNMP) << "Init SNMP (" << type << ")" << endl;
    init_snmp(type); // Initialize the SNMP library
    SOCK_STARTUP;
    QStringList pl = lanView::getInstance()->pSet->value(_sMibPath).toString().split(QChar(','));
    QString dir;
    foreach (dir, pl) {
        add_mibdir(dir.toStdString().c_str());
        PDEB(SNMP) << "add_mib_dir(" << dir << ") ..." << endl;
    }
    inited = true;
}

void    netSnmp::implicitDown(void)
{
    if (!inited) return;    // Ha nem volt init, akkor nem kell
    if (objCnt != 0 || explicitInit != 0) return;   // Még nem esedékes a down
    SOCK_CLEANUP;
    PDEB(SNMP) << "Shutdown SNMP (" << type << ")" << endl;
    snmp_shutdown(type);
    inited = false;
}

void    netSnmp::init(void)
{
    // DBGFN();
    explicitInit++;
    implicitInit();
    // DBGFNL();
}
void    netSnmp::down(void)
{
    // DBGFN();
    explicitInit--;
    implicitDown();
    // DBGFNL();
}

void    netSnmp::clrStat(void)
{
    status = snmp_errno = STAT_SUCCESS;
    emsg.clear();
}
int     netSnmp::setStat(void)
{
    status = snmp_errno;
    if (status) emsg = snmp_api_errstring(status);
    else emsg.clear();
    return status;
}

int     netSnmp::setStat(bool __e, const QString& __em)
{
    if (!__e) {
        clrStat();
    }
    else {
        if (snmp_errno != STAT_SUCCESS) {
            status = snmp_errno;
            emsg = snmp_api_errstring(status);
        }
        else {
            status = 1;
            emsg = "[ERROR]";
        }
        if (__em.isEmpty() == false) emsg += QChar('/') + __em;
    }
    return status;
}

void netSnmp::setMibDirs(const char * __dl)
{
    init();
    netsnmp_set_mib_directory(__dl);
}


/* *********************************************************************************************** */

const oid cOId::zero  = 0;

cOId::cOId() : QVector<oid>(MAX_OID_LEN, zero), netSnmp()
{
    //PDEB(OBJECT) << __PRETTY_FUNCTION__ << " = " << VDEBPTR(this) << endl;
    oidSize = 0;
}
cOId::cOId(const oid *__oid, size_t __len) : QVector<oid>(MAX_OID_LEN), netSnmp()
{
    //PDEB(OBJECT) << __PRETTY_FUNCTION__ << QChar('(') << __oid << _sCommaSp << __len << ") = " << VDEBPTR(this) << endl;
    set(__oid, __len);
}
cOId::cOId(const char *__oid) : QVector<oid>(MAX_OID_LEN), netSnmp()
{
    //PDEB(OBJECT) << __PRETTY_FUNCTION__ << QChar('(') << __oid << ") = " << VDEBPTR(this) << endl;
    set(__oid);
}
cOId::cOId(const QString& __oid) : QVector<oid>(MAX_OID_LEN), netSnmp()
{
    //PDEB(OBJECT) << __PRETTY_FUNCTION__ << QChar('(') << __oid << ") = " << VDEBPTR(this) << endl;
    set(__oid);
}
cOId::cOId(const cOId& __oid) : QVector<oid>(MAX_OID_LEN), netSnmp()
{
    //PDEB(OBJECT) << __PRETTY_FUNCTION__ << QChar('(') << __oid.toString() << ") = " << VDEBPTR(this) << endl;
    set(__oid);
}
cOId::~cOId()
{
    //PDEB(OBJECT) << __PRETTY_FUNCTION__ << " = " << VDEBPTR(this) << endl;
}

bool    cOId::chkSize(size_t __len)
{
    if (__len > MAX_OID_LEN) return false;
    fill(zero, MAX_OID_LEN);
    return true;
}
cOId::operator bool() const
{
    if (oidSize > MAX_OID_LEN) EXCEPTION(EPROGFAIL);
    return oidSize >= MIN_OID_LEN && status == 0;
}
bool cOId::operator !() const
{
    return !(bool)(*this);
}

cOId&   cOId::set(const oid *__oid, size_t __len)
{
    clrStat();
    if (!chkSize(__len)) EXCEPTION(EOUTRANGE);
    if (__len) memcpy(data(), __oid, sizeof(oid) * __len);
    oidSize = __len;
    return *this;
}

cOId& cOId::set(const char * __oid)
{
    clrStat();
    fill(zero, MAX_OID_LEN);
    oidSize = MAX_OID_LEN;
#if 0
    // return size, if error, return 0
    if (0 == get_node(__oid, data(), &oidSize)) {
#else
    if ( nullptr == snmp_parse_oid(__oid, data(), &oidSize)) {
#endif
        oidSize = 0;
        setStat(true, QObject::tr("cOId& cOId::set(%1)").arg(QString(__oid)));
    }
    return *this;
}

QString cOId::description() const
{
    char    buf[SPRINT_MAX_LEN];
    int len = snprint_description(buf, SPRINT_MAX_LEN, const_cast<oid *>(data()), oidSize, 0);
    return len > 0 ? QString::fromUtf8(buf) : _sUnKnown;
}

QString cOId::toString() const
{
    if (oidSize == 0) return QString();
    char    buf[SPRINT_MAX_LEN];
    int len = snprint_objid(buf, SPRINT_MAX_LEN, const_cast<oid *>(data()), oidSize);
    return len > 0 ? QString::fromUtf8(buf) : _sUnKnown;
}

cOId cOId::mid(int _first, int _size)
{
    cOId r;
    uint i, e;
    if (_size > 0) {
        e = _first + _size;
        if (e > oidSize) e = oidSize;
    }
    else {
        e = oidSize;
    }
    for (i = _first; i < e; ++i) {
        int x = this->at(i);
        r << x;
    }
    return r;
}

QVariant cOId::toVariant() const
{
    QVariantList    v;
    for (unsigned int i = 0; i < oidSize; ++i) v << (unsigned int)operator[](i);
    return v;
}

QString cOId::toNumString() const
{
    QString r;
    for (unsigned int i = 0; i < oidSize; i++) {
        if (i) r += QChar('.');
        r += QString::number((int)(*this)[i]);
    }
    return r;
}

cMac cOId::toMac() const
{
    cMac r;
    if (oidSize >= 6) {
        qlonglong v = 0;
        for (int i = 6; i > 0; i--) {
            uint b = (*this)[oidSize - i];
            if (b > 255) return r;  // invalid
            v = (v << 8) + b;
        }
        r.set(v);
    }
    return r;
}

QHostAddress cOId::toIPV4(uint _in) const
{
    QHostAddress r;
    _in += 4;
    if (oidSize >= _in) {
        qint32 v = 0;
        for (int i = _in; i > 0; i--) {
            uint b = (*this)[oidSize - i];
            if (b > 255) return QHostAddress();  // invalid
            v = (v << 8) + b;
        }
        r = QHostAddress(v);
    }
    return r;
}

cOId& cOId::operator <<(int __i)
{
    if (oidSize + 1 > MAX_OID_LEN) EXCEPTION(EOUTRANGE);
    (*this)[oidSize] = __i;
    ++oidSize;
    return *this;
}

cOId& cOId::operator <<(const QString& __s)
{
    QString base = toString();
    return set(base + QChar('.') + __s);
}

bool  cOId::operator  <(const cOId& __o) const
{
    if (oidSize < __o.oidSize) {
        for (unsigned int i = 0; i < oidSize; i++) if ((*this)[i] != __o[i]) return false;
        return true;
    }
    return false;
}

bool  cOId::operator ==(const cOId& __o) const
{
    if (oidSize == __o.oidSize) {
        for (unsigned int i = 0; i < oidSize; i++) if ((*this)[i] != __o[i]) return false;
        return true;
    }
    return false;
}

cOId& cOId::operator -=(const cOId& __o)
{
    // _DBGFN() << " @(" << toString() << "," << __o.toString() << ")" << endl;
    if (__o < *this) {
        if (oidSize <= __o.oidSize) EXCEPTION(EPROGFAIL);
        remove(0, __o.oidSize);
        oidSize -= __o.oidSize;
    }
    else             clear();
    // DBGFNL();
    return *this;
}

/* *********************************************************************************************** */

cOIdVector::cOIdVector(const QStringList& __sl) : QVector<cOId>(__sl.size())
{
    int i,n = size();
    for (i = 0; i < n; ++i) {
        operator[](i).set(__sl[i]);
    }
}

QString cOIdVector::toString() const
{
    QString r = "[";
    for (int i = 0; i < size(); i++) {
        r += (*this)[i].toString() + QChar(',');
    }
    r.chop(1);
    return r + "]";
}

/* *********************************************************************************************** */

const char *cSnmp::defComunity = __sPublic;
const QString cSnmp::sSnmpTableIndex = "snmp_table_index";

cSnmp::cSnmp() : netSnmp()
{
    // PDEB(OBJECT) << __PRETTY_FUNCTION__ << " = " << (void *)this << endl;
    _init();
}

cSnmp::cSnmp(const char * __host, const char * __com, int __ver) : netSnmp()
{
    PDEB(OBJECT) << __PRETTY_FUNCTION__ << QChar('(') << __host << _sCommaSp << __ver << _sCommaSp << __ver << ") = " << (void *)this << endl;
    _init();
    open(__host, __com, __ver);
}

cSnmp::cSnmp(const QString& __host, const QString& __com, int __ver) : netSnmp()
{
    PDEB(OBJECT) << __PRETTY_FUNCTION__ << QChar('(') << __host << _sCommaSp << __ver << _sCommaSp << __ver << ") = " << (void *)this << endl;
    _init();
    open(__host, __com, __ver);
}


void cSnmp::_init(void)
{
    ss   = nullptr;
    pdu  = response = nullptr;
    //vars = NULL;
    count  = 0;
    memset(&session,0,sizeof(session));
    actVar = nullptr;
}

cSnmp::~cSnmp()
{
    // PDEB(OBJECT) << __PRETTY_FUNCTION__ << " = " << p2string(this) << endl;
    _clear();
}

void  cSnmp::_clear(void)
{
    if (session.peername)  SNMP_FREE(session.peername);
    if (session.community) SNMP_FREE(session.community);
    session.peername = nullptr;
    session.community = nullptr;
    session.community_len = 0;
    if (response) snmp_free_pdu(response);
    if (ss)       snmp_close(ss);
    response = pdu = nullptr;
    ss = nullptr;
    clrStat();
}

int cSnmp::open(const char * __host, const char * __com, int __ver)
{

    PDEB(VVERBOSE) << __PRETTY_FUNCTION__ << QChar('(') << __host << _sCommaSp << __ver << _sCommaSp << __ver << ")" << endl;
    _clear();
    snmp_sess_init(&session);
    session.peername        = strdup(__host);
    session.version         = __ver;
    session.community       = (u_char *)strdup(__com);
    session.community_len   = strlen(__com);
    ss = snmp_open(&session);
    return setStat(ss == nullptr, __PRETTY_FUNCTION__);
}

int cSnmp::open(const QString& __host, const QString& __com, int __ver)
{
    return open(__host.toStdString().c_str(), __com.toStdString().c_str(), __ver);
}

QVariant cSnmp::value(const netsnmp_variable_list * __var)
{
    // DBGFN();
    //static const char * snmpType = "SNMP variable type";
    QVariant    r;
    qulonglong  ul;
    if (nullptr != __var) {
        // PDEB(VVERBOSE) << " type = # " << (int)__var->type << "; ";
        switch (__var->type) {
            case ASN_BOOLEAN: //         ((u_char)0x01)
                r = bool(*(__var->val.integer));
                // _PDEB(VVERBOSE) << DBOOL(r.toBool()) << endl;
                break;
            case ASN_INTEGER: //         ((u_char)0x02)
                r = int(*(__var->val.integer));
                // _PDEB(VVERBOSE) << r.toInt() << endl;
                break;
            case 0x41:  // COUNTER32
            case 0x42:  // GAUGE32
            case 0x43:  // ?? PowerNet-MIB: time ticks ??
                r = u_int(*(__var->val.integer));
                //_PDEB(VVERBOSE) << r.toUInt() << endl;
                break;
            case ASN_APP_COUNTER64: //   ((u_char)0x46)
                ul   = __var->val.counter64->high;
                ul <<= 32;
                ul  += __var->val.counter64->low;
                r = ul;
                // _PDEB(VVERBOSE) << r.toULongLong() << endl;
                break;
            case ASN_BIT_STR: //         ((u_char)0x03)
                r = bitString2Array(__var->val.bitstring, __var->val_len);
                // _PDEB(VVERBOSE) << toString(r.toBitArray()) << endl;
                break;
            case ASN_APPLICATION: //     ((u_char)0x40)     hátha ??!!
            case ASN_OCTET_STR: //       ((u_char)0x04)
                r = QByteArray(reinterpret_cast<const char *>(__var->val.string), int(__var->val_len));
                // _PDEB(VVERBOSE) << "len = " << __var->val_len << " data = " << dump(r.toByteArray()) << endl;
                break;
            case ASN_NULL: //            ((u_char)0x05)
                // _PDEB(VVERBOSE) << "NULL" << endl;
                break;
            case ASN_OBJECT_ID: {//      ((u_char)0x06)
                // cOId _oid = cOId((oid *)__var->val.string, __var->val_len / sizeof(oid));    // Ez lenne a logikus
                size_t len = __var->val_len / sizeof(int);             // De 64 biten ez tűnik jónak
                cOId _oid = cOId(reinterpret_cast<oid *>(__var->val.string), len);
                if (len > 0) {
                    QString sOid;
                    for (int i = 0; i < int(len); ++i) {
                        sOid += QString::number(_oid[i]) + ".";
                    }
                    sOid.chop(1);
                    r = sOid;
                }
                // _PDEB(VVERBOSE) << r.toString() << endl;
                break;
            }
            case ASN_SEQUENCE: //        ((u_char)0x10)
            case ASN_SET: //             ((u_char)0x11)
            // case ASN_PRIMITIVE:       ((u_char)0x00)
            case ASN_UNIVERSAL: //       ((u_char)0x00)
            // case ASN_APPLICATION: //     ((u_char)0x40)
            // case ASN_LONG_LEN:        (0x80)
            // case ASN_BIT8:            (0x80)
            case ASN_CONTEXT: //         ((u_char)0x80)
            case ASN_PRIVATE: //         ((u_char)0xC0)
            case ASN_CONSTRUCTOR: //     ((u_char)0x20)
            case ASN_EXTENSION_ID: //    (0x1F)
                // EXCEPTION(ENOIMPL, __var->type, snmpType);
                // _PDEB(VVERBOSE) << "Other" << endl;
            default:
                // EXCEPTION(EENUMVAL, __var->type, snmpType);
                r = QChar((char)__var->type);
                // _PDEB(VVERBOSE) << "Unknown" << endl;
                break;
        }
    }
    else {
        DWAR() << "__var is NULL." << endl;
    }
    // DBGFNL();
    return r;
}

tVariantVector  cSnmp::values()
{
    tVariantVector  vv;
    for (first(); actVar != nullptr; next()) {
        vv << value();
    }
    return vv;
}

cOId cSnmp::name(const netsnmp_variable_list * __var)
{
    cOId r;
    if (__var != nullptr) {
        r.set(__var->name, __var->name_length);
    }
    return r;
}

cOIdVector cSnmp::names()
{
    cOIdVector r;
    for (first(); actVar != nullptr; next()) {
        r << name();
    }
    return r;
}


int cSnmp::type(const netsnmp_variable_list * __var)
{
    return __var == nullptr ? -1 : __var->type;
}


int cSnmp::__get(void)
{
    if (response != nullptr) {
        snmp_free_pdu(response);
        response = nullptr;
        actVar = nullptr;
    }
    status = snmp_synch_response(ss, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        (void)first();
    }
    else {
        if (status == STAT_SUCCESS)      status = response->errstat;
        else if (status == STAT_TIMEOUT) status = SNMPERR_TIMEOUT;
        else if (ss->s_snmp_errno != 0)  status = ss->s_snmp_errno;
        else                             status = SNMPERR_GENERR;
        emsg = snmp_api_errstring(status);
    }
    return status;
}
int cSnmp::_get(int cmd, const cOId& __oid)
{
    clrStat();
    pdu = snmp_pdu_create(cmd);
    snmp_add_null_var(pdu, __oid.data(), __oid.size());
    return __get();
}

int cSnmp::_get(int cmd, const cOIdVector& __oids)
{
    clrStat();
    pdu = snmp_pdu_create(cmd);
    for (int i = 0; i < __oids.size(); ++i) {
        const cOId& o = __oids[i];
        snmp_add_null_var(pdu, o.data(), o.size());
    }
    return __get();
}

bool cSnmp::checkOId(const cOId& __o)
{
    if (__o.size() > MAX_OID_LEN) EXCEPTION(EPROGFAIL);
    if (__o.status != 0) {
        status = __o.status;
        emsg   = __o.emsg.isEmpty() ? QObject::tr("Invalid OID") : __o.emsg;
        return false;
    }
    if (__o.size() < MIN_OID_LEN) {
        status = 1;
        emsg   = QObject::tr("OID is NULL");
        return false;
    }
    return true;
}

bool cSnmp::checkOId(const cOIdVector& __o)
{
    cOIdVector::const_iterator    it;
    for (it = __o.begin(); it != __o.end(); it++) {
        if (!checkOId(*it)) return status;
    }
    return true;
}

int cSnmp::get(const cOId& __oid)
{
    if (!checkOId(__oid)) return status;
    return _get(SNMP_MSG_GET, __oid);
}

int cSnmp::get(const cOIdVector& __oids)
{
    if (!checkOId(__oids)) return status;
    return _get(SNMP_MSG_GET, __oids);
}

int cSnmp::getNext(const cOId& __oid)
{
    if (!checkOId(__oid)) return status;
    return _get(SNMP_MSG_GETNEXT, __oid);
}

int cSnmp::getNext(const cOIdVector& __oids)
{
    if (!checkOId(__oids)) return status;
    return _get(SNMP_MSG_GETNEXT, __oids);
}

int cSnmp::getNext(void)
{
    int r;
    // Ez csak egy eredményes get vagy getNext után hívható.
    if (response == nullptr || response->variables == nullptr) EXCEPTION(EPROGFAIL);
    const netsnmp_variable_list *vl = response->variables;
    if (vl->next_variable == nullptr) {     // Egy elem
        cOId    o(vl->name, vl->name_length);
        r = getNext(o);
    }
    else {                      // Több elem
        int n, i;
        for (n = 0; vl != nullptr; vl = vl->next_variable) ++n; // megszámoljuk
        cOIdVector ov(n);
        for (i = 0, vl = response->variables; vl != nullptr; vl = vl->next_variable, ++i) ov[i].set(vl->name, vl->name_length);
        r = getNext(ov);
    }
    return r;
}

int cSnmp::getTable(const cOId& baseId, const QStringList& columns, cTable& result)
{
    if (!checkOId(baseId)) return status;
    cOIdVector  ov(columns.size());
    QString s;
    int i = 0;
    foreach (s, columns) {
        ov[i].set(baseId);
        ov[i] << s;
        if (!checkOId(ov[i])) return status;
        i++;
    }
    return getTable(ov, columns, result);
}

int cSnmp::columns2oids(const QString& baseId, const QStringList& columns, cOIdVector&  ov)
{
    ov.resize(columns.size());
    QString s, sb = baseId;
    if (!sb.endsWith(QChar(':'))) sb += QChar('.');
    int i = 0;
    foreach (s, columns) {
        cOId& o = ov[i];
        i++;
        o.set(sb + s);
        if (!checkOId(o)) return status;
    }
    return 0;   // OK
}

int cSnmp::getTable(const QString& baseId, const QStringList& columns, cTable& result)
{
    cOIdVector  ov;
    columns2oids(baseId, columns, ov);
    return getTable(ov, columns, result);
}

int cSnmp::checkTableColumns(const cOIdVector& Ids, QBitArray& result)
{
    if (getNext(Ids)) return 1;
    int ncol = Ids.size();
    result.resize(ncol);
    for (int i = 0; i < ncol; ++i) {    // Columns
        if (actVar == nullptr) EXCEPTION(EPROGFAIL);
        const cOId&     oib = Ids[i];       // Column base OID
        const cOId      oia = name();       // Cell OID
        result[i] = (oib < oia);            // Overrun? (if no)
        next();
    }
    return 0;   // OK
}

int cSnmp::getTable(const cOIdVector& Ids, const QStringList& columns, cTable& result, oid _maxRowIndex)
{
    _DBGFN() << "@([" << Ids.toString() << ", " << columns.join(",") << ", [[" << result.toString() << "]], " << _maxRowIndex << ")" << endl;
    int ncol = columns.size();      // Table column number
    if (Ids.size() != ncol) EXCEPTION(EPROGFAIL);
    if (getNext(Ids)) return status;
    int over = 0;               // Overruns counter
    oid row = 0;                // Row SNMP index
    result.clear();
    tVariantVector& ix  = result[sSnmpTableIndex];
    if (!first()) {
        PDEB(VERBOSE) << "first() is NULL." << endl;
        return 1;
    }
    while (true) {                          // ROWS
        for (int i = 0; i < ncol; ++i) {    // Columns
            if (actVar == nullptr) EXCEPTION(EPROGFAIL);
            const QString&  col = columns[i];   // Column name
            const cOId&     oib = Ids[i];       // Column base OID
            const cOId      oia = name();       // Cell OID
            tVariantVector& vv  = result[col];  // Result table column reference
            PDEB(SNMP) << "getTab : " << col << " : " << oib.toNumString() << " < " << oia.toNumString() << " = " << value().toString() << endl;
            if (oib < oia) {                    // Overrun? (if no)
                if (i == 0) {                   // First column
                    row = oia.last();           // Row SNMP index
                    if (row > _maxRowIndex) return 0;
                    ix << QVariant((int)row);
                }
                else if (row != oia.last()) {   // Check SNMP index
                    emsg = "Confused indexes : " + oia.toNumString() + " / " + QString::number(row);
                    return 1;
                }
                vv << value();
            }
            else {                              // Overrun, end of row
                if (over != i) {                // Mindegyik oszlopnak ugyanott van vége!
                    emsg = QString("Confused last column: key = %1, i(%2) != over(%3)").arg(col).arg(i).arg(over);
                    return 1;
                }
                ++over;
                if (over == ncol) return 0;        // Minden oszlopnak megvan a vége
            }
            next();
        }
        if (actVar != nullptr) EXCEPTION(EPROGFAIL);
        if (getNext()) return 1;
    }
}

int cSnmp::getXIndex(const cOId& xoid, QMap<int, int>& xix, bool reverse)
{
    int r = getNext(xoid);
    if (r != 0) return r;
    while (xoid < name()) {
        cOId ko = name() - xoid;
        if (ko.size() != 1) return -1;
        bool ok;
        int origIx = value().toInt(&ok);
        if (!ok) return -1;
        int newIx = ko.last();
        if (reverse) {
            xix[origIx] = newIx;
        }
        else {
            xix[newIx] = origIx;
        }
        // PDEB(VVERBOSE) << QString("xix[%1] = %2").arg(k).arg(ix) << endl;
        r = getNext();
        if (r) return r;
    }
    return 0;
}

int cSnmp::getBitMaps(const cOId& _oid, QMap<int, QBitArray>& maps)
{
    maps.clear();
    int r = getNext(_oid);
    if (r != 0) return r;
    while (_oid < name()) {
        cOId ko = name() - _oid;
        if (ko.size() != 1) return -1;
        int ix = ko.last();
        QVariant v = value();
        QBitArray ba;
        switch (v.userType()) {
        case QVariant::BitArray:    // ?
            ba = v.toBitArray();
            break;
        case QVariant::ByteArray: {
            QByteArray a = v.toByteArray();
            ba = bitString2Array((u_char *)a.data(), a.size());
            break;
            }
        default:
            return -1;
        }
        maps[ix] = ba;
        r = getNext();
        if (r) return r;
    }
    return 0;
}


QString snmpNotSupMsg()
{
    EXCEPTION(EPROGFAIL);
    return _sNul;
}

#else  // SNMP_IS_EXISTS

/*
cSnmp::async::async()
{
    _init();
}
cSnmp::async::~async()
{
    clear();
}
cSnmp::async::clear(void)
{
    if (state != READY) EXCEPTION(EPROGFAIL);
    if (response) snmp_free_pdu(response);
    if (ss)       snmp_close(ss);
    response = pdu = NULL;
    ss = NULL;
}
cSnmp::async::_init(void)
{
    response = pdu = NULL;
    ss = NULL;
    state = READY;
}

int cSnmp::async::asyncResponse(init __op, snmp_session *__ss, int __id, snmp_pdu *__resp, voi * __magic)
{
    async&   s = *(async *)__magic;
    s.state    = EVENT;
    s.response = pdu;
}
*/

QString snmpNotSupMsg()
{
    return QObject::tr("Az snmp modul nincs engedélyezve");
}

#endif // SNMP_IS_EXISTS
