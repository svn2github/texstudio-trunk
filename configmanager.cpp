
#include "configmanager.h"

#include "configdialog.h"
#include "latexeditorview.h"
#include "latexcompleter_config.h"
#include "latexeditorview_config.h"
#include "webpublishdialog_config.h"
#include "smallUsefulFunctions.h"

#include <QDomElement>

#include "manhattanstyle.h"

ConfigManager::ConfigManager(QObject *parent): QObject (parent),
	buildManager(0),editorConfig(new LatexEditorViewConfig), completerConfig (new LatexCompleterConfig), webPublishDialogConfig (new WebPublishDialogConfig), menuParent(0), menuParentsBar(0){ //TODO: fix theoretical memory leak (it doesn't matter, this is almost a singletone)
}

QSettings* ConfigManager::readSettings() {
	//load config
	bool usbMode = isExistingFileRealWritable(QCoreApplication::applicationDirPath()+"/texmakerx.ini");
	if (!usbMode)
		if (isExistingFileRealWritable(QCoreApplication::applicationDirPath()+"/texmaker.ini")) {
			//import texmaker usb settings
			usbMode=(QFile(QCoreApplication::applicationDirPath()+"/texmaker.ini")).copy(QCoreApplication::applicationDirPath()+"/texmakerx.ini");
		}
	QSettings *config;
	if (usbMode) {
		config=new QSettings(QCoreApplication::applicationDirPath()+"/texmakerx.ini",QSettings::IniFormat);
	} else {
		config=new QSettings(QSettings::IniFormat,QSettings::UserScope,"benibela","texmakerx");
		if (config->childGroups().empty()) {
			//import texmaker global settings
			QSettings oldconfig(QSettings::IniFormat,QSettings::UserScope,"xm1","texmaker");
			QStringList keys=oldconfig.allKeys();
			foreach(QString key, keys) config->setValue(key,oldconfig.value(key,""));
		}
	}
	configFileName=config->fileName();
	configFileNameBase=configFileName;
	if (configFileNameBase.endsWith(".ini")) configFileNameBase=configFileNameBase.replace(QString(".ini"),"");

	config->beginGroup("texmaker");

	//-----------------------files---------------------------
	//editor
	newfile_encoding=QTextCodec::codecForName(config->value("Files/New File Encoding", "utf-8").toString().toAscii().data());
	autodetectLoadedFile=config->value("Files/Auto Detect Encoding Of Loaded Files", "true").toBool();

	ignoreLogFileNames=config->value("Files/Ignore Log File Names",0).toInt(); //default never ignore

	//recent files
	maxRecentFiles=config->value("Files/Max Recent Files", 5).toInt();
	maxRecentProjects=config->value("Files/Max Recent Projects", 3).toInt();
	recentFilesList=config->value("Files/Recent Files").toStringList();
	recentProjectList=config->value("Files/Recent Project Files").toStringList();
	sessionRestore=config->value("Files/RestoreSession",false).toBool(); 
	sessionFilesToRestore=config->value("Files/Session/Files").toStringList();
	sessionCurrent=config->value("Files/Session/CurrentFile","").toString();
	sessionMaster=config->value("Files/Session/MasterFile","").toString();
	lastDocument=config->value("Files/Last Document","").toString();
	parseBibTeX=config->value("Files/Parse BibTeX",true).toBool();
	parseMaster=config->value("Files/Parse Master",true).toBool();
	
	
	//----------------------------editor--------------------
	editorConfig->wordwrap=config->value("Editor/WordWrap",true).toBool();
	editorConfig->parenmatch=config->value("Editor/Parentheses Matching",true).toBool();
	editorConfig->showlinemultiples=config->value("Editor/Line Number Multiples",-1).toInt();
	if (editorConfig->showlinemultiples==-1) {
		if (config->value("Editor/Line Numbers",true).toBool()) editorConfig->showlinemultiples=1;  //texmaker import
		else editorConfig->showlinemultiples=0;
	}

	editorConfig->autoindent=config->value("Editor/Auto Indent",true).toBool();
	editorConfig->weakindent=config->value("Editor/Weak Indent",true).toBool();
	editorConfig->folding=config->value("Editor/Folding",true).toBool();
	editorConfig->showlinestate=config->value("Editor/Show Line State",true).toBool();
	editorConfig->showcursorstate=config->value("Editor/Show Cursor State",true).toBool();	
	editorConfig->readSettings(*config);
	
	//completion
	completerConfig->enabled=config->value("Editor/Completion",true).toBool();
	completerConfig->caseSensitive=(LatexCompleterConfig::CaseSensitive) config->value("Editor/Completion Case Sensitive",2).toInt();
	completerConfig->completeCommonPrefix=config->value("Editor/Completion Complete Common Prefix",true).toBool();
	completerConfig->eowCompletes=config->value("Editor/Completion EOW Completes", true).toBool();
	completerConfig->tooltipHelp=config->value("Editor/Completion Enable Tooltip Help", true).toBool();
	completerConfig->usePlaceholders=config->value("Editor/Completion Use Placeholders", true).toBool();
	completerConfig->loadFiles(config->value("Editor/Completion Files",QStringList() << "texmakerx.cwl" << "tex.cwl" << "latex-document.cwl" << "latex-mathsymbols.cwl").toStringList());
	
	//web publish dialog
	webPublishDialogConfig->readSettings(*config);

	//other dialogs
	lastHardWrapColumn=config->value("Dialogs/Last Hard Wrap Column",80).toInt();
	
	//preview
	previewMode=(PreviewMode) config->value("Preview/Mode",0).toInt();

	//build commands
	if (!buildManager) QMessageBox::critical(0,"TexMakerX","No build Manager created! => crash",QMessageBox::Ok);
	buildManager->readSettings(*config);
	showLogAfterCompiling=config->value("Tools/Show Log After Compiling", true).toBool();
	
	//read user key replacements
	keyReplace.clear();
	keyReplaceAfterWord.clear();
	keyReplaceBeforeWord.clear();
	int keyReplaceCount = config->value("User/KeyReplaceCount",-1).toInt();
	if (keyReplaceCount ==-1) {
		//default
		keyReplace.append("\"");
		QString loc=QString(QLocale::system().name()).left(2);
		if (loc=="de") {
			keyReplaceBeforeWord.append("\">");
			keyReplaceAfterWord.append("\"<");
/*
			keyReplace.append("'");
			keyReplaceBeforeWord.append("''");
			keyReplaceAfterWord.append("``");*/
		} else {
			keyReplaceAfterWord.append("''");
			keyReplaceBeforeWord.append("``");
		}
		keyReplace.append("%");
		keyReplaceBeforeWord.append("%");
		keyReplaceAfterWord.append(" %");
	} else for (int i=0; i<keyReplaceCount; i++) {
			keyReplace.append(config->value("User/KeyReplace"+QVariant(i).toString(),i!=0?"'":"\"").toString());
			keyReplaceAfterWord.append(config->value("User/KeyReplaceAfterWord"+QVariant(i).toString(),i!=0?"":"").toString());
			keyReplaceBeforeWord.append(config->value("User/KeyReplaceBeforeWord"+QVariant(i).toString(),i!=0?"":"\">").toString());
		}
	LatexEditorView::setKeyReplacements(&keyReplace,&keyReplaceAfterWord,&keyReplaceBeforeWord);

	//menu shortcuts
	int size = config->beginReadArray("keysetting");
	for (int i = 0; i < size; ++i) {
		config->setArrayIndex(i);
		managedMenuNewShortcuts.append(QPair<QString, QString> (config->value("id").toString(), config->value("key").toString()));
	}
	config->endArray();
	
	
	//--------------------appearance------------------------------------
	QFontDatabase fdb;
	QStringList xf = fdb.families();
	QString deft;
	//editor
#ifdef Q_WS_WIN
	if (xf.contains("Courier New",Qt::CaseInsensitive)) deft="Courier New";
	else deft=qApp->font().family();
	QString fam=config->value("Editor/Font Family",deft).toString();
	int si=config->value("Editor/Font Size",10).toInt();
#else
	if (xf.contains("DejaVu Sans Mono",Qt::CaseInsensitive)) deft="DejaVu Sans Mono";
//else if (xf.contains("Lucida Sans Unicode",Qt::CaseInsensitive)) deft="Lucida Sans Unicode";
	else if (xf.contains("Lucida Sans Typewriter",Qt::CaseInsensitive)) deft="Lucida Sans Typewriter";
	else deft=qApp->font().family();
	QString fam=config->value("Editor/Font Family",deft).toString();
	int si=config->value("Editor/Font Size",qApp->font().pointSize()).toInt();
#endif
	editorConfig->editorFont=QFont(fam,si);

	editorConfig->displayModifyTime=config->value("Editor/Display Modifytime",true).toBool();
	editorConfig->closeSearchAndReplace=config->value("Editor/Close Search Replace Together",false).toBool();
	editorConfig->useLineForSearch=config->value("Editor/Use Line For Search",true).toBool();
	//interface
#ifdef Q_WS_X11
	if ((x11desktop_env() != 4) || (!QStyleFactory::keys().contains("Oxygen")))
		interfaceStyle="Plastique"; //plastique style if not kde4
	else
		interfaceStyle="";
		
	if (xf.contains("DejaVu Sans",Qt::CaseInsensitive)) interfaceFontFamily="DejaVu Sans";
	else if (xf.contains("DejaVu Sans LGC",Qt::CaseInsensitive)) interfaceFontFamily="DejaVu Sans LGC";
	else if (xf.contains("Bitstream Vera Sans",Qt::CaseInsensitive)) interfaceFontFamily="Bitstream Vera Sans";
	else if (xf.contains("Luxi Sans",Qt::CaseInsensitive)) interfaceFontFamily="Luxi Sans";
	else interfaceFontFamily=QApplication::font().family();
#else
	interfaceStyle="";
	interfaceFontFamily=QApplication::font().family();
#endif
		
	configShowAdvancedOptions = config->value("Interface/Config Show Advanced Options",false).toBool();
	interfaceStyle=config->value("X11/Style",interfaceStyle).toString(); //named X11 for backward compatibility
	defaultStyleName=QApplication::style()->objectName();
	#if QT_VERSION >= 0x040500
	modernStyle=config->value("GUI/Style", false).toBool();
	if (modernStyle) {
		ManhattanStyle* style=new ManhattanStyle(interfaceStyle==""?defaultStyleName:interfaceStyle);
		if (style->isValid()) QApplication::setStyle(style);
	} else 
	#endif
		if (interfaceStyle!="") QApplication::setStyle(interfaceStyle); 

	interfaceFontFamily = config->value("X11/Font Family",QApplication::font().family()).toString();
	interfaceFontSize = config->value("X11/Font Size",QApplication::font().pointSize()).toInt();		
	QApplication::setFont(QFont(interfaceFontFamily, interfaceFontSize));
	
#ifdef Q_WS_X11
	QPalette pal = QApplication::palette();
	pal.setColor(QPalette::Active, QPalette::Highlight, QColor("#4490d8"));
	pal.setColor(QPalette::Inactive, QPalette::Highlight, QColor("#4490d8"));
	pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor("#4490d8"));

	pal.setColor(QPalette::Active, QPalette::HighlightedText, QColor("#ffffff"));
	pal.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor("#ffffff"));
	pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#ffffff"));

	pal.setColor(QPalette::Active, QPalette::Base, QColor("#ffffff"));
	pal.setColor(QPalette::Inactive, QPalette::Base, QColor("#ffffff"));
	pal.setColor(QPalette::Disabled, QPalette::Base, QColor("#ffffff"));

	pal.setColor(QPalette::Active, QPalette::WindowText, QColor("#000000"));
	pal.setColor(QPalette::Inactive, QPalette::WindowText, QColor("#000000"));
	pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#000000"));

	pal.setColor(QPalette::Active, QPalette::ButtonText, QColor("#000000"));
	pal.setColor(QPalette::Inactive, QPalette::ButtonText, QColor("#000000"));
	pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#000000"));

	if (x11desktop_env() ==4) {
		pal.setColor(QPalette::Active, QPalette::Window, QColor("#eae9e9"));
		pal.setColor(QPalette::Inactive, QPalette::Window, QColor("#eae9e9"));
		pal.setColor(QPalette::Disabled, QPalette::Window, QColor("#eae9e9"));

		pal.setColor(QPalette::Active, QPalette::Button, QColor("#eae9e9"));
		pal.setColor(QPalette::Inactive, QPalette::Button, QColor("#eae9e9"));
		pal.setColor(QPalette::Disabled, QPalette::Button, QColor("#eae9e9"));
	} else {
		pal.setColor(QPalette::Active, QPalette::Window, QColor("#fbf8f1"));
		pal.setColor(QPalette::Inactive, QPalette::Window, QColor("#fbf8f1"));
		pal.setColor(QPalette::Disabled, QPalette::Window, QColor("#fbf8f1"));

		pal.setColor(QPalette::Active, QPalette::Button, QColor("#fbf8f1"));
		pal.setColor(QPalette::Inactive, QPalette::Button, QColor("#fbf8f1"));
		pal.setColor(QPalette::Disabled, QPalette::Button, QColor("#fbf8f1"));
	}

	QApplication::setPalette(pal);
#endif
	

	tabbedLogView=config->value("LogView/Tabbed","true").toBool();
	newLeftPanelLayout=config->value("Interface/New Left Panel Layout",true).toBool();

	//language
	language=config->value("Interface/Language","").toString();
	lastLanguage=language;
	QString locale=language;
	appTranslator=new QTranslator(this);
	basicTranslator=new QTranslator(this);
	loadTranslations(language);
	QCoreApplication::installTranslator(appTranslator);
	QCoreApplication::installTranslator(basicTranslator);

	#ifndef QT_NO_DEBUG
	debugLastFileModification=config->value("Debug/Last Application Modification").toDateTime();
	#endif

	
	config->endGroup();

	return config;
}
QSettings* ConfigManager::saveSettings() {
	QSettings *config=new QSettings(configFileName, QSettings::IniFormat);
	config->setValue("IniMode",true);

	config->beginGroup("texmaker");

	//-----------------------files------------------------
	//editor
	config->setValue("Files/New File Encoding", newfile_encoding?newfile_encoding->name():"??");
	config->setValue("Files/Auto Detect Encoding Of Loaded Files", autodetectLoadedFile);
	
	config->setValue("Files/Ignore Log File Names",ignoreLogFileNames);
	
	//recent files
	config->setValue("Files/Max Recent Files", maxRecentFiles);
	config->setValue("Files/Max Recent Projects", maxRecentProjects);
	if (recentFilesList.count()>0) config->setValue("Files/Recent Files",recentFilesList);
	if (recentProjectList.count()>0) config->setValue("Files/Recent Project Files",recentProjectList);
	//session is saved by main class (because we don't know the active files here)
	config->setValue("Files/Last Document",lastDocument);
	config->setValue("Files/Parse BibTeX",parseBibTeX);
	config->setValue("Files/Parse Master",parseMaster);

	//--------------------editor--------------------------
	config->setValue("Editor/WordWrap",editorConfig->wordwrap);

	config->setValue("Editor/Parentheses Matching",editorConfig->parenmatch);
	config->setValue("Editor/Line Number Multiples",editorConfig->showlinemultiples);
	config->setValue("Editor/Auto Indent",editorConfig->autoindent);
	config->setValue("Editor/Weak Indent",editorConfig->weakindent);

	config->setValue("Editor/Folding",editorConfig->folding);
	config->setValue("Editor/Show Line State",editorConfig->showlinestate);
	config->setValue("Editor/Show Cursor State",editorConfig->showcursorstate);
	editorConfig->saveSettings(*config);
	
	//completion
	config->setValue("Editor/Completion",completerConfig->enabled);
	config->setValue("Editor/Completion Case Sensitive",completerConfig->caseSensitive);
	config->setValue("Editor/Completion Complete Common Prefix",completerConfig->completeCommonPrefix);
	config->setValue("Editor/Completion EOW Completes", completerConfig->eowCompletes);
	config->setValue("Editor/Completion Enable Tooltip Help", completerConfig->tooltipHelp);
	config->setValue("Editor/Completion Use Placeholders", completerConfig->usePlaceholders);
	if (!completerConfig->getLoadedFiles().isEmpty())
		config->setValue("Editor/Completion Files",completerConfig->getLoadedFiles());

	//web publish dialog
	webPublishDialogConfig->saveSettings(*config);
	
	//other dialogs
	config->setValue("Dialogs/Last Hard Wrap Column",lastHardWrapColumn);
		
	//preview
	config->setValue("Preview/Mode",previewMode);
	
	//---------------------build commands----------------
	buildManager->saveSettings(*config);
	config->setValue("Tools/Show Log After Compiling", showLogAfterCompiling);
	
	//-------------------key replacements-----------------
	int keyReplaceCount = keyReplace.count();
	config->setValue("User/KeyReplaceCount",keyReplaceCount);
	for (int i=0; i<keyReplaceCount; i++) {
		config->setValue("User/KeyReplace"+QVariant(i).toString(),keyReplace[i]);
		config->setValue("User/KeyReplaceAfterWord"+QVariant(i).toString(),keyReplaceAfterWord[i]);
		config->setValue("User/KeyReplaceBeforeWord"+QVariant(i).toString(),keyReplaceBeforeWord[i]);
	}

	//menu shortcuts
	config->beginWriteArray("keysetting");
	for (int i = 0; i < managedMenuNewShortcuts.size(); ++i) {
		config->setArrayIndex(i);
		config->setValue("id", managedMenuNewShortcuts[i].first);
		config->setValue("key", managedMenuNewShortcuts[i].second);
	}
	config->endArray();

	//------------------appearance--------------------
	config->setValue("Interface/Config Show Advanced Options",configShowAdvancedOptions);
	config->setValue("X11/Style",interfaceStyle); //named X11/ for backward compatibility
	config->setValue("X11/Font Family",interfaceFontFamily);
	config->setValue("X11/Font Size",interfaceFontSize);
	config->setValue("GUI/Style", modernStyle);
	
	config->setValue("LogView/Tabbed",tabbedLogView);
	
	config->setValue("Interface/New Left Panel Layout",newLeftPanelLayout);
	config->setValue("Interface/Language",language); 
	
	//editor
	config->setValue("Editor/Font Family",editorConfig->editorFont.family());
	config->setValue("Editor/Font Size",editorConfig->editorFont.pointSize());

	config->setValue("Editor/Display Modifytime",editorConfig->displayModifyTime);
	config->setValue("Editor/Close Search Replace Together",editorConfig->closeSearchAndReplace);
	config->setValue("Editor/Use Line For Search",editorConfig->useLineForSearch);

	//debug
	#ifndef QT_NO_DEBUG
	config->setValue("Debug/Last Application Modification",debugLastFileModification);
	#endif
	
	config->endGroup();

	return config;
}


ConfigDialog* ConfigManager::createConfigDialog(QWidget* parent) {
	ConfigDialog *confDlg = new ConfigDialog(parent);
	return confDlg;
}
bool ConfigManager::execConfigDialog(ConfigDialog* confDlg) {

	//files
	if (newfile_encoding)
		confDlg->ui.comboBoxEncoding->setCurrentIndex(confDlg->ui.comboBoxEncoding->findText(newfile_encoding->name(), Qt::MatchExactly));
	confDlg->ui.checkBoxAutoDetectOnLoad->setChecked(autodetectLoadedFile);
	confDlg->ui.comboBoxIgnoreLogFileNames->setCurrentIndex(ignoreLogFileNames);
	
	confDlg->ui.spinBoxMaxRecentFiles->setValue(maxRecentFiles);
	confDlg->ui.spinBoxMaxRecentProjects->setValue(maxRecentProjects);
	confDlg->ui.checkBoxParseBibTeX->setChecked(parseBibTeX);
	confDlg->ui.checkBoxParseMaster->setChecked(parseMaster);
	
	//-----------------------editor------------------------------
	confDlg->ui.checkBoxWordwrap->setChecked(editorConfig->wordwrap);
	
	switch (editorConfig->showlinemultiples) {
	case 0:
		confDlg->ui.comboboxLineNumbers->setCurrentIndex(0);
		break;
	case 10:
		confDlg->ui.comboboxLineNumbers->setCurrentIndex(2);
		break;
	default:
		confDlg->ui.comboboxLineNumbers->setCurrentIndex(1);
	}
	if (editorConfig->autoindent && editorConfig->weakindent) confDlg->ui.comboBoxAutoIndent->setCurrentIndex(1);
	else if (editorConfig->autoindent) confDlg->ui.comboBoxAutoIndent->setCurrentIndex(2);
	else confDlg->ui.comboBoxAutoIndent->setCurrentIndex(0);
	confDlg->ui.checkBoxFolding->setChecked(editorConfig->folding);
	confDlg->ui.checkBoxLineState->setChecked(editorConfig->showlinestate);
	confDlg->ui.checkBoxState->setChecked(editorConfig->showcursorstate);
	confDlg->ui.checkBoxRealTimeCheck->setChecked(editorConfig->realtimeChecking);
	confDlg->ui.checkBoxInlineSpellCheck->setChecked(editorConfig->inlineSpellChecking);
	confDlg->ui.checkBoxInlineReferenceCheck->setChecked(editorConfig->inlineReferenceChecking);
	confDlg->ui.checkBoxInlineCitationCheck->setChecked(editorConfig->inlineCitationChecking);
	
	//completion
	confDlg->ui.checkBoxCompletion->setChecked(completerConfig->enabled);
	confDlg->ui.checkBoxCaseSensitive->setChecked(completerConfig->caseSensitive!=LatexCompleterConfig::CCS_CASE_INSENSITIVE);
	confDlg->ui.checkBoxCaseSensitiveInFirstCharacter->setChecked(completerConfig->caseSensitive==LatexCompleterConfig::CCS_FIRST_CHARACTER_CASE_SENSITIVE);
	confDlg->ui.checkBoxCompletePrefix->setChecked(completerConfig->completeCommonPrefix);
	confDlg->ui.checkBoxEOWCompletes->setChecked(completerConfig->eowCompletes);
	confDlg->ui.checkBoxToolTipHelp->setChecked(completerConfig->tooltipHelp);
	confDlg->ui.checkBoxUsePlaceholders->setChecked(completerConfig->usePlaceholders);
	
	
	QStringList languageFiles=findResourceFiles("translations","texmakerx_*.qm") 
							<< findResourceFiles("","texmakerx_*.qm");
	int langId=-1;
	for (int i=0;i<languageFiles.count();i++){
		//_gettext. 
		QString cur=languageFiles[i].mid(languageFiles[i].lastIndexOf("_")+1);
		cur.truncate(cur.indexOf("."));
		confDlg->ui.comboBoxLanguage->addItem(cur);
		if (cur == language) langId=i;
	}
	confDlg->ui.comboBoxLanguage->addItem("en");
	confDlg->ui.comboBoxLanguage->addItem(tr("default"));
	if (language=="") confDlg->ui.comboBoxLanguage->setEditText(tr("default"));
	else confDlg->ui.comboBoxLanguage->setEditText(language);
	if (langId!=-1) confDlg->ui.comboBoxLanguage->setCurrentIndex(langId);
	else if (language=="en") confDlg->ui.comboBoxLanguage->setCurrentIndex(confDlg->ui.comboBoxLanguage->count()-2);
	else confDlg->ui.comboBoxLanguage->setCurrentIndex(confDlg->ui.comboBoxLanguage->count()-1);
	
	QStringList files=findResourceFiles("completion","*.cwl");
	QListWidgetItem *item;
	const QStringList& loadedFiles = completerConfig->getLoadedFiles();
	foreach(QString elem,files) {
		item=new QListWidgetItem(elem,confDlg->ui.completeListWidget);
		item->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
		if (loadedFiles.contains(elem)) item->setCheckState(Qt::Checked);
		else  item->setCheckState(Qt::Unchecked);
	}
	
	//preview
	confDlg->ui.comboBoxPreviewMode->setCurrentIndex(previewMode);
	confDlg->ui.comboBoxDvi2PngMode->setCurrentIndex(buildManager->dvi2pngMode);
	
	//--build things
	//normal commands
	QGridLayout* gl=new QGridLayout(confDlg->ui.groupBoxCommands);
	confDlg->ui.groupBoxCommands->setLayout(gl);
	for (BuildManager::LatexCommand cmd=BuildManager::CMD_LATEX; cmd <= BuildManager::CMD_GHOSTSCRIPT; ++cmd){
		QLabel *l = new QLabel(confDlg);
		l->setText(BuildManager::commandDisplayName(cmd));
		QLineEdit *e = new QLineEdit(confDlg);
		e->setText(buildManager->getLatexCommandForDisplay(cmd));
		QPushButton *b = new QPushButton(confDlg);
		b->setIcon(QIcon(":/images/fileopen.png"));
		connect(b,SIGNAL(clicked()),this,SLOT(browseCommand()));
		QPushButton *bdefault = new QPushButton(confDlg);
		bdefault->setIcon(QIcon(":/images/undo.png"));
		connect(bdefault,SIGNAL(clicked()),this,SLOT(undoCommand()));
 		gl->addWidget(l,(int)cmd,0);
		gl->addWidget(e,(int)cmd,1);
		gl->addWidget(b,(int)cmd,2);
		gl->addWidget(bdefault,(int)cmd,3);
		buttonsToCommands.insert(b,cmd);
		buttonsToCommands.insert(bdefault,cmd);
		commandsToEdits.insert(cmd,e);
	}
	//quickbuild/more page
	if (buildManager->quickmode==1) confDlg->ui.radioButton1->setChecked(true);
	else if (buildManager->quickmode==2) confDlg->ui.radioButton2->setChecked(true);
	else if (buildManager->quickmode==3) confDlg->ui.radioButton3->setChecked(true);
	else if (buildManager->quickmode==4) confDlg->ui.radioButton4->setChecked(true);
	else if (buildManager->quickmode==5) confDlg->ui.radioButton5->setChecked(true);
	else if (buildManager->quickmode==6) confDlg->ui.radioButton6->setChecked(true);
	confDlg->ui.lineEditUserquick->setEnabled(buildManager->quickmode==6);
	confDlg->ui.lineEditExecuteBeforeCompiling->setText(buildManager->getLatexCommandForDisplay(BuildManager::CMD_USER_PRECOMPILE));
	confDlg->ui.lineEditUserquick->setText(buildManager->getLatexCommandForDisplay(BuildManager::CMD_USER_QUICK));
	
	confDlg->ui.checkBoxShowLog->setChecked(showLogAfterCompiling);
	
	//menu shortcuts
	QTreeWidgetItem * menuShortcuts=new QTreeWidgetItem((QTreeWidget*)0, QStringList() << QString(tr("Menus")));
	foreach(QMenu* menu, managedMenus)
	managedMenuToTreeWidget(menuShortcuts,menu);
	confDlg->ui.shortcutTree->addTopLevelItem(menuShortcuts);
	menuShortcuts->setExpanded(true);

	QTreeWidgetItem * editorItem=new QTreeWidgetItem((QTreeWidget*)0, QStringList() << ConfigDialog::tr("Editor"));
	//add special key replacements
	QTreeWidgetItem * keyReplacements = new QTreeWidgetItem(editorItem, QStringList() << ConfigDialog::tr("Special Key Replacement"));
	QTreeWidgetItem * columnItem=new QTreeWidgetItem(keyReplacements, QStringList() << ConfigDialog::tr("New column meaning:") << ConfigDialog::tr("Key to replace") << ConfigDialog::tr("Text to insert before word") << ConfigDialog::tr("Text to insert after word"));
	QFont f;
	f.setUnderline(true);
	columnItem->setFont(0,f);
	f.setBold(true);
	columnItem->setFont(1,f);
	columnItem->setFont(2,f);
	columnItem->setFont(3,f);
	for (int i=0; i<keyReplace.size(); i++)
		(new QTreeWidgetItem(keyReplacements, QStringList() << ShortcutDelegate::deleteRowButton << keyReplace[i] << keyReplaceBeforeWord[i] << keyReplaceAfterWord[i]))
		->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
	new QTreeWidgetItem(keyReplacements, QStringList() << ShortcutDelegate::addRowButton);
	confDlg->ui.shortcutTree->addTopLevelItem(editorItem);
	editorItem->setExpanded(true);

	ShortcutDelegate delegate;
	delegate.treeWidget=confDlg->ui.shortcutTree;
	confDlg->ui.shortcutTree->setItemDelegate(&delegate); //setting in the config dialog doesn't work
	delegate.connect(confDlg->ui.shortcutTree,SIGNAL(itemClicked(QTreeWidgetItem *, int)),&delegate,SLOT(treeWidgetItemClicked(QTreeWidgetItem * , int)));

	//appearance
	confDlg->ui.checkBoxShowAdvancedOptions->setChecked(configShowAdvancedOptions);
	confDlg->ui.comboBoxInterfaceStyle->clear();
	confDlg->ui.comboBoxInterfaceStyle->addItems(QStyleFactory::keys()<<tr("default"));
	if (interfaceStyle=="") //default
		confDlg->ui.comboBoxInterfaceStyle->setCurrentIndex(confDlg->ui.comboBoxInterfaceStyle->count()-1);
	else 
		confDlg->ui.comboBoxInterfaceStyle->setCurrentIndex(QStyleFactory::keys().indexOf(interfaceStyle));
	confDlg->ui.comboBoxInterfaceStyle->setEditText(interfaceStyle);
	confDlg->ui.comboBoxInterfaceFont->setCurrentFont(QFont(interfaceFontFamily));
	confDlg->ui.spinBoxInterfaceFontSize->setValue(interfaceFontSize);

	confDlg->ui.checkBoxTabbedLogView->setChecked(tabbedLogView);
	confDlg->ui.checkBoxTabbedStructureView->setChecked(!newLeftPanelLayout);
	
	confDlg->ui.checkBoxDisplayModifyTime->setChecked(editorConfig->displayModifyTime);
	confDlg->ui.checkBoxCloseSearchReplaceTogether->setChecked(editorConfig->closeSearchAndReplace);
	confDlg->ui.checkBoxUseLineForSearch->setChecked(editorConfig->useLineForSearch);
	
	confDlg->ui.comboBoxInterfaceModernStyle->setCurrentIndex(modernStyle?1:0);
	
	//editor font
	confDlg->ui.comboBoxFont->lineEdit()->setText(editorConfig->editorFont.family());
	confDlg->ui.spinBoxSize->setValue(editorConfig->editorFont.pointSize());
	
	
	//EXECUTE IT
	bool executed = confDlg->exec();
	//handle changes
	if (executed) {
		//files
		newfile_encoding=QTextCodec::codecForName(confDlg->ui.comboBoxEncoding->currentText().toAscii().data());
		autodetectLoadedFile=confDlg->ui.checkBoxAutoDetectOnLoad->isChecked();
		ignoreLogFileNames=confDlg->ui.comboBoxIgnoreLogFileNames->currentIndex();
		
		if (maxRecentFiles!=confDlg->ui.spinBoxMaxRecentFiles->value() ||
		   maxRecentProjects != confDlg->ui.spinBoxMaxRecentProjects->value()){
			maxRecentFiles=confDlg->ui.spinBoxMaxRecentFiles->value();
			maxRecentProjects=confDlg->ui.spinBoxMaxRecentProjects->value();
			updateRecentFiles(true);
		}
		parseBibTeX=confDlg->ui.checkBoxParseBibTeX->isChecked();
		parseMaster=confDlg->ui.checkBoxParseMaster->isChecked();
		
		//editor
		editorConfig->wordwrap=confDlg->ui.checkBoxWordwrap->isChecked();
		editorConfig->autoindent=confDlg->ui.comboBoxAutoIndent->currentIndex()!=0;
		editorConfig->weakindent=confDlg->ui.comboBoxAutoIndent->currentIndex()==1;
		switch (confDlg->ui.comboboxLineNumbers->currentIndex()) {
		case 0:
			editorConfig->showlinemultiples=0;
			break;
		case 2:
			editorConfig->showlinemultiples=10;
			break;
		default:
			editorConfig->showlinemultiples=1;
			break;
		}
		editorConfig->folding=confDlg->ui.checkBoxFolding->isChecked();
		editorConfig->showlinestate=confDlg->ui.checkBoxLineState->isChecked();
		editorConfig->showcursorstate=confDlg->ui.checkBoxState->isChecked();
		editorConfig->realtimeChecking=confDlg->ui.checkBoxRealTimeCheck->isChecked();
		editorConfig->inlineSpellChecking=confDlg->ui.checkBoxInlineSpellCheck->isChecked();
		editorConfig->inlineReferenceChecking=confDlg->ui.checkBoxInlineReferenceCheck->isChecked();
		editorConfig->inlineCitationChecking=confDlg->ui.checkBoxInlineCitationCheck->isChecked();
		
		
		//completion
		completerConfig->enabled=confDlg->ui.checkBoxCompletion->isChecked();
		if (!confDlg->ui.checkBoxCaseSensitive->isChecked()) completerConfig->caseSensitive=LatexCompleterConfig::CCS_CASE_INSENSITIVE;
		else if (confDlg->ui.checkBoxCaseSensitiveInFirstCharacter->isChecked()) completerConfig->caseSensitive=LatexCompleterConfig::CCS_FIRST_CHARACTER_CASE_SENSITIVE;
		else completerConfig->caseSensitive=LatexCompleterConfig::CCS_CASE_SENSITIVE;
		completerConfig->completeCommonPrefix=confDlg->ui.checkBoxCompletePrefix->isChecked();
		completerConfig->eowCompletes=confDlg->ui.checkBoxEOWCompletes->isChecked();
		completerConfig->tooltipHelp=confDlg->ui.checkBoxToolTipHelp->isChecked();
		completerConfig->usePlaceholders=confDlg->ui.checkBoxUsePlaceholders->isChecked();
		QStringList newFiles;
		QListWidgetItem *elem;
		for (int i=0; i<confDlg->ui.completeListWidget->count(); i++) {
			elem=confDlg->ui.completeListWidget->item(i);
			if (elem->checkState()==Qt::Checked) newFiles.append(elem->text());
		}
		completerConfig->loadFiles(newFiles);
		
		//preview
		previewMode=(PreviewMode) confDlg->ui.comboBoxPreviewMode->currentIndex();
		buildManager->dvi2pngMode=(BuildManager::Dvi2PngMode) confDlg->ui.comboBoxDvi2PngMode->currentIndex();
		
		//build things
		for (BuildManager::LatexCommand cmd=BuildManager::CMD_LATEX; cmd <= BuildManager::CMD_GHOSTSCRIPT; ++cmd){
			if (!commandsToEdits.value(cmd)) continue;
			buildManager->setLatexCommand(cmd,commandsToEdits.value(cmd)->text());;
		}
		
		buildManager->setLatexCommand(BuildManager::CMD_USER_PRECOMPILE,confDlg->ui.lineEditExecuteBeforeCompiling->text());
		buildManager->setLatexCommand(BuildManager::CMD_USER_QUICK,confDlg->ui.lineEditUserquick->text());

		if (confDlg->ui.radioButton1->isChecked()) buildManager->quickmode=1;
		if (confDlg->ui.radioButton2->isChecked()) buildManager->quickmode=2;
		if (confDlg->ui.radioButton3->isChecked()) buildManager->quickmode=3;
		if (confDlg->ui.radioButton4->isChecked()) buildManager->quickmode=4;
		if (confDlg->ui.radioButton5->isChecked()) buildManager->quickmode=5;
		if (confDlg->ui.radioButton6->isChecked()) buildManager->quickmode=6;

		showLogAfterCompiling=confDlg->ui.checkBoxShowLog->isChecked();	
		
		//key replacements
		keyReplace.clear();
		keyReplaceBeforeWord.clear();
		keyReplaceAfterWord.clear();
		for (int i=1; i<keyReplacements->childCount()-1; i++) {
			keyReplace << keyReplacements->child(i)->text(1);
			keyReplaceBeforeWord << keyReplacements->child(i)->text(2);
			keyReplaceAfterWord << keyReplacements->child(i)->text(3);
		}
		confDlg->fmConfig->apply();

		
		//menus
		managedMenuNewShortcuts.clear();
		treeWidgetToManagedMenuTo(menuShortcuts);
				
		//appearance
		configShowAdvancedOptions=confDlg->ui.checkBoxShowAdvancedOptions->isChecked();
		//  interface
		if (confDlg->ui.comboBoxInterfaceFont->currentFont().family()!=interfaceFontFamily ||
			confDlg->ui.spinBoxInterfaceFontSize->value()!=interfaceFontSize) {
			interfaceFontSize=confDlg->ui.spinBoxInterfaceFontSize->value();
			interfaceFontFamily=confDlg->ui.comboBoxInterfaceFont->currentFont().family();
			QApplication::setFont(QFont(interfaceFontFamily, interfaceFontSize));
		}
		if (confDlg->ui.comboBoxInterfaceStyle->currentText()!=interfaceStyle || 
			confDlg->ui.comboBoxInterfaceModernStyle->currentIndex()!=(modernStyle?1:0)){
			interfaceStyle=confDlg->ui.comboBoxInterfaceStyle->currentText();
			modernStyle=confDlg->ui.comboBoxInterfaceModernStyle->currentIndex()==1;
			QString newStyle=interfaceStyle;
			if (interfaceStyle==tr("default")) {
				interfaceStyle="";
				newStyle=defaultStyleName;
			}
			QPalette pal = QApplication::palette();
			#if QT_VERSION >= 0x040500
			if (modernStyle) {
				ManhattanStyle* style=new ManhattanStyle(newStyle);
				if (style->isValid()) QApplication::setStyle(style);
			} else 
			#endif
				QApplication::setStyle(newStyle);
			QApplication::setPalette(pal);
		}
	
		// read checkbox and set logViewer accordingly
		if (tabbedLogView!=confDlg->ui.checkBoxTabbedLogView->isChecked()) 
			emit tabbedLogViewChanged(confDlg->ui.checkBoxTabbedLogView->isChecked());
		tabbedLogView=confDlg->ui.checkBoxTabbedLogView->isChecked();
		if (newLeftPanelLayout!=!confDlg->ui.checkBoxTabbedStructureView->isChecked())
			emit newLeftPanelLayoutChanged(!confDlg->ui.checkBoxTabbedStructureView->isChecked());
		newLeftPanelLayout=!confDlg->ui.checkBoxTabbedStructureView->isChecked();
		
		editorConfig->displayModifyTime=confDlg->ui.checkBoxDisplayModifyTime->isChecked();
		editorConfig->closeSearchAndReplace=confDlg->ui.checkBoxCloseSearchReplaceTogether->isChecked();
		editorConfig->useLineForSearch=confDlg->ui.checkBoxUseLineForSearch->isChecked();

		//language
		lastLanguage=language;
		language = confDlg->ui.comboBoxLanguage->currentText();
		if (language == tr("default")) language="";
		if (language!=lastLanguage) loadTranslations(language);
		
		//  editor font
		QString fam=confDlg->ui.comboBoxFont->lineEdit()->text();
		int si=confDlg->ui.spinBoxSize->value();
		editorConfig->editorFont=QFont (fam,si);		
		
	}
	return executed;
}


bool ConfigManager::addRecentFile(const QString & fileName, bool asMaster){ 
	int p=recentFilesList.indexOf(fileName);
	bool changed=p!=0;
	if (p>0) recentFilesList.removeAt(p);
	if (changed) recentFilesList.prepend(fileName);
	if (recentFilesList.count()>maxRecentFiles) recentFilesList.removeLast();

	if (asMaster) {
		p=recentProjectList.indexOf(fileName);
		changed|=p!=0;
		if (p>0) recentProjectList.removeAt(p);
		if (p!=0) recentProjectList.prepend(fileName);
		if (recentProjectList.count()>maxRecentProjects) recentProjectList.removeLast();
	}

	if (changed) updateRecentFiles();
	
	return changed;
}

void ConfigManager::updateRecentFiles(bool alwaysRecreateMenuItems) {
	QMenu* recentMenu = getManagedMenu("main/file/openrecent");
	if (alwaysRecreateMenuItems || (recentMenu->actions().count()!=maxRecentFiles+maxRecentProjects+1)) {
		QList<QAction*> actions=recentMenu->actions(); //recentMenu->clear() doesn't seem to delete the actions (why?)
		for (int i = 0; i< actions.count(); i++)
			recentMenu->removeAction(actions[i]); //neccessary or it crashes
		for (int i = 0; i < maxRecentProjects; ++i) {
			QAction* old=menuParent->findChild<QAction*>("main/file/openrecent/p"+QString::number(i));
			if (old!=NULL) recentMenu->addAction(old);
			else newManagedAction(recentMenu, "p"+QString::number(i), tr("Recent 'Master Document' %1").arg(i), SLOT(fileOpenRecentProject()))->setVisible(false);
		}
		recentMenu->addSeparator();
		for (int i = 0; i < maxRecentFiles; ++i){
			QAction* old=menuParent->findChild<QAction*>("main/file/openrecent/"+QString::number(i));
			if (old!=NULL) recentMenu->addAction(old);
			else newManagedAction(recentMenu, QString::number(i), tr("Recent File %1").arg(i), SLOT(fileOpenRecent()))->setVisible(false);		
		}
	}

	for (int i=0; i < maxRecentProjects; i++) {
		QAction* act = getManagedAction(QString("main/file/openrecent/p%1").arg(i));
		if (i<recentProjectList.count()) {
			act->setVisible(true);
			act->setText(tr("Master Document: ")+recentProjectList.at(i));
			act->setData(recentProjectList.at(i));
		} else act->setVisible(false);
	}
	for (int i=0; i < maxRecentFiles; i++) {
		QAction* act = getManagedAction(QString("main/file/openrecent/%1").arg(i));
		if (i<recentFilesList.count()) {
			act->setVisible(true);
			act->setText(recentFilesList.at(i));
			act->setData(recentFilesList.at(i));
		} else act->setVisible(false);
	}
}


QMenu* ConfigManager::newManagedMenu(const QString &id,const QString &text) {
	if (!menuParentsBar) qFatal("No menu parent bar!");
	if (!menuParent) qFatal("No menu parent!");
	//check if an old menu with this id exists and update it (for retranslation)
	QMenu *old=menuParent->findChild<QMenu*>(id);
	if (old) {
		old->setTitle(text);
		return old;
	}
	//create new
	QMenu* menu = new QMenu(qobject_cast<QWidget*>(menuParent));
	menuParentsBar->addMenu(menu);
	menu->setTitle(text);
	menu->setObjectName(id);
	managedMenus.append(menu);
	return menu;
}
QMenu* ConfigManager::newManagedMenu(QMenu* menu, const QString &id,const QString &text) {
	if (!menu) return newManagedMenu(id,text);
	QString completeId=menu->objectName()+"/"+ id;
	//check if an old menu with this id exists and update it (for retranslation)
	QMenu *old=menuParent->findChild<QMenu*>(completeId);
	if (old) {
		old->setTitle(text);
		return old;
	}
	//create new
	QMenu* submenu=menu->addMenu(text);
	submenu->setObjectName(completeId);
	return submenu;
}
QAction* ConfigManager::newManagedAction(QWidget* menu, const QString &id,const QString &text, const char* slotName, const QList<QKeySequence> &shortCuts, const QString & iconFile) {
	if (!menuParent) qFatal("No menu parent!");
	QString completeId=menu->objectName()+"/"+ id;

	QAction *old=menuParent->findChild<QAction*>(completeId);
	if (old) {
		old->setText(text);
		return old;
	}

	QAction *act;
	if (iconFile.isEmpty()) act=new QAction(text, menuParent);
	else act=new QAction(QIcon(iconFile), text, menuParent);
	act->setObjectName(completeId);
	act->setShortcuts(shortCuts);
	if (slotName) connect(act, SIGNAL(triggered()), menuParent, slotName);
	menu->addAction(act);
	for (int i=0; i<shortCuts.size(); i++)
		managedMenuShortcuts.insert(act->objectName()+QString::number(i),shortCuts[i]);
	return act;
}
QAction* ConfigManager::newManagedAction(QWidget* menu, const QString &id, QAction* act) {
	if (!menuParent) qFatal("No menu parent!");
	QString completeId = menu->objectName()+"/"+id;
	
	QAction *old=menuParent->findChild<QAction*>(completeId);
	if (old) 
		return old;


	act->setObjectName(completeId);
	menu->addAction(act);
	managedMenuShortcuts.insert(act->objectName()+"0",act->shortcut());
	return act;
}
QAction* ConfigManager::getManagedAction(QString id) {
	QAction* act=0;
	if (menuParent) act=menuParent->findChild<QAction*>(id);
	if (act==0) qWarning("Can't find internal menu %s",id.toAscii().data());
	return act;
}
QMenu* ConfigManager::getManagedMenu(QString id) {
	QMenu* menu=0;
	if (menuParent) menu=menuParent->findChild<QMenu*>(id);
	if (menu==0) qWarning("Can't find internal menu %s",id.toAscii().data());
	return menu;
}
void ConfigManager::removeManagedMenus(){
	/*foreach (QMenu* menu, managedMenus){
		menu->clear();
		delete menu;
	}
	menuParentsBar->clear();*/
}
void ConfigManager::triggerManagedAction(QString id){
	QAction* act = getManagedAction(id);
	if (act) act->trigger();
}
void ConfigManager::modifyManagedShortcuts(){
	//modify shortcuts
	for (int i=0; i< managedMenuNewShortcuts.size(); i++) {
		QString id=managedMenuNewShortcuts[i].first;
		int num=-1;
		if (managedMenuNewShortcuts[i].first.endsWith("~0")) num=0;
		else if (managedMenuNewShortcuts[i].first.endsWith("~1")) num=1;
		else { } //backward compatibility
		if (num!=-1) id.chop(2);
		QAction * act= getManagedAction(id);
		if (act) {
			if (num!=1) act->setShortcut(QKeySequence(managedMenuNewShortcuts[i].second));
			else act->setShortcuts((QList<QKeySequence>()<<act->shortcut())<<managedMenuNewShortcuts[i].second);
		}
	}
}
void ConfigManager::loadManagedMenu(QMenu* parent,const QDomElement &f) {
	QMenu *menu = newManagedMenu(parent,f.attributes().namedItem("id").nodeValue(),f.attributes().namedItem("text").nodeValue());
	QDomNodeList children = f.childNodes();
	for (int i = 0; i < children.count(); i++) {
		QDomElement c = children.at(i).toElement();
		if (c.nodeName()=="menu") loadManagedMenu(menu,c);
		else if (c.nodeName()=="insert" || c.nodeName()=="action") {
			QDomNamedNodeMap  att=c.attributes();
			QByteArray ba;
			const char* slotfunc;
			if (c.nodeName()=="insert") slotfunc=SLOT(InsertFromAction());
			else {
				ba=att.namedItem("slot").nodeValue().toLocal8Bit();
				slotfunc=ba.data();
			}
			QAction * act=newManagedAction(menu,
			                               att.namedItem("id").nodeValue(),
			                               att.namedItem("text").nodeValue(),slotfunc,
			                               QList<QKeySequence>()<<  QKeySequence(att.namedItem("shortcut").nodeValue()),
										   att.namedItem("icon").nodeValue());
			act->setWhatsThis(att.namedItem("info").nodeValue());
			act->setData(att.namedItem("insert").nodeValue());
		} else if (c.nodeName()=="separator") menu->addSeparator();
	}
}
void ConfigManager::loadManagedMenus(const QString &f) {
	QFile settings(f);

	if (settings.open(QFile::ReadOnly | QFile::Text)) {
		QDomDocument doc;
		doc.setContent(&settings);

		QDomNodeList f = doc.documentElement().childNodes();

		for (int i = 0; i < f.count(); i++)
			if (f.at(i).nodeName()=="menu")
				loadManagedMenu(0,f.at(i).toElement());
	}
}

void ConfigManager::managedMenuToTreeWidget(QTreeWidgetItem* parent, QMenu* menu) {
	if (!menu) return;
	QTreeWidgetItem* menuitem= new QTreeWidgetItem(parent, QStringList(menu->title().replace("&","")));
	if (menu->objectName().count("/")<=2) menuitem->setExpanded(true);
	QList<QAction *> acts=menu->actions();
	for (int i=0; i<acts.size(); i++)
		if (acts[i]->menu()) managedMenuToTreeWidget(menuitem, acts[i]->menu());
		else {
			QTreeWidgetItem* twi=new QTreeWidgetItem(menuitem, QStringList() << acts[i]->text()
					<< managedMenuShortcuts[acts[i]->objectName()+"0"]
					<< acts[i]->shortcut().toString(QKeySequence::NativeText));
			twi->setIcon(0,acts[i]->icon());
			if (!acts[i]->isSeparator()) twi->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
			twi->setData(0,Qt::UserRole,acts[i]->objectName());
			if (acts[i]->shortcuts().size()>1) twi->setText(3,acts[i]->shortcuts()[1].toString(QKeySequence::NativeText));
		}


}
void ConfigManager::treeWidgetToManagedMenuTo(QTreeWidgetItem* item) {
	if (item->childCount() > 0) {
		for (int i=0; i< item->childCount(); i++)
			treeWidgetToManagedMenuTo(item->child(i));
	} else {
		QString id=item->data(0,Qt::UserRole).toString();
		if (id=="") return;
		QAction * act=getManagedAction(id);
		if (act) {
			QString mseq=item->text(2);
			if(mseq==tr("<none>")) mseq="";
			if(mseq==tr("<default>")) mseq=managedMenuShortcuts[act->objectName()+"0"].toString(QKeySequence::NativeText);
			QKeySequence sc=QKeySequence(mseq);
			act->setShortcut(sc);
			if (sc!=managedMenuShortcuts.value(act->objectName()+"0",QKeySequence()))
				managedMenuNewShortcuts.append(QPair<QString, QString> (id+"~0", sc.toString(QKeySequence ::PortableText)));
			mseq=item->text(3);
			if(mseq==tr("<none>")) mseq="";
			if(mseq==tr("<default>")) mseq=managedMenuShortcuts.value(act->objectName()+"1",QKeySequence()).toString(QKeySequence::NativeText);
			sc=QKeySequence(mseq);
			if (mseq!="") act->setShortcuts((QList<QKeySequence>()<<act->shortcut()) << sc);
			if (sc!=managedMenuShortcuts.value(act->objectName()+"1",QKeySequence()))
				managedMenuNewShortcuts.append(QPair<QString, QString> (id+"~1", sc.toString(QKeySequence ::PortableText)));
			if(id=="main/view/outputview"){  // special handling for outputview because of "esc"-key
				if((item->text(2).toUpper()=="ESC"||item->text(2).isEmpty())) act->setShortcutContext(Qt::WidgetShortcut);
				else act->setShortcutContext(Qt::WindowShortcut);
			}
		}
	}

}
void ConfigManager::loadTranslations(QString locale){
	if (locale=="") {
		locale = QString(QLocale::system().name()).left(2);
		if (locale.length() < 2) locale = "en";
	}
	QString tmxTranslationFile=findResourceFile("texmakerx_"+locale+".qm");
	//if (tmxTranslationFile!="") {
		appTranslator->load(tmxTranslationFile);
		basicTranslator->load(findResourceFile("qt_"+locale+".qm"));
	//}
}

void ConfigManager::browseCommand(){
	QPushButton *pb = qobject_cast<QPushButton*> (sender());
	if (!buttonsToCommands.contains(pb)) return;
	BuildManager::LatexCommand cmd=buttonsToCommands.value(pb);
	QLineEdit* ed = commandsToEdits.value(cmd);
	if (!ed) return;
	QString path = ed->text();
	if (path.contains(' ')) path.truncate(path.indexOf(' '));
	if (!path.contains('/') && !path.contains('\\')) {//no absolute path
		path=BuildManager::findFileInPath(path);
		if (path=="") path=QDir::rootPath(); //command not found, where could it be?
	} else {
		//opendialog doesn't like quotation like "C:\program files\..."
		if (path.startsWith('"')) path=path.remove(0,1);
		if (path.endsWith('"')) path.chop(1);
	}
	QString location=QFileDialog::getOpenFileName(0,tr("Browse program"),path,"Program (*)",0,QFileDialog::DontResolveSymlinks);
	if (!location.isEmpty()) {
		location.replace(QString("\\"),QString("/"));
		location="\""+location+"\" "+BuildManager::defaultCommandOptions(cmd);
		ed->setText(location);
	}
}
void ConfigManager::undoCommand(){
	QPushButton *pb = qobject_cast<QPushButton*> (sender());
	if (!buttonsToCommands.contains(pb)) return;
	BuildManager::LatexCommand cmd=buttonsToCommands.value(pb);
	QLineEdit* ed = commandsToEdits.value(cmd);
	if (!ed) return;
	ed->setText(BuildManager::guessCommandName(cmd));
}

