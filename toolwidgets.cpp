#include "toolwidgets.h"
#include "math.h"
#include "smallUsefulFunctions.h"

Q_DECLARE_METATYPE(QAction*)

void adjustScrollBar(QScrollBar *scrollBar, double factor)
{
	scrollBar->setValue(int(factor * scrollBar->value()
							+ ((factor - 1) * scrollBar->pageStep()/2)));
}


PreviewWidget::PreviewWidget(QWidget * parent): QScrollArea(parent){
	setBackgroundRole(QPalette::Base);

	mCenter=false;

	preViewer = new QLabel(this);
	preViewer->setBackgroundRole(QPalette::Base);
	preViewer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	preViewer->setScaledContents(true);
	preViewer->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(preViewer,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(contextMenu(QPoint)));
	connect(this,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(contextMenu(QPoint)));
	setContextMenuPolicy(Qt::CustomContextMenu);

	setWidget(preViewer);
}

void PreviewWidget::scaleImage(double factor)
{
	REQUIRE(preViewer->pixmap());
	pvscaleFactor *= factor;
	preViewer->resize(pvscaleFactor * preViewer->pixmap()->size());

	adjustScrollBar(horizontalScrollBar(), factor);
	adjustScrollBar(verticalScrollBar(), factor);

}

void PreviewWidget::previewLatex(const QPixmap& previewImage){
	preViewer->setPixmap(previewImage);
	preViewer->adjustSize();
	pvscaleFactor=1.0;
}

void PreviewWidget::fitImage(){
	REQUIRE(preViewer->pixmap());
	// needs to be improved
	QSize m_size=size()-QSize(2,2);
	QSize m_labelSize=preViewer->size();
	qreal ratio=1.0*m_labelSize.height()/m_labelSize.width();
	qreal ratioPreviewer=1.0*m_size.height()/m_size.width();
	int h,w;
	if(ratioPreviewer>ratio){
		h=qRound(ratio*m_size.width());
		w=m_size.width();
		pvscaleFactor=1.0*w/preViewer->pixmap()->size().width();
	} else {
		h=m_size.height();
		w=qRound(m_size.height()/ratio);
		pvscaleFactor=1.0*h/preViewer->pixmap()->size().height();;
	}
	preViewer->resize(w,h);
	//setWidgetResizable(true);
}
void PreviewWidget::centerImage(){
	mCenter=!mCenter;
	if(mCenter) setAlignment(Qt::AlignCenter);
	else setAlignment(Qt::AlignLeft|Qt::AlignTop);
	scaleImage(1.0);
}

void PreviewWidget::zoomOut(){
	scaleImage(1/1.4);
}

void PreviewWidget::zoomIn(){
	scaleImage(1.4);
}

void PreviewWidget::resetZoom(){
	pvscaleFactor=1.0;
	scaleImage(1.0);
}

void PreviewWidget::wheelEvent(QWheelEvent *event){
	if (!preViewer->pixmap()) return;
	if(event->modifiers()==Qt::ControlModifier){
		int numDegrees = event->delta() / 8;
		int numSteps = numDegrees / 15;
		scaleImage(pow(1.4,numSteps));
		event->accept();
	} else QScrollArea::wheelEvent(event);
}

void PreviewWidget::contextMenu(QPoint point) {
	if (!preViewer->pixmap()) return;
	QMenu menu;
	menu.addAction(tr("zoom in "),this, SLOT(zoomIn()));
	menu.addAction(tr("zoom out"),this, SLOT(zoomOut()));
	menu.addAction(tr("reset zoom"),this, SLOT(resetZoom()));
	menu.addAction(tr("fit"),this, SLOT(fitImage()));
	if(mCenter) menu.addAction(tr("left-align image"),this, SLOT(centerImage()));
	else menu.addAction(tr("center image"),this, SLOT(centerImage()));
	QWidget* menuParent = qobject_cast<QWidget*>(sender());
	Q_ASSERT(menuParent);
	if (!menuParent) menuParent = preViewer;
	menu.exec(menuParent->mapToGlobal(point));
}




const int LAYOUT_PAGE_MESSAGES=0;
const int LAYOUT_PAGE_LOG=1;
const int LAYOUT_PAGE_ERRORS=2;
const int LAYOUT_PAGE_PREVIEW=3;
const int LAYOUT_PAGE_SEARCH=4;
	
OutputViewWidget::OutputViewWidget(QWidget * parent): QDockWidget(parent), logModel(0), logpresent(false), tabbedLogView(false){
	logModel = new LatexLogModel(this);//needs loaded line marks
	searchResultModel = new SearchResultModel(this);

	OutputTable= new QTableView(this);
	OutputTable2= new QTableView(this); // second table view for tab log view

	// Search Results tree
        SearchTreeDelegate *searchDelegate=new SearchTreeDelegate(this);
	OutputTree= new QTreeView(this);
        OutputTree->setUniformRowHeights(true);
	OutputTree->setModel(searchResultModel);
        OutputTree->setItemDelegate(searchDelegate);
	connect(OutputTree,SIGNAL(clicked(QModelIndex)),this,SLOT(clickedSearchResult(QModelIndex)));

	QFontMetrics fm(QApplication::font());
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
		connect(table, SIGNAL(clicked(const QModelIndex &)), this, SLOT(clickedOnLogModelIndex(const QModelIndex &)));
	
		table->horizontalHeader()->setStretchLastSection(true);
		table->setMinimumHeight(5*(fm.lineSpacing()+4));

		QAction * act = new QAction("&Copy",table);
		connect(act, SIGNAL(triggered()), SLOT(copyMessage()));
		table->addAction(act);
		act = new QAction("&Copy all",table);
		connect(act, SIGNAL(triggered()), SLOT(copyAllMessages()));
		table->addAction(act);
		act = new QAction("&Copy all with line numbers",table);
		connect(act, SIGNAL(triggered()), SLOT(copyAllMessagesWithLineNumbers()));
		table->addAction(act);
		act = new QAction("&Show in log",table);
		connect(act, SIGNAL(triggered()), SLOT(showMessageInLog()));;
		table->addAction(act);

		table->setContextMenuPolicy(Qt::ActionsContextMenu);
	}

	OutputTextEdit = new LogEditor(this);
	OutputTextEdit->setFocusPolicy(Qt::ClickFocus);
	OutputTextEdit->setMinimumHeight(3*(fm.lineSpacing()+4));
	OutputTextEdit->setReadOnly(true);
	connect(OutputTextEdit, SIGNAL(clickOnLogLine(int)),this,SLOT(gotoLogLine(int)));

	OutputLogTextEdit = new LogEditor(this);
	OutputLogTextEdit->setFocusPolicy(Qt::ClickFocus);
	OutputLogTextEdit->setMinimumHeight(3*(fm.lineSpacing()+4));
	OutputLogTextEdit->setReadOnly(true);
	connect(OutputLogTextEdit, SIGNAL(clickOnLogLine(int)),this,SLOT(gotoLogLine(int)));

	OutputLayout= new QStackedWidget(this);

	QVBoxLayout* OutputVLayout= new QVBoxLayout(); //contains the widgets for the normal mode (OutputTable + OutputLogTextEdit)
	OutputVLayout->setSpacing(0);
	OutputVLayout->setMargin(0);

	// add widget to log view
	OutputLayout->addWidget(OutputTextEdit);

	OutputVLayout->addWidget(OutputTable);
	OutputVLayout->addWidget(OutputLogTextEdit);
	QWidget* tempWidget=new QWidget (this);
	tempWidget->setLayout(OutputVLayout);
	OutputLayout->addWidget(tempWidget);

	OutputLayout->addWidget(OutputTable2);

	// previewer
	previewWidget = new PreviewWidget(this);
	OutputLayout->addWidget(previewWidget);

	// global search results
	OutputLayout->addWidget(OutputTree);

	// order for tabbar
	logViewerTabBar=new QTabBar(this);
	logViewerTabBar->addTab("m");
	logViewerTabBar->addTab("l");
	logViewerTabBar->addTab("e");
	logViewerTabBar->addTab("p");
	logViewerTabBar->addTab("s");
	retranslateUi();
	logViewerTabBar->hide(); //internal default is non tabbed mode

	this->setWidget(OutputLayout);

	connect(logViewerTabBar, SIGNAL(currentChanged(int)),
	        OutputLayout, SLOT(setCurrentIndex(int)));

	connect(logViewerTabBar, SIGNAL(currentChanged(int)),
	        this, SIGNAL(tabChanged(int)));

}
void OutputViewWidget::setTabbedLogView(bool tabbed){
	tabbedLogView=tabbed;
	if (tabbed) {
		this->setTitleBarWidget(logViewerTabBar);
		OutputTable->hide();
	} else {
		this->setTitleBarWidget(0);
		OutputTable->show();
	}
}
void OutputViewWidget::previewLatex(const QPixmap& pixmap){
	previewWidget->previewLatex(pixmap);
	//showPreview();	
}

void OutputViewWidget::clickedSearchResult(const QModelIndex& index){
	emit jumpToSearch(searchResultModel->getFilename(index),searchResultModel->getLineNumber(index));
}

LatexLogModel* OutputViewWidget::getLogModel(){
	return logModel;
}

//copied and modified from qbytearray.cpp
//should be an optimization for qtextstream, but doesn't really improve anything
QByteArray simplifyLineConserving(const QByteArray& ba)
{
	if (ba.size() == 0)
		return QByteArray();
	QByteArray result(ba.size(), Qt::Uninitialized);
	const char *from = ba.constData();
	const char *fromend = from + ba.size();
	int outc=0;
	char *to = result.data();
	for (;;) {
		while (from!=fromend && isspace(uchar(*from)))
			from++;
		while (from!=fromend && !isspace(uchar(*from)))
			to[outc++] = *from++;
		if (from!=fromend) {
			if (uchar(*from) == '\n' || uchar(*from) == '\r') to[outc++] = '\n';
			else to[outc++] = ' ';
		} else
			break;
	}
	if (outc > 0 && to[outc-1] == ' ')
		outc--;
	result.resize(outc);
	return result;
}

void OutputViewWidget::loadLogFile(const QString &logname, const QString & compiledFileName){
	OutputLogTextEdit->clear();
	QFile f(logname);
	if (f.open(QIODevice::ReadOnly)) {

		if (f.size() > 2*1024*1024 && 
		    !txsConfirmWarning(tr("The logfile is very large (> %1 MB) are you sure you want to load it?").arg(f.size() / 1024 / 1024))) 
			return;
		
		QByteArray fullLog = simplifyLineConserving(f.readAll());
		f.close();
		
		int sure;
		QTextCodec * codec = guessEncodingBasic(fullLog, &sure);
		if (!sure) codec = QTextCodec::codecForLocale();
		
		OutputLogTextEdit->setPlainText(codec->toUnicode(fullLog));
		
        logModel->parseLogDocument(OutputLogTextEdit->document(), compiledFileName);

		logpresent=true;		
		//update table size
		OutputTable->resizeColumnsToContents();
		OutputTable->resizeRowsToContents();
		OutputTable2->resizeColumnsToContents();
		OutputTable2->resizeRowsToContents();	
		
		selectLogEntry(0,false);
	}
}
bool OutputViewWidget::logPresent(){
	return logpresent;
}
bool OutputViewWidget::isPreviewPanelVisible(){
	if (!isVisible()) return false;
	return !tabbedLogView || logViewerTabBar->currentIndex()==LAYOUT_PAGE_PREVIEW;
}
void OutputViewWidget::setMessage(const QString &message){
	logViewerTabBar->setCurrentIndex(0);
	OutputTextEdit->setText(message);
}
void OutputViewWidget::insertMessageLine(const QString &message){
	OutputTextEdit->insertLine(message);
}
void OutputViewWidget::copy(){
	if (tabbedLogView && OutputLayout->currentIndex() == LAYOUT_PAGE_ERRORS)
		copyMessage();
	else
		OutputLogTextEdit->copy();
}

void OutputViewWidget::resetMessages(bool noTabChange){
	OutputTextEdit->clear();
	if(!noTabChange) logViewerTabBar->setCurrentIndex(0);
}
void OutputViewWidget::resetMessagesAndLog(bool noTabChange){
	resetMessages(noTabChange);
	resetLog(noTabChange);
}
void OutputViewWidget::resetLog(bool /*noTabChange*/){
	logpresent=false;
}
void OutputViewWidget::selectLogEntry(int logEntryNumber, bool makeVisible){
	if (logEntryNumber<0 || logEntryNumber>=logModel->count()) return;
	if (makeVisible) showErrorListOrLog();
	OutputTable->scrollTo(logModel->index(logEntryNumber,1),QAbstractItemView::PositionAtCenter);
	OutputTable->selectRow(logEntryNumber);
	OutputTable2->scrollTo(logModel->index(logEntryNumber,1),QAbstractItemView::PositionAtCenter);
	OutputTable2->selectRow(logEntryNumber);
	OutputLogTextEdit->setCursorPosition(logModel->at(logEntryNumber).logline, 0);
}
void OutputViewWidget::showLogOrErrorList(bool noTabChange){
	if (!isVisible()) show();
	if (OutputLayout->currentIndex()!=LAYOUT_PAGE_LOG && OutputLayout->currentIndex()!=LAYOUT_PAGE_ERRORS &&!noTabChange)
		logViewerTabBar->setCurrentIndex(LAYOUT_PAGE_LOG);
}
void OutputViewWidget::showErrorListOrLog(){
	if (!isVisible()) show();
	if (tabbedLogView) {
		if (OutputLayout->currentIndex()!=LAYOUT_PAGE_LOG) 
			logViewerTabBar->setCurrentIndex(LAYOUT_PAGE_ERRORS);
	} else logViewerTabBar->setCurrentIndex(LAYOUT_PAGE_LOG);
}
void OutputViewWidget::showPreview(){
	if (!isVisible()) show();
	logViewerTabBar->setCurrentIndex(LAYOUT_PAGE_PREVIEW);
}

void OutputViewWidget::showSearchResults(){
	if (!isVisible()) show();
	logViewerTabBar->setCurrentIndex(LAYOUT_PAGE_SEARCH);
}
void OutputViewWidget::gotoLogEntry(int logEntryNumber) {
	if (logEntryNumber<0 || logEntryNumber>=logModel->count()) return;
	//select entry in table view/log
	//OutputTable->scrollTo(logModel->index(logEntryNumber,1),QAbstractItemView::PositionAtCenter);
	//OutputLogTextEdit->setCursorPosition(logModel->at(logEntryNumber).logline, 0);
	selectLogEntry(logEntryNumber);
	//notify editor	
	emit logEntryActivated(logEntryNumber);
}

void OutputViewWidget::retranslateUi(){
	logViewerTabBar->setTabText(0,tr("messages"));
	logViewerTabBar->setTabText(1,tr("log file"));
	logViewerTabBar->setTabText(2,tr("errors"));
	logViewerTabBar->setTabText(3,tr("preview"));
	logViewerTabBar->setTabText(4,tr("search results"));
}

void OutputViewWidget::clickedOnLogModelIndex(const QModelIndex& index){
	gotoLogEntry(index.row());
}
void OutputViewWidget::gotoLogLine(int logLine){
	gotoLogEntry(logModel->logLineNumberToLogEntryNumber(logLine));
}

void OutputViewWidget::copyMessage(){
	QModelIndex curMessage = tabbedLogView ? OutputTable2->currentIndex() : OutputTable->currentIndex();
	if (!curMessage.isValid()) return;
	curMessage = logModel->index(curMessage.row(), 3);
	REQUIRE(QApplication::clipboard());
	QApplication::clipboard()->setText(logModel->data(curMessage, Qt::DisplayRole).toString());
}
void OutputViewWidget::copyAllMessages(){
	QStringList result;
	for (int i=0;i<logModel->count();i++)
		result << logModel->data(logModel->index(i, 3), Qt::DisplayRole).toString();
	REQUIRE(QApplication::clipboard());
	QApplication::clipboard()->setText(result.join("\n"));
}
void OutputViewWidget::copyAllMessagesWithLineNumbers(){
	QStringList result;
	for (int i=0;i<logModel->count();i++)
		result << logModel->data(logModel->index(i, 2), Qt::DisplayRole).toString() +": "+logModel->data(logModel->index(i, 3), Qt::DisplayRole).toString();
	REQUIRE(QApplication::clipboard());
	QApplication::clipboard()->setText(result.join("\n"));
}

void OutputViewWidget::showMessageInLog(){
	logViewerTabBar->setCurrentIndex(LAYOUT_PAGE_LOG);
	QModelIndex curMessage = tabbedLogView ? OutputTable2->currentIndex() : OutputTable->currentIndex();
	if (!curMessage.isValid()) return;
	gotoLogEntry(curMessage.row());
}


void OutputViewWidget::addSearch(QList<QDocumentLineHandle *> search,QString name){
	searchResultModel->addSearch(search,name);
}
void OutputViewWidget::clearSearch(){
	searchResultModel->clear();
}
void OutputViewWidget::setSearchExpression(QString exp,bool isCase,bool isWord,bool isRegExp){
        searchResultModel->setSearchExpression(exp,isCase,isWord,isRegExp);
}
int OutputViewWidget::getNextSearchResultColumn(QString text,int col){
        return searchResultModel->getNextSearchResultColumn(text,col);
}
bool OutputViewWidget::childHasFocus(){
	return OutputLogTextEdit->hasFocus() || (tabbedLogView?OutputTable2->hasFocus():OutputTable->hasFocus());
}

void OutputViewWidget::changeEvent(QEvent *event){
	switch (event->type()) {
	case QEvent::LanguageChange:
		retranslateUi();
		break;
	default:
		break;
	}	
}

//====================================================================
// CustomDelegate for search results
//====================================================================
SearchTreeDelegate::SearchTreeDelegate(QObject *parent):QItemDelegate(parent)
{
    ;
}

void SearchTreeDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QPalette::ColorGroup    cg  = option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;

    /*if( cg == QPalette::Normal && !(option.state & QStyle::State_Active) )
        cg = QPalette::Inactive;*/

    if( option.state & QStyle::State_Selected )
    {
        painter->fillRect( option.rect, option.palette.brush(cg, QPalette::Highlight) );
        painter->setPen( option.palette.color(cg, QPalette::HighlightedText) );
    }
    else
    {
        painter->setPen( option.palette.color(cg, QPalette::Text) );
    }

    if( index.data().toString().isEmpty() )
        return;
    painter->save();
    QString text=index.data().toString();
    QRect r=option.rect;
    QStringList textList=text.split("|");
    for(int i=0;i<textList.size();i++){
        QString temp=textList.at(i);
        int w=option.fontMetrics.width(temp);
        if(i%2) {
            painter->fillRect( QRect(r.left(),r.top(),w,r.height()), QBrush(Qt::yellow) );
        }
        painter->drawText(r,Qt::AlignLeft || Qt::AlignTop || Qt::TextSingleLine, temp);
        r.setLeft(r.left()+w+1);
    }
    painter->restore();
}

QSize SearchTreeDelegate::sizeHint(const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
 {
       QFontMetrics fontMetrics = option.fontMetrics;
       QRect rect = fontMetrics.boundingRect(index.data().toString());
       return QSize(rect.width(), rect.height());
}

//====================================================================
// CustomWidgetList (for left panel)
//====================================================================
CustomWidgetList::CustomWidgetList(QWidget* p): 
	QDockWidget(p), toolbox(0), frame(0),stack(0), toolbar(0)
{
	toggleViewAction()->setIcon(QIcon(":/images/sidebar.png"));
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this,SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customContextMenuRequested(QPoint)));
}
void CustomWidgetList::addWidget(QWidget* widget, const QString& id, const QString& text, const QString& iconName){
	widgets << widget;
	widget->setProperty("id",id);
	widget->setProperty("Name",text);
	widget->setProperty("iconName",iconName);
	widget->setProperty("StructPos",widgets.size());

	QAction *Act = new QAction(text, this);
	Act->setCheckable(true);
	Act->setChecked(!hiddenWidgetsIds.contains(id));
	Act->setData(id);
	connect(Act, SIGNAL(toggled(bool)), this, SLOT(toggleWidgetFromAction(bool)));
	addAction(Act);
}
void CustomWidgetList::setWidgetText(const QString& id, const QString& text){
	setWidgetText(widget(id),text);
}
void CustomWidgetList::setWidgetText(QWidget* widget, const QString& text){
	int pos=widgets.indexOf(widget);
	if (pos<0) return;
	widget->setProperty("Name",text);
	if (newStyle) actions()[pos]->setToolTip(text);
	else toolbox->setItemText(pos,text);
}
void CustomWidgetList::setWidgetIcon(const QString& id, const QString& icon){
	setWidgetIcon(widget(id), icon);
}

void CustomWidgetList::setWidgetIcon(QWidget* widget, const QString& icon){
	int pos=widgets.indexOf(widget);
	if (pos<0) return;
	widget->setProperty("iconName",icon);
}

void CustomWidgetList::showPageFromAction(){
	QAction* act=qobject_cast<QAction*>(sender());
	if (!act) return;
	QWidget* wid=widget(act->data().toString());
	stack->setCurrentWidget(wid);
	setWindowTitle(act->toolTip());
	foreach (QAction* a, toolbar->actions())
		a->setChecked(a==act);
}
void CustomWidgetList::currentWidgetChanged(int i){
	Q_ASSERT(newStyle==false);
	setWindowTitle(toolbox->itemText(i));
}
void CustomWidgetList::toggleWidgetFromAction(bool on){
	QAction* act=qobject_cast<QAction*>(sender());
	if (!act || act->data().toString()=="") return;
	if (on) 
		hiddenWidgetsIds.removeAll(act->data().toString());
	else if (!hiddenWidgetsIds.contains(act->data().toString())) 
		hiddenWidgetsIds.append(act->data().toString());
	showWidgets(newStyle);
}
void CustomWidgetList::customContextMenuRequested(const QPoint& localPosition){
	QWidget *widget=currentWidget();
	if(widget && widget->underMouse()) //todo?: use a more reliable function than underMouse (see qt bug 260000)
		emit widgetContextMenuRequested(widget, mapToGlobal(localPosition));
	else{
		QMenu menu;
		menu.addActions(actions());
		menu.exec(mapToGlobal(localPosition));
	}
}
void CustomWidgetList::showWidgets(bool newLayoutStyle){
	if (toolbox) {
		for (int i=0;i<widgets.count();i++){
			toolbox->removeItem(toolbox->indexOf(widgets[i]));
			widgets[i]->setParent(this);//otherwise it will be deleted
		}
		delete toolbox;
	}
	if (stack) {
		for (int i=0;i<widgets.count();i++){
			stack->removeWidget(widgets[i]);
			widgets[i]->setParent(this);//otherwise it will be deleted
		}
		delete stack;
	}
	if (toolbar) delete toolbar;
	if (frame) delete frame; 
	newStyle=newLayoutStyle;
	if (newLayoutStyle) {
		toolbox=0;
		frame=new QFrame(this);
		frame->setLineWidth(0);
		frame->setFrameShape(QFrame::Box);
		frame->setFrameShadow(QFrame::Plain);

		toolbar=new QToolBar("LogToolBar",this);
		toolbar->setFloatable(false);
		toolbar->setOrientation(Qt::Vertical);
		toolbar->setMovable(false);
		toolbar->setIconSize(QSize(16,16 ));

		stack=new QStackedWidget(this);

		for (int i=0;i<widgets.size();i++)
			if (!hiddenWidgetsIds.contains(widgetId(widgets[i]))) {
				stack->addWidget(widgets[i]);
				QAction* act=toolbar->addAction(QIcon(widgets[i]->property("iconName").toString()),widgets[i]->property("Name").toString());
				act->setCheckable(true);
				if (i==0) act->setChecked(true);
				act->setData(widgetId(widgets[i]));
				connect(act,SIGNAL(triggered()),this,SLOT(showPageFromAction()));
				widgets[i]->setProperty("associatedAction", QVariant::fromValue<QAction*>(act));
			} else widgets[i]->hide();
		
		QHBoxLayout* hlayout= new QHBoxLayout(frame);
		hlayout->setSpacing(0);
		hlayout->setMargin(0);
		hlayout->addWidget(toolbar);
		hlayout->addWidget(stack);
			
		setWidget(frame);
	} else {
		frame=0;
		toolbar=0;
		stack=0;
		toolbox = new QToolBox(this);
		for (int i=0;i<widgets.size();i++)
			if (!hiddenWidgetsIds.contains(widgetId(widgets[i]))) {
				toolbox->addItem(widgets[i],QIcon(widgets[i]->property("iconName").toString()),widgets[i]->property("Name").toString());
			} else widgets[i]->hide();
		connect(toolbox,SIGNAL(currentChanged(int)),SLOT(currentWidgetChanged(int)));
		setWidget(toolbox);

	}
	if (!widgets.empty()) //name after active (first) widget
		setWindowTitle(widgets.first()->property("Name").toString());
}
int CustomWidgetList::widgetCount() const{
	return widgets.count();
}
void CustomWidgetList::setHiddenWidgets(const QString& hidden){
	hiddenWidgetsIds=hidden.split("|");
}
QString CustomWidgetList::hiddenWidgets() const{
	return hiddenWidgetsIds.join("|");
}

/*'void CustomWidgetList::addWidgetOld(QWidget* widget, const QString& text, const QIcon& icon){
}
void CustomWidgetList::addWidgetNew(QWidget* widget, const QString& text, const QIcon& icon){
	stack->addWidget(*list);
	toolbar->addAction(icon,text);
}*/

QWidget* CustomWidgetList::widget(int i) const{
	return widgets[i];
}
QWidget* CustomWidgetList::widget(const QString& id) const{
	for (int i=0;i<widgets.size();i++)
		if (widgetId(widgets[i])==id) 
			return widgets[i];
	return 0;
}
void CustomWidgetList::setCurrentWidget(QWidget* widget){
	if (newStyle) {
		stack->setCurrentWidget(widget);
		QAction* act = widget->property("associatedAction").value<QAction*>();
		foreach (QAction* a, toolbar->actions())
			a->setChecked(a==act);
	} else {
		toolbox->setCurrentWidget(widget);
	}
}
QWidget* CustomWidgetList::currentWidget() const{
	if (newStyle) return stack->currentWidget();
	else return toolbox->currentWidget();
}
bool CustomWidgetList::isNewLayoutStyleEnabled() const{
	return newStyle;
}
QString CustomWidgetList::widgetId(QWidget* widget) const{
	if (!widget) return "";
	return widget->property("id").toString();
}
