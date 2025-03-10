#include "findbymac.h"
#include "srvdata.h"
#include "lv2link.h"
#include "cerrormessagebox.h"
#include "scan.h"

#include "ui_findbymac.h"


cFBMExpThread::cFBMExpThread(cMac& _mac, QHostAddress& _ip, cSnmpDevice& _st, cFindByMac * _par)
    : QThread(_par), pEQ(nullptr), mac(_mac), ip(_ip), st(_st)
{
    ;
}

void cFBMExpThread::run()
{
    pEQ = cExportQueue::init(false);
    connect(pEQ, SIGNAL(ready()), this, SLOT(readyLine()));
    exploreByAddress(mac, ip, st);
    QString s;
    while (!(s = pEQ->pop()).isEmpty()) expLine(s);
}

void cFBMExpThread::readyLine()
{
    expLine(pEQ->pop());
}

static const QString _sNMap = "nmap";

cPopUpNMap::cPopUpNMap(QWidget *par, const QString& sIp)
    : cPopupReportWindow(par, tr("Start nmap..."), tr("%1 nmap report").arg(sIp))
    , process(new QProcess(this))
{
    pButtonSave->setDisabled(true);
    QStringList args("-A");
    args << sIp;
    connect(process, SIGNAL(started()),     this, SLOT(processStarted()));
    connect(process, SIGNAL(finished(int)), this, SLOT(processFinished(int)));
    connect(process, SIGNAL(readyRead()),   this, SLOT(processReadyRead()));
    connect(process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
    first = true;
    process->start(_sNMap, args);
}

void cPopUpNMap::processStarted()
{
    pTextEdit->setText(tr("nmap started ..."));
}

void cPopUpNMap::processFinished(int ec)
{
    if (ec == 0) {
        pTextEdit->append(htmlGrInf(_sOk));
    }
    else {
        pTextEdit->append(htmlError(tr("Exit code : %1").arg(ec)));
    }
    pButtonSave->setEnabled(true);
}

void cPopUpNMap::processReadyRead()
{
    QString out;
    out = process->readAllStandardOutput();
    if (!out.isEmpty()) {
        static QChar cSp(' ');
        static QChar cNl('\n');
        QStringList lines = out.split(cNl);
        foreach (QString line, lines) {
            if (line.isEmpty()) {
                if (!first) {
                    text += sHtmlBr;
                }
            }
            else {
                int n = line.indexOf(cSp);
                if (n >= 0 && n < 8) {
                    QString s;
                    if (n > 0) {
                        s = line.mid(0, n);
                        line.remove(0, n);
                    }
                    do {
                        s += sHtmlNbsp;
                        line = line.remove(0,1);
                    } while (line.startsWith(cSp));
                    line = s + toHtml(line);
                }
                text += line + sHtmlBr;
            }
            first = false;
        }
    }
    out = process->readAllStandardError();
    if (!out.isEmpty()) text += htmlError(out, true);
    pTextEdit->setHtml(text);
    resizeByText();
    first = false;
    // PDEB(VERBOSE) << "Text : \n" << text << endl;
}

void cPopUpNMap::processError(QProcess::ProcessError e)
{
    QString text;
    text = ProcessError2Message(e);
    pTextEdit->append(htmlError(text, true));
}

const enum ePrivilegeLevel cFindByMac::rights = PL_VIEWER;
eTristate cFindByMac::nmapExists = TS_NULL;

cFindByMac::cFindByMac(QMdiArea *parent) :
    cIntSubObj(parent),
    pUi(new Ui::FindByMac)
{
    pq = newQuery();
    pThread = nullptr;
    fMAC = fIP = fSw = false;
    pUi->setupUi(this);
    setAllMac();
    // RDP Windows-on, ha ez egy RDP kliens
#if defined(Q_OS_WINDOWS)
    rdpClientAddr = rdpClientAddress();
    if (rdpClientAddr.isNull()) {
        pUi->pushButtonRDP->hide();
    }
#else   // not defined(Q_OS_WINDOWS)
    pUi->pushButtonRDP->hide();
#endif  // defined(Q_OS_WINDOWS)

#ifdef SNMP_IS_EXISTS
    setSwitchs();
#else   // SNMP_IS_EXISTS
    pUi->comboBoxSw->hide();
    pUi->labelSw->hide();
    pUi->pushButtonExplore->hide();
    fSw = false;
#endif  // SNMP_IS_EXISTS
    if (nmapExists == TS_NULL) {
        QProcess    nmapTest;
        nmapTest.start(_sNMap, QStringList("-V"));
        if (!nmapTest.waitForFinished(5000)) nmapTest.kill();
        nmapExists = nmapTest.exitCode() == 0 ? TS_TRUE : TS_FALSE;
    }
    if (nmapExists == TS_FALSE) {
        pUi->pushButtonNMap->hide();
    }
    on_comboBoxMAC_currentTextChanged(pUi->comboBoxMAC->currentText());
}

cFindByMac::~cFindByMac()
{
    delete pq;
}

void cFindByMac::setMacAnIp(const cMac& mac, const QHostAddress& a)
{
    pUi->comboBoxIP->setCurrentText(a.toString());
    pUi->comboBoxMAC->setCurrentText(mac.toString());
}

void cFindByMac::setAllMac()
{
    // Query: All known MAC
    static const QString sql =
            "SELECT hwaddress FROM ("
               " SELECT DISTINCT(hwaddress) FROM arps"
              " UNION DISTINCT"
               " SELECT hwaddress FROM mactab"
              " UNION DISTINCT"
               " SELECT DISTINCT(hwaddress) FROM interfaces WHERE hwaddress IS NOT NULL) AS macs"
             " ORDER BY hwaddress ASC";
    QStringList items;
    if (execSql(*pq, sql)) do {
        // pUi->comboBox->addItem(pq->value(0).toString()); // SLOW !!
        items << pq->value(0).toString();
    } while (pq->next());
    pUi->comboBoxMAC->clear();
    pUi->comboBoxMAC->addItems(items);
}

void cFindByMac::setSwitchs()
{
#ifdef SNMP_IS_EXISTS
    static const QString sql = "SELECT node_name FROM snmpdevices WHERE 'switch' = ANY (node_type) ORDER BY node_name";
    QStringList items;
    QString centralSwitchName = cSysParam::getTextSysParam(*pq, "central_switch_name");
    int currentIndex = 0;
    if (execSql(*pq, sql)) do {
        // pUi->comboBox->addItem(pq->value(0).toString()); // SLOW !!
        QString switchName = pq->value(0).toString();
        if (switchName == centralSwitchName) currentIndex = items.size();
        items << switchName;
    } while (pq->next());
    pUi->comboBoxSw->clear();
    pUi->comboBoxSw->addItems(items);
    fSw = !items.isEmpty();
    if (fSw) pUi->comboBoxSw->setCurrentIndex(currentIndex);
#endif
}

void cFindByMac::setButtons()
{
    pUi->pushButtonFindMac->setEnabled(fMAC);
    pUi->pushButtonFindIp->setEnabled(fIP);
    pUi->pushButtonNMap->setEnabled(fIP);
    pUi->pushButtonExplore->setEnabled(fMAC && fIP && fSw);
    pUi->toolButtonMAC2IP->setEnabled(fMAC);
    pUi->toolButtonIP2MAC->setEnabled(fIP);
}

void cFindByMac::appendReport(const QString& s)
{
    if (!sReport.isEmpty()) sReport += sHtmlBr;
    sReport += s;
    pUi->textEdit->setHtml(sHtmlHead + sReport + sHtmlTail);
}


void cFindByMac::on_comboBoxMAC_currentTextChanged(const QString& s)
{
    fMAC = cMac::isValid(s);
    setButtons();
}

void cFindByMac::on_comboBoxIP_currentTextChanged(const QString& s)
{
    fIP = !QHostAddress(s).isNull();
    setButtons();
}

void cFindByMac::on_pushButtonClear_clicked()
{
    pUi->pushButtonLocalhost->setEnabled(true);
    pUi->comboBoxLoMac->setDisabled(true);
    pUi->comboBoxLoMac->clear();
    pUi->comboBoxLoIp->setDisabled(true);
    pUi->comboBoxLoIp->clear();
    setAllMac();
    pUi->comboBoxIP->clear();
    sReport.clear();
}

void cFindByMac::on_pushButtonFindMac_clicked()
{
    QString sMac = pUi->comboBoxMAC->currentText();
    QString text = htmlReportByMac(*pq, sMac);
    appendReport(text);
}

void cFindByMac::on_pushButtonExplore_clicked()
{
#ifdef SNMP_IS_EXISTS
    QSqlQuery q = getQuery();
    QHostAddress ip(pUi->comboBoxIP->currentText());
    cMac mac(pUi->comboBoxMAC->currentText());
    QString swName = pUi->comboBoxSw->currentText();
    cSnmpDevice sw;
    if (ip.isNull() || !mac.isValid() || !sw.fetchByName(q, swName)) {
        appendReport(htmlError(tr("Hibás adatok.")));
        return;
    }
    pThread = new cFBMExpThread(mac, ip, sw, this);
    connect(pThread, SIGNAL(expLine(QString)), this, SLOT(expLine(QString)));
    connect(pThread, SIGNAL(finished()), this, SLOT(finished()));
    pUi->pushButtonClear->setDisabled(true);
    pUi->pushButtonExplore->setDisabled(true);
    pUi->pushButtonFindMac->setDisabled(true);
    pUi->pushButtonSave->setDisabled(true);
    pThread->start();
#endif
}

void cFindByMac::on_toolButtonIP2MAC_clicked()
{
    QHostAddress ip(pUi->comboBoxIP->currentText());
    cMac mac = cArp::ip2mac(*pq, ip, EX_IGNORE);
    if (mac.isValid()) pUi->comboBoxMAC->setCurrentText(mac.toString());
}

void cFindByMac::on_toolButtonMAC2IP_clicked()
{
    cMac mac(pUi->comboBoxMAC->currentText());
    QList<QHostAddress> ipl = cArp::mac2ips(*pq, mac);
    pUi->comboBoxIP->clear();
    foreach (QHostAddress a, ipl) {
        pUi->comboBoxIP->addItem(a.toString());
    }
    pUi->comboBoxIP->setCurrentIndex(0);
}

void cFindByMac::expLine(QString s)
{
    appendReport(s);
}

void cFindByMac::finished()
{
    pUi->pushButtonClear->setDisabled(false);
    pUi->pushButtonFindMac->setDisabled(false);
    pUi->pushButtonSave->setDisabled(false);
}

void cFindByMac::on_pushButtonFindIp_clicked()
{
    QString sIp = pUi->comboBoxIP->currentText();
    QString text = htmlReportByIp(*pq, sIp);
    appendReport(text);
}

void cFindByMac::on_pushButtonNMap_clicked()
{
    QString sIp = pUi->comboBoxIP->currentText();
    new cPopUpNMap(this, sIp);
}

void cFindByMac::on_pushButtonLocalhost_clicked()
{
    pUi->pushButtonLocalhost->setDisabled(true);
    myInterfaces = QNetworkInterface::allInterfaces();
    QMutableListIterator<QNetworkInterface>    i(myInterfaces);
    cMac    mac;
    while (i.hasNext()) {
        QNetworkInterface &iface = i.next();
        mac = iface.hardwareAddress();
        if (mac.isValid()) {
            pUi->comboBoxLoMac->addItem(mac.toString());
        }
        else {
            i.remove();
        }
    }
    bool e = !myInterfaces.isEmpty();
    pUi->comboBoxLoMac->setEnabled(e);
    if (e) {
        pUi->comboBoxLoMac->setCurrentIndex(0);
        on_comboBoxLoMac_activated(0);
    }
}

void cFindByMac::on_comboBoxLoMac_activated(int index)
{
    pUi->comboBoxMAC->setCurrentText(myInterfaces.at(index).hardwareAddress());
    bool e = false;
    foreach (QNetworkAddressEntry ae, myInterfaces.at(index).addressEntries()) {
        QHostAddress a = ae.ip();
        if (!a.isNull()) {
            pUi->comboBoxLoIp->addItem(a.toString());
            e = true;
        }
    }
    pUi->comboBoxLoIp->setEnabled(e);
    if (e) {
        pUi->comboBoxLoIp->setCurrentIndex(0);
        on_comboBoxLoIp_activated(pUi->comboBoxLoIp->currentText());
    }
}

void cFindByMac::on_comboBoxLoIp_activated(const QString &arg1)
{
    pUi->comboBoxIP->setCurrentText(arg1);
}

void cFindByMac::on_pushButtonRDP_clicked()
{
#if defined(Q_OS_WINDOWS)
    rdpClientAddr = rdpClientAddress();
    pUi->comboBoxIP->setCurrentText(rdpClientAddr.toString());
    on_toolButtonIP2MAC_clicked();
#endif  // defined(Q_OS_WINDOWS)
}


void cFindByMac::on_pushButtonSave_clicked()
{
    static QString fileName;
    cFileDialog::textEditToFile(fileName, pUi->textEdit, this);
}
