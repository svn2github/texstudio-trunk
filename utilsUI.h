#ifndef UTILSUI_H
#define UTILSUI_H

#include "mostQtHeaders.h"

bool txsConfirm(const QString &message);
bool txsConfirmWarning(const QString &message);
QMessageBox::StandardButton txsConfirmWarning(const QString &message, QMessageBox::StandardButtons buttons);
void txsInformation(const QString &message);
void txsWarning(const QString& message);
void txsWarning(const QString &message, bool &noWarnAgain);
void txsCritical(const QString& message);

//setup toolbutton as substitute for const combobox
QToolButton* createComboToolButton(QWidget *parent,const QStringList& list, int height,const QObject * receiver, const char * member,QString defaultElem="",QToolButton *combo=0);
//find the tool button which contains a given action
QToolButton* comboToolButtonFromAction(QAction* action);

QToolButton* createToolButtonForAction(QAction* action);

#endif // UTILSUI_H
