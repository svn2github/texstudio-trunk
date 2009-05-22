/***************************************************************************
 *   copyright       : (C) 2003-2008 by Pascal Brachet                     *
 *   addons by Luis Silvestre                                              *
 *   http://www.xm1math.net/texmaker/                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation  either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
//#include <stdlib.h>

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QClipboard>
#include <QStatusBar>
#include <QSettings>
#include <QApplication>
#include <QDesktopWidget>
#include <QTextCodec>
#include <QFileInfo>
#include <QLabel>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QLineEdit>
#include <QProcess>
#include <QComboBox>
#include <QDomNode>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QCheckBox>
#include <QLocale>
#include <QTabWidget>
#include <QToolTip>
#include <QStyleFactory>
#include <QStyle>
#include <QFontDatabase>
#include <QTextBlock>
#include <QDebug>
#include <QDesktopServices>
#include <QAbstractItemModel>
#include <QFrame>
#include <QFontMetrics>
#include <QCloseEvent>

#include "texmaker.h"
#include "latexeditorview.h"

#include "smallUsefulFunctions.h"

#include "structdialog.h"
#include "filechooser.h"
#include "tabdialog.h"
#include "arraydialog.h"
#include "tabbingdialog.h"
#include "letterdialog.h"
#include "quickdocumentdialog.h"
#include "usermenudialog.h"
#include "usertooldialog.h"
#include "refdialog.h"
#include "aboutdialog.h"
#include "configdialog.h"
#include "encodingdialog.h"
#include "webpublishdialog.h"

#include "qdocument.h"
#include "qdocumentcursor.h"
#include "qdocumentline.h"
#include "qdocumentline_p.h"

const int Texmaker::structureTreeLineColumn=4;

Texmaker::Texmaker(QWidget *parent, Qt::WFlags flags)
		: QMainWindow(parent, flags), textAnalysisDlg(0), spellDlg(0) {
	ReadSettings();
	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

#ifdef Q_WS_MACX
	setWindowIcon(QIcon(":/images/logo128.png"));
#else
	setWindowIcon(QIcon(":/images/appicon.png"));
#endif

	setIconSize(QSize(22,22));


// PANNEAU STRUCTURE
	StructureView = new QDockWidget(this);
	StructureView->setObjectName("StructureView");
	StructureView->setAllowedAreas(Qt::AllDockWidgetAreas);
	StructureView->setFeatures(QDockWidget::DockWidgetClosable);
	StructureView->setWindowTitle(tr("Structure"));
	addDockWidget(Qt::LeftDockWidgetArea, StructureView);
	StructureToolbox=new QToolBox(StructureView);
	StructureView->setWidget(StructureToolbox);

	StructureTreeWidget=new QTreeWidget(StructureToolbox);
	StructureTreeWidget->setColumnCount(1);
	StructureTreeWidget->header()->hide();
	StructureTreeWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
//StructureTreeWidget->setToolTip(tr("Click to jump to the line"));
	connect(StructureTreeWidget, SIGNAL(itemPressed(QTreeWidgetItem *,int)), SLOT(ClickedOnStructure(QTreeWidgetItem *,int)));
// connect( StructureTreeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *,int )), SLOT(DoubleClickedOnStructure(QTreeWidgetItem *,int))); // qt4 bugs - don't use it
	StructureToolbox->addItem(StructureTreeWidget,QIcon(":/images/structure.png"),tr("Structure"));

	RelationListWidget=new SymbolListWidget(StructureToolbox,0);
	connect(RelationListWidget, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(InsertSymbol(QTableWidgetItem*)));
	StructureToolbox->addItem(RelationListWidget,QIcon(":/images/math1.png"),tr("Relation symbols"));

	ArrowListWidget=new SymbolListWidget(StructureToolbox,1);
	connect(ArrowListWidget, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(InsertSymbol(QTableWidgetItem*)));
	StructureToolbox->addItem(ArrowListWidget,QIcon(":/images/math2.png"),tr("Arrow symbols"));

	MiscellaneousListWidget=new SymbolListWidget(StructureToolbox,2);
	connect(MiscellaneousListWidget, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(InsertSymbol(QTableWidgetItem*)));
	StructureToolbox->addItem(MiscellaneousListWidget,QIcon(":/images/math3.png"),tr("Miscellaneous symbols"));

	DelimitersListWidget=new SymbolListWidget(StructureToolbox,3);
	connect(DelimitersListWidget, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(InsertSymbol(QTableWidgetItem*)));
	StructureToolbox->addItem(DelimitersListWidget,QIcon(":/images/math4.png"),tr("Delimiters"));

	GreekListWidget=new SymbolListWidget(StructureToolbox,4);
	connect(GreekListWidget, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(InsertSymbol(QTableWidgetItem*)));
	StructureToolbox->addItem(GreekListWidget,QIcon(":/images/math5.png"),tr("Greek letters"));

	MostUsedListWidget=new SymbolListWidget(StructureToolbox,5);
	connect(MostUsedListWidget, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(InsertSymbol(QTableWidgetItem*)));
	StructureToolbox->addItem(MostUsedListWidget,QIcon(":/images/math6.png"),tr("Most used symbols"));
	SetMostUsedSymbols();

	PsListWidget=new PstricksListWidget(StructureToolbox);
	connect(PsListWidget, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(InsertPstricks(QListWidgetItem*)));
	StructureToolbox->addItem(PsListWidget,QIcon(":/images/pstricks.png"),tr("Pstricks Commands"));

	MpListWidget=new MetapostListWidget(StructureToolbox);
	connect(MpListWidget, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(InsertMetaPost(QListWidgetItem*)));
	StructureToolbox->addItem(MpListWidget,QIcon(":/images/metapost.png"),tr("MetaPost Commands"));

	mainSpeller=new SpellerUtility();;
	mainSpeller->loadDictionary(spell_dic,configManager.configFileNameBase);
	mainSpeller->setActive(realtimespellchecking);

	LatexEditorView::setSpeller(mainSpeller);



	QDocument::setFormatFactory(m_formats);
	mainSpeller->spellcheckErrorFormat=m_formats->id("spellingMistake");

	QDocument::setShowSpaces(QDocument::ShowTrailing | QDocument::ShowLeading | QDocument::ShowTabs);


	QString qxsPath=QFileInfo(findResourceFile("qxs/tex.qnfa")).path();
	m_languages = new QLanguageFactory(m_formats, this);
	m_languages->addDefinitionPath(qxsPath);
	QLineMarksInfoCenter::instance()->loadMarkTypes(qxsPath+"/marks.qxm");


// OUTPUT WIDGETS
	OutputView = new QDockWidget(this);
	OutputView->setObjectName("OutputView");
	OutputView->setAllowedAreas(Qt::AllDockWidgetAreas);
	OutputView->setFeatures(QDockWidget::DockWidgetClosable);
	OutputView->setWindowTitle(tr("Messages / Log File"));
	addDockWidget(Qt::BottomDockWidgetArea,OutputView);
	connect(OutputView,SIGNAL(visibilityChanged(bool)),this,SLOT(OutputViewVisibilityChanged(bool)));


	logModel = new LatexLogModel(this);//needs loaded line marks

	OutputTable= new QTableView(this);
	OutputTable2= new QTableView(this); // second table view for tab log view
	QFontMetrics fm(qApp->font());
	for (int i=0;i<2;i++){ //setup tables
		QTableView* table=(i==0)?OutputTable:OutputTable2;
		table->setModel(logModel);

		table->setSelectionBehavior(QAbstractItemView::SelectRows);
		table->setSelectionMode(QAbstractItemView::SingleSelection);
		table->setColumnWidth(0,fm.width("> "));
		table->setColumnWidth(1,20*fm.width("w"));
		table->setColumnWidth(2,fm.width("WarningW"));
		table->setColumnWidth(3,fm.width("Line WWWWW"));
		table->setColumnWidth(4,20*fm.width("w"));
		connect(table, SIGNAL(clicked(const QModelIndex &)), this, SLOT(ClickedOnLogLine(const QModelIndex &)));
	
		table->horizontalHeader()->setStretchLastSection(true);
		table->setMinimumHeight(5*(fm.lineSpacing()+4));
	}

	OutputTextEdit = new LogEditor(this);
	OutputTextEdit->setFocusPolicy(Qt::ClickFocus);
	OutputTextEdit->setMinimumHeight(3*(fm.lineSpacing()+4));
	OutputTextEdit->setReadOnly(true);
	connect(OutputTextEdit, SIGNAL(clickonline(int)),this,SLOT(gotoLine(int)));

	OutputLogTextEdit = new LogEditor(this);
	OutputLogTextEdit->setFocusPolicy(Qt::ClickFocus);
	OutputLogTextEdit->setMinimumHeight(3*(fm.lineSpacing()+4));
	OutputLogTextEdit->setReadOnly(true);
	connect(OutputLogTextEdit, SIGNAL(clickonline(int)),this,SLOT(gotoLine(int)));

	OutputLayout= new QStackedWidget;

	QVBoxLayout* OutputVLayout= new QVBoxLayout; //contains the widgets for the normal mode (OutputTable + OutputLogTextEdit)
	OutputVLayout->setSpacing(0);
	OutputVLayout->setMargin(0);

	// add widget to log view
	OutputLayout->addWidget(OutputTextEdit);

	OutputVLayout->addWidget(OutputTable);
	OutputVLayout->addWidget(OutputLogTextEdit);
	QWidget* tempWidget=new QWidget (this);
	tempWidget->setLayout(OutputVLayout);
	OutputLayout->addWidget(tempWidget);

	if (tabbedLogView) {
		OutputTable->hide();
	}
	OutputLayout->addWidget(OutputTable2);

	// order for tabbar
	logViewerTabBar=new QTabBar(this);
	logViewerTabBar->addTab(tr("messages"));
	logViewerTabBar->addTab(tr("log file"));
	logViewerTabBar->addTab(tr("errors"));

	OutputView->setWidget(OutputLayout);

	if (tabbedLogView) {
		OutputView->setTitleBarWidget(logViewerTabBar);
	} else {
		logViewerTabBar->hide();
	}

	connect(logViewerTabBar, SIGNAL(currentChanged(int)),
	        OutputLayout, SLOT(setCurrentIndex(int)));

	connect(logViewerTabBar, SIGNAL(currentChanged(int)),
	        this, SLOT(tabChanged(int)));

	logpresent=false;

// TAB WIDGET EDITEUR
	EditorView=new QTabWidget(this);
	EditorView->setFocusPolicy(Qt::ClickFocus);
	EditorView->setFocus();
	connect(EditorView, SIGNAL(currentChanged(QWidget *)), this, SLOT(UpdateCaption()));
#if QT_VERSION >= 0x040500
	EditorView->setTabsClosable(true);
	connect(EditorView, SIGNAL(tabCloseRequested(int)), this, SLOT(CloseEditorTab(int)));
#endif
	setCentralWidget(EditorView);
	
	setupMenus();
	configManager.updateRecentFiles(true);
	setupToolBars();

	restoreState(windowstate, 0);


	createStatusBar();
	UpdateCaption();
	singlemode=true;
	MasterName=getName();

	show();

	stat1->setText(QString(" %1 ").arg(tr("Normal Mode")));
	stat2->setText(QString(" %1 ").arg(tr("Ready")));
//connect(stat3, SIGNAL(itemClicked ( QTableWidgetItem*)), this, SLOT(InsertSymbol(QTableWidgetItem*)));


	setAcceptDrops(true);


	completer=new LatexCompleter(this);
	updateCompleter();
	LatexEditorView::setCompleter(completer);

	if (!configManager.sessionFilesToRestore.empty()) {
		for (int i=0; i<configManager.sessionFilesToRestore.size(); i++)
			load(configManager.sessionFilesToRestore[i], configManager.sessionFilesToRestore[i]==configManager.sessionMaster);
		FileAlreadyOpen(configManager.sessionCurrent);
		configManager.sessionFilesToRestore.clear(); //save memory ;-)
		ToggleRememberAct->setChecked(true);
	}
}

QMenu* Texmaker::newManagedMenu(QMenu* menu, const QString &id,const QString &text){
	return configManager.newManagedMenu(menu,id,text);
}
QAction* Texmaker::newManagedAction(QWidget* menu, const QString &id,const QString &text, const char* slotName, const QKeySequence &shortCut, const QString & iconFile) {
	return configManager.newManagedAction(menu,id,text,slotName,QList<QKeySequence>() << shortCut, iconFile);
}
QAction* Texmaker::newManagedAction(QWidget* menu, const QString &id,const QString &text, const char* slotName, const QList<QKeySequence> &shortCuts, const QString & iconFile) {
	return configManager.newManagedAction(menu,id,text,slotName,shortCuts, iconFile);
}
QAction* Texmaker::newManagedAction(QWidget* menu, const QString &id, QAction* act){
	return configManager.newManagedAction(menu,id,act);
}
QAction* Texmaker::getManagedAction(QString id) {
	return configManager.getManagedAction(id);
}
QMenu* Texmaker::newManagedMenu(const QString &id,const QString &text){
	return configManager.newManagedMenu(id,text);
}

void Texmaker::setupMenus() {
	configManager.menuParent=this;
	configManager.menuParentsBar=menuBar();
	
//file
	QMenu *menu=newManagedMenu("main/file",tr("&File"));
	newManagedAction(menu, "new",tr("New"), SLOT(fileNew()), Qt::CTRL+Qt::Key_N, ":/images/filenew.png");
	newManagedAction(menu, "open",tr("Open"), SLOT(fileOpen()), Qt::CTRL+Qt::Key_O, ":/images/fileopen.png");

	QMenu *submenu=newManagedMenu(menu, "openrecent",tr("Open Recent")); //only create the menu here, actions are created by config manager
	
	menu->addSeparator();
	newManagedAction(menu,"save",tr("Save"), SLOT(fileSave()), Qt::CTRL+Qt::Key_S, ":/images/filesave.png");
	newManagedAction(menu,"saveas",tr("Save As"), SLOT(fileSaveAs()), Qt::CTRL+Qt::ALT+Qt::Key_S);
	newManagedAction(menu,"saveall",tr("Save All"), SLOT(fileSaveAll()), Qt::CTRL+Qt::SHIFT+Qt::Key_S);

	menu->addSeparator();
	newManagedAction(menu,"close",tr("Close"), SLOT(fileClose()), Qt::CTRL+Qt::Key_W, ":/images/fileclose.png");
	newManagedAction(menu,"closeall",tr("Close All"), SLOT(fileCloseAll()));

	menu->addSeparator();
	newManagedAction(menu, "print",tr("Print"), SLOT(filePrint()), Qt::CTRL+Qt::Key_P);

	menu->addSeparator();
	newManagedAction(menu,"exit",tr("Exit"), SLOT(fileExit()), Qt::CTRL+Qt::Key_Q);

//edit

	menu=newManagedMenu("main/edit",tr("&Edit"));
	newManagedAction(menu, "undo",tr("Undo"), SLOT(editUndo()), Qt::CTRL+Qt::Key_Z, ":/images/undo.png");
	newManagedAction(menu, "redo",tr("Redo"), SLOT(editRedo()), Qt::CTRL+Qt::Key_Y, ":/images/redo.png");

	menu->addSeparator();
	newManagedAction(menu,"copy",tr("Copy"), SLOT(editCopy()), (QList<QKeySequence>()<< Qt::CTRL+Qt::Key_C)<<Qt::CTRL+Qt::Key_Insert, ":/images/editcopy.png");
	newManagedAction(menu,"cut",tr("Cut"), SLOT(editCut()), (QList<QKeySequence>()<< Qt::CTRL+Qt::Key_X)<<Qt::SHIFT+Qt::Key_Delete, ":/images/editcut.png");
	newManagedAction(menu,"paste",tr("Paste"), SLOT(editPaste()), (QList<QKeySequence>()<< Qt::CTRL+Qt::Key_V)<<Qt::SHIFT+Qt::Key_Insert, ":/images/editpaste.png");
	newManagedAction(menu,"selectall",tr("Select All"), SLOT(editSelectAll()), Qt::CTRL+Qt::Key_A);
	newManagedAction(menu,"eraseLine",tr("Erase Line"), SLOT(editEraseLine()), (QList<QKeySequence>()<< Qt::CTRL+Qt::Key_K));
	menu->addSeparator();
	newManagedAction(menu,"pasteAsLatex",tr("Paste as Latex"), SLOT(editPasteLatex()), Qt::CTRL+Qt::SHIFT+Qt::Key_V, ":/images/editpaste.png");
	newManagedAction(menu,"convertToLatex",tr("Convert to Latex"), SLOT(convertToLatex()));

	LatexEditorView::setBaseActions(menu->actions());

	menu->addSeparator();
	newManagedAction(menu,"comment", tr("Comment"), SLOT(editComment()));
	newManagedAction(menu,"uncomment",tr("Uncomment"), SLOT(editUncomment()));
	newManagedAction(menu,"indent",tr("Indent"), SLOT(editIndent()));
	newManagedAction(menu,"unindent",tr("Unindent"), SLOT(editUnindent()));

	menu->addSeparator();
	newManagedAction(menu,"find", tr("Find"), SLOT(editFind()), Qt::CTRL+Qt::Key_F);
	newManagedAction(menu,"findnext",tr("Find Next"), SLOT(editFindNext()), Qt::CTRL+Qt::Key_M);
	newManagedAction(menu,"replace",tr("Replace"), SLOT(editReplace()), Qt::CTRL+Qt::Key_R);

	menu->addSeparator();
	submenu=newManagedMenu(menu, "goto",tr("Go to"));
	newManagedAction(submenu,"line", tr("Line"), SLOT(editGotoLine()), Qt::CTRL+Qt::Key_G, ":/images/goto.png");
	newManagedAction(submenu,"lastchange",tr("last change"), SLOT(editJumpToLastChange()), Qt::CTRL+Qt::Key_H);
	newManagedAction(submenu,"nextchange",tr("\"next\" change"), SLOT(editJumpToLastChangeForward()), Qt::CTRL+Qt::SHIFT+Qt::Key_H);
	submenu->addSeparator();
	newManagedAction(submenu,"markprev",tr("Previous mark"),SLOT(PreviousMark()),Qt::CTRL+Qt::Key_Up);//, ":/images/errorprev.png");
	newManagedAction(submenu,"marknext",tr("Next mark"),SLOT(NextMark()),Qt::CTRL+Qt::Key_Down);//, ":/images/errornext.png");
	submenu->addSeparator();
	newManagedAction(submenu,"errorprev",tr("Previous error"),SLOT(PreviousError()),Qt::CTRL+Qt::SHIFT+Qt::Key_Up, ":/images/errorprev.png");
	newManagedAction(submenu,"errornext",tr("Next error"),SLOT(NextError()),Qt::CTRL+Qt::SHIFT+Qt::Key_Down, ":/images/errornext.png");
	newManagedAction(submenu,"warningprev",tr("Previous warning"),SLOT(PreviousWarning()),Qt::CTRL+Qt::ALT+Qt::Key_Up);//, ":/images/errorprev.png");
	newManagedAction(submenu,"warningnext",tr("Next warning"),SLOT(NextWarning()),Qt::CTRL+Qt::ALT+Qt::Key_Down);//, ":/images/errornext.png");
	newManagedAction(submenu,"badboxprev",tr("Previous bad box"),SLOT(PreviousBadBox()),Qt::SHIFT+Qt::ALT+Qt::Key_Up);//, ":/images/errorprev.png");
	newManagedAction(submenu,"badboxnext",tr("Next bad box"),SLOT(NextBadBox()),Qt::SHIFT+Qt::ALT+Qt::Key_Down);//, ":/images/errornext.png");

	submenu=newManagedMenu(menu, "gotoBookmark",tr("Goto Bookmark"));
	for (int i=0; i<=9; i++)
		newManagedAction(submenu,QString("bookmark%1").arg(i),tr("Bookmark %1").arg(i),SLOT(gotoBookmark()),Qt::CTRL+Qt::Key_0+i)
		->setData(i);

	submenu=newManagedMenu(menu, "toggleBookmark",tr("Toggle Bookmark"));
	newManagedAction(submenu,QString("bookmark"),tr("unnamed bookmark"),SLOT(toggleBookmark()),Qt::CTRL+Qt::SHIFT+Qt::Key_B)
	->setData(-1);
	for (int i=0; i<=9; i++)
		newManagedAction(submenu,QString("bookmark%1").arg(i),tr("Bookmark %1").arg(i),SLOT(toggleBookmark()),Qt::CTRL+Qt::SHIFT+Qt::Key_0+i)
		->setData(i);

	submenu=newManagedMenu(menu, "complete",tr("Complete"));
	newManagedAction(submenu, "normal", tr("normal"), SLOT(NormalCompletion()),Qt::CTRL+Qt::Key_Space);
	newManagedAction(submenu, "environment", tr("\\begin{ completion"), SLOT(InsertEnvironmentCompletion()),Qt::CTRL+Qt::ALT+Qt::Key_Space);
	newManagedAction(submenu, "text", tr("normal text"), SLOT(InsertTextCompletion()),Qt::SHIFT+Qt::ALT+Qt::Key_Space);

	menu->addSeparator();
	submenu=newManagedMenu(menu,"lineend",tr("Line Ending"));
	QActionGroup *lineEndingGroup=new QActionGroup(this);
	QAction* act=newManagedAction(submenu, "crlf", tr("DOS/Windows (CR LF)"), SLOT(editChangeLineEnding()));
	act->setData(QDocument::Windows);
	act->setCheckable(true);
	lineEndingGroup->addAction(act);
	act=newManagedAction(submenu, "lf", tr("Unix (LF)"), SLOT(editChangeLineEnding()));
	act->setData(QDocument::Unix);
	act->setCheckable(true);
	lineEndingGroup->addAction(act);
	act=newManagedAction(submenu, "cr", tr("Old Mac (CR)"), SLOT(editChangeLineEnding()));
	act->setData(QDocument::Mac);
	act->setCheckable(true);
	lineEndingGroup->addAction(act);


	newManagedAction(menu,"encoding",tr("Setup Encoding"),SLOT(editSetupEncoding()));


	menu->addSeparator();
	newManagedAction(menu,"spelling",tr("Check Spelling"),SLOT(editSpell()),Qt::CTRL+Qt::SHIFT+Qt::Key_F7);

	menu->addSeparator();
	newManagedAction(menu,"reparse",tr("Refresh Structure"),SLOT(UpdateStructure()));


//tools

	menu=newManagedMenu("main/tools",tr("&Tools"));
	newManagedAction(menu, "quickbuild",tr("Quick Build"), SLOT(QuickBuild()), Qt::Key_F1, ":/images/quick.png");

	menu->addSeparator();
	newManagedAction(menu, "latex",tr("LaTeX"), SLOT(Latex()), Qt::Key_F2, ":/images/latex.png");
	newManagedAction(menu, "viewdvi",tr("View Dvi"), SLOT(ViewDvi()), Qt::Key_F3, ":/images/viewdvi.png");
	newManagedAction(menu, "dvi2ps",tr("Dvi->PS"), SLOT(DviToPS()), Qt::Key_F4, ":/images/dvips.png");
	newManagedAction(menu, "viewps",tr("View PS"), SLOT(ViewPS()), Qt::Key_F5, ":/images/viewps.png");
	newManagedAction(menu, "pdflatex",tr("PDFLaTeX"), SLOT(PDFLatex()), Qt::Key_F6, ":/images/pdflatex.png");
	newManagedAction(menu, "viewpdf",tr("View PDF"), SLOT(ViewPDF()), Qt::Key_F7, ":/images/viewpdf.png");
	newManagedAction(menu, "ps2pdf",tr("PS->PDF"), SLOT(PStoPDF()), Qt::Key_F8, ":/images/ps2pdf.png");
	newManagedAction(menu, "dvipdf",tr("DVI->PDF"), SLOT(DVItoPDF()), Qt::Key_F9, ":/images/dvipdf.png");
	newManagedAction(menu, "viewlog",tr("View Log"), SLOT(RealViewLog()), Qt::Key_F10, ":/images/viewlog.png");
	newManagedAction(menu, "bibtex",tr("BibTeX"), SLOT(MakeBib()), Qt::Key_F11);
	newManagedAction(menu, "makeindex",tr("MakeIndex"), SLOT(MakeIndex()), Qt::Key_F12);

	menu->addSeparator();
	newManagedAction(menu, "metapost",tr("MetaPost"), SLOT(MetaPost()));
	menu->addSeparator();
	newManagedAction(menu, "clean",tr("Clean"), SLOT(CleanAll()));
	menu->addSeparator();
	newManagedAction(menu, "htmlexport",tr("Convert to Html"), SLOT(WebPublish()));
	menu->addSeparator();
	newManagedAction(menu, "analysetext",tr("Analyse Text"), SLOT(AnalyseText()));

//  Latex/Math external
	configManager.loadManagedMenus(":/uiconfig.xml");

//wizards

	menu=newManagedMenu("main/wizards",tr("&Wizards"));
	newManagedAction(menu, "start",tr("Quick Start"), SLOT(QuickDocument()));
	newManagedAction(menu, "letter",tr("Quick Letter"), SLOT(QuickLetter()));

	menu->addSeparator();
	newManagedAction(menu, "tabular",tr("Quick Tabular"), SLOT(QuickTabular()));
	newManagedAction(menu, "tabbing",tr("Quick Tabbing"), SLOT(QuickTabbing()));
	newManagedAction(menu, "array",tr("Quick Array"), SLOT(QuickArray()));

	menu=newManagedMenu("main/bibtex",tr("&Bibliography"));
	newManagedAction(menu, "jourarticle", tr("Article in Journal"), SLOT(InsertBib1()));
	newManagedAction(menu, "confarticle", tr("Article in Conference Proceedings"), SLOT(InsertBib2()));
	newManagedAction(menu, "collarticle", tr("Article in a collection"), SLOT(InsertBib3()));
	newManagedAction(menu, "bookpage", tr("Chapter or Pages in a Book"), SLOT(InsertBib4()));
	newManagedAction(menu, "conference", tr("Conference Proceedings"), SLOT(InsertBib5()));
	newManagedAction(menu, "book", tr("Book"), SLOT(InsertBib6()));
	newManagedAction(menu, "booklet", tr("Booklet"), SLOT(InsertBib7()));
	newManagedAction(menu, "phdthesis", tr("PhD. Thesis"), SLOT(InsertBib8()));
	newManagedAction(menu, "masterthesis", tr("Master's Thesis"), SLOT(InsertBib9()));
	newManagedAction(menu, "report", tr("Technical Report"), SLOT(InsertBib10()));
	newManagedAction(menu, "manual", tr("Technical Manual"), SLOT(InsertBib11()));
	newManagedAction(menu, "unpublished", tr("Unpublished"), SLOT(InsertBib12()));
	newManagedAction(menu, "misc", tr("Miscellaneous"), SLOT(InsertBib13()));
	menu->addSeparator();
	newManagedAction(menu, "clean", tr("Clean"), SLOT(CleanBib()));


//  User
	menu=newManagedMenu("main/user",tr("&User"));
	submenu=newManagedMenu(menu,"tags",tr("User &Tags"));
	for (int i=0; i<10; i++)
		newManagedAction(submenu, QString("tag%1").arg(i),QString("%1: %2").arg(i+1).arg(UserMenuName[i]), SLOT(InsertUserTag()), Qt::SHIFT+Qt::Key_F1+i)
		->setData(i);
	submenu->addSeparator();
	newManagedAction(submenu, QString("manage"),tr("Edit User &Tags"), SLOT(EditUserMenu()));

	submenu=newManagedMenu(menu,"commands",tr("User &Commands"));
	for (int i=0; i<5; i++)
		newManagedAction(submenu, QString("cmd%1").arg(i),QString("%1: %2").arg(i+1).arg(UserToolName[i]), SLOT(UserTool()), Qt::SHIFT+Qt::ALT+Qt::Key_F1+i)
		->setData(i);
	submenu->addSeparator();
	newManagedAction(submenu, QString("manage"),tr("Edit User &Commands"), SLOT(EditUserTool()));

//---view---
	menu=newManagedMenu("main/view",tr("&View"));
	newManagedAction(menu, "nextdocument",tr("Next Document"), SLOT(gotoNextDocument()), Qt::ALT+Qt::Key_PageUp);
	newManagedAction(menu, "prevdocument",tr("Previous Document"), SLOT(gotoPrevDocument()), Qt::ALT+Qt::Key_PageDown);

	menu->addSeparator();
	newManagedAction(menu, "structureview",StructureView->toggleViewAction());
	newManagedAction(menu, "outputview",OutputView->toggleViewAction());

	menu->addSeparator();
	submenu=newManagedMenu(menu, "collapse", tr("Collapse"));
	newManagedAction(submenu, "all", tr("Everything"), SLOT(viewCollapseEverything()));
	newManagedAction(submenu, "block", tr("Nearest block"), SLOT(viewCollapseBlock()));
	for (int i=1; i<=4; i++)
		newManagedAction(submenu, QString::number(i), tr("Level %1").arg(i), SLOT(viewCollapseLevel()))->setData(i);
	submenu=newManagedMenu(menu, "expand", tr("Expand"));
	newManagedAction(submenu, "all", tr("Everything"), SLOT(viewExpandEverything()));
	newManagedAction(submenu, "block", tr("Nearest block"), SLOT(viewExpandBlock()));
	for (int i=1; i<=4; i++)
		newManagedAction(submenu, QString::number(i), tr("Level %1").arg(i), SLOT(viewExpandLevel()))->setData(i);

//---options---
	menu=newManagedMenu("main/options",tr("&Options"));
	newManagedAction(menu, "config",tr("Configure TexMakerX"), SLOT(GeneralOptions()), 0,":/images/configure.png");
	
	menu->addSeparator();
	ToggleAct=newManagedAction(menu, "masterdocument",tr("Define Current Document as 'Master Document'"), SLOT(ToggleMode()));
	ToggleRememberAct=newManagedAction(menu, "remembersession",tr("Remember session when closing"));
	ToggleRememberAct->setCheckable(true);

//---help---
	menu=newManagedMenu("main/help",tr("&Help"));
	newManagedAction(menu, "latexreference",tr("LaTeX Reference"), SLOT(LatexHelp()), 0,":/images/help.png");
	newManagedAction(menu, "usermanual",tr("User Manual"), SLOT(UserManualHelp()), 0,":/images/help.png");

	menu->addSeparator();
	newManagedAction(menu, "appinfo",tr("About TexMakerX"), SLOT(HelpAbout()), 0,":/images/appicon.png");

//-----context menus-----
	StructureTreeWidget->setObjectName("StructureTree");
	//StructureTreeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
	StructureTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	newManagedAction(StructureTreeWidget,"CopySection",tr("Copy"), SLOT(editSectionCopy()));
	newManagedAction(StructureTreeWidget,"CutSection",tr("Cut"), SLOT(editSectionCut()));
	newManagedAction(StructureTreeWidget,"PasteBefore",tr("Paste before"), SLOT(editSectionPasteBefore()));
	newManagedAction(StructureTreeWidget,"PasteAfter",tr("Paste after"), SLOT(editSectionPasteAfter()));
	QAction* sep = new QAction(StructureTreeWidget);
	sep->setSeparator(true);
	StructureTreeWidget->addAction(sep);
	newManagedAction(StructureTreeWidget,"IndentSection",tr("Indent Section"), SLOT(editIndentSection()));
	newManagedAction(StructureTreeWidget,"UnIndentSection",tr("Unindent Section"), SLOT(editUnIndentSection()));
	connect(StructureTreeWidget,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(StructureContextMenu(QPoint)));

	configManager.modifyManagedShortcuts();
}

void Texmaker::setupToolBars() {
	QStringList list;
//file
	fileToolBar = addToolBar("File");
	fileToolBar->setObjectName("File");

	fileToolBar->addAction(getManagedAction("main/file/new"));
	fileToolBar->addAction(getManagedAction("main/file/open"));
	fileToolBar->addAction(getManagedAction("main/file/save"));
	fileToolBar->addAction(getManagedAction("main/file/close"));

//edit
	editToolBar = addToolBar("Edit");
	editToolBar->setObjectName("Edit");

	editToolBar->addAction(getManagedAction("main/edit/undo"));
	editToolBar->addAction(getManagedAction("main/edit/redo"));
	editToolBar->addAction(getManagedAction("main/edit/copy"));
	editToolBar->addAction(getManagedAction("main/edit/cut"));
	editToolBar->addAction(getManagedAction("main/edit/paste"));


//tools
	runToolBar = addToolBar("Tools");
	runToolBar->setObjectName("Tools");

	runToolBar->addAction(getManagedAction("main/tools/viewlog"));
	runToolBar->addAction(getManagedAction("main/edit/goto/errorprev"));
	runToolBar->addAction(getManagedAction("main/edit/goto/errornext"));

	runToolBar->addSeparator();
	runToolBar->addAction(getManagedAction("main/tools/quickbuild"));
	runToolBar->addAction(getManagedAction("main/tools/latex"));
	runToolBar->addAction(getManagedAction("main/tools/viewdvi"));
	runToolBar->addAction(getManagedAction("main/tools/dvi2ps"));
	runToolBar->addAction(getManagedAction("main/tools/viewps"));
	runToolBar->addAction(getManagedAction("main/tools/pdflatex"));
	runToolBar->addAction(getManagedAction("main/tools/viewpdf"));

//format
	formatToolBar = addToolBar("Format");
	formatToolBar->setObjectName("Format");
	insertToolBarBreak(formatToolBar);

	list.clear();
	list.append("part");
	list.append("chapter");
	list.append("section");
	list.append("subsection");
	list.append("subsubsection");
	list.append("paragraph");
	list.append("subparagraph");
	QFontMetrics fontMetrics(runToolBar->font());
	combo1=createComboToolButton(formatToolBar,list,runToolBar->height()-2,fontMetrics,this,SLOT(SectionCommand()));
	formatToolBar->addWidget(combo1);
	formatToolBar->addSeparator();

	list.clear();
	list.append("label");
	list.append("ref");
	list.append("pageref");
	list.append("index");
	list.append("cite");
	list.append("footnote");
	combo2=createComboToolButton(formatToolBar,list,runToolBar->height()-2,fontMetrics,this,SLOT(OtherCommand()));
	formatToolBar->addWidget(combo2);
	formatToolBar->addSeparator();

	list.clear();
	list.append("tiny");
	list.append("scriptsize");
	list.append("footnotesize");
	list.append("small");
	list.append("normalsize");
	list.append("large");
	list.append("Large");
	list.append("LARGE");
	list.append("huge");
	list.append("Huge");
	combo3=createComboToolButton(formatToolBar,list,runToolBar->height()-2,fontMetrics,this,SLOT(SizeCommand()));
	formatToolBar->addWidget(combo3);
	formatToolBar->addSeparator();

	formatToolBar->addAction(getManagedAction("main/latex/fontstyles/textbf"));
	formatToolBar->addAction(getManagedAction("main/latex/fontstyles/textit"));
	formatToolBar->addAction(getManagedAction("main/latex/fontstyles/underline"));
	formatToolBar->addAction(getManagedAction("main/latex/environment/flushleft"));
	formatToolBar->addAction(getManagedAction("main/latex/environment/center"));
	formatToolBar->addAction(getManagedAction("main/latex/environment/flushright"));
	formatToolBar->addSeparator();
	formatToolBar->addAction(getManagedAction("main/latex/spacing/newline"));

//math
	mathToolBar = addToolBar("Math");
	mathToolBar->setObjectName("Math");

	mathToolBar->addAction(getManagedAction("main/math/mathmode"));
	mathToolBar->addAction(getManagedAction("main/math/subscript"));
	mathToolBar->addAction(getManagedAction("main/math/superscript"));
	mathToolBar->addAction(getManagedAction("main/math/frac"));
	mathToolBar->addAction(getManagedAction("main/math/dfrac"));
	mathToolBar->addAction(getManagedAction("main/math/sqrt"));

	mathToolBar->addSeparator();

	list.clear();
	list.append("left (");
	list.append("left [");
	list.append("left {");
	list.append("left <");
	list.append("left )");
	list.append("left ]");
	list.append("left }");
	list.append("left >");
	list.append("left.");
	combo4=createComboToolButton(mathToolBar,list,runToolBar->height()-2,fontMetrics,this,SLOT(LeftDelimiter()));
	mathToolBar->addWidget(combo4);
	mathToolBar->addSeparator();

	list.clear();
	list.append("right )");
	list.append("right ]");
	list.append("right }");
	list.append("right >");
	list.append("right (");
	list.append("right [");
	list.append("right {");
	list.append("right <");
	list.append("right.");
	combo5=createComboToolButton(mathToolBar,list,runToolBar->height()-2,fontMetrics,this,SLOT(RightDelimiter()));
	mathToolBar->addWidget(combo5);

}


void Texmaker::createStatusBar() {
	QStatusBar * status=statusBar();
	stat1=new QLabel(status);
	stat2=new QLabel(status);
	stat3=new QLabel(status);
	status->addPermanentWidget(stat3,0);
	status->addPermanentWidget(stat2,0);
	status->addPermanentWidget(stat1,0);
//statCursor=new QLabel( status );
	for (int i=1; i<=3; i++) {
		QPushButton* pb=new QPushButton(QIcon(QString(":/images/bookmark%1.png").arg(i)),"",status);
		pb->setToolTip(tr("Click to jump to the bookmark"));
		connect(pb,SIGNAL(clicked()),getManagedAction(QString("main/edit/gotoBookmark/bookmark%1").arg(i)),SIGNAL(triggered()));
		pb->setMaximumSize(20,20);
		pb->setFlat(true);
		status->addPermanentWidget(pb,0);
	}
}

void Texmaker::UpdateCaption() {
	QString title;
	if (!currentEditorView())	{
		title="TexMakerX";
	} else {
		title="Document : "+getName();
		if (currentEditorView()->editor) {
			if (currentEditorView()->editor->getFileEncoding()) stat3->setText(currentEditorView()->editor->getFileEncoding()->name());
			else stat3->setText("unknown");
			QDocument::LineEnding le = currentEditorView()->editor->document()->lineEnding();
			if (le==QDocument::Conservative) le= currentEditorView()->editor->document()->originalLineEnding();
			switch (le) {
#ifdef Q_WS_WIN
			case QDocument::Local:
#endif
			case QDocument::Windows:
				getManagedAction("main/edit/lineend/crlf")->setChecked(true);
				break;
			case QDocument::Mac:
				getManagedAction("main/edit/lineend/cr")->setChecked(true);
				break;
			default:
				getManagedAction("main/edit/lineend/lf")->setChecked(true);
			}
			//input_encoding=current	EditorView()->editor->getEncoding();
		}
	}
	setWindowTitle(title);
	UpdateStructure();
	if (singlemode) {
		OutputTextEdit->clear();
		//logViewerTabBar->setCurrentIndex(0);
		//OutputTable->hide();
		logpresent=false;
	}
	QString finame=getName();
	if (finame!="untitled" && finame!="") configManager.lastDocument=finame;
}

void Texmaker::CloseEditorTab(int tab) {
	int cur=EditorView->currentIndex();
	int total=EditorView->count();
	EditorView->setCurrentIndex(tab);
	fileClose();
	if (cur>tab) cur--;//removing moves to left
	if (total!=EditorView->count() && cur!=tab)//if user clicks cancel stay in clicked editor
		EditorView->setCurrentIndex(cur);
}

void Texmaker::lineMarkToolTip(int line, int mark) {
	if (!currentEditorView())	return;
	if (line < 0 || line>=currentEditorView()->editor->document()->lines()) return;
	int errorMarkID = QLineMarksInfoCenter::instance()->markTypeId("error");
	int warningMarkID = QLineMarksInfoCenter::instance()->markTypeId("warning");
	int badboxMarkID = QLineMarksInfoCenter::instance()->markTypeId("badbox");
	if (mark != errorMarkID && mark != warningMarkID && mark != badboxMarkID) return;
	int error = currentEditorView()->lineToLogEntries.value(currentEditorView()->editor->document()->line(line).handle(),-1);
	if (error<0 || error >= logModel->count()) return;
	currentEditorView()->lineMarkPanel->setToolTipForTouchedMark(logModel->at(error).niceMessage());
}

void Texmaker::NewDocumentStatus(bool m) {
	if (!currentEditorView())	return;
	if (m) {
		EditorView->setTabIcon(EditorView->indexOf(currentEditorView()),QIcon(":/images/modified.png"));
		EditorView->setTabText(EditorView->indexOf(currentEditorView()),QFileInfo(getName()).fileName());
	} else {
		EditorView->setTabIcon(EditorView->indexOf(currentEditorView()),QIcon(":/images/empty.png"));
		EditorView->setTabText(EditorView->indexOf(currentEditorView()),QFileInfo(getName()).fileName());
	}
}

LatexEditorView *Texmaker::currentEditorView() const {
	return qobject_cast<LatexEditorView *>(EditorView->currentWidget());
}

void Texmaker::configureNewEditorView(LatexEditorView *edit) {
	edit->editor->document()->setLineEnding(QDocument::Local);
	m_languages->setLanguage(edit->codeeditor->editor(), ".tex");
	EditorView->setCurrentIndex(EditorView->indexOf(edit));

	edit->environmentFormat=m_formats->id("environment");

	connect(edit->editor, SIGNAL(contentModified(bool)), this, SLOT(NewDocumentStatus(bool)));
	connect(edit->lineMarkPanel, SIGNAL(toolTipRequested(int,int)),this,SLOT(lineMarkToolTip(int,int)));

	updateEditorSetting(edit);
}
void Texmaker::updateEditorSetting(LatexEditorView *edit) {
	edit->editor->setFont(configManager.editorFont);
	edit->editor->setLineWrapping(wordwrap);
	edit->editor->setFlag(QEditor::AutoIndent,autoindent);
	edit->lineMarkPanelAction->setChecked((showlinemultiples!=0) ||folding||showlinestate);
	edit->lineNumberPanelAction->setChecked(showlinemultiples!=0);
	edit->lineNumberPanel->setVerboseMode(showlinemultiples!=10);
	edit->lineFoldPanel->setChecked(folding);
	edit->lineChangePanel->setChecked(showlinestate);
	edit->statusPanel->setChecked(showcursorstate);
}
QString Texmaker::getName() {
	QString title;
	if (!currentEditorView())	{
		title="";
	} else {
		title=filenames[currentEditorView()];
	}
	return title;
}
QString Texmaker::getCurrentFileName() {
	if (!currentEditorView()) return "";
	return filenames[currentEditorView()];
}
QString Texmaker::getCompileFileName(){
	if (singlemode) return getCurrentFileName();
	else return MasterName;
}
bool Texmaker::FileAlreadyOpen(QString f) {
	FilesMap::Iterator it;
	QString fw32,funix,forig;
	for (it = filenames.begin(); it != filenames.end(); ++it) {
		forig=filenames[it.key()];
		fw32=filenames[it.key()];
		funix=filenames[it.key()];
		fw32.replace(QString("\\"),QString("/"));
		funix.replace(QString("/"),QString("\\"));
		if ((forig==f) || (fw32==f) || (funix==f)) {
			EditorView->setCurrentIndex(EditorView->indexOf(it.key()));
			return true;
		}
	}
	return false;
}
///////////////////FILE//////////////////////////////////////


LatexEditorView* Texmaker::load(const QString &f , bool asProject) {
	QString f_real=f;
#ifdef Q_WS_WIN
	QRegExp regcheck("/([a-zA-Z]:[/\\\\].*)");
	if (regcheck.exactMatch(f)) f_real=regcheck.cap(1);
#endif
	raise();

	if (FileAlreadyOpen(f_real)) {
		if (asProject) {
			if (singlemode) ToggleMode();
			else if (!singlemode && MasterName != f_real) {
				ToggleMode();
				ToggleMode();
			}
		}
		return 0;
	}

	if (!QFile::exists(f_real)) return 0;
	LatexEditorView *edit = new LatexEditorView(0);
	EditorView->addTab(edit, QFileInfo(f_real).fileName());
	configureNewEditorView(edit);

	QFile file(f_real);
	if (!file.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(this,tr("Error"), tr("You do not have read permission to this file."));
		return 0;
	}

	if (configManager.autodetectLoadedFile) edit->editor->load(f_real,0);
	else edit->editor->load(f_real,configManager.newfile_encoding);
	edit->editor->document()->setLineEnding(edit->editor->document()->originalLineEnding());

//filenames.replace( edit, f_real );
	filenames.remove(edit);
	filenames.insert(edit, f_real);
	edit->editor->setFocus();
	UpdateCaption();
	NewDocumentStatus(false);
	MarkCurrentFileAsRecent();
	ShowStructure();
	if (asProject) {
		if (singlemode) ToggleMode();
		else if (!singlemode && MasterName != f_real) {
			ToggleMode();
			ToggleMode();
		}
	}
	return edit;
}

void Texmaker::gotoLine(int line) {
	if (currentEditorView() && line>=0)	{
		currentEditorView()->editor->setCursorPosition(line,0);
		currentEditorView()->editor->ensureCursorVisible();
		currentEditorView()->editor->setFocus();
	}
}


void Texmaker::fileNew(QString fileName) {
	LatexEditorView *edit = new LatexEditorView(0);
	if (configManager.newfile_encoding)
		edit->editor->setFileEncoding(configManager.newfile_encoding);
	else
		edit->editor->setFileEncoding(QTextCodec::codecForName("utf-8"));

	EditorView->addTab(edit, fileName);
	configureNewEditorView(edit);

	filenames.remove(edit);
	filenames.insert(edit, fileName);

	UpdateCaption();
	NewDocumentStatus(false);

	edit->editor->setFocus();
}

void Texmaker::fileOpen() {
	QString currentDir=QDir::homePath();
	if (!configManager.lastDocument.isEmpty()) {
		QFileInfo fi(configManager.lastDocument);
		if (fi.exists() && fi.isReadable()) currentDir=fi.absolutePath();
	}
	QString fn = QFileDialog::getOpenFileName(this,tr("Open File"),currentDir,"TeX files (*.tex *.bib *.sty *.cls *.mp);;All files (*.*)");
	if (!fn.isEmpty()) load(fn);
}

void Texmaker::fileSave() {
	if (!currentEditorView())
		return;

	QString fn=getName();
	if (fn == "untitled")
		fileSaveAs();
	else if (! QFile::exists(fn))
		fileSaveAs(getName());
	else {
		/*QFile file( *filenames.find( currentEditorView() ) );
		if ( !file.open( QIODevice::WriteOnly ) )
			{
			QMessageBox::warning( this,tr("Error"),tr("The file could not be saved. Please check if you have write permission."));
			return;
			}*/
		currentEditorView()->editor->save();
		//currentEditorView()->editor->setModified(false);
		fn=getName();
		MarkCurrentFileAsRecent();
	}
	UpdateCaption();
}

void Texmaker::fileSaveAs(QString fileName) {
	if (!currentEditorView())
		return;

	// select a directory/filepath
	QString currentDir=QDir::homePath();
	if (fileName.isEmpty()) {
		if (!configManager.lastDocument.isEmpty()) {
			QFileInfo fi(configManager.lastDocument);
			if (fi.exists() && fi.isReadable())
				currentDir=fi.absolutePath();
		}
	} else {
		QFileInfo currentFile(fileName);
		if (currentFile.absoluteDir().exists())
			currentDir = fileName;
	}

	// get a file name
	QString fn = QFileDialog::getSaveFileName(this,tr("Save As"),currentDir,"TeX files (*.tex *.bib *.sty *.cls *.mp);;All files (*.*)");
	if (!fn.isEmpty()) {
		QFileInfo fic(fn);
		currentEditorView()->editor->setFileName(fn);
		filenames.remove(currentEditorView());
		filenames.insert(currentEditorView(), fn);

		// save file
		currentEditorView()->editor->save();
		MarkCurrentFileAsRecent();

		EditorView->setTabText(EditorView->indexOf(currentEditorView()),fic.fileName());
	}

	UpdateCaption();
}

void Texmaker::fileSaveAll() {
//LatexEditorView *temp = new LatexEditorView(EditorView,colorMath,colorCommand,colorKeyword);
//temp=currentEditorView();
	int currentIndex=EditorView->indexOf(currentEditorView());
	FilesMap::Iterator it;
	for (it = filenames.begin(); it != filenames.end(); ++it) {
		EditorView->setCurrentIndex(EditorView->indexOf(it.key()));
		fileSave();
	}
	EditorView->setCurrentIndex(currentIndex);
	UpdateCaption();
}

void Texmaker::fileClose() {
	if (!currentEditorView())	return;
	if (currentEditorView()->editor->isContentModified()) {
		switch (QMessageBox::warning(this, "TexMakerX",
		                             tr("The document contains unsaved work. "
		                                "Do you want to save it before closing?"),
		                             tr("Save and Close"), tr("Don't Save and Close"), tr("Cancel"),
		                             0,
		                             2)) {
		case 0:
			fileSave();
			filenames.remove(currentEditorView());
			delete currentEditorView();
			break;
		case 1:
			filenames.remove(currentEditorView());
			delete currentEditorView();
			break;
		case 2:
		default:
			return;
			break;
		}
	} else {
		filenames.remove(currentEditorView());
		delete currentEditorView();
	}
	UpdateCaption();
}

void Texmaker::fileCloseAll() {
	bool go=true;
	while (currentEditorView() && go) {
		if (currentEditorView()->editor->isContentModified()) {
			switch (QMessageBox::warning(this, "TexMakerX",
			                             tr("The document contains unsaved work. "
			                                "Do you want to save it before exiting?"),
			                             tr("Save and Close"), tr("Don't Save and Close"), tr("Cancel"),
			                             0,
			                             2)) {
			case 0:
				fileSave();
				filenames.remove(currentEditorView());
				delete currentEditorView();
				break;
			case 1:
				filenames.remove(currentEditorView());
				delete currentEditorView();
				break;
			case 2:
			default:
				go=false;
				return;
				break;
			}
		} else {
			filenames.remove(currentEditorView());
			delete currentEditorView();
		}
	}
	UpdateCaption();
}

void Texmaker::fileExit() {
	SaveSettings();
	bool accept=true;
	while (currentEditorView() && accept) {
		if (currentEditorView()->editor->isContentModified()) {
			switch (QMessageBox::warning(this, "TexMakerX",
			                             tr("The document contains unsaved work. "
			                                "Do you want to save it before exiting?"),
			                             tr("Save and Close"), tr("Don't Save and Close"),tr("Cancel"),
			                             0,
			                             2)) {
			case 0:
				fileSave();
				filenames.remove(currentEditorView());
				delete currentEditorView();
				break;
			case 1:
				filenames.remove(currentEditorView());
				delete currentEditorView();
				break;
			case 2:
			default:
				accept=false;
				break;
			}

		} else {
			filenames.remove(currentEditorView());
			delete currentEditorView();
		}
	}
	if (accept) {
		if (mainSpeller) {
			delete mainSpeller; //this saves the ignore list
			mainSpeller=0;
		}
		qApp->quit();
	}
}

void Texmaker::closeEvent(QCloseEvent *e) {
	SaveSettings();
	bool accept=true;
	while (currentEditorView() && accept) {
		if (currentEditorView()->editor->isContentModified()) {
			switch (QMessageBox::warning(this, "TexMakerX",
			                             tr("The document contains unsaved work. "
			                                "Do you want to save it before exiting?"),
			                             tr("Save and Close"), tr("Don't Save and Close"), tr("Cancel"),
			                             0,
			                             2)) {
			case 0:
				fileSave();
				filenames.remove(currentEditorView());
				delete currentEditorView();
				break;
			case 1:
				filenames.remove(currentEditorView());
				delete currentEditorView();
				break;
			case 2:
			default:
				accept=false;
				break;
			}
		} else {
			filenames.remove(currentEditorView());
			delete currentEditorView();
		}
	}
	if (accept)  {
		if (mainSpeller) {
			delete mainSpeller; //this saves the ignore list
			mainSpeller=0;
		}
		e->accept();
	} else e->ignore();
}


void Texmaker::fileOpenRecent() {
	QAction *action = qobject_cast<QAction *>(sender());
	if (action) load(action->data().toString());
}

void Texmaker::fileOpenRecentProject() {
	QAction *action = qobject_cast<QAction *>(sender());
	if (action) load(action->data().toString(),true);
}

void Texmaker::MarkCurrentFileAsRecent(){
	configManager.addRecentFile(getCurrentFileName(),!singlemode && MasterName==getCurrentFileName());
}

void Texmaker::filePrint() {
	if (!currentEditorView()) return;
	currentEditorView()->editor->print();

}
//////////////////////////// EDIT ///////////////////////
void Texmaker::editUndo() {
	if (!currentEditorView()) return;
	currentEditorView()->editor->undo();
}

void Texmaker::editRedo() {
	if (!currentEditorView()) return;
	currentEditorView()->editor->redo();
}

void Texmaker::editCut() {
	if (!currentEditorView()) return;
	currentEditorView()->editor->cut();
}

void Texmaker::editCopy() {
	if (!currentEditorView()) return;
	currentEditorView()->editor->copy();
}

void Texmaker::editPaste() {
	if (!currentEditorView()) return;
	currentEditorView()->editor->paste();
}

void Texmaker::editPasteLatex() {
	if (!currentEditorView()) return;
// manipulate clipboard text
	QClipboard *clipboard = QApplication::clipboard();
	QString originalText = clipboard->text();
	QString newText=textToLatex(originalText);
	clipboard->setText(newText);
// insert
	currentEditorView()->editor->paste();
}

void Texmaker::convertToLatex() {
	if (!currentEditorView()) return;
// get selection and change it
	QDocumentCursor c = currentEditorView()->editor->cursor();
	QString originalText = c.selectedText();
	QString newText=textToLatex(originalText);
// insert
	c.insertText(newText);
}

void Texmaker::editSelectAll() {
	if (!currentEditorView()) return;
	currentEditorView()->editor->selectAll();
}

void Texmaker::editEraseLine() {
	if (!currentEditorView()) return;
	QDocumentCursor c = currentEditorView()->editor->cursor();
	c.eraseLine();
}

void Texmaker::editFind() {
	if (!currentEditorView())	return;
	currentEditorView()->editor->find();
}

void Texmaker::editFindNext() {
	if (!currentEditorView())	return;
	currentEditorView()->editor->findNext();
}


void Texmaker::editReplace() {
	if (!currentEditorView())	return;
	currentEditorView()->editor->replace();
}

void Texmaker::editGotoLine() {
	if (!currentEditorView())	return;
	currentEditorView()->editor->gotoLine();
}

void Texmaker::editJumpToLastChange() {
	if (!currentEditorView())	return;
	currentEditorView()->jumpChangePositionBackward();
}
void Texmaker::editJumpToLastChangeForward() {
	if (!currentEditorView())	return;
	currentEditorView()->jumpChangePositionForward();
}

void Texmaker::editComment() {
	if (!currentEditorView())	return;
	currentEditorView()->editor->commentSelection();
}

void Texmaker::editUncomment() {
	if (!currentEditorView())	return;
	currentEditorView()->editor->uncommentSelection();
}

void Texmaker::editIndent() {
	if (!currentEditorView())	return;
	currentEditorView()->editor->indentSelection();
}

void Texmaker::editUnindent() {
	if (!currentEditorView())	return;
	currentEditorView()->editor->unindentSelection();
}

void Texmaker::editSpell() {
	if (!currentEditorView()) {
		QMessageBox::information(this,"TexMakerX",tr("No document open"),0);
		return;
	}
	if (!spellDlg) spellDlg=new SpellerDialog(this,mainSpeller);
	spellDlg->setEditorView(currentEditorView());
	spellDlg->startSpelling();
}

void Texmaker::editChangeLineEnding() {
	if (!currentEditorView()) return;
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	currentEditorView()->editor->document()->setLineEnding(QDocument::LineEnding(action->data().toInt()));
	UpdateCaption();
}
void Texmaker::editSetupEncoding() {
	if (!currentEditorView()) return;
	EncodingDialog enc(this,currentEditorView()->editor);
	enc.exec();
	UpdateCaption();
}

QPoint Texmaker::sectionSelection(QTreeWidgetItem* m_item) {
	// called by action
        QDocumentLine mLine(m_item->data(structureTreeLineColumn,Qt::UserRole).value<QDocumentLineHandle*>());
        int l=mLine.lineNumber()+1;
	// find next section or higher
	QTreeWidgetItem* m_parent;
	int m_index;
	do {
		m_parent=m_item->parent();
		if (m_parent) {
			m_index=m_parent->indexOfChild(m_item);
			m_item=m_parent;
		} else m_index=-1;
	} while ((m_index>=0)&&(m_index>=m_parent->childCount()-1)&&(m_parent->text(1)!="part"));
	if (m_index>=0&&m_index<m_parent->childCount()-1) {
		m_item=m_parent->child(m_index+1);
                QDocumentLine mLine(m_item->data(structureTreeLineColumn,Qt::UserRole).value<QDocumentLineHandle*>());
                int m_endingLine=mLine.lineNumber();
                return QPoint(l,m_endingLine);
	} else {
		if (!currentEditorView()) return QPoint();
		// no ending section but end of document
		int m_endingLine=currentEditorView()->editor->document()->findLineContaining("\\end{document}",l,Qt::CaseInsensitive);
		if (m_endingLine<0) m_endingLine=currentEditorView()->editor->document()->lines();
		return QPoint(l,m_endingLine);
	}
}

void Texmaker::editIndentSection() {
	if (!currentEditorView()) return;
	// replace list
	QStringList m_replace;
	m_replace << "\\subparagraph" << "\\paragraph" << "\\subsubsection" << "\\subsection" << "\\section" << "\\chapter";
	// replace sections
	QPoint m_point=sectionSelection(StructureTreeWidget->currentItem());
	QString m_line;
	QDocumentCursor m_cursor=currentEditorView()->editor->cursor();
	for (int l=m_point.x(); l<=m_point.y(); l++) {
		currentEditorView()->editor->setCursorPosition(l-1,0);
		m_cursor=currentEditorView()->editor->cursor();
		m_line=currentEditorView()->editor->cursor().line().text();
		QString m_old="";
		foreach(QString elem,m_replace) {
			if (m_old!="") m_line.replace(elem,m_old);
			m_old=elem;
		}

		m_cursor.movePosition(1,QDocumentCursor::EndOfLine, QDocumentCursor::KeepAnchor);
		currentEditorView()->editor->setCursor(m_cursor);
		currentEditorView()->editor->cursor().insertText(m_line);
	}

}

void Texmaker::editUnIndentSection() {
	if (!currentEditorView()) return;

	QStringList m_replace;

	m_replace << "\\chapter" << "\\section" << "\\subsection" << "\\subsubsection" << "\\paragraph" << "\\subparagraph" ;

	// replace sections
	QPoint m_point=sectionSelection(StructureTreeWidget->currentItem());
	QString m_line;
	QDocumentCursor m_cursor=currentEditorView()->editor->cursor();
	for (int l=m_point.x(); l<=m_point.y(); l++) {
		currentEditorView()->editor->setCursorPosition(l-1,0);
		m_cursor=currentEditorView()->editor->cursor();
		m_line=currentEditorView()->editor->cursor().line().text();
		QString m_old="";
		foreach(QString elem,m_replace) {
			if (m_old!="") m_line.replace(elem,m_old);
			m_old=elem;
		}

		m_cursor.movePosition(1,QDocumentCursor::EndOfLine, QDocumentCursor::KeepAnchor);
		currentEditorView()->editor->setCursor(m_cursor);
		currentEditorView()->editor->cursor().insertText(m_line);
	}

}

void Texmaker::editSectionCopy() {
	// called by action
	QPoint m_point=sectionSelection(StructureTreeWidget->currentItem());
	editSectionCopy(m_point.x(),m_point.y());
}

void Texmaker::editSectionCut() {
	// called by action
	QPoint m_point=sectionSelection(StructureTreeWidget->currentItem());
	editSectionCut(m_point.x(),m_point.y());
	//UpdateStructure();
}

void Texmaker::editSectionCopy(int startingLine, int endLine) {
	if (!currentEditorView()) return;

	currentEditorView()->editor->setCursorPosition(startingLine-1,0);
	QDocumentCursor m_cursor=currentEditorView()->editor->cursor();
	//m_cursor.movePosition(1, QDocumentCursor::NextLine, QDocumentCursor::KeepAnchor);
	m_cursor.setSilent(true);
	m_cursor.movePosition(endLine-startingLine, QDocumentCursor::NextLine, QDocumentCursor::KeepAnchor);
	m_cursor.movePosition(0,QDocumentCursor::EndOfLine,QDocumentCursor::KeepAnchor);
	currentEditorView()->editor->setCursor(m_cursor);
	currentEditorView()->editor->copy();
}

void Texmaker::editSectionCut(int startingLine, int endLine) {
	if (!currentEditorView()) return;
	currentEditorView()->editor->setCursorPosition(startingLine-1,0);
	QDocumentCursor m_cursor=currentEditorView()->editor->cursor();
	m_cursor.setSilent(true);
	m_cursor.movePosition(endLine-startingLine, QDocumentCursor::NextLine, QDocumentCursor::KeepAnchor);
	m_cursor.movePosition(0,QDocumentCursor::EndOfLine,QDocumentCursor::KeepAnchor);
	currentEditorView()->editor->setCursor(m_cursor);
	currentEditorView()->editor->cut();
}

void Texmaker::editSectionPasteAfter() {
	if (!currentEditorView()) return;

	QPoint m_point=sectionSelection(StructureTreeWidget->currentItem());
	editSectionPasteAfter(m_point.y());
	//UpdateStructure();
}

void Texmaker::editSectionPasteBefore() {
	if (!currentEditorView()) return;

	QString m_text=StructureTreeWidget->currentItem()->text(2);
	bool okay;
	int l=m_text.toInt(&okay,10);

	editSectionPasteBefore(l-1);
	//UpdateStructure();
}

void Texmaker::editSectionPasteAfter(int line) {
	if (line>=currentEditorView()->editor->document()->lines()) {
		currentEditorView()->editor->setCursorPosition(line-1,0);
		QDocumentCursor c=currentEditorView()->editor->cursor();
		c.movePosition(1,QDocumentCursor::End,QDocumentCursor::MoveAnchor);
		currentEditorView()->editor->setCursor(c);
		currentEditorView()->editor->cursor().insertText("\n");
	} else {
		currentEditorView()->editor->setCursorPosition(line,0);
		currentEditorView()->editor->cursor().insertText("\n");
		currentEditorView()->editor->setCursorPosition(line,0);
	}
	editPaste();
	//UpdateStructure();
}

void Texmaker::editSectionPasteBefore(int line) {
	currentEditorView()->editor->setCursorPosition(line,0);
	currentEditorView()->editor->cursor().insertText("\n");
	currentEditorView()->editor->setCursorPosition(line,0);
	editPaste();
	//UpdateStructure();
}


/////////////// CONFIG ////////////////////
void Texmaker::ReadSettings() {
	QSettings *config=configManager.readSettings();

	config->beginGroup("texmaker");
	singlemode=true;

	QRect screen = QApplication::desktop()->screenGeometry();
	int w= config->value("Geometries/MainwindowWidth",screen.width()-100).toInt();
	int h= config->value("Geometries/MainwindowHeight",screen.height()-100).toInt() ;
	int x= config->value("Geometries/MainwindowX",10).toInt();
	int y= config->value("Geometries/MainwindowY",10).toInt() ;
	resize(w,h);
	move(x,y);
	windowstate=config->value("MainWindowState").toByteArray();

	wordwrap=config->value("Editor/WordWrap",true).toBool();
	parenmatch=config->value("Editor/Parentheses Matching",true).toBool();
	showlinemultiples=config->value("Editor/Line Number Multiples",-1).toInt();
	if (showlinemultiples==-1) {
		if (config->value("Editor/Line Numbers",true).toBool()) showlinemultiples=1;  //texmaker import
		else showlinemultiples=0;
	}

	completion=config->value("Editor/Completion",true).toBool();
	autoindent=config->value("Editor/Auto Indent",true).toBool();
	folding=config->value("Editor/Folding",true).toBool();
	showlinestate=config->value("Editor/Show Line State",true).toBool();
	showcursorstate=config->value("Editor/Show Cursor State",true).toBool();
	realtimespellchecking=config->value("Editor/Real-Time Spellchecking",true).toBool();



	showoutputview=config->value("Show/OutputView",true).toBool();
	showstructview=config->value("Show/Structureview",true).toBool();

	quickmode=config->value("Tools/Quick Mode",1).toInt();

	latex_command=BuildManager::lazyDefaultRead(*config,"Tools/Latex",BuildManager::CMD_LATEX);
	dvips_command=BuildManager::lazyDefaultRead(*config,"Tools/Dvips",BuildManager::CMD_DVIPS);
	ps2pdf_command=BuildManager::lazyDefaultRead(*config,"Tools/Ps2pdf",BuildManager::CMD_PS2PDF);
	makeindex_command=BuildManager::lazyDefaultRead(*config,"Tools/Makeindex",BuildManager::CMD_MAKEINDEX);
	bibtex_command=BuildManager::lazyDefaultRead(*config,"Tools/Bibtex",BuildManager::CMD_BIBTEX);
	pdflatex_command=BuildManager::lazyDefaultRead(*config,"Tools/Pdflatex",BuildManager::CMD_PDFLATEX);
	dvipdf_command=BuildManager::lazyDefaultRead(*config,"Tools/Dvipdf",BuildManager::CMD_DVIPDF);
	metapost_command=BuildManager::lazyDefaultRead(*config,"Tools/Metapost",BuildManager::CMD_METAPOST);
	viewdvi_command=BuildManager::lazyDefaultRead(*config,"Tools/Dvi",BuildManager::CMD_VIEWDVI);
	viewps_command=BuildManager::lazyDefaultRead(*config,"Tools/Ps",BuildManager::CMD_VIEWPS);
	viewpdf_command=BuildManager::lazyDefaultRead(*config,"Tools/Pdf",BuildManager::CMD_VIEWPDF);
	ghostscript_command=BuildManager::lazyDefaultRead(*config,"Tools/Ghostscript",BuildManager::CMD_GHOSTSCRIPT);
	precompile_command=config->value("Tools/Precompile","").toString();

	userquick_command=config->value("Tools/Userquick","latex -interaction=nonstopmode %.tex|bibtex %.aux|latex -interaction=nonstopmode %.tex|latex -interaction=nonstopmode %.tex|xdvi %.dvi").toString();
	userClassList=config->value("Tools/User Class").toStringList();
	userPaperList=config->value("Tools/User Paper").toStringList();
	userEncodingList=config->value("Tools/User Encoding").toStringList();
	userOptionsList=config->value("Tools/User Options").toStringList();

	for (int i=0; i<=9; i++) {
		UserMenuName[i]=config->value(QString("User/Menu%1").arg(i+1),"").toString();
		UserMenuTag[i]=config->value(QString("User/Tag%1").arg(i+1),"").toString();
	}
	for (int i=0; i<=4; i++) {
		UserToolName[i]=config->value(QString("User/ToolName%1").arg(i+1),"").toString();
		UserToolCommand[i]=config->value(QString("User/Tool%1").arg(i+1),"").toString();
	}

        struct_level.clear();
        if(config->value("Structure/Structure Level 1","").toString()!=""){
            struct_level << "part" << "chapter" << "section" << "subsection" << "subsubsection";
        }else{
            int i=0;
            QString elem;
            while((elem=config->value("Structure/Structure Level "+QString::number(i+1),"").toString())!=""){
                struct_level << elem;
                i++;
            }
        }

	document_class=config->value("Quick/Class","article").toString();
	typeface_size=config->value("Quick/Typeface","10pt").toString();
	paper_size=config->value("Quick/Papersize","a4paper").toString();
	document_encoding=config->value("Quick/Encoding","latin1").toString();
	ams_packages=config->value("Quick/AMS",true).toBool();
	makeidx_package=config->value("Quick/MakeIndex",false).toBool();
	author=config->value("Quick/Author","").toString();


	spell_dic=config->value("Spell/Dic","<dic not found>").toString();
	if (spell_dic=="<dic not found>") {
		spell_dic=findResourceFile(QString(QLocale::system().name())+".dic");
		if (spell_dic=="") spell_dic=findResourceFile("en_US.dic");
		if (spell_dic=="") spell_dic=findResourceFile("en_GB.dic");
		if (spell_dic=="") spell_dic=findResourceFile("fr_FR.dic");
		if (spell_dic=="") spell_dic=findResourceFile("de_DE.dic");
	}

	for (int i=0; i <412 ; i++)
		symbolScore[i]=config->value("Symbols/symbol"+QString::number(i),0).toInt();




	config->endGroup();

	config->beginGroup("formats");
	m_formats = new QFormatFactory(":/qxs/defaultFormats.qxf", this); //load default formats from resource file
	m_formats->load(*config,true); //load customized formats
	config->endGroup();

	config->beginGroup("completionFile");
	completerFiles=config->value("Completion/completionFiles",QStringList("texmakerx.cwl")).toStringList();
	readCompletionList(completerFiles);
	config->endGroup();

	tabbedLogView=config->value("LogView/Tabbed","true").toBool();

	delete config;
}

void Texmaker::SaveSettings() {
	QSettings *config=configManager.saveSettings();

	config->beginGroup("texmaker");
	QList<int> sizes;
	QList<int>::Iterator it;


	config->setValue("MainWindowState",saveState(0));
	config->setValue("Geometries/MainwindowWidth", width());

	config->setValue("Geometries/MainwindowHeight", height());
	config->setValue("Geometries/MainwindowX", x());
	config->setValue("Geometries/MainwindowY", y());

	config->setValue("Editor/WordWrap",wordwrap);

	config->setValue("Editor/Parentheses Matching",parenmatch);
	config->setValue("Editor/Line Number Multiples",showlinemultiples);
	config->setValue("Editor/Completion",completion);
	config->setValue("Editor/Auto Indent",autoindent);

	config->setValue("Editor/Folding",folding);
	config->setValue("Editor/Show Line State",showlinestate);
	config->setValue("Editor/Show Cursor State",showcursorstate);
	config->setValue("Editor/Real-Time Spellchecking",realtimespellchecking);

	config->setValue("Show/OutputView",showoutputview);

	config->setValue("Show/Structureview",showstructview);

	config->setValue("Tools/Quick Mode",quickmode);

	config->setValue("Tools/Latex",latex_command);
	config->setValue("Tools/Dvi",viewdvi_command);
	config->setValue("Tools/Dvips",dvips_command);
	config->setValue("Tools/Ps",viewps_command);


	config->setValue("Tools/Ps2pdf",ps2pdf_command);

	config->setValue("Tools/Makeindex",makeindex_command);

	config->setValue("Tools/Bibtex",bibtex_command);
	config->setValue("Tools/Pdflatex",pdflatex_command);
	config->setValue("Tools/Pdf",viewpdf_command);

	config->setValue("Tools/Dvipdf",dvipdf_command);
	config->setValue("Tools/Metapost",metapost_command);

	config->setValue("Tools/Ghostscript",ghostscript_command);
	config->setValue("Tools/Userquick",userquick_command);
	config->setValue("Tools/Precompile",precompile_command);

	if (userClassList.count()>0)
		config->setValue("Tools/User Class",userClassList);
	if (userPaperList.count()>0) config->setValue("Tools/User Paper",userPaperList);
	if (userEncodingList.count()>0) config->setValue("Tools/User Encoding",userEncodingList);
	if (userOptionsList.count()>0) config->setValue("Tools/User Options",userOptionsList);

	if (ToggleRememberAct->isChecked()) {
		config->setValue("Files/RestoreSession",true);
		QStringList curFiles;//store in order
		for (int i=0; i<EditorView->count(); i++) {
			LatexEditorView *ed=qobject_cast<LatexEditorView *>(EditorView->widget(i));
			if (ed) curFiles.append(filenames[ed]);
		}
		config->setValue("Files/Session/Files",curFiles);
		config->setValue("Files/Session/CurrentFile",currentEditorView()?filenames[currentEditorView()]:"");
		config->setValue("Files/Session/MasterFile",singlemode?"":MasterName);
	} else config->setValue("Files/RestoreSession",false);

	for (int i=0; i<=9; i++) {
		config->setValue(QString("User/Menu%1").arg(i+1),UserMenuName[i]);
		config->setValue(QString("User/Tag%1").arg(i+1),UserMenuTag[i]);
	}
	for (int i=0; i<=4; i++) {
		config->setValue(QString("User/ToolName%1").arg(i+1),UserToolName[i]);
		config->setValue(QString("User/Tool%1").arg(i+1),UserToolCommand[i]);
	}

        for(int i=0;i<struct_level.length();i++)
            config->setValue("Structure/Structure Level "+QString::number(i+1),struct_level[i]);

	config->setValue("Quick/Class",document_class);

	config->setValue("Quick/Typeface",typeface_size);
	config->setValue("Quick/Papersize",paper_size);
	config->setValue("Quick/Encoding",document_encoding);
	config->setValue("Quick/AMS",ams_packages);

	config->setValue("Quick/MakeIndex",makeidx_package);
	config->setValue("Quick/Author",author);


	config->setValue("Spell/Dic",spell_dic);

	for (int i=0; i <412 ; i++) {
		config->setValue("Symbols/symbol"+QString::number(i),symbolScore[i]);
	}



	config->endGroup();

	config->beginGroup("formats");
	m_formats->save(*config);
	config->endGroup();

	config->setValue("LogView/Tabbed",tabbedLogView);

	if (!completerFiles.isEmpty()) {
		config->beginGroup("completionFile");
		config->setValue("Completion/completionFiles",completerFiles);
		config->endGroup();
	}

	delete config;
}

////////////////// STRUCTURE ///////////////////
void Texmaker::ShowStructure() {
//StructureToolbox->setCurrentItem(StructureTreeWidget);
	StructureToolbox->setCurrentIndex(StructureToolbox->indexOf(StructureTreeWidget));
}

void Texmaker::UpdateStructure() {
// collect user define tex commands for completer
// initialize List
	userCommandList.clear();

//
        QTreeWidgetItem *Child, *theitem;
        QVector<QTreeWidgetItem *> parent_level(struct_level.length());
	QString current;
	if (StructureTreeWidget->currentItem()) current=StructureTreeWidget->currentItem()->text(0);
	StructureTreeWidget->clear();
	if (!currentEditorView()) return;
	QString shortName = getName();
	if ((shortName.right(4)!=".tex") && (shortName!="untitled"))  return;
	int pos;
	while ((pos = (int)shortName.indexOf('/')) != -1)
		shortName.remove(0,pos+1);
	QTreeWidgetItem *top = new QTreeWidgetItem(StructureTreeWidget);
	top->setIcon(0,QIcon(":/images/doc.png"));
	top->setText(0,shortName);
	Child=parent_level[0]=parent_level[1]=parent_level[2]=parent_level[3]=parent_level[4]=top;
	labelitem.clear();
	QTreeWidgetItem *toplabel = new QTreeWidgetItem(top);
	toplabel->setText(0,"LABELS");
	QTreeWidgetItem *toptodo = new QTreeWidgetItem(top);
	toptodo->setText(0,"TODO");
	toptodo->setHidden(true);
	QString s;
	for (int i=0; i<currentEditorView()->editor->document()->lines(); i++) {
		int tagStart, tagEnd;
		//// newcommand ////
		tagStart=tagEnd=0;
		s=currentEditorView()->editor->text(i);
		tagStart=s.indexOf("\\newcommand{", tagEnd);
		if (tagStart!=-1) {
			s=s.mid(tagStart+12,s.length());
			tagStart=s.indexOf("}", tagEnd);
			if (tagStart!=-1) {
				int optionStart=s.indexOf("[", tagEnd);
				int options=0;
				if (optionStart!=-1) {
					int optionEnd=s.indexOf("]", tagEnd);
					QString zw=s.mid(optionStart+1,optionEnd-optionStart-1);
					bool ok;
					options=zw.toInt(&ok,10);
					if (!ok) options=0;
				}
				s=s.mid(0,tagStart);
				for (int i=0; i<options; i++) {
					if (i==0) s.append("{%|}");
					else s.append("{}");
				}
				userCommandList.append(s);
			}
		};
		//// label ////
		s=currentEditorView()->editor->text(i);
		s=findToken(s,"\\label{");
		if (s!="") {
			labelitem.append(s);
			Child = new QTreeWidgetItem(toplabel);
			Child->setText(3,s);
			Child->setText(2,QString::number(i+1));
			s=s+" ("+tr("line")+" "+QString::number(i+1)+")";
			Child->setText(1,"label");
			Child->setText(0,s);
			Child->setData(structureTreeLineColumn,Qt::UserRole,QVariant::fromValue(currentEditorView()->editor->document()->line(i).handle()));
		}
		//// TODO marker
		s=currentEditorView()->editor->text(i);
		int l=s.indexOf("\%TODO");
		if (l>=0) {
			s=s.mid(l+6,s.length());
			Child = new QTreeWidgetItem(toptodo);
			Child->setText(3,s);
			Child->setText(2,QString::number(i+1));
			s=s+" ("+tr("line")+" "+QString::number(i+1)+")";
			Child->setText(1,"todo");
			Child->setText(0,s);
			toptodo->setHidden(false);
			Child->setData(structureTreeLineColumn,Qt::UserRole,QVariant::fromValue(currentEditorView()->editor->document()->line(i).handle()));
		}
		//// include,input ////
		QStringList inputTokens;
		inputTokens << "input" << "include";
		for(int header=0;header<inputTokens.length();header++){
			s=currentEditorView()->editor->text(i);
			s=findToken(s,"\\"+inputTokens.at(header)+"{");
			if (s!="") {
				Child = new QTreeWidgetItem(top);
				Child->setText(0,s);
				Child->setIcon(0,QIcon(":/images/include.png"));
				Child->setText(1,inputTokens.at(header));
				Child->setData(structureTreeLineColumn,Qt::UserRole,QVariant::fromValue(currentEditorView()->editor->document()->line(i).handle()));
			};
		}//for
		//// all sections ////
		for(int header=0;header<struct_level.length();header++){
			s=currentEditorView()->editor->text(i);
			s=findToken(s,QRegExp("\\\\"+struct_level[header]+"\\*?[\\{\\[]"));
			if (s!="") {
				s=extractSectionName(s);
				s=s+" ("+tr("line")+" "+QString::number(i+1)+")";
				parent_level[header] = header==0 ? new QTreeWidgetItem(top) : new QTreeWidgetItem(parent_level[header-1]);
				parent_level[header]->setText(0,s);
				parent_level[header]->setIcon(0,QIcon(":/images/"+struct_level[header]+".png"));
				parent_level[header]->setText(1,struct_level[header]);
				parent_level[header]->setText(2,QString::number(i+1));
				parent_level[header]->setToolTip(0, s);
				parent_level[header]->setData(structureTreeLineColumn,Qt::UserRole,QVariant::fromValue(currentEditorView()->editor->document()->line(i).handle()));
				for(int j=header+1;j<parent_level.size();j++)
					parent_level[j]=parent_level[header];
			};
		}
	}
	if (!current.isEmpty()) {
		QList<QTreeWidgetItem *> fItems=StructureTreeWidget->findItems(current,Qt::MatchRecursive,0);
		if ((fItems.size()>0) && (fItems.at(0))) {
			StructureTreeWidget->setCurrentItem(fItems.at(0));
			theitem=fItems.at(0)->parent();
			while ((theitem) && (theitem!=top)) {
				StructureTreeWidget->setItemExpanded(theitem,true);
				theitem=theitem->parent();
			}
		}
	}
	StructureTreeWidget->setItemExpanded(top,true);
	updateCompleter();
}

void Texmaker::ClickedOnStructure(QTreeWidgetItem *item,int col) {
	Qt::MouseButtons mb=QApplication::mouseButtons();
	if (QApplication::mouseButtons()==Qt::RightButton) return; // avoid jumping to line if contextmenu is called

	QString finame;
	if (singlemode) {
		finame=getName();
	} else {
		finame=MasterName;
	}
	if ((singlemode && !currentEditorView()) || finame=="untitled" || finame=="") {
		return;
	}
	QFileInfo fi(finame);
	QString name=fi.absoluteFilePath();
	QString flname=fi.fileName();
	QString basename=name.left(name.length()-flname.length());
	if (item)  {
		QString s=item->text(1);
		if (s=="include") {
			QString fname=item->text(0);
			if (fname.right(4)==".tex") fname=basename+fname;
			else fname=basename+fname+".tex";
			QFileInfo fi(fname);
			if (fi.exists() && fi.isReadable()) load(fname);
		} else if (s=="input") {
			QString fname=item->text(0);
			if (fname.right(4)==".tex") fname=basename+fname;
			else fname=basename+fname+".tex";
			QFileInfo fi(fname);
			if (fi.exists() && fi.isReadable()) load(fname);
		} else {
			QDocumentLine mLine(item->data(structureTreeLineColumn,Qt::UserRole).value<QDocumentLineHandle*>());
			int l=mLine.lineNumber();
			if(l<0) return;
            currentEditorView()->editor->setCursorPosition(l,1);
			currentEditorView()->editor->setFocus();
		}
	}
}

//////////TAGS////////////////
void Texmaker::NormalCompletion() {
	if (!currentEditorView())	return;
	currentEditorView()->complete(true);
}
void Texmaker::InsertEnvironmentCompletion() {
	if (!currentEditorView())	return;
	QDocumentCursor c = currentEditorView()->editor->cursor();
	QString eow=getCommonEOW();
	while (c.columnNumber()>0 && !eow.contains(c.previousChar())) c.movePosition(1,QDocumentCursor::PreviousCharacter);
	c.insertText("\\begin{");//remaining part is up to the completion engine
	//c=currentEditorView()->editor->cursor();
	//c.movePosition(QString("\\begin{)").length(), QDocumentCursor::NextCharacter);
	//currentEditorView()->editor->setCursor(c);
	//if (currentEditorView()->editor->completionEngine())
	//    currentEditorView()->editor->completionEngine()->complete();
	currentEditorView()->complete(true);
}
// tries to complete normal text
// only starts up if already 2 characters have been typed in
void Texmaker::InsertTextCompletion() {
	if (!currentEditorView())    return;
	QDocumentCursor c = currentEditorView()->editor->cursor();
	QString eow=getCommonEOW();
	int i=0;
	int col=c.columnNumber();
	QString word=c.line().text();
	while (c.columnNumber()>0 && !eow.contains(c.previousChar())) {
		c.movePosition(1,QDocumentCursor::PreviousCharacter);
		i++;
	}
	if (i>1) {
		QString my_text=currentEditorView()->editor->text();
		int end=0;
		int k=0; // number of occurences of search word.
		word=word.mid(col-i,i);
		//TODO: Boundary needs to specified more exactly
		//TODO: type in text needs to be excluded, if not already present
		QString wrd;
		QStringList words;
		while ((i=my_text.indexOf(QRegExp("\\b"+word),end))>0) {
			end=my_text.indexOf(QRegExp("\\b"),i+1);
			if (end>i) {
				if (word==my_text.mid(i,end-i)) {
					k=k+1;
					if (k==2) words << my_text.mid(i,end-i);
				} else {
					if (!words.contains(my_text.mid(i,end-i)))
						words << my_text.mid(i,end-i);
				}
			} else {
				if (word==my_text.mid(i,end-i)) {
					k=k+1;
					if (k==2) words << my_text.mid(i,end-i);
				} else {
					if (!words.contains(my_text.mid(i,end-i)))
						words << my_text.mid(i,my_text.length()-i);
				}
			}
		}

		completer->setWords(words, true);
		currentEditorView()->complete(true,true);
	}
}

void Texmaker::InsertTag(QString Entity, int dx, int dy) {
	if (!currentEditorView())	return;
	int curline,curindex;
	currentEditorView()->editor->getCursorPosition(curline,curindex);
	currentEditorView()->editor->cursor().insertText(Entity);
	if (dy==0) currentEditorView()->editor->setCursorPosition(curline,curindex+dx);
	else if (dx==0) currentEditorView()->editor->setCursorPosition(curline+dy,0);
	else currentEditorView()->editor->setCursorPosition(curline+dy,curindex+dx);
	currentEditorView()->editor->setFocus();
	OutputTextEdit->clear();
	//logViewerTabBar->setCurrentIndex(0);
	//OutputTable->hide();
	//logpresent=false;
}

void Texmaker::InsertSymbol(QTableWidgetItem *item) {
	QString code_symbol;
	QRegExp rxnumber(";([0-9]+)");
	int number=-1;
	if (item) {
		if (rxnumber.indexIn(item->text()) != -1) number=rxnumber.cap(1).toInt();
		if ((number>-1) && (number<412)) symbolScore[number]=symbolScore[number]+1;
		code_symbol=item->text().remove(rxnumber);
		InsertTag(code_symbol,code_symbol.length(),0);
		SetMostUsedSymbols();
	}
}

void Texmaker::InsertMetaPost(QListWidgetItem *item) {
	QString mpcode;
	if (item) {
		mpcode=item->text();
		if (mpcode!="----------") InsertTag(mpcode,mpcode.length(),0);
	}
}

void Texmaker::InsertPstricks(QListWidgetItem *item) {
	QString pstcode;
	if (item  && !item->font().bold()) {
		pstcode=item->text();
		pstcode.remove(QRegExp("\\[(.*)\\]"));
		InsertTag(pstcode,pstcode.length(),0);
	}
}

void Texmaker::InsertFromAction() {
	if (!currentEditorView())	return;
	QAction *action = qobject_cast<QAction *>(sender());
	if (action)	{
		CompletionWord(action->data().toString()).insertAt(currentEditorView()->editor,currentEditorView()->editor->cursor());
		OutputTextEdit->setText(CompletionWord(action->whatsThis()).shownWord);
	}
}

void Texmaker::InsertWithSelectionFromString(const QString& text) {
	bool ok;
	QStringList tagList;
	if (!currentEditorView())	return;
	tagList= text.split("/");
	if (!currentEditorView()->editor->cursor().hasSelection()) {
		InsertTag(tagList.at(0)+tagList.at(1),tagList.at(2).toInt(&ok, 10),tagList.at(3).toInt(&ok, 10));
	} else {
		currentEditorView()->editor->cut();
		InsertTag(tagList.at(0),tagList.at(2).toInt(&ok, 10),tagList.at(3).toInt(&ok, 10));
		currentEditorView()->editor->paste();
		InsertTag(tagList.at(1),0,0);
	}
}

void Texmaker::InsertFromString(const QString& text) {
	bool ok;
	QStringList tagList;
	if (!currentEditorView()) return;
	tagList= text.split("/");
	InsertTag(tagList.at(0),tagList.at(1).toInt(&ok, 10),tagList.at(2).toInt(&ok, 10));
}

void Texmaker::InsertBib() {
	if (!currentEditorView())	return;
//currentEditorView()->editor->viewport()->setFocus();
	QString tag;
	QFileInfo fi(getName());
	tag=QString("\\bibliography{");
	tag +=fi.completeBaseName();
	tag +=QString("}\n");
	InsertTag(tag,0,1);
	OutputTextEdit->clear();
	logViewerTabBar->setCurrentIndex(0);
//OutputTable->hide();
	OutputTextEdit->insertLine("The argument to \\bibliography refers to the bib file (without extension)");
	OutputTextEdit->insertLine("which should contain your database in BibTeX format.");
	OutputTextEdit->insertLine("Texmaker inserts automatically the base name of the TeX file");
}

void Texmaker::InsertStruct() {
	QString actData, tag;
	if (!currentEditorView())	return;
//currentEditorView()->editor->viewport()->setFocus();
	QAction *action = qobject_cast<QAction *>(sender());
	if (action) {
		actData=action->data().toString();
		StructDialog *stDlg = new StructDialog(this,actData);
		if (stDlg->exec()) {
			if (stDlg->ui.checkBox->isChecked()) {
				tag=actData+"{";
			} else {
				tag=actData+"*{";
			}
			tag +=stDlg->ui.TitlelineEdit->text();
			tag +=QString("}\n");
			InsertTag(tag,0,1);
			UpdateStructure();
		}
	}
}

void Texmaker::InsertStructFromString(const QString& text) {
	QString tag;
	if (!currentEditorView())	return;
//currentEditorView()->editor->viewport()->setFocus();
	StructDialog *stDlg = new StructDialog(this,text);
	if (stDlg->exec()) {
		if (stDlg->ui.checkBox->isChecked()) {
			tag=text+"{";
		} else {
			tag=text+"*{";
		}
		tag +=stDlg->ui.TitlelineEdit->text();
		tag +=QString("}\n");
		InsertTag(tag,0,1);
		UpdateStructure();
	}
}

void Texmaker::InsertImage() {
	if (!currentEditorView())	return;
	QString currentDir=QDir::homePath();
	QString finame;
	if (singlemode) {
		finame=getName();
	} else {
		finame=MasterName;
	}
	QFileInfo fi(finame);
	if (finame!="untitled") currentDir=fi.absolutePath();
	FileChooser *sfDlg = new FileChooser(this,tr("Select an image File"));
	sfDlg->setFilter("Graphic files (*.eps *.pdf *.png);;All files (*.*)");
	sfDlg->setDir(currentDir);
	if (sfDlg->exec()) {
		QString fn=sfDlg->fileName();
		QFileInfo fi(fn);
		InsertTag("\\includegraphics[scale=1]{"+getRelativePath(currentDir, fn)+fi.fileName()+"} ",26,0);
	}
}

void Texmaker::InsertInclude() {
	if (!currentEditorView())	return;
	QString currentDir=QDir::homePath();
	QString finame;
	if (singlemode) {
		finame=getName();
	} else {
		finame=MasterName;
	}
	QFileInfo fi(finame);
	if (finame!="untitled") currentDir=fi.absolutePath();
	FileChooser *sfDlg = new FileChooser(this,tr("Select a File"));
	sfDlg->setFilter("TeX files (*.tex);;All files (*.*)");
	sfDlg->setDir(currentDir);
	if (sfDlg->exec()) {
		QString fn=sfDlg->fileName();
		QFileInfo fi(fn);
		InsertTag("\\include{"+getRelativePath(currentDir, fn)+fi.completeBaseName()+"}",9,0);
	}
	UpdateStructure();
}

void Texmaker::InsertInput() {
	if (!currentEditorView())	return;
	QString currentDir=QDir::homePath();
	QString finame;
	if (singlemode) {
		finame=getName();
	} else {
		finame=MasterName;
	}
	QFileInfo fi(finame);
	if (finame!="untitled") currentDir=fi.absolutePath();
	FileChooser *sfDlg = new FileChooser(this,tr("Select a File"));
	sfDlg->setFilter("TeX files (*.tex);;All files (*.*)");
	sfDlg->setDir(currentDir);
	if (sfDlg->exec()) {
		QString fn=sfDlg->fileName();
		QFileInfo fi(fn);
		InsertTag("\\input{"+getRelativePath(currentDir, fn)+fi.completeBaseName()+"}",7,0);
	}
	UpdateStructure();
}

void Texmaker::QuickTabular() {
	if (!currentEditorView())	return;
	QString al="";
	QString vs="";
	QString hs="";
	QString tag;
	TabDialog *quickDlg = new TabDialog(this,"Tabular");
	QTableWidgetItem *item=new QTableWidgetItem();
	if (quickDlg->exec()) {
		int y = quickDlg->ui.spinBoxRows->value();
		int x = quickDlg->ui.spinBoxColumns->value();
		if ((quickDlg->ui.comboSeparator->currentIndex())==0) vs=QString("|");
		if ((quickDlg->ui.comboSeparator->currentIndex())==1) vs=QString("||");
		if ((quickDlg->ui.comboSeparator->currentIndex())==2) vs=QString("");
		if ((quickDlg->ui.comboSeparator->currentIndex())==3) vs=QString("@{}");
		tag = QString("\\begin{tabular}{")+vs;
		if ((quickDlg->ui.comboAlignment->currentIndex())==0) al=QString("c")+vs;
		if ((quickDlg->ui.comboAlignment->currentIndex())==1) al=QString("l")+vs;
		if ((quickDlg->ui.comboAlignment->currentIndex())==2) al=QString("r")+vs;
		if ((quickDlg->ui.comboAlignment->currentIndex())==3) al=QString("p{}")+vs;
		if (quickDlg->ui.checkBox->isChecked()) hs=QString("\\hline ");
		for (int j=0; j<x; j++) {
			tag +=al;
		}
		tag +=QString("}\n");
		for (int i=0; i<y; i++) {
			tag +=hs;
			for (int j=0; j<x-1; j++) {
				item =quickDlg->ui.tableWidget->item(i,j);
				if (item) tag +=item->text()+ QString(" & ");
				else tag +=QString(" & ");
			}
			item =quickDlg->ui.tableWidget->item(i,x-1);
			if (item) tag +=item->text()+ QString(" \\\\ \n");
			else tag +=QString(" \\\\ \n");
		}
		if (quickDlg->ui.checkBox->isChecked()) tag +=hs+QString("\n\\end{tabular} ");
		else tag +=QString("\\end{tabular} ");
		InsertTag(tag,0,0);
	}

}

void Texmaker::QuickArray() {
	if (!currentEditorView())	return;
	QString al;
	ArrayDialog *arrayDlg = new ArrayDialog(this,"Array");
	QTableWidgetItem *item=new QTableWidgetItem();
	if (arrayDlg->exec()) {
		int y = arrayDlg->ui.spinBoxRows->value();
		int x = arrayDlg->ui.spinBoxColumns->value();
		QString env=arrayDlg->ui.comboEnvironment->currentText();
		QString tag = QString("\\begin{")+env+"}";
		if (env=="array") {
			tag+="{";
			if ((arrayDlg->ui.comboAlignment->currentIndex())==0) al=QString("c");
			if ((arrayDlg->ui.comboAlignment->currentIndex())==1) al=QString("l");
			if ((arrayDlg->ui.comboAlignment->currentIndex())==2) al=QString("r");
			for (int j=0; j<x; j++) {
				tag +=al;
			}
			tag+="}";
		}
		tag +=QString("\n");
		for (int i=0; i<y-1; i++) {
			for (int j=0; j<x-1; j++) {
				item =arrayDlg->ui.tableWidget->item(i,j);
				if (item) tag +=item->text()+ QString(" & ");
				else tag +=QString(" & ");
			}
			item =arrayDlg->ui.tableWidget->item(i,x-1);
			if (item) tag +=item->text()+ QString(" \\\\ \n");
			else tag +=QString(" \\\\ \n");
		}
		for (int j=0; j<x-1; j++) {
			item =arrayDlg->ui.tableWidget->item(y-1,j);
			if (item) tag +=item->text()+ QString(" & ");
			else tag +=QString(" & ");
		}
		item =arrayDlg->ui.tableWidget->item(y-1,x-1);
		if (item) tag +=item->text()+ QString("\n\\end{")+env+"} ";
		else tag +=QString("\n\\end{")+env+"} ";
		InsertTag(tag,0,0);
	}
}

void Texmaker::QuickTabbing() {
	if (!currentEditorView()) return;
	TabbingDialog *tabDlg = new TabbingDialog(this,"Tabbing");
	if (tabDlg->exec()) {
		int	x = tabDlg->ui.spinBoxColumns->value();
		int	y = tabDlg->ui.spinBoxRows->value();
		QString s=tabDlg->ui.lineEdit->text();
		QString tag = QString("\\begin{tabbing}\n");
		for (int j=1; j<x; j++) {
			tag +="\\hspace{"+s+"}\\=";
		}
		tag+="\\kill\n";
		for (int i=0; i<y-1; i++) {
			for (int j=1; j<x; j++) {
				tag +=" \\> ";
			}
			tag+="\\\\ \n";
		}
		for (int j=1; j<x; j++) {
			tag +=" \\> ";
		}
		tag += QString("\n\\end{tabbing} ");
		InsertTag(tag,0,2);
	}
}

void Texmaker::QuickLetter() {
	if (!currentEditorView()) {
		fileNew();
		if (!currentEditorView()) return;
	}
	QString tag=QString("\\documentclass[");
	LetterDialog *ltDlg = new LetterDialog(this,"Letter");
	if (ltDlg->exec()) {
		tag+=ltDlg->ui.comboBoxPt->currentText()+QString(",");
		tag+=ltDlg->ui.comboBoxPaper->currentText()+QString("]{letter}");
		tag+=QString("\n");
		if (ltDlg->ui.comboBoxEncoding->currentText()!="NONE") tag+=QString("\\usepackage[")+ltDlg->ui.comboBoxEncoding->currentText()+QString("]{inputenc}");
		if (ltDlg->ui.comboBoxEncoding->currentText().startsWith("utf8")) tag+=QString(" \\usepackage{ucs}");
		tag+=QString("\n");
		if (ltDlg->ui.checkBox->isChecked()) tag+=QString("\\usepackage{amsmath}\n\\usepackage{amsfonts}\n\\usepackage{amssymb}\n");
		tag+="\\address{your name and address} \n";
		tag+="\\signature{your signature} \n";
		tag+="\\begin{document} \n";
		tag+="\\begin{letter}{name and address of the recipient} \n";
		tag+="\\opening{saying hello} \n \n";
		tag+="write your letter here \n \n";
		tag+="\\closing{saying goodbye} \n";
		tag+="%\\cc{Cclist} \n";
		tag+="%\\ps{adding a postscript} \n";
		tag+="%\\encl{list of enclosed material} \n";
		tag+="\\end{letter} \n";
		tag+="\\end{document}";
		if (ltDlg->ui.checkBox->isChecked()) {
			InsertTag(tag,9,5);
		} else {
			InsertTag(tag,9,2);
		}
	}
}

void Texmaker::QuickDocument() {
	QString opt="";
	int li=3;
	int f;
	if (!currentEditorView()) {
		fileNew();
		if (!currentEditorView()) return;
	}
	QString tag=QString("\\documentclass[");
	QuickDocumentDialog *startDlg = new QuickDocumentDialog(this,"Quick Start");
	startDlg->otherClassList=userClassList;
	startDlg->otherPaperList=userPaperList;
	startDlg->otherEncodingList=userEncodingList;
	startDlg->otherOptionsList=userOptionsList;
	startDlg->Init();
	f=startDlg->ui.comboBoxClass->findText(document_class,Qt::MatchExactly | Qt::MatchCaseSensitive);
	startDlg->ui.comboBoxClass->setCurrentIndex(f);
	f=startDlg->ui.comboBoxSize->findText(typeface_size,Qt::MatchExactly | Qt::MatchCaseSensitive);
	startDlg->ui.comboBoxSize->setCurrentIndex(f);
	f=startDlg->ui.comboBoxPaper->findText(paper_size,Qt::MatchExactly | Qt::MatchCaseSensitive);
	startDlg->ui.comboBoxPaper->setCurrentIndex(f);
	f=startDlg->ui.comboBoxEncoding->findText(document_encoding,Qt::MatchExactly | Qt::MatchCaseSensitive);
	startDlg->ui.comboBoxEncoding->setCurrentIndex(f);
	startDlg->ui.checkBoxAMS->setChecked(ams_packages);
	startDlg->ui.checkBoxIDX->setChecked(makeidx_package);
	startDlg->ui.lineEditAuthor->setText(author);
	if (startDlg->exec()) {
		tag+=startDlg->ui.comboBoxSize->currentText()+QString(",");
		tag+=startDlg->ui.comboBoxPaper->currentText();
		QList<QListWidgetItem *> selectedItems=startDlg->ui.listWidgetOptions->selectedItems();
		for (int i = 0; i < selectedItems.size(); ++i) {
			if (selectedItems.at(i)) opt+=QString(",")+selectedItems.at(i)->text();
		}
		tag+=opt+QString("]{");
		tag+=startDlg->ui.comboBoxClass->currentText()+QString("}");
		tag+=QString("\n");
		if (startDlg->ui.comboBoxEncoding->currentText()!="NONE") tag+=QString("\\usepackage[")+startDlg->ui.comboBoxEncoding->currentText()+QString("]{inputenc}");
		tag+=QString("\n");
		if (startDlg->ui.comboBoxEncoding->currentText().startsWith("utf8x")) {
			tag+=QString("\\usepackage{ucs}\n");
			li=li+1;
		}
		if (startDlg->ui.checkBoxAMS->isChecked()) {
			tag+=QString("\\usepackage{amsmath}\n\\usepackage{amsfonts}\n\\usepackage{amssymb}\n");
			li=li+3;
		}
		if (startDlg->ui.checkBoxIDX->isChecked()) {
			tag+=QString("\\usepackage{makeidx}\n");
			li=li+1;
		}
		if (startDlg->ui.lineEditAuthor->text()!="") {
			tag+="\\author{"+startDlg->ui.lineEditAuthor->text()+"}\n";
			li=li+1;
		}
		if (startDlg->ui.lineEditTitle->text()!="") {
			tag+="\\title{"+startDlg->ui.lineEditTitle->text()+"}\n";
			li=li+1;
		}
		tag+=QString("\\begin{document}\n\n\\end{document}");
		InsertTag(tag,0,li);
		document_class=startDlg->ui.comboBoxClass->currentText();
		typeface_size=startDlg->ui.comboBoxSize->currentText();
		paper_size=startDlg->ui.comboBoxPaper->currentText();
		document_encoding=startDlg->ui.comboBoxEncoding->currentText();
		ams_packages=startDlg->ui.checkBoxAMS->isChecked();
		makeidx_package=startDlg->ui.checkBoxIDX->isChecked();
		author=startDlg->ui.lineEditAuthor->text();
		userClassList=startDlg->otherClassList;
		userPaperList=startDlg->otherPaperList;
		userEncodingList=startDlg->otherEncodingList;
		userOptionsList=startDlg->otherOptionsList;
	}
}

void Texmaker::InsertBib1() {
	QString tag = QString("@Article{,\n");
	tag+="author = {},\n";
	tag+="title = {},\n";
	tag+="journal = {},\n";
	tag+="year = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTvolume = {},\n";
	tag+="OPTnumber = {},\n";
	tag+="OPTpages = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,9,0);
	OutputTextEdit->insertLine("Bib fields - Article in Journal");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib2() {
	QString tag = QString("@InProceedings{,\n");
	tag+="author = {},\n";
	tag+="title = {},\n";
	tag+="booktitle = {},\n";
	tag+="OPTcrossref = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTpages = {},\n";
	tag+="OPTyear = {},\n";
	tag+="OPTeditor = {},\n";
	tag+="OPTvolume = {},\n";
	tag+="OPTnumber = {},\n";
	tag+="OPTseries = {},\n";
	tag+="OPTaddress = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTorganization = {},\n";
	tag+="OPTpublisher = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,15,0);
	OutputTextEdit->insertLine("Bib fields - Article in Conference Proceedings");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib3() {
	QString tag = QString("@InCollection{,\n");
	tag+="author = {},\n";
	tag+="title = {},\n";
	tag+="booktitle = {},\n";
	tag+="OPTcrossref = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTpages = {},\n";
	tag+="OPTpublisher = {},\n";
	tag+="OPTyear = {},\n";
	tag+="OPTeditor = {},\n";
	tag+="OPTvolume = {},\n";
	tag+="OPTnumber = {},\n";
	tag+="OPTseries = {},\n";
	tag+="OPTtype = {},\n";
	tag+="OPTchapter = {},\n";
	tag+="OPTaddress = {},\n";
	tag+="OPTedition = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,14,0);
	OutputTextEdit->insertLine("Bib fields - Article in a Collection");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib4() {
	QString tag = QString("@InBook{,\n");
	tag+="ALTauthor = {},\n";
	tag+="ALTeditor = {},\n";
	tag+="title = {},\n";
	tag+="chapter = {},\n";
	tag+="publisher = {},\n";
	tag+="year = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTvolume = {},\n";
	tag+="OPTnumber = {},\n";
	tag+="OPTseries = {},\n";
	tag+="OPTtype = {},\n";
	tag+="OPTaddress = {},\n";
	tag+="OPTedition = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTpages = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,8,0);
	OutputTextEdit->insertLine("Bib fields - Chapter or Pages in a Book");
	OutputTextEdit->insertLine("ALT.... : you have the choice between these two fields");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib5() {
	QString tag = QString("@Proceedings{,\n");
	tag+="title = {},\n";
	tag+="year = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTeditor = {},\n";
	tag+="OPTvolume = {},\n";
	tag+="OPTnumber = {},\n";
	tag+="OPTseries = {},\n";
	tag+="OPTaddress = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTorganization = {},\n";
	tag+="OPTpublisher = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,13,0);
	OutputTextEdit->insertLine("Bib fields - Conference Proceedings");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib6() {
	QString tag = QString("@Book{,\n");
	tag+="ALTauthor = {},\n";
	tag+="ALTeditor = {},\n";
	tag+="title = {},\n";
	tag+="publisher = {},\n";
	tag+="year = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTvolume = {},\n";
	tag+="OPTnumber = {},\n";
	tag+="OPTseries = {},\n";
	tag+="OPTaddress = {},\n";
	tag+="OPTedition = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,6,0);
	OutputTextEdit->insertLine("Bib fields - Book");
	OutputTextEdit->insertLine("ALT.... : you have the choice between these two fields");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib7() {
	QString tag = QString("@Booklet{,\n");
	tag+="title = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTauthor = {},\n";
	tag+="OPThowpublished = {},\n";
	tag+="OPTaddress = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTyear = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,9,0);
	OutputTextEdit->insertLine("Bib fields - Booklet");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib8() {
	QString tag = QString("@PhdThesis{,\n");
	tag+="author = {},\n";
	tag+="title = {},\n";
	tag+="school = {},\n";
	tag+="year = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTtype = {},\n";
	tag+="OPTaddress = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,11,0);
	OutputTextEdit->insertLine("Bib fields - PhD. Thesis");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib9() {
	QString tag = QString("@MastersThesis{,\n");
	tag+="author = {},\n";
	tag+="title = {},\n";
	tag+="school = {},\n";
	tag+="year = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTtype = {},\n";
	tag+="OPTaddress = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,15,0);
	OutputTextEdit->insertLine("Bib fields - Master's Thesis");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib10() {
	QString tag = QString("@TechReport{,\n");
	tag+="author = {},\n";
	tag+="title = {},\n";
	tag+="institution = {},\n";
	tag+="year = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTtype = {},\n";
	tag+="OPTnumber = {},\n";
	tag+="OPTaddress = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,12,0);
	OutputTextEdit->insertLine("Bib fields - Technical Report");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib11() {
	QString tag = QString("@Manual{,\n");
	tag+="title = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTauthor = {},\n";
	tag+="OPTorganization = {},\n";
	tag+="OPTaddress = {},\n";
	tag+="OPTedition = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTyear = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,8,0);
	OutputTextEdit->insertLine("Bib fields - Technical Manual");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib12() {
	QString tag = QString("@Unpublished{,\n");
	tag+="author = {},\n";
	tag+="title = {},\n";
	tag+="note = {},\n";
	tag+="OPTkey = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTyear = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,13,0);
	OutputTextEdit->insertLine("Bib fields - Unpublished");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}
void Texmaker::InsertBib13() {
	QString tag = QString("@Misc{,\n");
	tag+="OPTkey = {},\n";
	tag+="OPTauthor = {},\n";
	tag+="OPTtitle = {},\n";
	tag+="OPThowpublished = {},\n";
	tag+="OPTmonth = {},\n";
	tag+="OPTyear = {},\n";
	tag+="OPTnote = {},\n";
	tag+="OPTannote = {}\n";
	tag+="}\n";
	InsertTag(tag,6,0);
	OutputTextEdit->insertLine("Bib fields - Miscellaneous");
	OutputTextEdit->insertLine("OPT.... : optionnal fields (use the 'Clean' command to remove them)");
}

void Texmaker::CleanBib() {
	if (!currentEditorView()) return;
	currentEditorView()->cleanBib();
}

void Texmaker::InsertUserTag() {
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	int id = action->data().toInt();
	if (id<0 || id>=10) return;
	QString userTag=UserMenuTag[id];
	if (userTag.left(1)=="%") {
		userTag=userTag.remove(0,1);
		QString s="\\begin{"+userTag+"}\n\n\\end{"+userTag+"}\n";
		InsertTag(s,0,1);
	} else CompletionWord(userTag).insertAt(currentEditorView()->editor,currentEditorView()->editor->cursor());
}

void Texmaker::EditUserMenu() {
	UserMenuDialog *umDlg = new UserMenuDialog(this,tr("Edit User &Tags"));
	for (int i = 0; i <= 9; i++) {
		umDlg->Name[i]=UserMenuName[i];
		umDlg->Tag[i]=UserMenuTag[i];
		umDlg->init();
	}
	if (umDlg->exec())
		for (int i = 0; i <= 9; i++) {
			UserMenuName[i]=umDlg->Name[i];
			UserMenuTag[i]=umDlg->Tag[i];
			QAction * act=getManagedAction("main/user/tags/tag"+QString::number(i));
			if (act) act->setText(QString::number(i+1)+": "+UserMenuName[i]);
		}
}

void Texmaker::SectionCommand() {
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	if (!currentEditorView()) return;
	InsertStructFromString("\\"+action->text());
	combo1->defaultAction()->setText(action->text());
}

void Texmaker::OtherCommand() {
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	if (!currentEditorView()) return;

	QString text=action->text();
	combo2->defaultAction()->setText(text);

	if (text=="label") {
		InsertFromString("\\label{} /7/0");
		return;
	}
	if (text=="ref") {
		InsertRef();
		return;
	}
	if (text=="pageref") {
		InsertPageRef();
		return;
	}
	if (text=="index") {
		InsertFromString("\\index{}/7/0");
		return;
	}
	if (text=="cite") {
		InsertFromString("\\cite{}/6/0");
		return;
	}
	if (text=="footnote") {
		InsertFromString("\\footnote{}/10/0");
		return;
	}
}

void Texmaker::InsertRef() {
	UpdateStructure();
	QString tag="";
	RefDialog *refDlg = new RefDialog(this,"Labels");
	refDlg->ui.comboBox->addItems(labelitem);
	if (!labelitem.isEmpty() && refDlg->exec()) {
		tag="\\ref{"+refDlg->ui.comboBox->currentText()+"}";
		InsertTag(tag,tag.length(),0);
	} else InsertTag("\\ref{}",5,0);
	OutputTextEdit->insertLine("\\ref{key}");
}

void Texmaker::InsertPageRef() {
	UpdateStructure();
	QString tag="";
	RefDialog *refDlg = new RefDialog(this,"Labels");
	refDlg->ui.comboBox->addItems(labelitem);
	if (!labelitem.isEmpty() && refDlg->exec()) {
		tag="\\pageref{"+refDlg->ui.comboBox->currentText()+"}";
		InsertTag(tag,tag.length(),0);
	} else InsertTag("\\pageref{}",9,0);
	OutputTextEdit->insertLine("\\pageref{key}");
}

void Texmaker::SizeCommand() {
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;

	if (!currentEditorView()) return;

	QString text=action->text();
	combo3->defaultAction()->setText(text);

	if (text=="tiny") {
		InsertWithSelectionFromString("\\begin{tiny}/\\end{tiny}/12/0");
		return;
	}
	if (text=="scriptsize") {
		InsertWithSelectionFromString("\\begin{scriptsize}/\\end{scriptsize}/18/0");
		return;
	}
	if (text=="footnotesize") {
		InsertWithSelectionFromString("\\begin{footnotesize}/\\end{footnotesize}/20/0");
		return;
	}
	if (text=="small") {
		InsertWithSelectionFromString("\\begin{small}/\\end{small}/13/0");
		return;
	}
	if (text=="normalsize") {
		InsertWithSelectionFromString("\\begin{normalsize}/\\end{normalsize}/18/0");
		return;
	}
	if (text=="large") {
		InsertWithSelectionFromString("\\begin{large}/\\end{large}/13/0");
		return;
	}
	if (text=="Large") {
		InsertWithSelectionFromString("\\begin{Large}/\\end{Large}/13/0");
		return;
	}
	if (text=="LARGE") {
		InsertWithSelectionFromString("\\begin{LARGE}/\\end{LARGE}/13/0");
		return;
	}
	if (text=="huge") {
		InsertWithSelectionFromString("\\begin{huge}/\\end{huge}/12/0");
		return;
	}
	if (text=="Huge") {
		InsertWithSelectionFromString("\\begin{Huge}/\\end{Huge}/12/0");
		return;
	}
}

void Texmaker::LeftDelimiter() {
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	if (!currentEditorView()) return;
	QString text=action->text();
	combo4->defaultAction()->setText(text);

	if (text=="left (") InsertTag("\\left( ",7,0);
	if (text=="left [") InsertTag("\\left[ ",7,0);
	if (text=="left {") InsertTag("\\left\\lbrace ",13,0);
	if (text=="left <") InsertTag("\\left\\langle ",13,0);
	if (text=="left )") InsertTag("\\left) ",7,0);
	if (text=="left ]") InsertTag("\\left] ",7,0);
	if (text=="left }") InsertTag("\\left\\rbrace ",13,0);
	if (text=="left >") InsertTag("\\left\\rangle ",13,0);
	if (text=="left.") InsertTag("\\left. ",7,0);
}

void Texmaker::RightDelimiter() {
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	if (!currentEditorView()) return;
	QString text=action->text();
	combo5->defaultAction()->setText(text);

	if (text=="right (") InsertTag("\\right( ",8,0);
	if (text=="right [") InsertTag("\\right[ ",8,0);
	if (text=="right {") InsertTag("\\right\\lbrace ",14,0);
	if (text=="right <") InsertTag("\\right\\langle ",14,0);
	if (text=="right )") InsertTag("\\right) ",8,0);
	if (text=="right ]") InsertTag("\\right] ",8,0);
	if (text=="right }") InsertTag("\\right\\rbrace ",14,0);
	if (text=="right >") InsertTag("\\right\\rangle ",14,0);
	if (text=="right.") InsertTag("\\right. ",8,0);
}

///////////////TOOLS////////////////////
void Texmaker::RunCommand(QString comd,bool waitendprocess,bool showStdout) {

	QString finame=getCompileFileName();
	QString commandline=comd;
	QByteArray result;
	if ((singlemode && !currentEditorView()) || finame=="untitled" || finame=="") {
		QMessageBox::warning(this,tr("Error"),tr("Can't detect the file name"));
		return;
	}
	fileSaveAll();

	QFileInfo fi(finame);
	commandline=BuildManager::parseExtendedCommandLine(commandline,fi,currentEditorView()->editor->cursor().lineNumber()+1);
	if (commandline.trimmed().isEmpty()) {
		ERRPROCESS=true;
		OutputTextEdit->insertLine("Error : no command given \n");
		return;
	}
#ifdef Q_WS_WIN
	if (commandline.startsWith("dde://")) {
		buildManager.executeDDE(commandline);
		return;
	}
#endif
	proc = new QProcess(this);
	proc->setWorkingDirectory(fi.absolutePath());

	connect(proc, SIGNAL(readyReadStandardError()),this, SLOT(readFromStderr()));
	if (showStdout) connect(proc, SIGNAL(readyReadStandardOutput()),this, SLOT(readFromStdoutput()));
	connect(proc, SIGNAL(finished(int)),this, SLOT(SlotEndProcess(int)));
	OutputTextEdit->clear();
	logViewerTabBar->setCurrentIndex(0);
//OutputTable->hide();
//OutputTextEdit->insertLine(commandline+"\n");
	proc->start(commandline);
	if (!proc->waitForStarted(1000)) {
		ERRPROCESS=true;
		OutputTextEdit->insertLine("Error: "+tr("could not start the command:")+" "+commandline+"\n");
		return;
	} else OutputTextEdit->insertLine(tr("Process started")+"\n");
	FINPROCESS=false;
	if (waitendprocess) {
		while (!FINPROCESS) {
			qApp->instance()->processEvents(QEventLoop::ExcludeUserInputEvents);
		}
	}
}

void Texmaker::RunPreCompileCommand() {
	logpresent=false;//log to old (whenever latex is called)
	if (precompile_command.isEmpty()) return;
	RunCommand(precompile_command,true,false);
}

void Texmaker::readFromStderr() {
	QByteArray result=proc->readAllStandardError();
	QString t=QString(result).simplified();
	if (!t.isEmpty()) OutputTextEdit->insertLine(t+"\n");
}

void Texmaker::readFromStdoutput() {
	QByteArray result=proc->readAllStandardOutput();
	QString t=QString(result).trimmed();
	if (!t.isEmpty()) OutputTextEdit->insertLine(t+"\n");
}

void Texmaker::SlotEndProcess(int err) {
	FINPROCESS=true;
	QString result=((err) ? "Process exited with error(s)" : "Process exited normally");
	if (err) ERRPROCESS=true;
	OutputTextEdit->insertLine(result);
	stat2->setText(QString(" %1 ").arg(tr("Ready")));
}

void Texmaker::QuickBuild() {
	RunPreCompileCommand();
	stat2->setText(QString(" %1 ").arg(tr("Quick Build")));
	ERRPROCESS=false;
	switch (quickmode) {
	case 1: {
		stat2->setText(QString(" %1 ").arg("Latex"));
		RunCommand(latex_command,true,false);
		if (ERRPROCESS && !LogExists()) {
			QMessageBox::warning(this,tr("Error"),tr("Could not start the command."));
			return;
		}
		ViewLog();
		if (NoLatexErrors()) {
			stat2->setText(QString(" %1 ").arg("Dvips"));
			if (!ERRPROCESS) RunCommand(dvips_command,true,false);
			else return;
			if (!ERRPROCESS) ViewPS();
			else return;
		} else {
			NextError();
			SwitchToErrorList();
		}
	}
	break;
	case 2: {
		stat2->setText(QString(" %1 ").arg("Latex"));
		RunCommand(latex_command,true,false);
		if (ERRPROCESS && !LogExists()) {
			QMessageBox::warning(this,tr("Error"),tr("Could not start the command."));
			return;
		}
		ViewLog();
		if (NoLatexErrors()) {
			if (!ERRPROCESS) ViewDvi();
			else return;
		} else {
			NextError();
			SwitchToErrorList();
		}
	}
	break;
	case 3: {
		stat2->setText(QString(" %1 ").arg("Pdf Latex"));
		RunCommand(pdflatex_command,true,false);
		if (ERRPROCESS && !LogExists()) {
			QMessageBox::warning(this,tr("Error"),tr("Could not start the command."));
			return;
		}
		ViewLog();
		if (NoLatexErrors()) {
			if (!ERRPROCESS) ViewPDF();
			else return;
		} else {
			NextError();
			SwitchToErrorList();
		}
	}
	break;
	case 4: {
		stat2->setText(QString(" %1 ").arg("Latex"));
		RunCommand(latex_command,true,false);
		if (ERRPROCESS && !LogExists()) {
			QMessageBox::warning(this,tr("Error"),tr("Could not start the command."));
			return;
		}
		ViewLog();
		if (NoLatexErrors()) {
			stat2->setText(QString(" %1 ").arg("Dvi to Pdf"));
			if (!ERRPROCESS) RunCommand(dvipdf_command,true,false);
			else return;
			if (!ERRPROCESS) ViewPDF();
			else return;
		} else {
			NextError();
			SwitchToErrorList();	
		}
	}
	break;
	case 5: {
		stat2->setText(QString(" %1 ").arg("Latex"));
		RunCommand(latex_command,true,false);
		if (ERRPROCESS && !LogExists()) {
			QMessageBox::warning(this,tr("Error"),tr("Could not start the command."));
			return;
		}
		ViewLog();
		if (NoLatexErrors()) {
			stat2->setText(QString(" %1 ").arg("Dvips"));
			if (!ERRPROCESS) RunCommand(dvips_command,true,false);
			else return;
			stat2->setText(QString(" %1 ").arg("Ps to Pdf"));
			if (!ERRPROCESS) RunCommand(ps2pdf_command,true,false);
			else return;
			if (!ERRPROCESS) ViewPDF();
		} else {
			NextError();
			SwitchToErrorList();
		}
	}
	break;
	case 6: {
		QStringList commandList=userquick_command.split("|");
		for (int i = 0; i < commandList.size(); ++i) {
			if ((!ERRPROCESS)&&(!commandList.at(i).isEmpty())) RunCommand(commandList.at(i),true,true);
			else return;
		}
	}
	break;
	}
	if (NoLatexErrors()) ViewLog();
//ViewLog();
//DisplayLatexError();
}

void Texmaker::Latex() {
	RunPreCompileCommand();
	stat2->setText(QString(" %1 ").arg("Latex"));
	RunCommand(latex_command,false,false);
}

void Texmaker::ViewDvi() {
	stat2->setText(QString(" %1 ").arg(tr("View Dvi file")));
	RunCommand(viewdvi_command,false,false);
}

void Texmaker::DviToPS() {
	stat2->setText(QString(" %1 ").arg("Dvips"));
	RunCommand(dvips_command,false,false);
}

void Texmaker::ViewPS() {
	stat2->setText(QString(" %1 ").arg(tr("View PS file")));
	RunCommand(viewps_command,false,false);
}

void Texmaker::PDFLatex() {
	RunPreCompileCommand();
	stat2->setText(QString(" %1 ").arg("Pdf Latex"));
	RunCommand(pdflatex_command,false,false);
}

void Texmaker::ViewPDF() {
	stat2->setText(QString(" %1 ").arg(tr("View Pdf file")));
	RunCommand(viewpdf_command,false,false);
}

void Texmaker::MakeBib() {
	stat2->setText(QString(" %1 ").arg("Bibtex"));
	RunCommand(bibtex_command,false,false);
}

void Texmaker::MakeIndex() {
	stat2->setText(QString(" %1 ").arg("Make index"));
	RunCommand(makeindex_command,false,false);
}

void Texmaker::PStoPDF() {
	stat2->setText(QString(" %1 ").arg("Ps -> Pdf"));
	RunCommand(ps2pdf_command,false,false);
}

void Texmaker::DVItoPDF() {
	stat2->setText(QString(" %1 ").arg("Dvi -> Pdf"));
	RunCommand(dvipdf_command,false,false);
}

void Texmaker::MetaPost() {
	stat2->setText(QString(" %1 ").arg("Mpost"));
	QString finame=getName();
	QFileInfo fi(finame);
	RunCommand(metapost_command+fi.completeBaseName()+"."+fi.suffix(),false,false);
}

void Texmaker::CleanAll() {
	QString finame,f;
	if (singlemode) {
		finame=getName();
	} else {
		finame=MasterName;
	}
	if ((singlemode && !currentEditorView()) || finame=="untitled" || finame=="") {
		QMessageBox::warning(this,tr("Error"),tr("Can't detect the file name"));
		return;
	}
	QString extensionStr=".log,.aux,.dvi,.lof,.lot,.bit,.idx,.glo,.bbl,.ilg,.toc,.ind";
	int query =QMessageBox::warning(this, "TexMakerX", tr("Delete the output files generated by LaTeX?")+QString("\n(%1)").arg(extensionStr),tr("Delete Files"), tr("Cancel"));
	if (query==0) {
		fileSave();
		QFileInfo fi(finame);
		QString basename=fi.absolutePath()+"/"+fi.completeBaseName();
		QStringList extension=extensionStr.split(",");
		stat2->setText(QString(" %1 ").arg(tr("Clean")));
		foreach(QString ext, extension)
		QFile::remove(basename+ext);
		stat2->setText(QString(" %1 ").arg(tr("Ready")));
	}
}

void Texmaker::UserTool() {
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	if (action->data().toInt()<0 || action->data().toInt()>=5) return;
	QString cmd=UserToolCommand[action->data().toInt()];
	if (cmd.isEmpty()) return;
	QStringList commandList=cmd.split("|");
	ERRPROCESS=false;
	for (int i = 0; i < commandList.size(); ++i)
		if ((!ERRPROCESS)&&(!commandList.at(i).isEmpty())) RunCommand(commandList.at(i),true,true);
		else return;
}


void Texmaker::EditUserTool() {
	UserToolDialog *utDlg = new UserToolDialog(this,tr("Edit User &Commands"));
	for (int i = 0; i <= 4; i++) {
		utDlg->Name[i]=UserToolName[i];
		utDlg->Tool[i]=UserToolCommand[i];
		utDlg->init();
	}
	if (utDlg->exec())
		for (int i = 0; i <= 4; i++) {
			UserToolName[i]=utDlg->Name[i];
			UserToolCommand[i]=utDlg->Tool[i];
			QAction * act=getManagedAction("main/user/commands/cmd"+QString::number(i));
			if (act) act->setText(QString::number(i+1)+": "+UserToolName[i]);
		}
}

void Texmaker::WebPublish() {
	if (!currentEditorView()) {
		QMessageBox::information(this,"TexMakerX",tr("No document open"),0);
		return;
	}
	if (!currentEditorView()->editor->getFileEncoding()) return;
	QString finame;
	fileSave();
	if (singlemode) {
		finame=getName();
	} else {
		finame=MasterName;
	}
	if (finame=="untitled") finame="";
	WebPublishDialog *ttwpDlg = new WebPublishDialog(this,tr("Convert to Html"),ghostscript_command,latex_command,dvips_command,
	        currentEditorView()->editor->getFileEncoding());
	ttwpDlg->ui.inputfileEdit->setText(finame);
	ttwpDlg->exec();
	delete ttwpDlg;
}


void Texmaker::AnalyseText() {
	if (!currentEditorView()) {
		QMessageBox::information(this,"TexMakerX",tr("No document open"),0);
		return;
	}
	if (!textAnalysisDlg) {
		textAnalysisDlg = new TextAnalysisDialog(this,tr("Text Analysis"));
		connect(textAnalysisDlg,SIGNAL(destroyed()),this,SLOT(AnalyseTextFormDestroyed()));
	}
	if (!textAnalysisDlg) return;
	textAnalysisDlg->setEditor(currentEditorView()->editor);//->document(), currentEditorView()->editor->cursor());
	textAnalysisDlg->init();
	for (int i=0; i<StructureTreeWidget->topLevelItemCount(); i++)
		//if (StructureTreeWidget->topLevelItem(i)->text(0)==MasterName)
		textAnalysisDlg->interpretStructureTree(StructureTreeWidget->topLevelItem(i));

	textAnalysisDlg->show();
	textAnalysisDlg->raise(); //not really necessary, since the dlg is always on top
	textAnalysisDlg->activateWindow();
}
void Texmaker::AnalyseTextFormDestroyed() {
	textAnalysisDlg=0;
}
//////////////// MESSAGES - LOG FILE///////////////////////
bool Texmaker::LogExists() {
	QString finame;
	if (singlemode) {
		finame=getName();
	} else {
		finame=MasterName;
	}
	if ((singlemode && !currentEditorView()) ||finame=="untitled" || finame=="") {
		return false;
	}
	QFileInfo fi(finame);
	QString name=fi.absoluteFilePath();
	QString ext=fi.suffix();
	QString basename=name.left(name.length()-ext.length()-1);
	QString logname=basename+".log";
	QFileInfo fic(logname);
	if (fic.exists() && fic.isReadable()) return true;
	else return false;
}

void Texmaker::SwitchToErrorList(){
	if (tabbedLogView) logViewerTabBar->setCurrentIndex(2);
	else logViewerTabBar->setCurrentIndex(1);
}

//shows the log (even if it is empty)
void Texmaker::RealViewLog() {
	ViewLog();
	if (!OutputView->isVisible()) OutputView->show();
	if (OutputLayout->currentIndex()==0) logViewerTabBar->setCurrentIndex(1);
}

//shows the log if there are errors
void Texmaker::ViewLog() {
	OutputLogTextEdit->clear();
	logpresent=false;
	QString finame;
	if (singlemode) {
		finame=getName();
	} else {
		finame=MasterName;
	}
	if ((singlemode && !currentEditorView()) ||finame=="untitled" || finame=="") {
		QMessageBox::warning(this,tr("Error"),tr("Could not start the command."));
		ERRPROCESS=true;
		return;
	}
	QFileInfo fi(finame);
	QString name=fi.absoluteFilePath();
	QString ext=fi.suffix();
	QString basename=name.left(name.length()-ext.length()-1);
	QString logname=basename+".log";
	QString line;
	QFileInfo fic(logname);
	if (fic.exists() && fic.isReadable()) {
		//OutputLogTextEdit->insertLine("LOG FILE :");
		QFile f(logname);
		if (f.open(QIODevice::ReadOnly)) {
			QTextStream t(&f);
//		OutputTextEdit->setPlainText( t.readAll() );
			while (!t.atEnd()) {
				line=t.readLine();
				line=line.simplified();
				if (!line.isEmpty()) OutputLogTextEdit->append(line);
			}
		}
		f.close();
		LatexError();

	} else {
		QMessageBox::warning(this,tr("Error"),tr("Log File not found !"));
	}
}

void Texmaker::ClickedOnLogLine(const QModelIndex & index) {
	GoToLogEntry(index.row());
}
void Texmaker::OutputViewVisibilityChanged(bool visible) {
	if (visible) OutputView->toggleViewAction()->setShortcut(Qt::Key_Escape);
	else OutputView->toggleViewAction()->setShortcut(QKeySequence());
}

////////////////////////// ERRORS /////////////////////////////
void Texmaker::LatexError() {
	QString overrideFileName=""; //workaround, see parseLogDocument for reason
	if (configManager.ignoreLogFileNames==2 ||
		(configManager.ignoreLogFileNames==1 && singlemode)) overrideFileName=getCurrentFileName();
	logModel->parseLogDocument(OutputLogTextEdit->document(), getCompileFileName(), overrideFileName);
	logpresent=true;

	//display latex errors in table
	OutputTable->resizeColumnsToContents();
	OutputTable->resizeRowsToContents();
	OutputTable2->resizeColumnsToContents();
	OutputTable2->resizeRowsToContents();
	//display errors in editor
	DisplayLatexError();
	if (logModel->found(LT_ERROR)) {
		OutputView->show();    //show log when error
		NextError();
		if (OutputLayout->currentIndex()==0) 
			SwitchToErrorList();
	}
}

void Texmaker::DisplayLatexError() {
	int errorMarkID = QLineMarksInfoCenter::instance()->markTypeId("error");
	int warningMarkID = QLineMarksInfoCenter::instance()->markTypeId("warning");
	int badboxMarkID = QLineMarksInfoCenter::instance()->markTypeId("badbox");
	for (int i=0; i<EditorView->count(); i++) {
		LatexEditorView *ed=qobject_cast<LatexEditorView *>(EditorView->widget(i));
		if (ed) {
			ed->editor->document()->removeMarks(errorMarkID);
			ed->editor->document()->removeMarks(warningMarkID);
			ed->editor->document()->removeMarks(badboxMarkID);
			ed->logEntryToLine.clear();
			ed->lineToLogEntries.clear();
		}
	}
	//backward, so the more important marks (with lower indices) will be inserted last and
	//returned first be QMultiHash.value
	for (int i = logModel->count()-1; i >= 0; i--)
		if (logModel->at(i).oldline!=-1)
			for (FilesMap::iterator it=filenames.begin(); it!=filenames.end(); ++it)
				if (it.value().endsWith(logModel->at(i).file)) {
					QDocumentLine l=it.key()->editor->document()->line(logModel->at(i).oldline-1);
					if (logModel->at(i).type==LT_ERROR) l.addMark(errorMarkID);
					else if (logModel->at(i).type==LT_WARNING) l.addMark(warningMarkID);
					else if (logModel->at(i).type==LT_BADBOX) l.addMark(badboxMarkID);
					it.key()->lineToLogEntries.insert(l.handle(),i);
					it.key()->logEntryToLine[i]=l.handle();
					break;
				}

	//OutputTable->show();
	OutputLogTextEdit->setCursorPosition(0 , 0);
}

bool Texmaker::NoLatexErrors() {
	return !logModel->found(LT_ERROR);
}

void Texmaker::GoToLogEntry(int logEntryNumber) {
	if (logEntryNumber<0 || logEntryNumber>=logModel->count()) return;
	//select entry in table view/log
	OutputTable->scrollTo(logModel->index(logEntryNumber,1),QAbstractItemView::PositionAtCenter);
	OutputLogTextEdit->setCursorPosition(logModel->at(logEntryNumber).logline, 0);
	//get editor containing log entry
	LatexEditorView* edView=0;
	for (FilesMap::iterator it=filenames.begin(); it!=filenames.end(); ++it)
		if (it.value().endsWith(logModel->at(logEntryNumber).file)) {
			edView=it.key();
			break;
		}
	if (!edView) {
		edView=load(logModel->at(logEntryNumber).file);
		if (!edView) {
			QMessageBox::warning(0,"TexMakerX",tr("Couldn't open file %1").arg(logModel->at(logEntryNumber).file));
			return;
		}
		DisplayLatexError(); //set marks
	}
	if (edView != currentEditorView()) EditorView->setCurrentWidget(edView);
	//get line
	QDocumentLineHandle* lh = edView->logEntryToLine.value(logEntryNumber, 0);
	if (!lh) return;
	//goto line
	gotoLine(QDocumentLine(lh).lineNumber());
}

void Texmaker::GoToLogEntryAt(int newLineNumber) {
	//goto line
	if (newLineNumber<0) return;
	gotoLine(newLineNumber);
	//find error number
	QDocumentLineHandle* lh=currentEditorView()->editor->document()->line(newLineNumber).handle();
	int logEntryNumber=currentEditorView()->lineToLogEntries.value(lh,-1);
	if (logEntryNumber==-1) return;
	//goto log entry
	OutputTable->scrollTo(logModel->index(logEntryNumber,1),QAbstractItemView::PositionAtCenter);
	OutputTable->selectRow(logEntryNumber);
	OutputLogTextEdit->setCursorPosition(logModel->at(logEntryNumber).logline, 0);

	QPoint p=currentEditorView()->editor->mapToGlobal(currentEditorView()->editor->mapFromContents(currentEditorView()->editor->cursor().documentPosition()));
	//  p.ry()+=2*currentEditorView()->editor->document()->fontMetrics().lineSpacing();
	QToolTip::showText(p, logModel->at(logEntryNumber).niceMessage(), 0);
	LatexEditorView::hideTooltipWhenLeavingLine=newLineNumber;
}

void Texmaker::GoToMark(bool backward, int id) {
	if (backward)
		GoToLogEntryAt(currentEditorView()->editor->document()->findPreviousMark(id,qMax(0,currentEditorView()->editor->cursor().lineNumber()-1),0));
	else
		GoToLogEntryAt(currentEditorView()->editor->document()->findNextMark(id,currentEditorView()->editor->cursor().lineNumber()+1));
}

void Texmaker::NextMark() {
	if (!currentEditorView()) return;
	GoToMark(false,-1);
}
void Texmaker::PreviousMark() {
	if (!currentEditorView()) return;
	GoToMark(true,-1);
}


void Texmaker::gotoNearLogEntry(LogType lt, bool backward, QString notFoundMessage) {
	if (!logpresent) {
		ViewLog();
	}
	if (logpresent) {
		if (logModel->found(lt))
			GoToMark(backward, logModel->markID(lt));
		else {
			QMessageBox::information(this,"TexMakerX",notFoundMessage);
			OutputTextEdit->setCursorPosition(0 , 0);
		}
	}
}
void Texmaker::NextError() {
	gotoNearLogEntry(LT_ERROR,false,tr("No LaTeX errors detected !"));
}

void Texmaker::PreviousError() {
	gotoNearLogEntry(LT_ERROR,true,tr("No LaTeX errors detected !"));	
}

void Texmaker::NextWarning() {
	gotoNearLogEntry(LT_WARNING,false,tr("No LaTeX warnings detected !"));
}
void Texmaker::PreviousWarning() {
	gotoNearLogEntry(LT_WARNING,true,tr("No LaTeX warnings detected !"));
}

void Texmaker::NextBadBox() {
	gotoNearLogEntry(LT_BADBOX,false,tr("No bad boxes detected !"));
}
void Texmaker::PreviousBadBox() {
	gotoNearLogEntry(LT_BADBOX,true,tr("No bad boxes detected !"));
}
//////////////// HELP /////////////////
void Texmaker::LatexHelp() {
	QString latexHelp=findResourceFile("latexhelp.html");
	if (latexHelp!="") QDesktopServices::openUrl("file:///"+latexHelp);
	else QMessageBox::warning(this,tr("Error"),tr("File not found"));
}

void Texmaker::UserManualHelp() {
	QString locale = QString(QLocale::system().name()).left(2);
	if (locale.length() < 2 || locale!="fr") locale = "en";
	QString latexHelp=findResourceFile("usermanual_"+locale+".html");
	if (latexHelp!="") QDesktopServices::openUrl("file:///"+latexHelp);
	else QMessageBox::warning(this,tr("Error"),tr("File not found"));
}

void Texmaker::HelpAbout() {
	AboutDialog *abDlg = new AboutDialog(this);
	abDlg->exec();
}
////////////// OPTIONS //////////////////////////////////////
void Texmaker::GeneralOptions() {
	ConfigDialog *confDlg = configManager.createConfigDialog(this);

	confDlg->ui.lineEditLatex->setText(latex_command);
	confDlg->ui.lineEditPdflatex->setText(pdflatex_command);
	confDlg->ui.lineEditDvips->setText(dvips_command);
	confDlg->ui.lineEditDviviewer->setText(viewdvi_command);
	confDlg->ui.lineEditPsviewer->setText(viewps_command);
	confDlg->ui.lineEditDvipdfm->setText(dvipdf_command);
	confDlg->ui.lineEditPs2pdf->setText(ps2pdf_command);
	confDlg->ui.lineEditBibtex->setText(bibtex_command);
	confDlg->ui.lineEditMakeindex->setText(makeindex_command);
	confDlg->ui.lineEditPdfviewer->setText(viewpdf_command);
	confDlg->ui.lineEditMetapost->setText(metapost_command);
	confDlg->ui.lineEditGhostscript->setText(ghostscript_command);
	confDlg->ui.lineEditExecuteBeforeCompiling->setText(precompile_command);

	confDlg->ui.checkBoxWordwrap->setChecked(wordwrap);
	confDlg->ui.checkBoxTabbedLogView->setChecked(tabbedLogView);
	switch (showlinemultiples) {
	case 0:
		confDlg->ui.comboboxLineNumbers->setCurrentIndex(0);
		break;
	case 10:
		confDlg->ui.comboboxLineNumbers->setCurrentIndex(2);
		break;
	default:
		confDlg->ui.comboboxLineNumbers->setCurrentIndex(1);
	}
	confDlg->ui.checkBoxAutoIndent->setChecked(autoindent);
	confDlg->ui.checkBoxCompletion->setChecked(completion);
	confDlg->ui.checkBoxFolding->setChecked(folding);
	confDlg->ui.checkBoxLineState->setChecked(showlinestate);
	confDlg->ui.checkBoxState->setChecked(showcursorstate);
	confDlg->ui.checkBoxRealTimeCheck->setChecked(realtimespellchecking);

	confDlg->ui.lineEditAspellCommand->setText(spell_dic);

	if (quickmode==1) {
		confDlg->ui.radioButton1->setChecked(true);
		confDlg->ui.lineEditUserquick->setEnabled(false);
	}
	if (quickmode==2) {
		confDlg->ui.radioButton2->setChecked(true);
		confDlg->ui.lineEditUserquick->setEnabled(false);
	}
	if (quickmode==3) {
		confDlg->ui.radioButton3->setChecked(true);
		confDlg->ui.lineEditUserquick->setEnabled(false);
	}
	if (quickmode==4)  {
		confDlg->ui.radioButton4->setChecked(true);
		confDlg->ui.lineEditUserquick->setEnabled(false);
	}
	if (quickmode==5)  {
		confDlg->ui.radioButton5->setChecked(true);
		confDlg->ui.lineEditUserquick->setEnabled(false);
	}
	if (quickmode==6)  {
		confDlg->ui.radioButton6->setChecked(true);
		confDlg->ui.lineEditUserquick->setEnabled(true);
	}
	confDlg->ui.lineEditUserquick->setText(userquick_command);


	//completion words
	configManager.words=completerFiles;


	if (configManager.execConfigDialog(confDlg)) {
		if (confDlg->ui.radioButton1->isChecked()) quickmode=1;
		if (confDlg->ui.radioButton2->isChecked()) quickmode=2;
		if (confDlg->ui.radioButton3->isChecked()) quickmode=3;
		if (confDlg->ui.radioButton4->isChecked()) quickmode=4;
		if (confDlg->ui.radioButton5->isChecked()) quickmode=5;
		if (confDlg->ui.radioButton6->isChecked()) quickmode=6;
		userquick_command=confDlg->ui.lineEditUserquick->text();

		latex_command=confDlg->ui.lineEditLatex->text();
		pdflatex_command=confDlg->ui.lineEditPdflatex->text();
		dvips_command=confDlg->ui.lineEditDvips->text();
		viewdvi_command=confDlg->ui.lineEditDviviewer->text();
		viewps_command=confDlg->ui.lineEditPsviewer->text();
		dvipdf_command=confDlg->ui.lineEditDvipdfm->text();
		ps2pdf_command=confDlg->ui.lineEditPs2pdf->text();
		bibtex_command=confDlg->ui.lineEditBibtex->text();
		makeindex_command=confDlg->ui.lineEditMakeindex->text();
		viewpdf_command=confDlg->ui.lineEditPdfviewer->text();
		metapost_command=confDlg->ui.lineEditMetapost->text();
		ghostscript_command=confDlg->ui.lineEditGhostscript->text();
		precompile_command=confDlg->ui.lineEditExecuteBeforeCompiling->text();

		wordwrap=confDlg->ui.checkBoxWordwrap->isChecked();
		completion=confDlg->ui.checkBoxCompletion->isChecked();
		autoindent=confDlg->ui.checkBoxAutoIndent->isChecked();
		switch (confDlg->ui.comboboxLineNumbers->currentIndex()) {
		case 0:
			showlinemultiples=0;
			break;
		case 2:
			showlinemultiples=10;
			break;
		default:
			showlinemultiples=1;
			break;
		}
		spell_dic=confDlg->ui.lineEditAspellCommand->text();
		folding=confDlg->ui.checkBoxFolding->isChecked();
		showlinestate=confDlg->ui.checkBoxLineState->isChecked();
		showcursorstate=confDlg->ui.checkBoxState->isChecked();
		realtimespellchecking=confDlg->ui.checkBoxRealTimeCheck->isChecked();

		// read checkbox and set logViewer accordingly
		tabbedLogView=confDlg->ui.checkBoxTabbedLogView->isChecked();
		if (tabbedLogView) {
			OutputView->setTitleBarWidget(logViewerTabBar);
			OutputTable->hide();
		} else {
			OutputView->setTitleBarWidget(0);
			OutputTable->show();
		}

		mainSpeller->setActive(realtimespellchecking);
		mainSpeller->loadDictionary(spell_dic,configManager.configFileNameBase);

		if (currentEditorView()) {
			FilesMap::Iterator it;
			for (it = filenames.begin(); it != filenames.end(); ++it)
				updateEditorSetting(it.key());
			UpdateCaption();
			OutputTextEdit->clear();
			logViewerTabBar->setCurrentIndex(0);
			//OutputTable->hide();
			//OutputTextEdit->insertLine("Editor settings apply only to new loaded document.");
		}
		//completion words
		readCompletionList(configManager.words);
		completerFiles=configManager.words;
		updateCompleter();
	}
	delete confDlg;
}
void Texmaker::executeCommandLine(const QStringList& args, bool realCmdLine) {
	// remove unused argument warning
	Q_UNUSED(realCmdLine);

	// parse command line
	QString fileToLoad;
	bool activateMasterMode = false;
	int line=-1;
	for (int i = 0; i < args.size(); ++i) {
		if (args[i]=="") continue;
		if (args[i][0] != '-')  fileToLoad=args[i];
		//-form is for backward compatibility
		if (args[i] == "--master") activateMasterMode=true;
		if (args[i] == "--line" && i+1<args.size())  line=args[++i].toInt()-1;
	}

	// execute command line
	QFileInfo ftl(fileToLoad);
	if (fileToLoad != "") {
		if (ftl.exists())
			load(fileToLoad, activateMasterMode);
		else if (ftl.absoluteDir().exists()) {
			fileNew(ftl.absoluteFilePath());
			if (activateMasterMode) {
				if (singlemode) ToggleMode();
				else {
					ToggleMode();
					ToggleMode();
				}
			}
			return ;
		}
	}

	if (line!=-1){
		QApplication::processEvents();
		gotoLine(line);
	}
}
void Texmaker::onOtherInstanceMessage(const QString &msg) { // Added slot for messages to the single instance
	show();
	activateWindow();
	executeCommandLine(msg.split("#!#"),false);
}
void Texmaker::ToggleMode() {
//QAction *action = qobject_cast<QAction *>(sender());
	if (!singlemode) {
		ToggleAct->setText(tr("Define Current Document as 'Master Document'"));
		OutputTextEdit->clear();
		logViewerTabBar->setCurrentIndex(0);
		//OutputTable->hide();
		logpresent=false;
		singlemode=true;
		stat1->setText(QString(" %1 ").arg(tr("Normal Mode")));
		return;
	}
	if (singlemode && currentEditorView()) {
		MasterName=getName();
		if (MasterName=="untitled" || MasterName=="") {
			QMessageBox::warning(this,tr("Error"),tr("Could not start the command."));
			return;
		}
		QString shortName = MasterName;
		int pos;
		while ((pos = (int)shortName.indexOf('/')) != -1) shortName.remove(0,pos+1);
		ToggleAct->setText(tr("Normal Mode (current master document :")+shortName+")");
		singlemode=false;
		stat1->setText(QString(" %1 ").arg(tr("Master Document")+ ": "+shortName));
		MarkCurrentFileAsRecent();
		return;
	}
}
////////////////// VIEW ////////////////

void Texmaker::gotoNextDocument() {
	if (EditorView->count() < 2) return;
	int cPage = EditorView->currentIndex() + 1;
	if (cPage >= EditorView->count()) EditorView->setCurrentIndex(0);
	else EditorView->setCurrentIndex(cPage);
}

void Texmaker::gotoPrevDocument() {
	if (EditorView->count() < 2) return;
	int cPage = EditorView->currentIndex() - 1;
	if (cPage < 0) EditorView->setCurrentIndex(EditorView->count() - 1);
	else EditorView->setCurrentIndex(cPage);
}

void Texmaker::viewCollapseEverything() {
	if (!currentEditorView()) return;
	currentEditorView()->foldEverything(false);
}
void Texmaker::viewCollapseLevel() {
	if (!currentEditorView()) return;
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	currentEditorView()->foldLevel(false,action->data().toInt());
}
void Texmaker::viewCollapseBlock() {
	if (!currentEditorView()) return;
	currentEditorView()->foldBlockAt(false,currentEditorView()->editor->cursor().lineNumber());
}
void Texmaker::viewExpandEverything() {
	if (!currentEditorView()) return;
	currentEditorView()->foldEverything(true);
}
void Texmaker::viewExpandLevel() {
	if (!currentEditorView()) return;
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	currentEditorView()->foldLevel(true,action->data().toInt());
}
void Texmaker::viewExpandBlock() {
	if (!currentEditorView()) return;
	currentEditorView()->foldBlockAt(true,currentEditorView()->editor->cursor().lineNumber());
}


void Texmaker::gotoBookmark() {
	if (!currentEditorView()) return;
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	currentEditorView()->jumpToBookmark(action->data().toInt());
}
void Texmaker::toggleBookmark() {
	if (!currentEditorView()) return;
	QAction *action = qobject_cast<QAction*>(sender());
	if (!action) return;
	currentEditorView()->toggleBookmark(action->data().toInt()); //-1 is unnamed bookmark
}

//*********************************
void Texmaker::dragEnterEvent(QDragEnterEvent *event) {
	if (event->mimeData()->hasFormat("text/uri-list")) event->acceptProposedAction();
}

void Texmaker::dropEvent(QDropEvent *event) {
	QRegExp rx("file://(.*\\.(?:tex|bib|sty|cls|mp))");
	QList<QUrl> uris=event->mimeData()->urls();
	QString uri;
	for (int i = 0; i < uris.size(); ++i) {
		uri=uris.at(i).toString();
		//QMessageBox::information(0,uri,uri,0);
		if (rx.exactMatch(uri)) load(rx.cap(1));
	}
	event->acceptProposedAction();
}

//***********************************
void Texmaker::SetMostUsedSymbols() {
	for (int i = 0; i <=11; ++i) symbolMostused[i]=-1;
	QList<int> list_num, list_score;
	list_num.clear();
	list_score.clear();
	for (int i=0; i <412 ; i++) {
		list_num.append(i);
		list_score.append(symbolScore[i]);
	}
	int max;
	for (int i = 0; i < 412; i++) {
		max=i;
		for (int j = i+1; j < 412; j++) {
			if (list_score.at(j)>list_score.at(max)) max=j;
		}
		if (max!=i) {
			list_num.swap(i,max);
			list_score.swap(i,max);
		}
	}
	for (int i = 0; i <=11; ++i) {
		if (list_score.at(i)>0) symbolMostused[i]=list_num.at(i);
	}
	MostUsedListWidget->SetUserPage(symbolMostused);
}

void Texmaker::readCompletionList(const QStringList &files) {
	completerWords.clear();
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	foreach(QString file, files) {
		QString fn=findResourceFile("completion/"+file);
		QFile tagsfile(fn);
		if (tagsfile.open(QFile::ReadOnly)) {
			QString line;
			while (!tagsfile.atEnd()) {
				line = tagsfile.readLine();
				if (!line.isEmpty() && !line.startsWith("#") && !line.startsWith(" ")) {
					if (line.startsWith("\\pageref")||line.startsWith("\\ref")) continue;
					if (!line.contains("%")){
						if (line.contains("{")) {
							line.replace("{","{%<");
							line.replace("}","%>}");
						}
						if (line.contains("(")) {
							line.replace("(","(%<");
							line.replace(")","%>)");
						}
						if (line.contains("[")) {
							line.replace("[","[%<");
							line.replace("]","%>]");
						}
						int i;
						if (line.startsWith("\\begin")||line.startsWith("\\end")) {
							i=line.indexOf("%<",0);
							line.replace(i,2,"");
							i=line.indexOf("%>",0);
							line.replace(i,2,"");
							if (line.endsWith("\\item\n")) {
								line.chop(6);
							}
						}
						i=line.indexOf("%<",0);
						line.replace(i,2,"%|%<");
						i=line.indexOf("%>",0);
						line.replace(i,2,"%>%|");
					}
					completerWords.append(line.trimmed());
				}
			}
		}
	}
	QApplication::restoreOverrideCursor();
	//updateCompleter();
}


void Texmaker::updateCompleter() {
	QStringList words;

	words=completerWords;
	words.append(userCommandList);
	for (int i=0; i<labelitem.count(); ++i) {
		words.append("\\ref{"+labelitem.at(i)+"}");
		words.append("\\pageref{"+labelitem.at(i)+"}");
	}


	completer->setWords(words);

	if (!LatexCompleter::hasHelpfile()) {
		QFile f(findResourceFile("latexhelp.html"));
		if (!f.open(QIODevice::ReadOnly| QIODevice::Text))  LatexCompleter::parseHelpfile("<missing>");
		else LatexCompleter::parseHelpfile(QTextStream(&f).readAll());
	}
}

void Texmaker::tabChanged(int i) {
	if (i>0 && !logpresent) RealViewLog();
}

void Texmaker::StructureContextMenu(QPoint point) {
	QTreeWidgetItem* item=StructureTreeWidget->currentItem();
	if(item->parent()&&item->text(0)!="LABELS"&&item->text(1)!="include"&&item->text(1)!="input"){
		if (item->parent()->text(0)=="LABELS") {
			QMenu menu;
			menu.addAction(tr("Insert"),this, SLOT(editPasteRef()));
			menu.addAction(tr("Insert as %1").arg("\\ref{...}"),this, SLOT(editPasteRef()));
			menu.addAction(tr("Insert as %1").arg("\\pageref{...}"),this, SLOT(editPasteRef()));
			menu.exec(StructureTreeWidget->mapToGlobal(point));
		} else {
			QMenu menu(this);
			menu.addAction(tr("Copy"),this, SLOT(editSectionCopy()));
			menu.addAction(tr("Cut"),this, SLOT(editSectionCut()));
			menu.addAction(tr("Paste before"),this, SLOT(editSectionPasteBefore()));
			menu.addAction(tr("Paste after"),this, SLOT(editSectionPasteAfter()));
			menu.addSeparator();
			menu.addAction(tr("Indent Section"),this, SLOT(editIndentSection()));
			menu.addAction(tr("Unindent Section"),this, SLOT(editUnIndentSection()));
			menu.exec(StructureTreeWidget->mapToGlobal(point));
		}
	}
}

void Texmaker::editPasteRef() {
	if (!currentEditorView()) return;

	QAction *action = qobject_cast<QAction *>(sender());
	QString name=action->text();
	if (name==tr("Insert")) {
		QDocumentCursor m_cursor=currentEditorView()->editor->cursor();
		QTreeWidgetItem* item=StructureTreeWidget->currentItem();
		m_cursor.insertText(item->text(3));
	} else {
		name.remove(0,name.indexOf("\\"));
		name.chop(name.length()-name.indexOf("{"));
		QDocumentCursor m_cursor=currentEditorView()->editor->cursor();
		QTreeWidgetItem* item=StructureTreeWidget->currentItem();
		m_cursor.insertText(name+"{"+item->text(3)+"}");
	}
}

void removeDeletedLineHandle(QTreeWidgetItem* item, QDocumentLineHandle* l){
	for (int i=0; i< item->childCount(); i++)
		removeDeletedLineHandle(item,l);
	if (item->data(Texmaker::structureTreeLineColumn,Qt::UserRole).value<QDocumentLineHandle*>() == l) 
		item->setData(Texmaker::structureTreeLineColumn,Qt::UserRole,QVariant());
}

void Texmaker::lineHandleDeleted(QDocumentLineHandle* l){
	for (int i=0;i<StructureTreeWidget->topLevelItemCount();i++)
		removeDeletedLineHandle(StructureTreeWidget->topLevelItem(i),l);
}
