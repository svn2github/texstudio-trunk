/****************************************************************************
**
** Copyright (C) 2006-2009 fullmetalcoder <fullmetalcoder@hotmail.fr>
**
** This file is part of the Edyuk project <http://edyuk.org>
** 
** This file may be used under the terms of the GNU General Public License
** version 3 as published by the Free Software Foundation and appearing in the
** file GPL.txt included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#ifndef _QCODE_EDIT_H_
#define _QCODE_EDIT_H_

#include "mostQtHeaders.h"

#include "qce-config.h"

/*!
	\file qcodeedit.h
	\brief Definition of the QCodeEdit class
*/

class QMenu;
class QPanel;
class QEditor;
class QWidget;
class QString;
class QAction;
class QPanelLayout;
class QPanelWatcher;
class QDocument;

#define Q_COMMAND QList<QGenericArgument>()

class QCE_EXPORT QCodeEdit
{
	friend class QPanelWatcher;
	
	public:
		enum Position
		{
			West,
			North,
			South,
			East
		};
		
		QCodeEdit(QWidget *p = 0);
		QCodeEdit(bool actions, QWidget *p = 0, QDocument *doc=0);
		QCodeEdit(const QString& layout, QWidget *p = 0);
		QCodeEdit(const QString& layout, bool actions, QWidget *p = 0);
		virtual ~QCodeEdit();
		
		QEditor* editor() const;
		QPanelLayout* panelLayout() const;
		
		QAction* addPanel(QPanel *panel, Position pos, bool _add = false);
		QAction* addPanel(const QString& name, Position pos, bool _add = false); 
		
		bool hasPanel(const QString& type) const;
		QList<QPanel*> panels(const QString& type = QString()) const;
		
		QAction* toggleViewAction(QPanel *p) const;
		
		void sendPanelCommand(	const QString& type,
								const char *signature,
								const QList<QGenericArgument>& args = Q_COMMAND);
		void sendPanelCommand(	const QString& type,
								const char *signature,
								const QList<QVariant>& args);

		static QCodeEdit* manager(QEditor *e);
		static QEditor* managed(const QString& f);
		
	//protected:
		QCodeEdit(QEditor *e, QPanelLayout *p);
		QCodeEdit(QEditor *e, const QString& l);
		
	private:
		QPanelWatcher *m_watcher;
		QPointer<QEditor> m_editor;
		QPointer<QPanelLayout> m_layout;
		
		QMenu *m_panelsMenu;
		QList<QAction*> m_actions;
		
		static QList<QCodeEdit*> m_instances;
};

//TODO: move this to somewhere else?
void QMetaObjectInvokeMethod(QObject* o, const char *signature, const QList<QGenericArgument>& args);
void QMetaObjectInvokeMethod(QObject* o, const char *signature, const QList<QVariant>& args);
QByteArray createMethodSignature(const char* methodName, const QList<QVariant>& args);

#endif // _QCODE_EDIT_H_
