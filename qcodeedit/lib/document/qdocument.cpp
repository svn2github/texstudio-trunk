/****************************************************************************
**
** Copyright (C) 2006-2008 fullmetalcoder <fullmetalcoder@hotmail.fr>
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

#include "qdocument.h"
#include "smallUsefulFunctions.h"

/*
	Document model :

	Goals :
		* KISS : Keep It Simple Stupid
		* FAST : ...
		* LIGHTWEIGHT : reduce memory usage
		* FLEXIBLE : allow punctual bidi through QTextLayout

	Implementation :
		QDocument
		QDocumentPrivate

		QDocumentLine and QDocumentLineHandle => equivalent of QTextBlock
		QDocumentCursor and QDocumentCursorHandle => equivalent of QTextCursor

	Note :
		The KISS principle has been kinda mistreated in the private API due to
		the addition of some complex features which where not planned to be
		supported when defining the goals (e.g line wrapping, variable width
		fonts, ...). Such a compromission is affordable but should be avoided
		whenever possible in the future. And of course the public API should
		never suffer from such a thing.
*/

/*!
	\ingroup document
	@{
*/

/*!
	\class QDocument

	\brief A class storing a document

	QCE uses an architecture very similar to that of QTextEdit/QTextDocument
	which closely ressembles model/view. The document holds all the textual
	and formatting data. It offers some (mostly indirect) ways of modifying
	its content and is usable without any GUI.

	In QCE, a document is merely a list of QDocumentLine objects on which some
	extra formatting can be applied and which can be wrapped/hidden in various
	ways.

	The document model has been designed with three goals in mind :
	<ul>
	<li>performance
	<li>flexibility
	<li>low memory usage
	</ul>

	QDocument supports Bidi by using QTextLayout on lines that require it and
	prefers custom rendering in other cases to achieve the above goals.

	All the actual text editing is done through QDocumentCursor objects.

	\see QDocumentLine
	\see QDocumentCursor
*/

#include "qdocument_p.h"
#include "qdocumentcommand.h"

#include "qformat.h"
#include "qformatscheme.h"
#include "qlanguagedefinition.h"
#include "qlinemarksinfocenter.h"

#include <QPen>
#include <QTime>
#include <QRect>
#include <QLine>
#include <QPainter>
#include <QPrinter>
#include <QTextStream>
#include <QTextLayout>
#include <QApplication>
#include <QVarLengthArray>
#include <QMessageBox>

struct RenderRange
{
	int position;
	int length;
	int format;
	int wrap;
};

static QList<GuessEncodingCallback> guessEncodingCallbacks;

static int m_spaceSignOffset = 2;

static QPoint m_spaceSign[] = {
	QPoint(2, -1),
	QPoint(2, 0),
	QPoint(3, 0)
};

inline static bool isWord(QChar c)
{ return c.isLetterOrNumber(); } // see qnfa.cpp isWord  || (c == QLatin1Char('_')); }, _ is no word character in LaTeX

inline static bool isDelimiter(QChar c)
{
	QString delimiters="(){}$+-/*,;.";
	return delimiters.contains(c);
}

void QDocument::setWorkAround(QDocument::WorkAroundFlag workAround, bool newValue){
	QDocumentPrivate::setWorkAround(workAround, newValue);
}

bool QDocument::hasWorkAround(QDocument::WorkAroundFlag workAround){
	return QDocumentPrivate::hasWorkAround(workAround);
}

bool QDocument::getFixedPitch() const{
	return m_impl && m_impl->getFixedPitch();
}

int QDocument::screenColumn(const QChar *d, int l, int tabStop, int column)
{
	if ( tabStop == 1 )
		return l;

	int idx = 0;

	while ( idx < l )
	{
		const QChar& c = d[idx];

		if ( c == QLatin1Char('\t') )
		{
			int taboffset = tabStop - (column % tabStop);
			column += taboffset;
		} else {
			++column;
		}

		++idx;
	}

	//qDebug("%s : %i", qPrintable(QString(d, l)), column);

	return column;
}

QString QDocument::screenable(const QChar *d, int l, int tabStop, int column)
{
	if ( tabStop == 1 )
		return QString(d, l);

	QString fragment;
	int idx = 0;

	while ( idx < l )
	{
		QChar c = d[idx];

		if ( c == QLatin1Char('\t') )
		{
			int taboffset = tabStop - (column % tabStop);

			fragment += QString(taboffset, QLatin1Char(' '));
			column += taboffset;
		} else {
			fragment += c;
			++column;
		}

		++idx;
	}

	return fragment;
}

struct InitStruct
{
	InitStruct()
	{
		qRegisterMetaType<QDocumentIterator>("QDocumentIterator");
		qRegisterMetaType<QDocumentConstIterator>("QDocumentConstIterator");
	}
};

static InitStruct init_inst;

/*!
	\brief ctor
*/
QDocument::QDocument(QObject *p)
 : QObject(p), m_impl(new QDocumentPrivate(this))
{
	if ( !QDocumentPrivate::m_font )
	{
		// must not happen if config dialog plugged in...
		setFont(QFont("Monospace", 10));
	}


	setText(QString());
	setLineEnding(QDocument::Conservative);

	connect(&(m_impl->m_commands)	, SIGNAL( cleanChanged(bool) ),
			this					, SIGNAL( cleanChanged(bool) ) );

	connect(&(m_impl->m_commands)	, SIGNAL( canUndoChanged(bool) ),
			this					, SIGNAL( undoAvailable(bool) ) );

	connect(&(m_impl->m_commands)	, SIGNAL( canRedoChanged(bool) ),
			this					, SIGNAL( redoAvailable(bool) ) );

	connect(this							,
			SIGNAL( lineDeleted(QDocumentLineHandle*) ),
			QLineMarksInfoCenter::instance(),
			SLOT  ( lineDeleted(QDocumentLineHandle*) ) );

}

/*!
	\brief dtor
*/
QDocument::~QDocument()
{
	delete m_impl;
}

/*!
	\brief Clear the content of the document
*/
void QDocument::clear()
{
	setText(QString());
}

/*!
	\return whether there commands to undo on the command stack
*/
bool QDocument::canUndo() const
{
	return m_impl ? m_impl->m_commands.canUndo() : false;
}

/*!
	\return whether there are commands to redo on the command stack
*/
bool QDocument::canRedo() const
{
	return m_impl ? m_impl->m_commands.canRedo() : false;
}

/*!
	\brief Undo the last editing operation
*/
void QDocument::undo()
{
	if ( m_impl )
	{
		m_impl->m_commands.undo();
		m_impl->m_lastModified = QDateTime::currentDateTime();
}
}

/*!
	\brief Redo the last undone editing operation
*/
void QDocument::redo()
{
	if ( m_impl )
	{
		m_impl->m_commands.redo();
		m_impl->m_lastModified = QDateTime::currentDateTime();
	}

}

void QDocument::clearUndo()
{
	if ( m_impl )
	{
		m_impl->m_commands.clear();
	}
}


/*!
	\return The content of the document
	\param mode extra processing to perform on text
*/
QString QDocument::text(int mode) const
{
	QString s;

	if ( !m_impl || m_impl->m_lines.isEmpty() )
		return s;

	int line = 0;
	int prevIndent = 0, curIndent = 0, nextIndent = m_impl->m_lines.at(0)->nextNonSpaceChar(0);

	if ( nextIndent < 0 )
		nextIndent = 0;

	foreach ( const QDocumentLineHandle *l, m_impl->m_lines )
	{
		prevIndent = curIndent;
		curIndent = nextIndent;
		bool notLastLine = ++line < m_impl->m_lines.count();
		nextIndent = notLastLine ? m_impl->m_lines.at(line)->nextNonSpaceChar(0) : 0;

		if ( nextIndent < 0 )
			nextIndent = 0;

		QString buf = l->text();
		int avgIndent = qMax(prevIndent, nextIndent);

		if ( (mode & RestoreTrailingIndent) && buf.isEmpty() && avgIndent )
		{
			buf = QString(avgIndent, '\t');
		} else if ( mode & RemoveTrailingWS ) {

			int len = 0, idx = buf.length();

			while ( --idx >= 0 )
			{
				if ( !buf.at(idx).isSpace() )
					break;

				++len;
			}

			++idx;

			if ( len && (idx || !(mode & PreserveIndent)) )
				buf.remove(idx, len);
		}

		if (notLastLine)
			s += buf + m_impl->m_lineEndingString;
		else if (!buf.isEmpty())
			s += buf; //last line doesn't ends with \n (it must be possible to create a single line string)
	}

	//s.chop(m_impl->m_lineEndingString.count());
	return s;
}

/*!
	\return The content of the document
	\param removeTrailing whether to remove trailing whitespaces
	\param preserveIndent whether to keep trailing whitespaces when they are indent
*/
QString QDocument::text(bool removeTrailing, bool preserveIndent) const
{
	int mode = 0;

	if ( removeTrailing )
		mode |= RemoveTrailingWS;

	if ( preserveIndent )
		mode |= PreserveIndent;

	return text(mode);
}

QStringList QDocument::textLines() const{
	QStringList res;
	if ( !m_impl || m_impl->m_lines.isEmpty() )
		return res;
	foreach ( const QDocumentLineHandle *l, m_impl->m_lines )
		res << l->text();
	return res;
}

/*!
	\brief Set the content of the document
*/
void QDocument::setText(const QString& s)
{
	if ( !m_impl )
		return;

	int last = 0, idx = 0;

	m_impl->m_deleting = true;

	foreach ( QDocumentLineHandle *h, m_impl->m_lines )
		emit lineDeleted(h);
	//qDeleteAll(m_impl->m_lines);
	foreach ( QDocumentLineHandle *h, m_impl->m_lines )
	{
//		emit lineRemoved(h);
//		emit lineDeleted(h);
		h->m_doc = 0;
		h->deref();
	}

	m_impl->discardAutoUpdatedCursors();

	m_impl->m_lines.clear();
	m_impl->m_marks.clear();
	m_impl->m_status.clear();
	m_impl->m_hidden.clear();
	m_impl->m_wrapped.clear();
	m_impl->m_matches.clear();
	m_impl->m_largest.clear();
	m_impl->m_commands.clear();

	m_impl->m_deleting = false;

	m_impl->_nix = 0;
	m_impl->_dos = 0;
	m_impl->_mac = 0;

	while ( idx < s.length() )
	{
		if ( s.at(idx) == '\r') {
			m_impl->m_lines << new QDocumentLineHandle(
						s.mid(last, idx - last),
						this
					);
			++idx;
			if (idx < s.length() && s.at(idx) == '\n') {
				++(m_impl->_dos);
				++idx;
			} else ++(m_impl->_mac);
			last = idx;
		} else if ( s.at(idx) == '\n') {
			++(m_impl->_nix);

			m_impl->m_lines << new QDocumentLineHandle(
						s.mid(last, idx - last),
						this
					);
			last = ++idx;
		} else {
			++idx;
		}
	}

	if ( idx != last )
	{
		m_impl->m_lines << new QDocumentLineHandle(
								s.mid(last, s.length() - last),
								this
							);

	} else {
		Q_ASSERT(s.isEmpty() || s.endsWith("\n") || s.endsWith("\r"));
		m_impl->m_lines << new QDocumentLineHandle(this); //last character was \n, or empty string
	}
//
//	if ( (idx > 0) && ((idx - 1) < s.length()) && ((s.at(idx - 1) == '\n') || (s.at(idx - 1) == '\r')) )
//		m_impl->m_lines << new QDocumentLineHandle(this);
//

	//qDebug("[one go] dos : %i; nix : %i", m_impl->_dos, m_impl->_nix);

	m_impl->m_lastModified = QDateTime::currentDateTime();
	
	if ( lineEnding() == Conservative )
		setLineEnding(Conservative);

	m_impl->setWidth();
	m_impl->setHeight();

	emit lineCountChanged(lineCount());
	
	m_impl->emitContentsChange(0, m_impl->m_lines.count());
}

QTextCodec* guessEncoding(const QByteArray& data){
	const char* str = data.data();
	int size = data.size();
	QTextCodec* guess = 0;
	int sure = 0;
	if (size>0) {
		char prev=str[0];
		int good=0;int bad=0;
		int utf16le=0, utf16be = 0;
		for (int i=1;i<size;i++) {
			char cur = str[i];
			if ((cur & 0xC0) == 0x80) {
				if ((prev & 0xC0) == 0xC0) good++;
				else if ((prev & 0x80) == 0x00) bad++;
			} else {
				if ((prev & 0xC0) == 0xC0) bad++;
				//if (cur==0) { if (i & 1 == 0) utf16be++; else utf16le++;}
				if (prev==0) {
					if ((i & 1) == 1) utf16be++;
					else utf16le++;
				}
			}
			prev=cur;
		}
		sure = 1;
		// less than 0.1% of the characters can be wrong for utf-16 if at least 1% are valid (for English text)
		if (utf16le > utf16be) {
			if (utf16be <= size / 1000 && utf16le >= size / 100 && utf16le >= 2) guess = QTextCodec::codecForName("UTF-16LE");
		} else {
			if (utf16le <= size / 1000 && utf16be >= size / 100 && utf16be >= 2) guess = QTextCodec::codecForName("UTF-16BE");
		}
		if (!guess){
			if (good > 10*bad) guess = QTextCodec::codecForName("UTF-8");
			else {
				guess = QTextCodec::codecForName("ISO-8859-1");
				if (bad <= size / 1000) sure = 0;
			}
		}
	}
	if (!guessEncodingCallbacks.empty())
		foreach (const GuessEncodingCallback& callback, guessEncodingCallbacks)
			callback(data, guess, sure);

	if (guess!=0) return guess;
	else return QTextCodec::codecForName("UTF-8"); //default
}


void QDocument::load(const QString& file, QTextCodec* codec){
	QFile f(file);

	// gotta handle line endings ourselves if we want to detect current line ending style...
	//if ( !f.open(QFile::Text | QFile::ReadOnly) )
	if ( !f.open(QFile::ReadOnly) )
	{
		setText(QString());
		return;
	}

	const int size = f.size();
	//const int size = m_lastFileState.size = f.size();

	if ( size < 500000 )
	{
		// instant load for files smaller than 500kb
		QByteArray d = f.readAll();
		if (codec == 0)
			codec=guessEncoding(d);

		setText(codec->toUnicode(d));
	} else {
		// load by chunks of 100kb otherwise to avoid huge peaks of memory usage
		// and driving mad the disk drivers

		int count = 0;
		QByteArray ba;

		startChunkLoading();
		//m_lastFileState.checksum = 0;

		ba = f.read(100000);
		if (codec == 0)
			codec=guessEncoding(ba);

		QTextDecoder *dec = codec->makeDecoder();
		do
		{
			count += ba.count();
			//m_lastFileState.checksum ^= qChecksum(ba.constData(), ba.size());
			addChunk(dec->toUnicode(ba));
			ba = f.read(100000);
		} while ( (count < size) && ba.count() );
		delete dec;
		stopChunkLoading();
	}
	setCodec(codec);
	setLastModified(QFileInfo(file).lastModified());
}

/*!
	\brief Start a chunk loading

	It is possible to load document contents in one piece
	or by chunks. To achieve the later you have to proceed as follows :

	\code
	QDocument doc;
	doc.startChunkLoading();

	// fetch data and add it using doc.addChunk();

	doc.stopChunkLoading();
	\endcode

	\see addChunk(const QString&)
	\see stopChunkLoading()
*/
void QDocument::startChunkLoading()
{
	if ( !m_impl )
		return;

	m_impl->m_deleting = true;

	//qDeleteAll(m_impl->m_lines);
	foreach ( QDocumentLineHandle *h, m_impl->m_lines )
		emit lineDeleted(h);
	foreach ( QDocumentLineHandle *h, m_impl->m_lines )
	{
//		emit lineRemoved(h);
//		emit lineDeleted(h);
		h->m_doc = 0;
		h->deref();
 	}

	m_impl->discardAutoUpdatedCursors();

	m_impl->m_lines.clear();
	m_impl->m_marks.clear();
	m_impl->m_status.clear();
	m_impl->m_hidden.clear();
	m_impl->m_wrapped.clear();
	m_impl->m_matches.clear();
	m_impl->m_largest.clear();
	m_impl->m_commands.clear();

	m_impl->m_deleting = false;

	m_impl->_nix = 0;
	m_impl->_dos = 0;
	m_impl->_mac = 0;

	m_leftOver.clear();
}

/*!
	\brief Stop chunk loading

	\see startChunkLoading()
*/
void QDocument::stopChunkLoading()
{
	if ( m_leftOver.count() )
	{
		m_impl->m_lines << new QDocumentLineHandle(
								m_leftOver,
								this
							);

		m_leftOver.clear();

	} else {
		m_impl->m_lines << new QDocumentLineHandle(this);
	}

	//qDebug("[chunk] dos : %i; nix : %i", m_impl->_dos, m_impl->_nix);

	m_impl->m_lastModified = QDateTime::currentDateTime();
	
	if ( lineEnding() == Conservative )
		setLineEnding(Conservative);

	m_impl->setWidth();
	m_impl->setHeight();

	emit lineCountChanged(lineCount());
	
	emit m_impl->emitContentsChange(0, m_impl->m_lines.count());
}

/*!
	\return The format scheme used by the document
*/
QFormatScheme* QDocument::formatScheme() const
{
	return m_impl ? m_impl->m_formatScheme : 0;
}

/*!
	\brief Set the format scheme used by the document
*/
void QDocument::setFormatScheme(QFormatScheme *f)
{
	if ( m_impl )
		m_impl->setFormatScheme(f);
}

QColor QDocument::getBackground() const{
	if (m_impl && m_impl->m_formatScheme) {
		if (m_impl->m_formatScheme->format("background").background.isValid())
			return m_impl->m_formatScheme->format("background").background; //independent of "normal" format (otherwise it can't be merged with the current line)
		else if (m_impl->m_formatScheme->format("normal").background.isValid())
			return m_impl->m_formatScheme->format("normal").background;
	}
	return QColor();
}

/*!
	\return the language definition set to the document
*/
QLanguageDefinition* QDocument::languageDefinition() const
{
	return m_impl ? m_impl->m_language : 0;
}

/*!
	\brief Set the language definition
*/
void QDocument::setLanguageDefinition(QLanguageDefinition *f)
{
	if ( m_impl )
		m_impl->m_language = f;
}

/*!
	\brief Update the formatting of the whole document
	This function is only useful when changing the language definition
	of a non-empty document. Make sure you do not call it more often
	than needed.
*/
void QDocument::highlight()
{
	if ( m_impl )
		m_impl->emitContentsChange(0, lines());
}

/*!
	\brief Add a chunk of text to the document
*/
void QDocument::addChunk(const QString& txt)
{
	if ( !m_impl || txt.isEmpty() )
		return;

	m_leftOver += txt;
	int idx = 0, last = 0;

	while ( idx < m_leftOver.length() )
	{
		if ( m_leftOver.at(idx) == '\r') {
            m_impl->m_lines << new QDocumentLineHandle(
                                    m_leftOver.mid(last, idx - last),
                                    this
                                );
            ++idx;
		    if (idx < m_leftOver.length() && m_leftOver.at(idx) == '\n') {
                ++(m_impl->_dos);
                ++idx;
		    } else ++(m_impl->_mac);
			last = idx;
		} else if ( m_leftOver.at(idx) == '\n') {
            ++(m_impl->_nix);

            m_impl->m_lines << new QDocumentLineHandle(
										m_leftOver.mid(last, idx - last),
										this
                                );
			last = ++idx;
        } else {
			++idx;
		}
	}

	if ( idx != last )
		m_leftOver = m_leftOver.mid(last);
	else
		m_leftOver.clear();

}

/*!
	\brief Print the content of the document
	\param pr printer to use

	\note the printer MUST be initialized (probably using a printing dialog)
*/
void QDocument::print(QPrinter *pr)
{
	QRect fit = pr->pageRect();

	if ( pr->printRange() == QPrinter::Selection )
	{
		qWarning("printing selection not implemented yet");
		return;
	}

	if ( fit.width() < width() )
	{
		// TODO: got to temporarily wrap text to fit page size

		qWarning("temporary wrapping not implementated yet");
	}

	const int lineCount = lines();
	const int linesPerPage = fit.height() / m_impl->m_lineSpacing;
	int pageCount = lineCount / linesPerPage;

	if ( lineCount % linesPerPage )
		++pageCount;

	//qDebug("%i lines per page -> %i pages", linesPerPage, pageCount);

	const int pageWidth = fit.width();
	const int pageHeight = linesPerPage * m_impl->m_lineSpacing;

	int firstPage = pr->fromPage(), lastPage = pr->toPage();

	if ( !lastPage )
		lastPage = pageCount - 1;

	QPainter p(pr);
	PaintContext cxt;
	cxt.xoffset = 0;
	cxt.yoffset = firstPage * pageHeight;
	cxt.width = pageWidth;
	cxt.height = pageHeight - m_impl->m_lineSpacing;
	cxt.palette = QApplication::palette();
	cxt.fillCursorRect = false;
	cxt.blinkingCursor = false;

	for ( int i = firstPage; i <= lastPage; ++i )
	{
		draw(&p, cxt);

		cxt.yoffset += pageHeight;

		if ( i != lastPage )
		{
			pr->newPage();
			p.translate(0, -pageHeight);
		}
	}
}

/*!
	\return The line ending policy of the document

	The line ending policy determines how line endings
	are used when saving the document (which includes
	fetching the document's text()).

	It can either be conservative (auto detect upon loading
	and do not modify when saving later on) or enforce
	a particular line ending (either local line ending
	or a specific value).
*/
QDocument::LineEnding QDocument::lineEnding() const
{
	return m_impl ? m_impl->m_lineEnding : Local;
}


/*!
	\return the lin endings detected upon loading

	This should only ever take the the Window of Linux value
	if a document has been loaded. If no content has been
	loaded it will fall back to Local.
*/
QDocument::LineEnding QDocument::originalLineEnding() const
{
    if (!m_impl) return Local;
    if (m_impl->_dos > m_impl->_nix  && m_impl->_dos > m_impl->_mac) return Windows;
    if (m_impl->_nix > m_impl->_dos  && m_impl->_nix > m_impl->_mac) return Unix;
    if (m_impl->_mac > m_impl->_dos  && m_impl->_mac > m_impl->_nix) return Mac;
	return Local;
}

QString QDocument::lineEndingString() const{
	return m_impl?m_impl->m_lineEndingString:"\n";
}

/*!
	\brief Set the line ending policy of the document
*/
void QDocument::setLineEnding(LineEnding le)
{
	if ( !m_impl )
		return;

	m_impl->m_lineEnding = le;
	QString& les = m_impl->m_lineEndingString;

	switch ( le )
	{
		case Conservative :

			switch (originalLineEnding()) {
			    case Windows: les = "\r\n"; break;
			    case Mac: les = "\r"; break;
			    default: les = "\n";
			}

			break;

		case Local :
			#ifdef Q_OS_WIN
			les = "\r\n";
			//#elif defined(Q_OS_MAC)
			//les = "\r";
			#else
			les = "\n";
			#endif
			break;

		case Unix :
			les = "\n";
			break;

		case Mac :
			les = "\r";
			break;

		case Windows :
			les = "\r\n";
			break;

		default :
			les = "\n";
			break;
	}

	emit lineEndingChanged(le);
}

QTextCodec* QDocument::codec() const{
	return (m_impl && m_impl->m_codec)?m_impl->m_codec:QDocumentPrivate::m_defaultCodec;
}
void QDocument::setCodec(QTextCodec* codec){
	if (!m_impl) return;
	m_impl->m_codec=codec;
}


/*!
	\return the font used by ALL documents to render their content

	This font is also used to do calculations (such as converting
	(line, column) cursor position to (x, y) document position (or
	the inverse transformation))

	\note this limitation is historic and may disappear
	in future versions
*/
QFont QDocument::font()
{
	return *(QDocumentPrivate::m_font);
}

/*!
	\brief Set the font of ALL documents

	\note this limitation is historic and may disappear
	in future versions
*/
void QDocument::setFont(const QFont& f)
{
	QDocumentPrivate::setFont(f);
	//emit contentsChanged();
}

/*!
	\return The font metrics used by ALL documents

	\note this limitation is historic and may disappear
	in future versions
*/
/*const QFontMetrics QDocument::fontMetrics()
{
	return QFontMetrics(*QDocumentPrivate::m_font);
}*/

/*!
	\return The line spacing used by ALL documents, it is identically to
	the vertical distance of the top pixels of two non-wrapped, successive lines

	\note this limitation is historic and may disappear
	in future versions
*/
int QDocument::getLineSpacing()
{
	return QDocumentPrivate::m_lineSpacing;
}

/*!
	\return The default tab stop common to ALL documents

	\note this limitation is historic and may disappear
	in future versions
*/
int QDocument::tabStop()
{
	return QDocumentPrivate::m_defaultTabStop;
}

/*!
	\brief Set the default tab stop common to all documents

	\note this limitation is historic and may disappear
	in future versions
*/
void QDocument::setTabStop(int n)
{
	QDocumentPrivate::m_defaultTabStop = n;

	foreach ( QDocumentPrivate *d, QDocumentPrivate::m_documents )
	{
		d->m_tabStop = n;
		d->emitFormatsChanged();
	}
}

/*!
	\return the whitesapce display mode
*/
QDocument::WhiteSpaceMode QDocument::showSpaces()
{
	return QDocumentPrivate::m_showSpaces;
}

/*!
	\brief Set the whitespace display mode
*/
void QDocument::setShowSpaces(WhiteSpaceMode m)
{
	QDocumentPrivate::m_showSpaces = m;

	foreach ( QDocumentPrivate *d, QDocumentPrivate::m_documents )
		d->emitFormatsChanged();

}

/*!
	\brief Set the edit cursor

	Archaic concept designed for use in QEditor
	(is it still used???)
*/
QDocumentCursor* QDocument::editCursor() const
{
	return m_impl ? m_impl->m_editCursor : 0;
}

/*!
	\brief Set the edit cursor

	\see editCursor()
*/
void QDocument::setEditCursor(QDocumentCursor *c)
{
	if ( m_impl )
		m_impl->m_editCursor = c;

}

/*!
	\return the width of the document, in pixels

	The width of the document is that of longest text line.
*/
int QDocument::width() const
{
	return m_impl ? m_impl->m_width : 0;
}

/*!
	\return the height of the document, in pixels
*/
int QDocument::height() const
{
	return m_impl ? m_impl->m_height : 0;
}

/*!
	\return The width constraint imposed on that document

	Setting a width constraint on a document achieves line
	wrapping.
*/
int QDocument::widthConstraint() const
{
	return (m_impl && m_impl->m_constrained) ? m_impl->m_width : 0;
}

/*!
	\return the number of text lines in the document

	The number of visual lines may differ from that of text
	lines as soon as line wrapping and/or folding are enabled.
	
	\deprecated Use lineCount() instead
*/
int QDocument::lines() const
{
	return m_impl ? m_impl->m_lines.count() : 0;
}

/*!
	\return the number of text lines in the document
	
	The number of visual lines may differ from that of text
	lines as soon as line wrapping and/or folding are enabled.
*/
int QDocument::lineCount() const
{
	return m_impl ? m_impl->m_lines.count() : 0;
}

/*!
	\return the number of visual lines in the document
	\deprecated Use visualLineCount() instead
*/
int QDocument::visualLines() const
{
	return m_impl ? m_impl->visualLine(m_impl->m_lines.count() - 1) : 0;
}

/*!
	\return the number of visual lines in the document
*/
int QDocument::visualLineCount() const
{
	return m_impl ? m_impl->visualLine(m_impl->m_lines.count() - 1) : 0;
}

/*!
	\brief Convert a text (logical) line number int a visual line number

	\note this is not a 1:1 mapping as logical lines can span over several visual lines
*/
int QDocument::visualLineNumber(int textLineNumber) const
{
	return m_impl ? m_impl->visualLine(textLineNumber) : -1;
}

/*!
	\brief Convert a visual line number int a text (logical) line number

	\note this is not a 1:1 mapping as logical lines can span over several visual lines
*/
int QDocument::textLineNumber(int visualLineNumber) const
{
	return m_impl ? m_impl->textLine(visualLineNumber) : -1;
}

/*!
	\brief Clear the width constraint, if any
*/
void QDocument::clearWidthConstraint()
{
	if ( m_impl )
		m_impl->setWidth(0);
}

/*!
	\brief Set a new width constraint
	\param width maximum width to allow

	Passing a value inferior (or equal) to zero clear the width constraint, if any.
*/
void QDocument::setWidthConstraint(int width)
{
	if ( m_impl )
		m_impl->setWidth(qMax(0, width));
}

void QDocument::markFormatCacheDirty(){
    if ( m_impl )
	    m_impl->markFormatCacheDirty();
}

/*!
	\return the line object at a given line number
	\param line Text line to acces
*/
QDocumentLine QDocument::line(int line) const
{
	return QDocumentLine(m_impl->at(line));
}

/*!
	\return the line number corresponding to a given document y coordinate
	\param ypos Y document coordinate of the target
	\param wrap if not null, will be set to the wrap offset (position of the
	visual line among the sublines of the wrapped text line).

*/
int QDocument::lineNumber(int ypos, int *wrap) const
{
	int ln = ypos / m_impl->m_lineSpacing;

	return m_impl->textLine(ln, wrap);
}

/*!
	\return the line object to which an iterator points
*/
QDocumentLine QDocument::line(QDocumentConstIterator iterator) const
{
	return (m_impl && (m_impl->constEnd() != iterator)) ? QDocumentLine(*iterator) : QDocumentLine();
}

/*!
	\return A cursor operating on the document, placed at a given position
	This method has three functions:
	  cursor(l, c)           Creates a cursor at (l, c)
	  cursor(la, ca, lt)     Creates a cursor with anchor (la, ca) selecting to (lt, length of lt)
	  cursor(la, ca, lt, ct) Creates a cursor with anchor (la, ca) selecting to (lt, lc)

	\param line cursor line number (text line)
	\param column cursor text column
	\param lineTo selection ending line. If this is not given, it is set to line
	\param columnTo selection ending column. If this is not given it is set to the end of lineTo (if that is given) or to column
*/
QDocumentCursor QDocument::cursor(int line, int column, int lineTo, int columnTo) const
{
	//Q_ASSERT(line >= 0);
	//Q_ASSERT(lineTo < lineCount() );
	if ( lineTo == -1 )
		return QDocumentCursor(const_cast<QDocument*>(this), line, column);
	else {
		QDocumentCursor c(const_cast<QDocument*>(this), line, column);
		if (lineTo != lineCount()) {
			c.setLineNumber(lineTo, QDocumentCursor::KeepAnchor);
			if (columnTo != -1) c.setColumnNumber(columnTo, QDocumentCursor::KeepAnchor);
			else c.setColumnNumber(this->line(lineTo).length(), QDocumentCursor::KeepAnchor);
		} else {
			c.setLineNumber(lineCount()-1, QDocumentCursor::KeepAnchor);
			c.setColumnNumber(this->line(lineTo).length(), QDocumentCursor::KeepAnchor);
		}
		return c;
	}
}


/*!
	\return the document line which contains a given (document-wide) text position

	\note The sole purpose of this method is to have an API close to that of QTextDocument.
	This method being ridiculously slow it is recommended to avoid it whenever possible.
*/
QDocumentLine QDocument::findLine(int& position) const
{
	if ( !m_impl )
		return QDocumentLine();

	return QDocumentLine(m_impl->lineForPosition(position));
}

int QDocument::findLineContaining(const QString &searchText,  const int& startLine, const Qt::CaseSensitivity cs, const bool backward) const{
	if(backward){
		for (int i=startLine;i>-1;i--)
			if(line(i).text().contains(searchText,cs))
				return i;
	} else {
		for (int i=startLine;i<lines();i++)
			if(line(i).text().contains(searchText,cs))
				return i;
	}
	return -1;
}

int QDocument::findLineRegExp(const QString &searchText,  const int& startLine, const Qt::CaseSensitivity cs, const bool wholeWord, const bool useRegExp) const{

    QRegExp m_regexp=generateRegExp(searchText,cs==Qt::CaseSensitive,wholeWord,useRegExp);

    for (int i=startLine;i<lines();i++){
        if(m_regexp.indexIn(line(i).text(),0)>-1)
            return i;
    }

    return -1;
}



/*!
	\return The Y document coordinate of a given line
	\param ln textual line number
*/
int QDocument::y(int ln) const
{
	if ( !m_impl )
		return -1;

	return m_impl->m_lineSpacing * m_impl->visualLine(ln);
}

/*!
	\overload

	\return The Y document coordinate of a given line
	\param l line object

	\note Significantly slower than the line number based version.
*/
int QDocument::y(const QDocumentLine& l) const
{
	qDebug("bad perf...");

	return y(l.lineNumber());
}

/*!
	\return the rectangle (in document position) occupied by the line
	\param line textual line number

	\note the width of the returned rectangle is the DOCUMENT's width
*/
QRect QDocument::lineRect(int line) const
{
	const int yoff = y(line);

	return (yoff != -1) ? QRect(0, yoff, width(), this->line(line).lineSpan() * m_impl->m_lineSpacing) : QRect();
}

/*!
	\overload
	\return the rectangle (in document position) occupied by the line

	\note the width of the returned rectangle is the DOCUMENT's width
	\note Significantly slower than the line number based version.
*/
QRect QDocument::lineRect(const QDocumentLine& l) const
{
	//return lineRect(l.lineNumber());
	const int yoff = y(l);

	return (yoff != -1) ? QRect(0, yoff, width(), m_impl->m_lineSpacing) : QRect();
}

/*!
	\return the line at a given document position
*/
QDocumentLine QDocument::lineAt(const QPoint& p) const
{
	if ( !m_impl )
		return QDocumentLine();

	return line(lineNumber(p.y()));
}

/*!
	\return a document iterator pointing to the first line
*/
QDocumentConstIterator QDocument::begin() const
{
	Q_ASSERT(m_impl);

	return m_impl->m_lines.constBegin();
}

/*!
	\return a document iterator pointing past the last line
*/
QDocumentConstIterator QDocument::end() const
{
	Q_ASSERT(m_impl);

	return m_impl->m_lines.constEnd();
}

/*!
	\return a document iterator pointing to a given line
*/
QDocumentConstIterator QDocument::iterator(int ln) const
{
	Q_ASSERT(m_impl);

	return begin() + ln;
}

/*!
	\overload
	\note If you can avoid using this method, do so unless performance really isn't an issue
*/
QDocumentConstIterator QDocument::iterator(const QDocumentLine& l) const
{
	Q_ASSERT(m_impl);

	QDocumentConstIterator it = begin(), e = end();

	while ( (*it != l.handle()) && (it != e) )
		++it;

	return it;
}

/*!
	\brief Convert a document (or viewport) (x, y) position to a (line, column) cursor position
	\param p document position
	\param line where the line number will be stored
	\param column where the column (text position within line) will be stored
*/
void QDocument::cursorForDocumentPosition(const QPoint& p, int& line, int& column) const
{
	if ( !m_impl )
		return;

	int wrap = 0;
	line = lineNumber(p.y(), &wrap);
	QDocumentLine l = this->line(line);

	if ( !l.isValid() )
		return;

	//qDebug("%i %i", line, wrap);
	column = l.documentOffsetToCursor(p.x(), wrap * QDocumentPrivate::m_lineSpacing);

	//qDebug("(%i, %i) -> (%i [+%i], %i)", p.x(), p.y(), line, wrap, column);
}

/*!
	\return The cursor nearest to a document (x, y) position
*/
QDocumentCursor QDocument::cursorAt(const QPoint& p) const
{
	int ln = -1, col = -1;

	cursorForDocumentPosition(p, ln, col);

	return QDocumentCursor(const_cast<QDocument*>(this), ln, col);
}

/*!
	\brief Draw the contents of the document
	\param p painter to use
	\param cxt paint context (specifies what part of the document to draw, among other things)
*/
void QDocument::draw(QPainter *p, PaintContext& cxt)
{
	m_impl->draw(p, cxt);
}


QString QDocument::exportAsHtml(const QDocumentCursor& range, bool includeFullHeader, bool simplifyCSS) const{
	return m_impl->exportAsHtml(range.isValid()?range:cursor(0,0,lineCount()-1), includeFullHeader, simplifyCSS);
}
/*!
	\brief Execute a document command (editing operation)
*/
void QDocument::execute(QDocumentCommand *cmd)
{
	Q_ASSERT(m_impl || !cmd);
	if ( m_impl && cmd )
		m_impl->execute(cmd);

}

/*!
	\return The default line ending policy
*/
QDocument::LineEnding QDocument::defaultLineEnding()
{
	return QDocumentPrivate::m_defaultLineEnding;
}

/*!
	\brief Set the default line ending policy

	\note The line ending policy of existing documents is changed accordingly
*/
void QDocument::setDefaultLineEnding(QDocument::LineEnding le)
{
	QDocumentPrivate::m_defaultLineEnding = le;

	foreach ( QDocumentPrivate *d, QDocumentPrivate::m_documents )
	{
		d->m_doc->setLineEnding(le);
	}
}

/*!
	\return The default text codec used to load and save document contents

	\note a null pointer indicates auto detection
*/
QTextCodec* QDocument::defaultCodec(){
	return QDocumentPrivate::m_defaultCodec;
}
void QDocument::setDefaultCodec(QTextCodec* codec){
	QDocumentPrivate::m_defaultCodec=codec;
}

void QDocument::addGuessEncodingCallback(const GuessEncodingCallback& callback){
	if ( !guessEncodingCallbacks.contains(callback) )
		guessEncodingCallbacks.append(callback);
}
void QDocument::removeGuessEncodingCallback(const GuessEncodingCallback& callback){
	guessEncodingCallbacks.removeAll(callback);
}
/*!
	\return The default format scheme used to draw document contents
*/
QFormatScheme* QDocument::defaultFormatScheme()
{
	return QDocumentPrivate::m_defaultFormatScheme;
}

/*!
	\brief Set the default format scheme

	\note Existing documents using the default format scheme will see their format scheme changed
*/
void QDocument::setDefaultFormatScheme(QFormatScheme *f)
{
	foreach ( QDocumentPrivate *d, QDocumentPrivate::m_documents )
	{
		if ( d->m_formatScheme == QDocumentPrivate::m_defaultFormatScheme )
			d->setFormatScheme(f);
	}

	QDocumentPrivate::m_defaultFormatScheme = f;
}

/*!
	\deprecated
	\brief Compatibility alias for defaultFormatScheme()
*/
QFormatScheme* QDocument::formatFactory()
{
	return defaultFormatScheme();
}

/*!
	\deprecated
	\brief Compatibility alias for setDefaultFormatScheme()
*/
void QDocument::setFormatFactory(QFormatScheme *f)
{
	setDefaultFormatScheme(f);
}

/*!
	\brief Begin a macro

	Macro in QDocument map directly to macro in the underlying undo stack
*/
void QDocument::beginMacro()
{
	if ( m_impl )
		m_impl->beginChangeBlock();

}

/*!
	\brief End a macro
*/
void QDocument::endMacro()
{
	if ( m_impl )
		m_impl->endChangeBlock();

}

/*!
	\brief Is a macro active
*/
bool QDocument::hasMacros(){
	return m_impl?m_impl->hasChangeBlocks():false;
}

/*!
	\brief Get an available group id for matches
*/
int QDocument::getNextGroupId()
{
	return m_impl ? m_impl->getNextGroupId() : -1;
}

void QDocument::releaseGroupId(int groupId)
{
	if ( m_impl )
		m_impl->releaseGroupId(groupId);
}

/*!
	\brief Clear matches
*/
void QDocument::clearMatches(int gid)
{
	if ( m_impl )
	{
		m_impl->clearMatches(gid);
	}
}

/*void QDocument:: clearMatchesFromToWhenFlushing(int groupId, int firstMatch, int lastMatch);
	if ( m_impl )
	{
		m_impl->clearMatchesFromToWhenFlushing(gid,firstMatch,lastMatch);
	}
}*/

/*!
	\brief Highlight the matched sequences

	\note Both position are BEFORE the matched characters (cursor position).
*/
void QDocument::addMatch(int gid, int line, int pos, int len, int format)
{
	if ( m_impl )
	{
		m_impl->addMatch(gid, line, pos, len, format);
	}
}

/*!
	\
*/
void QDocument::flushMatches(int gid)
{
	if ( m_impl )
	{
		m_impl->flushMatches(gid);
	}
}

/*!
	\return Whether the document is in a clean state

	This is undo stak terminology. A clean document is one
	whose undo stack is at the same index it was upon last
	save/load. In other words : clean = !modified
*/
bool QDocument::isClean() const
{
	return m_impl ? m_impl->m_commands.isClean() : true;
}

void QDocument::expand(int line){
	if (!languageDefinition()) return;
	languageDefinition()->expand(this, line);
}
void QDocument::collapse(int line){
	if (!languageDefinition()) return;
	languageDefinition()->collapse(this, line);
}
void QDocument::expandParents(int l){
	if (!languageDefinition()) return;
	int prevLine=-1;
	while (line(l).isHidden()) {
		QMap<int,int>::const_iterator it=m_impl->m_hidden.upperBound(prevLine);
		prevLine=-1;
		for (;it!=m_impl->m_hidden.end();++it){
			if (it.key()<l && it.key()+ it.value()>=l){
				prevLine=it.key();
				expand(it.key());
				break;
			}
		}
		if (prevLine==-1) //don't loop forever
			break;
	}
}

//Collapse at the first possible point before/at the line
void QDocument::foldBlockAt(bool unFold, int l) {
	if (unFold) {
		//search nearest line which is folded and unfold it
		for (;l>=0;l--)
			if (line(l).hasFlag(QDocumentLine::CollapsedBlockStart)) {
				languageDefinition()->expand(this,l);
				break;
			}
	} else {
		//search latest line before l which can be folded and is not folded
		int foldAt=-1;
		QFoldedLineIterator fli = languageDefinition()->foldedLineIterator(this);
		while (fli.lineNr<=l) {
			if (fli.open && !fli.collapsedBlockStart) foldAt=fli.lineNr;
			++fli;
		}
		//fold it
		if (foldAt!=-1)
			languageDefinition()->collapse(this,foldAt);
	}
}

/*!
   (Internal method), returns if one of the lines between from and to (inclusive) belong to
   a folded block (they can still be all visible if one of them starts/ends a hidden block)
*/
bool QDocument::linesPartiallyFolded(int fromInc, int toInc){
	while ( fromInc <= toInc )
	{
		if (line(fromInc).hasAnyFlag(QDocumentLine::Hidden | QDocumentLine::CollapsedBlockStart | QDocumentLine::CollapsedBlockEnd) )
			return true;

		++fromInc;
	}
	return false;
}

/*!
   Correct the folding
    i.e., it ensures that no line is hidden which is not in an collapsable block
    (useful if the blocks have changed)
*/
void QDocument::correctFolding(int fromInc, int toInc){
	Q_UNUSED(fromInc);
	Q_UNUSED(toInc);
	//TODO: Optimize it, use fromInc/toInc to handle it locally (problem: behaviour of closing brackets depends on open brackets
	//      earlier in the document), merge the redunant folding correction in removeLines to this (problem: if all folded
	//      lines are removed, the hidden flags which are checked by ld->correctFolding are already all correct, but the m_hidden
	//      map won't be correct if it has not been corrected by the checks in removeLines)

	QLanguageDefinition* ld=languageDefinition();
	if (!ld)
		return;

	if (!ld->correctFolding(this))
		return;

	//recalculate the map of hidden lines (=cache) from the hidden flag of the lines
	m_impl->m_hidden.clear();	
	QList<QPair<int,int> > blockStartList;
	for (QFoldedLineIterator fli = ld->foldedLineIterator(this);
	     fli.line.isValid();
	     ++fli){
		if (fli.collapsedBlockStart)
			blockStartList << QPair<int,int>(fli.lineNr, fli.open);
		if (fli.collapsedBlockEnd){
			Q_ASSERT(!blockStartList.empty());
			int c=fli.close;
			while (blockStartList.size()>0 && blockStartList.last().second<=c){
				c-=blockStartList.last().second;
				if (fli.hiddenCollapsedBlockEnd)
					m_impl->m_hidden.insertMulti(blockStartList.last().first, fli.lineNr-blockStartList.last().first);
				else
					m_impl->m_hidden.insertMulti(blockStartList.last().first, fli.lineNr-blockStartList.last().first-1);
				blockStartList.removeLast();
			}
			if (c>0 && !blockStartList.empty()) blockStartList.last().second-=c;
		}
	}
	for (int i=0;i<blockStartList.size();i++)
		m_impl->m_hidden.insertMulti(blockStartList[i].first,lines()-1-blockStartList[i].first);

	m_impl->setHeight();
	//emitFormatsChange(line, count);
	m_impl->emitFormatsChanged();
}

/*!
	\brief Set the document to clean state

	This method does not go back to clean state but tell
	the stack that the current state is to be considered
	as the clean state.
*/
void QDocument::setClean()
{
	if ( m_impl )
	{
		m_impl->m_commands.setClean();
		//m_impl->m_status.clear();

		QHash<QDocumentLineHandle*, QPair<int, int> >::iterator it = m_impl->m_status.begin();

		while ( it != m_impl->m_status.end() )
		{
			it->second = it->first;
			++it;
		}
	}
}

/*!
	\return Whether a given line has been modified since last save/load
*/
bool QDocument::isLineModified(const QDocumentLine& l) const
{
	if ( !m_impl )
		return false;

	QHash<QDocumentLineHandle*, QPair<int, int> >::const_iterator it = m_impl->m_status.constFind(l.handle());

	//if ( it != m_impl->m_status.constEnd() )
	//	qDebug("%i vs %i", it->first, it->second);

	return it != m_impl->m_status.constEnd() ? it->first != it->second : false;
}

/*!
	\return Whether a given line has been modified since last load
*/
bool QDocument::hasLineEverBeenModified(const QDocumentLine& l) const
{
	return m_impl ? m_impl->m_status.contains(l.handle()) : false;
}

/*!
	\return the maximum number of marks on a single line

	This is meant for the line mark panel to smartly adapt its size.
*/
int QDocument::maxMarksPerLine() const
{
	return m_impl ? m_impl->maxMarksPerLine() : 0;
}

/*!
	\brief Find the next mark of a given type
	\return the line on which a mark was found
	\param id mark identifier to find
	\param from line from which to start search
	\param until line until which to search

	\a from and \a until can be negatives, in which case they
	indicate positions from the end of the document (i.e -1 is
	last line, -2 the line before last line and so on).

	If \a until is inferior to \a from and no matching mark
	is found in the [from, end] range then the [0, until]
	range will also be searched.
*/
int QDocument::findNextMark(int id, int from, int until) const
{
	return m_impl ? m_impl->findNextMark(id, from, until) : -1;
}

/*!
	\brief Find the previous mark of a given type
	\return the line on which a mark was found
	\param id mark identifier to find
	\param from line from which to start search
	\param until line until which to search

	\a from and \a until can be negatives, in which case they
	indicate positions from the end of the document (i.e -1 is
	last line, -2 the line before last line and so on).

	If \a until is superior to \a from and no matching mark
	is found in the [0, from] range then the [until, end]
	range will also be searched (both range being searched backward,
	of course).
*/
int QDocument::findPreviousMark(int id, int from, int until) const
{
	return m_impl ? m_impl->findPreviousMark(id, from, until) : -1;
}

void QDocument::removeMarks(int id){
    if (m_impl) m_impl->removeMarks(id);
}

/*!
	\return the date/time of the last modification of the document
	
	If the document has not been modified since its load the date/time
	of last modification (as reported by QFileInfo) will be returned.
*/
QDateTime QDocument::lastModified() const
{
	return m_impl ? m_impl->m_lastModified : QDateTime();
}

/*!
	\brief set the date/time of the last modification of the document
	
	You should not need to use that EVER. It is only provided for use
	in QEditor (and possibly in some panels).
*/
void QDocument::setLastModified(const QDateTime& d)
{
	if ( m_impl )
		m_impl->m_lastModified = d;
}

/////////////////////////
//	QDocumentLineHandle
/////////////////////////
//static quint32 q_line_counter = 0;

/*!
	\class QDocumentLineHandle
	\brief Private implementation of a document line
*/

/*!
	\
*/
QDocumentLineHandle::QDocumentLineHandle(QDocument *d)
 : m_doc(d)
#if QT_VERSION >= 0x040400
 , m_ref(1)
#endif
 , m_indent(0)
 , m_state(QDocumentLine::LayoutDirty)
 , m_layout(0)
 , lineHasSelection(QDocumentLineHandle::noSel)
{
	#if QT_VERSION < 0x040400
	m_ref.init(1);
	#endif
}

/*!
	\
*/
QDocumentLineHandle::QDocumentLineHandle(const QString& s, QDocument *d)
 : m_text(s)
 , m_doc(d)
#if QT_VERSION >= 0x040400
 , m_ref(1)
#endif
 , m_indent(0)
 , m_state(QDocumentLine::LayoutDirty)
 , m_layout(0)
 , lineHasSelection(QDocumentLineHandle::noSel)
{
	#if QT_VERSION < 0x040400
	m_ref.init(1);
	#endif
}

QDocumentLineHandle::~QDocumentLineHandle()
{
	Q_ASSERT(!m_ref);

	if ( m_doc && m_doc->impl() )
		m_doc->impl()->emitLineDeleted(this);
}

int QDocumentLineHandle::count() const
{
	return m_text.count();
}

int QDocumentLineHandle::length() const
{
	return m_text.length();
}

int QDocumentLineHandle::line() const
{
	return (m_doc && m_doc->impl()) ? m_doc->impl()->indexOf(this) : -1;
}

int QDocumentLineHandle::position() const
{
	return (m_doc && m_doc->impl()) ? m_doc->impl()->position(this) : -1;
}

QString QDocumentLineHandle::text() const
{
	return m_text;
}

int QDocumentLineHandle::indent() const
{
	int l = nextNonSpaceChar(0);
	return QDocument::screenColumn(m_text.constData(), l == -1 ? m_text.length() : l, m_doc->tabStop());
}

int QDocumentLineHandle::nextNonSpaceChar(uint pos) const
{
	const int len = m_text.length();
	const QChar *unicode = m_text.unicode();

	for ( int i = pos; i < len; ++i )
	{
		if ( !unicode[i].isSpace() )
			return i;
	}

	return -1;
}

int QDocumentLineHandle::previousNonSpaceChar(int pos) const
{
	const int len = m_text.length();
	const QChar *unicode = m_text.unicode();

	if ( pos < 0 )
		pos = 0;

	if ( pos >= len )
		pos = len - 1;

	for ( int i = pos; i >= 0; --i )
	{
		if ( !unicode[i].isSpace() )
			return i;
	}

	return -1;
}

QDocument* QDocumentLineHandle::document() const
{
	return m_doc;
}

bool QDocumentLineHandle::hasFlag(int s) const
{
	return m_state & s;
}

void QDocumentLineHandle::setFlag(int s, bool y) const
{
	if ( y )
		m_state |= s;
	else
		m_state &= ~s;
}

QDocumentLineHandle* QDocumentLineHandle::next() const
{
	return (m_doc && m_doc->impl()) ? m_doc->impl()->next(this) : 0;
}

QDocumentLineHandle* QDocumentLineHandle::previous() const
{
	return (m_doc && m_doc->impl()) ? m_doc->impl()->previous(this) : 0;
}

QList<int> QDocumentLineHandle::getBreaks(){
    QList<int> res;
    QPair<int, int> elem;
    foreach(elem,m_frontiers){
	res << elem.first;
    }
    return res;
}

void QDocumentLineHandle::updateWrap() const
{
	m_indent = 0;
	m_frontiers.clear();

	if ( !m_doc->impl()->m_constrained )
	{
		if ( m_layout )
			setFlag(QDocumentLine::LayoutDirty, true);

		return;
	}

	const int tabStop = m_doc->impl()->m_tabStop;
	const int maxWidth = m_doc->widthConstraint();

	if ( m_layout )
	{
		layout();
	} else if ( QDocumentPrivate::m_fixedPitch ) {

		int idx = 0, minx = 0, lastBreak = 0, lastWidth = 0, lastX = 0, rx,
			x = QDocumentPrivate::m_leftMargin, column = 0, cwidth;

		QChar c;
		int indent = 0;
		const int sw = QDocumentPrivate::m_spaceWidth;

		while ( idx < m_text.length() && m_text.at(idx).isSpace() )
		{
			c = m_text.at(idx);

			if ( c.unicode() == '\t' )
			{
				int taboffset = tabStop - (column % tabStop);

				column += taboffset;
				cwidth = sw * taboffset;
			} else {
				++column;
				cwidth = sw;
			}

			x += cwidth;
			++idx;
		}

		indent = idx;
		minx = rx = x;

		if ( (minx + sw) >= maxWidth )
		{
			//qWarning("Please stop shrinking so aggressively.\nNo attempt will be made to show something decent");
			// shrinking too aggresively (or too much spaces...) ungraceful fallback

			indent = idx = 0;
			minx = rx = x = QDocumentPrivate::m_leftMargin;
		}

		m_indent = minx - QDocumentPrivate::m_leftMargin;

		while ( idx < m_text.length() )
		{
			if ( c.isSpace() )  //!isWord(c) || !isWord(m_text.at(idx)) )
			{
				lastX = rx;
				lastWidth = x;
				lastBreak = idx;
			}

			c = m_text.at(idx);

			if ( c.unicode() == '\t' )
			{
				int taboffset = tabStop - (column % tabStop);

				column += taboffset;
				cwidth = sw * taboffset;
			} else {
				++column;
				cwidth = sw;
			}

			if ( x + cwidth > maxWidth )
			{
				if ( lastBreak == (m_frontiers.count() ? m_frontiers.last().first : indent) )
				{
					// perfect cut or fallback to aggressive cut
					m_frontiers << qMakePair(idx, rx);
					lastBreak = idx;
					lastWidth = x;
					lastX = rx;
					x = minx;
				} else if ( lastBreak < idx ) {
					// word cut at a non-ideal position
					m_frontiers << qMakePair(lastBreak, lastX);
					x = minx + (x - lastWidth);
				} else {
					m_frontiers << qMakePair(idx, rx);
					x = minx;
				}
			}

			rx += cwidth;
			x += cwidth;
			++idx;
		}
	} else {
		QVector<int> composited = compose();

		int idx = 0, minx = 0, lastBreak = 0, lastWidth = 0, lastX = 0, rx,
			x = QDocumentPrivate::m_leftMargin, column = 0, cwidth;

		QChar c;
		int indent = 0;
		int fmt = 0;
		//const QVector<QFont>& fonts = m_doc->impl()->m_fonts; // unused

		QDocumentPrivate *d = m_doc->impl();

		while ( idx < m_text.length() && m_text.at(idx).isSpace() )
		{
			c = m_text.at(idx);
			fmt = idx < composited.count() ? composited[idx] : 0;
			//QFontMetrics fm(fmt < fonts.count() ? fonts.at(fmt) : m_doc->font());

			if ( c.unicode() == '\t' )
			{
				int taboffset = tabStop - (column % tabStop);

				column += taboffset;
				cwidth = d->textWidth(fmt," ") * taboffset;
				//cwidth = fm.width(' ') * taboffset;
			} else {
				++column;
				cwidth = d->textWidth(fmt,QString(c));
				//cwidth = fm.width(c);
			}

			x += cwidth;
			++idx;
		}

		indent = idx;
		minx = rx = x;

		if ( (minx + QDocumentPrivate::m_spaceWidth) >= maxWidth )
		{
			//qWarning("Please stop shrinking so aggressively.\nNo attempt will be made to show something decent");

			indent = idx = 0;
			minx = rx = x = QDocumentPrivate::m_leftMargin;
		}

		m_indent = minx - QDocumentPrivate::m_leftMargin;
		int fmts[3];
		QFormat formats[3];

		while ( idx < m_text.length() )
		{
			if ( c.isSpace() )  //!isWord(c) || !isWord(m_text.at(idx)) )
			{
				lastX = rx;
				lastWidth = x;
				lastBreak = idx;
			}

			c = m_text.at(idx);
			fmt = idx < composited.count() ? composited[idx] : 0;
			int fontFormat;
			d->m_formatScheme->extractFormats(fmt, fmts, formats, fontFormat);

			if ( c.unicode() == '\t' )
			{
				int taboffset = tabStop - (column % tabStop);

				column += taboffset;
				cwidth = d->textWidth(fontFormat, " ");
				cwidth = cwidth * taboffset;
			} else {
				++column;
				cwidth = d->textWidth(fontFormat, c);
			}

			if ( x + cwidth > maxWidth )
			{
				if ( lastBreak == (m_frontiers.count() ? m_frontiers.last().first : indent) )
				{
					// perfect cut or fallback to aggressive cut
					m_frontiers << qMakePair(idx, rx);
					lastBreak = idx;
					lastWidth = x;
					lastX = rx;
					x = minx;
				} else if ( lastBreak < idx ) {
					// word cut at a non-ideal position
					m_frontiers << qMakePair(lastBreak, lastX);
					x = minx + (x - lastWidth);
				} else {
					m_frontiers << qMakePair(idx, rx);
					x = minx;
				}
			}

			rx += cwidth;
			x += cwidth;
			++idx;
		}
	}
}

int QDocumentLineHandle::cursorToX(int cpos) const
{
	cpos = qBound(0, cpos, m_text.length());

	if ( m_layout )
	{
		int xoff = QDocumentPrivate::m_leftMargin, coff = 0, line = m_frontiers.count();

		for ( int i = 0; i < m_frontiers.count(); ++i )
		{
			if ( m_frontiers.at(i).first >= cpos )
			{
				line = i;

				break;
			}
		}

		if ( line )
		{
			//coff = m_frontiers.at(line - 1).first;
			xoff = m_frontiers.at(line - 1).second;
		}

		//qDebug("c:%i (wrap:%i) => c2x(x - %i) + %i", cpos, line, coff, xoff);
		return qRound(m_layout->lineAt(line).cursorToX(cpos - coff)) + xoff;
	}

	int tabStop = m_doc->impl()->m_tabStop;

	if ( QDocumentPrivate::m_fixedPitch )
	{
		return QDocument::screenColumn(m_text.constData(), cpos, tabStop)
				* QDocumentPrivate::m_spaceWidth
				+ QDocumentPrivate::m_leftMargin;
	}

	//qDebug("c->x(%i) unsafe computations...", cpos);

	QVector<int> composited = compose();
	const QVector<QFont>& fonts = m_doc->impl()->m_fonts;

	if ( (composited.count() < cpos) || fonts.isEmpty() )
		return QFontMetrics(*QDocumentPrivate::m_font).width(m_text.left(cpos));

	int idx = 0, column = 0, cwidth;
	int screenx = QDocumentPrivate::m_leftMargin;

	while ( idx < cpos )
	{
		QChar c = m_text.at(idx);
		int fmt = idx < composited.count() ? composited[idx] : 0;
		QFontMetrics fm(fmt < fonts.count() ? fonts.at(fmt) : m_doc->font());

		if ( c == '\t' )
		{
			int taboffset = tabStop - (column % tabStop);

			column += taboffset;
			cwidth = fm.width(' ') * taboffset;
		} else {
			++column;
			cwidth = fm.width(c);
		}

		screenx += cwidth;
		++idx;
	}

	//qDebug("cursorToX(%i) = %i", cpos, screenx);

	return screenx;
}

int QDocumentLineHandle::xToCursor(int xpos) const
{
	//qDebug("x->c(%i) unsafe computations...", xpos);
	if ( m_layout )
	{
		int xoff = QDocumentPrivate::m_leftMargin, coff = 0, line = m_frontiers.count();

		for ( int i = 0; i < m_frontiers.count(); ++i )
		{
			if ( m_frontiers.at(i).second >= xpos )
			{
				line = i;

				break;
			}
		}

		if ( line )
		{
			//coff = m_frontiers.at(line - 1).first;
			xoff = m_frontiers.at(line - 1).second;
		}

		//qDebug("x:%i (wrap:%i) => x2c(x - %i) + %i", xpos, line, xoff, coff);
		return m_layout->lineAt(line).xToCursor(xpos - xoff) + coff;
	}

	int screenx = xpos;
	int tabStop = m_doc->impl()->m_tabStop;
	const QVector<QFont>& fonts = m_doc->impl()->m_fonts;

	if ( QDocumentPrivate::m_fixedPitch )
	{
		int screenPos =
					(screenx - QDocumentPrivate::m_leftMargin / 2)
				/
					QDocumentPrivate::m_spaceWidth
				;

		if ( tabStop == 1 )
			return screenPos;

		int idx = 0, column = 0;

		while ( (column < screenPos) && (idx < m_text.length()) )
		{
			QChar c = m_text.at(idx);

			if ( c == QLatin1Char('\t') )
			{
				int taboffset = tabStop - (column % tabStop);
				column += taboffset;
			} else {
				++column;
			}

			++idx;
		}

		return idx;
	} else {
		if ( screenx <= QDocumentPrivate::m_leftMargin )
			return 0;

		QVector<int> composited = compose();

		int idx = 0, x = 0, column = 0, cwidth;
		screenx -= QDocumentPrivate::m_leftMargin;

		while ( idx < m_text.length() )
		{
			int fmt = idx < composited.count() ? composited[idx] : 0;
			QFontMetrics fm(fmt < fonts.count() ? fonts.at(fmt) : m_doc->font());

			if ( m_text.at(idx) == '\t' )
			{
				int taboffset = tabStop - (column % tabStop);

				column += taboffset;
				cwidth = fm.width(' ') * taboffset;
			} else {
				++column;
				cwidth = fm.width(m_text.at(idx));
			}

			int mid = (x + (cwidth / 2) + 1);

			if ( screenx <= mid )
				return idx;
			else if ( screenx <= (x + cwidth) )
				return idx + 1;

			x += cwidth;
			++idx;
		}

		return m_text.length();
	}
}

int QDocumentLineHandle::wrappedLineForCursor(int cpos) const
{
	int wrap = m_frontiers.count();

	for ( int i = 0; i < m_frontiers.count(); ++i )
	{
		if ( m_frontiers.at(i).first > cpos )
		{
			wrap = i;
			break;
		}
	}

	return wrap;
}

int QDocumentLineHandle::documentOffsetToCursor(int x, int y) const
{
	int wrap = y / QDocumentPrivate::m_lineSpacing;

	if ( wrap > m_frontiers.count() )
	{
		// return an invalid value instead?
		//qDebug("a bit too far : (%i, %i)", x, y);
		//wrap = m_frontiers.count();

		return m_text.length();
	}

	if ( m_frontiers.count() )
	{
		//qDebug("(%i, %i) : %i", x, y, wrap);
		x = qMin(x, m_doc->widthConstraint());
	}

	x -= QDocumentPrivate::m_leftMargin;

	if ( m_layout )
	{
		return m_layout->lineAt(wrap).xToCursor(x);
	}

	int cpos = 0;
	int max = m_text.length();

	if ( wrap < m_frontiers.count() )
		max = m_frontiers.at(wrap).first - 1;

	if ( wrap > 0 )
		cpos = m_frontiers.at(wrap - 1).first;

	if ( wrap )
		x -= m_indent;


	int rx = 0, column = 0;
	QList<RenderRange> ranges;
	splitAtFormatChanges(&ranges, 0, cpos, max);

	QDocumentPrivate *d = m_doc->impl();

	foreach ( const RenderRange& r, ranges ) {
		int oldcolumn = column, oldrx = rx;
		int tempFmts[FORMAT_MAX_COUNT]; QFormat tempFormats[FORMAT_MAX_COUNT]; int newFont;
		d->m_formatScheme->extractFormats(r.format, tempFmts, tempFormats, newFont);
		d->calcPositionAfterRenderRange(rx, column, r, newFont, m_text);

		if ( rx > x ) {
			const QString& subText = m_text.mid(r.position, r.length);
			RenderRange rcopied = r;
			for ( int i = 0; i < r.length; i++ ) {
				rx = oldrx;
				column = oldcolumn;
				rcopied.length = i;
				d->calcPositionAfterRenderRange(rx, column, rcopied, newFont, m_text);
				if ( rx >= x ) break;
			}
			cpos += rcopied.length;
			/*for (int i = 0;)
			if ( r.format & FORMAT_SPACE ) {
				for (int i =  const QChar& c, subText ) {
					int len = QDocument::screenColumn(&c, 1, d->m_tabStop, column) - column;
					if ( rx + cw*len/2 >= x ) break;
					rx += cw*len;
					column += len;
					cpos++;
				}
			} else {
				foreach ( const QChar& c, subText ){
					int cw = d->textWidth(newFont, c);
					if ( rx + 2*cw/3 >= x ) break;
					rx += cw;
					cpos++;
				}
			}*/

			//move the cursor out of multi-byte ucs-2 characters
			while (cpos < m_text.length() && m_text.at(cpos).category() == QChar::Mark_NonSpacing)
				cpos++;
			if (cpos < m_text.length() && m_text.at(cpos).isLowSurrogate() && cpos > 0 && m_text.at(cpos - 1).isHighSurrogate())
				cpos++;

			return cpos;
		}
		cpos += r.length;
	}
	return cpos;
}

void QDocumentLineHandle::cursorToDocumentOffset(int cpos, int& x, int& y) const
{
	if ( cpos > m_text.length() )
		cpos = m_text.length();
	else if ( cpos < 0 )
		cpos = 0;

	int idx = 0;
	int wrap = wrappedLineForCursor(cpos);

	x = QDocumentPrivate::m_leftMargin;
	y = wrap * QDocumentPrivate::m_lineSpacing;

	if ( wrap )
	{
		idx = m_frontiers.at(wrap - 1).first;
	}

	int column = 0;

	if ( m_layout )
	{
		x += int(m_layout->lineAt(wrap).cursorToX(cpos));
	} else {
		if ( wrap )
			x += m_indent;

		QList<RenderRange> ranges;
		splitAtFormatChanges(&ranges, 0, idx, qMin(text().length(), cpos));

		QDocumentPrivate *d = m_doc->impl();

		foreach (const RenderRange& r, ranges) {
			int tempFmts[FORMAT_MAX_COUNT]; QFormat tempFormats[FORMAT_MAX_COUNT]; int newFont;
			d->m_formatScheme->extractFormats(r.format, tempFmts, tempFormats, newFont);
			d->calcPositionAfterRenderRange(x, column, r, newFont, m_text);
		}
	}
}

QPoint QDocumentLineHandle::cursorToDocumentOffset(int cpos) const
{
	QPoint p;
	cursorToDocumentOffset(cpos, p.rx(), p.ry());
	return p;
}

void QDocumentLineHandle::clearOverlays()
{
	m_overlays.clear();

	//setFlag(QDocumentLine::LayoutDirty, true);
	setFlag(QDocumentLine::FormatsApplied, false);
	//applyOverlays();
}

void QDocumentLineHandle::clearOverlays(int format){
	int oldsize = m_overlays.size();
	for ( int i = m_overlays.size()-1; i>=0; i-- )
		if ( m_overlays[i].format == format )
			m_overlays.removeAt(i);

	if (oldsize != m_overlays.size())
		setFlag(QDocumentLine::FormatsApplied, false);
}

void QDocumentLineHandle::addOverlay(const QFormatRange& over)
{
	m_overlays << over;

	//setFlag(QDocumentLine::LayoutDirty, true);
	setFlag(QDocumentLine::FormatsApplied, false);

	//applyOverlays();
}

void QDocumentLineHandle::removeOverlay(const QFormatRange& over)
{
	int i = m_overlays.removeAll(over);
	/*if ( !i )
		qDebug("lost overlay");
		*/
	//setFlag(QDocumentLine::LayoutDirty, true);
	if ( i )
		setFlag(QDocumentLine::FormatsApplied, false);
	//applyOverlays();
}

bool QDocumentLineHandle::hasOverlay(int id){
    for (int i =0; i<m_overlays.size();i++)
        if (m_overlays[i].format==id) return true;
    return false;
}

QList<QFormatRange> QDocumentLineHandle::getOverlays(int preferredFormat){
	if (preferredFormat==-1) return m_overlays;
	/*QList<QFormatRange> result=m_overlays;
	    for (int i=result.size()-1;i>=0;i--)
		if (result[i].format!=preferredFormat) result.removeAt(i);*/
	QList<QFormatRange> result;
	for (int i=0;i<m_overlays.size();i++)
		if (m_overlays[i].format==preferredFormat) result.append(m_overlays[i]);
	return result;
}

QFormatRange QDocumentLineHandle::getOverlayAt(int index, int preferredFormat){
    QFormatRange best(0,0,0);
    foreach (QFormatRange fr, m_overlays)
        if (fr.offset<=index && fr.offset+fr.length>=index && (fr.format==preferredFormat || (preferredFormat==-1)))
            if (best.length<fr.length) best=fr;
    return best;
}

QFormatRange QDocumentLineHandle::getFirstOverlayBetween(int start, int end, int preferredFormat){
    QFormatRange best(0,0,0);
    foreach (QFormatRange fr, m_overlays)
        if (fr.offset<=end && fr.offset+fr.length>=start && (fr.format==preferredFormat || (preferredFormat==-1)))
            if (fr.offset<best.offset || best.length==0) best=fr;
    return best;
}
QFormatRange QDocumentLineHandle::getLastOverlayBetween(int start, int end, int preferredFormat){
    QFormatRange best(0,0,0);
    foreach (QFormatRange fr, m_overlays)
        if (fr.offset<=end && fr.offset+fr.length>=start && (fr.format==preferredFormat || (preferredFormat==-1)))
            if (fr.offset>best.offset|| best.length==0) best=fr;
    return best;
}

void QDocumentLineHandle::shiftOverlays(int position, int offset)
{
	if ( offset > 0 )
	{
		for ( int i = 0; i < m_overlays.count(); ++i )
		{
			QFormatRange& r = m_overlays[i];

			if ( r.offset >= position )
			{
				r.offset += offset;
			} else if ( r.offset + r.length > position ) {
				m_overlays.removeAt(i);
				--i;
			}
		}
	} else if ( offset < 0 ) {
		const int max = position - offset;

		for ( int i = 0; i < m_overlays.count(); ++i )
		{
			QFormatRange& r = m_overlays[i];

			if ( r.offset >= max )
			{
				r.offset += offset;
			} else if ( r.offset + r.length >= position ) {
				m_overlays.removeAt(i);
				--i;
			}
		}
	}

	setFlag(QDocumentLine::FormatsApplied, false);
}

void QDocumentLineHandle::setFormats(const QVector<int>& fmts)
{
	m_formats = fmts;

	while ( m_formats.count() > m_text.length() )
		m_formats.pop_back();

	while ( m_formats.count() < m_text.length() )
		m_formats.append(0);

	//setFlag(QDocumentLine::LayoutDirty, true);
	setFlag(QDocumentLine::FormatsApplied, false);
	//applyOverlays();
}

QVector<int> QDocumentLineHandle::compose() const
{
	//QMediumArray m_composited(m_text.length());
	if ( hasFlag(QDocumentLine::FormatsApplied) )
		return m_cache;

	m_cache.resize(m_text.length());

	for ( int i = 0; i < qMin(m_formats.count(), m_text.length()); ++i )
		m_cache[i] = m_formats.at(i);

	for ( int i = m_formats.count(); i < m_text.length(); ++i )
		m_cache[i] = 0;

	// compositing formats and overlays	
        //QDocumentPrivate * d = document()->impl();
	foreach ( const QFormatRange& r, m_overlays )
	{
		int beg = qMax(0, r.offset);
		int end = qMin(r.offset + r.length, m_cache.count());

		for ( int i = beg; i < end; ++i )
			QFormatScheme::mergeFormats(m_cache[i], r.format);
	}
//qDebug("\n");
	setFlag(QDocumentLine::FormatsApplied, true);

	return m_cache;
}

QList<QTextLayout::FormatRange> QDocumentLineHandle::decorations() const
{
	if ( !hasFlag(QDocumentLine::FormatsApplied) )
		compose();

	// turning format "map" into ranges that QTextLayout can understand...
	QList<QTextLayout::FormatRange> m_ranges;

	int fid = 0;
	QTextLayout::FormatRange r;
	r.start = r.length = -1;

	int i = 0;

	//if ( m_cache.isEmpty() )
	//	qWarning("empty cache...");

	while ( i < m_cache.count() )
	{
		while ( (i < m_cache.count()) && !m_cache[i] )
			++i;

		if ( i >= m_cache.count() )
			break;

		fid = m_cache[i];

		r.start = i;
		r.format = m_doc->formatScheme()->format(fid).toTextCharFormat();

		while ( (i < m_cache.count()) && (m_cache[i] == fid) )
			++i;

		if ( i >= m_cache.count() )
			break;

		r.length = i - r.start;
		m_ranges << r;

		r.start = r.length = -1;
	}

	if ( r.start != -1 )
	{
		r.length = m_cache.count() - r.start;
		m_ranges << r;
	}

	return m_ranges;
}

void QDocumentLineHandle::applyOverlays() const
{
	if ( !m_layout )
		return;

	//m_layout->setAdditionalFormats(decorations());

	//setFlag(QDocumentLine::FormatsApplied, true);
}

void QDocumentLineHandle::layout() const
{
	bool needLayout = false;
	static QList<QChar::Direction> m_layoutRequirements = QList<QChar::Direction>()
		<< QChar::DirR
		<< QChar::DirAL
		<< QChar::DirRLE
		<< QChar::DirRLO
		<< QChar::DirPDF
		<< QChar::DirAN;

	for ( int i = 0; (i < m_text.length()) && !needLayout; ++i )
	{
		QChar c = m_text.at(i);

		needLayout = m_layoutRequirements.contains(c.direction());
	}

	if ( needLayout )
	{
		//qDebug("layout needed at line %i", this->line());

		if ( !m_layout )
		{
			m_layout = new QTextLayout(m_text, QDocument::font());
		} else {
			m_layout->setText(m_text);
			//m_layout->setFont(config()->font());
		}

		m_layout->setCacheEnabled(false);
		// Initial setup of the QTextLayout.

		// Tab width
		QTextOption opt;
		opt.setFlags(QTextOption::IncludeTrailingSpaces);
		opt.setTabStop(m_doc->tabStop() * QDocumentPrivate::m_spaceWidth);

		//opt.setWrapMode(QTextOption::NoWrap);
		opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

		// Find the first strong character in the string.
		// If it is an RTL character, set the base layout direction of the string to RTL.
		//
		// See http://www.unicode.org/reports/tr9/#The_Paragraph_Level (Sections P2 & P3).
		// Qt's text renderer ("scribe") version 4.2 assumes a "higher-level protocol"
		// (such as KatePart) will specify the paragraph level, so it does not apply P2 & P3
		// by itself. If this ever change in Qt, the next code block could be removed.
		if ( m_text.isRightToLeft() )
			opt.setTextDirection(Qt::RightToLeft);
		
		m_layout->setTextOption(opt);
		
		// Syntax highlighting, inbuilt and arbitrary
		//m_layout->setAdditionalFormats(m_ranges);

		// Begin layouting
		m_layout->beginLayout();

		m_frontiers.clear();

		int i = 0, rx = 0, height = 0, minwidth = 0;

		forever
		{
			QTextLine line = m_layout->createLine();

			if ( !line.isValid() )
			{
				//if ( m_layout->lineCount() > 1 )
				//	qWarning("Troubles expected on line %i", this->line());

				break;
			}

			if ( m_doc->widthConstraint() )
				line.setLineWidth(m_doc->widthConstraint() - QDocumentPrivate::m_leftMargin);
			else
				line.setNumColumns(m_text.length());

			rx += qRound(line.naturalTextWidth()); //qRound(line.cursorToX(line.textLength()));

			if ( m_doc->impl()->m_constrained && m_layout->textOption().textDirection() == Qt::RightToLeft )
			{
				line.setPosition(QPoint(qRound(qreal(m_doc->widthConstraint() - 2 * QDocumentPrivate::m_leftMargin) - line.naturalTextWidth()), height));
			} else {
				line.setPosition(QPoint(minwidth, height));
				
				if ( !i )
				{
					m_indent = minwidth = cursorToX(nextNonSpaceChar(0)) - QDocumentPrivate::m_leftMargin;
					
					if ( minwidth < 0 || minwidth >= m_doc->widthConstraint() )
						minwidth = 0;
			}
			}

			m_frontiers << qMakePair(line.textStart() + line.textLength(), rx);
			
			++i;

			height += QDocumentPrivate::m_lineSpacing;
			//height += QDocument::fontMetrics().height();
		}

		m_frontiers.pop_back();

		m_layout->endLayout();
	} else {
		if ( m_layout )
			delete m_layout;

		m_layout = 0;
		//updateWrap();
	}

	setFlag(QDocumentLine::LayoutDirty, false);
}



void QDocumentLineHandle::splitAtFormatChanges(QList<RenderRange>* ranges, const QVector<int>* sel, int from, int until) const{
	if (until == -1 || until > m_text.count())
		until = m_text.count();

	if (from == until)
		return;

	QVector<int> m_composited = compose();

	QVector<int> merged;
	merged.fill(0, until);

	// merge selection ranges with the rest (formats + overlays)
	//int max = qMin(m_text.count(), m_composited.count());

	for ( int i = from; i < until; ++i )
	{
		if ( m_composited.count() > i )
			merged[i] = m_composited.at(i);

		// separate spaces to ease rendering loop
		if ( m_text.at(i).isSpace() )
			merged[i] |= FORMAT_SPACE;
	}

	if (sel) {
		for ( int i = 0; i < sel->count(); i += 2 )
		{
			int max = m_text.length();

			if ( (i + 1) < sel->count() )
				max = qMin(sel->at(i + 1), max);

			for ( int j = sel->at(i); j < max; ++j )
				merged[j] |= FORMAT_SELECTION;
		}
	}

	// generate render ranges
	if ( merged.count() )
	{
		int i = from, wrap = 0;
		int frontier = qMin(until,m_frontiers.count() ? m_frontiers.first().first : until);

		if (from != 0)
			while (i > frontier) {
				++wrap;
				frontier = wrap < m_frontiers.count() ? m_frontiers.at(wrap).first : until;
			}


		while ( i < until )
		{
			RenderRange r;
			r.position = i;
			r.length = 1;
			r.wrap = wrap;
			r.format = merged.at(i);

			while ( ((i + 1) < frontier) && (merged.at(i + 1) == r.format) )
			{
				++r.length;
				++i;
			}

			*ranges << r;
			++i;

			if ( i == frontier )
			{
				++wrap;
				frontier = wrap < m_frontiers.count() ? m_frontiers.at(wrap).first : until;
			}
		}
	} else if ( m_frontiers.count() ) {
		//TODO: is this branch ever reached?
		Q_ASSERT(false);

		// no formatting (nor selection) : simpler
		int i = 0, wrap = 0, max = m_text.count(),
			frontier = m_frontiers.count() ? m_frontiers.first().first : max;

		while ( i < max )
		{
			RenderRange r;
			r.position = i;
			r.length = 1;
			r.wrap = wrap;
			r.format = fullSel ? FORMAT_SELECTION : 0;

			while ( ((i + 1) < frontier) )
			{
				++r.length;
				++i;
			}

			*ranges << r;
			++i;

			if ( i == frontier )
			{
				++wrap;
				frontier = wrap < m_frontiers.count() ? m_frontiers.at(wrap).first : max;
			}
		}
	} else {
		// neither formatting nor line wrapping : simple drawText()
		RenderRange r;
		r.position = 0;
		r.length = m_text.length();
		r.format = fullSel ? FORMAT_SELECTION : 0;

		*ranges << r;
	}
}

void QDocumentLineHandle::draw(	QPainter *p,
								int xOffset,
								int vWidth,
								const QVector<int>& sel,
								const QVector<int>& cursor,
								const QPalette& pal,
								bool fullSel,
								int yStart,
								int yEnd) const
{
	if ( hasFlag(QDocumentLine::LayoutDirty) )
		layout();


	if ( m_layout )
	{
		if ( !hasFlag(QDocumentLine::FormatsApplied) )
			m_layout->setAdditionalFormats(decorations());

		//if ( !hasFlag(QDocumentLine::FormatsApplied) )
		//	applyOverlays();

		const int lineSpacing = QDocumentPrivate::m_lineSpacing;

		QVector<QTextLayout::FormatRange> selections;

		QTextCharFormat fmt;
		fmt.setBackground(pal.highlight());
		fmt.setForeground(pal.highlightedText());

		QTextLayout::FormatRange range;
		if ( fullSel )
		{
			range.start = 0;
			range.format = fmt;
			range.length = m_text.length();
			selections << range;
		} else {
			for ( int i = 0; i < sel.count(); ++i )
			{
				range.start = sel[i];
				range.format = fmt;

				if ( (i + 1) < sel.count() )
				{
					// regular selection subset
					range.length = sel[++i] - range.start;

				} else if ( m_layout->lineCount() ) {
					// span to end of line, not only text
					range.length = m_text.length() - range.start;
					qreal lineWidth = m_layout->lineAt(m_layout->lineCount() - 1).naturalTextWidth();
					const int endX = QDocumentPrivate::m_leftMargin + qRound(lineWidth) - xOffset;

					QRect area(endX, lineSpacing * i, vWidth - endX, lineSpacing);

					p->fillRect(area, fmt.background());
				}

				selections << range;
			}
		}

		QPoint off(QDocumentPrivate::m_leftMargin, 0);

		m_layout->draw(p, off, selections);

		for ( int i = 0; i < cursor.count(); ++i )
			m_layout->drawCursor(p, off, cursor[i]);

		//m_layout->clearAdditionalFormats();
	} else if ( m_text.isEmpty() ) {
		// enforce selection drawing on empty lines
		if ( sel.count() == 1 )
			p->fillRect(
						qMax(xOffset, QDocumentPrivate::m_leftMargin),
						0,
						vWidth,
						QDocumentPrivate::m_lineSpacing,
						pal.highlight()
						);

		// enforce cursor drawing on empty lines
		if ( cursor.count() && (xOffset < QDocumentPrivate::m_leftMargin) )
			p->drawLine(
						QDocumentPrivate::m_leftMargin,
						0,
						QDocumentPrivate::m_leftMargin,
						QDocumentPrivate::m_lineSpacing
						);
		// draw line width when hard wrapping is activated
		if(m_doc->impl()->hardLineWrap()){
		    p->save();
		    p->setPen(Qt::lightGray);
		    p->drawLine(m_doc->impl()->width(), 0,m_doc->impl()->width() , QDocumentPrivate::m_lineSpacing);
		    p->restore();
		}

	} else {
		QList<RenderRange> ranges;
		splitAtFormatChanges(&ranges, &sel);

		// find start of trailing whitespaces
		int last = m_text.length();

		while ( (last > 0) && m_text.at(last - 1).isSpace() )
			--last;


		int fmt = fullSel ? FORMAT_SELECTION : 0;
		int lastFont = 0;
		QDocumentPrivate *d = m_doc->impl();
		const int ts = d->m_tabStop;
		const int maxWidth = xOffset + vWidth;
		const bool unbounded = sel.count() & 1;
		const QColor ht = pal.highlightedText().color();

		const bool showTabs = QDocument::showSpaces() & QDocument::ShowTabs,
				showLeading = QDocument::showSpaces() & QDocument::ShowLeading,
				showTrailing = QDocument::showSpaces() & QDocument::ShowTrailing;

		//const int fns = nextNonSpaceChar(0);
		int indent = qMax(0, m_indent) + QDocumentPrivate::m_leftMargin;

		int cidx = 0;
		int rngIdx = 0;
		int column = 0;
#ifdef Q_OS_LINUX
		bool continuingWave = false, brokenWave = false;
		int dir = 0; // 0 = down; 1 = up
#endif
		int wrap = 0, xpos = QDocumentPrivate::m_leftMargin, ypos = 0;
		bool leading = ranges.first().format & FORMAT_SPACE, pastLead = false;

		foreach ( const RenderRange& r, ranges )
		{
			++rngIdx;

			if ( wrap != r.wrap )
			{
#ifdef Q_OS_LINUX
				continuingWave = false;
#endif
				if ( fmt & FORMAT_SELECTION )
				{
					// finish selection
					p->fillRect(
									xpos, ypos,
									maxWidth - xpos, QDocumentPrivate::m_lineSpacing,
									pal.highlight()
								);

				}

				/*
				if ( pastLead && (r.format & FORMAT_SPACE) )
				{
					indent = QDocumentPrivate::m_leftMargin;
				}
				*/

				++wrap;
				column = 0;
				ypos += QDocumentPrivate::m_lineSpacing;
				if(ypos>yEnd) return;
				xpos = indent;

				if ( r.format & FORMAT_SELECTION )
				{
					// finish selection
					p->fillRect(
									QDocumentPrivate::m_leftMargin, ypos,
									xpos, QDocumentPrivate::m_lineSpacing,
									pal.highlight()
								);

				}
			}
			if ( ypos < yStart ) continue;

			if ( leading && !(r.format & FORMAT_SPACE) )
			{
				//indent = xpos;
				leading = false;
				pastLead = true;
			}

			// TODO : clip more accurately (i.e inside ranges)
			if ( xpos > maxWidth ){
				if( d->hardLineWrap() ) continue;
				else break;
			}

			fmt = r.format;
			int fmts[FORMAT_MAX_COUNT];
			QFormat formats[FORMAT_MAX_COUNT];
			int newFont = lastFont;
			d->m_formatScheme->extractFormats(fmt, fmts, formats, newFont);

			if (newFont != lastFont) {
				d->tunePainter(p, newFont);
				lastFont = newFont;
			}

			int rwidth = 0;
			int tcol = column;
			const QString rng = m_text.mid(r.position, r.length);

			if ( r.format & FORMAT_SPACE )
			{
				int currentSpaceWidth = d->textWidth(newFont, " ");
				foreach ( QChar c, rng )
				{
					if ( c.unicode() == '\t' )
					{
						int toff = ts - (tcol % ts);
						rwidth += toff * currentSpaceWidth;
						tcol += toff;
					} else {
						rwidth += currentSpaceWidth;
						++tcol;
					}
				}
			} else {
				column += r.length;
				rwidth = d->textWidth(lastFont, rng);
			}

			if ( (xpos + rwidth) <= xOffset )
			{
				xpos += rwidth;

				if ( r.format & FORMAT_SPACE )
					column = tcol;

				continue;
			}

			int xspos = xpos;
			const QPen oldpen = p->pen();
			const int baseline = ypos + QDocumentPrivate::m_ascent;



			if ( fullSel || (fmt & FORMAT_SELECTION) )
			{
				p->setPen(ht);

				p->fillRect(xpos, ypos,
							rwidth, QDocumentPrivate::m_lineSpacing,
							pal.highlight());

			} else {
				if ( formats[0].foreground.isValid() ) p->setPen(formats[0].foreground);
				else if ( formats[1].foreground.isValid() ) p->setPen(formats[1].foreground);
				else if ( formats[2].foreground.isValid() ) p->setPen(formats[2].foreground);
				else p->setBrush(pal.text());

				if ( formats[0].background.isValid() )
				{
					p->fillRect(xpos, ypos,
								rwidth, QDocumentPrivate::m_lineSpacing,
								formats[0].background);
				} else if ( formats[1].background.isValid() )
				{
					p->fillRect(xpos, ypos,
								rwidth, QDocumentPrivate::m_lineSpacing,
								formats[1].background);
				} else if ( formats[2].background.isValid() )
				{
					p->fillRect(xpos, ypos,
								rwidth, QDocumentPrivate::m_lineSpacing,
								formats[2].background);
				}
			}

			if ( r.format & FORMAT_SPACE )
			{
				// spaces
				int max = r.position + r.length;

				if ( rngIdx == ranges.count() )
					++max;

				int currentSpaceWidth = d->textWidth(newFont, " ");
				for ( int i = r.position; i < max; ++i )
				{
					while ( cidx < cursor.count() )
					{
						if ( cursor.at(cidx) == i )
						{
							p->drawLine(xpos, ypos, xpos, ypos + QDocumentPrivate::m_lineSpacing);

							++cidx;
						} else {
							break;
						}
					}

					if ( i == r.position + r.length )
						break;

					bool isTab = m_text.at(i).unicode() == '\t';

					if ( isTab )
					{
						int toff = ts - (column % ts);
						column += toff;
						int xoff = toff * currentSpaceWidth;
						/*
						if ( r.format & FORMAT_SELECTION )
						{
							p->fillRect(xpos, ypos,
										xoff, QDocumentPrivate::m_lineSpacing,
										pal.highlight());

						}
						*/
						if ( showTabs )
						{
							p->translate(xpos - m_spaceSignOffset, ypos + QDocumentPrivate::m_ascent);
							p->drawPoints(m_spaceSign, sizeof(m_spaceSign) / sizeof(QPoint));
							p->translate(m_spaceSignOffset - xpos,-ypos - QDocumentPrivate::m_ascent);
						}

						xpos += xoff;
					} else {
						++column;
						/*
						if ( r.format & FORMAT_SELECTION )
						{
							p->fillRect(xpos, ypos,
										QDocumentPrivate::m_spaceWidth, QDocumentPrivate::m_lineSpacing,
										pal.highlight());

						}
						*/
						if (
								(
									leading
								&&
									showLeading
								)
							||
								(
									(r.position >= last)
								&&
									showTrailing
								)
							)
						{
							p->translate(xpos - m_spaceSignOffset, ypos + QDocumentPrivate::m_ascent);
							p->drawPoints(m_spaceSign, sizeof(m_spaceSign) / sizeof(QPoint));
							p->translate(m_spaceSignOffset - xpos,-ypos - QDocumentPrivate::m_ascent);
						}

						xpos += currentSpaceWidth;
					}
				}

				/*
				if ( leading )
				{
					//indent = xpos;
					leading = false;
					pastLead = true;
				}
				*/

			} else {
				p->drawText(xpos, baseline, rng);

				while ( cidx < cursor.count() )
				{
					const int xcoff = cursor.at(cidx) - r.position;

					if ( xcoff < 0 )
					{
						++cidx;
						continue;
					}

					if ( xcoff < (rngIdx == ranges.count() ? r.length + 1 : r.length) )
					{
						int xcpos = xpos;

						if ( xcoff )
							xcpos += d->textWidth(newFont, rng.left(xcoff));

						//qDebug("drawing cursor %i (col %i, x=%i)", cidx, cursor.at(cidx), xcpos);

						p->drawLine(xcpos, ypos, xcpos, ypos + QDocumentPrivate::m_lineSpacing);

						++cidx;
					} else {
						break;
					}
				}

				xpos += rwidth;
			}

			//qDebug("underline pos : %i", p->fontMetrics().underlinePos());

			if ( formats[0].linescolor.isValid() ) p->setPen(formats[0].linescolor);
			else if ( formats[1].linescolor.isValid() ) p->setPen(formats[1].linescolor);
			else if ( formats[2].linescolor.isValid() ) p->setPen(formats[2].linescolor);

			const int ydo = qMin(baseline + p->fontMetrics().underlinePos(), ypos + QDocumentPrivate::m_lineSpacing - 1);
			const int yin = baseline - p->fontMetrics().strikeOutPos();
			const int yup = qMax(baseline - p->fontMetrics().overlinePos() + 1, ypos);

			if ( formats[0].overline || formats[1].overline || formats[2].overline )
			{
				p->drawLine(xspos, yup, xpos, yup);
			}

			if ( formats[0].strikeout || formats[1].strikeout || formats[2].strikeout )
			{
				p->drawLine(xspos, yin, xpos, yin);
			}

			if ( formats[0].underline || formats[1].underline || formats[2].underline )
			{
				p->drawLine(xspos, ydo, xpos, ydo);
			}

			if ( formats[0].waveUnderline || formats[1].waveUnderline || formats[2].waveUnderline )
 			{
				/*
				those goddamn font makers take liberties with common sense
				and make it so damn harder to figure proper y offset for wave
				underline (keeping the regular underline pos make it look
				weird or even invisible on some fonts... reference rendering
				with DejaVu Sans Mono)
				*/

				// we used fixed wave amplitude of 3 (well strictly speaking
				// amplitude would be 1.5 but let's forget about waves and
				// focus on getting that code to work...

				// gotta center things
				const int ycenter = ypos + QDocumentPrivate::m_lineSpacing - 3;
									/*
									qMin(
										ypos + (QDocumentPrivate::m_ascent + QDocumentPrivate::m_lineSpacing) / 2,
										ypos + QDocumentPrivate::m_lineSpacing - 3
									);*/

				//if (format.waveUnderlineForeground.isValid())
				//	p->setPen(format.waveUnderlineForeground);
#ifndef Q_OS_LINUX
				QPen pn2=p->pen();
				QVector<qreal>pattern2;
				pattern2 << 1.0 << 3.0;
				pn2.setDashPattern(pattern2);
				p->setPen(pn2);
				p->drawLine(xspos, ydo, xpos, ydo);
				p->drawLine(xspos+1, ydo-1, xpos, ydo-1);
				p->drawLine(xspos+2, ydo, xpos, ydo);
				p->drawLine(xspos+3, ydo+1, xpos, ydo+1);
				/*
				QColor cl=p->pen().color();
				QImage wv(4,3,QImage::Format_ARGB32);
				wv.fill(0x00ffffff);
				wv.setPixel(0,0,cl.rgba());
				wv.setPixel(1,1,cl.rgba());
				wv.setPixel(2,2,cl.rgba());
				wv.setPixel(3,1,cl.rgba());

				QBrush bwv(wv);
				QTransform tf;
				tf.translate(0,ycenter%3);
				bwv.setTransform(tf);

				p->save();
				p->setBrush(bwv);
				p->setPen(Qt::NoPen);
				p->drawRect(xspos,ycenter,rwidth,3);
				p->restore();
				*/
			}
#else

				int cp = 0;
				brokenWave = false;

				while ( cp < rwidth )
 				{
					if ( !cp && !continuingWave )
					{
						dir = 0;
						p->drawLine(xspos, ycenter, xspos + 1, ycenter + 1);
						++cp;
					} else if ( !cp && brokenWave ) {
						if ( !dir )
							p->drawLine(xspos, ycenter, xspos + 1, ycenter + 1);
						else
							p->drawLine(xspos, ycenter, xspos + 1, ycenter - 1);

					} else {
						if ( cp + 2 > rwidth)
						{
							if ( !dir )
								p->drawLine(xspos + cp, ycenter - 1, xspos + cp + 1, ycenter);
							else
								p->drawLine(xspos + cp, ycenter + 1, xspos + cp + 1, ycenter);

							// trick to keep current direction
							dir ^= 1;

							brokenWave = true;
							++cp;
						} else {
							if ( !dir )
								p->drawLine(xspos + cp, ycenter - 1, xspos + cp + 2, ycenter + 1);
							else
								p->drawLine(xspos + cp, ycenter + 1, xspos + cp + 2, ycenter - 1);

							cp += 2;
						}
					}

					dir ^= 1;
 				}

				continuingWave = true;
 			} else {
				continuingWave = false;
				dir = 0;

			}
#endif
			if(m_doc->impl()->hardLineWrap()){
			    p->setPen(Qt::lightGray);
			    p->drawLine(m_doc->impl()->width(), yStart,m_doc->impl()->width() , yEnd);
			}
			p->setPen(oldpen);
		}

		if ( unbounded ){
		    p->fillRect(
			    xpos, ypos,
			    maxWidth - xpos, QDocumentPrivate::m_lineSpacing,
			    pal.highlight()
			    );
		    if(m_doc->impl()->hardLineWrap()){
			p->setPen(Qt::lightGray);
			p->drawLine(m_doc->impl()->width(), yStart,m_doc->impl()->width() , yEnd);
		    }
		}

	}
}

QString QDocumentLineHandle::exportAsHtml(int fromOffset, int toOffset) const{
	if ( !document()->formatScheme() ) return text();
	if (toOffset == -1) toOffset = m_text.length();
	QList<RenderRange> ranges;
	splitAtFormatChanges(&ranges,0);
	QString result = "<pre>";
	foreach ( const RenderRange& r, ranges ) {
		if ( r.position + r.length < fromOffset ) continue;
		if ( r.position > toOffset ) break;

		int fmt = r.format;
		int fmts[FORMAT_MAX_COUNT];
		QFormat formats[FORMAT_MAX_COUNT];
		int newFont;
		document()->formatScheme()->extractFormats(fmt, fmts, formats, newFont);
		for (int i=2;i>=0;i--)
			if ( fmts[i] )
				result+=QString("<span class=\"fmt%1\">").arg(fmts[i]);
		//result += QString("<span class=\"fmt%1\">").arg(newFont);
		result += m_text.mid(r.position, r.length).replace("&","&amp;").replace("<","&lt;");
		//result += "</span>";
		for ( int i=0; i<FORMAT_MAX_COUNT; i++)
			if ( fmts[i])
				result+=QString("</span>");
	}
	return result+" </pre>";
}

//////////////////


/////////////////////////
//	QDocumentCursorHandle
/////////////////////////
QDocumentCursorHandle::QDocumentCursorHandle(QDocument *d, int line)
 :	m_flags(ColumnMemory), m_doc(d),
	#if QT_VERSION >= 0x040400
	m_ref(0),
	#endif
	m_begOffset(0), m_endOffset(0), m_max(0), m_begLine(line), m_endLine(-1)
{
	#if QT_VERSION < 0x040400
	m_ref.init(0);
	#endif

	//m_blocks.push(0);
	//qDebug("Cursor handle created : 0x%x", this);
}

QDocumentCursorHandle::~QDocumentCursorHandle()
{
	//qDebug("Cursor handle deleted : 0x%x", this);
	Q_ASSERT(!m_ref);

	if (isAutoUpdated())
		setAutoUpdated(false);
}

void QDocumentCursorHandle::copy(const QDocumentCursorHandle *c)
{
	if ( !c )
		return;

	if ( isAutoUpdated() )
		setAutoUpdated(false);

	m_begLine = c->m_begLine;
	m_begOffset = c->m_begOffset;
	m_endLine = c->m_endLine;
	m_endOffset = c->m_endOffset;
	m_flags = c->m_flags & ~AutoUpdated; //copy isn't automatically autoupdated
	m_max = c->m_max;
}

QDocument* QDocumentCursorHandle::document() const
{
	return m_doc;
}

QDocumentCursorHandle* QDocumentCursorHandle::clone(bool cloneAutoUpdatedFlag) const
{
	QDocumentCursorHandle *c = new QDocumentCursorHandle(m_doc);
	c->copy(this);

	if (cloneAutoUpdatedFlag)
		c->setAutoUpdated(isAutoUpdated());

	return c;
}

bool QDocumentCursorHandle::atEnd() const
{
	if ( !m_doc )
		return true;

	bool atLineEnd;
	QDocumentLine l = m_doc->line(m_begLine);

	//qDebug("Cursor handle : 0x%x->atEnd() => 0x%x", this, m_begLine.handle());

	if ( l.isValid() )
	{
		atLineEnd = m_begOffset == l.length();
		l = m_doc->line(m_begLine + 1);
	} else {
		//qWarning("Invalid cursor...");
		return true;
	}

	return l.isNull() && atLineEnd;
}

bool QDocumentCursorHandle::atStart() const
{
	if ( !m_doc )
		return true;

	QDocumentLine l = m_doc->line(m_begLine - 1);

	return l.isNull() && !m_begOffset;
}

bool QDocumentCursorHandle::atBlockEnd() const
{
	return atLineEnd();
}

bool QDocumentCursorHandle::atBlockStart() const
{
	return atLineStart();
}

bool QDocumentCursorHandle::atLineEnd() const
{
	if ( !m_doc )
		return true;

	QDocumentLine l = m_doc->line(m_begLine);

	return l.isValid() ? l.length() == m_begOffset : false;
}

bool QDocumentCursorHandle::atLineStart() const
{
	if ( !m_doc )
		return true;

	QDocumentLine l = m_doc->line(m_begLine);

	return l.isValid() ? !m_begOffset : false;
}

bool QDocumentCursorHandle::hasSelection() const
{
	if ( !m_doc )
		return false;

	QDocumentLine l1 = m_doc->line(m_begLine), l2 = m_doc->line(m_endLine);

	return l1.isValid() && l2.isValid();
}

bool QDocumentCursorHandle::isSilent() const
{
	return hasFlag(Silent);
}

void QDocumentCursorHandle::setSilent(bool y)
{
	if ( y )
		setFlag(Silent);
	else
		clearFlag(Silent);
}

bool QDocumentCursorHandle::isAutoUpdated() const
{
	return hasFlag(AutoUpdated);
}

void QDocumentCursorHandle::setAutoUpdated(bool y)
{
	if ( isAutoUpdated() == y || !m_doc )
		return;
	if ( y ) m_doc->impl()->addAutoUpdatedCursor(this);
	else m_doc->impl()->removeAutoUpdatedCursor(this);
}

bool QDocumentCursorHandle::isAutoErasable() const{
	return hasFlag(AutoErasable);
}
void QDocumentCursorHandle::setAutoErasable(bool y){
	if (y) setFlag(AutoErasable);
	else clearFlag(AutoErasable);
}


QDocumentLine QDocumentCursorHandle::line() const
{
	if ( !m_doc )
		return QDocumentLine();

	return m_doc->line(m_begLine);
}

QDocumentLine QDocumentCursorHandle::anchorLine() const
{
	if ( !m_doc )
		return QDocumentLine();

	return m_endLine != -1 ? m_doc->line(m_endLine) : line();
}

int QDocumentCursorHandle::lineNumber() const
{
	return m_begLine;
}

int QDocumentCursorHandle::anchorLineNumber() const
{
	return m_endLine != -1 ? m_endLine : m_begLine;
}

int QDocumentCursorHandle::anchorColumnNumber() const
{
	if ( !m_doc )
		return -1;

	return m_doc->line(m_endLine).isValid() ? m_endOffset : m_begOffset;
}

int QDocumentCursorHandle::visualColumnNumber() const
{
	return QDocument::screenColumn(
						line().text().constData(),
						m_begOffset,
						QDocument::tabStop()
					);

}

int QDocumentCursorHandle::columnNumber() const
{
	return m_begOffset;
}

void QDocumentCursorHandle::setLineNumber(int l, int m)
{
	if ( !m_doc )
		return;

	QDocumentLine /*l1 = m_doc->line(m_begLine), */l2 = m_doc->line(m_endLine);
	QDocumentLine ln = m_doc->line(l);

	if (ln.isNull()) 
		return;
	
	if ( m & QDocumentCursor::KeepAnchor )
	{
		if ( l2.isNull() )
		{
			m_endLine = m_begLine;
			m_endOffset = m_begOffset;
		}

		m_begLine = l; //qBound(0, c, l1.length());
	} else {
		m_endLine = -1;
		m_endOffset = 0;
		m_begLine = l;
	}

	refreshColumnMemory();
}


void QDocumentCursorHandle::setColumnNumber(int c, int m)
{
	if ( !m_doc )
		return;

	QDocumentLine /*l1 = m_doc->line(m_begLine), */l2 = m_doc->line(m_endLine);

	if ( m & QDocumentCursor::KeepAnchor )
	{
		if ( l2.isNull() )
		{
			m_endLine = m_begLine;
			m_endOffset = m_begOffset;
		}

		m_begOffset = c; //qBound(0, c, l1.length());
	} else {
		m_endLine = -1;
		m_endOffset = 0;
		m_begOffset = c; //qBound(0, c, l1.length());
	}

	refreshColumnMemory();
}

void QDocumentCursorHandle::setAnchorLineNumber(int l){
	if ( !m_doc )
		return;

	if ( m_doc->line(l).isNull() )
		return;

	m_endLine = l;
}

void QDocumentCursorHandle::setAnchorColumnNumber(int c){
	m_endOffset = c;
}


QPoint QDocumentCursorHandle::documentPosition() const
{
	if ( !m_doc )
		return QPoint();

	return QPoint(0, m_doc->y(m_begLine)) + m_doc->line(m_begLine).cursorToDocumentOffset(m_begOffset);
}

QPoint QDocumentCursorHandle::anchorDocumentPosition() const
{
	if ( !m_doc )
		return QPoint();

	if ( m_endLine < 0 || m_endOffset < 0 )
		return documentPosition();

	return QPoint(0, m_doc->y(m_endLine)) + m_doc->line(m_endLine).cursorToDocumentOffset(m_endOffset);
}

QPolygon QDocumentCursorHandle::documentRegion() const
{
	QPolygon poly;
	QPoint p = documentPosition(), ap = anchorDocumentPosition();

	int w = m_doc->width();
	const int lm = m_doc->impl()->m_leftMargin;
	const int ls = m_doc->impl()->m_lineSpacing - 1;

	if ( p == ap )
	{
		poly
			<< p
			<< QPoint(p.x() + 1, p.y())
			<< QPoint(p.x() + 1, p.y() + ls)
			<< QPoint(p.x(), p.y() + ls);
	} else if ( p.y() == ap.y() ) {
		poly
			<< p
			<< ap
			<< QPoint(ap.x(), ap.y() + ls)
			<< QPoint(p.x(), p.y() + ls);
	} else if ( p.y() < ap.y() ) {
		poly
			<< p
			<< QPoint(w, p.y());

		if ( ap.x() < w )
			poly << QPoint(w, ap.y()) << ap;

		poly
			<< QPoint(ap.x(), ap.y() + ls)
			<< QPoint(lm, ap.y() + ls)
			<< QPoint(lm, p.y() + ls);

		if ( p.x() > lm )
			poly << QPoint(p.x(), p.y() + ls);
	} else {
		poly
			<< ap
			<< QPoint(w, ap.y());

		if ( p.x() < w )
			poly << QPoint(w, p.y()) << p;

		poly
			<< QPoint(p.x(), p.y() + ls)
			<< QPoint(lm, p.y() + ls)
			<< QPoint(lm, ap.y() + ls);

		if ( ap.x() > lm )
			poly << QPoint(ap.x(), ap.y() + ls);
	}

	return poly;
}

int QDocumentCursorHandle::position() const
{
	if ( !m_doc )
		return -1;

	int pos = m_doc->line(m_begLine).position();

	return (pos != -1) ? pos + m_begOffset : pos;
}

void QDocumentCursorHandle::shift(int offset)
{
	if ( !m_doc )
		return;

	QDocumentLine l1 = m_doc->line(m_begLine), l2 = m_doc->line(m_endLine);

	if ( l1.isValid() )
		m_begOffset = qBound(0, m_begOffset + offset, l1.length());

	if ( l2.isValid() )
		m_endOffset = qBound(0, m_endOffset + offset, l2.length());
}

void QDocumentCursorHandle::refreshColumnMemory()
{
	//m_max = m_doc->line(line).cursorToX(offset);
	m_max = hasFlag(ColumnMemory) ? m_doc->line(m_begLine).cursorToDocumentOffset(m_begOffset).x() : 0;
}

bool QDocumentCursorHandle::hasColumnMemory() const
{
	return hasFlag(ColumnMemory);
}

void QDocumentCursorHandle::setColumnMemory(bool y)
{
	if ( y )
		setFlag(ColumnMemory);
	else
		clearFlag(ColumnMemory);
}

void QDocumentCursorHandle::setPosition(int pos, int m)
{
	Q_UNUSED(pos)
	Q_UNUSED(m)

	qWarning("Set position to cursor using character index : forbidden...");
	/*
	if ( m == QDocumentCursor::MoveAnchor )
	{
		m_begLine = m_doc->findLine(pos);
		m_begOffset = (m_begLine.isValid() ? pos : 0);

		m_endLine = QDocumentLine();
		m_endOffset = 0;

		m_max = m_begLine.cursorToX(m_begOffset);
	} else {
		m_endLine = m_doc->findLine(pos);
		m_endOffset = (m_begLine.isValid() ? pos : 0);

		//m_max = 0;
	}
	*/
}

bool QDocumentCursorHandle::movePosition(int count, int op, int m)
{
	if ( !m_doc )
		return false;

	QDocumentLine l, l1 = m_doc->line(m_begLine), l2 = m_doc->line(m_endLine);

	int &line = m_begLine;
	int &offset = m_begOffset;
	static QRegExp wordStart("\\b\\w+$"), wordEnd("^\\w+\\b");

	if ( !(m & QDocumentCursor::KeepAnchor) )
	{
		m_endLine = -1;
		m_endOffset = 0;
	} else if ( !l2.isValid() ) {
		m_endLine = m_begLine;
		m_endOffset = m_begOffset;
	}

	int beg = 0, end = m_doc->lines();

	switch ( op )
	{
		case QDocumentCursor::Left :
		{
			if ( atStart() )
				return false;

			int remaining = offset;

			do
			{
				if ( remaining >= count )
				{
					offset = remaining - count;
					break;
				} else if ( line == beg ) {
					offset = 0;
					break;
				}

				do
				{
					--line;
				} while ( (line > beg) && m_doc->line(line).hasFlag(QDocumentLine::Hidden) && !(m & QDocumentCursor::ThroughFolding));

				//*line = *it;

				count -= remaining + 1; // jumping a line is one char
				offset = remaining = m_doc->line(line).length();
			} while ( count && remaining );

			refreshColumnMemory();

			break;
		}

		case QDocumentCursor::Right :
		{
			if ( atEnd() )
				return false;

			int remaining = m_doc->line(line).length() - offset;

			do
			{
				if ( remaining >= count )
				{
					offset += count;
					break;
				} else if ( (line + 1) == end ) {
					offset = remaining;
					break;
				}

				do
				{
					++line;
				} while ( ((line+1) < end) && m_doc->line(line).hasFlag(QDocumentLine::Hidden) && !(m & QDocumentCursor::ThroughFolding));

				//*line = *it;

				offset = 0;
				count -= remaining + 1;
				remaining = m_doc->line(line).length();
			} while ( count && remaining );

			refreshColumnMemory();

			break;
		}

		case QDocumentCursor::Up :
		{
			if ( atStart() )
				return false;

			//qDebug("%i, %i  : up", line, offset);

			if ( m & QDocumentCursor::ThroughWrap )
			{
				QPoint p = documentPosition();

				if ( hasColumnMemory() )
					p.rx() = qMax(p.x(), m_max);

				p.ry() -= QDocumentPrivate::m_lineSpacing * count;

				if ( p.y() >= 0 )
				{
					m_doc->cursorForDocumentPosition(p, line, offset);
				} else {
					line = 0;
					offset = 0;
				}

				return true;
			}

			while ( count && (line > beg) )
			{
				--line;

				if ( !m_doc->line(line).hasFlag(QDocumentLine::Hidden) || (m & QDocumentCursor::ThroughFolding))
					--count;

			}

			//*line = QDocumentLine(*it);
			//*offset = line->xToCursor(qMin(line->cursorToX(*offset), m_max), 0);
			l = m_doc->line(line);

			if ( count )
				offset = 0;
			else if ( m == QDocumentCursor::MoveAnchor )
				offset = l.xToCursor(
						qMax(
							l.cursorToX(
								qMin(
									offset,
									l.length()
								)
							),
							m_max
						)
					);
			else
				offset = qMin(l.length(), offset);

			break;
		}

		case QDocumentCursor::Down :
		{
			if ( atEnd() )
				return false;

			if ( m & QDocumentCursor::ThroughWrap )
			{
				QPoint p = documentPosition();

				if ( hasColumnMemory() )
					p.rx() = qMax(p.x(), m_max);

				p.ry() += QDocumentPrivate::m_lineSpacing * count;

				int oldLine = line, oldCol = offset;
				m_doc->cursorForDocumentPosition(p, line, offset);
				if ( oldLine == line && oldCol == offset )
					offset = m_doc->line(line).length();
				return true;
			}

			while ( count && ((line + 1) < end) )
			{
				++line;

				if ( !m_doc->line(line).hasFlag(QDocumentLine::Hidden) || (m & QDocumentCursor::ThroughFolding))
					--count;

			}

			//*line = QDocumentLine(*it);
			l = m_doc->line(line);

			if ( count )
				offset = l.length();
			else if ( m == QDocumentCursor::MoveAnchor )
				offset = l.xToCursor(
						qMax(
							l.cursorToX(
								qMin(
									offset,
									l.length()
								)
							),
							m_max
						)
					);
			else
				offset = qMin(l.length(), offset);

			break;
		}

		case QDocumentCursor::Start :
			if ( atStart() )
				return false;

			m_max = offset = 0;
			line = 0; //m_doc->line(0);
			break;

		case QDocumentCursor::End :
			if ( atEnd() )
				return false;

			line = end - 1; //QDocumentLine(*(m_doc->impl()->end() - 1));
			offset = m_doc->line(line).length();
			refreshColumnMemory();
			break;

		case QDocumentCursor::StartOfBlock :
			if ( atBlockStart() )
				return false;

//QMessageBox::information(0,QString::number(m_doc->line(line).cursorToDocumentOffset(offset+1).y()),"a"+QString::number(m_doc->line(line).cursorToDocumentOffset(offset-1).y()),0);
			if ( m & QDocumentCursor::ThroughWrap &&
                m_doc->line(line).cursorToDocumentOffset(offset+1).y()==m_doc->line(line).cursorToDocumentOffset(offset-1).y()) //not at w. line start
			{
                QPoint p = documentPosition();
                p.rx() = 0;

                m_doc->cursorForDocumentPosition(p, line, offset);
				m_max = 0;//w.line start, avoiding 0 bug
                return true;
			}

			m_max = offset = 0;
			break;

		case QDocumentCursor::EndOfBlock :
			if ( atBlockEnd() )
				return false;

			if ( m & QDocumentCursor::ThroughWrap &&
                m_doc->line(line).cursorToDocumentOffset(offset+2).y()==m_doc->line(line).cursorToDocumentOffset(offset+1).y())//not at w. line end
                //m_doc->line(line).cursorToDocumentOffset(offset).y()/QDocumentPrivate::m_lineSpacing+1<m_doc->line(line).lineSpan()) //not in the last
			{
				int curline=line;
			    //goto next line start
			    if (m_doc->line(line).cursorToDocumentOffset(offset+1).y()!=m_doc->line(line).cursorToDocumentOffset(offset-1).y())
                    offset++;
				QPoint p = documentPosition();
				p.rx() = -1;
				p.ry() += QDocumentPrivate::m_lineSpacing;
				//if (wlinestart) //must move down to solve problem with documentPosition() at w. line start
                  //  p.ry() += QDocumentPrivate::m_lineSpacing;

				m_doc->cursorForDocumentPosition(p, line, offset);

				//one left in the w. line before
				offset--;
				m_max = m_doc->line(line).cursorToDocumentOffset(offset).x();
				if ((curline != line)||(m_doc->height()==p.ry())) line=curline; //jumped to far, work around for wrapped last line
				else if (offset>0) return true;
			}

			offset = m_doc->line(line).length();
			refreshColumnMemory();
			break;

		case QDocumentCursor::NextBlock :

			if ( atEnd() )
				return false;

			while ( ((line + 1) < end) && count )
			{
				++line;

				if ( !m_doc->line(line).hasFlag(QDocumentLine::Hidden) || (m & QDocumentCursor::ThroughFolding))
					--count;

			}

			if ( !count )
			{
				//*line = *it;
				offset = 0;
			} else {
				//*line = QDocumentLine(*(m_doc->impl()->end() - 1));
				offset = m_doc->line(line).length();
			}

			break;

		case QDocumentCursor::PreviousBlock :

			if ( atStart() )
				return false;

			offset = 0;

			while ( (line > beg) && count )
			{
				--line;

				if ( !m_doc->line(line).hasFlag(QDocumentLine::Hidden) || (m & QDocumentCursor::ThroughFolding))
					--count;

			}

			if ( !count )
			{
				//*line = *it;
				offset = m_doc->line(line).length();
			} else {
				offset = 0;
				//*line = QDocumentLine(*(m_doc->impl()->begin()));
			}

			//*line = *it;

			break;

		case QDocumentCursor::WordLeft :
		case QDocumentCursor::PreviousWord :
		{
			if ( atStart() )
				return false;

			l = m_doc->line(line);

			//for ( int loop = 0; loop <= 1; ++loop )
			//{
			//	while ( l.isValid() )

			// -- patch --
			/* eats up white space */
			while ( (offset > 0) && !isWord(l.text().at(offset - 1)) && !isDelimiter(l.text().at(offset - 1)) )
				--offset;

			/* start of line */
			if ( offset == 0 )
			{
				/* first line, first char => nothing to do */
				if( line == beg )
					return true;

				do
			// -- patch --
				{
// 					//offset = qMin(offset, l.length() - 1);
// 					bool next = (l.length() && offset >= 0) ? isWord(l.text().at(offset)) : true;
//
// 					if ( loop )
// 						next = !next;
//
// 					if ( !next )
// 						break;
//
// 					if ( offset > 0 )
// 					{
// 						--offset;
// 					} else if ( line != beg ) {
// 						do
// 						{
// 							//*line = *(--it);
// 							--line;
// 							l = m_doc->line(line);
// 							offset = l.length() - 1;
// 						} while ( l.isValid() && (line != beg) && l.hasFlag(QDocumentLine::Hidden) );
// 					} else {
// 						break;
// 					}
// 				}
// 			}
//
// 			while ( l.isValid() )
// 			{
// 				offset = qMin(offset, l.length());
// 				bool next = (offset <= 0) ? false : isWord(l.text().at(offset - 1));
//
// 				if ( !next )
// 					break;
//
// 				--offset;

			// -- patch --
					--line;
					l = m_doc->line(line);
					offset = l.length();
				} while ( (line != beg) && l.isValid() && l.hasFlag(QDocumentLine::Hidden) && !(m & QDocumentCursor::ThroughFolding));
				return true;
			// -- patch --
			}

			// -- patch --
			/* eats up delimiters */
			bool delimiter_used=false;
			while ( (offset > 0) && isDelimiter(l.text().at(offset-1)) ){
				--offset;
				delimiter_used=true;
			}
			/* eats up whole word */
			if(!delimiter_used){
				while ( (offset > 0) && isWord(l.text().at(offset - 1)) )
					--offset;
			}
			// -- patch --

			refreshColumnMemory();

			break;
		}

		case QDocumentCursor::WordRight :
		case QDocumentCursor::NextWord :
		{
			if ( atEnd() )
				return false;

			l = m_doc->line(line);
			int lineLength = l.text().length();

// 			for ( int loop = 0; loop <= 1; ++loop )
			// -- patch --
			/* end of line */
			if ( offset == lineLength )
 			{
// 				while ( l.isValid() )
				/* last line, last char => nothing to do */
				if ( line == end )
					return true;
			// -- patch --
				do
				{
// 					//offset = qBound(0, offset, l.length() - 1);
// 					bool next = (offset < l.length()) ? isWord(l.text().at(offset)) : true;
//
// 					if ( loop )
// 						next = !next;
//
// 					if ( !next )
// 						break;
//
// 					if ( offset < l.length() )
// 					{
// 						++offset;
// 					} else if ( (line + 1) != end ) {
// 						offset = 0;
// 						do
// 						{
// 							++line;
// 							l = m_doc->line(line);
// 						} while ( l.isValid() && ((line + 1) != end) && (l.hasFlag(QDocumentLine::Hidden) || !l.length()) );
// 					} else {
			// -- patch --
					++line;
					l = m_doc->line(line);
					offset = 0;
				} while ( (line != end) && l.isValid() && l.hasFlag(QDocumentLine::Hidden) && !(m & QDocumentCursor::ThroughFolding));

				lineLength = l.text().length();
				/* empty line */
				if ( lineLength == 0 )
					return true;

				/* eats up white space */
				while ( !isWord(l.text().at(offset)) )
				{
					++offset;
					/* move to end of line */
					if ( offset == lineLength )
						break;
			// -- patch --
//					}
				}
			// -- patch --
				return true;
			// -- patch --
			}

			// -- patch --
			/* next char */
			//++offset;
			bool delimiter_used=false;
			while ( (offset < lineLength) && isDelimiter(l.text().at(offset)) ){
				++offset;
				delimiter_used=true;
			}

			/* eats up whole word */
			if(!delimiter_used){
				while ( (offset < lineLength) && isWord(l.text().at(offset)) )
					++offset;
			}

			/* eats up white space */
			while ( (offset < lineLength) && !isWord(l.text().at(offset))&&!isDelimiter(l.text().at(offset)) )
				++offset;
			// -- patch --

			refreshColumnMemory();

			break;
		}

		case QDocumentCursor::StartOfWord :
		{
			int x = wordStart.indexIn(m_doc->line(line).text().left(offset));

			if ( x != -1 )
			{
				offset = x;
			} else {
				qDebug("failed to find SOW : %i + %i != %i",
						x, wordStart.matchedLength(), offset);

				return false;
			}

			refreshColumnMemory();

			break;
		}

		case QDocumentCursor::EndOfWord :
		{
			int x = wordEnd.indexIn(m_doc->line(line).text(), offset, QRegExp::CaretAtOffset);

			if ( x == offset )
			{
				offset += wordEnd.matchedLength();
			} else {
				qDebug("failed to find EOW");
				return false;
			}

			refreshColumnMemory();

			break;
		}

		default:
			qWarning("Unhandled move operation...");
			return false;
	};

	return true;
}

void QDocumentCursorHandle::moveTo(const QDocumentCursor &c, int m)
{
	if ( !c.isValid() || !m_doc )
		return;

	if(!(m&QDocumentCursor::KeepAnchor)){
	    m_begLine = c.handle()->m_begLine;
	    m_begOffset = c.handle()->m_begOffset;

	    m_endLine = -1;
	    m_endOffset = 0;
	}else{
	    m_endLine = c.handle()->m_begLine;
	    m_endOffset = c.handle()->m_begOffset;
	}

	refreshColumnMemory();
}

void QDocumentCursorHandle::moveTo(int line, int column, int m)
{
    if(!(m&QDocumentCursor::KeepAnchor)){
	m_begLine = line;
	m_begOffset = column;

	m_endLine = -1;
	m_endOffset = 0;
    }else{
	m_endLine = line;
	m_endOffset = column;
    }

	refreshColumnMemory();
}

void QDocumentCursorHandle::insertText(const QString& s, bool keepAnchor)
{
	if ( !m_doc || s.isEmpty() || m_doc->line(m_begLine).isNull() )
		return;

	bool sel = hasSelection();

	if ( sel )
	{
		beginEditBlock();
		removeSelectedText(keepAnchor);
	}

	QDocumentCommand *command = new QDocumentInsertCommand(
										m_begLine,
										m_begOffset,
										s,
										m_doc
									);
	
	command->setKeepAnchor(keepAnchor);
	command->setTargetCursor(this);
	execute(command);

	if ( sel )
		endEditBlock();
}

void QDocumentCursorHandle::eraseLine()
{
	if ( !m_doc )
		return;

	QDocumentCommand *command = 0;

	if((m_begLine>m_doc->lineCount())||(m_begLine<0)) return; // return if cursor is out of range
	if((m_endLine>m_doc->lineCount())) m_endLine=m_doc->lineCount()-1;

	if ( m_endLine == -1 )
	{
		command = new QDocumentEraseCommand(
										m_begLine,
										0,
										m_begLine + 1,
										0,
										m_doc
									);
	} else {
		command = new QDocumentEraseCommand(
										qMin(m_begLine, m_endLine),
										0,
										qMax(m_begLine, m_endLine) + 1,
										0,
										m_doc
									);
	}
	command->setTargetCursor(this);
	execute(command);
}

QChar QDocumentCursorHandle::nextChar() const
{
	if ( !m_doc )
		return QChar();

	QDocumentLine l = m_doc->line(m_begLine);

	if ( !l.isValid() )
		return QChar();

	return m_begOffset < l.length() ? l.text().at(m_begOffset) : (atEnd()?QLatin1Char('\0'):QLatin1Char('\n'));
}

QChar QDocumentCursorHandle::previousChar() const
{
	if ( !m_doc || (m_begLine <= 0 && m_begOffset <= 0) )
		return QChar();

	QDocumentLine l = m_doc->line(m_begLine);

	if ( !l.isValid() || m_begOffset > l.length() )
		return QChar();

	return m_begOffset ? l.text().at(m_begOffset - 1) : QLatin1Char('\n');
}

void QDocumentCursorHandle::deleteChar()
{
	if ( !m_doc )
		return;

	QDocumentLine l = m_doc->line(m_begLine);

	if ( l.isNull() || atEnd() )
		return;

	QDocumentCommand *command = 0;

	if ( !atLineEnd() )
	{
		command = new QDocumentEraseCommand(
											m_begLine,
											m_begOffset,
											m_begLine,
											m_begOffset + 1,
											m_doc
										);

	} else {
		// merge two blocks...
		command = new QDocumentEraseCommand(
											m_begLine,
											m_begOffset,
											m_begLine + 1,
											0,
											m_doc
										);

	}

	command->setTargetCursor(this);
	command->setUndoOffset(-1);
	execute(command);
}

void QDocumentCursorHandle::deletePreviousChar()
{
	if ( !m_doc )
		return;

	QDocumentLine l = m_doc->line(m_begLine);

	if ( l.isNull() || atStart() )
		return;

	QDocumentCommand *command = 0;

	if ( !atLineStart() )
	{
		command = new QDocumentEraseCommand(
											m_begLine,
											m_begOffset - 1,
											m_begLine,
											m_begOffset,
											m_doc
										);

	} else {
		// merge two blocks...
		QDocumentLine prev = m_doc->line(m_begLine - 1);

		command = new QDocumentEraseCommand(
											m_begLine - 1,
											prev.length(),
											m_begLine,
											m_begOffset,
											m_doc
										);

	}

	command->setTargetCursor(this);
	execute(command);
}

void QDocumentCursorHandle::execute(QDocumentCommand *c)
{
	Q_ASSERT(m_doc);

	if ( !m_doc )
		return; //returning means c will never freed

	if ( isSilent() && !c->isSilent() )
		c->setSilent(isSilent());
	
	if ( m_blocks.count() )
	{
		c->redo();
		m_blocks.top()->addCommand(c);

	} else if ( m_doc ) {
		//qDebug("Cursor handle executing command : 0x%x", this);

		m_doc->execute(c);
	}
}

void QDocumentCursorHandle::beginEditBlock()
{
	m_blocks.push(new QDocumentCommandBlock(m_doc));
}

void QDocumentCursorHandle::endEditBlock()
{
	if ( !m_doc || m_blocks.isEmpty() )
		return;

	//qDebug("Cursor handle executing command : 0x%x [block]", this);

	QDocumentCommandBlock *block = m_blocks.pop();

	// special trick to prevent double redo() while getting rid of
	// bugs occuring in when inserting/erasing in overlapping lines
	// inside a command block
	block->setWeakLock(true);

	m_doc->execute(block);
}

QDocumentCursor QDocumentCursorHandle::selectionStart() const
{
	if ( !m_doc )
		return QDocumentCursor();

	if ( !hasSelection() )
		return QDocumentCursor(clone(false));

	QDocumentCursor pos(m_doc, m_begLine, m_begOffset),
					anc(m_doc, m_endLine, m_endOffset);

	return (pos < anc) ? pos : anc;
}

QDocumentCursor QDocumentCursorHandle::selectionEnd() const
{
	if ( !m_doc )
		return QDocumentCursor();

	if ( !hasSelection() )
		return QDocumentCursor(clone(false));

	QDocumentCursor pos(m_doc, m_begLine, m_begOffset),
					anc(m_doc, m_endLine, m_endOffset);

	return (pos > anc) ? pos : anc;
}

bool QDocumentCursorHandle::eq(const QDocumentCursorHandle *h)
{
	return (m_begLine == h->m_begLine) && (m_begOffset == h->m_begOffset);
	/*
	if ( !hasSelection() )
		return (m_begLine == h->m_begLine) && (m_begOffset == h->m_begOffset);

	return
			(m_begLine == h->m_begLine)
		&&
			(m_begOffset == h->m_begOffset)
		&&
			(m_endLine == h->m_endLine)
		&&
			(m_endOffset == h->m_endOffset)
		;
	*/
}

bool QDocumentCursorHandle::lt(const QDocumentCursorHandle *h)
{
	return
				(m_begLine < h->m_begLine)
			||
				((m_begLine == h->m_begLine) && (m_begOffset < h->m_begOffset))
			;
}

bool QDocumentCursorHandle::gt(const QDocumentCursorHandle *h)
{
	return
				(m_begLine > h->m_begLine)
			||
				((m_begLine == h->m_begLine) && (m_begOffset > h->m_begOffset))
			;
}

QString QDocumentCursorHandle::selectedText() const
{
	if ( !m_doc )
		return QString();

	QDocumentLine l1 = m_doc->line(m_begLine), l2 = m_doc->line(m_endLine);

	if ( l1.isNull() || l2.isNull() )
		return QString();

	QString s;

	if ( m_begLine == m_endLine )
	{
		int min = qMin(m_begOffset, m_endOffset),
			max = qMax(m_begOffset, m_endOffset);

		s = l1.text().mid(min, max - min);
	} else if ( m_begLine < m_endLine ) {
		s = l1.text().mid(m_begOffset);

		int it = m_begLine;
		//QDocumentConstIterator it = m_doc->impl()->index(m_begLine.handle());

		while ( ++it < m_endLine )
		{
			s += '\n';
			s += m_doc->line(it).text();
		}

		s += '\n';
		s += l2.text().left(m_endOffset);
	} else {
		s = l2.text().mid(m_endOffset);

		int it = m_endLine;
		//QDocumentConstIterator it = m_doc->impl()->index(m_endLine.handle());

		while ( ++it < m_begLine )
		{
			s += '\n';
			s += m_doc->line(it).text();
		}

		s += '\n';
		s += l1.text().left(m_begOffset);
	}

	return s;
}

void QDocumentCursorHandle::clearSelection()
{
	if ( m_doc && m_doc->line(m_endLine).isValid() )
	{
		//m_begLine = m_endLine;
		//m_begOffset = m_endOffset;

		m_endLine = -1;
		m_endOffset = -1;
	}
}

void QDocumentCursorHandle::replaceSelectedText(const QString& text)
{
	int begline, begcol;
	beginBoundary(begline, begcol);
	
	bool atStart = (begline == m_begLine && begcol == m_begOffset);
	
	if ( text.isEmpty() )
	{
		removeSelectedText();
	} else {
		insertText(text, true);
		
		/*
			Adjust selection around the new text 
		*/
		if ( atStart )
		{
			m_endLine = m_begLine;
			m_begLine = begline;
			m_endOffset = m_begOffset;
			m_begOffset = begcol;
		} else {
			m_endLine = begline;
			m_endOffset = begcol;
		}
	}
	
	//qDebug("[%i, %i] => ( (%i, %i), (%i, %i) )", begline, begcol, m_begLine, m_begOffset, m_endLine, m_endOffset);
}

void QDocumentCursorHandle::select(QDocumentCursor::SelectionType t)
{
	if ( !m_doc || !m_doc->line(m_begLine).isValid() )
		return;

	if ( t == QDocumentCursor::LineUnderCursor )
	{
		movePosition(1, QDocumentCursor::StartOfLine, QDocumentCursor::MoveAnchor);
		movePosition(1, QDocumentCursor::EndOfLine, QDocumentCursor::KeepAnchor);

	} else if ( t == QDocumentCursor::WordUnderCursor ) {

		movePosition(1, QDocumentCursor::StartOfWord, QDocumentCursor::MoveAnchor);
		movePosition(1, QDocumentCursor::EndOfWord, QDocumentCursor::KeepAnchor);
	}
}

void QDocumentCursorHandle::setSelectionBoundary(const QDocumentCursor& c)
{
	if (
			!m_doc
		||
			(m_begLine == -1)
		||
			(
					(c.lineNumber() == m_begLine)
				&&
					(c.columnNumber() == m_begOffset)
			)
		)
		return;

	//qDebug("setting new selection boundary... ");

	if ( !hasSelection() )
	{
		m_endLine = m_begLine;
		m_endOffset = m_begOffset;
	}

	m_begLine = c.lineNumber();
	m_begOffset = c.columnNumber();
}

bool QDocumentCursorHandle::isWithinSelection(const QDocumentCursor& c) const
{
	if ( !hasSelection() || c.isNull() ) //|| c.hasSelection() )
		return false;

	int minOff, maxOff, min, max;

	if ( m_begLine > m_endLine )
	{
		max = m_begLine;
		maxOff = m_begOffset;

		min = m_endLine;
		minOff = m_endOffset;
	} else {
		min = m_begLine;
		minOff = m_begOffset;

		max = m_endLine;
		maxOff = m_endOffset;
	}

	return (m_begLine == m_endLine)
		?
			(
				(c.lineNumber() == m_begLine)
			&&
				(qMin(m_begOffset, m_endOffset) <= c.columnNumber())
			&&
				(qMax(m_begOffset, m_endOffset) >= c.columnNumber())
			)
		:
			(
				(
					(c.lineNumber() > min)
				&&
					(c.lineNumber() < max)
				)
			||
				(
					(c.lineNumber() == min)
				&&
					(minOff <= c.columnNumber())
				)
			||
				(
					(c.lineNumber() == max)
				&&
					(maxOff >= c.columnNumber())
				)
			)
		;

}

/*
	beware when modifying these as their current form handle the special
	case of no selection (m_endLine == -1) and a hasty change may break
	that behavior : no selection -> both boundary are the cursor pos = (m_begLine, m_begOffset)
*/
void QDocumentCursorHandle::beginBoundary(int& begline, int& begcol) const
{
	if ( m_begLine == m_endLine ) {
		begline = m_begLine;
		if ( m_begOffset < m_endOffset /*&& (m_begLine!=-1)*/)
			begcol = m_begOffset;
		else
			begcol = m_endOffset;
	} else if ( m_begLine < m_endLine || m_endLine==-1) {
		begline = m_begLine;
		begcol = m_begOffset;
	} else {
		begline = m_endLine;
		begcol = m_endOffset;
	}
}

void QDocumentCursorHandle::endBoundary(int& endline, int& endcol) const
{
	if ( m_begLine == m_endLine )
	{
		endline = m_endLine;
		if ( m_begOffset < m_endOffset /*&& (m_endOffset!=-1)*/)
			endcol = m_endOffset;
		else
			endcol = m_begOffset;
	} else if ( m_begLine < m_endLine) {
		endline = m_endLine;
		endcol = m_endOffset;
	} else {
		endline = m_begLine;
		endcol = m_begOffset;
	}
}

void QDocumentCursorHandle::boundaries(int& begline, int& begcol, int& endline, int& endcol) const
{
	beginBoundary(begline, begcol);
	endBoundary(endline, endcol);
	
	/*
	if ( m_begLine == m_endLine )
	{
		endline = m_endLine;
		if ( m_begOffset < m_endOffset //&& (m_endOffset!=-1)\\)
			endcol = m_endOffset;
		else
			endcol = m_begOffset;
	} else if ( m_begLine < m_endLine) {
		endline = m_endLine;
		endcol = m_endOffset;
	} else {
		endline = m_begLine;
		endcol = m_begOffset;
	}
	*/
}

void QDocumentCursorHandle::substractBoundaries(int lbeg, int cbeg, int lend, int cend)
{
	int tlmin, tlmax, tcmin, tcmax;

	boundaries(tlmin, tcmin, tlmax, tcmax);

	bool begFirst = tlmin == m_begLine && tcmin == m_begOffset;

	if ( tlmax < lbeg || tlmin > lend || (tlmax == lbeg && tcmax < cbeg) || (tlmin == lend && tcmin > cend) )
	{
		// no intersection
		return;
	}

	//references so we don't have to difference between the cases
	int &rtlbeg = begFirst?m_begLine:m_endLine;
	int &rtcbeg = begFirst?m_begOffset:m_endOffset;
	int &rtlend = begFirst?m_endLine:m_begLine;
	int &rtcend = begFirst?m_endOffset:m_begOffset;
	
	int numLines = lend - lbeg;
	bool beyondBeg = (tlmin > lbeg || (tlmin == lbeg && tcmin >= cbeg));
	bool beyondEnd = (tlmax < lend || (tlmax == lend && tcmax <= cend));

	if ( beyondBeg && beyondEnd )
	{
		//qDebug("(%i, %i : %i, %i) erased as in (%i, %i : %i, %i)", tlmin, tcmin, tlmax, tcmax, lbeg, cbeg, lend, cend);
		// cursor erased...
		m_begLine = m_endLine = lbeg;
		m_begOffset = m_endOffset = cbeg;
	} else if ( beyondEnd ) {
		//qDebug("beyond end");
		rtlend = lbeg;
		rtcend = cbeg;
	} else if ( beyondBeg ) {
		//qDebug("beyond beg");
		
		rtlbeg = lbeg;  //selection is moved upwards
		rtcbeg = cbeg;//to the begin of the removed part
		rtlend -= numLines; //end line moved upwards by the count of removed lines
		if ( tlmax == lend)
			rtcend = rtcend - cend + cbeg; //end column moved forward by the count of removed characters
	} else {
		rtlend -= numLines;
		if ( tlmax == lend )
			rtcend -= cend - cbeg;
	}

	//qDebug("(%i, %i : %i, %i) corrected to (%i, %i : %i, %i) after subtracting of (%i, %i : %i, %i)", tlmin, tcmin, tlmax, tcmax, m_begLine, m_begOffset, m_endLine, m_endOffset, lbeg, cbeg, lend, cend);
}

/*!
  Sets the selection given by lbeg/cbeg/lend/cend to the largest selection which is contained in
  the selection of this cursor and the passed boundaries
  (it sets all to -1 if such a selection doesn't exists)
*/
void QDocumentCursorHandle::intersectBoundaries(int& lbeg, int& cbeg, int& lend, int& cend) const
{
	int tlmin, tlmax, tcmin, tcmax, clmin, clmax, ccmin, ccmax;

	boundaries(tlmin, tcmin, tlmax, tcmax);
	clmin = lbeg;
	clmax = lend;
	ccmin = cbeg;
	ccmax = cend;

	if ( tlmax < clmin || tlmin > clmax || (tlmax == clmin && tcmax < ccmin) || (tlmin == clmax && tcmin > ccmax) )
	{
		lbeg = cbeg = lend = cend = -1;
		return;
	}

	if ( tlmin == clmin )
	{
		lbeg = tlmin;
		cbeg = qMax(tcmin, ccmin);
	} else if ( tlmin < clmin ) {
		lbeg = clmin;
		cbeg = ccmin;
	} else {
		lbeg = tlmin;
		cbeg = tcmin;
	}

	if ( tlmax == clmax )
	{
		lend = tlmax;
		cend = qMin(tcmax, ccmax);
	} else if ( tlmax < clmax ) {
		lend = tlmax;
		cend = tcmax;
	} else {
		lend = clmax;
		cend = ccmax;
	}
}

void QDocumentCursorHandle::intersectBoundaries(QDocumentCursorHandle *h, int& lbeg, int& cbeg, int& lend, int& cend) const
{
	int tlmin, tlmax, tcmin, tcmax, clmin, clmax, ccmin, ccmax;

	boundaries(tlmin, tcmin, tlmax, tcmax);
	h->boundaries(clmin, ccmin, clmax, ccmax);

	if ( tlmax < clmin || tlmin > clmax || (tlmax == clmin && tcmax < ccmin) || (tlmin == clmax && tcmin > ccmax) )
	{
		lbeg = cbeg = lend = cend = -1;
		return;
	}

	if ( tlmin == clmin )
	{
		lbeg = tlmin;
		cbeg = qMax(tcmin, ccmin);
	} else if ( tlmin < clmin ) {
		lbeg = clmin;
		cbeg = ccmin;
	} else {
		lbeg = tlmin;
		cbeg = tcmin;
	}

	if ( tlmax == clmax )
	{
		lend = tlmax;
		cend = qMin(tcmax, ccmax);
	} else if ( tlmax < clmax ) {
		lend = tlmax;
		cend = tcmax;
	} else {
		lend = clmax;
		cend = ccmax;
	}
}

/*!
  Creates a new cursor whose selection is the largest region which is contained in the selection of both
  (returns c if c has no selection but is within the selection of c and returns an invalid cursor if
  this has no selection)
*/
QDocumentCursor QDocumentCursorHandle::intersect(const QDocumentCursor& c) const
{
	if ( !hasSelection() )
	{
		//if ( c.hasSelection() && c.isWithinSelection(QDocumentCursor(this)) )
		//	return QDocumentCursor(clone());

	} else if ( !c.hasSelection() ) {

		if ( isWithinSelection(c) )
			return c;

	} else {
		QDocumentCursorHandle *h = c.handle();

		int lbeg, lend, cbeg, cend;
		intersectBoundaries(h, lbeg, cbeg, lend, cend);

		if ( lbeg != -1 )
		{
			QDocumentCursor c(m_doc, lbeg, cbeg);

			if ( lend != -1 && (lbeg != lend || cbeg != cend) )
			{
				c.setSelectionBoundary(QDocumentCursor(m_doc, lend, cend));
			}

			return c;
		}
	}

	return QDocumentCursor();
}

void QDocumentCursorHandle::removeSelectedText(bool keepAnchor)
{
	if ( !m_doc )
		return;

	QDocumentLine l1 = m_doc->line(m_begLine), l2 = m_doc->line(m_endLine);

	if ( l1.isNull() || l2.isNull() )
		return;

	QDocumentCommand *c;

	if ( m_begLine < m_endLine )
	{
		c = new QDocumentEraseCommand(
										m_begLine,
										m_begOffset,
										m_endLine,
										m_endOffset,
										m_doc
									);
		
	} else if ( m_begLine > m_endLine ) {
		c = new QDocumentEraseCommand(
										m_endLine,
										m_endOffset,
										m_begLine,
										m_begOffset,
										m_doc
									);
		
		//m_begLine = m_endLine;
		//m_begOffset = m_endOffset;
		
	} else {
		c = new QDocumentEraseCommand(
										m_begLine,
										qMin(m_begOffset, m_endOffset),
										m_endLine,
										qMax(m_begOffset, m_endOffset),
										m_doc
									);
		
		//m_begOffset = qMin(m_begOffset, m_endOffset);
		//m_endLine = -1;
		//m_endOffset = -1;
	}

	c->setKeepAnchor(keepAnchor);
	c->setTargetCursor(this);
	execute(c);
}

//////////////////

/////////////////////////
//	QDocumentPrivate
/////////////////////////

template <class T> T* getStaticDefault() { static T _globStatInst; return &_globStatInst; }

QTextCodec* QDocumentPrivate::m_defaultCodec = 0;

QFont* QDocumentPrivate::m_font = 0;// = QApplication::font();
QFormatScheme* QDocumentPrivate::m_formatScheme = 0;// = QApplication::font();
QMap<int,WCache*> QDocumentPrivate::m_fmtWidthCache;
QVector<QFont> QDocumentPrivate::m_fonts;
QList<QFontMetrics> QDocumentPrivate::m_fontMetrics;

int QDocumentPrivate::m_defaultTabStop = 4;
QFormatScheme* QDocumentPrivate::m_defaultFormatScheme = getStaticDefault<QFormatScheme>();

QList<QDocumentPrivate*> QDocumentPrivate::m_documents;

bool QDocumentPrivate::m_fixedPitch;
QDocument::WorkAroundMode QDocumentPrivate::m_workArounds=0;
int QDocumentPrivate::m_ascent;// = m_fontMetrics.ascent();
int QDocumentPrivate::m_descent;// = m_fontMetrics.descent();
int QDocumentPrivate::m_leading;// = m_fontMetrics.leading();
int QDocumentPrivate::m_spaceWidth;// = m_fontMetrics.width(' ');
int QDocumentPrivate::m_lineHeight;// = m_fontMetrics.height();
int QDocumentPrivate::m_lineSpacing;// = m_fontMetrics.lineSpacing();

int QDocumentPrivate::m_leftMargin = 5;
QDocument::WhiteSpaceMode QDocumentPrivate::m_showSpaces = QDocument::ShowNone;
QDocument::LineEnding QDocumentPrivate::m_defaultLineEnding = QDocument::Conservative;

int QDocumentPrivate::m_wrapMargin = 15;

QDocumentPrivate::QDocumentPrivate(QDocument *d)
 : 	m_doc(d),
	m_editCursor(0),
	m_lastGroupId(-1),
	m_constrained(false),
	m_hardLineWrap(false),
	m_width(0),
	m_height(0),
	m_tabStop(m_defaultTabStop),
	m_language(0),
	m_maxMarksPerLine(0),
	_nix(0),
	_dos(0),
	_mac(0),
	m_lineEnding(m_defaultLineEnding),
	m_codec(m_defaultCodec),
	m_oldLineCacheOffset(0), m_oldLineCacheWidth(0)
{
	m_documents << this;
}

QDocumentPrivate::~QDocumentPrivate()
{
	m_marks.clear();
	m_largest.clear();

	m_deleting = true;

	//qDeleteAll(m_lines);
	foreach ( QDocumentLineHandle *h, m_lines )
		h->deref();

	discardAutoUpdatedCursors(true);

	m_lines.clear();

	m_deleting = false;

	m_commands.clear();

	m_documents.removeAll(this);
}

int QDocumentPrivate::findNextMark(int id, int from, int until)
{
	if ( from < 0 ) {
		from += m_lines.count();
		if (from < 0) from=m_lines.count()-1;
	} else if (from >= m_lines.count())
        from=m_lines.count()-1;

	QHash<QDocumentLineHandle*, QList<int> >::const_iterator e = m_marks.constEnd();

	int max = until;

	if ( max < 0 )
		max += m_lines.count();
	else if ( max < from )
		max = m_lines.count() - 1;

	for ( int i = from; i <= max; ++i )
	{
		QDocumentLineHandle *h = m_lines.at(i);

		QHash<QDocumentLineHandle*, QList<int> >::const_iterator it = m_marks.constFind(h);

		if ( it != e && !it->isEmpty() && (id==-1 || it->contains(id))  )
			return i;

	}

	if ( until > 0 && until < from )
	{
		for ( int i = 0; i <= until; ++i )
		{
			QDocumentLineHandle *h = m_lines.at(i);

			QHash<QDocumentLineHandle*, QList<int> >::const_iterator it = m_marks.constFind(h);

			if ( it != e && !it->isEmpty() && (id==-1 || it->contains(id))  )
				return i;

		}
	}

	return -1;
}

int QDocumentPrivate::findPreviousMark(int id, int from, int until)
{
	if ( from < 0 ) {
		from += m_lines.count();
		if (from < 0) from = m_lines.count() - 1;
	} else if (from >= m_lines.count())
        from = m_lines.count()-1;

	if ( until < 0 )
	{
		until += m_lines.count();
	} else if ( until >= m_lines.count() ) {
		until = m_lines.count() - 1;
	}

	QHash<QDocumentLineHandle*, QList<int> >::const_iterator e = m_marks.constEnd();

	int min = until;

	if ( min > from )
		min = 0;

	for ( int i = from; i >= min; --i )
	{
		QDocumentLineHandle *h = m_lines.at(i);

		QHash<QDocumentLineHandle*, QList<int> >::const_iterator it = m_marks.constFind(h);

		if ( it != e && !it->isEmpty() && (id==-1 || it->contains(id)))
			return i;

	}

	if ( until > 0 && until > from )
	{
		for ( int i = m_lines.count() - 1; i >= until; --i )
		{
			QDocumentLineHandle *h = m_lines.at(i);

			QHash<QDocumentLineHandle*, QList<int> >::const_iterator it = m_marks.constFind(h);

			if ( it != e && !it->isEmpty() && (id==-1 || it->contains(id)))
				return i;

		}
	}

	return -1;
}

void QDocumentPrivate::removeMarks(int id){
    QList<QDocumentLineHandle*> changed;

    QHash<QDocumentLineHandle*, QList<int> >::iterator
        it = m_marks.begin(),
        end = m_marks.end();
    //change all silently
    m_maxMarksPerLine = 0;
    while (it!=end) {
        int n = it->removeAll(id);
        if (n) changed << it.key();
        if ( it->isEmpty() ) it=m_marks.erase(it);
        else {
            if (it->count() > m_maxMarksPerLine) m_maxMarksPerLine = it->count();
            ++it;
        }
    }

	//then notify
	for (int i=0; i<changed.size();i++){
        emitMarkChanged(changed[i], id, false);
		changed[i]->setFlag(QDocumentLine::LayoutDirty,true);
	}
}

void QDocumentPrivate::execute(QDocumentCommand *cmd)
{
	if ( !cmd )
		return;
	
	m_lastModified = QDateTime::currentDateTime();
	
	if ( m_macros.count() )
	{
		cmd->redo();
		m_macros.top()->addCommand(cmd);
	} else {
	m_commands.push(cmd);
}
}

void QDocumentPrivate::draw(QPainter *p, QDocument::PaintContext& cxt)
{
	//QTime t;
	//t.start();
	QDocumentLineHandle *h;
	bool inSel = false, fullSel;
	QList<QDocumentCursorHandle*>::iterator cit;
	int i, realln, pos = 0, xOffset,
		firstLine = qMax(0, cxt.yoffset / m_lineSpacing),
		lastLine = qMax(0, firstLine + (cxt.height / m_lineSpacing));

	if ( cxt.height % m_lineSpacing )
		++lastLine;

	p->setFont(*m_font);

	bool currentLine=false;

	int lineCacheWidth = m_oldLineCacheWidth;
	if(m_oldLineCacheOffset!=cxt.xoffset || m_oldLineCacheWidth < cxt.width) {
		m_LineCache.clear();
		if (m_width) lineCacheWidth = cxt.width;
		else lineCacheWidth = (cxt.width+15) & (~16);         //a little bit larger if not wrapped
		//qDebug("clear");
	}


	QBrush bg,
		base = cxt.palette.base(),
		selbg = cxt.palette.highlight(),
		//alternate = QLineMarksInfoCenter::instance()->markType("current").color;
		alternate = QDocument::formatFactory()->format("current").toTextCharFormat().background(); //current line


	QColor repBackground = m_doc->getBackground();
	if ( repBackground.isValid() )
		base.setColor(repBackground);


	if ( !alternate.color().isValid() )
		alternate = cxt.palette.alternateBase();

	QVector<int> m_cursorLines(0), m_selectionBoundaries(0);

	int wrap = 0;
	i = textLine(firstLine, &wrap);
	firstLine -= wrap;
	realln = firstLine;

	//qDebug("lines [%i, %i]", firstLine, lastLine);

	pos += firstLine * m_lineSpacing;

	// adjust first line to take selections into account...
	foreach ( const QDocumentSelection& s, cxt.selections )
	{
		if ( (s.startLine < i) && (s.endLine >= i) )
		{
			inSel = true;
			break;
		}
	}

	for ( ; realln <= lastLine; ++i )
	{
		if ( i >= m_lines.count() )
		{
			//qDebug("line %i not valid", i);
			break;
		}

		h = m_lines.at(i);

		// ugly workaround..., disabled 20.12.'09 because it slows down rendering speed on mac considerably and i don't see its function
		//if( !m_fixedPitch && !h->hasFlag(QDocumentLine::Hidden))
		//	adjustWidth(i);

		const int wrap = h->m_frontiers.count();
		const bool wrapped = wrap;

		//if ( wrapped )
		//	qDebug("line %i is wrapped over %i sublines", i, *wit);

		// selections stuff (must do it before whatever the visibility...)
		m_selectionBoundaries.clear();

		fullSel = false;

		if ( inSel )
			m_selectionBoundaries.prepend(0);

		foreach ( const QDocumentSelection& s, cxt.selections )
		{
			if ( i == s.startLine )
			{
				if ( !(m_selectionBoundaries.count() & 1) )
					m_selectionBoundaries.append(s.start);

				if ( i == s.endLine )
				{
					m_selectionBoundaries.append(s.end);
				} else {
					//++selLevel;
					inSel = true;
					//selEnd = h->m_text.length();
				}
			} else if ( inSel && (i == s.endLine) ) {

				if ( m_selectionBoundaries.count() % 2 )
					m_selectionBoundaries.append(s.end);

				//--selLevel;
				inSel = false;
			}
		}


		if ( inSel && m_selectionBoundaries.count() == 1 && m_selectionBoundaries.at(0) == 0 )
		{
			m_selectionBoundaries.clear();
			fullSel = true;
		}

		if ( h->hasFlag(QDocumentLine::Hidden) )
		{
			continue;
		} else
			++realln;

		if ( wrapped )
			realln += wrap;

		m_cursorLines.clear();

		bg = base;

		// idx = column = 0;
		xOffset = m_leftMargin; // margin

		// cursor(s) stuff
		cit = cxt.cursors.begin();

		while ( cit != cxt.cursors.end() )
		{
			if ( (*cit)->lineNumber() == i )
			{
				if ( cxt.blinkingCursor )
					m_cursorLines.append((*cit)->columnNumber());

				if ( cxt.fillCursorRect ){
					bg = alternate;
					currentLine=true;
				}

				cit = cxt.cursors.erase(cit);
			} else {
				++cit;
			}
		}

		cit = cxt.extra.begin();

		while ( cit != cxt.extra.end() )
		{
			if ( (*cit)->lineNumber() == i )
			{
				m_cursorLines.append((*cit)->columnNumber());

				cit = cxt.extra.erase(cit);
			} else {
				++cit;
			}
		}

		qSort(m_cursorLines);

		QList<int> m = marks(h);

		// line marks stuff
		if ( m.count() )
		{
			QLineMarksInfoCenter *mic = QLineMarksInfoCenter::instance();

			QColor c = mic->markType(mic->priority(m)).color;

			if ( c.isValid() )
				bg = c;

		}

		if ( realln < firstLine )
			continue;

		//qDebug("drawing line %i (visual %i)", i, realln);
		/* not used when caching
		p->fillRect(qMax(cxt.xoffset, m_leftMargin), pos,
					cxt.width, m_lineSpacing,
					fullSel ? selbg : bg);

		if ( wrapped )
			p->fillRect(qMax(cxt.xoffset, m_leftMargin), pos + m_lineSpacing,
						cxt.width, m_lineSpacing * wrap, fullSel ? selbg : bg);
		*/

		//p->fillRect(cxt.xoffset, pos + 1,
		//			cxt.width, m_lineHeight,
		//			bg);
		bool curSelectionState=inSel || (!m_selectionBoundaries.empty()) || (!m_cursorLines.empty());

		if(fullSel && h->lineHasSelection!=QDocumentLineHandle::fullSel) {
			h->setFlag(QDocumentLine::LayoutDirty,true);
			h->lineHasSelection=QDocumentLineHandle::fullSel;
		}
		if(!fullSel && curSelectionState){
			h->setFlag(QDocumentLine::LayoutDirty,true);
			h->lineHasSelection=QDocumentLineHandle::partialSel;
		}
		if(!curSelectionState && h->lineHasSelection!=QDocumentLineHandle::noSel){
			h->setFlag(QDocumentLine::LayoutDirty,true);
			h->lineHasSelection=QDocumentLineHandle::noSel;
		}

		p->save();

		// simplify line drawing
		p->translate(0, pos);

		// draw text with caching
		QPixmap *px;

		if(!currentLine&&!h->hasFlag(QDocumentLine::LayoutDirty)&&h->hasFlag(QDocumentLine::FormatsApplied)&&m_LineCache.contains(h)){
			px=m_LineCache.object(h);
			p->drawPixmap(m_oldLineCacheOffset,0,*px);
		} else {
			int ht=m_lineSpacing*(wrap+1);
			int yoff= (currentLine && cxt.yoffset-pos>0) ? cxt.yoffset-pos : 0;
			if(currentLine){
				ht= ht > cxt.yoffset+cxt.height-pos ? cxt.yoffset+cxt.height-pos : ht;
				if( ht <= 0 )
					ht=m_lineSpacing;
				px = new QPixmap(lineCacheWidth,ht-yoff);
			}else{
				px = new QPixmap(lineCacheWidth,ht-yoff);
			}
			//px->fill(base.color());//fullSel ? selbg.color() : bg.color());
			px->fill(fullSel ? selbg.color() : bg.color());
			QPainter pnt(px);
			pnt.setFont(p->font());
			pnt.translate(-cxt.xoffset,-yoff);
			pnt.fillRect(0, 0,
									m_leftMargin, ht,
									base); // fillrect executed twice, to be optimized
			/*pnt.fillRect(qMax(cxt.xoffset, m_leftMargin), 0,
						lineCacheWidth, m_lineSpacing,
						fullSel ? selbg : bg); // fillrect executed twice, to be optimized

			if ( wrapped )
				pnt.fillRect(qMax(cxt.xoffset, m_leftMargin), m_lineSpacing,
							 lineCacheWidth, m_lineSpacing * wrap, fullSel ? selbg : bg);*/
			h->draw(&pnt, cxt.xoffset, lineCacheWidth, m_selectionBoundaries, m_cursorLines, cxt.palette, fullSel,yoff,ht);
			p->drawPixmap(cxt.xoffset,yoff,*px);
			pnt.end();
			if(!currentLine) m_LineCache.insert(h,px);
			else {
				m_LineCache.remove(h);
				delete px;
			}
		}


		currentLine=false;
		// see above
		p->translate(0, -pos);

		// draw fold rect indicator
		if ( h->hasFlag(QDocumentLine::CollapsedBlockStart) )
		{
			p->setBrush(Qt::NoBrush);
			p->setPen(QPen(Qt::blue, 1, Qt::DotLine));

			//p->drawRect(cxt.xoffset + 2, pos,
			//			cxt.width - 4, m_lineSpacing - 1);

			p->drawRect(m_leftMargin, pos,
						cxt.width - 4, m_lineSpacing * (wrap + 1) - 1);

		}

		p->restore();

		pos += m_lineSpacing;

		if ( wrapped )
		{
			pos += m_lineSpacing * wrap;
		}

		//qDebug("drawing line %i in %i ms", i, t.elapsed());
	}

	m_oldLineCacheOffset = cxt.xoffset;
	m_oldLineCacheWidth = lineCacheWidth;
	//qDebug("painting done"); // in %i ms...", t.elapsed());
}

QString QDocumentPrivate::exportAsHtml(const QDocumentCursor& range, bool includeHeader, bool simplifyCSS) const{
	QString result;
	if (includeHeader) {
		result += "<html><head>";
		if ( m_formatScheme ) {
			result += "<style type=\"text/css\">";
			result += QString("pre { margin: %1px }\n").arg(simplifyCSS?0:1);
			result += m_formatScheme->exportAsCSS(simplifyCSS);
			result += "</style>";
		}
		result += "</head><body>";
	}
	QDocumentSelection sel = range.selection();
	REQUIRE_RET(sel.startLine >= 0 && sel.startLine < m_lines.size(),"");
	REQUIRE_RET(sel.endLine >= 0 && sel.endLine < m_lines.size(),"");
	result += m_lines[sel.startLine]->exportAsHtml(sel.start)+"\n";
	for (int i=sel.startLine+1; i<sel.endLine; i++)
		result += m_lines[i]->exportAsHtml() + "\n";
	if (sel.startLine != sel.endLine)
		result += m_lines[sel.endLine]->exportAsHtml(0,sel.end);

	if (includeHeader)
		result += "</body></html>";
	return result;
}


int QDocumentPrivate::position(const QDocumentLineHandle *l) const
{
	int pos = 0;

	int idx = m_lines.indexOf(const_cast<QDocumentLineHandle*>(l));

	if ( idx == -1 )
		return -1;

	for ( int i = 0; i < idx; i++ )
		pos += m_lines.at(i)->length();

	pos += m_lineEndingString.length()*idx;
		
	return pos;
}

QDocumentLineHandle* QDocumentPrivate::lineForPosition(int& position) const
{
	int pos = 0, idx = 0;

	while ( (pos + m_lines.at(idx)->length()) < position )
		pos += m_lines.at(idx++)->length();


	return 0;
}

void QDocumentPrivate::setHardLineWrap(bool wrap)
{
    m_hardLineWrap=wrap;
}

void QDocumentPrivate::setWidth(int width)
{
	int oldConstraint = m_constrained;
	m_constrained = width > 0;

	if ( m_constrained )
	{
		int oldWidth = m_width;

		m_width = width;

		if ( oldConstraint && oldWidth < width )
		{
			// expand : simply remove old wraps if possible

			QMap<int, int>::iterator it = m_wrapped.begin();

			while ( it != m_wrapped.end() )
			{
				QDocumentLineHandle *h = it.key() < m_lines.count() ? m_lines.at(it.key()) : 0;

				if ( h )
					h->updateWrap();

				int sz = h ? h->m_frontiers.count() : 0;

				if ( sz )
				{
					//qDebug("changing wrap at line %i from %i to %i", it.key(), *it, sz);
					*it = sz;
					++it;
				} else {
					//qDebug("removing wrap at line %i", it.key());
					it = m_wrapped.erase(it);
				}
			}
		} else if ( oldWidth > width ) {
			// shrink : scan whole document and create new wraps wherever needed
			//qDebug("global width scan [constraint on]");
			//m_wrapped.clear();
			setWidth();
		}
	} else {
		//qDebug("global width scan [constraint off]");
		m_wrapped.clear();
		setWidth();
	}

	if ( m_editCursor )
	{
		m_editCursor->refreshColumnMemory();
	}

	emitWidthChanged();
	setHeight();

	emitFormatsChanged();
	markFormatCacheDirty();
}

void QDocumentPrivate::removeWrap(int i){
    m_wrapped.remove(i);
}

void QDocumentPrivate::setWidth()
{
	m_largest.clear();
	const int max = m_lines.count();

	if ( m_constrained )
	{
		int first = -1;

		for ( int i = 0; i < max; ++i )
		{
			QDocumentLineHandle *l = m_lines.at(i);
			int olw = l->m_frontiers.count();

			l->updateWrap();

			int lw = l->m_frontiers.count();

			if ( olw == lw )
				continue;

			if ( lw )
			{
				//qDebug("added wrap on line %i", line);
				m_wrapped[i] = lw;
			} else {
				//qDebug("removed wrap on line %i", line);
				m_wrapped.remove(i);
			}

			if ( first != -1 )
				first = i;
		}

		if ( first != -1 )
			emitFormatsChange(first, -1);
	} else {
		int oldWidth = m_width;

		m_width = 0;

		foreach ( QDocumentLineHandle *l, m_lines )
		{
			if ( l->hasFlag(QDocumentLine::Hidden) )
				continue;

			l->m_frontiers.clear();

			int w = l->cursorToX(l->length());

			if ( w > m_width )
			{
				m_width = w;

				m_largest.clear();
				m_largest << qMakePair(l, w);
			}
		}

		if ( m_width != oldWidth )
			emitWidthChanged();
	}
}

static const int widthCacheSize = 5;

void QDocumentPrivate::adjustWidth(int line)
{
	if ( line < 0 || line >= m_lines.count() )
		return;

	QDocumentLineHandle *l = m_lines.at(line);

	if ( m_constrained )
	{
		int olw = l->m_frontiers.count();

		l->updateWrap();

		int lw = l->m_frontiers.count();

		if ( olw == lw )
			return;

		if ( l->m_layout )
			l->setFlag(QDocumentLine::LayoutDirty);

		if ( lw )
		{
			//qDebug("added wrap on line %i", line);
			m_wrapped[line] = lw;
		} else {
			//qDebug("removed wrap on line %i", line);
			m_wrapped.remove(line);
		}

		emitFormatsChange(line, -1);
		setHeight();

	} else {
		l->m_frontiers.clear();

		int w = l->cursorToX(l->length());

		if ( w > m_width )
		{
			m_width = w;
			emitWidthChanged();

			m_largest.clear();
			m_largest << qMakePair(l, w);
		} else if ( m_largest.count() && (m_largest.at(0).first == l) ) {
			int old = m_largest.at(0).second;

			if ( w < old )
				setWidth();
		}
	}
}

void QDocumentPrivate::setHeight()
{
	int oldHeight = m_height;
	int last = visualLine(m_lines.count() - 1) + 1;

	if ( m_lines.count() )
		last += m_lines.last()->m_frontiers.count();

	m_height = last * m_lineSpacing;

	if ( oldHeight != m_height )
		emitHeightChanged();
}

void QDocumentPrivate::setFont(const QFont& f)
{
	if ( !m_font )
	{
		m_font = new QFont;
	}

	qDeleteAll(m_fmtWidthCache);
	m_fmtWidthCache.clear();

	*m_font = f;

	// ensures the font is fixed pitch to avoid idsplay glitches
	// and inconsistency of column selections
	// :( does not work well...
	//m_font->setFixedPitch(true);

	// set the styling so that if the font is not found Courier one will be used
	m_font->setStyleHint(QFont::Courier, QFont::PreferQuality);

	//disable kerning because words are drawn at once, but there width is calculated character
	//by character (in functions which calculate the cursor position)
	m_font->setKerning(false);

	QFontMetrics fm(*m_font);
	m_spaceWidth = fm.width(' ');
	m_lineSpacing = fm.lineSpacing();
	m_ascent = fm.ascent();
	m_descent = fm.descent();
	m_leading = fm.leading();

	m_lineHeight = fm.height();
	if(m_lineHeight>m_lineSpacing) m_lineSpacing=m_lineHeight;
	//m_lineHeight = m_ascent + m_descent - 2;

	updateFormatCache();
	//if ( !m_fixedPitch )
	//	qDebug("unsafe computations...");

	foreach ( QDocumentPrivate *d, m_documents )
	{
		d->setWidth();
		d->setHeight();
	}
}

void QDocumentPrivate::setFormatScheme(QFormatScheme *f)
{
	m_formatScheme = f;
	setFont(*m_font);
}

void QDocumentPrivate::tunePainter(QPainter *p, int fid)
{
	if ( fid < m_fonts.count() )
	{
		p->setFont(m_fonts.at(fid));
		//p->setPen(m_colors.at(fid));
	} else {
		p->setFont(*m_font);
		//p->setPen(Qt::black);
	}
}

int QDocumentPrivate::textWidth(int fid, const QString& text){
	if ( fid < 0 || fid >= m_fonts.size() || text.isEmpty()) return 0;

	/*
	  There are three different ways to calculate the width:
	  1. String length * Character Width           if fixedPitch && no surrogates && no asian characters
	  2. Sum of all character widths               if !DisableWidthCache && no surrogates
	  3. QFontMetric::width                        else
	*/
	bool containsSurrogates = false;
	if ( QDocumentPrivate::m_fixedPitch ) {
		bool containsAsianChars = false;
		foreach (const QChar& c, text){
			const QChar::Category cat = c.category();
			if (cat == QChar::Letter_Other)
				containsAsianChars = true; //character which can have a different width even in fixed pitch fonts
			else if (cat == QChar::Other_Surrogate || cat == QChar::Mark_Enclosing || cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining)
				containsSurrogates = true; //strange characters (e.g.  0xbcd, 0x1d164)
		}
		if (!containsAsianChars && !containsSurrogates)
			return text.length() * QDocumentPrivate::m_spaceWidth;
	} else {
		//only check for the strange characters
		foreach (const QChar& c, text){
			const QChar::Category cat = c.category();
			if (cat == QChar::Other_Surrogate || cat == QChar::Mark_Enclosing || cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining)
				containsSurrogates = true;
		}
	}

	if ( containsSurrogates || (m_workArounds & QDocument::DisableWidthCache) )
		return m_fontMetrics[fid].width(text);

	int rwidth=0;

	WCache *wCache;
	if(m_fmtWidthCache.contains(fid)){
		wCache=m_fmtWidthCache.value(fid);
	}else{
		wCache=new WCache;
		m_fmtWidthCache.insert(fid,wCache);
	}
	foreach(const QChar& c, text){
		if(wCache->contains(c)){
			rwidth+=wCache->value(c);
		}else {
			int cwidth = m_fontMetrics[fid].width(c);
			wCache->insert(c,cwidth);
			rwidth+=cwidth;
		}
	}
	return rwidth;
}

void QDocumentPrivate::calcPositionAfterRenderRange(int &xpos, int &column, const RenderRange& r, const int newFont, const QString& text){
	const QString& subText = text.mid(r.position, r.length);
	if (r.format & FORMAT_SPACE) {
		int realLength = QDocument::screenColumn(subText.constData(), subText.length(), m_tabStop, column) - column;
		column += realLength;
		xpos += textWidth(newFont, " ") * realLength;
	} else {
		column += r.length;
		xpos += textWidth(newFont, subText);
	}
}

void QDocumentPrivate::updateFormatCache()
{
	m_fixedPitch = !hasWorkAround(QDocument::DisableFixedPitchMode) && QFontInfo(*m_font).fixedPitch();

	m_fonts.clear();
	m_fontMetrics.clear();

	if ( !m_font )
		return;

	if ( !m_formatScheme )
	{
		m_fonts << *m_font;
		m_fontMetrics << QFontMetrics(*m_font);
		return;
	}

	const int end = m_formatScheme->formatCount();

	m_fonts.reserve(end);

	for ( int i = 0; i < end; i++ )
	{
		QFormat fmt = m_formatScheme->format(i);

		QFont f(*m_font);
		Q_ASSERT(!f.kerning());
		f.setWeight(fmt.weight);
		f.setItalic(fmt.italic);
		if ( !fmt.fontFamily.isEmpty() ){
			f.setFamily(fmt.fontFamily);
			m_fixedPitch = false;
		}
		if ( fmt.pointSize ){
			f.setPointSize(fmt.pointSize);
			m_fixedPitch = false;
		}

		m_fonts << f;
		m_fontMetrics << QFontMetrics(f);
	}

	foreach ( QDocumentPrivate *d, m_documents )
		d->emitFormatsChanged();

	//emitFormatsChanged();
}

void QDocumentPrivate::emitWidthChanged()
{
	if ( !m_doc )
		return;

	emit m_doc->widthChanged(m_width);

	emit m_doc->sizeChanged(QSize(m_width, m_height));
}

void QDocumentPrivate::emitHeightChanged()
{
	if ( !m_doc )
		return;

	emit m_doc->heightChanged(m_height);

	emit m_doc->sizeChanged(QSize(m_width, m_height));
}

void QDocumentPrivate::insertLines(int after, const QList<QDocumentLineHandle*>& l)
{
	//qDebug("inserting : %i, %i", after, l.count());

	int i = 0;

	foreach ( QDocumentLineHandle *h, l )
	{
		h->setFlag(QDocumentLine::Hidden, false);
		h->setFlag(QDocumentLine::CollapsedBlockStart, false);
		h->setFlag(QDocumentLine::CollapsedBlockEnd, false);
		h->m_frontiers.clear();
	}

	QMap<int, int>::iterator it = m_hidden.begin();

	while ( it != m_hidden.end() )
	{
		if ( (it.key() <= after) && ((it.key() + *it) > after) )
		{
			*it += l.count();

			foreach ( QDocumentLineHandle *h, l )
				h->setFlag(QDocumentLine::Hidden, true);
		}

		++it;
	}

	++after;
	updateHidden(after, l.count());
	updateWrapped(after, l.count());

	while ( i < l.count() )
	{
		// TODO : move (and abstract somehow) inside the line (handle?)
		l.at(i)->m_context.reset();

		m_lines.insert(after + i, l.at(i));

		adjustWidth(after + i);

		++i;
	}

	emit m_doc->lineCountChanged(m_lines.count());
	setHeight();
}

void QDocumentPrivate::removeLines(int after, int n)
{
	if ( (after >= 0) && (after < m_lines.count()) )
		m_lines.at(after)->setFlag(QDocumentLine::CollapsedBlockStart, false);

	QMap<int, int>::iterator it = m_hidden.begin();

	//qDebug("translating %i", visualLine);
/*	//remove/resize the m_hidden cache if the removed lines are within a hidden block
	//buggy (test {\n}\ndeleted\n{\n}) and not needed if the qdocumentcommand corrects
	//the folding of modified lines
	while ( it != m_hidden.end() )
	{
		if ( (it.key() > after) && (it.key() <= (after + n)) )
		{
			//folded block starting line within the removed lines
			int i = it.key(), end = i + *it, depth = 0;

			while ( i <= end )
			{
				if ( !depth )
					m_lines.at(i)->setFlag(QDocumentLine::Hidden, false);

				if ( m_lines.at(i)->hasFlag(QDocumentLine::CollapsedBlockStart) )
					++depth;
				else if ( m_lines.at(i)->hasFlag(QDocumentLine::CollapsedBlockEnd) )
					--depth;

				++i;
			}

			it = m_hidden.erase(it);

		} else if ( (it.key() < after) && (it.key() + *it) > after ) {
			//folded starting before the removed lines and containing them
			if ( (it.key() + *it) > (after + n) )
			{
				// fully inside
				*it -= n;
				++it;
			} else {
				// goes beyond...
				int i = it.key(), end = i + *it, depth = 0;

				while ( i <= end )
				{
					if ( !depth )
						m_lines.at(i)->setFlag(QDocumentLine::Hidden, false);

					if ( m_lines.at(i)->hasFlag(QDocumentLine::CollapsedBlockStart) )
						++depth;
					else if ( m_lines.at(i)->hasFlag(QDocumentLine::CollapsedBlockEnd) )
						--depth;

					++i;
				}

				it = m_hidden.erase(it);
			}
		} else {
			++it;
		}
	}
*/
	it = m_wrapped.begin();

	while ( it != m_wrapped.end() )
	{
		if ( (it.key() > after) && (it.key() <= (after + n)) )
		{
			//qDebug("eraser %i", it.key());
			it = m_wrapped.erase(it);
		} else {
			++it;
		}
	}


	++after;
	updateHidden(after, -n);
	updateWrapped(after, -n);
	for(int i=after;i<after+n;i++){
		emit m_doc->lineRemoved(m_lines[i]);
	}
	m_lines.remove(after, n);

	emit m_doc->lineCountChanged(m_lines.count());
	setHeight();
}

QDocumentLineHandle* QDocumentPrivate::at(int line) const
{
	return ((line >= 0) && (line < m_lines.count())) ? m_lines.at(line) : 0;
}

int QDocumentPrivate::indexOf(const QDocumentLineHandle *l) const
{
	return m_lines.indexOf(const_cast<QDocumentLineHandle*>(l));
}

QDocumentIterator QDocumentPrivate::index(const QDocumentLineHandle *l)
{
	QDocumentIterator i = m_lines.begin();

	int idx = indexOf(l);

	return (idx != -1) ? i + idx : m_lines.end();
}

QDocumentConstIterator QDocumentPrivate::index(const QDocumentLineHandle *l) const
{
	QDocumentConstIterator i = m_lines.constBegin();

	int idx = indexOf(l);

	return (idx != -1) ? i + idx : m_lines.end();
}

QDocumentLineHandle* QDocumentPrivate::next(const QDocumentLineHandle *l) const
{
	if ( !l )
		return m_lines.count() ? m_lines.first() : 0;

	int idx = m_lines.indexOf(const_cast<QDocumentLineHandle*>(l));

	return ((idx != -1) && ((idx + 1) < m_lines.count())) ? m_lines.at(idx + 1) : 0;
}

QDocumentLineHandle* QDocumentPrivate::previous(const QDocumentLineHandle *l) const
{
	if ( !l )
		return m_lines.count() ? m_lines.last() : 0;

	int idx = m_lines.indexOf(const_cast<QDocumentLineHandle*>(l));

	return ((idx != -1) && (idx > 0)) ? m_lines.at(idx - 1) : 0;
}

void QDocumentPrivate::beginChangeBlock()
{
	QDocumentCommandBlock *b = new QDocumentCommandBlock(m_doc);
	
	m_macros.push(b);
}

void QDocumentPrivate::endChangeBlock()
{
	if ( !m_macros.count() )
		return;
	
	QDocumentCommandBlock *b = m_macros.pop();
	b->setWeakLock(true);
	
	execute(b);
}

bool QDocumentPrivate::hasChangeBlocks(){
	return m_macros.count()!=0;
}
/*!
	\brief Acquire group id
*/
int QDocumentPrivate::getNextGroupId()
{
	if ( m_freeGroupIds.count() )
		return m_freeGroupIds.takeFirst();

	return ++m_lastGroupId;
}

/*!
	\brief Relase group id
*/
void QDocumentPrivate::releaseGroupId(int groupId)
{
	if ( groupId == m_lastGroupId )
	{
		--m_lastGroupId;
		while ( m_freeGroupIds.removeAll(m_lastGroupId) )
		{
			--m_lastGroupId;
		}
	} else {
		m_freeGroupIds << groupId;
	}
}

/*!
	\brief Clear matches
*/
void QDocumentPrivate::clearMatches(int groupId)
{
	QHash<int, MatchList>::iterator mit = m_matches.find(groupId);

	if ( mit == m_matches.end() )
	{
		return;
	}

	MatchList& matches = *mit;

	foreach ( const Match& m, matches )
	{
		m.h->removeOverlay(m.range);
		m_LineCache.remove(m.h);
	}

	matches.removeStart = 0;
	matches.removeLength = matches.count();
}

/*void QDocumentPrivate::clearMatchesFromToWhenFlushing(gid,firstMatch,lastMatch){
	QHash<int, MatchList>::iterator mit = m_matches.find(groupId);

	if ( mit == m_matches.end() )
	{
		return;
	}

	MatchList& matches = *mit;
	if (firstMatch>=matches.length()) firstMatch=matches.length()-1;
	if (lastMatch>=matches.length()) lastMatch=matches.length()-1;
	for (int i=firstMatch; i<=lastMatch; i++){
		matches[i].h->removeOverlay(matches[i].range);
	}
	matches.removeStart=firstMatch;
	matches.removeLength=lastMatch-firstMatch;
}*/
/*!
	\brief Highlight the matched sequences

	\note Both position are BEFORE the matched characters (cursor position).
*/
void QDocumentPrivate::addMatch(int groupId, int line, int pos, int len, int format)
{
	//qDebug("match (%i, %i, %i)", line, pos, len);

	Match m;
	m.line = line;
	m.h = at(line);
	if (!m.h) return;
	m_LineCache.remove(m.h);
	m.range = QFormatRange(pos, len, format);
	m_matches[groupId] << m;

	m.h->addOverlay(m.range);
}

void QDocumentPrivate::flushMatches(int groupId)
{
	QHash<int, MatchList>::iterator mit = m_matches.find(groupId);

	if ( mit == m_matches.end() )
	{
		return;
	}

	MatchList& matches = *mit;

	QMap<int, int> areas;

	foreach ( const Match& m, matches )
	{
		int n = 1;
		int l = m.line;

		//qDebug("simple:(%i, %i)", l, 1);

		QMap<int, int>::iterator tmp, it = areas.find(l);

		if ( it != areas.end() )
			continue;

		it = areas.insert(m.line, n);

		if ( it != areas.end() && (it - 1) != areas.end() )
		{
			tmp = it - 1;
			int off = tmp.key() + *tmp - l;

			if ( off >= 0 && (off < n) )
			{
				*tmp += n - off;
				it = areas.erase(it) - 1;
			}
		}

		if ( it != areas.end() && (it + 1) != areas.end() )
		{
			tmp = it + 1;
			int off = it.key() + *it - tmp.key();

			if ( off >= 0 && (off < *tmp) )
			{
				*it += *tmp;
				areas.erase(tmp);
			}
		}
		//emitFormatsChange(m.line, 1);
	}

	// remove old matches
	while ( matches.removeLength )
	{
		matches.removeAt(matches.removeStart);
		--matches.removeLength;
	}
	
	// send update messages
	QMap<int, int>::const_iterator it = areas.constBegin();

	while ( it != areas.constEnd() )
	{
		//qDebug("merged:(%i, %i)", it.key(), *it);
		emitFormatsChange(it.key(), *it);

		++it;
	}

	// update storage "meta-data"
	if ( matches.isEmpty() )
	{
		m_matches.remove(groupId);

		releaseGroupId(groupId);
	}
	//qDebug("done with matches");
}

QList<int> QDocumentPrivate::marks(QDocumentLineHandle *h) const
{
	//return QList<int>() << 1; //testcase

	return m_marks.contains(h) ? m_marks.value(h) : QList<int>();
}

void QDocumentPrivate::addMark(QDocumentLineHandle *h, int mid)
{
	QList<int>& l = m_marks[h];

	if (l.empty()) l << mid;
	else {
        int p=QLineMarksInfoCenter::instance()->priority(mid);
        int i;
        for (i=0;i<l.size();i++)
            if (QLineMarksInfoCenter::instance()->priority(l[i])>=p) {
                l.insert(i,mid);
                break;
            }
        if (i==l.size()) l << mid;
	}

	m_maxMarksPerLine = qMax(l.count(), m_maxMarksPerLine);

	emitMarkChanged(h, mid, true);
}

void QDocumentPrivate::toggleMark(QDocumentLineHandle *h, int mid)
{
	if ( m_marks.value(h).contains(mid) )
	{
		removeMark(h, mid);
	} else {
		addMark(h, mid);
	}
}

void QDocumentPrivate::removeMark(QDocumentLineHandle *h, int mid)
{
	QHash<QDocumentLineHandle*, QList<int> >::iterator it = m_marks.find(h);

	if ( it == m_marks.end() )
		return;

	int count = it->count();
	int n = it->removeAll(mid);

	if ( it->isEmpty() )
		m_marks.erase(it);

	if ( n && (count == m_maxMarksPerLine) )
	{
		QHash<QDocumentLineHandle*, QList<int> >::const_iterator
			rit = m_marks.constBegin(),
			end = m_marks.constEnd();

		m_maxMarksPerLine = 0;

		while ( rit != end )
		{
			m_maxMarksPerLine = qMax(rit->count(), m_maxMarksPerLine);
			++rit;
		}
	}

	emitMarkChanged(h, mid, false);
}

int QDocumentPrivate::visualLine(int textLine) const
{
	if ( textLine < 0 )
		return 0;

	int hiddenLines = 0, wrappedLines = 0;
	QMap<int, int>::const_iterator hit, wit, he, we;
	hit = m_hidden.constBegin();
	wit = m_wrapped.constBegin();
	he = m_hidden.constEnd();
	we = m_wrapped.constEnd();

	//qDebug("translating %i", visualLine);

	while ( hit != he || wit != we )
	{
		if ( hit != he && (wit == we || hit.key() <= wit.key()) )
		{
			int hl = hit.key();

			if ( hl >= textLine )
				break;

			int max = 0;

			do
			{
				max = qMax(max, hit.key() - hl + *hit);
				++hit;
			} while ( (hit != he) && (hit.key() <= hl + max) );

			hiddenLines += max;

			if ( wit != we && wit.key() == hl )
			{
				wrappedLines += *wit;
				++wit;
			}

			while ( wit != we )
			{
				if ( wit.key() > hl + max )
					break;

				++wit;
			}

		} else {
			if ( wit.key() >= textLine )
				break;

			if ( m_lines.at(wit.key())->hasFlag(QDocumentLine::Hidden) )
			{
				++wit;
				continue;
			}

			wrappedLines += *wit;
			++wit;
		}
	}

	//qDebug("translating %i => %i", textLine, textLine - hiddenLines + wrappedLines);

	return textLine - hiddenLines + wrappedLines;
}

int QDocumentPrivate::textLine(int visualLine, int *wrap) const
{
	if ( visualLine < 0 )
		return 0;

	int hiddenLines = 0, wrappedLines = 0, vis = 0, txt = 0, mess = 0;
	QMap<int, int>::const_iterator
		h = m_hidden.constBegin(),
		w = m_wrapped.constBegin(),
		he = m_hidden.constEnd(),
		we = m_wrapped.constEnd();

	//qDebug("translating %i", visualLine);

	while ( vis < visualLine )
	{
		if ( h != he )
		{
			int hl = h.key();

			if ( w == we || hl <= w.key() )
			{
				if ( visualLine + mess <= hl )
					break;

				if ( w != we && w.key() == hl )
				{
					//qDebug("trying to solve : h=(%i, %i), w=(%i, %i)", hl, *h, w.key(), *w);
					const int off = (visualLine + mess) - hl;
					if ( off <= *w )
					{
						//qDebug("%i -> %i + %i", visualLine, hl, off);
						if ( wrap )
							*wrap = off;

						return hl;
					}
				}

				int max = 0;

				do
				{
					max = qMax(max, h.key() - hl + *h);
					++h;
				} while ( (h != he) && (h.key() <= hl + max) );

				// very important : do not forget possible wrapping on folded block start
				if ( w != we && w.key() == hl )
				{
					wrappedLines += *w;
					++w;
				}

				while ( w != we )
				{
					if ( w.key() > txt + max )
						break;

					++w;
				}

				hiddenLines += max;

			} else {
				txt = w.key();

				if ( m_lines.at(txt)->hasFlag(QDocumentLine::Hidden) )
				{
					++w;
					continue;
				}

				if ( visualLine + mess < txt )
					break;

				wrappedLines += *w;
				++w;
			}
		} else if ( w != we ) {
			txt = w.key();

			if ( m_lines.at(txt)->hasFlag(QDocumentLine::Hidden) )
			{
				++w;
				continue;
			}

			if ( visualLine + mess < txt )
				break;

			wrappedLines += *w;
			++w;
		} else {
			break;
		}

		mess = hiddenLines - wrappedLines;
		vis = txt - mess;
	}

	we = m_wrapped.constBegin();

	if ( m_wrapped.count() && w != we )
	{
		--w;

		int wl = w.key();
		bool hwrap = m_lines.at(wl)->hasFlag(QDocumentLine::Hidden);

		if ( !hwrap )
		{
			int base = this->visualLine(wl);

			if ( visualLine >= base && visualLine <= base + *w )
			{
				//qDebug("rebased : %i to %i", visualLine, base);

				if ( wrap )
					*wrap = visualLine - base;

				return wl;
			}
		}
	}

	//qDebug("[visual:text] : %i : %i", visualLine, visualLine + mess);
	int off = visualLine + mess - (m_lines.count() - 1);

	if ( off > 0 )
	{
		if ( wrap )
			*wrap = m_lines.last()->m_frontiers.count();

		return m_lines.count() - 1;
	}

	return visualLine + mess;
}

void QDocumentPrivate::hideEvent(int line, int count)
{
	m_hidden.insertMulti(line, count);

	setHeight();
	//emitFormatsChange(line, count);
	emitFormatsChanged();
}

void QDocumentPrivate::showEvent(int line, int count)
{
	QMap<int, int>::iterator it = m_hidden.find(line);

	while ( (it != m_hidden.end()) && (it.key() == line)  )
	{
		if ( *it == count  || count == -1)
		{
//			qDebug("showing %i lines from %i", count, line);
			it = m_hidden.erase(it);
		} else
			++it;
	}

	setHeight();
	//emitFormatsChange(line, count);
	emitFormatsChanged();
}

void QDocumentPrivate::updateHidden(int line, int count)
{
	if ( m_hidden.isEmpty() || (line > (m_hidden.constEnd() - 1).key() ) )
		return;

	QMap<int, int> prev = m_hidden;
	m_hidden.clear();

	QMap<int, int>::const_iterator it = prev.constBegin();

	//qDebug("shifting by %i from %i", count, line);

	while ( it != prev.constEnd() )
	{
		if ( it.key() < line )
		{
			m_hidden.insertMulti(it.key(), *it);
		} else {
			m_hidden.insertMulti(it.key() + count, *it);
		}

		++it;
	}
}

void QDocumentPrivate::updateWrapped(int line, int count)
{
	if ( m_wrapped.isEmpty() || (line > (m_wrapped.constEnd() - 1).key() ) )
		return;

	QMap<int, int> prev = m_wrapped;
	QMap<int, int>::iterator it = prev.begin();

	m_wrapped.clear();

	//qDebug("shifting by %i from %i", count, line);

	while ( it != prev.end() )
	{
		if ( it.key() < line )
		{
			m_wrapped.insert(it.key(), *it);
		} else {
			//qDebug("moved wrap from line %i to line %i", it.key(), it.key() + count);
			m_wrapped.insert(it.key() + count, *it);
		}

		++it;
	}
}

void QDocumentPrivate::emitFormatsChange(int line, int lines)
{
	emit m_doc->formatsChange(line, lines);
}

void QDocumentPrivate::emitContentsChange(int line, int lines)
{
	//for ( int i = line; i < (line + lines); i++ )
	//	m_lines.at(i)->cache();

	int n = lines;

	if ( m_language )
	{
		n = m_language->tokenize(m_doc, line, lines);
	}
	//qDebug("%i, %i : %i", line, lines, n);

	emit m_doc->contentsChange(line, lines);
	emit m_doc->contentsChanged();

	if ( n > lines )
		emitFormatsChange(line + lines, n - lines);
}

void QDocumentPrivate::markFormatCacheDirty(){
	foreach(QDocumentLineHandle *dlh,m_lines){
		dlh->setFlag(QDocumentLine::FormatsApplied,false);
	}
}

//adds an auto updated cursor
void QDocumentPrivate::addAutoUpdatedCursor(QDocumentCursorHandle* c){
	if ( c->hasFlag(QDocumentCursorHandle::AutoUpdated) ) return;
	m_autoUpdatedCursorIndex.insert(c, m_autoUpdatedCursorList.size());
	m_autoUpdatedCursorList << c;
	c->setFlag( QDocumentCursorHandle::AutoUpdated );
}

//removes an auto updated cursor
void QDocumentPrivate::removeAutoUpdatedCursor(QDocumentCursorHandle* c){
	if ( !c->hasFlag(QDocumentCursorHandle::AutoUpdated) ) return;
	int size = m_autoUpdatedCursorList.size();
	int pos = m_autoUpdatedCursorIndex.value(c, size);
	if ( pos>=size ) return; //not found
	if ( pos != size-1) {
		//fast delete, order doesn't matter
		m_autoUpdatedCursorList[pos] = m_autoUpdatedCursorList.last();
		m_autoUpdatedCursorIndex.insert(m_autoUpdatedCursorList[pos], pos);
	}
	m_autoUpdatedCursorList.removeLast();
	m_autoUpdatedCursorIndex.remove(c);
	c->clearFlag( QDocumentCursorHandle::AutoUpdated );
}

//clears auto updated cursors
void QDocumentPrivate::discardAutoUpdatedCursors(bool documentDeleted){
	foreach (QDocumentCursorHandle* h, m_autoUpdatedCursorList){
		h->clearFlag(QDocumentCursorHandle::AutoUpdated);
		if (documentDeleted) h->m_doc = 0;
	}
	m_autoUpdatedCursorList.clear();
}

void QDocumentPrivate::setWorkAround(QDocument::WorkAroundFlag workAround, bool newValue){
	if (!!(m_workArounds & workAround) == newValue) return;
	if (newValue) m_workArounds |= workAround;
	else m_workArounds &= ~workAround;
	if (workAround == QDocument::DisableFixedPitchMode)
		updateFormatCache();

}

bool QDocumentPrivate::hasWorkAround(QDocument::WorkAroundFlag workAround){
	return (m_workArounds & workAround);
}

bool QDocumentPrivate::getFixedPitch(){
	return m_fixedPitch;
}

void QDocumentPrivate::emitFormatsChanged()
{
	emit m_doc->formatsChanged();
}

void QDocumentPrivate::emitContentsChanged()
{
	//emit m_doc->contentsChanged();
}

void QDocumentPrivate::emitLineDeleted(QDocumentLineHandle *h)
{
	if ( !m_deleting )
	{
		m_marks.remove(h);
		m_status.remove(h);

		int idx = m_lines.indexOf(h);

		if ( idx != -1 )
		{
			//qDebug("removing line %i", idx);

			m_lines.remove(idx);

			if ( m_largest.count() && (m_largest.at(0).first == h) )
			{
				m_largest.remove(0);
				setWidth();
			}

			m_hidden.remove(idx);
			m_wrapped.remove(idx);

			setHeight();
		}
	}

	emit m_doc->lineDeleted(h);
}

void QDocumentPrivate::emitMarkChanged(QDocumentLineHandle *l, int m, bool on)
{
	emitFormatsChanged();
	emit m_doc->markChanged(l, m, on);
}

/*! @} */


