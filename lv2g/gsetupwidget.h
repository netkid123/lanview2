#ifndef CGSETUPWIDGET_H
#define CGSETUPWIDGET_H

#include <QObject>
#include <QWidget>
#include <QSound>
#include "lv2g.h"
#include "ui_gsetup.h"

/// @class cGSetupWidget
/// Program GUI setup (tab) widget
class LV2GSHARED_EXPORT cGSetupWidget : public cIntSubObj
{
    Q_OBJECT
public:
    cGSetupWidget(QMdiArea *par);
    /// Minimális jogosultsági szint az eléréséhez.
    static const enum ePrivilegeLevel rights;
protected:
    void applicate();
    QSettings &qset;
    Ui_GSetup  *pUi;
    QSound     *pSound;
protected slots:
    void applicateAndRestart();
    void applicateAndExit();
    void applicateAndClose();
    void selectAlarmFile();
    void testAlarmFile();
};

#endif // CGSETUPWIDGET_H
