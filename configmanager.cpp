
#include "configmanager.h"

#include "configdialog.h"
#include "latexeditorview.h"
#include "latexcompleter_config.h"
#include "latexeditorview_config.h"
#include "webpublishdialog_config.h"
#include "insertgraphics_config.h"
#include "grammarcheck_config.h"
#include "PDFDocument_config.h"
#include "smallUsefulFunctions.h"
#include "codesnippet.h"
#include "updatechecker.h"

#include <QDomElement>

#include "qformatconfig.h"

#include "manhattanstyle.h"

const QString TXS_AUTO_REPLACE_QUOTE_OPEN = "TMX:Replace Quote Open";
const QString TXS_AUTO_REPLACE_QUOTE_CLOSE = "TMX:Replace Quote Close";

const char* PROPERTY_COMMAND_ID = "cmdID";
const char* PROPERTY_NAME_WIDGET = "nameWidget";
const char* PROPERTY_WIDGET_TYPE = "widgetType";
const char* PROPERTY_ASSOCIATED_INPUT = "associatedInput";
const char* PROPERTY_ADD_BUTTON = "addButton";
Q_DECLARE_METATYPE(QPushButton*)


ManagedProperty::ManagedProperty():storage(0),type(PT_VOID),widgetOffset(0){
}

#define CONSTRUCTOR(TYPE, ID) \
	ManagedProperty::ManagedProperty(TYPE* storage, QVariant def, ptrdiff_t widgetOffset)\
	: storage(storage), type(ID), def(def), widgetOffset(widgetOffset){} \
	ManagedProperty ManagedProperty::fromValue(TYPE value) { \
		ManagedProperty res;    \
		res.storage = new TYPE; \
		*((TYPE*)(res.storage)) = value; \
		res.type = ID;          \
		res.def = value;        \
		res.widgetOffset = 0;   \
		return res;             \
	}                              
PROPERTY_TYPE_FOREACH_MACRO(CONSTRUCTOR)
#undef CONSTRUCTOR


void ManagedProperty::deallocate(){ 
	switch (type) {
#define CASE(TYPE, ID) case ID: delete ((TYPE*)(storage)); break;
PROPERTY_TYPE_FOREACH_MACRO(CASE)
#undef CASE
	default: Q_ASSERT(false);
	}
	storage = 0;
}


static ConfigManager* globalConfigManager = 0;
ConfigManagerInterface* ConfigManagerInterface::getInstance(){
	Q_ASSERT(globalConfigManager);
	return globalConfigManager;
}


Q_DECLARE_METATYPE(ManagedProperty*);

ManagedToolBar::ManagedToolBar(const QString &newName, const QStringList &defs): name(newName), defaults(defs), toolbar(0){}

QVariant ManagedProperty::valueToQVariant() const{
	Q_ASSERT(storage);
	if (!storage) return QVariant();
	switch (type){
#define CONVERT(TYPE, ID) case ID: return *((TYPE*)storage);
PROPERTY_TYPE_FOREACH_MACRO(CONVERT)
#undef CONVERT
	default:
		Q_ASSERT(false);
		return QVariant();
	}
}
void ManagedProperty::valueFromQVariant(const QVariant v){
	Q_ASSERT(storage);
	if (!storage) return;
	switch (type){
	case PT_VARIANT: *((QVariant*)storage) = v; break;
	case PT_INT: *((int*)storage) = v.toInt(); break;
	case PT_BOOL: *((bool*)storage) = v.toBool(); break;
	case PT_STRING: *((QString*)storage) = v.toString(); break;
	case PT_STRINGLIST: *((QStringList*)storage) = v.toStringList(); break;
	case PT_DATETIME: *((QDateTime*)storage) = v.toDateTime(); break;
	case PT_FLOAT: *((float*)storage) = v.toFloat(); break;
	case PT_DOUBLE: *((double*)storage) = v.toDouble(); break;
	case PT_BYTEARRAY: *((QByteArray*)storage) = v.toByteArray(); break;
	case PT_LIST: *((QList<QVariant>*)storage) = v.toList(); break;
	default:
		Q_ASSERT(false);
	}
}

void ManagedProperty::writeToObject(QObject* w) const{
	Q_ASSERT(storage && w);
	if (!storage || !w) return;
	
	QCheckBox* checkBox = qobject_cast<QCheckBox*>(w);
	if (checkBox) {
		Q_ASSERT(type == PT_BOOL);
		checkBox->setChecked(*((bool*)storage));
		return;
	}
	QLineEdit* edit = qobject_cast<QLineEdit*>(w);
	if (edit){
		Q_ASSERT(type == PT_STRING);
		edit->setText(*((QString*)storage));
		return;
	}
	/*QTextEdit* tedit = qobject_cast<QTextEdit*>(w);
 if (tedit){
  *((QString*)storage) = tedit->toPlainText();
  continue;
 }*/
	QSpinBox* spinBox = qobject_cast<QSpinBox*>(w);
	if (spinBox){
		Q_ASSERT(type == PT_INT);
		spinBox->setValue(*((int*)storage));
		return;
	}
	QComboBox* comboBox = qobject_cast<QComboBox*>(w);
	if (comboBox){
		switch (type) {
		case PT_BOOL:
			comboBox->setCurrentIndex(*((bool*)storage)?1:0);
			return;
		case PT_INT:
			comboBox->setCurrentIndex(*((int*)storage));
			return;
		case PT_STRING:{
			int index = comboBox->findText(*(QString*)storage);
			if (index > 0) comboBox->setCurrentIndex(index);
			if (comboBox->isEditable()) comboBox->setEditText(*(QString*)storage);
			return;
		}
		case PT_STRINGLIST:{
			QStringList& sl = *(QStringList*)storage;
		
			int cp=comboBox->lineEdit() ? comboBox->lineEdit()->cursorPosition() : -1000; 
			while (comboBox->count() > sl.size()) 
				comboBox->removeItem(comboBox->count()-1);
			for (int i=0;i<qMin(sl.size(),comboBox->count());i++)
				if (comboBox->itemText(i) != sl[i]) 
					comboBox->setItemText(i,sl[i]);
			for (int i=comboBox->count();i<sl.size();i++)
				comboBox->addItem(sl[i]);
			if (cp != -1000) {
				//combobox visible (i.e. as used in search panel)
				if (!sl.isEmpty() && comboBox->currentText()!=sl.last() && comboBox->currentIndex()!=sl.size()-1)
					comboBox->setCurrentIndex(sl.size()-1);
				comboBox->lineEdit()->setCursorPosition(cp); 
			} // else:  combobox invisible (i.e. as used in universal input dialog)
			return;
		}
		default:
			Q_ASSERT(false);
		}
	}
	QDoubleSpinBox* doubleSpinBox = qobject_cast<QDoubleSpinBox*>(w);
	if (doubleSpinBox){
		switch (type) {
		case PT_DOUBLE: doubleSpinBox->setValue(*((double*)storage)); break;
		case PT_FLOAT: doubleSpinBox->setValue(*((float*)storage)); break;
		default:
			Q_ASSERT(false);
		}
		return;
	}
	QAction* action = qobject_cast<QAction*>(w);
	if (action) {
		Q_ASSERT(type == PT_BOOL);
		action->setChecked(*((bool*)storage));
		return;
	}
	QTextEdit* textEdit = qobject_cast<QTextEdit*>(w);
	if (textEdit) {
		switch (type) {
		case PT_STRING: textEdit->setPlainText(*((QString*)storage)); break;
		case PT_STRINGLIST: textEdit->setPlainText(((QStringList*)storage)->join("\n")); break;
		default:
			Q_ASSERT(false);
		}
		return;
	}
	Q_ASSERT(false);
}
bool ManagedProperty::readFromObject(const QObject* w){
#define READ_FROM_OBJECT(TYPE, VALUE) {           \
	TYPE oldvalue = *((TYPE*)storage);         \
	*((TYPE*)storage) = VALUE;                 \
	return oldvalue != *((TYPE*)storage);      \
}	
	Q_ASSERT(storage);
	if (!storage) return false;
	const QCheckBox* checkBox = qobject_cast<const QCheckBox*>(w);
	if (checkBox) {
		Q_ASSERT(type == PT_BOOL);
		READ_FROM_OBJECT(bool, checkBox->isChecked())
	}
	const QLineEdit* edit = qobject_cast<const QLineEdit*>(w);
	if (edit){
		Q_ASSERT(type == PT_STRING);
		READ_FROM_OBJECT(QString, edit->text())
	}
	/*QTextEdit* tedit = qobject_cast<QTextEdit*>(w);
 if (tedit){
  *((QString*)storage) = tedit->toPlainText();
  continue;
 }*/
	const QSpinBox* spinBox = qobject_cast<const QSpinBox*>(w);
	if (spinBox){
		Q_ASSERT(type == PT_INT);
		READ_FROM_OBJECT(int, spinBox->value())
	}
	const QComboBox* comboBox = qobject_cast<const QComboBox*>(w);
	if (comboBox){
		switch (type) {
		case PT_BOOL:   READ_FROM_OBJECT(bool, comboBox->currentIndex()!=0)
		case PT_INT:    READ_FROM_OBJECT(int, comboBox->currentIndex())
		case PT_STRING: READ_FROM_OBJECT(QString, comboBox->currentText())
		case PT_STRINGLIST:{
			QString oldvalue;
			if (!((QStringList*)storage)->isEmpty())
				oldvalue = ((QStringList*)storage)->first();
			*((QStringList*)storage) = QStringList(comboBox->currentText());
			return oldvalue != comboBox->currentText();
		}
		default:
			Q_ASSERT(false);
		}
	}
	const QDoubleSpinBox* doubleSpinBox = qobject_cast<const QDoubleSpinBox*>(w);
	if (doubleSpinBox){
		switch (type) {
		case PT_DOUBLE: READ_FROM_OBJECT(double, doubleSpinBox->value())
		case PT_FLOAT: READ_FROM_OBJECT(float, doubleSpinBox->value())
		default:
			Q_ASSERT(false);
		}
	}
	const QAction* action = qobject_cast<const QAction*>(w);
	if (action){
		Q_ASSERT(type == PT_BOOL);
		Q_ASSERT(action->isCheckable());
		READ_FROM_OBJECT(bool, action->isChecked());
	}
	
	const QTextEdit* textEdit = qobject_cast<const QTextEdit*>(w);
	if (textEdit){
		switch (type) {
		case PT_STRING:     READ_FROM_OBJECT(QString, textEdit->toPlainText())
		case PT_STRINGLIST: READ_FROM_OBJECT(QStringList, textEdit->toPlainText().split("\n"))
		default:
			Q_ASSERT(false);
		}
	}
	Q_ASSERT(false);
	return false;
}
#undef READ_FROM_OBJECT

QTextCodec* ConfigManager::newFileEncoding = 0;

QString getText(QWidget* w){
	if (qobject_cast<QLineEdit*>(w)) return qobject_cast<QLineEdit*>(w)->text();
	else if (qobject_cast<QComboBox*>(w)) return qobject_cast<QComboBox*>(w)->currentText();
	else REQUIRE_RET(false, "");
}
void setText(QWidget* w, const QString& t){
	if (qobject_cast<QLineEdit*>(w)) qobject_cast<QLineEdit*>(w)->setText(t);
	else if (qobject_cast<QComboBox*>(w)) qobject_cast<QComboBox*>(w)->setEditText(t);
	else REQUIRE(false);
}

void assignNameWidget(QWidget *w, QWidget *nameWidget) {
	w->setProperty(PROPERTY_NAME_WIDGET, QVariant::fromValue<QWidget*>(nameWidget));
	QString cmdID = nameWidget->property(PROPERTY_COMMAND_ID).toString();
	if (!cmdID.isEmpty()) {
		// user commands don't store the ID in the property, because it's editable
		// In builtin commmands, the ID is fixed, so we can directly assign it to the widget.
		// this speeds up lookup in getCmdID
		w->setProperty(PROPERTY_COMMAND_ID, cmdID);
	}
}
QString getCmdID(QWidget *w) {
	QString cmdID = w->property(PROPERTY_COMMAND_ID).toString();
	if (!cmdID.isEmpty()) return cmdID;

	QWidget *nameWidget = w->property(PROPERTY_NAME_WIDGET).value<QWidget*>();
	if (!nameWidget) nameWidget = w;
	cmdID = nameWidget->property(PROPERTY_COMMAND_ID).toString();
	if (!cmdID.isEmpty()) return cmdID;

	// user commands don't store the ID in the property, because it's editable
	QLineEdit *le = qobject_cast<QLineEdit *>(nameWidget);
	REQUIRE_RET(le, "");
	QString combinedName = le->text();
	int pos = combinedName.indexOf(":");
	cmdID = (pos == -1)?combinedName:combinedName.left(pos);
	return cmdID;
}

ConfigManager::ConfigManager(QObject *parent): QObject (parent),
       buildManager(0),editorConfig(new LatexEditorViewConfig),
       completerConfig (new LatexCompleterConfig),
       webPublishDialogConfig (new WebPublishDialogConfig),
       pdfDocumentConfig(new PDFDocumentConfig),
       insertGraphicsConfig(new InsertGraphicsConfig),
       grammarCheckerConfig(new GrammarCheckerConfig),
	   menuParent(0), menuParentsBar(0), modifyMenuContentsFirstCall(true), persistentConfig(0) {
	
	Q_ASSERT(!globalConfigManager);
	globalConfigManager = this;
	
	managedToolBars.append(ManagedToolBar("Custom", QStringList()));
	managedToolBars.append(ManagedToolBar("File", QStringList() << "main/file/new" << "main/file/open" << "main/file/save" << "main/file/close"));
	managedToolBars.append(ManagedToolBar("Edit", QStringList() << "main/edit/undo" << "main/edit/redo" << "main/edit/copy" << "main/edit/cut" << "main/edit/paste"));
    //managedToolBars.append(ManagedToolBar("Tools", QStringList() << "main/tools/commands/viewlog" << "main/edit2/goto/errorprev" << "main/edit2/goto/errornext"	<< "separator"
    //                                      << "main/tools/commands/quickbuild" << "main/tools/commands/latex" << "main/tools/commands/viewdvi" << "main/tools/commands/dvi2ps" << "main/tools/commands/viewps" << "main/tools/commands/pdflatex" << "main/tools/commands/viewpdf"));
    managedToolBars.append(ManagedToolBar("Tools", QStringList() << "main/tools/viewlog" << "main/edit2/goto/errorprev" << "main/edit2/goto/errornext"	<< "separator"
                                             << "main/tools/quickbuild" << "main/tools/compile" << "main/tools/view" ));
	//managedToolBars.append(ManagedToolBar("Math", QStringList() << "main/math/mathmode" << "main/math/subscript" << "main/math/superscript" << "main/math/frac" << "main/math/dfrac" << "main/math/sqrt" << "separator"
	//                                      << "tags/brackets/left" << "separator" << "tags/brackets/right"));
	managedToolBars.append(ManagedToolBar("Math", QStringList() << "tags/brackets/left" << "separator" << "tags/brackets/right"));
	//managedToolBars.append(ManagedToolBar("Format", QStringList() << "main/latex/sectioning" << "separator" << "main/latex/references" <<"separator" <<
	//                                      "main/latex/fontsizes" << "separator" <<
	//                                      "main/latex/fontstyles/textbf" << "main/latex/fontstyles/textit" << "main/latex/fontstyles/underline" << "main/latex/environment/flushleft" << "main/latex/environment/center" << "main/latex/environment/flushright"
	//                                      << "separator" << "main/latex/spacing/newline"));
	managedToolBars.append(ManagedToolBar("Format", QStringList() << "main/latex/sectioning" << "separator" << "main/latex/references" <<"separator" << "main/latex/fontsizes"));
	managedToolBars.append(ManagedToolBar("Table", QStringList() << "main/latex/tabularmanipulation/addRow" << "main/latex/tabularmanipulation/addColumn" << "main/latex/tabularmanipulation/pasteColumn" << "main/latex/tabularmanipulation/removeRow" << "main/latex/tabularmanipulation/removeColumn" << "main/latex/tabularmanipulation/cutColumn" << "main/latex/tabularmanipulation/alignColumns"));
	managedToolBars.append(ManagedToolBar("Diff", QStringList() << "main/file/svn/prevdiff" << "main/file/svn/nextdiff"  ));
	managedToolBars.append(ManagedToolBar("Central", QStringList() << "main/edit/goto/goback" << "main/edit/goto/goforward" << "separator" << "main/latex/fontstyles/textbf" << "main/latex/fontstyles/textit" << "main/latex/fontstyles/underline" << "main/latex/environment/flushleft" << "main/latex/environment/center" << "main/latex/environment/flushright" << "separator" <<
	                                      "main/latex/spacing/newline" << "separator" <<
	                                      "main/math/mathmode" << "main/math/subscript" << "main/math/superscript" << "main/math/frac" << "main/math/dfrac" << "main/math/sqrt"));
	
	registerOption("ToolBar/CentralVisible", &centralVisible, true);
	registerOption("StructureView/ShowLinenumbers", &showLineNumbersInStructure, false);
	registerOption("StructureView/Indentation", &indentationInStructure, -1);
	
	enviromentModes << "verbatim" << "numbers";
	
	
	
	Ui::ConfigDialog *pseudoDialog = (Ui::ConfigDialog*) 0;

	//beginRegisterGroup("texmaker");
	//files
	registerOption("Files/New File Encoding", &newFileEncodingName, "utf-8", &pseudoDialog->comboBoxEncoding); //check
	registerOption("Files/Auto Detect Encoding Of Loaded Files", &autodetectLoadedFile, true, &pseudoDialog->checkBoxAutoDetectOnLoad);
	registerOption("Common Encodings", &commonEncodings, QStringList() << "UTF-8" << "ISO-8859-1" << "windows-1252" << "Apple Roman");
	//recent files
	registerOption("Files/Max Recent Files", &maxRecentFiles, 5, &pseudoDialog->spinBoxMaxRecentFiles);
	registerOption("Files/Max Recent Projects", &maxRecentProjects, 3, &pseudoDialog->spinBoxMaxRecentProjects);
	registerOption("Files/Recent Files", &recentFilesList);
	registerOption("Files/Recent Project Files", &recentProjectList);
	registerOption("Files/RestoreSession", &sessionRestore);
	registerOption("Files/Last Document", &lastDocument);
	registerOption("Files/Parse BibTeX", &parseBibTeX, true, &pseudoDialog->checkBoxParseBibTeX);
	registerOption("Files/Parse Master", &parseMaster, true, &pseudoDialog->checkBoxParseMaster);
	registerOption("Files/Autosave", &autosaveEveryMinutes, 0);
    registerOption("Files/Autoload",&autoLoadChildren,false, &pseudoDialog->checkBoxAutoLoad);
	
	registerOption("Spell/DictionaryDir", &spellDictDir, "", &pseudoDialog->leDictDir); //don't translate it
	registerOption("Spell/Language", &spellLanguage, "<none>", &pseudoDialog->comboBoxSpellcheckLang);
	registerOption("Spell/Dic", &spell_dic, "<dic not found>", 0);
	registerOption("Thesaurus/Database", &thesaurus_database, "<dic not found>", &pseudoDialog->comboBoxThesaurusFileName);
	
	//updates
	registerOption("Update/AutoCheck", &autoUpdateCheck, true, &pseudoDialog->checkBoxAutoUpdateCheck);
	registerOption("Update/AutoCheckInvervalDays", &autoUpdateCheckIntervalDays, 7, &pseudoDialog->spinBoxAutoUpdateCheckIntervalDays);
	registerOption("Update/LastCheck", &lastUpdateCheck, QDateTime());

	//editor
	registerOption("Editor/WordWrapMode", &editorConfig->wordwrap, 1, &pseudoDialog->comboBoxLineWrap);
	registerOption("Editor/WrapLineWidth", &editorConfig->lineWidth, 80, &pseudoDialog->spinBoxWrapLineWidth);
	registerOption("Editor/Parentheses Matching", &editorConfig->parenmatch, true); //TODO: checkbox?
	registerOption("Editor/Parentheses Completion", &editorConfig->parenComplete, true, &pseudoDialog->checkBoxAutoCompleteParens);
	registerOption("Editor/Line Number Multiples", &editorConfig->showlinemultiples, -1);
	registerOption("Editor/Cursor Surrounding Lines", &editorConfig->cursorSurroundLines, 5);
	registerOption("Editor/BoldCursor", &editorConfig->boldCursor, true, &pseudoDialog->checkBoxBoldCursor);
	registerOption("Editor/Auto Indent", &editorConfig->autoindent, true);
	registerOption("Editor/Weak Indent", &editorConfig->weakindent, true);
	registerOption("Editor/Indent with Spaces", &editorConfig->indentWithSpaces, false);
	registerOption("Editor/Folding", &editorConfig->folding, true, &pseudoDialog->checkBoxFolding);
	registerOption("Editor/Show Line State", &editorConfig->showlinestate, true, &pseudoDialog->checkBoxLineState);
	registerOption("Editor/Show Cursor State", &editorConfig->showcursorstate, true, &pseudoDialog->checkBoxState);
	registerOption("Editor/Real-Time Spellchecking", &editorConfig->realtimeChecking, true, &pseudoDialog->checkBoxRealTimeCheck); //named for compatibility reasons with older txs versions
	registerOption("Editor/Check Spelling", &editorConfig->inlineSpellChecking, true, &pseudoDialog->checkBoxInlineSpellCheck);
	registerOption("Editor/Check Citations", &editorConfig->inlineCitationChecking, true, &pseudoDialog->checkBoxInlineCitationCheck);
	registerOption("Editor/Check References", &editorConfig->inlineReferenceChecking, true, &pseudoDialog->checkBoxInlineReferenceCheck);
	registerOption("Editor/Check Syntax", &editorConfig->inlineSyntaxChecking, true, &pseudoDialog->checkBoxInlineSyntaxCheck);
	registerOption("Editor/Check Grammar", &editorConfig->inlineGrammarChecking, true, &pseudoDialog->checkBoxInlineGrammarCheck);
	registerOption("Editor/Show Whitespace", &editorConfig->showWhitespace, false, &pseudoDialog->checkBoxShowWhitespace);
	registerOption("Editor/TabStop", &editorConfig->tabStop, 4 , &pseudoDialog->sbTabSpace);
	registerOption("Editor/ToolTip Help", &editorConfig->toolTipHelp, true , &pseudoDialog->checkBoxToolTipHelp2);
	registerOption("Editor/ToolTip Preview", &editorConfig->toolTipPreview, true , &pseudoDialog->checkBoxToolTipPreview);
	
	registerOption("Editor/Replace Quotes", &replaceQuotes, 0 , &pseudoDialog->comboBoxReplaceQuotes);
	
	registerOption("Editor/Close Search Replace Together", &editorConfig->closeSearchAndReplace, false, &pseudoDialog->checkBoxCloseSearchReplaceTogether);
	registerOption("Editor/Use Line For Search", &editorConfig->useLineForSearch, true, &pseudoDialog->checkBoxUseLineForSearch);	
	registerOption("Editor/Search Only In Selection", &editorConfig->searchOnlyInSelection, true, &pseudoDialog->checkBoxSearchOnlyInSelection);	
	registerOption("Editor/Auto Replace Commands", &CodeSnippet::autoReplaceCommands, true, &pseudoDialog->checkBoxAutoReplaceCommands);
	
	registerOption("Editor/Font Family", &editorConfig->fontFamily, "", &pseudoDialog->comboBoxFont);
	registerOption("Editor/Font Size", &editorConfig->fontSize, -1, &pseudoDialog->spinBoxSize);
    registerOption("Editor/Line Spacing Percent", &editorConfig->lineSpacingPercent, 100, &pseudoDialog->spinBoxLineSpacingPercent);
	registerOption("Editor/Esc for closing log", &useEscForClosingLog, false, &pseudoDialog->cb_CloseLogByEsc);
	
	registerOption("Editor/Mouse Wheel Zoom", &editorConfig->mouseWheelZoom, true, &pseudoDialog->checkBoxMouseWheelZoom);
	
	registerOption("Editor/Hack Auto Choose", &editorConfig->hackAutoChoose, true, &pseudoDialog->checkBoxHackAutoRendering);
	registerOption("Editor/Hack Disable Fixed Pitch", &editorConfig->hackDisableFixedPitch, false, &pseudoDialog->checkBoxHackDisableFixedPitch);
	registerOption("Editor/Hack Disable Width Cache", &editorConfig->hackDisableWidthCache, false, &pseudoDialog->checkBoxHackDisableWidthCache);
	registerOption("Editor/Hack Disable Line Cache", &editorConfig->hackDisableLineCache, false, &pseudoDialog->checkBoxHackDisableLineCache);
	registerOption("Editor/Hack Disable Accent Workaround", &editorConfig->hackDisableAccentWorkaround, false, &pseudoDialog->checkBoxHackDisableAccentWorkaround);
	registerOption("Editor/Hack Render Mode", &editorConfig->hackRenderingMode, 0, &pseudoDialog->comboBoxHackRenderMode);
	
	//completion
	registerOption("Editor/Completion", &completerConfig->enabled, true, &pseudoDialog->checkBoxCompletion);
	Q_ASSERT(sizeof(int)==sizeof(LatexCompleterConfig::CaseSensitive));
	registerOption("Editor/Completion Case Sensitive", (int*)&completerConfig->caseSensitive, 2);
	registerOption("Editor/Completion Complete Common Prefix", &completerConfig->completeCommonPrefix, true, &pseudoDialog->checkBoxCompletePrefix);
	registerOption("Editor/Completion EOW Completes", &completerConfig->eowCompletes, true, &pseudoDialog->checkBoxEOWCompletes);
	registerOption("Editor/Completion Enable Tooltip Help", &completerConfig->tooltipHelp, true, &pseudoDialog->checkBoxToolTipHelp);
	registerOption("Editor/Completion Use Placeholders", &completerConfig->usePlaceholders, true, &pseudoDialog->checkBoxUsePlaceholders);
	registerOption("Editor/Completion Prefered Tab", (int*)&completerConfig->preferedCompletionTab, 0,&pseudoDialog->comboBoxPreferedTab);
	registerOption("Editor/Completion Tab Relative Font Size Percent", &completerConfig->tabRelFontSizePercent, 100,&pseudoDialog->spinBoxTabRelFontSize);
	
	//table autoformating
	registerOption("TableAutoformat/Special Commands", &tableAutoFormatSpecialCommands, "\\hline,\\cline,\\intertext,\\shortintertext,\\toprule,\\midrule,\\bottomrule", &pseudoDialog->leTableFormatingSpecialCommands);
	registerOption("TableAutoformat/Special Command Position", &tableAutoFormatSpecialCommandPos, 0, &pseudoDialog->cbTableFormatingSpecialCommandPos);
	registerOption("TableAutoformat/One Line Per Cell", &tableAutoFormatOneLinePerCell, false, &pseudoDialog->cbTableFormatingOneLinePerCell);

	//grammar
	registerOption("Grammar/Long Repetition Check", &grammarCheckerConfig->longRangeRepetitionCheck, true, &pseudoDialog->checkBoxGrammarRepetitionCheck);
	registerOption("Grammar/Bad Word Check", &grammarCheckerConfig->badWordCheck, true, &pseudoDialog->checkBoxGrammarBadWordCheck);
	registerOption("Grammar/Long Repetition Check Distance", &grammarCheckerConfig->maxRepetitionDelta, 3, &pseudoDialog->spinBoxGrammarRepetitionDistance);
	registerOption("Grammar/Very Long Repetition Check Distance", &grammarCheckerConfig->maxRepetitionLongRangeDelta, 10, &pseudoDialog->spinBoxGrammarLongRangeRepetition);
	registerOption("Grammar/Very Long Repetition Check Min Length", &grammarCheckerConfig->maxRepetitionLongRangeMinWordLength, 6, &pseudoDialog->spinBoxGrammarLongRangeRepetitionMinLength);
	registerOption("Grammar/Word Lists Dir", &grammarCheckerConfig->wordlistsDir, "", &pseudoDialog->lineEditGrammarWordlists);
	registerOption("Grammar/Language Tool URL", &grammarCheckerConfig->languageToolURL, "http://localhost:8081/",&pseudoDialog->lineEditGrammarLTUrl);
	registerOption("Grammar/Language Tool Path", &grammarCheckerConfig->languageToolPath, "", &pseudoDialog->lineEditGrammarLTPath);
	registerOption("Grammar/Language Tool Java Path", &grammarCheckerConfig->languageToolJavaPath, "java", &pseudoDialog->lineEditGrammarLTJava);
	registerOption("Grammar/Language Tool Autorun", &grammarCheckerConfig->languageToolAutorun, true, &pseudoDialog->checkBoxGrammarLTAutorun);
	registerOption("Grammar/Language Tool Ignored Rules", &grammarCheckerConfig->languageToolIgnoredRules, "", &pseudoDialog->lineEditGrammarLTIgnoredRules);
#define TEMP(ID) registerOption("Grammar/Special Rules" #ID, &grammarCheckerConfig->specialIds##ID, "", &pseudoDialog->lineEditGrammarSpecialRules##ID)
	TEMP(1); TEMP(2); TEMP(3); TEMP(4); 
#undef TEMP

	//other dialogs
	registerOption("Dialogs/Last Hard Wrap Column", &lastHardWrapColumn, 80);
	registerOption("Dialogs/Last Hard Wrap Smart Scope Selection", &lastHardWrapSmartScopeSelection, false);
	registerOption("Dialogs/Last Hard Wrap Join Lines", &lastHardWrapJoinLines, false);
	
	//build commands
	registerOption("Tools/SingleViewerInstance", &singleViewerInstance, false, &pseudoDialog->checkBoxSingleInstanceViewer);
	registerOption("Tools/Show Log After Compiling", &showLogAfterCompiling, true, &pseudoDialog->checkBoxShowLog);
	registerOption("Tools/Show Stdout", &showStdoutOption, 1, &pseudoDialog->comboBoxShowStdout);
	registerOption("Tools/Automatic Rerun Times", &BuildManager::autoRerunLatex, 5, &pseudoDialog->spinBoxRerunLatex);

	//Paths
#ifdef Q_WS_MACX
	QString defaultSearchPaths = "/usr/local/texlive/2012/bin/x86_64-darwin"; //workaround for xelatex
#else
	QString defaultSearchPaths;
#endif
	registerOption("Tools/Search Paths", &BuildManager::additionalSearchPaths, defaultSearchPaths, &pseudoDialog->lineEditPathCommands);
	registerOption("Tools/Log Paths", &BuildManager::additionalLogPaths, "", &pseudoDialog->lineEditPathLog);
	registerOption("Tools/PDF Paths", &BuildManager::additionalPdfPaths, "", &pseudoDialog->lineEditPathPDF);
	
	//SVN
	registerOption("Tools/Auto Checkin after Save", &autoCheckinAfterSave, false, &pseudoDialog->cbAutoCheckin);
	registerOption("Tools/SVN Undo", &svnUndo, false, &pseudoDialog->cbSVNUndo);
	registerOption("Tools/SVN KeywordSubstitution", &svnKeywordSubstitution, false, &pseudoDialog->cbKeywordSubstitution);
	registerOption("Tools/SVN Search Path Depth", &svnSearchPathDepth, 2, &pseudoDialog->sbDirSearchDepth);
	
	//interfaces
	registerOption("GUI/Style", &modernStyle, false, &pseudoDialog->comboBoxInterfaceModernStyle);
#ifdef Q_WS_X11
	interfaceFontFamily = "<later>";
	interfaceStyle="<later>";
#else
	interfaceFontFamily=QApplication::font().family();
	interfaceStyle="";
#endif
	registerOption("GUI/Texmaker Palette", &useTexmakerPalette, false, &pseudoDialog->checkBoxUseTexmakerPalette);
	registerOption("GUI/Use System Theme", &useSystemTheme, true, &pseudoDialog->checkBoxUseSystemTheme);
	registerOption("X11/Font Family", &interfaceFontFamily, interfaceFontFamily, &pseudoDialog->comboBoxInterfaceFont); //named X11 for backward compatibility
	registerOption("X11/Font Size", &interfaceFontSize, QApplication::font().pointSize(), &pseudoDialog->spinBoxInterfaceFontSize);
	registerOption("X11/Style", &interfaceStyle, interfaceStyle, &pseudoDialog->comboBoxInterfaceStyle);
	
	registerOption("Interface/Config Show Advanced Options", &configShowAdvancedOptions, false, &pseudoDialog->checkBoxShowAdvancedOptions);
	registerOption("Interface/Config Riddled", &configRiddled, false);	
	registerOption("LogView/Tabbed", &tabbedLogView, true, &pseudoDialog->checkBoxTabbedLogView);
	registerOption("Interface/New Left Panel Layout", &newLeftPanelLayout, true);
	registerOption("Interface/MRU Document Chooser", &mruDocumentChooser, false, &pseudoDialog->checkBoxMRUDocumentChooser);

	//language
	registerOption("Interface/Language", &language, "", &pseudoDialog->comboBoxLanguage);
	
	//preview
	registerOption("Preview/Mode", (int*)&previewMode, (int)PM_INLINE, &pseudoDialog->comboBoxPreviewMode);
	registerOption("Preview/Auto Preview", (int*)&autoPreview, 1, &pseudoDialog->comboBoxAutoPreview);
	registerOption("Preview/Auto Preview Delay", &autoPreviewDelay, 300, &pseudoDialog->spinBoxAutoPreviewDelay);
	
	//pdf preview
	QRect screen = QApplication::desktop()->availableGeometry();
	registerOption("Geometries/PdfViewerLeft", &pdfDocumentConfig->windowLeft, screen.left()+screen.width()*2/3);
	registerOption("Geometries/PdfViewerTop", &pdfDocumentConfig->windowTop, screen.top()+10);
	registerOption("Geometries/PdfViewerWidth", &pdfDocumentConfig->windowWidth, screen.width()/3-26);
	registerOption("Geometries/PdfViewerHeight", &pdfDocumentConfig->windowHeight, screen.height()-100);
	registerOption("Geometries/PdfViewerMaximized", &pdfDocumentConfig->windowMaximized, false);
	registerOption("Geometries/PdfViewerState", &pdfDocumentConfig->windowState, QByteArray());
	
	registerOption("Preview/CacheSize", &pdfDocumentConfig->cacheSizeMB, 512, &pseudoDialog->spinBoxCacheSizeMB);
	registerOption("Preview/DPI", &pdfDocumentConfig->dpi, QApplication::desktop()->logicalDpiX(), &pseudoDialog->spinBoxPreviewDPI);
	registerOption("Preview/Scale Option", &pdfDocumentConfig->scaleOption, 1, &pseudoDialog->comboBoxPreviewScale);
	registerOption("Preview/Scale", &pdfDocumentConfig->scale, 100, &pseudoDialog->spinBoxPreviewScale);
	registerOption("Preview/Magnifier Size", &pdfDocumentConfig->magnifierSize, 300, &pseudoDialog->spinBoxPreviewMagnifierSize);
	registerOption("Preview/Magnifier Shape", &pdfDocumentConfig->magnifierShape, 1, &pseudoDialog->comboBoxPreviewMagnifierShape);
	registerOption("Preview/Magnifier Border", &pdfDocumentConfig->magnifierBorder, false, &pseudoDialog->checkBoxPreviewMagnifierBorder);
	
	registerOption("Preview/Sync File Mask", &pdfDocumentConfig->syncFileMask, "*.tex", &pseudoDialog->lineEditPreviewSyncFileMask);
	
#ifndef QT_NO_DEBUG
	registerOption("Debug/Last Application Modification", &debugLastFileModification);
	registerOption("Debug/Last Full Test Run", &debugLastFullTestRun);
#endif
	
	
}

ConfigManager::~ConfigManager(){
	delete editorConfig;
	delete completerConfig;
	delete webPublishDialogConfig;
	delete insertGraphicsConfig;
	if (persistentConfig) delete persistentConfig;
}

QSettings* ConfigManager::readSettings(bool reread) {
	//load config
	QSettings *config = persistentConfig;
	bool importTexmakerSettings = false;
	bool importTexMakerXSettings = false;
	if (!config){
		bool usbMode = isExistingFileRealWritable(QCoreApplication::applicationDirPath()+"/texstudio.ini");
		if (!usbMode)
			if (isExistingFileRealWritable(QCoreApplication::applicationDirPath()+"/texmakerx.ini")) {
				//import texmaker usb settings
				usbMode=(QFile(QCoreApplication::applicationDirPath()+"/texmakerx.ini")).copy(QCoreApplication::applicationDirPath()+"/texstudio.ini");
				importTexMakerXSettings = true;
			}
		if (!usbMode)
			if (isExistingFileRealWritable(QCoreApplication::applicationDirPath()+"/texmaker.ini")) {
				//import texmaker usb settings
				usbMode=(QFile(QCoreApplication::applicationDirPath()+"/texmaker.ini")).copy(QCoreApplication::applicationDirPath()+"/texstudio.ini");
				importTexmakerSettings = true;
			}
		if (usbMode) {
			config=new QSettings(QCoreApplication::applicationDirPath()+"/texstudio.ini",QSettings::IniFormat);
		} else {
			config=new QSettings(QSettings::IniFormat,QSettings::UserScope,"texstudio","texstudio");
			if (config->childGroups().empty()) {
				QSettings texmakerxsetting(QSettings::IniFormat,QSettings::UserScope,"benibela","texmakerx");				
				//import texmakerx global settings
				if (texmakerxsetting.childGroups().empty()) {
					//import texmaker global settings
					QSettings oldconfig(QSettings::IniFormat,QSettings::UserScope,"xm1","texmaker");
					QStringList keys=oldconfig.allKeys();
					foreach(const QString& key, keys) config->setValue(key,oldconfig.value(key,""));
					importTexmakerSettings = true;
				} else {
					QDir texmakerxdir = QFileInfo(texmakerxsetting.fileName()).dir();
					QDir texstudiodir = QFileInfo(config->fileName()).dir();
					if (!texstudiodir.exists()) {
						QDir txsparent = texstudiodir;
						txsparent.cdUp();
						txsparent.mkpath(texstudiodir.absolutePath());
					}
					QStringList additionalFiles = texmakerxdir.entryList(QStringList() << "*");
					foreach (const QString &f, additionalFiles)
						QFile::copy(texmakerxdir.absoluteFilePath(f),texstudiodir.absoluteFilePath(f));
					
					QStringList keys=texmakerxsetting.allKeys();
					foreach(const QString& key, keys) config->setValue(key,texmakerxsetting.value(key,""));
					importTexMakerXSettings = true;
				}
				
			}
		}
		configFileName=config->fileName();
		configFileNameBase=configFileName;
		configBaseDir=QFileInfo(configFileName).absolutePath();
		if (!configBaseDir.endsWith("/")) configBaseDir+="/";
		completerConfig->importedCwlBaseDir=configBaseDir;// set in LatexCompleterConfig to get access from LatexDocument
		if (configFileNameBase.endsWith(".ini")) configFileNameBase=configFileNameBase.replace(QString(".ini"),"");
		persistentConfig = config;
	}
	
	config->beginGroup("texmaker");
	
	if ((importTexmakerSettings || importTexMakerXSettings) && config->value("Tools/IntegratedPdfViewer", false).toBool())
		config->setValue("Tools/Pdf", "<default>");
	
	//----------managed properties--------------------
	for (int i=0;i<managedProperties.size();i++)
		managedProperties[i].valueFromQVariant(config->value(managedProperties[i].name, managedProperties[i].def));
	
	//language
	QString locale=language;
	appTranslator=new QTranslator(this);
	basicTranslator=new QTranslator(this);
	loadTranslations(language);
	QCoreApplication::installTranslator(appTranslator);
	QCoreApplication::installTranslator(basicTranslator);
	
	//------------------files--------------------
	newFileEncoding=QTextCodec::codecForName(newFileEncodingName.toAscii().data());
	
	//----------------------------dictionaries-------------------------
	
	if (spellDictDir.isEmpty() || ((importTexmakerSettings  || importTexMakerXSettings)  && !QFileInfo(QDir(spellDictDir), spellLanguage+".dic").exists())) {
		// non-exeistent or invalid settings for dictionary
		// try restore from old format where there was only one dictionary - spell_dic can be removed later when users have migrated to the new version
		QString dic = spell_dic;
		if (!QFileInfo(dic).exists()) {
			// fallback to defaults
			QStringList temp;
			QStringList fallBackPaths;
#ifndef Q_WS_WIN
#ifndef PREFIX
#define PREFIX
#endif
			fallBackPaths << PREFIX"/share/hunspell" << PREFIX"/share/myspell"
			              << "/usr/share/hunspell" << "/usr/share/myspell" ;
#endif
#ifdef Q_OS_MAC
			fallBackPaths << "/Applications/texstudio.app/Contents/Resources";
#endif
			dic=findResourceFile(QString(QLocale::system().name())+".dic", true, temp, fallBackPaths);
			if (dic=="") spell_dic=findResourceFile("en_US.dic", true, temp, fallBackPaths);
			if (dic=="") spell_dic=findResourceFile("en_GB.dic", true, temp, fallBackPaths);
			if (dic=="") spell_dic=findResourceFile("fr_FR.dic", true, temp, fallBackPaths);
			if (dic=="") spell_dic=findResourceFile("de_DE.dic", true, temp, fallBackPaths);
		}
		QFileInfo fi(dic);
		if (fi.exists()) {
			spellDictDir = fi.absolutePath();
			spellLanguage = fi.baseName();
		}
	}
	if (grammarCheckerConfig->wordlistsDir.isEmpty()) {
		QString sw = findResourceFile("de.stopWords", true, QStringList(), QStringList() << spellDictDir);
		if (sw=="") sw = findResourceFile("en.stopWords", true, QStringList(), QStringList() << spellDictDir);
		if (QFileInfo(sw).exists()) grammarCheckerConfig->wordlistsDir = QFileInfo(sw).absolutePath();
	}
	
	if (thesaurus_database == "<dic not found>") {
		thesaurus_database = "";
	}
	if (thesaurus_database != "") {
		QFileInfo fi(thesaurus_database);
		if (!fi.exists()) { // try finding the file in other directories (e.g. after update tmx->txs
			thesaurus_database = findResourceFile(fi.fileName());
		}
	}
	if (thesaurus_database == "") { // fall back to system or fixed language
		QStringList fallBackPaths;
#ifdef Q_WS_X11
		fallBackPaths << PREFIX"/share/mythes" << "/usr/share/mythes" ;
#endif
		thesaurus_database=findResourceFile("th_"+QString(QLocale::system().name())+"_v2.dat", true, QStringList(), fallBackPaths);
		if (thesaurus_database=="") thesaurus_database=findResourceFile("th_en_US_v2.dat");
		if (thesaurus_database=="") thesaurus_database=findResourceFile("th_en_GB_v2.dat");
		if (thesaurus_database=="") thesaurus_database=findResourceFile("th_fr_FR_v2.dat");
		if (thesaurus_database=="") thesaurus_database=findResourceFile("th_de_DE_v2.dat");
	}
	
	
	//----------------------------editor--------------------
	if (editorConfig->showlinemultiples==-1) {
		if (config->value("Editor/Line Numbers",true).toBool()) editorConfig->showlinemultiples=1;  //texmaker import
		else editorConfig->showlinemultiples=0;
	}
	
	//completion
    QStringList cwlFiles=config->value("Editor/Completion Files",QStringList() << "tex.cwl" << "latex-document.cwl" << "latex-mathsymbols.cwl").toStringList();
	//completerConfig->words=loadCwlFiles(cwlFiles,ltxCommands,completerConfig);
	LatexParser &latexParser = LatexParser::getInstance();
	foreach(const QString& cwlFile,cwlFiles){
		LatexPackage pck=loadCwlFile(cwlFile,completerConfig);
		completerConfig->words.append(pck.completionWords);
		latexParser.optionCommands.unite(pck.optionCommands);
		latexParser.environmentAliases.unite(pck.environmentAliases);
		//ltxCommands->possibleCommands.unite(pck.possibleCommands); // qt error, does not work properly
		foreach(const QString& elem,pck.possibleCommands.keys()){
			QSet<QString> set2=pck.possibleCommands[elem];
			QSet<QString> set=latexParser.possibleCommands[elem];
			set.unite(set2);
			latexParser.possibleCommands[elem]=set;
		}
	}
	
	completerConfig->setFiles(cwlFiles);
	// remove old solution from .ini
	if(config->contains("Editor/Completion Usage"))
		config->remove("Editor/Completion Usage");
	//web publish dialog
	webPublishDialogConfig->readSettings(*config);
	//insert graphics dialog
	insertGraphicsConfig->readSettings(*config);
	
	//build commands
	if (!buildManager) {
		txsCritical("No build Manager created! => crash");
		return 0;
	}
	buildManager->readSettings(*config);
	runLaTeXBibTeXLaTeX=config->value("Tools/After BibTeX Change", "tmx://latex && tmx://bibtex && tmx://latex").toString()!="";
	
	//import old key replacements or set default
	QStringList keyReplace, keyReplaceAfterWord, keyReplaceBeforeWord;
	if (!config->value("User/New Key Replacements Created",false).toBool()) {
		int keyReplaceCount = config->value("User/KeyReplaceCount",-1).toInt();
		if (keyReplaceCount ==-1) {
			//default
			/* new system ...
   keyReplace.append("\"");
   QString loc=QString(QLocale::system().name()).left(2);
   if (loc=="de") {
    keyReplaceBeforeWord.append("\">");
    keyReplaceAfterWord.append("\"<");
   } else {
    keyReplaceAfterWord.append("''");
    keyReplaceBeforeWord.append("``");
   }
      */
			keyReplace.append("%");
			keyReplaceBeforeWord.append("%");
			keyReplaceAfterWord.append(" %");
		} else for (int i=0; i<keyReplaceCount; i++) {
			keyReplace.append(config->value("User/KeyReplace"+QVariant(i).toString(),i!=0?"'":"\"").toString());
			keyReplaceAfterWord.append(config->value("User/KeyReplaceAfterWord"+QVariant(i).toString(),i!=0?"":"").toString());
			keyReplaceBeforeWord.append(config->value("User/KeyReplaceBeforeWord"+QVariant(i).toString(),i!=0?"":"\">").toString());
		}
	}
	
	//user macros
	if(!reread){
	    if (config->value("Macros/0").isValid()) {
		for (int i=0; i<1000; i++) {
		    QStringList ls = config->value(QString("Macros/%1").arg(i)).toStringList();
		    if (ls.isEmpty()) break;
		    completerConfig->userMacro.append(Macro(ls));
		}
		for (int i=0; i < keyReplace.size(); i++) {
		    completerConfig->userMacro.append(Macro(
							  tr("Key replacement: %1 %2").arg(keyReplace[i]).arg(tr("before word")),
							  keyReplaceBeforeWord[i].replace("%", "%%"),
							  "",
							  "(?language:latex)(?<=\\s|^)"+QRegExp::escape(keyReplace[i])
							  ));
		    completerConfig->userMacro.append(Macro(
							  tr("Key replacement: %1 %2").arg(keyReplace[i]).arg(tr("after word")),
							  keyReplaceAfterWord[i].replace("%", "%%"),
							  "",
							  "(?language:latex)(?<=\\S)"+QRegExp::escape(keyReplace[i])
							  ));
		}
	    } else {
		// try importing old macros
		QStringList userTags = config->value("User/Tags").toStringList();
		QStringList userNames = config->value("User/TagNames").toStringList();
		QStringList userAbbrevs = config->value("User/TagAbbrevs").toStringList();
		QStringList userTriggers = config->value("User/TagTriggers").toStringList();

		while (userTriggers.size()<userTags.size()) userTriggers << "";

		for (int i=0; i < keyReplace.size(); i++) {
		    userNames.append(tr("Key replacement: %1 %2").arg(keyReplace[i]).arg(tr("before word")));
		    userTags.append(keyReplaceBeforeWord[i].replace("%", "%%"));
		    userAbbrevs.append("");
		    userTriggers.append("(?language:latex)(?<=\\s|^)"+QRegExp::escape(keyReplace[i]));

		    userNames.append(tr("Key replacement: %1 %2").arg(keyReplace[i]).arg(tr("after word")));
		    userTags.append(keyReplaceAfterWord[i].replace("%", "%%"));
		    userAbbrevs.append("");
		    userTriggers.append("(?language:latex)(?<=\\S)"+QRegExp::escape(keyReplace[i]));
		}

		for (int i=0;i<userTags.size();i++)
		    completerConfig->userMacro.append(Macro(userNames.value(i,""),userTags[i], userAbbrevs.value(i,""),userTriggers.value(i,"")));
	    }
	}
	//menu shortcuts
	QMap<QString, QString> aliases = QMap<QString, QString>();
	// key and value may be a full command or a prefix only
	aliases.insert("main/user/commands/", "main/tools/user/");
	aliases.insert("main/user/tags/", "main/macros/");
	aliases.insert("main/usertags/", "main/macros/");
	aliases.insert("main/tools/latex", "main/tools/commands/latex");
	aliases.insert("main/tools/viewdvi", "main/tools/commands/viewdvi");
	aliases.insert("main/tools/dvi2ps", "main/tools/commands/dvi2ps");
	aliases.insert("main/tools/viewps", "main/tools/commands/viewps");
	aliases.insert("main/tools/pdflatex", "main/tools/commands/pdflatex");
	aliases.insert("main/tools/viewpdf", "main/tools/commands/viewpdf");
	aliases.insert("main/tools/ps2pdf", "main/tools/commands/ps2pdf");
	aliases.insert("main/tools/dvipdf", "main/tools/commands/dvipdf");
	aliases.insert("main/tools/makeindex", "main/tools/commands/makeindex");
	aliases.insert("main/tools/metapost", "main/tools/commands/metapost");
	aliases.insert("main/tools/asymptote", "main/tools/commands/asymptote");
	aliases.insert("main/edit/eraseLine", "main/edit/lineoperations/eraseLine");
	aliases.insert("main/edit/eraseEndLine", "main/edit/lineoperations/eraseEndLine");

	int size = config->beginReadArray("keysetting");
	for (int i = 0; i < size; ++i) {
		config->setArrayIndex(i);
		QString id = config->value("id").toString();
		for (QMap<QString, QString>::iterator it = aliases.begin(), end = aliases.end(); it != end; ++it)
			if (id.startsWith(it.key())) {
				id.replace(0, it.key().length(), it.value());
				break;
			}
			
		managedMenuNewShortcuts.append(QPair<QString, QString> (id, config->value("key").toString()));
	}
	config->endArray();
	
	//changed latex menus
	manipulatedMenus=config->value("changedLatexMenus").toMap();
	
	//custom toolbar
	for (int i=0; i<managedToolBars.size();i++){
		ManagedToolBar& mtb=managedToolBars[i];
		mtb.actualActions=config->value(mtb.name+"ToolBar").toStringList();
		for (int i=0; i<mtb.actualActions.count(); i++) {
			for (QMap<QString, QString>::iterator it = aliases.begin(), end = aliases.end(); it != end; ++it) {
				QString id = mtb.actualActions.at(i);
				if (id.startsWith(it.key())) {
					id.replace(0, it.key().length(), it.value());
					mtb.actualActions.replace(i, id);
					break;
				}
			}
		}
		if (mtb.actualActions.empty()) mtb.actualActions=mtb.defaults;
	}
	replacedIconsOnMenus=config->value("customIcons").toMap();
	
	//custom highlighting
	customEnvironments=config->value("customHighlighting").toMap();
	LatexParser::getInstance().customCommands = QSet<QString>::fromList(config->value("customCommands").toStringList());
	
	//--------------------appearance------------------------------------
	QFontDatabase fdb;
	QStringList xf = fdb.families();
	//editor
#ifdef Q_WS_WIN
	if (editorConfig->fontFamily.isEmpty()){
		if (xf.contains("Courier New",Qt::CaseInsensitive)) editorConfig->fontFamily="Courier New";
		else editorConfig->fontFamily=qApp->font().family();
	}
	if (editorConfig->fontSize==-1)
		editorConfig->fontSize=10;
#else
	if (editorConfig->fontFamily.isEmpty()){
		if (xf.contains("DejaVu Sans Mono",Qt::CaseInsensitive)) editorConfig->fontFamily="DejaVu Sans Mono";
		//else if (xf.contains("Lucida Sans Unicode",Qt::CaseInsensitive)) editorConfig->fontFamily="Lucida Sans Unicode";
		else if (xf.contains("Lucida Sans Typewriter",Qt::CaseInsensitive)) editorConfig->fontFamily="Lucida Sans Typewriter";
		else editorConfig->fontFamily=qApp->font().family();
	}
	if (editorConfig->fontSize==-1)
		editorConfig->fontSize=qApp->font().pointSize();
#endif
	
	//interface
	systemPalette = QApplication::palette();
	defaultStyleName=QApplication::style()->objectName();
	
#ifdef Q_WS_X11
	if (interfaceFontFamily=="<later>") {
		//use an interface like Texmaker
		if (xf.contains("DejaVu Sans",Qt::CaseInsensitive)) interfaceFontFamily="DejaVu Sans";
		else if (xf.contains("DejaVu Sans LGC",Qt::CaseInsensitive)) interfaceFontFamily="DejaVu Sans LGC";
		else if (xf.contains("Bitstream Vera Sans",Qt::CaseInsensitive)) interfaceFontFamily="Bitstream Vera Sans";
		else if (xf.contains("Luxi Sans",Qt::CaseInsensitive)) interfaceFontFamily="Luxi Sans";
		else interfaceFontFamily=QApplication::font().family();
	}
	if (interfaceStyle=="<later>") {
		if (x11desktop_env()==0) //no-kde
		{
			if (QStyleFactory::keys().contains("GTK+")) interfaceStyle="GTK+";//gtkstyle
			else interfaceStyle="Cleanlooks";
		} else if ((x11desktop_env() ==4) && (QStyleFactory::keys().contains("Oxygen"))) interfaceStyle="Oxygen"; //kde4+oxygen
		else interfaceStyle="Plastique"; //others
	}
#endif
	
	setInterfaceStyle();
	
#ifdef Q_OS_MAC
	// workaround for unwanted font changes when changing the desktop
	// https://sourceforge.net/tracker/?func=detail&aid=3559432&group_id=250595&atid=1126426
	if (interfaceFontFamily != QApplication::font().family() 
	    || interfaceFontSize != QApplication::font().pointSize())
		QApplication::setDesktopSettingsAware(false);
#endif
	QApplication::setFont(QFont(interfaceFontFamily, interfaceFontSize));
		
	config->endGroup();
	
	return config;
}
QSettings* ConfigManager::saveSettings(const QString& saveName) {
	Q_ASSERT(persistentConfig);
	QSettings *config= saveName.isEmpty()?persistentConfig:(new QSettings(saveName, QSettings::IniFormat));
	config->setValue("IniMode",true);
	
	config->beginGroup("version");
	// updated on every access
	config->setValue("written_by_TXS_version", TXSVERSION);
	config->setValue("written_by_TXS_SVNversion", getSimplifiedSVNVersion());
	// written just the very first time
	if (!config->value("created_by_TXS_version").isValid())
		config->setValue("created_by_TXS_version", TXSVERSION);
	if (!config->value("created_by_TXS_SVNversion").isValid())
		config->setValue("created_by_TXS_SVNversion", getSimplifiedSVNVersion());
	config->endGroup();

	config->beginGroup("texmaker");

	buildManager->saveSettings(*config); //save first, so it can set managed properties
	
	//----------managed properties--------------------
	foreach (const ManagedProperty& mp, managedProperties)
		config->setValue(mp.name, mp.valueToQVariant());
	
	//completion
	if (!completerConfig->getLoadedFiles().isEmpty())
		config->setValue("Editor/Completion Files",completerConfig->getLoadedFiles());
	
	//web publish dialog
	webPublishDialogConfig->saveSettings(*config);
	insertGraphicsConfig->saveSettings(*config);
	
	
	//---------------------build commands----------------
	config->setValue("Tools/After BibTeX Change",runLaTeXBibTeXLaTeX?"tmx://latex && tmx://bibtex && tmx://latex":"");
	
	//-------------------key replacements-----------------
	config->setValue("User/New Key Replacements Created", true);
	
	//user macros
	int index=0;
	foreach (const Macro&m, completerConfig->userMacro){
		if(m.name==TXS_AUTO_REPLACE_QUOTE_OPEN || m.name==TXS_AUTO_REPLACE_QUOTE_CLOSE || m.document)
			continue;
		config->setValue(QString("Macros/%1").arg(index++), m.toStringList());
	}
    while(config->contains(QString("Macros/%1").arg(index))){ //remove old macros which are not used any more
        config->remove(QString("Macros/%1").arg(index));
        index++;
    }
    // remove old Tags
    config->remove("User/Tags");
    config->remove("User/TagNames");
    config->remove("User/TagAbbrevs");
    config->remove("User/TagTriggers");

	//menu shortcuts
	config->beginWriteArray("keysetting");
	for (int i = 0; i < managedMenuNewShortcuts.size(); ++i) {
		config->setArrayIndex(i);
		config->setValue("id", managedMenuNewShortcuts[i].first);
		config->setValue("key", managedMenuNewShortcuts[i].second);
	}
	config->endArray();
	
	//changed latex menus
	config->setValue("changedLatexMenus",manipulatedMenus);
	//custom toolbar
	for (int i=0; i<managedToolBars.size();i++){
		ManagedToolBar& mtb=managedToolBars[i];
		if (mtb.actualActions == mtb.defaults) config->setValue(mtb.name+"ToolBar", QStringList());
		else config->setValue(mtb.name+"ToolBar", mtb.actualActions);
	}
	config->setValue("customIcons",replacedIconsOnMenus);
	// custom highlighting
	config->setValue("customHighlighting",customEnvironments);
	QStringList zw=LatexParser::getInstance().customCommands.toList();
	config->setValue("customCommands",zw);
	
	config->endGroup();
	
	config->sync();
	
	return config;
}

bool ConfigManager::execConfigDialog() {
	ConfigDialog *confDlg = new ConfigDialog(qobject_cast<QWidget*>(parent()));
	confDlg->riddled = configRiddled;
	//----------managed properties--------------------
	foreach (const ManagedProperty& mp, managedProperties)
		if (mp.widgetOffset)
			mp.writeToObject(*((QWidget**)((char*)&confDlg->ui + mp.widgetOffset))); //convert to char*, because the offset is in bytes
	
	//files
	//if (newfile_encoding)
	//	confDlg->ui.comboBoxEncoding->setCurrentIndex(confDlg->ui.comboBoxEncoding->findText(newfile_encoding->name(), Qt::MatchExactly));

	//-----------------------editor------------------------------
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
	if(confDlg->ui.comboBoxAutoIndent->currentIndex()>0 && editorConfig->indentWithSpaces) confDlg->ui.comboBoxAutoIndent->setCurrentIndex(confDlg->ui.comboBoxAutoIndent->currentIndex()+2);
	
	//completion
	confDlg->ui.checkBoxCaseSensitive->setChecked(completerConfig->caseSensitive!=LatexCompleterConfig::CCS_CASE_INSENSITIVE);
	confDlg->ui.checkBoxCaseSensitiveInFirstCharacter->setChecked(completerConfig->caseSensitive==LatexCompleterConfig::CCS_FIRST_CHARACTER_CASE_SENSITIVE);
	
	
	lastLanguage = language;
	QStringList languageFiles=findResourceFiles("translations","texstudio_*.qm") << findResourceFiles("","texstudio_*.qm");
	languageFiles << findResourceFiles("translations","texmakerx_*.qm") << findResourceFiles("","texmakerx_*.qm");
	for (int i=languageFiles.count()-1;i>=0;i--){
		QString temp = languageFiles[i].mid(languageFiles[i].indexOf("_")+1);
		temp.truncate(temp.indexOf("."));
		if (languageFiles.contains(temp)) languageFiles.removeAt(i);
		else languageFiles[i] = temp;
	}
	if (!languageFiles.contains("en")) languageFiles.append("en");
	int langId=-1;
	for (int i=0;i<languageFiles.count();i++){
		confDlg->ui.comboBoxLanguage->addItem(languageFiles[i]);
		if (languageFiles[i] == language) langId=i;
	}
	confDlg->ui.comboBoxLanguage->addItem(tr("default"));
	if (language=="") confDlg->ui.comboBoxLanguage->setEditText(tr("default"));
	else confDlg->ui.comboBoxLanguage->setEditText(language);
	if (langId!=-1) confDlg->ui.comboBoxLanguage->setCurrentIndex(langId);
	else confDlg->ui.comboBoxLanguage->setCurrentIndex(confDlg->ui.comboBoxLanguage->count()-1);
	
	QStringList files=findResourceFiles("completion","*.cwl",QStringList(configBaseDir));
	QListWidgetItem *item;
	const QStringList& loadedFiles = completerConfig->getLoadedFiles();
	foreach(const QString& elem,files) {
		item=new QListWidgetItem(elem,confDlg->ui.completeListWidget);
		item->setFlags(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
		if (loadedFiles.contains(elem)) item->setCheckState(Qt::Checked);
		else  item->setCheckState(Qt::Unchecked);
	}
	//preview
	confDlg->ui.comboBoxDvi2PngMode->setCurrentIndex(buildManager->dvi2pngMode);
	
	//Autosave
	if(autosaveEveryMinutes==0) confDlg->ui.comboBoxAutoSave->setCurrentIndex(0);
	if(0<autosaveEveryMinutes && autosaveEveryMinutes<6) confDlg->ui.comboBoxAutoSave->setCurrentIndex(1);
	if(5<autosaveEveryMinutes && autosaveEveryMinutes<11) confDlg->ui.comboBoxAutoSave->setCurrentIndex(2);
	if(10<autosaveEveryMinutes && autosaveEveryMinutes<21) confDlg->ui.comboBoxAutoSave->setCurrentIndex(3);
	if(20<autosaveEveryMinutes) confDlg->ui.comboBoxAutoSave->setCurrentIndex(4);
	//--build things
	//normal commands
	pdflatexEdit = 0;
	tempCommands = buildManager->getAllCommands();
	QStringList tempOrder = buildManager->getCommandsOrder();
	rerunButtons.clear(); commandInputs.clear();
	createCommandList(confDlg->ui.groupBoxCommands, tempOrder, false, false);
	createCommandList(confDlg->ui.groupBoxMetaCommands, tempOrder, false, true);
	createCommandList(confDlg->ui.groupBoxUserCommands, tempOrder, true, false);
    confDlg->setBuildManger(buildManager);
	
	//quickbuild/more page	
	confDlg->ui.checkBoxReplaceBeamer->setChecked(buildManager->previewRemoveBeamer);
	confDlg->ui.checkBoxPrecompilePreamble->setChecked(buildManager->previewPrecompilePreamble);
	
	confDlg->ui.checkBoxRunAfterBibTeXChange->setChecked(runLaTeXBibTeXLaTeX);
	
	QIcon fileOpenIcon = getRealIcon("fileopen");
	confDlg->ui.pushButtonDictDir->setIcon(fileOpenIcon);
	confDlg->ui.btSelectThesaurusFileName->setIcon(fileOpenIcon);

	// grammar
	confDlg->ui.pushButtonGrammarWordlists->setIcon(fileOpenIcon);
	confDlg->ui.pushButtonGrammarLTPath->setIcon(fileOpenIcon);
	confDlg->ui.pushButtonGrammarLTJava->setIcon(fileOpenIcon);
	
	//menu shortcuts
	QTreeWidgetItem * menuShortcuts=new QTreeWidgetItem((QTreeWidget*)0, QStringList() << QString(tr("Menus")));
	foreach(QMenu* menu, managedMenus)
		managedMenuToTreeWidget(menuShortcuts,menu);
	confDlg->ui.shortcutTree->addTopLevelItem(menuShortcuts);
	menuShortcuts->setExpanded(true);
	
	QTreeWidgetItem * editorItem=new QTreeWidgetItem((QTreeWidget*)0, QStringList() << ConfigDialog::tr("Editor"));
	QTreeWidgetItem * editorKeys = new QTreeWidgetItem(editorItem, QStringList() << ConfigDialog::tr("Basic Key Mapping"));
	const int editorKeys_EditOperationRole = Qt::UserRole;
	
	Q_ASSERT((int)Qt::CTRL == (int)Qt::ControlModifier && (int)Qt::ALT == (int)Qt::AltModifier && (int)Qt::SHIFT == (int)Qt::ShiftModifier && (int)Qt::META == (int)Qt::MetaModifier);
	QMultiMap<int, int> keysReversed;
	QHash<int, int>::const_iterator it = this->editorKeys.constBegin();
	while (it != this->editorKeys.constEnd()) {
		keysReversed.insertMulti(it.value(), it.key());
		++it;
	}
	int ht=confDlg->ui.comboBoxLanguage->minimumSizeHint().height();
	foreach(const int elem, editorAvailableOperations){
		QList<int> keys=keysReversed.values(elem);
		bool listEmpty=false;
		if(keys.isEmpty()){
			keys<< 0;
			listEmpty=true;
		}
		foreach(const int key,keys){
			QTreeWidgetItem * twi=0;
			if(listEmpty){
				twi = new QTreeWidgetItem(editorKeys, QStringList() << LatexEditorViewConfig::translateEditOperation(elem) << "" << tr("<none>"));
			} else {
				twi = new QTreeWidgetItem(editorKeys, QStringList() << LatexEditorViewConfig::translateEditOperation(elem) << "" << QKeySequence(key).toString(QKeySequence::NativeText));
			}
			twi->setData(0, editorKeys_EditOperationRole, elem);
			twi->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
#ifdef Q_WS_WIN
			QSize sz=twi->sizeHint(0);
			twi->setSizeHint(0,QSize(sz.width(),ht));
#endif
		}
	}
	QTreeWidgetItem * twi=new QTreeWidgetItem(editorKeys, QStringList() << ShortcutDelegate::addRowButton);
#ifdef Q_WS_WIN
	QSize sz=twi->sizeHint(0);
	twi->setSizeHint(0,QSize(sz.width(),ht));
#else
    Q_UNUSED(twi);
    Q_UNUSED(ht);
#endif

	
	confDlg->ui.shortcutTree->addTopLevelItem(editorItem);
	editorItem->setExpanded(true);
	
	ShortcutDelegate delegate;
	delegate.treeWidget=confDlg->ui.shortcutTree;
	confDlg->ui.shortcutTree->setItemDelegate(&delegate); //setting in the config dialog doesn't work
	delegate.connect(confDlg->ui.shortcutTree,SIGNAL(itemClicked(QTreeWidgetItem *, int)),&delegate,SLOT(treeWidgetItemClicked(QTreeWidgetItem * , int)));
	
	//custom menus
	confDlg->menuParent=menuParent;
	changedItemsList.clear();
	manipulatedMenuTree.clear();
	superAdvancedItems.clear();
	foreach(QMenu* menu, managedMenus){
		QTreeWidgetItem *menuLatex=managedLatexMenuToTreeWidget(0,menu);
		if(menuLatex) {
			confDlg->ui.latexTree->addTopLevelItem(menuLatex);
			menuLatex->setExpanded(true);
		}
	}
	connect(confDlg->ui.checkBoxShowAllMenus, SIGNAL(toggled(bool)), SLOT(toggleVisibleTreeItems(bool)));
	toggleVisibleTreeItems(false);
	connect(confDlg->ui.latexTree,SIGNAL(itemChanged(QTreeWidgetItem*,int)),this,SLOT(latexTreeItemChanged(QTreeWidgetItem*,int)));
	QAction* act = new QAction(tr("Insert New Menu Item (before)"), confDlg->ui.latexTree);
	connect(act, SIGNAL(triggered()), SLOT(latexTreeNewItem()));
	confDlg->ui.latexTree->addAction(act);
	act = new QAction(tr("Insert New Sub Menu (before)"), confDlg->ui.latexTree);
	connect(act, SIGNAL(triggered()), SLOT(latexTreeNewMenuItem()));
	confDlg->ui.latexTree->addAction(act);
	confDlg->ui.latexTree->setContextMenuPolicy(Qt::ActionsContextMenu);

	ComboBoxDelegate * cbd = new ComboBoxDelegate(confDlg->ui.latexTree);
	cbd->defaultItems = possibleMenuSlots;
	confDlg->ui.latexTree->setItemDelegate(cbd);
	
	// custom toolbars
	confDlg->customizableToolbars.clear();
	foreach (const ManagedToolBar &mtb, managedToolBars){
		Q_ASSERT(mtb.toolbar);
		confDlg->customizableToolbars.append(mtb.actualActions);
		confDlg->ui.comboBoxToolbars->addItem(qApp->translate("Texmaker",qPrintable(mtb.name)));
	}
	confDlg->allMenus=managedMenus;
	confDlg->standardToolbarMenus=QList<QMenu*>()<< getManagedMenu("main/latex") << getManagedMenu("main/math") << getManagedMenu("main/macros");
	confDlg->ui.comboBoxActions->addItem(tr("Latex/Math menus"));
	confDlg->ui.comboBoxActions->addItem(tr("All menus"));
	confDlg->ui.comboBoxActions->addItem(tr("Special Tags"));
	confDlg->replacedIconsOnMenus=&replacedIconsOnMenus;
	
	
	//appearance
	QString displayedInterfaceStyle=interfaceStyle==""?tr("default"):interfaceStyle;
	confDlg->ui.comboBoxInterfaceStyle->clear();
	confDlg->ui.comboBoxInterfaceStyle->addItems(QStyleFactory::keys()<<tr("default"));
	confDlg->ui.comboBoxInterfaceStyle->setCurrentIndex(confDlg->ui.comboBoxInterfaceStyle->findText(displayedInterfaceStyle));
	confDlg->ui.comboBoxInterfaceStyle->setEditText(displayedInterfaceStyle);
	
	confDlg->ui.checkBoxTabbedLogView->setChecked(tabbedLogView);
	confDlg->ui.checkBoxTabbedStructureView->setChecked(!newLeftPanelLayout);
	
	confDlg->fmConfig->setBasePointSize( editorConfig->fontSize );
	confDlg->fmConfig->addScheme("",QDocument::defaultFormatScheme());
	
	// custom higlighting
	{
		confDlg->environModes=&enviromentModes;
		int l=0;
		confDlg->ui.twHighlighEnvirons->setRowCount(customEnvironments.size()+1);
		QMap<QString, QVariant>::const_iterator i;
		for (i = customEnvironments.constBegin(); i != customEnvironments.constEnd(); ++i){
			QString env=i.key();
			QTableWidgetItem *item=new QTableWidgetItem(env);
			confDlg->ui.twHighlighEnvirons->setItem(l,0,item);
			//item=new QTableWidgetItem(i.value());
			QComboBox *cb=new QComboBox(0);
			cb->insertItems(0,enviromentModes);
			cb->setCurrentIndex(i.value().toInt());
			confDlg->ui.twHighlighEnvirons->setCellWidget(l,1,cb);
			l++;
		}
		QTableWidgetItem *item=new QTableWidgetItem("");
		confDlg->ui.twHighlighEnvirons->setItem(l,0,item);
		//item=new QTableWidgetItem(i.value());
		QComboBox *cb=new QComboBox(0);
		cb->insertItems(0,enviromentModes);
		confDlg->ui.twHighlighEnvirons->setCellWidget(l,1,cb);
		
		confDlg->ui.twCustomSyntax->setRowCount(LatexParser::getInstance().customCommands.count()+1);
		l=0;
		foreach(const QString& cmd, LatexParser::getInstance().customCommands){
			QTableWidgetItem *item=new QTableWidgetItem(cmd);
			confDlg->ui.twCustomSyntax->setItem(l,0,item);
			l++;
		}
		item=new QTableWidgetItem("");
		confDlg->ui.twCustomSyntax->setItem(l,0,item);
	}
	
	
	//EXECUTE IT
	bool executed = confDlg->exec();
	configRiddled = confDlg->riddled;
	
	//handle changes
	if (executed) {
		QList<void*> changedProperties;
		//----------managed properties--------------------
		for (int i=0;i<managedProperties.size();i++)
			if (managedProperties[i].widgetOffset && managedProperties[i].readFromObject(*((QWidget**)((char*)&confDlg->ui + managedProperties[i].widgetOffset))))
				changedProperties << managedProperties[i].storage;
		
		//files
		newFileEncoding=QTextCodec::codecForName(newFileEncodingName.toAscii().data());
		
		if (changedProperties.contains(&maxRecentFiles) || changedProperties.contains(&maxRecentProjects))
			updateRecentFiles(true);
		
		//editor
		editorConfig->autoindent=confDlg->ui.comboBoxAutoIndent->currentIndex()!=0;
		editorConfig->weakindent=(confDlg->ui.comboBoxAutoIndent->currentIndex()&1)==1;
		editorConfig->indentWithSpaces=confDlg->ui.comboBoxAutoIndent->currentIndex()>2;
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
		
		//autosave
		QList<int> times;
		times << 0 << 5 << 10 << 20 << 60;
		autosaveEveryMinutes=times.value(confDlg->ui.comboBoxAutoSave->currentIndex(),0);
		// update macros menu to update quote replacement
		if (changedProperties.contains(&replaceQuotes)) {
			bool conflict = false;
			if (replaceQuotes)
				for(int i=0;i<completerConfig->userMacro.count();i++){
					const Macro& m=completerConfig->userMacro.at(i);
					if (m.name == TXS_AUTO_REPLACE_QUOTE_OPEN ||
					              m.name == TXS_AUTO_REPLACE_QUOTE_CLOSE) continue;
					if (m.trigger == "(?language:latex)(?<=\\s|^)\"" || m.trigger == "(?language:latex)(?<=^)\"" || m.trigger == "(?language:latex)(?<=\\S)\"") {
						conflict = true;
						break;
					}
				}
			if (conflict) 
				if (txsConfirm(tr("You have enabled auto quote replacement. However, there are macros with trigger string (?language:latex)(?<=\\s|^) or (?language:latex)(?<=\\S) which will override the new quote replacement.\nDo you want to remove them?"))){
					for(int i=completerConfig->userMacro.count()-1;i>=0;i--){
						const Macro& m=completerConfig->userMacro.at(i);
						if (m.trigger == "(?language:latex)(?<=\\s|^)\"" || m.trigger == "(?language:latex)(?<=^)\"" || m.trigger == "(?language:latex)(?<=\\S)\"") 
							completerConfig->userMacro.removeAt(i);
					}
				}
		}
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
		LatexParser& latexParser = LatexParser::getInstance();
		latexParser.clear();
		latexParser.init();
		//completerConfig->words=loadCwlFiles(newFiles,ltxCommands,completerConfig);
		foreach(const QString& cwlFile,newFiles){
			LatexPackage pck=loadCwlFile(cwlFile,completerConfig);
			completerConfig->words.append(pck.completionWords);
			latexParser.optionCommands.unite(pck.optionCommands);
			latexParser.environmentAliases.unite(pck.environmentAliases);
			
			//ltxCommands->possibleCommands.unite(pck.possibleCommands); qt bug
			foreach(const QString& elem,pck.possibleCommands.keys()){
				QSet<QString> set2=pck.possibleCommands[elem];
				QSet<QString> set=latexParser.possibleCommands[elem];
				set.unite(set2);
				latexParser.possibleCommands[elem]=set;
			}
		}
		completerConfig->setFiles(newFiles);
		//preview
		previewMode=(PreviewMode) confDlg->ui.comboBoxPreviewMode->currentIndex();
		buildManager->dvi2pngMode=(BuildManager::Dvi2PngMode) confDlg->ui.comboBoxDvi2PngMode->currentIndex();
		
		//build things
		QStringList userOrder;
		for (CommandMapping::iterator it = tempCommands.begin(), end = tempCommands.end(); it != end; )
			if (it.value().user) it = tempCommands.erase(it);
			else  ++it;
		for (int i=0;i<commandInputs.size();i++){
			CommandMapping::iterator it = tempCommands.find(commandInputs[i]->property(PROPERTY_COMMAND_ID).toString());
            if (it != tempCommands.end()) {
                QString text = getText(commandInputs[i]);
                QComboBox *cb=qobject_cast<QComboBox*>(commandInputs[i]);
                int i=-1;
                if(cb)
                    i=cb->findText(text);
                if(i>=0)
                    text=it.value().metaSuggestionList.value(i);
                it.value().commandLine =text;
            }
		}
		for (int i=0;i<rerunButtons.size();i++){
			CommandMapping::iterator it = tempCommands.find(getCmdID(rerunButtons[i]));
			if (it != tempCommands.end()) {
				it.value().rerunCompiler = rerunButtons[i]->isChecked();
			}
		}
		//read user commands
		for (int i=0; i<userGridLayout->count(); i++) {
			QWidget *nameWidget = userGridLayout->itemAt(i)->widget();
			if (!nameWidget) continue;
			if (CG_ID == nameWidget->property(PROPERTY_WIDGET_TYPE).toInt()) {
				CommandInfo ci;
				QString combinedName = getText(nameWidget);
				int pos = combinedName.indexOf(":");
				ci.id = pos == -1?combinedName:combinedName.left(pos);
				if (ci.id.isEmpty()) ci.id = "user";
				ci.displayName = pos == -1?combinedName:combinedName.mid(pos+1);
				ci.user = true;

				while (i<userGridLayout->count()-1) {
					i++;
					QWidget *w = userGridLayout->itemAt(i)->widget();
					if (!w) continue;
					int type = w->property(PROPERTY_WIDGET_TYPE).toInt();
					if (type == CG_ID || type == CG_ADD) {
						i--;
						break;
					} else if (type == CG_RERUN) {
						ci.rerunCompiler = static_cast<QPushButton*>(w)->isChecked();
					} else if (type == CG_CMD) {
						ci.commandLine = getText(w);
					}
				}
				tempCommands.insert(ci.id, ci);
				userOrder << ci.id;
			}
		}

		buildManager->setAllCommands(tempCommands, userOrder);
		/*TODO for (BuildManager::LatexCommand cmd=BuildManager::CMD_LATEX; cmd < BuildManager::CMD_USER_QUICK; ++cmd){
			if (!commandsToEdits.value(cmd)) continue;
			buildManager->setLatexCommand(cmd,commandsToEdits.value(cmd)->text());;
		}
		
		for (BuildManager::LatexCommand cmd=BuildManager::CMD_SVN; cmd <= BuildManager::CMD_SVNADMIN; ++cmd){
			if (!commandsToEdits.value(cmd)) continue;
			buildManager->setLatexCommand(cmd,commandsToEdits.value(cmd)->text());;
		}*/
		
		/*Q_ASSERT(confDlg->checkboxInternalPDFViewer);
		QString curPdfViewer = buildManager->getLatexCommand(BuildManager::CMD_VIEWPDF);
		if (confDlg->checkboxInternalPDFViewer && confDlg->checkboxInternalPDFViewer->isChecked() != curPdfViewer.startsWith(BuildManager::TXS_INTERNAL_PDF_VIEWER)) {
			//prepend/remove tmx://internal-pdf-viewer
			if (confDlg->checkboxInternalPDFViewer->isChecked())
				buildManager->setLatexCommand(BuildManager::CMD_VIEWPDF , BuildManager::TXS_INTERNAL_PDF_VIEWER + "/" + curPdfViewer);
			else if (curPdfViewer.startsWith(BuildManager::TXS_INTERNAL_PDF_VIEWER+"/"))
				buildManager->setLatexCommand(BuildManager::CMD_VIEWPDF , curPdfViewer.mid(BuildManager::TXS_INTERNAL_PDF_VIEWER.length() + 1));
			else
				buildManager->setLatexCommand(BuildManager::CMD_VIEWPDF , curPdfViewer.mid(BuildManager::TXS_INTERNAL_PDF_VIEWER.length()));
		}
		
		buildManager->setLatexCommand(BuildManager::CMD_USER_PRECOMPILE,confDlg->ui.lineEditExecuteBeforeCompiling->text());
		buildManager->setLatexCommand(BuildManager::CMD_USER_QUICK,confDlg->ui.lineEditUserquick->text());
		*/
		/*for (int i=0;i < confDlg->ui.quickbuildLayout->count(); i++) {
			QRadioButton *rb = qobject_cast<QRadioButton*>(confDlg->ui.quickbuildLayout->itemAt(i)->widget());
			if (rb && rb->isChecked()){
				buildManager->quickmode=rb->property("quickBuildMode").toInt();
				break;
			}
		}
		*/
		buildManager->previewRemoveBeamer = confDlg->ui.checkBoxReplaceBeamer->isChecked();
		buildManager->previewPrecompilePreamble = confDlg->ui.checkBoxPrecompilePreamble->isChecked();
		
		runLaTeXBibTeXLaTeX=confDlg->ui.checkBoxRunAfterBibTeXChange->isChecked();
		
		
		//formats
		confDlg->fmConfig->apply();
		this->editorKeys.clear();
		for (int i=0;i<editorKeys->childCount();i++) {
			int editOperation = editorKeys->child(i)->data(0, editorKeys_EditOperationRole).toInt();
			QKeySequence kSeq = QKeySequence::fromString(editorKeys->child(i)->text(2),QKeySequence::NativeText);
			if (!kSeq.isEmpty() && editOperation > 0) /* not QEditor::Invalid or QEditor::NoOperation*/
				this->editorKeys.insert(kSeq, editOperation);
		}

		//menus
		managedMenuNewShortcuts.clear();
		treeWidgetToManagedMenuTo(menuShortcuts);
		treeWidgetToManagedLatexMenuTo();
		
		// custom toolbar
		Q_ASSERT(confDlg->customizableToolbars.size() == managedToolBars.size());
		for (int i=0; i<managedToolBars.size();i++){
			ManagedToolBar& mtb=managedToolBars[i];
			mtb.actualActions=confDlg->customizableToolbars[i];
		}
		
		//  interface
		if (changedProperties.contains(&interfaceFontFamily) || changedProperties.contains(&interfaceFontSize)) {
#ifdef Q_OS_MAC
			// workaround for unwanted font changes when changing the desktop
			// https://sourceforge.net/tracker/?func=detail&aid=3559432&group_id=250595&atid=1126426
			QApplication::setDesktopSettingsAware(false);
#endif
			QApplication::setFont(QFont(interfaceFontFamily, interfaceFontSize));
		}
		if (changedProperties.contains(&interfaceStyle) || changedProperties.contains(&modernStyle) || changedProperties.contains(&useTexmakerPalette)){
			if (interfaceStyle==tr("default")) interfaceStyle="";
			setInterfaceStyle();
		}
		// read checkbox and set logViewer accordingly
		if (changedProperties.contains(&tabbedLogView))
			emit tabbedLogViewChanged(tabbedLogView);
		if (newLeftPanelLayout != !confDlg->ui.checkBoxTabbedStructureView->isChecked()){
			newLeftPanelLayout = !confDlg->ui.checkBoxTabbedStructureView->isChecked();
			emit newLeftPanelLayoutChanged(newLeftPanelLayout);
		}
		//language
		if (language == tr("default")) language="";
		if (language!=lastLanguage) loadTranslations(language);
		
		// custom highlighting
		customEnvironments.clear();
		for(int i=0;i<confDlg->ui.twHighlighEnvirons->rowCount();i++){
			QString env=confDlg->ui.twHighlighEnvirons->item(i,0)->text();
			if(!env.isEmpty()){
				if (env.endsWith("*") && !env.endsWith("\\*"))
					env.replace(env.length()-1,1,"\\*");
				QComboBox *cb=qobject_cast<QComboBox*>(confDlg->ui.twHighlighEnvirons->cellWidget(i,1));
				customEnvironments.insert(env,cb->currentIndex());
			}
		}
		latexParser.customCommands.clear();
		for(int i=0;i<confDlg->ui.twCustomSyntax->rowCount();i++){
			QString cmd=confDlg->ui.twCustomSyntax->item(i,0)->text();
			if(!cmd.isEmpty())
				latexParser.customCommands.insert(cmd);
		}
		
	}
	delete confDlg;
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

void ConfigManager::activateInternalViewer(bool activated){
	if(!activated) return;
	QLineEdit *le = pdflatexEdit;
	REQUIRE(le);
	if(le->text().contains("synctex")) return;
	if (!txsConfirm(tr("To fully utilize the internal pdf-viewer, synctex has to be activated. Shall TeXstudio do it now?")))
		return;
	QString zw=le->text();
	zw.replace("pdflatex ","pdflatex -synctex=1 ",Qt::CaseSensitive);
	le->setText(zw);
}

void ConfigManager::updateRecentFiles(bool alwaysRecreateMenuItems) {
	QMenu* recentMenu = getManagedMenu("main/file/openrecent");
	if (alwaysRecreateMenuItems || (recentMenu->actions().count()!=maxRecentFiles+maxRecentProjects+4)) {
		QList<QAction*> actions=recentMenu->actions(); //recentMenu->clear() doesn't seem to delete the actions (why?)
		for (int i = 0; i< actions.count(); i++)
			recentMenu->removeAction(actions[i]); //neccessary or it crashes
		for (int i = 0; i < maxRecentProjects; ++i)
			newOrLostOldManagedAction(recentMenu, "p"+QString::number(i), tr("Recent 'Master Document' %1").arg(i), SLOT(fileOpenRecentProject()))->setVisible(false);
		recentMenu->addSeparator();
		for (int i = 0; i < maxRecentFiles; ++i)
			newOrLostOldManagedAction(recentMenu, QString::number(i), tr("Recent File %1").arg(i), SLOT(fileOpenRecent()))->setVisible(false);
		newOrLostOldManagedAction(recentMenu, "list", tr("File list"), SLOT(fileRecentList()));
		newOrLostOldManagedAction(recentMenu, "firstFile", tr("Open first non-open file"), SLOT(fileOpenFirstNonOpen()));
		newOrLostOldManagedAction(recentMenu, "allFiles", tr("&* Open all files"), SLOT(fileOpenAllRecent()));
	}
	
	for (int i=0; i < maxRecentProjects; i++) {
		QAction* act = getManagedAction(QString("main/file/openrecent/p%1").arg(i));
		REQUIRE(act);
		if (i<recentProjectList.count()) {
			act->setVisible(true);
			QString temp = recentProjectList.at(i);
			temp.replace("&", "&&");
			act->setText(tr("Master Document: ")+(i<=13?QString("&%1 ").arg((char)('M'+i)):"")+ temp);
			act->setData(recentProjectList.at(i));
		} else act->setVisible(false);
	}
	for (int i=0; i < maxRecentFiles; i++) {
		QAction* act = getManagedAction(QString("main/file/openrecent/%1").arg(i));
		REQUIRE(act);
		if (i<recentFilesList.count()) {
			act->setVisible(true);
			char schar = '\0';
			if (i+1 <= 9) schar = i+1+'0';
			else if (i+1 <= 9+12) schar = i+1 + 'a' - 10;
			else if (i+1 <= 21+9) schar = i+1 + '!' - 22;
			else if (i+1 <= 30+5) schar = i+1 + '+' - 31;
			else if (i+1 <= 35+7) schar = i+1 + ':' - 36;
			else if (i+1 <= 42+5) schar = i+1 + '[' - 43;
			else if (i+1 <= 47+4) schar = i+1 + '{' - 48;
			QString temp = recentFilesList.at(i);
			temp.replace("&","&&");
			act->setText((schar?QString("&%1 ").arg(schar):"")+temp);
			act->setData(recentFilesList.at(i));
		} else act->setVisible(false);
	}
}

QMenu* ConfigManager::updateListMenu(const QString& menuName, const QStringList& items, const QString& namePrefix, bool prefixNumber, const char* slotName, const int baseShortCut, bool alwaysRecreateMenuItems, int additionalEntries){
	QMenu* menu = getManagedMenu(menuName);
	REQUIRE_RET(menu, 0);
	Q_ASSERT(menu->objectName() == menuName);
	QList<QAction*> actions = menu->actions();
	if (!alwaysRecreateMenuItems &&
	              actions.count() == items.size() + additionalEntries) {
		//set only title
		for (int i = 0; i< items.size(); i++) {
			Q_ASSERT(actions[i]->objectName() == menuName + "/" + namePrefix + QString::number(i));
			actions[i]->setText(prefixNumber?QString("%1: %2").arg(i+1).arg(items[i]) : items[i]);
		}
		if (watchedMenus.contains(menuName))
			emit watchedMenuChanged(menuName);
		return 0;
	}
	//recreate
	for (int i = 0; i< actions.count(); i++)
		menu->removeAction(actions[i]); //neccessary or it crashes
	for (int i = 0; i<items.size(); i++){
		QString id = namePrefix + QString::number(i);
		QString completeId = menu->objectName()+"/"+ id;
		Q_ASSERT(completeId == menuName + "/" + namePrefix + QString::number(i));
		newOrLostOldManagedAction(menu, id, prefixNumber?QString("%1: %2").arg(i+1).arg(items[i]) : items[i], slotName,  (baseShortCut && i<10)?(QList<QKeySequence>() << baseShortCut + i): QList<QKeySequence>())->setData(i);
	}
	if (watchedMenus.contains(menuName))
		emit watchedMenuChanged(menuName);
	return menu;
}

void ConfigManager::updateUserMacroMenu(bool alwaysRecreateMenuItems){
	QStringList macronames;
	// remove quote replacement from list
	for(int i=0;i<completerConfig->userMacro.count();i++){
		Macro m=completerConfig->userMacro.at(i);
		if(m.name==TXS_AUTO_REPLACE_QUOTE_OPEN || m.name==TXS_AUTO_REPLACE_QUOTE_CLOSE){
			completerConfig->userMacro.removeAt(i);
			i--;
		}
	}
	
	
	foreach (const Macro&m , completerConfig->userMacro)
		if (!m.document)
			macronames<<m.name;
	
	
	QMenu* recreatedMenu = updateListMenu("main/macros", macronames, "tag", true, SLOT(insertUserTag()), Qt::SHIFT+Qt::Key_F1, alwaysRecreateMenuItems);
	if (recreatedMenu) {
		recreatedMenu->addSeparator();
		newOrLostOldManagedAction(recreatedMenu, "manage",QCoreApplication::translate("Texmaker", "Edit &Macros..."), SLOT(editMacros()));
	}
	// update quote replacement
	static const char * open[7] = {"",  "``", "\"<", "\"`", "\\og ",  "\">", "\\enquote{"};
	static const char * close[7] = {"", "''", "\">", "\"'", "\\fg{}", "\"<", "}"};
	if (replaceQuotes >= 1 && replaceQuotes < 7) {
		completerConfig->userMacro.append(Macro(TXS_AUTO_REPLACE_QUOTE_OPEN, open[replaceQuotes], "", "(?language:latex)(?<=\\s|[(:]|^)\""));
		completerConfig->userMacro.append(Macro(TXS_AUTO_REPLACE_QUOTE_CLOSE, close[replaceQuotes], "", "(?language:latex)(?<=\\S)\""));
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
	REQUIRE_RET(menu, 0);
	QString menuId = menu->objectName();
	QString completeId = menu->objectName()+"/"+ id;
	
	QAction *old=menuParent->findChild<QAction*>(completeId);
	if (old) {
		old->setText(text);
		if (!iconFile.isEmpty()) old->setIcon(getRealIcon(iconFile));
		if (watchedMenus.contains(menuId))
			emit watchedMenuChanged(menuId);
		//don't set shortcut and slot!
		return old;
	}
	
	QAction *act;
	if (iconFile.isEmpty()) act=new QAction(text, menuParent);
	else act=new QAction(getRealIcon(iconFile), text, menuParent);
	
	act->setObjectName(completeId);
	act->setShortcuts(shortCuts);
	if (slotName) {
		connect(act, SIGNAL(triggered()), menuParent, slotName);
		act->setProperty("primarySlot", QString::fromLocal8Bit(slotName));
	}
	menu->addAction(act);
	for (int i=0; i<shortCuts.size(); i++)
		managedMenuShortcuts.insert(act->objectName()+QString::number(i),shortCuts[i]);
	if (watchedMenus.contains(menuId))
		emit watchedMenuChanged(menuId);
	return act;
}

//creates a new action or reuses an existing one (an existing one that is currently not in any menu, but has been in the given menu)
QAction* ConfigManager::newOrLostOldManagedAction(QWidget* menu, const QString &id,const QString &text, const char* slotName, const QList<QKeySequence> &shortCuts, const QString & iconFile){
	QAction* old = menuParent->findChild<QAction*>(menu->objectName() + "/" + id);
	if (!old)
		return newManagedAction(menu, id, text, slotName, shortCuts, iconFile);
	menu->addAction(old);
	old->setText(text);
	if (watchedMenus.contains(menu->objectName()))
		emit watchedMenuChanged(menu->objectName());
	return old;
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
QAction* ConfigManager::getManagedAction(const QString& id) {
	QAction* act=0;
	if (menuParent) act=menuParent->findChild<QAction*>(id);
	if (act==0) qWarning("Can't find internal action %s",id.toAscii().data());
	return act;
}
QList<QAction *> ConfigManager::getManagedActions(const QStringList& ids, const QString &commonPrefix) {
	QList<QAction *> actions;
	if (!menuParent) {
		qWarning("Can't find internal actions: menuParent missing.");
		return actions;
	}
	QAction *act;
	foreach(const QString& id, ids) {
		act=menuParent->findChild<QAction*>(commonPrefix+id);
		if (act==0) qWarning("Can't find internal action %s",id.toAscii().data());
		else actions << act;
	}
	return actions;
}
QMenu* ConfigManager::getManagedMenu(const QString& id) {
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
void ConfigManager::triggerManagedAction(const QString& id){
	QAction* act = getManagedAction(id);
	if (act) act->trigger();
}

QList<QVariant> parseCommandArguments (const QString& str) {
	QString s = str;
	QList<QVariant> result;
	if (str == "()") return result;
	s.remove(0,1);
	//                            1/-----2---\  /----3----\  /4-/5/6---------6\5\4--4\ 0
	static const QRegExp args("^ *((-? *[0-9]+)|(true|false)|(\"(([^\"]*|\\\\\")*)\")) *,?");
	while (args.indexIn(s) != -1) {
		if (!args.cap(2).isEmpty()) result << args.cap(2).toInt();
		else if (!args.cap(3).isEmpty()) result << (args.cap(3) == "true");
		else if (!args.cap(5).isEmpty()) result << (args.cap(5).replace("\\\"", "\"").replace("\\n", "\n").replace("\\t", "\t").replace("\\\\", "\\"));		
		s.remove(0, args.matchedLength());
	}
	return result;
}

void ConfigManager::connectExtendedSlot(QAction* act, const QString& slot){
	static const char * signal = SIGNAL(triggered());
	if (act->property("primarySlot").isValid()) 
		disconnect(act, signal, menuParent, act->property("primarySlot").toByteArray().data());
	
	if (slot.startsWith('1') || slot.startsWith('2')) {
		act->setProperty("primarySlot", slot);
		connect(act, signal, menuParent, slot.toLocal8Bit());
		return;
	}

	if (slot.endsWith("()") && !slot.contains(':')) {
		QString temp = "1" + slot;
		act->setProperty("primarySlot", temp);
		connect(act, signal, menuParent, temp.toLocal8Bit());
		return;
	}

	int argSeparator = slot.indexOf('(');
	REQUIRE(argSeparator >= 0);
	
	QString slotName = slot.mid(0, argSeparator);	
	QString args = slot.mid(argSeparator);
	if (slotName.contains(":")) {
		act->setProperty("editorSlot", QVariant());
		act->setProperty("editorViewSlot", QVariant());
		if (slotName.startsWith("editorView:")) 
			act->setProperty("editorViewSlot", slotName.mid(strlen("editorView:")));
		else if (slotName.startsWith("editor:")) 
			act->setProperty("editorSlot", slotName.mid(strlen("editor:")));
		else REQUIRE(false);
		slotName = SLOT(relayToEditorSlot());
	} else {
		act->setProperty("slot", slotName);
		slotName = SLOT(relayToOwnSlot());
	}
	act->setProperty("primarySlot", slotName);
	connect(act, signal, menuParent, slotName.toLocal8Bit());
	act->setProperty("args", parseCommandArguments(args));
}
	
QString prettySlotName(QAction* act) {
	QString primary = act->property("primarySlot").toString();
	if (primary.startsWith("1")) primary = primary.mid(1);
	if (primary == "relayToOwnSlot()" || primary == "relayToEditorSlot()") {
		if (primary == "relayToEditorSlot()") {
			if (act->property("editorViewSlot").isValid()) primary = "editorView:" + act->property("editorViewSlot").toString();
			else if (act->property("editorSlot").isValid()) primary = "editor:" + act->property("editorSlot").toString();
		} else primary = act->property("slot").toString();
		primary += "(";
		QList<QVariant> args = act->property("args").value<QList<QVariant> >();
		for (int i=0;i<args.size();i++) {
			if (i!=0) primary += ", ";
			if (args[i].type() == QVariant::String) primary += '"';
			primary += args[i].toString();
			if (args[i].type() == QVariant::String) primary += '"';
		}
		primary += ")";
	}
	return primary;
}


void ConfigManager::modifyMenuContents(){
	QStringList ids = manipulatedMenus.keys();
	while (!ids.isEmpty()) modifyMenuContent(ids, ids.first());
	modifyMenuContentsFirstCall = false;
}

void ConfigManager::modifyMenuContent(QStringList& ids, const QString& id){
	REQUIRE(menuParent);
	int index = ids.indexOf(id);
	if (index < 0) return;
	ids.removeAt(index);
	
	QMap<QString, QVariant>::const_iterator i = manipulatedMenus.find(id);
	if (i == manipulatedMenus.end()) return;
	
	
	QStringList m = i.value().toStringList();
	//qDebug() << id << ": ===> " << m.join(", ");
	QAction * act = menuParent->findChild<QAction*>(id);
	QMenu* mainMenu = 0;
	if (!act) {
		mainMenu = menuParent->findChild<QMenu*>(id);
		if (mainMenu) act = mainMenu->menuAction();
	}
	bool  newlyCreated = false;
	if (!act && m.value(3, "") != "") {
		newlyCreated = true;
		QString before = m.value(3);
		modifyMenuContent(ids, before);
		QAction * prevact = 0;
		if (!before.endsWith('/')) 
			prevact = menuParent->findChild<QAction*>(before);
		else {
			before.chop(1);
			QMenu* temp = menuParent->findChild<QMenu*>(before);
			if (temp) prevact = temp->menuAction();
		}
		QString menuName = before.left(before.lastIndexOf("/"));
		QMenu* menu = menuParent->findChild<QMenu*>(menuName);
		if (!menu) { modifyMenuContent(ids, menuName + "/"); menu = menuParent->findChild<QMenu*>(menuName); }
		if (!menu) return;
		
		Q_ASSERT(!prevact || menu->actions().contains(prevact));
		QStringList temp = id.split('/'); 
		if (temp.size() < 2) return;
		if (id.endsWith('/')) {
			QMenu* newMenu = newManagedMenu(menu, temp[temp.size()-2], m.first());
			act = newMenu->menuAction();
		} else {
			QString ownId = temp.takeLast();
			QByteArray defSlot = menu->property("defaultSlot").toByteArray(); 
			if (m.value(4).isEmpty()) {
				while (defSlot.isEmpty() && temp.size() >= 2) {
					temp.removeLast();
					QMenu* tempmenu = menuParent->findChild<QMenu*>(temp.join("/"));
					if (!tempmenu) continue;
					defSlot = tempmenu->property("defaultSlot").toByteArray();
				}
			}
			act = newManagedAction(menu, ownId, m.first(), defSlot.isEmpty()?0:defSlot.data());
		}
		if  (prevact) {
			menu->removeAction(act);
			menu->insertAction(prevact, act);
		}
	}
	if (!act) return;
	bool visible = !(m.value(2,"visible")=="hidden");
	if (modifyMenuContentsFirstCall && !newlyCreated && visible && act->text() == m.first() && act->data().toString() == m.at(1))
		manipulatedMenus.remove(mainMenu ? mainMenu->objectName() : act->objectName());
	act->setText(m.first());
	act->setData(m.at(1));
	act->setVisible(visible);
	if (!m.value(4).isEmpty()) {
		if (!act->property("originalSlot").isValid())
			act->setProperty("originalSlot", prettySlotName(act));
		connectExtendedSlot(act, m.value(4));
	} else if (act->property("originalSlot").isValid())
		connectExtendedSlot(act, act->property("originalSlot").toString());
	
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
		if (act) setManagedShortCut(act, num, managedMenuNewShortcuts[i].second);
	}
}

void ConfigManager::setManagedShortCut(QAction* act, int num, const QKeySequence& ks){
	REQUIRE(act);
	
	QList<QKeySequence> shortcuts = act->shortcuts();
	
	int oldIndex = -1;
	for (int i=0;i<shortcuts.size();i++)
		if (shortcuts[i].matches(ks) == QKeySequence::ExactMatch) 
			oldIndex = i;
	
	if (oldIndex != -1) {
		if (oldIndex <= num) 
			return;
		if (oldIndex > num) //allow to remove the first shortcut, by setting it to the second one
			shortcuts.removeAt(oldIndex);
	}
	if (num < shortcuts.size()) shortcuts[num] = ks;
	else shortcuts << ks;
	act->setShortcuts(shortcuts);
}

void ConfigManager::loadManagedMenu(QMenu* parent,const QDomElement &f) {
	QMenu *menu = newManagedMenu(parent,f.attributes().namedItem("id").nodeValue(),tr(qPrintable(f.attributes().namedItem("text").nodeValue())));
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
			                               tr(qPrintable(att.namedItem("text").nodeValue())),slotfunc,
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
			QTreeWidgetItem* twi=new QTreeWidgetItem(menuitem, QStringList() << acts[i]->text().replace("&","")
			                                         << managedMenuShortcuts.value(acts[i]->objectName()+"0", QKeySequence())
			                                         << acts[i]->shortcut().toString(QKeySequence::NativeText));
			if (!acts[i]->isSeparator()) {
				twi->setIcon(0,acts[i]->icon());
				twi->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
			} else {
				twi->setIcon(0,QIcon(":/images/separator.png"));
			}
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
			act->setShortcuts(QList<QKeySequence>());
			for (int num = 0; num < 2; num++){
				QString mseq = item->text(2+num);
				QString ns = QString::number(num);
				if(mseq==tr("<none>")) mseq="";
				if(mseq==tr("<default>")) mseq=managedMenuShortcuts.value(act->objectName()+ns,QKeySequence()).toString(QKeySequence::NativeText);
				QKeySequence sc(mseq);
				setManagedShortCut(act, num, sc);
				if (sc!=managedMenuShortcuts.value(act->objectName()+ns,QKeySequence()))
					managedMenuNewShortcuts.append(QPair<QString, QString> (id+ "~" + ns, sc.toString(QKeySequence ::PortableText)));
			}
			//todo: what is this?
			if(id=="main/view/outputview"){  // special handling for outputview because of "esc"-key
				if (item->text(2).isEmpty() || (act->shortcut().matches(Qt::Key_Escape) == QKeySequence::ExactMatch)) act->setShortcutContext(Qt::WidgetShortcut);
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
	QString txsTranslationFile=findResourceFile("texstudio_"+locale+".qm");
	if (txsTranslationFile == "")
		txsTranslationFile=findResourceFile("texmakerx_"+locale+".qm");
	//if (txsTranslationFile!="") {
	appTranslator->load(txsTranslationFile);
	basicTranslator->load(findResourceFile("qt_"+locale+".qm"));
	//}
}

void ConfigManager::setInterfaceStyle(){
	//style is controlled by the properties interfaceStyle, modernStyle and useTexmakerPalette
	//default values are read from systemPalette and defaultStyleName
	
	QString newStyle=interfaceStyle!=""?interfaceStyle:defaultStyleName;
#if QT_VERSION >= 0x040500
	if (modernStyle) {
		ManhattanStyle* style=new ManhattanStyle(newStyle);
		if (style->isValid()) QApplication::setStyle(style);
	} else 
#endif
		QApplication::setStyle(newStyle);
	QPalette pal = systemPalette;
	if (useTexmakerPalette){ //modify palette like texmaker does it
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
		
		pal.setColor( QPalette::Active, QPalette::Text, QColor("#000000") );
		pal.setColor( QPalette::Inactive, QPalette::Text, QColor("#000000") );
		pal.setColor( QPalette::Disabled, QPalette::Text, QColor("#000000") );
		
		pal.setColor(QPalette::Active, QPalette::ButtonText, QColor("#000000"));
		pal.setColor(QPalette::Inactive, QPalette::ButtonText, QColor("#000000"));
		pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#000000"));
		
		pal.setColor( QPalette::ToolTipText, QColor("#000000") );
		
		pal.setColor( QPalette::ToolTipBase, QColor("#FFFFDC") );
		
		if (x11desktop_env() ==4) {
			pal.setColor(QPalette::Active, QPalette::Window, QColor("#eae9e9"));
			pal.setColor(QPalette::Inactive, QPalette::Window, QColor("#eae9e9"));
			pal.setColor(QPalette::Disabled, QPalette::Window, QColor("#eae9e9"));
			
			pal.setColor(QPalette::Active, QPalette::Button, QColor("#eae9e9"));
			pal.setColor(QPalette::Inactive, QPalette::Button, QColor("#eae9e9"));
			pal.setColor(QPalette::Disabled, QPalette::Button, QColor("#eae9e9"));
		} else {
			/*pal.setColor(QPalette::Active, QPalette::Window, QColor("#fbf8f1"));
   pal.setColor(QPalette::Inactive, QPalette::Window, QColor("#fbf8f1"));
   pal.setColor(QPalette::Disabled, QPalette::Window, QColor("#fbf8f1"));
   
   pal.setColor(QPalette::Active, QPalette::Button, QColor("#fbf8f1"));
   pal.setColor(QPalette::Inactive, QPalette::Button, QColor("#fbf8f1"));
   pal.setColor(QPalette::Disabled, QPalette::Button, QColor("#fbf8f1"));*/
			pal.setColor( QPalette::Active, QPalette::Window, QColor("#f6f3eb") );
			pal.setColor( QPalette::Inactive, QPalette::Window, QColor("#f6f3eb") );
			pal.setColor( QPalette::Disabled, QPalette::Window, QColor("#f6f3eb") );
			
			pal.setColor( QPalette::Active, QPalette::Button, QColor("#f6f3eb") );
			pal.setColor( QPalette::Inactive, QPalette::Button, QColor("#f6f3eb") );
			pal.setColor( QPalette::Disabled, QPalette::Button, QColor("#f6f3eb") );
			
		}
	}
	QApplication::setPalette(pal);
}

/*! GridLayout::rowCount() apparently never decreases. Instead there may be empty rows at the end
	Therefore we manually keep track of the actual count of command rows
	*/
int ConfigManager::userCommandRowCount() {
	int index = userGridLayout->indexOf(userGridLayout->property(PROPERTY_ADD_BUTTON).value<QPushButton*>());
	if (index < 0) return 0;
	int r, unused;
	userGridLayout->getItemPosition(index, &r, &unused, &unused, &unused);
	return r;
}

void ConfigManager::addCommandRow(QGridLayout* gl, const CommandInfo& cmd, int row){
	static QStringList simpleMetaOptions = QStringList() << "quick" << "compile" << "view" << "view-pdf" << "bibliography";
	QWidget * parent = gl->parentWidget();

	// ID
	QWidget *nameWidget;
	if (cmd.user) nameWidget = new QLineEdit(cmd.id+":"+cmd.displayName, parent);
	else {
        QString lbl=qApp->translate("BuildManager",qPrintable(cmd.displayName));
		nameWidget = new QLabel(lbl, parent);
		if (configShowAdvancedOptions) nameWidget->setToolTip("ID: txs:///"+cmd.id);
		nameWidget->setProperty(PROPERTY_COMMAND_ID, cmd.id);
	}
	nameWidget->setProperty(PROPERTY_WIDGET_TYPE, CG_ID);


	// cmd Widget
	QWidget* cmdWidget;
	if (cmd.metaSuggestionList.isEmpty()) {
		cmdWidget = new QLineEdit(cmd.getPrettyCommand(), parent);
		if (cmd.id == "pdflatex") pdflatexEdit = qobject_cast<QLineEdit*>(cmdWidget);
	} else {
		cmdWidget = new QComboBox(parent);
		cmdWidget->setObjectName(cmd.id);
        if(!configShowAdvancedOptions && simpleMetaOptions.contains(cmd.id) && cmd.metaSuggestionList.contains(cmd.getPrettyCommand())){
            foreach(QString elem,cmd.simpleDescriptionList){
                elem=qApp->translate("BuildManager",qPrintable(elem));
				static_cast<QComboBox*>(cmdWidget)->addItem(elem);
            }
			static_cast<QComboBox*>(cmdWidget)->setEditable(false);
#ifndef QT_NO_DEBUG
			int i=cmd.metaSuggestionList.indexOf(cmd.getPrettyCommand());
            Q_ASSERT(i>=0);
#endif
            //static_cast<QComboBox*>(w)->setEditText();
        }else{
			static_cast<QComboBox*>(cmdWidget)->addItems(cmd.metaSuggestionList);
			static_cast<QComboBox*>(cmdWidget)->setEditable(true);
			static_cast<QComboBox*>(cmdWidget)->setEditText(cmd.getPrettyCommand());
        }

		int index = cmd.metaSuggestionList.indexOf(cmd.commandLine);
		if (index >= 0) static_cast<QComboBox*>(cmdWidget)->setCurrentIndex(index);
	}
	assignNameWidget(cmdWidget, nameWidget);
	cmdWidget->setProperty(PROPERTY_WIDGET_TYPE, CG_CMD);

	QList<QPushButton*> buttons;

	QPushButton *pb;
	if (cmd.user || cmd.meta) {
		pb = new QPushButton(QIcon(":/images/configure.png"), QString(), parent);
		pb->setToolTip(tr("Configure"));
		pb->setProperty(PROPERTY_WIDGET_TYPE, CG_CONFIG);
		connect(pb, SIGNAL(clicked()), SLOT(editCommand()));
		buttons << pb;
	}

	pb = new QPushButton(getRealIcon("fileopen"), "", parent);
	pb->setToolTip(tr("Select Program"));
	pb->setProperty(PROPERTY_WIDGET_TYPE, CG_PROGRAM);
	connect(pb, SIGNAL(clicked()), SLOT(browseCommand()));
	buttons << pb;

	if (!cmd.user && cmd.metaSuggestionList.isEmpty()) {
		pb = new QPushButton(getRealIcon("undo"), "", parent);
		pb->setToolTip(tr("Restore Default"));
		pb->setProperty(PROPERTY_WIDGET_TYPE, CG_RESET);
		connect(pb, SIGNAL(clicked()), SLOT(undoCommand()));
		buttons << pb;
	}
	if (cmd.user) {
		pb = new QPushButton(getRealIcon("list-remove"), "", parent);
		pb->setProperty(PROPERTY_WIDGET_TYPE, CG_DEL);
		connect(pb,SIGNAL(clicked()),SLOT(removeCommand()));
		buttons << pb;
		pb = new QPushButton(getRealIcon("up"), "", parent);
		if (row == 0) pb->setEnabled(false);
		pb->setProperty(PROPERTY_WIDGET_TYPE, CG_MOVEUP);
		connect(pb,SIGNAL(clicked()),SLOT(moveUpCommand()));
		buttons << pb;
		pb = new QPushButton(getRealIcon("down"), "", parent);
		pb->setProperty(PROPERTY_WIDGET_TYPE, CG_MOVEDOWN);
		connect(pb,SIGNAL(clicked()),SLOT(moveDownCommand()));
		buttons << pb;
	}
	bool advanced = cmd.meta && !simpleMetaOptions.contains(cmd.id);
	QList<QWidget*> temp; temp << nameWidget << cmdWidget; foreach (QWidget* w, buttons) temp << w;
	foreach (QWidget* x, temp) {
		x->setMinimumHeight(x->sizeHint().height());
		if (x != cmdWidget) x->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
		advanced |= (cmd.user && buttons.indexOf(static_cast<QPushButton*>(x)) >= 3);
		x->setProperty("advancedOption", advanced);
		if (advanced && !configShowAdvancedOptions) x->setVisible(false);
	}
	cmdWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
	gl->setRowStretch(row, 1);
	gl->addWidget(nameWidget, row, CG_ID);
	int off = 1;

	// rerun button
	static QStringList rerunnable = QStringList() << "latex" << "pdflatex" << "lualatex" << "xelatex" << "quick" << "compile" << "ps-chain" << "dvi-chain" << "pdf-chain" << "dvi-pdf-chain" << "dvi-ps-pdf-chain" << "asy-dvi-chain" << "asy-pdf-chain" << "pre-compile" << "internal-pre-compile" << "recompile-bibliography";
	if (cmd.user || rerunnable.contains(cmd.id)) {
		QIcon icon;
		pb = new QPushButton();
		icon.addFile(":/images/repeat-compile.png", QSize(), QIcon::Normal, QIcon::On);
		icon.addFile(":/images/repeat-compile-off.png", QSize(), QIcon::Normal, QIcon::Off);
		pb->setIcon(icon);
		pb->setToolTip("Repeat contained compilation commands");
		pb->setCheckable(true);
		pb->setChecked(cmd.rerunCompiler);
		assignNameWidget(pb, nameWidget);
		pb->setProperty(PROPERTY_WIDGET_TYPE, CG_RERUN);
		pb->setProperty("advancedOption", true);
        if (!configShowAdvancedOptions) pb->setVisible(false);
		gl->addWidget(pb, row, CG_RERUN, 1, 1);
		if (!cmd.user)
			rerunButtons << pb;
	}

	gl->addWidget(cmdWidget,row,1+off,1,1);
	for (int i = 0; i < buttons.size(); i++){
		gl->addWidget(buttons[i],row,2+off+i, 1, 1);
		buttons[i]->setProperty(PROPERTY_ASSOCIATED_INPUT, QVariant::fromValue<QWidget*>(cmdWidget));
		assignNameWidget(buttons[i], nameWidget);
	}

	QPushButton *addButton = gl->property(PROPERTY_ADD_BUTTON).value<QPushButton*>();
	if (cmd.user && addButton)
		QWidget::setTabOrder(buttons.last(), addButton);

	if (!cmd.user)
		commandInputs << cmdWidget;
}

void ConfigManager::createCommandList(QGroupBox* box, const QStringList& order, bool user, bool meta){
	QVBoxLayout *verticalLayout = new QVBoxLayout(box);
	QScrollArea *scrollAreaCommands = new QScrollArea(box);
	scrollAreaCommands->setWidgetResizable(true);
	QWidget *scrollAreaWidgetContents = new QWidget();
	QGridLayout* gl=new QGridLayout(scrollAreaWidgetContents);
	int row = 0;
	foreach (const QString& id, order){
		const CommandInfo& cmd = tempCommands.value(id);
        //bool isMeta = !cmd.metaSuggestionList.isEmpty();
        bool isMeta = cmd.meta;
		if (user != cmd.user) continue;
		if (!user && (isMeta != meta)) continue;
		addCommandRow(gl, cmd, row);
		row++;
	}
	if (user){
		QPushButton *addButton = new QPushButton(getRealIcon("list-add"), tr("Add"), box);
		addButton->setProperty(PROPERTY_WIDGET_TYPE, CG_ADD);
		addButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		connect(addButton, SIGNAL(clicked()), SLOT(addCommand()));
		gl->addWidget(addButton, row, 0, 1, 1, Qt::AlignLeft|Qt::AlignTop);
		userGridLayout = gl;
		setLastRowMoveDownEnable(false);
		gl->setProperty(PROPERTY_ADD_BUTTON, QVariant::fromValue<QPushButton*>(addButton));
	}
	//gl->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), row, CG_RERUN);

	scrollAreaCommands->setWidget(scrollAreaWidgetContents);
	verticalLayout->addWidget(scrollAreaCommands);
}

void ConfigManager::setFirstRowMoveUpEnable(bool enable){
	REQUIRE(userGridLayout);
	int rows = userCommandRowCount();
	for (int i=0; i < rows;i++) {
		QLayoutItem* li = userGridLayout->itemAtPosition(i, 6);
		if (li && li->widget()) { li->widget()->setEnabled(enable); break; }
	}
}

void ConfigManager::setLastRowMoveDownEnable(bool enable){
	REQUIRE(userGridLayout);
	int rows = userCommandRowCount();
	for (int i=rows-1; i >= 0; i--) {
		QLayoutItem* li = userGridLayout->itemAtPosition(i, 7);
		if (li && li->widget()) { li->widget()->setEnabled(enable); break; }
	}
}

void ConfigManager::browseCommand(){
	QPushButton *pb = qobject_cast<QPushButton*> (sender());
	REQUIRE(pb);
	QWidget *w = pb->property(PROPERTY_ASSOCIATED_INPUT).value<QWidget*>();
	REQUIRE(w);
	QString old = getText(w);
	QString path = old;
	if (old.contains("|")) {
		path = old.split("|").last().trimmed();
		if (path.isEmpty()) path = old.split("|").first().trimmed();
	}
	path = path.trimmed();
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
		location="\""+location+"\" "+ tempCommands.value(getCmdID(w)).defaultArgs;
		if (old.contains("|")) setText(w, old + (old.trimmed().endsWith("|")?"":" | ") + location);
		else setText(w, location);
	}
}
void ConfigManager::undoCommand(){
	QPushButton *pb = qobject_cast<QPushButton*> (sender());
	REQUIRE(pb);
	QWidget *w = pb->property(PROPERTY_ASSOCIATED_INPUT).value<QWidget*>();
	REQUIRE(w);
	setText(w, tempCommands.value(getCmdID(w)).guessCommandLine());
}

void ConfigManager::editCommand(){
	QPushButton *pb = qobject_cast<QPushButton*> (sender());
	REQUIRE(pb);
	QWidget *w = pb->property(PROPERTY_ASSOCIATED_INPUT).value<QWidget*>();
	REQUIRE(w);
	setText(w, buildManager->editCommandList(getText(w), getCmdID(w)));
}

void ConfigManager::addCommand(){
	QPushButton* self = qobject_cast<QPushButton*>(sender());
	REQUIRE(self); REQUIRE(userGridLayout);
	CommandInfo cmd;

	// make sure the ID is unique
	QStringList currentUserCmdIDs;
	for (int i=0; i<userGridLayout->count(); i++) {
		QWidget *nameWidget = userGridLayout->itemAt(i)->widget();
		if (!nameWidget || !nameWidget->property(PROPERTY_WIDGET_TYPE).toInt() == CG_ID) continue;
		currentUserCmdIDs << getCmdID(nameWidget);
	}
	for (int i=0; i<currentUserCmdIDs.count()+1; i++) {
		QString id = QString("user%1").arg(i);
		if (!currentUserCmdIDs.contains(id)) {
			cmd.id = id;
			break;
		}
	}

	cmd.user = true;
	setLastRowMoveDownEnable(true);

	int row, c, dr, dc;
	userGridLayout->getItemPosition(userGridLayout->count()-1, &row, &c, &dr, &dc);

	userGridLayout->removeWidget(self);
	addCommandRow(userGridLayout, cmd, row);
	userGridLayout->addWidget(self, row+1, 0);
	setLastRowMoveDownEnable(false);
}

void ConfigManager::removeCommand() {
	// deleting widget from within the grid causes layout problems because of empty rows
	// because we don't want to repopulate the whole table, we move the command to delete to the last row and delete it there
	// using moveCommand is not best in performance, but easy and safe and we're not performance critical here anyway
	QPushButton* self = qobject_cast<QPushButton*>(sender()); REQUIRE(self); REQUIRE(userGridLayout);

	userGridLayout->parentWidget()->setUpdatesEnabled(false);

	int row, col, unused;
	userGridLayout->getItemPosition(userGridLayout->indexOf(self), &row, &col, &unused, &unused ); REQUIRE(row >= 0);

	int rows = userCommandRowCount();
	for (int r = row; r<rows-1; r++) {
		moveCommand(+1, r);
	}

	QWidget *nameWidget = userGridLayout->itemAtPosition(rows-1, 0)->widget();
	QString cmdID(getCmdID(nameWidget));

	int index=userGridLayout->indexOf(nameWidget);
	while (index+1 < userGridLayout->count()) {
		QLayoutItem *li = userGridLayout->itemAt(index+1);
		QWidget *w = li->widget();
		if (w && nameWidget != li->widget()->property(PROPERTY_NAME_WIDGET).value<QWidget*>()) break;
		userGridLayout->removeItem(li);
		if (w) delete w;
		delete li;
	}
	delete userGridLayout->takeAt(index);
	delete nameWidget;

	// add button and spacer
	QPushButton *addButton = userGridLayout->property(PROPERTY_ADD_BUTTON).value<QPushButton*>();
	userGridLayout->removeWidget(addButton);
	userGridLayout->addWidget(addButton, rows-1, 0, 1, 1);
	/*col = 0;
	for (int i=index; i<userGridLayout->count(); i++) {
		QLayoutItem *li = userGridLayout->takeAt(index);
		qDebug() << li->widget();
		userGridLayout->addItem(li, rows-1, col++, 1, 1);
	}*/
	qDebug() << rows << userCommandRowCount();

	/*for (int i=0; i<userGridLayout->count(); i++) {
		int r, c, unused;
		userGridLayout->getItemPosition(i, &r, &c, &unused, &unused);
		qDebug() << i << r << c;
	}*/

	setLastRowMoveDownEnable(false);
	setFirstRowMoveUpEnable(false);

	userGridLayout->parentWidget()->setUpdatesEnabled(true);
}


void exchangeProperties(QWidget *w, QWidget *w2) {
	if (!w || !w2) return;

	QLineEdit *le;
	QComboBox *cb;
	QPushButton *pb;
	if ( (le = qobject_cast<QLineEdit *>(w)) ) {
		QLineEdit *le2 = qobject_cast<QLineEdit *>(w2);
		QString s = le->text();
		le->setText(le2->text());
		le2->setText(s);
	} else if ( (cb = qobject_cast<QComboBox *>(w)) ) {
		QComboBox *cb2 = qobject_cast<QComboBox *>(w2);
		QString cbCurrent = cb->currentText();
		QStringList cbTexts;
		for (int i=0; i<cb->count(); i++) {
			cbTexts << cb->itemText(i);
		}
		cb->clear();
		for (int i=0; i<cb2->count(); i++) {
			cb->addItem(cb2->itemText(i));
		}
		cb->setEditText(cb2->currentText());
		cb2->clear();
		cb2->addItems(cbTexts);
		cb2->setEditText(cbCurrent);
	} else if ((pb = qobject_cast<QPushButton *>(w)) && pb->isCheckable()) {
		QPushButton *pb2 = qobject_cast<QPushButton *>(w2);
		bool b = pb->isChecked();
		pb->setChecked(pb2->isChecked());
		pb2->setChecked(b);
	}
}
void ConfigManager::moveUpCommand(){ moveCommand(-1); }
void ConfigManager::moveDownCommand(){ moveCommand(+1); }
void ConfigManager::moveCommand(int dir, int atRow){
	if (atRow<0) {
		// determine row from sending button
		QPushButton* self = qobject_cast<QPushButton*>(sender()); REQUIRE(self); REQUIRE(userGridLayout);
		QWidget* w = self->property(PROPERTY_ASSOCIATED_INPUT).value<QWidget*>(); REQUIRE(w);
		int col, unused;
		userGridLayout->getItemPosition(userGridLayout->indexOf(self), &atRow, &col, &unused, &unused ); REQUIRE(atRow >= 0);
	}
	int cols = userGridLayout->columnCount();
	for (int c=0; c<cols; c++) {
		QLayoutItem* li = userGridLayout->itemAtPosition(atRow, c);
		QLayoutItem* li2 = userGridLayout->itemAtPosition(atRow+dir, c);
		Q_ASSERT(li && li2);
		QWidget *wd = li->widget();
		QWidget *wd2 = li2->widget();
		exchangeProperties(wd, wd2);
	}
	setLastRowMoveDownEnable(false);
	setFirstRowMoveUpEnable(false);
}

// manipulate latex menus
QTreeWidgetItem* ConfigManager::managedLatexMenuToTreeWidget(QTreeWidgetItem* parent, QMenu* menu) {
	if (!menu) return 0;
	static QStringList relevantMenus = QStringList() << "main/tools" << "main/latex" << "main/math";
	QTreeWidgetItem* menuitem= new QTreeWidgetItem(parent, QStringList(menu->title()));
	bool advanced = false;
	if (parent) advanced = parent->data(0, Qt::UserRole+2).toBool();
	else {
		menuitem->setData(0,Qt::UserRole,menu->objectName());
		if (manipulatedMenus.contains(menu->objectName())) {
			QFont bold = menuitem->font(0);
			bold.setBold(true);
			for (int j=0;j<3;j++) menuitem->setFont(j, bold);
		}
		if (!relevantMenus.contains(menu->objectName())) advanced = true;
	}
	if (advanced) { superAdvancedItems << menuitem; menuitem->setData(0,Qt::UserRole+2, true); }
	menuitem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
	menuitem->setCheckState(0, menu->menuAction() && menu->menuAction()->isVisible() ? Qt::Checked : Qt::Unchecked);
	if (menu->objectName().count("/")<=2) menuitem->setExpanded(true);
	QList<QAction *> acts=menu->actions();
	for (int i=0; i<acts.size(); i++){
		bool subAdvanced = advanced;
		QTreeWidgetItem* twi = 0;
		if (acts[i]->menu()) twi = managedLatexMenuToTreeWidget(menuitem, acts[i]->menu());
		else {
			subAdvanced |= !acts[i]->data().isValid();
			twi=new QTreeWidgetItem(menuitem, QStringList() << QString(acts[i]->text()) << acts[i]->data().toString() << prettySlotName(acts[i]));
			if (!acts[i]->isSeparator()) {
				twi->setIcon(0,acts[i]->icon());
				twi->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
				twi->setCheckState(0,acts[i]->isVisible() ? Qt::Checked : Qt::Unchecked);
			} else {
				twi->setIcon(0, QIcon(":/images/separator.png"));
			}
			twi->setData(2, Qt::UserRole, acts[i]->property("originalSlot").isValid()?acts[i]->property("originalSlot").toString():twi->text(2));
			if (manipulatedMenus.contains(acts[i]->objectName())) {
				QFont bold = twi->font(0);
				bold.setBold(true);
				for (int j=0;j<3;j++) twi->setFont(j, bold);
			}			
		}
		if (!twi) continue;
		QString id = acts[i]->menu()?(acts[i]->menu()->objectName() + "/"): acts[i]->objectName();
		twi->setData(0,Qt::UserRole,id);
		twi->setData(0,Qt::UserRole+2,subAdvanced);
		manipulatedMenuTree.insert(id, twi);
		if (subAdvanced) superAdvancedItems << twi;
		
		int j = i+1;
		for (; j < acts.size() && acts[j]->isSeparator(); j++) ;
		if (j < acts.size()) twi->setData(0,Qt::UserRole+1,acts[j]->menu() ? acts[j]->menu()->objectName(): acts[j]->objectName());
		else twi->setData(0,Qt::UserRole+1, menu->objectName() + "/");
	}
	if (acts.isEmpty()) {
		QTreeWidgetItem* filler=new QTreeWidgetItem(menuitem, QStringList() << QString("temporary menu end") << "");
		filler->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		filler->setData(0,Qt::UserRole, menu->objectName()+"/!!end");
	}
	return menuitem;
}

void ConfigManager::latexTreeItemChanged(QTreeWidgetItem* item,int ){
	if((item->flags() & Qt::ItemIsEditable) && !changedItemsList.contains(item)) {
		QFont f = item->font(0);
		f.setBold(true);
		for (int i=0;i<3;i++) item->setFont(i, f);
		changedItemsList.append(item);
	}
}
void ConfigManager::latexTreeNewItem(bool menu){
	QAction* a = qobject_cast<QAction*>(sender());
	REQUIRE(a);
	QTreeWidget* tw = qobject_cast<QTreeWidget*>(a->parentWidget());
	REQUIRE(tw);
	QTreeWidgetItem * old = tw->currentItem();
	//qDebug() << old->data(0, Qt::UserRole) << old->data(0, Qt::DisplayRole);
	REQUIRE(old);
	if (!old->parent()) return;
	QTreeWidgetItem* twi=new QTreeWidgetItem(QStringList() << QString("new item") << "");
	twi->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
	twi->setCheckState(0, Qt::Checked);
	static int ID = 0;
	QString oldID = old->data(0,Qt::UserRole).toString(), append = (menu?"/":"");
	if (oldID.endsWith("/")) oldID.chop(1);
	for (ID=0;ID<100000 && manipulatedMenuTree.contains(oldID+"_UII"+QString::number(ID)+append); ID++) ;
	QString newId = oldID + "_UII"+QString::number(ID)+append;
	twi->setData(0,Qt::UserRole, newId);
	twi->setData(0,Qt::UserRole+1, old->data(0,Qt::UserRole).toString());
	old->parent()->insertChild(old->parent()->indexOfChild(old), twi);
	manipulatedMenuTree.insert(newId, twi);
	latexTreeItemChanged(twi, 0);
	if (menu) {
		QTreeWidgetItem* filler=new QTreeWidgetItem(twi, QStringList() << QString("temporary menu end") << "");
		filler->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		filler->setData(0,Qt::UserRole, newId + "!!end");
	}
}
void ConfigManager::latexTreeNewMenuItem(){
	latexTreeNewItem(true);
}

void ConfigManager::toggleVisibleTreeItems(bool show){
	REQUIRE(!superAdvancedItems.isEmpty());
	foreach (QTreeWidgetItem* twi, superAdvancedItems)
		twi->setHidden(!show);
	QTreeWidget* tw = superAdvancedItems.first()->treeWidget();
	tw->setColumnHidden(2, !show);
	if (show && tw->columnWidth(0) + tw->columnWidth(1) + tw->columnWidth(2) > tw->width() + 50) 
		tw->setColumnWidth(1, tw->width() - tw->columnWidth(0) - tw->columnWidth(2));
}

void ConfigManager::treeWidgetToManagedLatexMenuTo() {
	foreach(QTreeWidgetItem* item,changedItemsList){
		QString id=item->data(0,Qt::UserRole).toString();
		if (id=="") continue;
		QStringList m;
		m << item->text(0) 
		  << item->text(1) 
		  << (item->checkState(0)==Qt::Checked?"visible":"hidden")
		  << item->data(0, Qt::UserRole+1).toString()
		  << ((item->text(2) != item->data(2, Qt::UserRole).toString())?item->text(2):"");
		manipulatedMenus.insert(id, m);
	}
	modifyMenuContents();	
}


void ConfigManager::registerOption(const QString& name, void* storage, PropertyType type, QVariant def, void* displayWidgetOffset){
	//#ifndef QT_NO_DEBUG
	//TODO: optimize
	for (int i=0;i<managedProperties.size();i++)
		if (managedProperties[i].name == name){
			if (managedProperties[i].storage == storage)
				return;
			qDebug() << "Duplicate option name" << name;
			Q_ASSERT(false);
		}
	//#endif
	ManagedProperty temp;
	temp.name = name;
	temp.storage = storage;
	temp.type = type;
	temp.def = def;
	temp.widgetOffset = (ptrdiff_t)displayWidgetOffset;
	managedProperties << temp;
	
	if (persistentConfig){
		persistentConfig->beginGroup("texmaker");
		temp.valueFromQVariant(persistentConfig->value(temp.name, temp.def));
		persistentConfig->endGroup();
	}
}

void ConfigManager::registerOption(const QString& name, void* storage, PropertyType type, QVariant def){
	registerOption(name, storage, type, def, 0);
}

#define REGISTER_OPTION(TYPE, ID) \
	void ConfigManager::registerOption(const QString& name, TYPE* storage, QVariant def,  void* displayWidgetOffset){ \
		registerOption(name, storage, ID, def, displayWidgetOffset); \
	} \
	void ConfigManager::registerOption(const QString& name, TYPE* storage, QVariant def){ \
		registerOption(name, storage, ID, def, 0); \
	}
PROPERTY_TYPE_FOREACH_MACRO(REGISTER_OPTION)
#undef REGISTER_OPTION


void ConfigManager::setOption(const QString& name, const QVariant& value){
	REQUIRE(persistentConfig);
	QString rname = name.startsWith("/") ? name.mid(1) : ("texmaker/"+name);
    ManagedProperty* option = 0;
	if (rname.startsWith("texmaker/") && ((option = getManagedProperty(rname.mid(9))))){
		option->valueFromQVariant(value);
		return;
	}
	persistentConfig->setValue(rname,value);
}
QVariant ConfigManager::getOption(const QString& name) const{
	REQUIRE_RET(persistentConfig,QVariant());
	QString rname = name.startsWith("/") ? name.mid(1) : ("texmaker/"+name);
    const ManagedProperty* option = 0;
	if (rname.startsWith("texmaker/") && (option = getManagedProperty(rname.mid(9))))
		return option->valueToQVariant();
	return persistentConfig->value(rname);
}

bool ConfigManager::existsOption(const QString& name) const{
	REQUIRE_RET(persistentConfig,false);
	QString rname = name.startsWith("/") ? name.mid(1) : ("texmaker/"+name);
	return persistentConfig->contains(rname);
}
void ConfigManager::linkOptionToDialogWidget(const void* optionStorage, QWidget* widget){
	ManagedProperty *property = getManagedProperty(optionStorage);
	REQUIRE(property);
	
	QWidget* parentWidget = widget->parentWidget();
	while (parentWidget && !qobject_cast<QDialog*>(parentWidget)) parentWidget = parentWidget->parentWidget();
	Q_ASSERT(parentWidget);
	QDialog* parentDialog = qobject_cast<QDialog*>(parentWidget);
	Q_ASSERT(parentDialog);
	
	if (managedOptionDialogs.contains(parentDialog)) {
		(*managedOptionDialogs.find(parentDialog)) << widget;
	} else {
		managedOptionDialogs.insert(parentDialog, QList<QWidget*>() << widget);
		connect(parentDialog, SIGNAL(accepted()), SLOT(managedOptionDialogAccepted()));
	}
	
	property->writeToObject(widget);
	widget->setProperty("managedProperty", QVariant::fromValue<void*>(property->storage));
}

void ConfigManager::linkOptionToObject(const void* optionStorage, QObject* object, LinkOptions options){
	ManagedProperty *property = getManagedProperty(optionStorage);
	REQUIRE(property);
	REQUIRE((options & LO_DIRECT_OVERRIDE) || property->type == PT_BOOL);
	if (managedOptionObjects.contains(property)) {
		Q_ASSERT(managedOptionObjects[property].first == options);
		managedOptionObjects[property].second << object;
	} else {
		managedOptionObjects.insert(property, QPair<LinkOptions, QList<QObject*> >(options, QList<QObject*>() << object));
	}
	property->writeToObject(object);
	object->setProperty("managedProperty", QVariant::fromValue<ManagedProperty*>(property));
	connect(object,SIGNAL(destroyed(QObject*)), SLOT(managedOptionObjectDestroyed(QObject*)));
	if (qobject_cast<QAction*>(object) || qobject_cast<QCheckBox*>(object))
		connect(object, SIGNAL(toggled(bool)), SLOT(managedOptionBoolToggled()));
}
void ConfigManager::updateAllLinkedObjects(const void* optionStorage){
	ManagedProperty *property = getManagedProperty(optionStorage);
	REQUIRE(property);
	updateManagedOptionObjects(property);
}

ManagedProperty* ConfigManager::getManagedProperty(const void* storage){
	for (int i=0;i<managedProperties.size();i++)
		if (managedProperties[i].storage == storage) return &managedProperties[i];
	return 0;
}
ManagedProperty* ConfigManager::getManagedProperty(const QString& name){
	for (int i=0;i<managedProperties.size();i++)
		if (managedProperties[i].name == name) return &managedProperties[i];
	return 0;
}
const ManagedProperty* ConfigManager::getManagedProperty(const QString& name) const{
	for (int i=0;i<managedProperties.size();i++)
		if (managedProperties[i].name == name) return &managedProperties[i];
	return 0;
}

void ConfigManager::getDefaultEncoding(const QByteArray&, QTextCodec*&guess, int &sure){
	if (sure >= 100) return;
	if (!newFileEncoding) return;
	//guess is utf-8 or latin1, no latex encoding definition detected
	if (guess && guess->mibEnum() == MIB_UTF8) return; //trust utf8 detection
	//guess is latin1, no latex encoding definition detected
	if (sure == 0) {
		//ascii file
		if (newFileEncoding->mibEnum() == MIB_UTF16BE || newFileEncoding->mibEnum() == MIB_UTF16LE) {
			guess = QTextCodec::codecForMib(MIB_UTF8);
			return;
		}
		guess = newFileEncoding;
		return;
	} else {
		//file containing invalid utf-8 characters
		if (newFileEncoding->mibEnum() == MIB_UTF16BE || newFileEncoding->mibEnum() == MIB_UTF16LE || newFileEncoding->mibEnum() == MIB_UTF8) {
			guess = QTextCodec::codecForMib(MIB_LATIN1);
			return;
		}
		guess = newFileEncoding;
		return;
	}
}

void ConfigManager::managedOptionDialogAccepted(){
	QDialog* dialog = qobject_cast<QDialog*>(sender());
	REQUIRE(dialog);
	foreach (const QWidget* widget, managedOptionDialogs.value(dialog, QList<QWidget*>())) {
		ManagedProperty* prop = getManagedProperty(widget->property("managedProperty").value<void*>());
		Q_ASSERT(prop);
		prop->readFromObject(widget);
	}
	managedOptionDialogs.remove(dialog);
}

void ConfigManager::managedOptionObjectDestroyed(QObject* obj){
	REQUIRE(obj);
	ManagedProperty* property = obj->property("managedProperty").value<ManagedProperty*>();
	REQUIRE(property);
	Q_ASSERT(managedOptionObjects.contains(property));
	Q_ASSERT(managedOptionObjects[property].second.contains(obj));
	managedOptionObjects[property].second.removeAll(obj);
	if (managedOptionObjects[property].second.isEmpty())
		managedOptionObjects.remove(property);
}

int isChecked(const QObject* obj){
	const QAction* act = qobject_cast<const QAction*>(obj);
	if (act) return (act->isChecked()?1:-1);
	const QCheckBox* cb = qobject_cast<const QCheckBox*>(obj);
	if (cb) return (cb->isChecked()?1:-1);
	return 0;
}

void ConfigManager::managedOptionBoolToggled(){
	int state = isChecked(sender());
	REQUIRE(state);
	ManagedProperty* property = sender()->property("managedProperty").value<ManagedProperty*>();
	REQUIRE(property);
	REQUIRE(property->type==PT_BOOL);
	if ((state > 0) == *(bool*)(property->storage)) return;
	if (managedOptionObjects[property].first & LO_DIRECT_OVERRIDE) {
		//full sync
		*(bool*)property->storage = (state > 0);
	} else {
		//voting
		int totalState=0;
		foreach (const QObject* o, managedOptionObjects[property].second)
			totalState += isChecked(o);
		if (totalState == 0) totalState = state;
		if ((totalState > 0) == *(bool*)(property->storage)) return;
		*(bool*)property->storage = (totalState > 0);
	}
	updateManagedOptionObjects(property);
}

void ConfigManager::updateManagedOptionObjects(ManagedProperty* property){
	if (!(managedOptionObjects[property].first & LO_UPDATE_ALL))
		return;
	foreach (QObject* o, managedOptionObjects[property].second)
		property->writeToObject(o);
}












