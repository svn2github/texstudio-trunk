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

#ifndef LATEXCOMPLETER_H
#define LATEXCOMPLETER_H

#include "mostQtHeaders.h"

#include "codesnippet.h"

#include "qcodeedit.h"
#include "qeditor.h"
#include "latexeditorview.h"
//#include "qdocumentline_p.h"


class CompletionListModel;
class LatexCompleterConfig;
//class Reference;

class LatexCompleter : public QObject  {
	Q_OBJECT
public:
	enum CompletionFlag { CF_FORCE_VISIBLE_LIST = 1, CF_NORMAL_TEXT = 2, CF_FORCE_REF = 4, CF_OVERRIDEN_BACKSLASH=8};
	Q_DECLARE_FLAGS(CompletionFlags, CompletionFlag);

	LatexCompleter(QObject *p = 0);
	virtual ~LatexCompleter();

	void complete(QEditor *newEditor, const CompletionFlags &flags);
	void setAdditionalWords(const QStringList &newwords, bool normalTextList=false);
	void setAbbreviations(const QStringList &Abbrevs,const QStringList &Tags);

	static void parseHelpfile(QString text);
	static bool hasHelpfile();
	
	bool acceptTriggerString(const QString& trigger);
	
	void setConfig(LatexCompleterConfig* config);
	const LatexCompleterConfig* getConfig() const;

	QString lookupWord(QString id);

	bool close();
	bool isVisible(){
	    return list->isVisible();
	}
private:
	friend class CompleterInputBinding;
	friend class CompletionListModel;
	static LatexCompleterConfig* config;
	int maxWordLen;
	QListView * list;
	CompletionListModel* listModel;
	QEditor *editor;

	QWidget *widget;
	QTabBar *tbBelow,*tbAbove;

	bool editorAutoCloseChars;

	void filterList(QString word,bool showMostUsed=false);
	bool acceptChar(QChar c,int pos);

	static QString helpFile;
	static QHash<QString, QString> helpIndices;
	static QHash<QString, int> helpIndicesCache;

private slots:
	void cursorPositionChanged();
	void selectionChanged(const QModelIndex & index);
	void editorDestroyed();
	void changeView(int pos);
	void listClicked(QModelIndex index);
};

#endif
