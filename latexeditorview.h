/***************************************************************************
 *   copyright       : (C) 2008 by Benito van der Zander                   *
 *   http://www.xm1math.net/texmaker/                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef LATEXEDITORVIEW_H
#define LATEXEDITORVIEW_H
#include "mostQtHeaders.h"
#include "qdocument.h"
#include "syntaxcheck.h"

class QDocumentLineHandle;

class LatexDocument;
class QCodeEdit;
class QEditor;
class QLineMarkPanel;
class QLineNumberPanel;
class QSearchReplacePanel;
class QGotoLinePanel;
class QStatusPanel;
class LatexCompleter;
class SpellerUtility;
class DefaultInputBinding;
class LatexEditorViewConfig;
class LatexEditorView : public QWidget  {
	Q_OBJECT
public:

	LatexEditorView(QWidget *parent, LatexEditorViewConfig* aconfig,LatexDocument *doc=0);
	~LatexEditorView();

	QCodeEdit *codeeditor;
	QEditor *editor;

	LatexDocument* document;

//  FindWidget *findwidget;
	//Functions affecting the editor

	void complete(int flags);
	void jumpChangePositionBackward();
	void jumpChangePositionForward();
	void jumpToBookmark(int bookmarkNumber);
	void toggleBookmark(int bookmarkNumber);
	bool gotoToLabel(const QString& label);
	
	void foldEverything(bool unFold);
	void foldLevel(bool unFold, int level);
	void foldBlockAt(bool unFold, int line);

	void cleanBib();
	
	
	static void setKeyReplacements(QStringList *UserKeyReplace, QStringList *UserKeyReplaceAfterWord, QStringList *UserKeyReplaceBeforeWord);
	static QList<QAction *> getBaseActions();
	static void setBaseActions(QList<QAction *> baseActions);
	static void setSpeller(SpellerUtility* mainSpeller);
	static void setCompleter(LatexCompleter* newCompleter);
	static LatexCompleter* getCompleter();
	void setBibTeXIds(QSet<QString>* newIds);
	
	QMultiHash<QDocumentLineHandle*, int> lineToLogEntries;
	QHash<int, QDocumentLineHandle*> logEntryToLine;

	static int hideTooltipWhenLeavingLine;

	void setLineMarkToolTip(const QString& tooltip);
	void updateSettings();

	QPoint getHoverPosistion(){
		return m_point;
	}

        int syntaxErrorFormat;

	void reCheckSyntax(int linenr=0, int count=-1);

	void closeCompleter();

	QList<QDocumentCursor> autoPreviewCursor;
private:
	QAction *lineNumberPanelAction, *lineMarkPanelAction, *lineFoldPanelAction, *lineChangePanelAction, 
	*statusPanelAction, *searchReplacePanelAction, *gotoLinePanelAction;
	QLineMarkPanel* lineMarkPanel;
	QLineNumberPanel* lineNumberPanel;
	QSearchReplacePanel* searchReplacePanel;
	QGotoLinePanel* gotoLinePanel;
	QStatusPanel* statusPanel;

	QPoint m_point;

        int environmentFormat,referencePresentFormat,referenceMissingFormat,referenceMultipleFormat, citationMissingFormat, citationPresentFormat,structureFormat,styleHintFormat,verbatimFormat;
	
	friend class DefaultInputBinding;
	friend class SyntaxCheckTest;
	static int bookMarkId(int bookmarkNumber);

	static SpellerUtility* speller;
	static LatexCompleter* completer;
	QSet<QString>* bibTeXIds;
	QList<QPair<QDocumentLineHandle*, int> > changePositions; //line, index
	int curChangePos;
	int lastSetBookmark; //only looks at 1..3 (mouse range)

	LatexEditorViewConfig* config;

	void getEnv(int lineNumber,StackEnvironment &env); // get Environment for syntax checking, number of cols is now part of env

	SyntaxCheck SynChecker;

private slots:
	void requestCitation(); //emits needCitation with selected text
        void openExternalFile();
	void lineMarkClicked(int line);
	void lineMarkToolTip(int line, int mark);
	void checkNextLine(QDocumentLineHandle *dlh,bool clearOverlay,int excessCols,int ticket);
public slots:
	void documentContentChanged(int linenr, int count);
	void documentFormatsChanged(int linenr, int count);
	void lineDeleted(QDocumentLineHandle* l);
	void spellCheckingReplace();
	void spellCheckingAlwaysIgnore();
	void spellCheckingListSuggestions();
	void dictionaryReloaded();
	void mouseHovered(QPoint pos);
	bool closeSomething();
	void insertHardLineBreaks(int newLength, bool smartScopeSelection, bool joinLines);
	void viewActivated();
	void clearOverlays();
	void updateLtxCommands();
	void paste();
signals:
	void lineHandleDeleted(QDocumentLineHandle* l);
	void showMarkTooltipForLogMessage(int logMessage);
	void needCitation(const QString& id);//request a new citation 
	void showPreview(const QString text);
	void showPreview(const QDocumentCursor& c);
	void openFile(const QString name);
};

class BracketInvertAffector: public PlaceHolder::Affector{
public:
	virtual QString affect(const QKeyEvent *e, const QString& base, int ph, int mirror) const;
	static BracketInvertAffector* instance();
};
#endif
