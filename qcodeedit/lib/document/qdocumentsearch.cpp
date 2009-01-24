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

/*!
	\file qdocumentsearch.cpp
	\brief Implementation of QDocumentSearch
*/

#include "qdocumentsearch.h"

/*!
	\ingroup document
	@{
*/

#include "qeditor.h"
#include "qdocument.h"
#include "qdocument_p.h"
#include "qdocumentline.h"
#include "qformatscheme.h"

#include <QMessageBox>

/*!
	\class QDocumentSearch
	\brief An helper class to perform search in document
	
	QDocumentSearch offer means to perform complex search in documents.
*/

QDocumentSearch::QDocumentSearch(QEditor *e, const QString& f, Options opt, const QString& r)
 : m_option(opt), m_string(f), m_replace(r), m_editor(e)
{
	
}

QDocumentSearch::~QDocumentSearch()
{
	clearMatches();
}

/*!
	\brief Clear matches
	
	This function should be called anytime you perform a search with the HighlightAll option,
	once you're done iterating over the matches.
*/
void QDocumentSearch::clearMatches()
{
	if ( !(m_editor && m_editor->document()) )
		return;
	
	QFormatScheme *f = m_editor->document()->formatFactory();
	
	if ( !f )
		return;
	
	quint8 sid = f->id("search");
	
	foreach ( const QDocumentCursor& c, m_highlight )
	{
		QFormatRange r(c.anchorColumn(), c.columnNumber() - c.anchorColumn(), sid);
		
		c.line().removeOverlay(r);
		
		//qDebug("(%i, %i, %i)", r.offset, r.length, r.format);
	}
	
	m_highlight.clear();
}

/*!
	\return The search pattern
*/
QString QDocumentSearch::searchText() const
{
	return m_string;
}

/*!
	\brief Set the search pattern
*/
void QDocumentSearch::setSearchText(const QString& f)
{
	m_string = f;
	
	if ( m_highlight.count() )
	{
		//qDebug("cleanup.");
		
		clearMatches();
		
		if ( m_editor )
			m_editor->viewport()->update();
	}
}

/*!
	\brief Test whether a given option is enabled
*/
bool QDocumentSearch::hasOption(Option opt) const
{
	return m_option & opt;
}

/*!
	\brief Set a search option
	\param opt option to set
	\param on whether to enable the option
*/
void QDocumentSearch::setOption(Option opt, bool on)
{
	if ( on )
		m_option |= opt;
	else
		m_option &= ~opt;
}

/*!
	\return the replacement text
*/
QString QDocumentSearch::replaceText() const
{
	return m_replace;
}

/*!
	\brief Set the replacement text
*/
void QDocumentSearch::setReplaceText(const QString& r)
{
	m_replace = r;
}

/*!
	\return The current cursor position
	
	This is useful to examine matches after performing a search.
*/
QDocumentCursor QDocumentSearch::cursor() const
{
	return m_cursor;
}

/*!
	\brief Set the cursor
	
	If the related option is set, search will start from that cursor position
*/
void QDocumentSearch::setCursor(const QDocumentCursor& c)
{
	m_cursor = c;
}

/*!
	\return The scope of the search
	
	An invalid cursor indicate that the scope is the whole document, otherwise
	the scope is the selection of the returned cursor.
*/
QDocumentCursor QDocumentSearch::scope() const
{
	return m_scope;
}

/*!
	\brief Set the search scope
	
	If the given cursor has no selection (a fortiori if it is invalid) then
	the scope is the whole document.
*/
void QDocumentSearch::setScope(const QDocumentCursor& c)
{
	if ( c.hasSelection() )
		m_scope = c;
	else
		m_scope = QDocumentCursor();
}

/*!
	\brief Test whether the end of the search scope has been reached
*/
bool QDocumentSearch::end(bool backward) const
{
	bool absEnd = backward ? m_cursor.atStart() : m_cursor.atEnd();
	
	if ( m_scope.isValid() && m_scope.hasSelection() )
	{
		absEnd |= !m_scope.isWithinSelection(m_cursor);
		/*
		qDebug(
				"(%i, %i, %i) %s in {(%i, %i), (%i, %i)}",
				m_cursor.lineNumber(),
				m_cursor.anchorColumn(),
				m_cursor.columnNumber(),
				absEnd ? "is not" : "is",
				m_scope.selectionStart().lineNumber(),
				m_scope.selectionStart().columnNumber(),
				m_scope.selectionEnd().lineNumber(),
				m_scope.selectionEnd().columnNumber()
			);
		*/
	}
	
	return absEnd;
}

/*!
	\brief Perform a search
	\param backward whether to go backward or forward
	\param all if true, the whole document will be searched first, all matches recorded and available for further navigation
	
	\note Technically speaking the all parameter make search behave similarly to the HighlightAll option, except that the former
	option does not alter the formatting of the document.
*/
void QDocumentSearch::next(bool backward, bool all)
{
	if ( !hasOption(Replace) && (all || hasOption(HighlightAll)) && m_highlight.count() )
	{
		m_index = m_index + (backward ? -1 : 1);
		
		if ( (m_index < 0 || m_index >= m_highlight.count()) )
		{
			if ( hasOption(Silent) )
			{
				m_cursor = QDocumentCursor();
				return;
			}
			
			int ret = 
			QMessageBox::question(
							m_editor,
							tr("Failure"),
							tr(
								"End of matches reached.\n"
								"Restart from the begining ?"
							),
							QMessageBox::Yes
							| QMessageBox::No,
							QMessageBox::Yes
						);
			
			if ( ret == QMessageBox::Yes )
			{
				m_index = backward ? m_highlight.count() : -1;
				next(backward);
			}
		} else {
			m_cursor = m_highlight.at(m_index);
			
			if ( m_editor && !hasOption(Silent) )
				m_editor->setCursor(m_cursor);
		}
		
		return;
	}
	
	if ( m_cursor.isNull() )
	{
		if ( m_scope.isValid() && m_scope.hasSelection() )
		{
			if ( backward )
				m_cursor = m_scope.selectionEnd();
			else
				m_cursor = m_scope.selectionStart();
		} else if ( m_editor ) {
			
			m_cursor = QDocumentCursor(m_editor->document());
			
			if ( backward )
				m_cursor.movePosition(1, QDocumentCursor::End);
			
		} else {
			QMessageBox::warning(0, 0, "Unable to perform search operation");
		}
	}
	
	/*
	qDebug(
		"searching %s from line %i (column %i)",
		backward ? "backward" : "forward",
		m_cursor.lineNumber(),
		m_cursor.columnNumber()
	);
	*/
	
	QRegExp m_regexp;
	Qt::CaseSensitivity cs = hasOption(CaseSensitive)
								?
									Qt::CaseSensitive
								:
									Qt::CaseInsensitive;
	
	if ( hasOption(RegExp) )
	{
		m_regexp = QRegExp(m_string, cs, QRegExp::RegExp);
	} else if ( hasOption(WholeWords) ) {
		m_regexp = QRegExp(
						QString("\\b%1\\b").arg(QRegExp::escape(m_string)),
						cs,
						QRegExp::RegExp
					);
	} else {
		m_regexp = QRegExp(m_string, cs, QRegExp::FixedString);
	}
	
	bool found = false;
	QDocumentCursor::MoveOperation move;
	move = backward ? QDocumentCursor::PreviousBlock : QDocumentCursor::NextBlock;
	
	/*
	// regression fix : here is how we clear the selection without moving back to the anchor column...
	if ( m_cursor.hasSelection() )
	{
		//if ( backward )
		//	m_cursor.moveTo(m_cursor.selectionStart());
		//else
		//	m_cursor.moveTo(m_cursor.selectionEnd());
		m_cursor.setColumnNumber(m_cursor.columnNumber());
	}
	*/

	QDocumentSelection boundaries;
	bool bounded = m_scope.isValid() && m_scope.hasSelection();
	
	// condition only to avoid debug messages...
	if ( bounded )
		boundaries = m_scope.selection();
	
	while ( !end(backward) )
	{
		if ( backward && !m_cursor.columnNumber() )
		{
			m_cursor.movePosition(1, QDocumentCursor::PreviousCharacter);
			continue;
		}
		
		int ln = m_cursor.lineNumber();
		QDocumentLine l = m_cursor.line();
		
		int coloffset = 0;
		QString s = l.text();
		
		if ( backward )
		{
			if ( bounded && (boundaries.startLine == ln) )
			{
				s = s.mid(boundaries.start);
				coloffset = boundaries.start;
			}
		} else {
			if ( bounded && (boundaries.endLine == ln) )
				s = s.left(boundaries.end);
			
		}
		
		int column = backward
				?
					m_regexp.lastIndexIn(s, m_cursor.columnNumber() - 1)
				:
					m_regexp.indexIn(s, m_cursor.columnNumber())
				;
		
		/*
		qDebug("searching %s in %s => %i",
				qPrintable(m_regexp.pattern()),
				qPrintable(s),
				column);
		*/
		
		if ( column != -1 && (backward ? (column + m_regexp.matchedLength()) <= m_cursor.columnNumber() : column >= m_cursor.columnNumber()) )
		{
			column += coloffset;
			
			if ( backward )
			{
				m_cursor.setColumnNumber(column + m_regexp.matchedLength());
				m_cursor.setColumnNumber(column, QDocumentCursor::KeepAnchor);
				
				/*
				m_cursor.movePosition(m_regexp.matchedLength(),
									QDocumentCursor::PreviousCharacter,
									QDocumentCursor::KeepAnchor);
				*/
			} else {
				m_cursor.setColumnNumber(column);
				m_cursor.setColumnNumber(column + m_regexp.matchedLength(), QDocumentCursor::KeepAnchor);
				
				/*
				m_cursor.movePosition(m_regexp.matchedLength(),
									QDocumentCursor::NextCharacter,
									QDocumentCursor::KeepAnchor);
				*/
			}
			
			if ( m_editor && !hasOption(Silent) && !hasOption(HighlightAll) )
				m_editor->setCursor(m_cursor);
			
			if ( hasOption(Replace) )
			{
				bool rep = true;
				
				if ( hasOption(Prompt) )
				{
					int ret = QMessageBox::question(m_editor, tr("Replacement prompt"),
										tr("Shall it be replaced?"),
										QMessageBox::Yes
										| QMessageBox::No
										| QMessageBox::Cancel,
										QMessageBox::Yes);
					
					if ( ret == QMessageBox::Yes )
					{
						rep = true;
					} else if ( ret == QMessageBox::No ) {
						rep = false;
					} else if ( QMessageBox::Cancel ) {
						//m_cursor.setColumnNumber(m_cursor.columnNumber());
						return;
					}
				}
				
				//
				if ( rep )
				{
					//qDebug("rep");
					QString replacement = m_replace;
					
					for ( int i = m_regexp.numCaptures(); i >= 0; --i )
						replacement.replace(QString("\\") + QString::number(i),
											m_regexp.cap(i));
					/*
					for ( int i = 0; (i < m_regexp.numCaptures()) && (i < 10); ++i )
						replacement.replace(QString("\\") + QString::number(i),
											m_regexp.cap(i));
					*/
					
					m_cursor.beginEditBlock();
					m_cursor.removeSelectedText();
					m_cursor.insertText(replacement);
					m_cursor.endEditBlock();
					
					if ( backward )
						m_cursor.movePosition(replacement.length(), QDocumentCursor::PreviousCharacter);
				} else {
					//qDebug("no rep");
				}
			} else if ( all || hasOption(HighlightAll) ) {
				
				if ( hasOption(HighlightAll) )
				{
					QFormatRange r(
								m_cursor.anchorColumn(),
								m_cursor.columnNumber() - m_cursor.anchorColumn(),
								m_editor->document()->formatFactory()->id("search")
							);
					
					//qDebug("(%i, %i, %i)", r.offset, r.length, r.format);
					m_cursor.line().addOverlay(r);
				}
				
				m_highlight << m_cursor;
				m_highlight.last().setAutoUpdated(true);
				
			} else {
				// regression fix : here is how we clear the selection without moving back to the anchor column...
				//m_cursor.clearSelection();
				//m_cursor.setColumnNumber(m_cursor.columnNumber());
			}
			
			found = true;
			
			if ( !(all || hasOption(HighlightAll)) )
				break;
			
			//m_cursor.setColumnNumber(m_cursor.columnNumber());
		} else {
			m_cursor.movePosition(1, move);
		}
	}
	
	if ( !hasOption(Replace) && hasOption(HighlightAll) && m_highlight.count() )
	{
		if ( m_editor && m_editor->document() )
			m_editor->document()->markViewDirty();
		
		m_index = backward ? m_highlight.count() : -1;
		return next(backward);
	}
	
	if ( !found )
	{
		m_cursor = QDocumentCursor();
		
		if ( hasOption(Silent) )
			return;
		
		int ret = 
		QMessageBox::question(
						m_editor,
						tr("Failure"),
						tr(
							"End of scope reached with no match.\n"
							"Restart from the begining ?"
						),
						QMessageBox::Yes
						| QMessageBox::No,
						QMessageBox::Yes
					);
		
		if ( ret == QMessageBox::Yes )
			next(backward);
	}
}

/*! @} */

