/***************************************************************************
 *   copyright       : (C) 2003-2007 by Pascal Brachet                     *
 *   http://www.xm1math.net/texmaker/                                      *
 *                         2008-2009 by Benito van der Zander              *
 *   http://texstudio.sourceforge.net
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TEXMAKER_H
#define TEXMAKER_H

#include "mostQtHeaders.h"

#include "bibtexparser.h"
#include "buildmanager.h"
#include "bookmarks.h"
#include "codesnippet.h"
#include "configmanager.h"
#include "cursorhistory.h"
#include "latexlog.h"
#include "latexdocument.h"
#include "latexeditorview.h"
#include "latexcompleter.h"
//#include "symbollistwidget.h"
#include "symbolgridwidget.h"
#include "xmltagslistwidget.h"
#include "spellerdialog.h"
#include "textanalysis.h"
#include "toolwidgets.h"
#include "txstabwidget.h"
#include "unicodeinsertion.h"
#include "tablemanipulation.h"
#include "PDFDocument.h"
#include <QSplitter>
#include <QSplashScreen>
#include "session.h"
#include "qformatfactory.h"
#include "qlanguagefactory.h"
#include "qlinemarksinfocenter.h"
#include "latexstyleparser.h"
#include "diffoperations.h"

typedef QHash<QString,int> SymbolList;

enum SVNSTATUS {
	Unknown=0,
	Unmanaged,
	Modified,
	Locked,
	CheckedIn,
	InConflict
};

class UserMenuDialog;
class GrammarCheck;
class FileSelector;

class Texmaker : public QMainWindow {
	Q_OBJECT
	
public:
	Texmaker(QWidget *parent = 0, Qt::WFlags flags = 0, QSplashScreen *splash = 0);
	~Texmaker();
	
	Q_INVOKABLE QString getCurrentFileName(); //returns the absolute file name of the current file or "" if none is opene
	Q_INVOKABLE QString getAbsoluteFilePath(const QString & relName, const QString &extension=""); //treats the path relative to the compiled .tex file
    Q_INVOKABLE QString getRelativeFileName(const QString & file, QString basepath, bool keepSuffix=false); //provide function for scripts
	QByteArray windowstate;
	bool tobemaximized,tobefullscreen;
	
public slots:
    LatexEditorView* load(const QString &f , bool asProject = false, bool hidden = false);
	void executeCommandLine(const QStringList& args, bool realCmdLine);
	void onOtherInstanceMessage(const QString &);  // For messages for the single instance
	
	void fuzzCursorHistory();
	void fuzzBackForward();


protected:
	//these are just wrappers around configManager so we don't have to type so much (todo??? move them to configmanager.h and use a singleton design???)
	Q_INVOKABLE inline QMenu* newManagedMenu(const QString &id,const QString &text){ return configManager.newManagedMenu(id,text); }
	Q_INVOKABLE inline QMenu* newManagedMenu(QMenu* menu, const QString &id,const QString &text){ return configManager.newManagedMenu(menu,id,text); }
	Q_INVOKABLE QAction* newManagedAction(QWidget* menu, const QString &id,const QString &text, const char* slotName=0, const QKeySequence &shortCut=0, const QString & iconFile="", const QList<QVariant>& args = QList<QVariant>());
	Q_INVOKABLE QAction* newManagedAction(QWidget* menu, const QString &id,const QString &text, const char* slotName, const QList<QKeySequence> &shortCuts, const QString & iconFile="", const QList<QVariant>& args = QList<QVariant>());
	Q_INVOKABLE QAction* newManagedEditorAction(QWidget* menu, const QString &id,const QString &text, const char* slotName=0, const QKeySequence &shortCut=0, const QString & iconFile="", const QList<QVariant>& args = QList<QVariant>());
	Q_INVOKABLE QAction* newManagedEditorAction(QWidget* menu, const QString &id,const QString &text, const char* slotName, const QList<QKeySequence> &shortCuts, const QString & iconFile="", const QList<QVariant>& args = QList<QVariant>());
	Q_INVOKABLE inline QAction* newManagedAction(QWidget* menu, const QString &id, QAction* act) { return configManager.newManagedAction(menu,id,act); }
	Q_INVOKABLE inline QMenu* getManagedMenu(const QString& id){ return configManager.getManagedMenu(id); }
	Q_INVOKABLE inline QAction* getManagedAction(const QString& id){ return configManager.getManagedAction(id); }
	Q_INVOKABLE inline QList<QAction *> getManagedActions(const QStringList& ids, const QString &commonPrefix=""){ return configManager.getManagedActions(ids, commonPrefix); }
	Q_INVOKABLE QAction* insertManagedAction(QAction* before, const QString &id,const QString &text, const char* slotName=0, const QKeySequence &shortCut=0, const QString & iconFile="");
	
	SymbolGridWidget* addSymbolGrid(const QString& SymbolList,  const QString& iconName, const QString& text);
	void addTagList(const QString& id, const QString& iconName, const QString& text, const QString& tagFile);
	
	QVariantMap *MapForSymbols;
	
private slots:
	void updateToolBarMenu(const QString& menuName);
	void showTestProgress(const QString &message);
private:
	void executeTests(const QStringList &args);
	void generateAddtionalTranslations();
	void setupMenus();
	void setupDockWidgets();
	void setupToolBars();
	void createStatusBar();
	bool FocusEditorForFile(QString f, bool checkTemporaryNames = false);
	bool closeAllFilesAsking(); //asks the user and close all files
	bool canCloseNow(); //asks the user and close all files, and prepares to exit txs
	void closeEvent(QCloseEvent *e);
	
	void updateUserMacros(bool updateMenu = true);
	
	QFormatFactory *m_formats;
	QLanguageFactory* m_languages;
	LatexCompleter* completer;
	
	QPointer<UnicodeInsertion> unicodeInsertionDialog;
	
	//gui
	TxsTabWidget *EditorTabs;
	QSplitter *splitter;
	QFrame *centralFrame;
	QToolBar* centralToolBar;
	CustomWidgetList *leftPanel;
	SymbolGridWidget *MostUsedSymbolWidget, *FavoriteSymbolWidget;
	QString hiddenLeftPanelWidgets;
	
	QTreeView *structureTreeView;
	LatexParser latexParser;
public:
	LatexDocuments documents;
private:
	OutputViewWidget *outputView; //contains output widgets (over OutputLayout)
	
	//menu
	QActionGroup *bibtexEntryActions;
	QActionGroup *biblatexEntryActions;
	QActionGroup *bibTypeActions;
	QActionGroup *highlightLanguageActions;

	//toolbars
	QAction *ToggleAct, *ToggleRememberAct;
	QAction *actSave, *actUndo, *actRedo;
	
	QLabel *statusLabelMode, *statusLabelProcess;
	QToolButton *statusTbLanguage;
	QToolButton *statusTbEncoding;
	QActionGroup *spellLanguageActions;
	
	CursorHistory *cursorHistory;

	//settings
	ConfigManager configManager;
public:
	BuildManager buildManager;
private:
	QStringList struct_level;
	
	qreal pdfSplitterRel;
	
	SpellerManager spellerManager;
	SafeThread grammarCheckThread;
	GrammarCheck *grammarCheck;
	Bookmarks *bookmarks;
	
	//dialogs
	TextAnalysisDialog *textAnalysisDlg;
	SpellerDialog *spellDlg;
	
	//tools
	usercodelist symbolMostused;
	QStringList symbolFavorites;
	
	Q_INVOKABLE LatexEditorView *currentEditorView() const;
	Q_INVOKABLE QEditor* currentEditor() const;
	void configureNewEditorView(LatexEditorView *edit);
    void configureNewEditorViewEnd(LatexEditorView *edit,bool asMaster=false, bool hidden = false);
	LatexEditorView* getEditorViewFromFileName(const QString &fileName, bool checkTemporaryNames = false);
	
	QAction *fullscreenModeAction;
	
	int runningPDFCommands, runningPDFAsyncCommands;

	void updateUserToolMenu();
	void linkToEditorSlot(QAction* act, const char* slot, const QList<QVariant>& args);
private slots:
	void relayToEditorSlot();
	void relayToOwnSlot();
	void autoRunScripts();
	void runScripts(int trigger);
	
protected slots:
	void fileNew(QString fileName="");
	void fileNewFromTemplate();
	void fileMakeTemplate();
	void fileOpen();
	void fileRestoreSession(bool showProgress = true);
	void fileSave();
	void fileSaveAll();
	void fileSaveAll(bool alsoUnnamedFiles, bool alwaysCurrentFile);
private slots:
	void fileNewInternal(QString fileName="");
	void fileSaveAs(const QString& fileName = "");
protected slots:	
	void fileClose();
	void fileCloseAll();
	void fileExit();
	void fileOpenRecent();
	void fileOpenAllRecent();
	void fileRecentList();
	void fileDocumentOpenFromChoosen(const QString& doc, int duplicate, int lineNr, int column);
	void viewDocumentList();
	void viewDocumentOpenFromChoosen(const QString& doc, int duplicate, int lineNr, int column);
	void fileOpenFirstNonOpen();
	void fileOpenRecentProject();
	void fileLoadSession();
	void loadSession(const QString &fileName);
	void fileSaveSession();
private slots:
	void restoreSession(const Session &s, bool showProgress = true);
	Session getCurrentSession();
protected slots:
	void MarkCurrentFileAsRecent();
private slots:
	void fileCheckin(QString filename="");
	void fileLockPdf(QString filename="");
	void fileCheckinPdf(QString filename="");
	void fileUpdate(QString filename="");
	void fileUpdateCWD(QString filename="");
	void checkin(QString fn,QString text="txs auto checkin",bool blocking=false);
	bool svnadd(QString fn,int stage=0);
	void svncreateRep(QString fn);
	void svnUndo(bool redo=false);
	void svnPatch(QEditor *ed,QString diff);
	void svnLock(QString fn);
	SVNSTATUS svnStatus(QString filename);
	void showOldRevisions();
	QStringList svnLog();
	void changeToRevision(QString rev,QString old_rev="");
	void svnDialogClosed();
	void fileDiff();
	void fileDiff3();
	bool checkSVNConflicted(bool substituteContents=true);
	void removeDiffMarkers(bool theirs=false);
	void editChangeDiff(QPoint pt);
	void jumpNextDiff();
	void jumpPrevDiff();
	void fileDiffMerge();
	void declareConflictResolved();
protected slots:	
	void openExternalFile(const QString& name,const QString& defaultExt="tex",LatexDocument *doc=0); // signaled by latexViewer to open specific file
	
	void editUndo();
	void editRedo();
	void editDebugUndoStack();
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
	void editHardLineBreak();
	void editHardLineBreakRepeat();
	void editEraseLine();
	void editEraseEndLine();
	void editMoveLineUp();
	void editMoveLineDown();
	void editDuplicateLine();
	void editEraseWordCmdEnv();
	void editGotoDefinition(QDocumentCursor c = QDocumentCursor());
	void editSpell();
	void editThesaurus(int line=-1,int col=-1);
	void editChangeLineEnding();
	void editSetupEncoding();
	void editInsertUnicode();
	void editInsertRefToNextLabel(bool backward=false);
	void editInsertRefToPrevLabel();
	void editFindGlobal();
	
	void findWordRepetions();
	void findNextWordRepetion();
	
	void StructureContextMenu(const QPoint& point);
	void structureContextMenuCloseDocument();
	void structureContextMenuSwitchMasterDocument();
	void SymbolGridContextMenu(QWidget* widget, const QPoint& point);
	void MostUsedSymbolsTriggered(bool direct=false);
	void symbolAddFavorite();
	void symbolRemoveFavorite();
	void symbolRemoveAllFavorites();
	
	void editorTabContextMenu(const QPoint& point);

	void addDocToLoad(QString filename);

	void moveCursorTodlh();

private slots:	
	void ReadSettings(bool reread=false);
	void SaveSettings(const QString& configName="");
	
protected slots:	
	void showMarkTooltipForLogMessage(QList<int> errors);
	void NewDocumentStatus();
	void NewDocumentLineEnding();
	void UpdateCaption();
	void updateMasterDocumentCaption();
	void updateUndoRedoStatus();
	void editorTabChanged(int index);
	void CloseEditorTab(int tab);
	void EditorTabMoved(int from,int to);
	
    void updateStructure(bool initial=false, LatexDocument *doc = 0);
	void ShowStructure();
	void clickedOnStructureEntry(const QModelIndex & index);
	void editRemovePlaceHolders();

	void NormalCompletion();
	void InsertEnvironmentCompletion();
	void InsertTextCompletion();
	void InsertTag(const QString &Entity, int dx=0, int dy=0);
	void InsertCitation(const QString &text);
	void InsertFormula(const QString &formula);
	void InsertSymbol(QTableWidgetItem *item);
	void InsertSymbolPressed(QTableWidgetItem *item);
	void InsertXmlTag(QListWidgetItem *item);	
	void insertXmlTagFromToolButtonAction();
	void callToolButtonAction();
	void InsertFromAction();
	void InsertBib();
	void InsertStruct();
	
	void InsertBibEntryFromAction();
	void InsertBibEntry(const QString& id="");
	void SetBibTypeFromAction();
	
	void insertUserTag(const QString& macro, int triggerId = 0);
	void insertUserTag();
	void editMacros();
	void macroDialogAccepted();
	
	void InsertRef(const QString &refCmd);
	void InsertRef();
	void InsertEqRef();
	void InsertPageRef();
	void createLabelFromAction();

	void changeTextCodec();
	void UpdateAvailableLanguages();
	void EditorSpellerChanged(const QString &name);
	void ChangeEditorSpeller();
	void InsertSpellcheckMagicComment();
    void addMagicRoot();
    void addMagicCoding();
	
	void QuickTabular();
	void QuickArray();
	void QuickTabbing();
	void QuickLetter();
	void QuickDocument();
	void QuickGraphics(const QString &graphicsFile = QString());
	void QuickMath();
	
	bool checkProgramPermission(const QString& program, const QString& cmdId, LatexDocument* master);
	void runInternalPdfViewer(const QFileInfo& master, const QString& options);
	void runBibliographyIfNecessary(const QFileInfo& cmd);
private slots:	
	void runInternalCommand(const QString& cmd, const QFileInfo& master, const QString& options);
	void commandLineRequested(const QString& cmdId, QString* result, bool*);
	void beginRunningCommand(const QString& commandMain, bool latex, bool pdf, bool async);
	void beginRunningSubCommand(ProcessX* p, const QString& commandMain, const QString& subCommand, const RunCommandFlags& flags);
	void endRunningSubCommand(ProcessX* p, const QString& commandMain, const QString& subCommand, const RunCommandFlags& flags);
	void endRunningCommand(const QString& commandMain, bool latex, bool pdf, bool async);
	
	
	bool runCommand(const QString& commandline, QString* buffer = 0);
protected slots:	
	void processNotification(const QString& message);
	void CleanAll();
	void commandFromAction();  //calls a command given by sender.data, doesn't wait
	
	void WebPublish();
	void WebPublishSource();
	void AnalyseText();
	void AnalyseTextFormDestroyed();
	void GenerateRandomText();
	
	void RealViewLog(bool noTabChange=false);
	void ViewLog(bool noTabChange=false);
	void ViewLogOrReRun(LatexCompileResult* result);
	void DisplayLatexError();
	bool gotoNearLogEntry(int lt, bool backward, QString notFoundMessage);
	bool NoLatexErrors();
	bool LogExists();
	void ClearMarkers();
	/////
	void LatexHelp();
	void UserManualHelp();
	void TexdocHelp();
	void HelpAbout();
	
	void GeneralOptions();
	void ToggleMode();
	
	void gotoNextDocument();
	void gotoPrevDocument();
	void gotoOpenDocument();
	void updateOpenDocumentMenu(bool localChange=false);
	
	void focusEditor();
	void focusViewer();

	void viewCloseSomething();
	void setFullScreenMode();
	void viewAlignWindows();
	void viewSetHighlighting(QAction *act);
	void showHighlightingMenu();
	
	void viewCollapseBlock();
	void viewExpandBlock();
	
	QObject* newPdfPreviewer(bool embedded=false);
	void pdfClosed();
	void restoreMacMenuBar();
	
	void masterDocumentChanged(LatexDocument * doc);
	void aboutToDeleteDocument(LatexDocument * doc);
	
	void SetMostUsedSymbols(QTableWidgetItem* item);
	
    void updateCompleter(LatexEditorView *edView = 0);
	void completerNeedsUpdate();
	void needUpdatedCompleter();
	
	void tabChanged(int i);
	
	void gotoLine(int line, int col=0, LatexEditorView *edView = 0);  // line is 0 based
	void gotoLocation(int line, const QString &fileName);  //0 based, absolute file name
	void gotoLogEntryEditorOnly(int logEntryNumber);
	bool gotoLogEntryAt(int newLineNumber);
	bool gotoMark(bool backward, int id=-1);
	void syncFromViewer(const QString &fileName, int line, bool activate, const QString& guessedWord);  //0 based, absolute file name, raise window
	
	void goBack();
	void goForward();
	void setGlobalCursor(const QDocumentCursor &c);
	void saveCurrentCursorToHistory();

	void previewLatex();
	void previewAvailable(const QString& imageFile, const PreviewSource& source);
	void clearPreview();
	void showPreview(const QString& text);
	void showPreview(const QDocumentCursor& c);
	void showPreview(const QDocumentCursor& c, bool addToList);
	void showPreviewQueue();
    void showImgPreview(const QString& fname);
    void showImgPreviewFinished(const QPixmap& pm, int page);
	
	void templateEdit(const QString &fname);
	
	void fileReloaded();
	void fileInConflict();
	void fileAutoReloading(QString fname);
	
	void jumpToSearch(QDocument* doc,int lineNumber);
	
	void cursorPositionChanged();
	void syncPDFViewer(QDocumentCursor cur, bool inForeground = true);
	//	void treeWidgetChanged();
	
	bool generateMirror(bool setCur=false);
	void generateBracketInverterMirror();
	void jumpToBracket();
	void selectBracket();
	void findMissingBracket();
	void cursorHovered();
	void loadProfile();
	void saveProfile();
	
	void addRowCB();
	void addColumnCB();
	void removeColumnCB();
	void removeRowCB();
	void cutColumnCB();
	void pasteColumnCB();
	void addHLineCB();
	void remHLineCB();
	void insertTableTemplate();
    void alignTableCols();
	
	void latexModelViewMode();
	void moveDocumentToFront();
	void moveDocumentToEnd();
	void moveDocumentToDest(int dest);
	
	void updateHighlighting();
	
	void toggleGrammar(int type);

private slots:
	void importPackage(QString name);
	void packageScanCompleted(QString name);
	void stopPackageParser();
	void packageParserFinished();
protected:
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);
	virtual void changeEvent(QEvent *e);
	virtual void resizeEvent(QResizeEvent *e);
	
	Qt::MouseButtons mb;
	
	bool mCompleterNeedsUpdate;
	
	bool previewEquation;
	int currentLine;
	QBrush oldBackground;
	bool mDontScrollToItem;
	
	QByteArray stateFullScreen;
	
	QDialog *svndlg;
	UserMenuDialog *userMacroDialog;
	QComboBox *cmbLog;
	
	QStringList m_columnCutBuffer;
	
	QTimer autosaveTimer;
	
	bool completionBaseCommandsUpdated;
	
	QSet<int> previewQueue;
	LatexEditorView* previewQueueOwner;
	
	QString fileFilters;
	QString selectedFileFilter;

	QWeakPointer<FileSelector> fileSelector;
	
	bool runBibliographyIfNecessaryEntered;
	
	LatexStyleParser *latexStyleParser;
		
	QMap<QString,QString> detectedEnvironmentsForHighlighting;
	
	LatexDocument* diffLoadDocHidden(QString f);
	void showDiff3(const QString file1,const QString file2);
	
	void restoreBookmarks(LatexEditorView *edView);
	
    bool completerPreview;
	
	LatexEditorView *editorViewForLabel(LatexDocument *doc, const QString &label);

public:
	Q_PROPERTY(QString clipboard READ clipboardText WRITE setClipboardText);
	Q_INVOKABLE QString clipboardText(const QClipboard::Mode& mode = QClipboard::Clipboard) const;
	Q_INVOKABLE void setClipboardText(const QString& text, const QClipboard::Mode& mode = QClipboard::Clipboard);
	Q_INVOKABLE int getVersion() const;
	
	static void recoverFromCrash();
	
public slots:
	void threadCrashed();
	void iamalive();
    void checkLatexInstall();
	
	void slowOperationStarted();
	void slowOperationEnded();
	
	
signals:
	void infoNewFile();
	void infoNewFromTemplate();
	void infoLoadFile(const QString& filename);
	void infoFileSaved(const QString& filename);
	void infoFileClosed();
	void infoAfterTypeset();
    void ImgPreview(const QString& fn);
};

Q_DECLARE_METATYPE(Texmaker*)

#endif

