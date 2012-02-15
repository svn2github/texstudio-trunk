/***************************************************************************
 *   copyright       : (C) 2003-2007 by Pascal Brachet                     *
 *   http://www.xm1math.net/texmaker/                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include "mostQtHeaders.h"

#include "ui_configdialog.h"

#include "qformat.h"

//TODO: perhaps move each class in its own file?
class ShortcutComboBox: public QComboBox{
	Q_OBJECT
public:
	ShortcutComboBox(QWidget *parent = 0);
protected:
	virtual void keyPressEvent(QKeyEvent *e);
};

class ShortcutDelegate : public QItemDelegate {
	Q_OBJECT

public:
	ShortcutDelegate(QObject *parent = 0);

	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
	                      const QModelIndex &index) const;

	void setEditorData(QWidget *editor, const QModelIndex &index) const;
	void setModelData(QWidget *editor, QAbstractItemModel *model,
	                  const QModelIndex &index) const;

	void updateEditorGeometry(QWidget *editor,
	                          const QStyleOptionViewItem &option, const QModelIndex &index) const;

	void drawDisplay(QPainter * painter, const QStyleOptionViewItem & option, const QRect & rect, const QString & text) const;

	bool isBasicEditorKey(const QModelIndex& index) const;

	QTreeWidget * treeWidget; //tree widget to remove duplicates from, not necessary

	static const QString deleteRowButton;
	static const QString addRowButton;
public slots:
	void treeWidgetItemClicked(QTreeWidgetItem * item, int column);
};

class BuildManager;
class QFormatConfig;
class ConfigDialog : public QDialog {
	Q_OBJECT

public:
	ConfigDialog(QWidget* parent = 0);
	~ConfigDialog();
	Ui::ConfigDialog ui;
	QCheckBox * checkboxInternalPDFViewer;

	QMap<QString,QFormat> editorFormats;
	QFormatConfig * fmConfig;
	QMap<QString,QVariant> * replacedIconsOnMenus;

	QObject* menuParent;
	QList<QStringList> customizableToolbars;
	QList<QMenu*> allMenus;
	QList<QMenu*> standardToolbarMenus;

	QStringList * environModes;

	BuildManager * buildManager;

	bool riddled;
public slots:
	void changePage(QListWidgetItem *current, QListWidgetItem *previous);
private slots:
	QListWidgetItem * createIcon(const QString &caption, const QIcon &icon, bool advancedOption=false);
	void comboBoxWithPathEdited(const QString& newText);
	void comboBoxWithPathHighlighted(const QString& newText);
	bool browse(QWidget* w, const QString& title, const QString& extension, const QString& startPath = QDir::homePath());
	void browseThesaurus();
	void browsePrecompiling();
	void browseGrammarLTPath();
	void browseGrammarLTJavaPath();
	void browseGrammarWordListsDir();
	void browseDictDir();
	void dictDirChanged(const QString& newText);
	void quickBuildWizard();
	void advancedOptionsToggled(bool on);
	void advancedOptionsClicked(bool on);
	void toolbarChanged(int toolbar);
	void actionsChanged(int actionClass);
	void toToolbarClicked();
	void fromToolbarClicked();
	void checkToolbarMoved();
	void customContextMenuRequested(const QPoint &p);
	void loadOtherIcon();
	void populatePossibleActions(QTreeWidgetItem* parent, const QMenu* menu,bool keepHierarchy);

	void custEnvAddLine();
	void custEnvRemoveLine();
	void custSyntaxAddLine();
	void custSyntaxRemoveLine();
private:
	bool askRiddle();
	static int lastUsedPage;
	static QPoint lastSize;
	int oldToolbarIndex;
};

Q_DECLARE_METATYPE(QAction*);

#endif
