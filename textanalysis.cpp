#include "textanalysis.h"
#include "qdocumentline.h"
#include "smallUsefulFunctions.h"
//#include "latexeditorview.h"
//#include <QMessageBox>
Word::Word(QString nw, int nc) {
	word=nw;
	count=nc;
}
bool Word::operator<(const  Word &cmpTo) const {
	if (count > cmpTo.count) return true;
	else if (count < cmpTo.count) return false;
	else return word.localeAwareCompare(cmpTo.word)<0;
}

int TextAnalysisModel::rowCount(const QModelIndex & parent) const {
	if (parent.isValid()) return 0;
	else return words.size();
}
bool TextAnalysisModel::hasChildren(const QModelIndex & parent) const {
	return !parent.isValid();
}
QVariant TextAnalysisModel::data(const QModelIndex &index, int role) const {
	if (!index.isValid() || (index.parent().isValid()))
		return QVariant();

	if (index.row() >= words.size())
		return QVariant();

	if (role == Qt::DisplayRole) {
		if (index.column()==0) return words[index.row()].word;
		else if (index.column()==1)  return words[index.row()].count;
		else if (index.column()==2 && wordCount>0)  return QString::number(words[index.row()].count*relativeProzentMultipler,'g',3)+" %";
	}
	return QVariant();
}
QVariant TextAnalysisModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (role != Qt::DisplayRole)
		return QVariant();

	if (orientation != Qt::Horizontal) return QString::number(section);
	else if (section == 0) return QString(TextAnalysisDialog::tr("word/phrase"));
	else if (section == 1) return QString(TextAnalysisDialog::tr("count"));
	else if (section == 2) return QString(TextAnalysisDialog::tr("count relative"));
	else return QVariant();
}
int TextAnalysisModel::columnCount(const QModelIndex & parent) const {
	// remove unused argument warning
	(void) parent;

	return 3;
}
void TextAnalysisModel::updateAll() {
	wordCount=0;
	characterInWords=0;
	for (int i=0; i<words.size(); i++) {
		wordCount+=words[i].count;
		characterInWords+=words[i].count*words[i].word.size();
	}
	qSort(words);
	if (words.size()>0) relativeProzentMultipler=100.0/words[0].count;
	else relativeProzentMultipler=0;
	//emit dataChanged?
}

TextAnalysisDialog::TextAnalysisDialog(QWidget* parent,  QString name)
		: QDialog(parent), document(0), editor(0), lastSentenceLength(-1), lastMinSentenceLength(-1), lastParsedMinWordLength(-1) {
	setWindowTitle(name);
	setAttribute(Qt::WA_DeleteOnClose);
	ui.setupUi(this);

	ui.resultView->setWordWrap(false);
//connect(ui.comboBox, SIGNAL(activated(int)),this,SLOT(change(int)));


	connect(ui.countButton, SIGNAL(clicked()), SLOT(slotCount()));
	connect(ui.closeButton, SIGNAL(clicked()), SLOT(slotClose()));
	connect(ui.searchSelectionButton, SIGNAL(clicked()), SLOT(slotSelectionButton()));
	connect(ui.resultView, SIGNAL(doubleClicked(const QModelIndex &)), SLOT(slotSelectionButton()));

}

TextAnalysisDialog::~TextAnalysisDialog() {
}

void TextAnalysisDialog::setEditor(QEditor* aeditor) {
	if (editor) disconnect(editor,0,this,0);
	if (aeditor) {
		editor=aeditor;
		document=aeditor->document();
		cursor=aeditor->cursor();
		connect(editor,SIGNAL(destroyed()),this,SLOT(editorDestroyed()));
	} else {
		document=0;
		cursor=0;
		editor=0;
	}
}

void TextAnalysisDialog::interpretStructureTree(QTreeWidgetItem *item) {
	if (item->text(1)==QString("chapter")) {
		chapters.append(QPair<QString,int> (item->text(0), item->text(3).toUInt()));
		ui.comboBox->addItem(item->text(0));
	} else for (int i=0; i<item->childCount(); i++)
			interpretStructureTree(item->child(i));
}

void TextAnalysisDialog::init() {
	alreadyCount=false;
	chapters.clear();
}

void TextAnalysisDialog::needCount() {
	if (alreadyCount && lastSentenceLength==ui.sentenceLengthSpin->value() &&
	        lastParsedMinWordLength==(ui.minimumLengthMeaning->currentIndex()==4?ui.minimumLengthSpin->value():0) &&
	        lastEndCharacters==(ui.respectEndCharsCheck->isChecked()?ui.sentenceEndChars->text():"") &&
	        lastMinSentenceLength == (ui.wordsPerPhraseMeaning->currentIndex()==1?ui.sentenceLengthSpin->value():0)) return;
	lastSentenceLength=ui.sentenceLengthSpin->value();
	lastMinSentenceLength = (ui.wordsPerPhraseMeaning->currentIndex()==1?ui.sentenceLengthSpin->value():0);
	int minimumWordLength=0;
	if (ui.minimumLengthMeaning->currentIndex()==4) minimumWordLength=ui.minimumLengthSpin->value();
	lastParsedMinWordLength=minimumWordLength;
	bool respectSentenceEnd = ui.respectEndCharsCheck->isChecked();
	lastEndCharacters=respectSentenceEnd?ui.sentenceEndChars->text():"";
	int totalLines=document->lines();
	int textLines=0;
	int commentLines=0;
	for (int i=0; i<3; i++) {
		maps[i].resize(2+chapters.size());
		for (int j=0; j<maps[i].size(); j++)
			maps[i][j].clear();
	}
	int selectionStartLine=-1;
	int selectionEndLine=-1;
	int selectionStartIndex=-1;
	int selectionEndIndex=-1;
	if (cursor.hasSelection()) {
		QDocumentSelection sel = cursor.selection();
		selectionStartLine=sel.startLine;
		selectionEndLine=sel.endLine;
		selectionStartIndex=sel.start;
		selectionEndIndex=sel.end;
		if (selectionStartLine>selectionEndLine) {
			int temp=selectionStartLine;
			selectionStartLine=selectionEndLine;
			selectionEndLine=temp;
			temp=selectionStartIndex;
			selectionStartIndex=selectionEndIndex;
			selectionEndIndex=temp;
		} else if (selectionStartLine == selectionEndLine && selectionStartIndex>selectionEndIndex) {
			int temp=selectionStartIndex;
			selectionStartIndex=selectionEndIndex;
			selectionEndIndex=temp;
		}
	}

	int nextChapter=0;
	int extraMap=0;
	QList<QString> lastWords[3];
	int sentenceLengths[3] = {0,0,0};
	for (int l=0; l<document->lines(); l++) {
		if (nextChapter<chapters.size() && l+1>=chapters[nextChapter].second) {
			if (nextChapter==0) extraMap=2;
			else extraMap++;
			nextChapter++;
			if (extraMap>=maps[0].size()) extraMap=0;
		}
		QString line=document->line(l).text();
		bool commentReached=false;
		int nextIndex=0;
		int wordStartIndex;
		bool lineCountedAsText=false;
		QString curWord;
		int state;
		int lastIndex=0;
		while ((state=nextWord(line,nextIndex,curWord,wordStartIndex,true))!=NW_NOTHING) {
			bool inSelection;
			if (selectionStartLine!=selectionEndLine)
				inSelection=((l<selectionEndLine) && (l>selectionStartLine)) ||
				            ((l==selectionStartLine) && (nextIndex>selectionStartIndex)) ||
				            ((l==selectionEndLine) && (wordStartIndex<=selectionEndIndex));
			else
				inSelection=(l==selectionStartLine) && (nextIndex>selectionStartIndex) && (wordStartIndex<=selectionEndIndex);
			curWord=curWord.toLower();
			int curType=-1;
			if (commentReached) {
				if (respectSentenceEnd) for (int i=lastIndex; i<wordStartIndex; i++)
						if (lastEndCharacters.contains(line.at(i))) {
							sentenceLengths[2]=0;
							lastWords[2].clear();
							break;
						}
				curType=2;
			} else if (state == NW_COMMENT) {
				//comment is only % character
				curType=2;
				if (!commentReached) {
					commentReached=true;
					commentLines++;
					//find sentence end characters which belong to the words before the comment start
					if (respectSentenceEnd) for (int i=lastIndex; i<wordStartIndex; i++)
							if (line.at(i)==QChar('%') && (i==0 || line.at(i-1)!=QChar('\%'))) {
								lastIndex=i;
								break;
							} else if (lastEndCharacters.contains(line.at(i))) {
								sentenceLengths[0]=0;
								lastWords[0].clear();
								break;
							}
				}
			} else if (state==NW_COMMAND) {
				curType=1;
			} else if (state==NW_TEXT) {
				curType=0;
				if (!lineCountedAsText) {
					textLines++;
					lineCountedAsText=true;
				}
			}
			if (respectSentenceEnd && !commentReached)
				for (int i=lastIndex; i<wordStartIndex; i++)
					if (lastEndCharacters.contains(line.at(i))) {
						sentenceLengths[0]=0;
						lastWords[0].clear();
						break;
					}
			lastIndex=nextIndex;
			if (curType!=-1 && curWord.size()>=minimumWordLength) {
				//if (lastSentenceLength>1) {
				lastWords[curType].append(curWord);
				sentenceLengths[curType]+=curWord.size()+1;
				if (lastWords[curType].size()>lastSentenceLength) {
					sentenceLengths[curType]-=lastWords[curType].first().size()+1;
					lastWords[curType].removeFirst();
				}
				if (lastWords[curType].size()>=lastMinSentenceLength) {
					curWord="";
					curWord.reserve(sentenceLengths[curType]);
					foreach(QString s, lastWords[curType])
					curWord+=s+" ";
					curWord.truncate(curWord.size()-1);
					maps[curType][0][curWord]=maps[curType][0][curWord]+1;
					if (inSelection) maps[curType][1][curWord]=maps[curType][1][curWord]+1;
					if (extraMap!=0) maps[curType][extraMap][curWord]=maps[curType][extraMap][curWord]+1;
				}
			}
		};
		if (respectSentenceEnd) //it makes sense to ignore in something like .%. the sentence end after the % (and is less work)
			for (int i=lastIndex; i<line.size(); i++)
				if (lastEndCharacters.contains(line.at(i))) {
					sentenceLengths[commentReached?2:0]=0;
					lastWords[commentReached?2:0].clear();
					break;
				}

	}

	alreadyCount=true;

	ui.totalLinesLabel->setText(QString::number(totalLines));
	ui.textLinesLabel->setText(QString::number(textLines));
	ui.commentLinesLabel->setText(QString::number(commentLines));
}

void TextAnalysisDialog::insertDisplayData(const QMap<QString,int> & map) {
	int minLen=0;
	int minCount=ui.minimumCountSpin->value();
	int phraseLength=ui.sentenceLengthSpin->value();
	QRegExp wordFilter;
	bool filtered=ui.filter->currentIndex()!=0;
	QString curFilter=ui.filter->currentText();
	if (ui.filter->currentIndex()==-1 ||
	        curFilter != ui.filter->itemText(ui.filter->currentIndex())) wordFilter=QRegExp(curFilter);
	else wordFilter=QRegExp(curFilter.mid(curFilter.indexOf("(")));

	switch (ui.minimumLengthMeaning->currentIndex()) {
	case 2: //at least one word must have min length, all shorter with space: (min-1 +1)*phraseLength-1
		minLen=ui.minimumLengthSpin->value(); //no break!
		for (QMap<QString, int>::const_iterator it = map.constBegin(); it!=map.constEnd(); ++it)
			if (it.value()>=minCount) {
				if (it.key().size()>=minLen*phraseLength) {
					if (filtered || wordFilter.exactMatch(it.key()))
						displayed.words.append(Word(it.key(),it.value()));
				} else {
					if (filtered && !wordFilter.exactMatch(it.key())) continue;
					QString t=it.key();
					int last=0;
					int i=0;
					for (; i<t.size(); i++)
						if (t.at(i)==' ') {
							if (i-last>=minLen) {
								displayed.words.append(Word(it.key(),it.value()));
								break;
							} else last=i+1;
						}
					if (i==t.size() && i-last>=minLen) displayed.words.append(Word(it.key(),it.value()));
				}
			}
		break;
	case 3: //all words must have min len, (min +1)*phraseLength-1
		minLen=ui.minimumLengthSpin->value();
		for (QMap<QString, int>::const_iterator it = map.constBegin(); it!=map.constEnd(); ++it)
			if (it.value()>=minCount && it.key().size()>=minLen) { //not minLen*phraseCount because there can be less words in a phrase
				if (filtered && !wordFilter.exactMatch(it.key())) continue;
				QString t=it.key();
				int last=0;
				bool ok=true;
				for (int i=0; i<t.size(); i++)
					if (t.at(i)==' ') {
						if (i-last<minLen) {
							ok=false;
							break;
						} else last=i+1;
					}
				if (ok && t.size()-last>=minLen)
					displayed.words.append(Word(it.key(),it.value()));
			}
		break;
	case 1:
		minLen=ui.minimumLengthSpin->value();  //no break!
	default:
		if (filtered) {
			for (QMap<QString, int>::const_iterator it = map.constBegin(); it!=map.constEnd(); ++it)
				if (it.value()>=minCount && it.key().size()>=minLen && wordFilter.exactMatch(it.key()))
					displayed.words.append(Word(it.key(),it.value()));
		} else {
			for (QMap<QString, int>::const_iterator it = map.constBegin(); it!=map.constEnd(); ++it)
				if (it.value()>=minCount && it.key().size()>=minLen)
					displayed.words.append(Word(it.key(),it.value()));
		}
	}
}
void TextAnalysisDialog::slotCount() {
	needCount();
	displayed.words.clear(); //insert into map to sort
	if (ui.normalTextCheck->isChecked()) insertDisplayData(maps[0][ui.comboBox->currentIndex()]);
	if (ui.commandsCheck->isChecked()) insertDisplayData(maps[1][ui.comboBox->currentIndex()]);
	if (ui.commentsCheck->isChecked()) insertDisplayData(maps[2][ui.comboBox->currentIndex()]);

	displayed.updateAll();

	ui.resultView->setModel(NULL);
	ui.resultView->setModel(&displayed);

	ui.resultView->setShowGrid(false);
	ui.resultView->resizeRowsToContents();


	ui.displayedWordLabel->setText(QString::number(displayed.wordCount));
	ui.differentWordLabel->setText(QString::number(displayed.words.size()));
	ui.characterInWordsLabel->setText(QString::number(displayed.characterInWords));
}

void TextAnalysisDialog::editorDestroyed() {
	setEditor(0);
}

void TextAnalysisDialog::slotSelectionButton() {
	if (!editor) return;
	int s=ui.resultView->currentIndex().row();
	if (s<0 || s>=displayed.words.size()) return;
	QString selected=displayed.words[s].word;
	if (selected=="") return;
	if (lastSentenceLength>1 && selected.contains(' ')) {
		selected.replace(" ","\\W+");
		editor->find(selected,true,true);
	} else editor->find(selected,true,false);

}

void TextAnalysisDialog::slotClose() {
	close();
}

