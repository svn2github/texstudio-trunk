/***************************************************************************
 *   copyright       : (C) 2003-2007 by Pascal Brachet                     *
 *   http://www.xm1math.net/texmaker/                                      *
 *                         2008-2009 by Benito van der Zander              *
 *   http://texmakerx.sourceforge.net
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TEXMAKER_H
#define TEXMAKER_H


#include "buildmanager.h"
#include "codesnippet.h"
#include "configmanager.h"
#include "latexlog.h"
#include "latexeditorview.h"
#include "latexcompleter.h"
#include "symbollistwidget.h"
#include "metapostlistwidget.h"
#include "pstrickslistwidget.h"
#include "spellerdialog.h"
#include "textanalysis.h"
#include "toolwidgets.h"
#include "thesaurusdialog.h"


#include "qformatfactory.h"
#include "qlanguagefactory.h"
#include "qlinemarksinfocenter.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QListWidget>
#include <QToolBox>
#include <QToolButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QAction>
#include <QTreeWidgetItem>
#include <QListWidgetItem>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QColor>
#include <QDateTime>
#include <QTextTable>
#include <QVBoxLayout>
#include <QTableView>


typedef  QMap<LatexEditorView*, QString> FilesMap;
typedef  QString Userlist[10];
typedef  QString UserCd[5];
typedef int SymbolList[412];

struct BibTeXFileInfo{
	QDateTime lastModified;
	QStringList ids;
	QString linksTo;
};

class Texmaker : public QMainWindow {
	Q_OBJECT

public:
	Texmaker(QWidget *parent = 0, Qt::WFlags flags = 0);

	QString getName();
	QString getCurrentFileName(); //returns the absolute file name of the current file or "" if none is opened
	QString getCompileFileName(); //returns the absolute file name of the file to be compiled (master or current)  TODO: test if it is always absolute (it depends on MasterFile and filenames)
	QString getAbsoluteFileName(const QString & relName, const QString &extension=""); //treats the path relative to the compiled .tex file
	QByteArray windowstate;
public slots:
	LatexEditorView* load(const QString &f , bool asProject = false);
	void executeCommandLine(const QStringList& args, bool realCmdLine);
	void onOtherInstanceMessage(const QString &);  // For messages for the single instance

private:
	//these are just wrappers around configManager so we don't have to type so much (todo??? move them to configmanager.h and use a singleton design???)
	inline QMenu* newManagedMenu(const QString &id,const QString &text);
	inline QMenu* newManagedMenu(QMenu* menu, const QString &id,const QString &text);
	inline QAction* newManagedAction(QWidget* menu, const QString &id,const QString &text, const char* slotName=0, const QKeySequence &shortCut=0, const QString & iconFile="");
	inline QAction* newManagedAction(QWidget* menu, const QString &id,const QString &text, const char* slotName, const QList<QKeySequence> &shortCuts, const QString & iconFile="");
	inline QAction* newManagedAction(QWidget* menu, const QString &id, QAction* act);
	inline QAction* getManagedAction(QString id);
	
	void addSymbolList(SymbolListWidget** list, int index, const char* slot, const QString& iconName, const QString& text);
	void setupDockWidgets();
	void setupMenus();
	void setupToolBars();
	void createStatusBar();
	bool FileAlreadyOpen(QString f);
	void closeEvent(QCloseEvent *e);

	FilesMap filenames;
	QMap<QString, BibTeXFileInfo> bibTeXFiles; //bibtex files loaded by tmx
	QStringList mentionedBibTeXFiles; //bibtex files imported in the tex file

	QFormatFactory *m_formats;
	QLanguageFactory* m_languages;
	LatexCompleter* completer;

//gui
	QDockWidget *StructureView;
	QTabWidget *EditorView;
	QToolBox *StructureToolbox;
	MetapostListWidget *MpListWidget;
	PstricksListWidget *PsListWidget;
	SymbolListWidget *RelationListWidget, *ArrowListWidget, *MiscellaneousListWidget, *DelimitersListWidget, *GreekListWidget, *MostUsedListWidget;
	QTreeWidget *StructureTreeWidget;

	OutputViewWidget *outputView; //contains output widgets (over OutputLayout)
	

//toolbars
//
	QToolBar *fileToolBar, *editToolBar, *runToolBar, *formatToolBar, *mathToolBar;
	QAction *ToggleAct, *ToggleRememberAct;

	QLabel *stat1, *stat2, *stat3;
	QString MasterName,persistentMasterFile;
	
	QToolButton *combo1,*combo2,*combo3,*combo4,*combo5;

//settings
	ConfigManager configManager;
	BuildManager buildManager;
	int split1_right, split1_left, split2_top, split2_bottom;
	bool singlemode, wordwrap, parenmatch, showoutputview, showstructview, ams_packages, makeidx_package, autoindent;
	int showlinemultiples;
	bool folding, showlinestate, showcursorstate, realtimespellchecking;
	QString document_class, typeface_size, paper_size, document_encoding, author;
	QString spell_dic, spell_ignored_words;
	QStringList struct_level;
	QString thesaurus_database;
	QStringList userClassList, userPaperList, userEncodingList, userOptionsList;
	QStringList labelitem;
	Userlist UserMenuName, UserMenuTag;
	UserCd UserToolName, UserToolCommand;

	int spellcheckErrorFormat;
	SpellerUtility *mainSpeller;

	ThesaurusDialog *thesaurusDialog;
//dialogs
	TextAnalysisDialog *textAnalysisDlg;
	SpellerDialog *spellDlg;

	QStringList userCommandList;

//tools
	bool FINPROCESS, ERRPROCESS;

	SymbolList symbolScore;
	usercodelist symbolMostused;

	LatexEditorView *currentEditorView() const;
	void configureNewEditorView(LatexEditorView *edit);
	void updateEditorSetting(LatexEditorView *edit);
	LatexEditorView* getEditorFromFileName(const QString &fileName);
	
	QAction* outputViewAction;
private slots:

	void fileNew(QString fileName="untitled");
	void fileOpen();
	void fileSave();
	void fileSaveAll();
	void fileSaveAs(QString fileName = "");
	void fileClose();
	void fileCloseAll();
	void fileExit();
	void fileOpenRecent();
	void fileOpenRecentProject();
	void MarkCurrentFileAsRecent();
	void filePrint();

	void editUndo();
	void editRedo();
	void editCut();
	void editCopy();
	void editPaste();
	void editSectionCopy();
	void editSectionCopy(int startingLine, int endLine);
	void editSectionCut();
	void editSectionCut(int startingLine, int endLine);
	void editSectionPasteAfter();
	void editSectionPasteAfter(int line);
	void editSectionPasteBefore();
	void editSectionPasteBefore(int line);
	void editPasteLatex();
	void convertToLatex();
	void editPasteRef();
	void editIndentSection();
	void editUnIndentSection();
	void editSelectAll();
	void editEraseLine();
	void editFind();
	void editFindNext();
	void editReplace();
	void editGotoLine();
	void editJumpToLastChange();
	void editJumpToLastChangeForward();
	void editComment();
	void editUncomment();
	void editIndent();
	void editUnindent();
	void editSpell();
	void editThesaurus();
	void editChangeLineEnding();
	void editSetupEncoding();

	void StructureContextMenu(QPoint point);

	void ReadSettings();
	void SaveSettings();

	void lineMarkToolTip(int line, int mark);
	void NewDocumentStatus(bool m);
	void UpdateCaption();
	void CloseEditorTab(int tab);

	void updateStructure();
	void updateStructureForFile(const QString& fileName);
	void ShowStructure();
	void ClickedOnStructure(QTreeWidgetItem *item,int);

	void NormalCompletion();
	void InsertEnvironmentCompletion();
	void InsertTextCompletion();
	void InsertTag(QString Entity, int dx, int dy);
	void InsertSymbol(QTableWidgetItem *item);
	void InsertMetaPost(QListWidgetItem *item);
	void InsertPstricks(QListWidgetItem *item);
	void InsertFromAction();
	void InsertWithSelectionFromString(const QString& text);
	void InsertFromString(const QString& text);
	void InsertBib();
	void InsertStruct();
	void InsertStructFromString(const QString& text);
	void InsertImage();
	void InsertInclude();
	void InsertInput();

	void InsertBib1();
	void InsertBib2();
	void InsertBib3();
	void InsertBib4();
	void InsertBib5();
	void InsertBib6();
	void InsertBib7();
	void InsertBib8();
	void InsertBib9();
	void InsertBib10();
	void InsertBib11();
	void InsertBib12();
	void InsertBib13();
	void CleanBib();

	void InsertUserTag();
	void EditUserMenu();

	void SectionCommand();
	void OtherCommand();
	void InsertRef();
	void InsertPageRef();
	void SizeCommand();
	void LeftDelimiter();
	void RightDelimiter();

	void QuickTabular();
	void QuickArray();
	void QuickTabbing();
	void QuickLetter();
	void QuickDocument();

	void runCommand(QString comd,bool waitendprocess,bool showStdout,QString fn="");
	void runCommand(BuildManager::LatexCommand cmd,bool waitendprocess,bool showStdout,QString fn="");
	void RunPreCompileCommand();
	void readFromStderr();
	void readFromStdoutput();
	void SlotEndProcess(int err);
	void QuickBuild();
	void CleanAll();
	void commandFromAction();  //calls a command given by sender.data, doesn't wait
	void UserTool();
	void EditUserTool();

	void WebPublish();
	void AnalyseText();
	void AnalyseTextFormDestroyed();
	
	void RealViewLog();
	void ViewLog();
	void DisplayLatexError();
	void NextMark();
	void PreviousMark();
	void gotoNearLogEntry(LogType lt, bool backward, QString notFoundMessage);
	void NextError();
	void PreviousError();
	void NextWarning();
	void PreviousWarning();
	void NextBadBox();
	void PreviousBadBox();
	bool NoLatexErrors();
	bool LogExists();
/////
	void LatexHelp();
	void UserManualHelp();
	void HelpAbout();

	void GeneralOptions();
	void ToggleMode();

	void gotoNextDocument();
	void gotoPrevDocument();
	
	void viewCollapseEverything();
	void viewCollapseLevel();
	void viewCollapseBlock();
	void viewExpandEverything();
	void viewExpandLevel();
	void viewExpandBlock();

//void ToggleMode();

	void gotoBookmark();
	void toggleBookmark();

	void SetMostUsedSymbols();

	void updateBibFiles();
	void updateCompleter();

	void tabChanged(int i);

	void gotoLine(int line);  //0 based
	void gotoLocation(int line, const QString &fileName);  //0 based, absolute file name
	void gotoLogEntryEditorOnly(int logEntryNumber);
	void gotoLogEntryAt(int newLineNumber);
	void gotoMark(bool backward, int id);

	void lineHandleDeleted(QDocumentLineHandle* l);
	
	void previewLatex();
	void previewAvailable(const QString& imageFile, const QString& text);

	void escAction();
protected:
	LatexEditorView* getEditorFromStructureItem(QTreeWidgetItem* m_item);
	QPoint sectionSelection(QTreeWidgetItem* m_item);
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);
	bool eventFilter(QObject *obj, QEvent *event);
	virtual void changeEvent(QEvent *e);
public:
	static const int structureTreeLineColumn;
};

#endif
