/*
	This is part of TeXworks, an environment for working with TeX documents
	Copyright (C) 2007-2010  Jonathan Kew

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	For links to further information, or to contact the author,
	see <http://texworks.org/>.
*/

#ifndef PDFDocument_H
#define PDFDocument_H

#ifndef NO_POPPLER_PREVIEW


#include "mostQtHeaders.h"

#include <QProgressDialog>

//#include "FindDialog.h"
#include "poppler-qt4.h"
#include "synctex_parser.h"

#include "ui_PDFDocument.h"
#include "pdfrendermanager.h"


const int kPDFWindowStateVersion = 1;

class QAction;
class QMenu;
class QToolBar;
class QScrollArea;
class QShortcut;
class QFileSystemWatcher;
class ConfigManagerInterface;
class PDFDocument;
class PDFWidget;

class PDFMagnifier : public QLabel
{
	Q_OBJECT

public:
	PDFMagnifier(QWidget *parent, qreal inDpi);
	void setPage(int p, qreal scale, const QRect& visibleRect);
	void reshape();
	
public slots:
	void setImage(QPixmap img,int pageNr);
	
protected:
	virtual void paintEvent(QPaintEvent *event);

private:
	int oldshape;
	int page;
	qreal	scaleFactor;
	qreal	parentDpi;
	QPixmap	image;
	
	QPoint mouseTranslate;
	
	QPoint	imageLoc, mouseOffset;
	QSize	imageSize;
	qreal	imageDpi;
	int imagePage;
};

#ifdef PHONON
#include <Phonon/VideoPlayer>

class PDFMovie: public Phonon::VideoPlayer
{
	Q_OBJECT
public:
	PDFMovie(PDFWidget* parent, Poppler::MovieAnnotation* annot, int page);
	void place();
protected:
	void contextMenuEvent(QContextMenuEvent *);
	void mouseReleaseEvent(QMouseEvent *e);
public slots:
	void realPlay();
	void setVolumeDialog();
	void seekDialog();
private:
	QMenu* popup;
	QRectF boundary;
	int page;
};
#endif

typedef enum {
	kFixedMag,
	kFitWidth,
	kFitWindow
} autoScaleOption;

class PDFScrollArea;
class PDFWidget : public QLabel
{
	Q_OBJECT

public:
    PDFWidget(bool embedded=false);
	virtual ~PDFWidget();
	
	void setDocument(Poppler::Document *doc);
    void setPDFDocument(PDFDocument *docu);

	void saveState(); // used when toggling full screen mode
	void restoreState();
	void setResolution(int res);
	void resetMagnifier();
	Q_INVOKABLE int normalizedPageIndex(int pageIndex);
	Q_INVOKABLE void goToPageDirect(int pageIndex, bool sync);
	Q_INVOKABLE void setHighlightPath(const int pageIndex, const QPainterPath& path);
	Q_INVOKABLE int getHighlightPage() const;
	Q_INVOKABLE void goToDestination(const QString& destName);
	Q_INVOKABLE void goToPageRelativePosition(int page, float xinpage, float yinpage);
	Q_INVOKABLE int getPageIndex();
	Q_INVOKABLE void reloadPage(bool sync = true);
	void updateStatusBar();
	void setGridSize(int gx, int gy, bool setAsDefault=false);
	Q_INVOKABLE int visiblePages() const;
	Q_INVOKABLE int pseudoNumPages() const;
	Q_INVOKABLE int realNumPages() const;
	Q_INVOKABLE int pageStep();
	Q_INVOKABLE int gridCols() const;
	Q_INVOKABLE int gridRowHeight() const;
	Q_INVOKABLE PDFDocument * getPDFDocument();
	Q_INVOKABLE int getPageOffset() const;
	double totalScaleFactor() const;

	int currentPageHistoryIndex() const;
	const QList<int> currentPageHistory() const;
	
	QPoint gridPagePosition(int pageIndex) const;
	QRect gridPageRect(int pageIndex) const;
	int gridPageIndex(const QPoint& position) const;
	void mapToScaledPosition(const QPoint& position, int & page, QPointF& scaledPos) const;
	QPoint mapFromScaledPosition(int page, const QPointF& scaledPos) const;
	int pageFromPos(const QPoint& pos) const;
	QRect pageRect(int page) const;
	QSizeF maxPageSizeF() const;
	QSizeF gridSizeF(bool ignoreVerticalGrid=false) const;

	Q_INVOKABLE void zoom(qreal scale);

	virtual void wheelEvent(QWheelEvent *event);

protected slots: //not private, so scripts have access
	void goFirst();
	void goPrev();
	void goNext();
	void goLast();
	void goForward();
	void goBack();
	void doPageDialog();
	
	void fitWidth(bool checked = true);
	void zoomIn();
	void zoomOut();
	void jumpToSource();
	
	void upOrPrev();
	void leftOrPrev();
	void pageUpOrPrev();

	void downOrNext();
	void rightOrNext();
	void pageDownOrNext();

	void clearHighlight();
	
public slots:
	void setSinglePageStep(bool step);
	void windowResized();
	void fitWindow(bool checked = true);
	void setTool(int tool);	
	void syncWindowClick(const QPoint& p, bool activate);
	void syncCurrentPage(bool activate);
	void fixedScale(qreal scale = 1.0);
	void setImage(QPixmap img,int pageNr);

signals:
	void changedPage(int, bool);
	void changedZoom(qreal);
	void changedScaleOption(autoScaleOption);
	void syncClick(int, const QPointF&, bool activate); //page position in page coordinates

protected:
	virtual void paintEvent(QPaintEvent *event);

	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
	virtual void mouseDoubleClickEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);

	virtual void keyPressEvent(QKeyEvent *event);
	virtual void keyReleaseEvent(QKeyEvent *event);

	virtual void focusInEvent(QFocusEvent *event);

	virtual void contextMenuEvent(QContextMenuEvent *event);

private:
	void init();
	void adjustSize();
	void updateCursor();
	void updateCursor(const QPoint& pos);
	void useMagnifier(const QMouseEvent *inEvent);
	void goToDestination(const Poppler::LinkDestination& dest);
	void doLink(const Poppler::Link *link);
	void doZoom(const QPoint& clickPos, int dir, qreal newScaleFactor=1.0);

	PDFScrollArea* getScrollArea();
	
	Poppler::Document	*document;
	//QList<int> pages;
	Poppler::Link		*clickedLink;
	Poppler::Annotation	*clickedAnnotation;

	int realPageIndex, oldRealPageIndex;
	QList<int> pages;
	qreal	scaleFactor;
	qreal	dpi;
	autoScaleOption scaleOption;

	float totalScrolling;
	
	int docPages;
	qreal			saveScaleFactor;
	autoScaleOption	saveScaleOption;

	QAction	*ctxZoomInAction;
	QAction	*ctxZoomOutAction;
	QShortcut *shortcutUp;
	QShortcut *shortcutLeft;
	QShortcut *shortcutDown;
	QShortcut *shortcutRight;
	QShortcut *shortcutPageUp1;
	QShortcut *shortcutPageUp2;
	QShortcut *shortcutPageUp3;
	QShortcut *shortcutPageUp4;
	QShortcut *shortcutPageDown1;
	QShortcut *shortcutPageDown2;
	QShortcut *shortcutPageDown3;
	QShortcut *shortcutPageDown4;

	QPixmap image;
	QRect	imageRect;
	qreal	imageDpi;
	int	imagePage;

	PDFMagnifier	*magnifier;
#ifdef PHONON
	PDFMovie	*movie;
#endif
	int		currentTool;	// the current tool selected in the toolbar
	int		usingTool;	// the tool actually being used in an ongoing mouse drag
	bool		singlePageStep;

	int gridx, gridy;

	bool forceUpdate;

	QPainterPath	highlightPath;
	int highlightPage;
	QTimer highlightRemover;
	
	static QCursor	*magnifierCursor;
	static QCursor	*zoomInCursor;
	static QCursor	*zoomOutCursor;

	mutable QSizeF maxPageSize; //cache pageSize

	QList<int> pageHistory;
	int pageHistoryIndex;

    PDFDocument *pdfdocument;
};

class PDFSearchResult {
public:
	PDFSearchResult(const PDFDocument* pdfdoc = NULL, int page = -1, QRectF rect = QRectF())
		: doc(pdfdoc), pageIdx(page), selRect(rect)
	{ }

	const PDFDocument* doc;
	int pageIdx;
	QRectF selRect;
};



struct PDFDocumentConfig;
class PDFDock;
class PDFSearchDock;
class PDFScrollArea;
class PDFDocument : public QMainWindow, private Ui::PDFDocument
{
	Q_OBJECT
	Q_PROPERTY(QString fileName READ fileName)

public:
    PDFDocument(PDFDocumentConfig* const pdfConfig,bool embedded=false);
	virtual ~PDFDocument();

	static PDFDocument *findDocument(const QString &fileName);
	static QList<PDFDocument*> documentList()
	{
		return docList;
	}

	Q_INVOKABLE QString fileName() const { return curFile; }
	Q_INVOKABLE QFileInfo getMasterFile() const { return masterFile; }

	void saveGeometryToConfig();

	Q_INVOKABLE void zoomToRight(QWidget *otherWindow);
	void showScale(qreal scale);
	Q_INVOKABLE void showPage(int page);
	Q_INVOKABLE void setResolution(int res);
	void resetMagnifier();
	Q_INVOKABLE void goToDestination(const QString& destName);
	Q_INVOKABLE void goToPage(const int page);
	bool hasSyncData()
	{
		return scanner != NULL;
	}

	Poppler::Document *popplerDoc()
	{
		return document;
	}
	
	Q_INVOKABLE PDFWidget *widget(){ return pdfWidget; }
	const PDFWidget *widget() const { return pdfWidget; }

	bool followCursor() const;
	PDFRenderManager *renderManager;

	static bool isCompiling;
	bool embeddedMode;
	bool autoClose;
	
protected:
	virtual void changeEvent(QEvent *event);
	virtual void closeEvent(QCloseEvent *event);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dropEvent(QDropEvent *event);

public slots:
	void reload(bool fillCache=true);
	void fillRenderCache(int pg=-1);
	void sideBySide();
	void doFindDialog();
	void doFindAgain();
	void goToSource();
	void toggleFullScreen(const bool fullscreen);
	int syncFromSource(const QString& sourceFile, int lineNo, bool activatePreview); //lineNo 0 based
	void syncFromView(const QString& pdfFile, const QFileInfo& masterFile, int page);
	void loadFile(const QString &fileName, const QFileInfo& masterFile, bool alert = true);
	void printPDF();
private slots:
	void fileOpen();
	
	void enablePageActions(int, bool);
	void enableZoomActions(qreal);
	void adjustScaleActions(autoScaleOption);
	void syncClick(int page, const QPointF& pos, bool activate);
	void reloadWhenIdle();
	void idleReload();

	void runExternalViewer();
    void runInternalViewer();
	void toggleEmbedded();
	void runQuickBuild();

	void setGrid();

	void closeSomething();
	void tileWindows();
	void stackWindows();
	void arrangeWindows(bool tile);
	void updateToolBarForOrientation(Qt::Orientation orientation);

	void jumpToPage();

	void search(bool backward, bool incremental);
	void setZoom();
	void zoomSliderChange(int pos = 0);
signals:
	void documentClosed();
	void reloaded();
	void syncSource(const QString& sourceFile, int line, bool activate, const QString& guessedWord); //view -> source
	void syncView(const QString& pdfFile, const QFileInfo& masterFile, int page); //view -> other view
	void fileDropped(const QUrl& url);

	void runCommand(const QString& command, const QFileInfo& masterFile, const QFileInfo& currentFile, int linenr);

	void triggeredAbout();
	void triggeredManual();
	void triggeredQuit();
	void triggeredPlaceOnLeft();
	void triggeredConfigure();
	
	void triggeredClone();

private:
    void init(bool embedded=false);
	void setCurrentFile(const QString &fileName);
	void loadSyncData();

	qreal zoomSliderPosToScale(int pos);
	int scaleToZoomSliderPos(qreal scale);

	QString curFile;
	qint64 curFileSize;
	QDateTime curFileLastModified;
	QFileInfo masterFile, lastSyncSourceFile;
	int lastSyncLineNumber;

	Poppler::Document	*document;
	
	PDFWidget	*pdfWidget;
	PDFScrollArea	*scrollArea;
	QButtonGroup	*toolButtonGroup;
	QToolButton *comboZoom;
	QLineEdit *leCurrentPage;
	QLabel *pageCountSeparator;
	QLabel *pageCountLabel;
	QIntValidator *leCurrentPageValidator;

	QLabel *pageLabel;
	QLabel *scaleLabel;
	QSlider *zoomSlider;
	QList<QAction*> recentFileActions;
	QShortcut* exitFullscreen;

	QFileSystemWatcher *watcher;
	QTimer *reloadTimer;
	
	synctex_scanner_t scanner;
	
	static QList<PDFDocument*> docList;
	
	PDFDock *dwClock, *dwOutline, *dwFonts, *dwInfo, *dwOverview;
	bool dwVisOutline,dwVisFonts,dwVisInfo,dwVisSearch,dwVisOverview;
	bool wasContinuous;
	PDFSearchDock *dwSearch;

	PDFSearchResult lastSearchResult;
	// stores the page idx a search was started on
	// after wrapping the search will continue only up to this page
	int firstSearchPage;

	bool wasMaximized;
	bool syncFromSourceBlock;  //temporary disable sync from source
	bool syncToSourceBlock;    //temporary disable sync to source (only for continuous scrolling)
};

#endif

#endif
