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

#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QListWidget>
#include <QToolBox>
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
#include <QTextTable>
#include <QVBoxLayout>
#include <QTableView>

#include "latexeditorview.h"
#include "latexcompleter.h"
#include "symbollistwidget.h"
#include "metapostlistwidget.h"
#include "pstrickslistwidget.h"
#include "logeditor.h"
#include "textanalysis.h"
#include "spellerdialog.h"


#include "qformatfactory.h"
#include "qlanguagefactory.h"
#include "qlinemarksinfocenter.h"


typedef  QMap<LatexEditorView*, QString> FilesMap;
typedef  QString Userlist[10];
typedef  QString UserCd[5];
typedef int SymbolList[412];

enum LogType {LT_ERROR, LT_WARNING, LT_BADBOX};
struct LatexLogEntry {
    QString file;
    LogType type;
    QString oldline;
    int oldLineNumber;
    int logline;
    QString message;
    LatexLogEntry (QString aFile, LogType aType, QString aOldline, int aLogline, QString aMessage)
    : file(aFile), type(aType), oldline(aOldline), logline (aLogline), message(aMessage){
        bool ok;
        oldLineNumber=oldline.toInt(&ok,10)-1;
        if (!ok) oldLineNumber=-1;
    };
};
class LatexLogModel: public QAbstractTableModel{
private:
    QList<LatexLogEntry> log;
public:
    LatexLogModel (QObject * parent = 0): QAbstractTableModel(parent){};
    int columnCount(const QModelIndex & parent) const
    {
        return parent.isValid()?0:4;
    }
    int rowCount(const QModelIndex &parent) const {
        return parent.isValid()?0:log.count();
    }
    QVariant data(const QModelIndex &index, int role) const
    {
        if (!index.isValid()) return QVariant();
        if (index.row() >= log.count() || index.row() < 0) return QVariant();
        if (role == Qt::ToolTipRole) return tr("Click to jump to the line");
        if (role == Qt::ForegroundRole) return log.at(index.row()).type==LT_ERROR?QBrush(QColor(Qt::red)):QBrush(QColor(Qt::blue));
        if (role != Qt::DisplayRole) return QVariant();
        switch (index.column()) {
            case 0: return log.at(index.row()).file;
            case 1: switch (log.at(index.row()).type) {
                case LT_ERROR: return tr("error");
                case LT_WARNING: return tr("warning");
                case LT_BADBOX: return tr("bad box");
                default:  return QVariant(); //return Texmaker::tr("unknown");
            }
            case 2: return tr("line")+ QString(" %1").arg(log.at(index.row()).oldline);
            case 3: return log.at(index.row()).message;
            default: return QVariant();
        }
    }    
    QVariant headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (role != Qt::DisplayRole) return QVariant();
        if (orientation != Qt::Horizontal) return QVariant();
        switch (section) {
            case 0: return tr("File");
            case 1: return tr("Type");
            case 2: return tr("Line");
            case 3: return tr("Message");
            default: return QVariant();
        }
    }   

    void reset(){QAbstractTableModel::reset();}
    
    int count(){return log.count();}    
    void clear(){log.clear();}
    const LatexLogEntry& at(int i){return log.at(i);}
    void append(QString aFile, LogType aType, QString aOldline, int aLogline, QString aMessage){
        log.append(LatexLogEntry(aFile, aType, aOldline, aLogline, aMessage));
    }
};


class Texmaker : public QMainWindow
{
    Q_OBJECT

public:
    Texmaker(QWidget *parent = 0, Qt::WFlags flags = 0);

QString getName();
QFont EditorFont;
QByteArray windowstate;
public slots:
LatexEditorView* load( const QString &f , bool asProject = false);
void gotoLine( int line );//0 based
void executeCommandLine( const QStringList& args, bool realCmdLine);
void onOtherInstanceMessage(const QString &);  // For messages for the single instance

private:
QMenu* newManagedMenu(const QString &id,const QString &text);
QMenu* newManagedMenu(QMenu* menu, const QString &id,const QString &text);
QAction* newManagedAction(QMenu* menu, const QString &id,const QString &text, const char* slotName=0, const QKeySequence &shortCut=0, const QString & iconFile="");
QAction* newManagedAction(QMenu* menu, const QString &id, QAction* act);
QAction* getManagedAction(QString id);
void loadManagedMenu(QMenu* parent,const QDomElement &f);
void loadManagedMenus(const QString &f);
void managedMenuToTreeWidget(QTreeWidgetItem* parent, QMenu* menu);    
void treeWidgetToManagedMenuTo(QTreeWidgetItem* item);
void setupMenus();
void setupToolBars();
void createStatusBar();
bool FileAlreadyOpen(QString f);
void closeEvent(QCloseEvent *e);

FilesMap filenames;

QString configFileName,configFileNameBase;
QFormatFactory *m_formats;
QLanguageFactory* m_languages;
LatexCompleter* completer;

//gui
QDockWidget *OutputView, *StructureView;
QTabWidget *EditorView;
LogEditor* OutputTextEdit;
QToolBox *StructureToolbox;
MetapostListWidget *MpListWidget;
PstricksListWidget *PsListWidget;
SymbolListWidget *RelationListWidget, *ArrowListWidget, *MiscellaneousListWidget, *DelimitersListWidget, *GreekListWidget, *MostUsedListWidget;
QTreeWidget *StructureTreeWidget;
QVBoxLayout *OutputLayout;
QTableView *OutputTable;


//menu-toolbar
QList<QMenu*> managedMenus;
QHash<QString,QKeySequence> managedMenuShortcuts;
QList<QPair<QString,QString> > managedMenuNewShortcuts;
//
QToolBar *fileToolBar, *editToolBar, *runToolBar, *formatToolBar, *mathToolBar;
QAction *ToggleAct, *ToggleRememberAct;

QLabel *stat1, *stat2, *stat3;
QString MasterName,persistentMasterFile;
bool logpresent;
QStringList recentFilesList, recentProjectList;
QStringList sessionFilesToRestore; 
QString sessionMaster;
QString sessionCurrent;

//settings
int split1_right, split1_left, split2_top, split2_bottom, quickmode;
bool singlemode, wordwrap, parenmatch, showoutputview, showstructview, ams_packages, makeidx_package, completion, autoindent;
int showlinemultiples;
bool folding, showlinestate, showcursorstate, realtimespellchecking;
QString document_class, typeface_size, paper_size, document_encoding, author;
QString latex_command, viewdvi_command, dvips_command, dvipdf_command, metapost_command;
QString viewps_command, ps2pdf_command, makeindex_command, bibtex_command, pdflatex_command, viewpdf_command, userquick_command, ghostscript_command;
QString spell_dic, spell_ignored_words;
QString lastDocument;
QTextCodec* newfile_encoding;
bool autodetectLoadedFile;
QString struct_level1, struct_level2, struct_level3, struct_level4, struct_level5;
QStringList userClassList, userPaperList, userEncodingList, userOptionsList;
QStringList structlist, labelitem, structitem;
Userlist UserMenuName, UserMenuTag;
UserCd UserToolName, UserToolCommand;
QVector<QString> UserKeyReplace, UserKeyReplaceAfterWord, UserKeyReplaceBeforeWord;

int spellcheckErrorFormat;
SpellerUtility *mainSpeller;
//dialogs
TextAnalysisDialog *textAnalysisDlg;
SpellerDialog *spellDlg;


//tools
QProcess *proc;
bool FINPROCESS, ERRPROCESS;
//latex errors
bool latexErrorsFound;
bool latexWarningsFound;
LatexLogModel * logModel;

//X11
//#if defined( Q_WS_X11 )
QString x11style;
QString x11fontfamily;
int x11fontsize;
//#endif
SymbolList symbolScore;
usercodelist symbolMostused;

LatexEditorView *currentEditorView() const;
void configureNewEditorView(LatexEditorView *edit);
void updateEditorSetting(LatexEditorView *edit);
private slots:

void fileNew();
void fileOpen();
void fileSave();
void fileSaveAll();
void fileSaveAs();
void fileClose();
void fileCloseAll();
void fileExit();
void fileOpenRecent();
void fileOpenRecentProject();
void AddRecentFile(const QString &f, bool asMaster=false);
void UpdateRecentFile();
void filePrint();

void editUndo();
void editRedo();
void editCut();
void editCopy();
void editPaste();
void editSelectAll();
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

void ReadSettings();
void SaveSettings();

void NewDocumentStatus(bool m);
void UpdateCaption();

void UpdateStructure();
void ShowStructure();
void ClickedOnStructure(QTreeWidgetItem *item,int);

void InsertEnvironmentCompletion();
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

void SectionCommand(const QString& text);
void OtherCommand(const QString& text);
void InsertRef();
void InsertPageRef();
void SizeCommand(const QString& text);
void LeftDelimiter(const QString& text);
void RightDelimiter(const QString& text);

void QuickTabular();
void QuickArray();
void QuickTabbing();
void QuickLetter();
void QuickDocument();

void RunCommand(QString comd,bool waitendprocess,bool showStdout);
void readFromStderr();
void readFromStdoutput();
void SlotEndProcess(int err);
void QuickBuild();
void Latex();
void ViewDvi();
void DviToPS();
void ViewPS();
void PDFLatex();
void ViewPDF();
void CleanAll();
void MakeBib();
void MakeIndex();
void PStoPDF();
void DVItoPDF();
void MetaPost();
void UserTool();
void EditUserTool();
void EditUserKeyReplacements();

void WebPublish();
void AnalyseText();
void AnalyseTextFormDestroyed();


void ViewLog();
void ClickedOnLogLine(const QModelIndex &);
void OutputViewVisibilityChanged(bool visible);
void LatexError();
void DisplayLatexError();
void GoToLogEntry(int logEntryNumber);
void GoToLogEntryAt(int newLineNumber);
void NextError();
void PreviousError();
void NextWarning();
void PreviousWarning();
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
//void ToggleMode();

void SetInterfaceFont();
void SetInterfaceType();

void gotoBookmark();
void toggleBookmark();

void SetMostUsedSymbols();

void updateCompleter();

protected:
void dragEnterEvent(QDragEnterEvent *event);
void dropEvent(QDropEvent *event);
};

#endif
