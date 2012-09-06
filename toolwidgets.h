/*
	This file contains the dockwidgets at the left/bottom side of txs
*/
#ifndef TOOLWIDGETS_H
#define TOOLWIDGETS_H

#include "mostQtHeaders.h"

#include "logeditor.h"
#include "latexlog.h"
#include "searchresultmodel.h"
#include "qdocumentsearch.h"

#include <QAbstractTextDocumentLayout>

class PreviewWidget : public QScrollArea
{
	Q_OBJECT
public:
    PreviewWidget(QWidget * parent = 0);

public slots:	
	void previewLatex(const QPixmap& previewImage);
	void fitImage();
	void scaleImage(double factor);
	void zoomOut();
	void zoomIn();
	void resetZoom();
	void contextMenu(QPoint point);
	void centerImage();

protected:
	void wheelEvent(QWheelEvent *event);

private:
	QLabel *preViewer;
	double pvscaleFactor;
	bool mCenter;
};

class OutputViewWidget: public QDockWidget{
	Q_OBJECT
public:
	OutputViewWidget(QWidget * parent = 0);
	
	
	LatexLogModel* getLogModel();
	void loadLogFile(const QString &logname, const QString & compiledFileName);
	bool logPresent();
	bool isPreviewPanelVisible();
	void setMessage(const QString &message); //set the message text (don't change page and no auto-show)
	void setSearchExpression(QString exp,bool isCase,bool isWord,bool isRegExp);
	int getNextSearchResultColumn(QString text,int col);
	bool childHasFocus();
	int getShownPage(){
		return OutputLayout->currentIndex();
	}

	virtual void changeEvent(QEvent *event);
	QString searchExpression();
public slots:
	void copy();
	void resetMessages(bool noTabChange=false); //remove all messages and jumps to the message page (stays hidden if not visible)
	void resetMessagesAndLog(bool noTabChange=false);
	void resetLog(bool noTabChange=false);
	void selectLogEntry(int logEntryNumber, bool makeVisible=true);
	void showLogOrErrorList(bool noTabChange=false); //this will show the log unless the error list is open
	void showErrorListOrLog(); //this will show the error list unless log is open
	void showPreview();
	void showSearchResults();
	void gotoLogEntry(int logEntryNumber);
	void setTabbedLogView(bool tabbed);
	void previewLatex(const QPixmap& pixmap);
	void addSearch(QList<QDocumentLineHandle *> search, QDocument* doc);
	void clearSearch();
	void insertMessageLine(const QString &message); //inserts the message text (don't change page and no auto-show)
signals:
	void locationActivated(int line, QString fileName); //0-based line, absolute file name
	void logEntryActivated(int logEntryNumber);
	void tabChanged(int tab);
	void jumpToSearch(QDocument* doc,int lineNumber);
private:
	PreviewWidget *previewWidget;
	QTableView *OutputTable, *OutputTable2;
	QTreeView *OutputTree;
	LogEditor *OutputTextEdit,*OutputLogTextEdit;	
	QTabBar *logViewerTabBar; //header to select outp (if tabbedLogView, then it is OutputView's TitleBarWidget)
	QStackedWidget*	OutputLayout;
	//Latex errors
	LatexLogModel * logModel; 
	SearchResultModel *searchResultModel;
	bool logpresent, tabbedLogView;
	
	void retranslateUi();
private slots:
	void clickedOnLogModelIndex(const QModelIndex& index);
	void clickedSearchResult(const QModelIndex& index);
	void gotoLogLine(int logLine);

	void copyMessage();
	void copyAllMessages();
	void copyAllMessagesWithLineNumbers();
	void showMessageInLog();
	void copySearchResult();
};

class SearchTreeDelegate: public QItemDelegate {
    Q_OBJECT
public:
        SearchTreeDelegate(QObject * parent = 0 );
protected:
        void paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
        //void drawDisplay( QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect, const QString& text ) const;
        QSize sizeHint(const QStyleOptionViewItem &option,const QModelIndex &index) const;
};

class CustomWidgetList: public QDockWidget{
	Q_OBJECT
public:
	CustomWidgetList(QWidget *p=0);
	void addWidget(QWidget* widget, const QString& id, const QString& text, const QString& iconName);
	void setWidgetText(const QString& id, const QString& text);
	void setWidgetText(QWidget* widget, const QString& text);
	void setWidgetIcon(const QString& id, const QString& icon);
	void setWidgetIcon(QWidget* widget, const QString& icon);
	int widgetCount() const;
	void setHiddenWidgets(const QString& hidden); 
	QString hiddenWidgets() const; 
	QWidget* widget(int i) const;
	QWidget* widget(const QString& id) const;
	void setCurrentWidget(QWidget* widget);
	QWidget* currentWidget() const;
	bool isNewLayoutStyleEnabled() const;
signals:
	void widgetContextMenuRequested(QWidget* widget, const QPoint& globalPosition);
public slots:
	void showWidgets(bool newLayoutStyle);
private slots:
	void showPageFromAction();
	void currentWidgetChanged(int i);
	void toggleWidgetFromAction(bool on);
	void customContextMenuRequested(const QPoint& localPosition);	
private:
	void showWidget(const QString& id);
	void hideWidget(const QString& id);
	//void addWidgetOld(QWidget* widget, const QString& id, const QString& text, const QString& iconName, const bool visible);
//	void addWidgetNew(QWidget* widget, const QString& id, const QString& text, const QString& iconName, const bool visible);
	QString widgetId(QWidget* widget) const;
	
	
	QStringList hiddenWidgetsIds;
	QList<QWidget*> widgets;
	bool newStyle;
	
	//old layout
	QToolBox *toolbox;
		
	//new layout
	QFrame* frame;
	QStackedWidget* stack;
	QToolBar* toolbar;
		
};

#endif 
