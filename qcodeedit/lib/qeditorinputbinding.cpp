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

#include "qeditorinputbinding.h"

#include "qeditor.h"
#include "qdocument.h"
#include "qdocumentcursor.h"

#include <QKeyEvent>

/////////////////////////////////////////////////////////////////////////////

QEditorInputBinding::MotionCommand::MotionCommand(QDocumentCursor::MoveOperation op, QDocumentCursor::MoveMode m, int n)
 : count(n), mode(m), operation(op)
{
	
}

void QEditorInputBinding::MotionCommand::exec(QEditor *e)
{
	QDocumentCursor c = e->cursor();
	c.movePosition(count, operation, mode);
	e->setCursor(c);
}

QEditorInputBinding::EditCommand::EditCommand(Operation op)
 : operation(op)
{
	
}

void QEditorInputBinding::EditCommand::exec(QEditor *e)
{
	QDocumentCursor c = e->cursor();
	
	switch ( operation )
	{
		case ClearSelection :
			c.clearSelection();
			break;
			
		case SelectWord :
			c.select(QDocumentCursor::WordUnderCursor);
			break;
			
		case SelectLine :
			c.select(QDocumentCursor::LineUnderCursor);
			break;
			
		case SelectDocument :
			c.movePosition(1, QDocumentCursor::Start, QDocumentCursor::MoveAnchor);
			c.movePosition(1, QDocumentCursor::End, QDocumentCursor::KeepAnchor);
			break;
			
		case DeleteChar :
			c.deleteChar();
			break;
			
		case DeletePreviousChar :
			c.deletePreviousChar();
			break;
			
		case DeleteLine :
			c.eraseLine();
			break;
			
		case DeleteSelection :
			c.removeSelectedText();
			break;
			
		case InsertLine :
			c.insertLine();
			break;
			
		case InsertClipBoard :
			e->paste();
			return;
			
		default:
			
			break;
	}
	
	e->setCursor(c);
}

QEditorInputBinding::WriteCommand::WriteCommand(const QString& t)
 : text(t)
{
	
}

void QEditorInputBinding::WriteCommand::exec(QEditor *e)
{
	e->write(text);
}

void QEditorInputBinding::GroupCommand::addCommand(Command *c)
{
	commands << c;
}

void QEditorInputBinding::GroupCommand::exec(QEditor *e)
{
	foreach ( Command *c, commands )
		c->exec(e);
}

/////////////////////////////////////////////////////////////////////////////

QEditorInputBinding::QEditorInputBinding()
{
}

QEditorInputBinding::~QEditorInputBinding()
{
	qDeleteAll(m_actions);
}

void QEditorInputBinding::setMapping(const QKeySequence& ks, Command *cmd)
{
	for ( int i = 0; i < m_keys.count(); ++i )
	{
		if ( m_keys.at(i) == ks )
		{
			delete m_actions[i];
			m_actions[i] = cmd;
			return;
		}
	}
	
	m_index << 0;
	m_keys << ks;
	m_actions << cmd;
}

bool QEditorInputBinding::isExclusive() const
{
	return false;
}

bool QEditorInputBinding::keyPressEvent(QKeyEvent *event, QEditor *editor)
{
	bool filter = false;
	
	for ( int i = 0; i < m_keys.count(); ++i )
	{
		int idx = m_index.at(i);
		const QKeySequence& ks = m_keys.at(i);
		
		if ( idx < ks.count() )
		{
			if ( ks[idx] == event->key() )
			{
				++idx;
				
				if ( idx == ks.count() )
				{
					qDebug("match");
					
					// key sequence matched
					m_actions.at(i)->exec(editor);
					
					// cancel other in progress matches ?
					m_index.fill(0);
					
					//return true;
				} else {
					//qDebug("step");
					m_index[i] = idx;
				}
				
				// filter out the event
				filter = true;
			} else {
				m_index[i] = 0;
			}
		} else {
			m_index[i] = 0;
		}
	}
	
	return filter;
}

void QEditorInputBinding::postKeyPressEvent(QKeyEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
}

bool QEditorInputBinding::inputMethodEvent(QInputMethodEvent* event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
	
	return false;
}

void QEditorInputBinding::postInputMethodEvent(QInputMethodEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
}

bool QEditorInputBinding::mouseMoveEvent(QMouseEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
	
	return false;
}

void QEditorInputBinding::postMouseMoveEvent(QMouseEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
}

bool QEditorInputBinding::mousePressEvent(QMouseEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
	
	return false;
}

void QEditorInputBinding::postMousePressEvent(QMouseEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
}

bool QEditorInputBinding::mouseReleaseEvent(QMouseEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
	
	return false;
}

void QEditorInputBinding::postMouseReleaseEvent(QMouseEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
}

bool QEditorInputBinding::mouseDoubleClickEvent(QMouseEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
	
	return false;
}

void QEditorInputBinding::postMouseDoubleClickEvent(QMouseEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
}

bool QEditorInputBinding::contextMenuEvent(QContextMenuEvent *event, QEditor *editor)
{
	Q_UNUSED(event)
	Q_UNUSED(editor)
	
	return false;
}
