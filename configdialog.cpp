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

#include "configdialog.h"

#include "qdocument.h"

#include "latexeditorview_config.h"
#include "smallUsefulFunctions.h"
#include "buildmanager.h"
#include "qformatconfig.h"

const QString ShortcutDelegate::addRowButton="<internal: add row>";
const QString ShortcutDelegate::deleteRowButton="<internal: delete row>";

ShortcutComboBox::ShortcutComboBox(QWidget *parent):QComboBox(parent){
	addItem(tr("<default>"));
	addItem(tr("<none>"));
	for (int k=Qt::Key_F1; k<=Qt::Key_F12; k++)
		addItem(QKeySequence(k).toString(QKeySequence::NativeText));
	for (int c=0; c<=1; c++)
		for (int s=0; s<=1; s++)
			for (int a=0; a<=1; a++) {
		if (!c && !s && !a) continue;
		for (int k=Qt::Key_F1; k<=Qt::Key_F12; k++)
			addItem(QKeySequence(c*Qt::CTRL+s*Qt::SHIFT+a*Qt::ALT+k).toString(QKeySequence::NativeText));
		for (int k=Qt::Key_0; k<=Qt::Key_9; k++)
			addItem(QKeySequence(c*Qt::CTRL+s*Qt::SHIFT+a*Qt::ALT+k).toString(QKeySequence::NativeText));
		for (int k=Qt::Key_A; k<=Qt::Key_Z; k++)
			addItem(QKeySequence(c*Qt::CTRL+s*Qt::SHIFT+a*Qt::ALT+k).toString(QKeySequence::NativeText));
		if (a || (c&&s)){
			for (int k=Qt::Key_PageUp; k<=Qt::Key_PageDown; k++)
				addItem(QKeySequence(c*Qt::CTRL+s*Qt::SHIFT+a*Qt::ALT+k).toString(QKeySequence::NativeText));
		}
	}
	setEditable(true);
}

void ShortcutComboBox::keyPressEvent(QKeyEvent *e){
	if ( (e->modifiers()!=0 && e->key() != Qt::Key_Alt && e->key() != Qt::Key_Shift && e->key() != Qt::Key_Control && e->key() != Qt::Key_AltGr && e->key() != Qt::Key_Meta && e->key() != 0 && e->key() != Qt::Key_Super_L && e->key() != Qt::Key_Super_R)
		|| (e->key() >= Qt::Key_F1 &&  e->key() <= Qt::Key_F35)) {
		QString newShortCut = QKeySequence(e->modifiers() | e->key()).toString(QKeySequence::NativeText);
		int index = findText(newShortCut);
		if (index != -1) setCurrentIndex(index);
		else setEditText(newShortCut);
		return;
	}
	QComboBox::keyPressEvent(e);
}


ShortcutDelegate::ShortcutDelegate(QObject *parent): treeWidget(0) {
	// remove unused argument warning
	(void) parent;
}
QWidget *ShortcutDelegate::createEditor(QWidget *parent,
                                        const QStyleOptionViewItem & option ,
                                        const QModelIndex & index) const {
	// remove unused argument warning
	(void) option;

	if (!index.isValid()) return 0;
	const QAbstractItemModel *model = index.model();
	if (model->index(index.row(),0,index.parent()).isValid() &&
	    model->data(model->index(index.row(),0,index.parent()),Qt::DisplayRole) == deleteRowButton) {
		//editor key replacement
		if (index.column()==0) return 0;
		return new QLineEdit(parent);
	}

	//basic key
	if (isBasicEditorKey(index)) {
		if (index.column() == 0) {
			QComboBox *ops = new QComboBox(parent);
			foreach (int o, LatexEditorViewConfig::possibleEditOperations()){
				ops->addItem(LatexEditorViewConfig::translateEditOperation(o), o);
			}
			return ops;
		}
		if (index.column() != 2) return 0;
		//continue as key
	}

	//menu shortcut key
	if (index.column()==1) txsWarning(tr("Sorry, you clicked in the wrong column.\nTo change a shortcut, you have to edit the third or fourth column."));
	if (index.column()!=2 && index.column()!=3) return 0;
	ShortcutComboBox *editor = new ShortcutComboBox(parent);

	return editor;
}

void ShortcutDelegate::setEditorData(QWidget *editor,
                                     const QModelIndex &index) const {
	QComboBox *box = qobject_cast<QComboBox*>(editor);
	//basic editor key
	if (box && isBasicEditorKey(index) && index.column() == 0){
		box->setCurrentIndex(LatexEditorViewConfig::possibleEditOperations().indexOf(index.model()->data(index, Qt::UserRole).toInt()));
		return;
	}
	QString value = index.model()->data(index, Qt::EditRole).toString();
	//menu shortcut key
	if (box) {
		QString normalized=QKeySequence(value).toString(QKeySequence::NativeText);
		int pos=box->findText(normalized);
		if (pos==-1) box->setEditText(value);
		else box->setCurrentIndex(pos);
		return;
	}
	//editor key replacement
	QLineEdit *le = qobject_cast<QLineEdit*>(editor);
	if (le) {
		le->setText(value);
	}
}
void ShortcutDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                    const QModelIndex &index) const {
	REQUIRE(model);
	QComboBox *box = qobject_cast<QComboBox*>(editor);
	//basic editor key
	if (box && isBasicEditorKey(index) && index.column() == 0){
		model->setData(index, box->itemData(box->currentIndex(),Qt::UserRole), Qt::UserRole);
		model->setData(index, box->itemText(box->currentIndex()), Qt::EditRole);
		model->setData(index, box->itemText(box->currentIndex()), Qt::DisplayRole);
		return;
	}
	//editor key replacement
	QLineEdit *le = qobject_cast<QLineEdit*>(editor);
	if (le) {
		if (le->text().size()!=1 && index.column()==1) {
			txsWarning(tr("Only single characters are allowed as key"));
			return;
		}
		model->setData(index, le->text(), Qt::EditRole);
		return;
	}
	//menu shortcut key
	if (index.column()!=2 && index.column()!=3) return;
	if (!box) return;
	QString value=box->currentText();
	if (value=="" || value=="none" || value==tr("<none>")) value="";
	else if (value=="<default>") ;
	else {
		value=QKeySequence(box->currentText()).toString(QKeySequence::NativeText);
		if (value=="" || (value.endsWith("+") && !value.endsWith("++"))) { //Alt+wrong=>Alt+
			txsWarning(ConfigDialog::tr("The shortcut you entered is invalid."));
			return;
		}

		/*int r=-1;
		for (int i=0;i<model->rowCount();i++)
		    if (model->data(model->index(i,2))==value) {
		        r=i;
		        break;
		        }*/
		if (treeWidget) {
			QList<QTreeWidgetItem *> li=treeWidget->findItems(value, Qt::MatchRecursive |Qt::MatchFixedString, 2);
			if (!li.empty() && li[0] && li[0]->text(0) == model->data(model->index(index.row(),0,index.parent()))) li.removeFirst();
			QList<QTreeWidgetItem *> li2=treeWidget->findItems(value, Qt::MatchRecursive |Qt::MatchFixedString, 3);
			if (!li2.empty() && li2[0] && li2[0]->text(0) == model->data(model->index(index.row(),0,index.parent()))) li2.removeFirst();
			li << li2;
			REQUIRE(treeWidget->topLevelItem(1));
			REQUIRE(treeWidget->topLevelItem(1)->childCount()>=1);
			QTreeWidgetItem* editorKeys = treeWidget->topLevelItem(1)->child(0);
			REQUIRE(editorKeys);
			if (!li.empty() && li.first() && (li.first()->parent() != editorKeys || isBasicEditorKey(index))) {
				QString duplicate=li.first()->text(0);//model->data(model->index(mil[0].row(),0,mil[0].parent()),Qt::DisplayRole).toString();
				if (txsConfirmWarning(ConfigDialog::tr("The shortcut you entered is the same as the one of this command:") +"\n"+duplicate+"\n"+ConfigDialog::tr("Should I delete this other shortcut?"))) {
					//model->setData(mil[0],"",Qt::DisplayRole);
					if (li[0]->text(2) == value)  li[0]->setText(2,"");
					else if (li[0]->text(3) == value)  li[0]->setText(3,"");
				}
			}
		}
	}
	model->setData(index, value, Qt::EditRole);
}
void ShortcutDelegate ::updateEditorGeometry(QWidget *editor,
						   const QStyleOptionViewItem &option, const QModelIndex &/* index */) const {
	editor->setGeometry(option.rect);
}

void ShortcutDelegate::drawDisplay(QPainter * painter, const QStyleOptionViewItem & option, const QRect & rect, const QString & text) const {
	QStyle *style = QApplication::style();
	if (!style) QItemDelegate::drawDisplay(painter, option, rect, text);
	else if (text=="<internal: delete row>") {
		QStyleOptionButton sob;
		sob.text = tr("delete row");
		sob.state = QStyle::State_Enabled;
		sob.rect = rect;
		style->drawControl(QStyle::CE_PushButton, &sob, painter);
	} else if (text=="<internal: add row>") {
		QStyleOptionButton sob;
		sob.text = tr("add row");
		sob.state = QStyle::State_Enabled;
		sob.rect = rect;
		style->drawControl(QStyle::CE_PushButton, &  sob, painter);
	} else QItemDelegate::drawDisplay(painter, option, rect, text);
}

void ShortcutDelegate::treeWidgetItemClicked(QTreeWidgetItem * item, int column) {
	Q_ASSERT(item);
	if (column!=0 || !item) return;
	/*QStyle *style = QApplication::style ();
	QPainter p(item->treeWidget ());
	QStyleOptionButton sob;
	sob.rect=item->treeWidget ()->visualItemRect(item);
	sob.state = QStyle::State_Enabled|QStyle::State_Sunken;
	if (style)
	    if (item->text(0)=="<internal: delete row>") {
	       sob.text = tr("delete row");
	       style->drawControl(QStyle::CE_PushButton, &sob, &p);
	    } else if (item->text(0)=="<internal: add row>"){
	       sob.text = tr("add row");
	       style->drawControl(QStyle::CE_PushButton, &  sob, &p);
	    }*/
	if (item->text(0)==deleteRowButton) {
		if (txsConfirm(ConfigDialog::tr("Do you really want to delete this row?"))) {
			Q_ASSERT(item->parent());
			if (!item->parent()) return;
			item->parent()->removeChild(item);
		}
	} else if (item->text(0)==addRowButton) {
		REQUIRE(item->parent());
		REQUIRE(item->treeWidget());
		REQUIRE(item->treeWidget()->topLevelItem(1));
		QString newText = item->parent() == item->treeWidget()->topLevelItem(1)->child(1) ? deleteRowButton : "";
		QTreeWidgetItem *twi = new QTreeWidgetItem((QTreeWidgetItem*)0,QStringList() << newText);
		twi->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
		item->parent()->insertChild(item->parent()->childCount()-1, twi);
	}
}

bool ShortcutDelegate::isBasicEditorKey(const QModelIndex& index) const{
	return index.parent().isValid() && index.parent().parent().isValid() &&
			!index.parent().parent().parent().isValid() &&
			index.parent().row() == 0 &&
			index.parent().parent().row() == 1;
}


ConfigDialog::ConfigDialog(QWidget* parent): QDialog(parent), checkboxInternalPDFViewer(0), buildManager(0) {
	setModal(true);
	ui.setupUi(this);

	ui.contentsWidget->setIconSize(QSize(96, 96));
	ui.contentsWidget->setViewMode(QListView::IconMode);
	ui.contentsWidget->setMovement(QListView::Static);

	//pageditor
	QFontDatabase fdb;
	ui.comboBoxFont->addItems(fdb.families());

	ui.comboBoxEncoding->addItem("UTF-8");
	foreach(int mib, QTextCodec::availableMibs()) {
		QTextCodec *codec = QTextCodec::codecForMib(mib);
		if (codec->name()!="UTF-8") ui.comboBoxEncoding->addItem(codec->name());
	}


	ui.comboBoxDictionaryFileName->setCompleter(0);
	ui.comboBoxThesaurusFileName->setCompleter(0);

	connect(ui.pushButtonAspell, SIGNAL(clicked()), this, SLOT(browseAspell()));
	connect(ui.btSelectThesaurusFileName, SIGNAL(clicked()), this, SLOT(browseThesaurus()));
	connect(ui.comboBoxDictionaryFileName, SIGNAL(editTextChanged(QString)), this, SLOT(lineEditAspellChanged(QString)));
	connect(ui.comboBoxDictionaryFileName, SIGNAL(editTextChanged(QString)), this, SLOT(comboBoxWithPathEdited(QString)));
	connect(ui.comboBoxThesaurusFileName, SIGNAL(editTextChanged(QString)), this, SLOT(comboBoxWithPathEdited(QString)));
	connect(ui.comboBoxDictionaryFileName, SIGNAL(highlighted(QString)), this, SLOT(comboBoxWithPathHighlighted(QString)));
	connect(ui.comboBoxThesaurusFileName, SIGNAL(highlighted(QString)), this, SLOT(comboBoxWithPathHighlighted(QString)));


	ui.labelGetDic->setText(tr("Get dictionaries at: %1").arg("<br><a href=\"http://wiki.services.openoffice.org/wiki/Dictionaries\">http://wiki.services.openoffice.org/wiki/Dictionaries</a>"));
	ui.labelGetDic->setOpenExternalLinks(true);
	//page custom environment
	connect(ui.pbAddLine, SIGNAL(clicked()), this, SLOT(custEnvAddLine()));
	connect(ui.pbRemoveLine, SIGNAL(clicked()), this, SLOT(custEnvRemoveLine()));
	connect(ui.pbAddSyntaxLine, SIGNAL(clicked()), this, SLOT(custSyntaxAddLine()));
	connect(ui.pbRemoveSyntaxLine, SIGNAL(clicked()), this, SLOT(custSyntaxRemoveLine()));
	environModes=0;
	//pagequick
	connect(ui.pushButtonQuickBuildWizard, SIGNAL(clicked()), SLOT(quickBuildWizard()));

	connect(ui.pushButtonExecuteBeforeCompiling, SIGNAL(clicked()), this, SLOT(browsePrecompiling()));


	fmConfig=new QFormatConfig(ui.formatConfigBox);
	fmConfig->addCategory(tr("Basic highlighting")) << "normal" <<"background" <<"comment" <<"commentTodo" <<"keyword" <<"extra-keyword" <<"math-keyword"  <<"numbers" <<"text" <<"environment" <<"structure" <<"escapeseq"  << "verbatim" << "picture" << "sweave";
	fmConfig->addCategory(tr("Inline checking")) <<"braceMatch" <<"braceMismatch" <<"styleHint" <<"spellingMistake" <<"latexSyntaxMistake" <<"referencePresent" <<"referenceMissing" <<"referenceMultiple" <<"citationPresent" <<"citationMissing"  <<"temporaryCodeCompletion";
	fmConfig->addCategory(tr("Line highlighting"))     <<"line:error" <<"line:warning" <<"line:badbox" <<"line:bookmark" <<"line:bookmark0" <<"line:bookmark1" <<"line:bookmark2" <<"line:bookmark3" <<"line:bookmark4" <<"line:bookmark5" <<"line:bookmark6"  <<"line:bookmark7" <<"line:bookmark8"<<"line:bookmark9"<<"current";
	fmConfig->addCategory(tr("Search")) <<"search"<<"replacement"<<"selection";

	connect(ui.spinBoxSize, SIGNAL(valueChanged(int)), fmConfig, SLOT(setBasePointSize(int)));
	
	//fmConfig->setMaximumSize(490,300);
	//fmConfig->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);
	(new QBoxLayout(QBoxLayout::TopToBottom, ui.formatConfigBox))->insertWidget(0,fmConfig);


	ui.shortcutTree->setHeaderLabels(QStringList()<<tr("Command")<<tr("Default Shortcut")<<tr("Current Shortcut")<<tr("Additional Shortcut"));
	ui.shortcutTree->setColumnWidth(0,200);
	
	//create icons
	createIcon(tr("General"),QIcon(":/images/configgeneral.png"));
	createIcon(tr("Commands"),QIcon(":/images/configtools.png"));
	createIcon(tr("Quick Build"),QIcon(":/images/configquick.png"));
	createIcon(tr("Shortcuts"),QIcon(":/images/configkeys.png"));
	createIcon(tr("Latex Menus"),QIcon(":/images/configkeys.png"), true);
	createIcon(tr("Custom Toolbar"),QIcon(":/images/configkeys.png"), true);
	createIcon(tr("Editor"),QIcon(":/images/configeditor.png"));
	createIcon(tr("Adv. Editor"),QIcon(":/images/configeditor.png"), true);
	createIcon(tr("Custom Highlighting"),QIcon(":/images/configeditor.png"), true);
	createIcon(tr("Completion"),QIcon(":/images/configcompletion.png"));
	createIcon(tr("Preview"),QIcon(":/images/configeditor.png"));
	createIcon(tr("SVN"),QIcon(":/images/configtools.png"));

	connect(ui.contentsWidget,
	        SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
	        this, SLOT(changePage(QListWidgetItem *, QListWidgetItem*)));
	ui.contentsWidget->setCurrentRow(0);
	connect(ui.checkBoxShowAdvancedOptions, SIGNAL(toggled(bool)), this, SLOT(advancedOptionsToggled(bool)));
	connect(ui.checkBoxShowAdvancedOptions, SIGNAL(clicked(bool)), this, SLOT(advancedOptionsClicked(bool)));

	// custom toolbars
	connect(ui.comboBoxToolbars,SIGNAL(currentIndexChanged(int)), SLOT(toolbarChanged(int)));
	ui.listCustomToolBar->setIconSize(QSize(96, 96));
	ui.listCustomToolBar->setViewMode(QListView::ListMode);
	ui.listCustomToolBar->setMovement(QListView::Static);
	connect(ui.pbToToolbar,SIGNAL(clicked()),this,SLOT(toToolbarClicked()));
	connect(ui.pbFromToolbar,SIGNAL(clicked()),this,SLOT(fromToolbarClicked()));
	ui.listCustomToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.listCustomToolBar,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(customContextMenuRequested(QPoint)));
	connect(ui.listCustomToolBar,SIGNAL(doubleClicked(QModelIndex)), SLOT(fromToolbarClicked()));

	connect(ui.comboBoxActions,SIGNAL(currentIndexChanged(int)), SLOT(actionsChanged(int)));
	connect(ui.treePossibleToolbarActions,SIGNAL(doubleClicked(QModelIndex)), SLOT(toToolbarClicked()));

#if QT_VERSION >= 0x040400
	ui.treePossibleToolbarActions->setHeaderHidden(true);
#endif
	//	ui.listCustomToolBar->setSelectionMode(QAbstractItemView::ExtendedSelection);
	//	ui.treePossibleToolbarActions->setSelectionMode(QAbstractItemView::ExtendedSelection);

#if (QT_VERSION < 0x040600) || (!defined(Q_WS_X11))
	ui.checkBoxUseSystemTheme->setVisible(false);
#endif
	
	QRect screen = QApplication::desktop()->screenGeometry();
	if (!screen.isEmpty()) {
		int nwidth = width(), nheight = height();
		if (nwidth > screen.width()) nwidth = screen.width();
		if (nheight > screen.height()) nheight = screen.height();
		if (nwidth == width() && nheight == height()) return;
		resize(nwidth, nheight);
		move(frameGeometry().right() > screen.right()?screen.left():x(),
		     frameGeometry().bottom() > screen.bottom()?screen.left():y());
	}	
}

ConfigDialog::~ConfigDialog() {
}

QListWidgetItem * ConfigDialog::createIcon(const QString &caption, const QIcon &icon, bool advancedOption){
	QListWidgetItem * button=new QListWidgetItem(ui.contentsWidget);
	button->setIcon(icon);
	button->setText(caption);
	button->setToolTip(caption);
	button->setTextAlignment(Qt::AlignHCenter);
	button->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
	if (advancedOption) {
		//button->setHidden(true);
		button->setData(Qt::UserRole, true);
	}
	return button;
}

void ConfigDialog::changePage(QListWidgetItem *current, QListWidgetItem *previous) {
	if (!current)
		current = previous;

	ui.pagesWidget->setCurrentIndex(ui.contentsWidget->row(current));
}


//pageditor
void ConfigDialog::browseAspell() {
	QString path=ui.comboBoxDictionaryFileName->currentText();
	if (path.isEmpty()) path=QDir::homePath();
	QString location=QFileDialog::getOpenFileName(this,tr("Browse dictionary"),path,"Dictionary (*.dic)",0,QFileDialog::DontResolveSymlinks);
	if (!location.isEmpty()) {
		location.replace(QString("\\"),QString("/"));
		//	location="\""+location+"\"";
		ui.comboBoxDictionaryFileName->setEditText(location);
	}
}
void ConfigDialog::lineEditAspellChanged(const QString &newText) {
	if (QFile(newText).exists()) {
		ui.comboBoxDictionaryFileName->setStyleSheet(QString());
		ui.labelGetDic->setText(tr("Get dictionary at: %1").arg("<br><a href=\"http://wiki.services.openoffice.org/wiki/Dictionaries\">http://wiki.services.openoffice.org/wiki/Dictionaries</a>"));
	} else {
		ui.comboBoxDictionaryFileName->setStyleSheet(QString("QComboBox {background: red}"));
		ui.labelGetDic->setText("<font color=\"red\">"+tr("(Dictionary doesn't exists)")+"</font><br>"+tr("Get dictionary at: %1").arg("<br><a href=\"http://wiki.services.openoffice.org/wiki/Dictionaries\">http://wiki.services.openoffice.org/wiki/Dictionaries</a>"));
	}
}


//sets the items of a combobox to the filenames and sub-directory names in the directory which name
//is the current text of the combobox
void ConfigDialog::comboBoxWithPathEdited(const QString& newText){
	if (newText.isEmpty()) return;
	QComboBox* box=qobject_cast<QComboBox*>(sender());
	if (!box) return;
	if (box->property("lastDirInDropDown").toString() == newText && !newText.endsWith("/") && !newText.endsWith(QDir::separator())) {
		box->setEditText(newText+QDir::separator());
		box->setProperty("lastDirInDropDown", "");
		return;
	}
	QString path;
	int lastSep = qMax(newText.lastIndexOf("/"),newText.lastIndexOf(QDir::separator()));
	if ((lastSep > 0) || (newText == "") || (QDir::separator()!='/') ) path=newText.mid(0,lastSep);
	else path="/";
	QString oldPath=box->property("dir").toString();
	if (path==oldPath) return;
	//prevent endless recursion + dir caching
	box->setProperty("dir",path);
	int curPos=box->lineEdit()->cursorPosition();
	box->clear();
	box->addItem(newText); //adding a new item in the empty list will set the edit text to it
	box->lineEdit()->setCursorPosition(curPos); // set Cursor to position before clearing

	QDir dir(path);
	QString absPath=dir.absolutePath();
	if (absPath==oldPath || (absPath+QDir::separator())==oldPath) return;
	if (!absPath.endsWith("/") && !absPath.endsWith(QDir::separator())) absPath+=QDir::separator();
	QStringList entries=dir.entryList(QStringList() << box->property("dirFilter").toString(), QDir::Files);
	foreach (const QString& file, entries)
		if (absPath+file!=newText)
			box->addItem(absPath+file);
	entries=dir.entryList(QStringList(), QDir::AllDirs);
	foreach (const QString& file, entries)
		if (absPath+file!=newText && file!=".")
			box->addItem(absPath+file);
}

void ConfigDialog::comboBoxWithPathHighlighted(const QString& newText){
	QComboBox* cb = qobject_cast<QComboBox*> (sender());
	Q_ASSERT(cb);
	if (!cb) return;
	if (!QFileInfo(newText).isDir()) {
		cb->setProperty("lastDirInDropDown", "");
		return;
	}
	cb->setProperty("lastDirInDropDown", newText);
}


void ConfigDialog::browseThesaurus() {
	QString path=ui.comboBoxThesaurusFileName->currentText();
	if (path.isEmpty()) path=QDir::homePath();
	QString location=QFileDialog::getOpenFileName(this,tr("Browse thesaurus database"),path,"Database (*.dat)",0,QFileDialog::DontResolveSymlinks);
	if (!location.isEmpty()) {
		location.replace(QString("\\"),QString("/"));
		//	location="\""+location+"\"";
		ui.comboBoxThesaurusFileName->setEditText(location);
	}
}

void ConfigDialog::browsePrecompiling() {
	QString path=ui.lineEditExecuteBeforeCompiling->text();
	if (path.startsWith('"')) path.remove(0,1);
	if (path.endsWith('"')) path.chop(1);
	if (path.isEmpty()) path=QDir::rootPath();
	QString location=QFileDialog::getOpenFileName(this,tr("Browse program"),path,"Program (*)",0,QFileDialog::DontResolveSymlinks);
	if (!location.isEmpty()) {
		location.replace(QString("\\"),QString("/"));
		location="\""+location+"\"";
		ui.lineEditExecuteBeforeCompiling->setText(location);
	}
}


void ConfigDialog::quickBuildWizard(){
	REQUIRE(buildManager);
	ui.lineEditUserquick->setText(buildManager->editCommandList(ui.lineEditUserquick->text()));
}

void hideShowAdvancedOptions(QWidget* w, bool on){
	foreach (QObject* o, w->children()){
		QWidget* w = qobject_cast<QWidget*> (o);
		if (!w) continue;
		if (w->property("advancedOption").isValid() && w->property("advancedOption").toBool())
			w->setVisible(on);
		hideShowAdvancedOptions(w,on);
	}
}

void ConfigDialog::advancedOptionsToggled(bool on){
	hideShowAdvancedOptions(this,on);
	ui.contentsWidget->reset();
	for (int i=0;i<ui.contentsWidget->count();i++)
		if (ui.contentsWidget->item(i)->data(Qt::UserRole).toBool())
			ui.contentsWidget->item(i)->setHidden(!on);
}

void ConfigDialog::advancedOptionsClicked(bool on){
	if (on) {
		if (!askRiddle()) ui.checkBoxShowAdvancedOptions->setChecked(false);
	}
}

void ConfigDialog::toolbarChanged(int toolbar){
	if (toolbar < 0 || toolbar >= customizableToolbars.size()) return;
	ui.listCustomToolBar->clear();
	foreach (const QString& actName, customizableToolbars[toolbar]){
		QAction* act=menuParent->findChild<QAction*>(actName);
		QListWidgetItem *item;
		if (act) item=new QListWidgetItem(act->icon(),act->text().replace("&",""));
		else item=new QListWidgetItem(actName);
		item->setData(Qt::UserRole,actName);
		ui.listCustomToolBar->addItem(item);
	}
}

void ConfigDialog::actionsChanged(int actionClass){
	ui.treePossibleToolbarActions->clear();
	ui.treePossibleToolbarActions->setRootIsDecorated(actionClass!=0);
	if (actionClass==2){
		//TODO: allow other tags, no hard coding
		QTreeWidgetItem *twi;
		twi=new QTreeWidgetItem(QStringList() << "tags/brackets/left");
		twi->setData(0,Qt::UserRole,"tags/brackets/left");
		ui.treePossibleToolbarActions->addTopLevelItem(twi);
		twi=new QTreeWidgetItem(QStringList() << "tags/brackets/right");
		twi->setData(0,Qt::UserRole,"tags/brackets/right");
		ui.treePossibleToolbarActions->addTopLevelItem(twi);
		twi=new QTreeWidgetItem(QStringList() << "list/dictionaries");
		twi->setData(0,Qt::UserRole,"list/dictionaries");
		ui.treePossibleToolbarActions->addTopLevelItem(twi);
		return;
	}

	const QList<QMenu*> &menus = (actionClass==0)?standardToolbarMenus:allMenus;
	foreach (const QMenu* menu, menus)
		populatePossibleActions(0, menu, actionClass!=0);
}

void ConfigDialog::toToolbarClicked(){
	if (!ui.treePossibleToolbarActions->currentItem()) return;
	if (ui.comboBoxToolbars->currentIndex() < 0 || ui.comboBoxToolbars->currentIndex() >= customizableToolbars.size()) return;
	QTreeWidgetItem *twi = ui.treePossibleToolbarActions->currentItem();
	QString actName=twi->data(0,Qt::UserRole).toString();
	QAction* act=menuParent->findChild<QAction*>(actName);
	QListWidgetItem *item;
	if (act) item=new QListWidgetItem(twi->icon(0),act->text().replace("&",""));
	else item=new QListWidgetItem(twi->icon(0),actName);
	item->setData(Qt::UserRole,actName);
	ui.listCustomToolBar->addItem(item);
	customizableToolbars[ui.comboBoxToolbars->currentIndex()].append(actName);
}

void ConfigDialog::fromToolbarClicked(){
	if(!ui.listCustomToolBar->currentItem()) return;
	if (ui.comboBoxToolbars->currentIndex() < 0 || ui.comboBoxToolbars->currentIndex() >= customizableToolbars.size()) return;
	/*QListWidgetItem *item=ui.listCustomToolBar->currentItem();
    QList<QListWidgetItem *>result=ui.listCustomIcons->findItems(item->text(),Qt::MatchExactly);
    foreach(QListWidgetItem *elem,result){
        elem->setHidden(false);
    }*/
	customizableToolbars[ui.comboBoxToolbars->currentIndex()].removeAt(ui.listCustomToolBar->currentRow());
	delete ui.listCustomToolBar->takeItem(ui.listCustomToolBar->currentRow());
}

void ConfigDialog::customContextMenuRequested(const QPoint &p){
	QMenu menu;
	menu.addAction(tr("Load other icon"),this, SLOT(loadOtherIcon()));
	menu.exec(ui.listCustomToolBar->mapToGlobal(p));
}

void ConfigDialog::loadOtherIcon(){
	QString fn = QFileDialog::getOpenFileName(this,tr("Select a File"),"",tr("Images (*.png *.xpm *.jpg *.bmp *.svg)"));
	if(!fn.isEmpty()){
		QListWidgetItem *item=ui.listCustomToolBar->currentItem();
		item->setIcon(QIcon(fn));
		replacedIconsOnMenus->insert(item->data(Qt::UserRole).toString(),fn);
		ui.listCustomToolBar->reset();
		// set the same icon on other list
		//TODO QList<QListWidgetItem *>result=ui.listCustomIcons->findItems(item->text(),Qt::MatchExactly);
		//foreach(QListWidgetItem *elem,result){
		//  elem->setIcon(QIcon(fn));
		//}
	}
}

void ConfigDialog::populatePossibleActions(QTreeWidgetItem* parent, const QMenu* menu,bool keepHierarchy){
	if (!menu) return;
	QList<QAction *> acts=menu->actions();
	if (keepHierarchy) {
		QTreeWidgetItem* twi = new QTreeWidgetItem(parent,QStringList() << menu->title().replace("&",""));
		twi->setData(0,Qt::UserRole,menu->objectName());
		if (parent) parent->addChild(twi);
		else ui.treePossibleToolbarActions->addTopLevelItem(twi);
		parent=twi;
	}
	for (int i=0; i<acts.size(); i++)
		if (acts[i]->menu()) populatePossibleActions(parent, acts[i]->menu(),keepHierarchy);
	else {
		//if(acts[i]->data().isValid()){
		QTreeWidgetItem* twi = new QTreeWidgetItem(parent,QStringList() << acts[i]->text().replace("&",""));
		if (!acts[i]->isSeparator()){
			if(!acts[i]->icon().isNull()) twi->setIcon(0,acts[i]->icon());
			else twi->setIcon(0,QIcon(":/images/appicon.png"));
		}
		twi->setToolTip(0,acts[i]->text());
		if (acts[i]->isSeparator()) twi->setData(0,Qt::UserRole,"separator");
		else twi->setData(0,Qt::UserRole,acts[i]->objectName());
		if (parent) parent->addChild(twi);
		else ui.treePossibleToolbarActions->addTopLevelItem(twi);
	}
}

void ConfigDialog::custEnvAddLine(){
	int i=ui.twHighlighEnvirons->rowCount();
	ui.twHighlighEnvirons->setRowCount(i+1);

	QStringList lst;
        /*if(environModes)
		lst=*environModes;
        else*/
		lst << "verbatim" << "math";

	QTableWidgetItem *item=new QTableWidgetItem("");
	ui.twHighlighEnvirons->setItem(i,0,item);
	QComboBox *cb=new QComboBox(0);
	cb->insertItems(0,lst);
	ui.twHighlighEnvirons->setCellWidget(i,1,cb);
}

void ConfigDialog::custEnvRemoveLine(){
	int i=ui.twHighlighEnvirons->currentRow();
	if(i<0)
		i=ui.twHighlighEnvirons->rowCount()-1;

	ui.twHighlighEnvirons->removeRow(i);

	i=ui.twHighlighEnvirons->rowCount();
	if(i==0){
		ui.twHighlighEnvirons->setRowCount(i+1);

		QStringList lst;
                if(environModes)
			lst=*environModes;
                else
			lst << "verbatim" << "math";

		QTableWidgetItem *item=new QTableWidgetItem("");
		ui.twHighlighEnvirons->setItem(i,0,item);
		QComboBox *cb=new QComboBox(0);
		cb->insertItems(0,lst);
		ui.twHighlighEnvirons->setCellWidget(i,1,cb);
	}
}

void ConfigDialog::custSyntaxAddLine(){
	int i=ui.twCustomSyntax->rowCount();
	ui.twCustomSyntax->setRowCount(i+1);

	QTableWidgetItem *item=new QTableWidgetItem("");
	ui.twCustomSyntax->setItem(i,0,item);
}

void ConfigDialog::custSyntaxRemoveLine(){
	int i=ui.twCustomSyntax->currentRow();
	if(i<0)
		i=ui.twCustomSyntax->rowCount()-1;
	ui.twCustomSyntax->removeRow(i);

	i=ui.twCustomSyntax->rowCount();
	if(i==0){
		ui.twCustomSyntax->setRowCount(i+1);

		QTableWidgetItem *item=new QTableWidgetItem("");
		ui.twCustomSyntax->setItem(i,0,item);
	}
}

bool ConfigDialog::askRiddle(){
	QString solution = QInputDialog::getText(this, tr("Riddle"), tr(
			"You come to a magic island where you meet three strange and wise friends. \n"
			"One of them is always telling the truth, another one is always lying, and the third is deaf, so he answers randomly and cannot lie(!). \n"
			"You ask the first: \"Are you lying?\", and he answers: \"No\".\n"
			"You ask the second: \"Is the first one lying?\", and he answers: \"No\".\n"
			"You ask the last: \"Is the second one lying?\", and he answers: \"No\".\n\n"
			"Which one of the three wise will always tell the truth?"));
	if (solution.isEmpty()) return false;
	bool a1 = solution.contains("1") || solution.contains(tr("first"),Qt::CaseInsensitive) || solution.contains(tr("one"),Qt::CaseInsensitive);
	bool a2 = solution.contains("2") || solution.contains(tr("second"),Qt::CaseInsensitive) || solution.contains(tr("two"),Qt::CaseInsensitive);
	bool a3 = solution.contains("3") || solution.contains(tr("third"),Qt::CaseInsensitive) || solution.contains(tr("three"),Qt::CaseInsensitive) || solution.contains(tr("last"),Qt::CaseInsensitive);
	if (!a1 && !a2 && !a3) { QMessageBox::warning(this,tr("Riddle"),tr("Please answer 1, 2 or 3")); return false; }
	if ((a1 && a2) || (a1 && a3) || (a2 && a3)) { QMessageBox::warning(this,tr("Riddle"),tr("Only one answer allowed")); return false; }
	if (a3) return true;
	return false;
}


