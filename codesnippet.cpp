#include "codesnippet.h"

#include "qeditor.h"
#include "qdocumentcursor.h"
#include "qdocumentline.h"
#include "filechooser.h"
#include "latexdocument.h"
#include "smallUsefulFunctions.h"

int CodeSnippetPlaceHolder::offsetEnd(){
	return offset + length;
}

void parseSnippetPlaceHolder(const QString& snippet, int& i, QString& curLine, CodeSnippetPlaceHolder& ph){
	ph.offset = curLine.length();
	ph.length = 0;
	ph.id = -1;
	ph.flags = 0;
	for (;i<snippet.length(); i++){
		if (snippet.at(i) == '%' && i+1<snippet.length()) {
			i++;
			switch (snippet.at(i).toAscii()){
				case'|': ph.flags |= CodeSnippetPlaceHolder::AutoSelect; break;
				case '>': return;
				case '%': curLine+='%'; ph.length++; break;
				case ':': goto secondLevelBreak;
				default: curLine+="%"; curLine+=snippet.at(i); ph.length++;
			}
		} else {
			curLine += snippet.at(i);
			ph.length++;
		}
	}
	secondLevelBreak:
	if (i>=snippet.length()) return;
	if (snippet.at(i)!=':') return;

	int snippetEnd = snippet.indexOf("%>", i);
	if (snippetEnd == -1) return;
	QString options = snippet.mid(i+1, snippetEnd-i-1);
	i = snippetEnd+1;
	foreach (const QString& s, options.split(",")) {
		QString t = s.trimmed();
		if (t == "mirror") ph.flags|=CodeSnippetPlaceHolder::Mirror;
		else if (t == "multiline") ph.flags|=CodeSnippetPlaceHolder::PreferredMultilineAutoSelect;
		else if (t.startsWith("id:")) ph.id = t.remove(0,3).toInt();
		else if (t.startsWith("select")) ph.flags|=CodeSnippetPlaceHolder::AutoSelect;
	}
}

bool CodeSnippet::autoReplaceCommands=true;

CodeSnippet::CodeSnippet(const QString &newWord) {
	QString realNewWord=newWord;
	// \begin magic
	if (newWord == "%<%:TEXMAKERX-GENERIC-ENVIRONMENT-TEMPLATE%>"){
		realNewWord = "\\begin{%<"+QObject::tr("environment-name")+"%:select,id:2%>}\n"
			      "%<"+QObject::tr("content...")+"%:select,multiline%>\n"
			      "\\end{%<"+QObject::tr("environment-name")+"%:mirror,id:2%>}";
	} else if (realNewWord.startsWith("\\begin{")&&
		!realNewWord.contains("\n")&&!realNewWord.contains("%n") //only a single line
	    && realNewWord.lastIndexOf("\\") == 0) //only one latex command in the line
	{
		int p=newWord.indexOf("{");
		QString environmentName=realNewWord.mid(p,newWord.indexOf("}")-p+1); //contains the {}
		QString content="%<"+QObject::tr("content...")+"%:multiline%>";
		realNewWord+="\n"+content+"\n\\end"+environmentName;
	}

	cursorLine=-1;
	cursorOffset=-1;
	anchorOffset=-1;
	usageCount=0;
	index=-1;
	QString curLine;

	curLine.reserve(realNewWord.length());
	word.reserve(realNewWord.length());
	bool escape=false;
	bool hasPlaceHolder, hasMirrors = false, hasAutoSelectPlaceHolder = false;
	placeHolders.append(QList<CodeSnippetPlaceHolder>()); //during the creation this contains a line more than lines

	CodeSnippetPlaceHolder tempPlaceholder;
	for (int i=0; i<realNewWord.length(); i++)
		if (!escape) {
			if (realNewWord.at(i)==QChar('\n')) {
				lines.append(curLine);
				placeHolders.append(QList<CodeSnippetPlaceHolder>());
				curLine.clear();
			} else if (realNewWord.at(i)==QChar('%')) escape=true;
			else {
				curLine+=realNewWord.at(i);
				word.append(realNewWord.at(i));
			}
		} else {
			escape=false;
			switch (realNewWord.at(i).toAscii()) {
			case '%':
				word+='%';
				curLine+='%';
				break;
			case '|':
				cursorLine=lines.count(); //first line is 0
				anchorOffset=cursorOffset;
				cursorOffset=curLine.length();
				break;
			case '<':
				i++;
				parseSnippetPlaceHolder(realNewWord, i, curLine, tempPlaceholder);
				hasPlaceHolder = true;
				if (tempPlaceholder.flags & CodeSnippetPlaceHolder::AutoSelect) hasAutoSelectPlaceHolder = true;
				if (tempPlaceholder.flags & CodeSnippetPlaceHolder::Mirror) hasMirrors = true;
				placeHolders.last().append(tempPlaceholder);
			//	foundDescription = true;
				break;
			case 'n':
				lines.append(curLine);
				placeHolders.append(QList<CodeSnippetPlaceHolder>());
				curLine.clear();
				//curLine+="\n";
				break;	
			default: // escape was not an escape character ...
				curLine+='%';
				curLine+=realNewWord.at(i);
				word.append('%');
				word.append(realNewWord.at(i));
			}
		}
	lines.append(curLine);
	if (cursorLine == -1 && hasPlaceHolder && !hasAutoSelectPlaceHolder) //use first placeholder at new selection if nothing else is set
		for (int i=0;i<placeHolders.count();i++)
			if (placeHolders[i].count()>0)
				placeHolders[i].first().flags |= CodeSnippetPlaceHolder::AutoSelect;

	/*if (cursorLine==-1 && foundDescription)
		for (int i=0;i<placeHolders.count();i++)
			if (placeHolders[i].count()>0) {
				cursorLine=i;
				anchorOffset =placeHolders[i][0].offset;
				cursorOffset =placeHolders[i][0].offset+placeHolders[i][0].length;
				break;
			}*/
	if (hasMirrors){
		for (int l=0;l<placeHolders.count();l++)
			for (int i=0;i<placeHolders[l].size();i++)
				if (placeHolders[l][i].flags & CodeSnippetPlaceHolder::Mirror) {
					for (int lm=0;lm<placeHolders.count();lm++)
						for (int im=0;im<placeHolders[lm].size();im++)
							if ((placeHolders[l][i].id == placeHolders[lm][im].id) &&
							    !(placeHolders[lm][im].flags & CodeSnippetPlaceHolder::Mirrored)){
								placeHolders[lm][im].flags |= CodeSnippetPlaceHolder::Mirrored;
								goto secondLevelBreak;
							}
					secondLevelBreak:;
				}
	}
	if (anchorOffset==-1) anchorOffset=cursorOffset;
	sortWord=word.toLower();
	sortWord.replace("{","!");//js: still using dirty hack, however order should be ' '{[* abcde...
	sortWord.replace("}","!");// needs to be replaced as well for sorting \bgein{abc*} after \bgein{abc}
	sortWord.replace("[","\"");//(first is a space->) !"# follow directly in the ascii table
	sortWord.replace("*","#");
}

bool CodeSnippet::operator< (const CodeSnippet &cw) const {
	return cw.sortWord > sortWord;
}
bool CodeSnippet::operator== (const CodeSnippet &cw) const {
	return cw.word == word;
}


void CodeSnippet::insert(QEditor* editor){
	if (!editor) return;
	QDocumentCursor c=editor->cursor();
	insertAt(editor,&c);
}
void CodeSnippet::insertAt(QEditor* editor, QDocumentCursor* cursor, bool usePlaceholders, bool byCompleter) const{
	if (lines.empty()||!editor||!cursor) return;
	
	QStringList curLines=lines;

	QString savedSelection;
	bool alwaysSelect = false;
	bool editBlockOpened = false;
	if (cursor->hasSelection()) {
		savedSelection=cursor->selectedText();
		editBlockOpened = true;
		cursor->beginEditBlock();
		cursor->removeSelectedText();
	}else if(!editor->cutBuffer.isEmpty()){
		savedSelection=editor->cutBuffer;
		editor->cutBuffer.clear();
		alwaysSelect = true;
	}
	bool multiLineSavedSelection = savedSelection.contains("\n");
	QDocumentCursor selector=*cursor;
	QDocumentLine curLine=cursor->line();

	//find filechooser escape %(   %)
	QString line=lines.join("\n");
	QRegExp rx("%\\((.+)%\\)");
	int pos=rx.indexIn(line,0);
	if(pos>-1){
	    FileChooser *sfDlg = new FileChooser(0,QApplication::tr("Select a File"));
		sfDlg->setFilter(rx.cap(1));
		LatexDocument *doc=qobject_cast<LatexDocument*>(cursor->document());
		QString path=doc->parent->getCompileFileName();
		path=getPathfromFilename(path);
		QString directory;
		if(path.isEmpty()) directory=QDir::homePath();
		else directory=path;
		sfDlg->setDir(directory);
		if (sfDlg->exec()) {
			QString fn=sfDlg->fileName();
			line.replace(rx,getRelativeBaseNameToPath(fn,path));
		}
		delete sfDlg;
	}

	// on multi line commands, replace environments only
	if(autoReplaceCommands && lines.size()>1 && line.contains("\\begin{")){
		QString curLine=cursor->line().text();
		int wordBreak=curLine.indexOf(QRegExp("\\W"),cursor->columnNumber());
		int closeCurl=curLine.indexOf("}",cursor->columnNumber());
		int openCurl=curLine.indexOf("{",cursor->columnNumber());
		int openBracket=curLine.indexOf("[",cursor->columnNumber());
		if(closeCurl>0){
			if(openBracket<0) openBracket=1e9;
			if(openCurl<0) openCurl=1e9;
			if(wordBreak<0) wordBreak=1e9;
			if(closeCurl<openBracket && (closeCurl<=wordBreak || openCurl<=wordBreak)){
				QString oldEnv;
				if(closeCurl<openCurl)
					oldEnv=curLine.mid(cursor->columnNumber(),closeCurl-cursor->columnNumber());
				else
					oldEnv=curLine.mid(openCurl+1,closeCurl-openCurl-1);
				QRegExp rx("\\\\begin\\{(.+)\\}");
				rx.setMinimal(true);
				rx.indexIn(line);
				QString newEnv=rx.cap(1);
				// remove curly brakets as well
				QDocument* doc=cursor->document();
				QString searchWord="\\end{"+oldEnv+"}";
				QString inhibitor="\\begin{"+oldEnv+"}";
				bool backward=false;
				int step=1;
				int startLine=cursor->lineNumber();
				//int startCol=cursor.columnNumber();
				int endLine=doc->findLineContaining(searchWord,startLine+step,Qt::CaseSensitive,backward);
				int inhibitLine=doc->findLineContaining(inhibitor,startLine+step,Qt::CaseSensitive,backward); // not perfect (same line end/start ...)
				while (inhibitLine>0 && endLine>0 && inhibitLine*step<endLine*step) {
					endLine=doc->findLineContaining(searchWord,endLine+step,Qt::CaseSensitive,backward); // not perfect (same line end/start ...)
					inhibitLine=doc->findLineContaining(inhibitor,inhibitLine+step,Qt::CaseSensitive,backward);
				}
				QString endText=doc->line(endLine).text();
				int start=endText.indexOf(searchWord);
				int offset=searchWord.indexOf("{");
				int length=searchWord.length()-offset-1;
				selector.moveTo(endLine,start+1+offset);
				selector.movePosition(length-1,QDocumentCursor::Right,QDocumentCursor::KeepAnchor);
				selector.replaceSelectedText(newEnv);
				cursor->movePosition(closeCurl-cursor->columnNumber()+1,QDocumentCursor::Right,QDocumentCursor::KeepAnchor);
				editor->insertText(*cursor,lines.first());
				if (editBlockOpened) cursor->endEditBlock();
				return;
			}
		}
	}

	int baseLine=cursor->lineNumber();
	int baseLineIndent = cursor->columnNumber(); //text before inserted word moves placeholders to the right
	int lastLineRemainingLength = curLine.text().length()-baseLineIndent; //last line will has length: indentation + codesnippet + lastLineRemainingLength
	editor->insertText(*cursor,line); //don't use cursor->insertText to keep autoindentation working

	if (editBlockOpened) cursor->endEditBlock();

	// on single line commands only: replace command
	if(byCompleter && autoReplaceCommands && lines.size()==1 && line.startsWith('\\')){
		if(cursor->nextChar().isLetterOrNumber()||cursor->nextChar()==QChar('{')){
			QString curLine=cursor->line().text();
			int wordBreak=curLine.indexOf(QRegExp("\\W"),cursor->columnNumber());
			int closeCurl=curLine.indexOf("}",cursor->columnNumber());
			int openCurl=curLine.indexOf("{",cursor->columnNumber());
			int openBracket=curLine.indexOf("[",cursor->columnNumber());
			if(!line.contains("{")){
				if(openBracket<0) openBracket=1e9;
				if(closeCurl<0) closeCurl=1e9;
				if(openCurl<0) openCurl=1e9;
				if(wordBreak<openBracket && wordBreak<closeCurl &&wordBreak<openCurl){
					if(wordBreak<0)
						cursor->movePosition(wordBreak-cursor->columnNumber(),QDocumentCursor::EndOfLine,QDocumentCursor::KeepAnchor);
					else
						cursor->movePosition(wordBreak-cursor->columnNumber(),QDocumentCursor::Right,QDocumentCursor::KeepAnchor);
					cursor->removeSelectedText();
					return;
				}
			}else{
				if(openCurl>-1){
					if(openBracket<0) openBracket=1e9;
					if(closeCurl<0) closeCurl=1e9;
					if(openCurl<openBracket && openCurl<closeCurl &&openCurl<=wordBreak){
						cursor->movePosition(openCurl-cursor->columnNumber(),QDocumentCursor::Right,QDocumentCursor::KeepAnchor);
						cursor->removeSelectedText();
						int curl=line.length()-line.indexOf("{");
						cursor->movePosition(curl,QDocumentCursor::Left,QDocumentCursor::KeepAnchor);
						cursor->removeSelectedText();
						return;
					}
				}
			}
		}
	}


	Q_ASSERT(placeHolders.size()==lines.count());
	if (usePlaceholders) {
		//check if there actually are placeholders to insert
		usePlaceholders=false;
		for (int l=0;l< lines.count();l++)
			usePlaceholders|=placeHolders[l].size();
	}
	int autoSelectPlaceholder = -1;
	if (usePlaceholders) {
		if (editor->currentPlaceHolder()!=-1 && 
			editor->getPlaceHolder(editor->currentPlaceHolder()).cursor.isWithinSelection(*cursor))
			editor->removePlaceHolder(editor->currentPlaceHolder()); //remove currentplaceholder to prevent nesting
		for (int l=0;l< lines.count();l++){
			//if (l<mLines.count()-1) cursor->insertLine();
			for (int i=0; i<placeHolders[l].size(); i++) {
				if (placeHolders[l][i].flags & CodeSnippetPlaceHolder::Mirror) continue;
				PlaceHolder ph;
				ph.length=placeHolders[l][i].length;
				ph.cursor = getCursor(editor, placeHolders[l][i], l, baseLine, baseLineIndent, lastLineRemainingLength);
				if (!ph.cursor.isValid()) continue;
				editor->addPlaceHolder(ph);
				if (placeHolders[l][i].flags & CodeSnippetPlaceHolder::Mirrored) {
					int phId = editor->placeHolderCount()-1;
					for (int lm=0; lm<placeHolders.size(); lm++)
						for (int im=0; im < placeHolders[lm].size(); im++)
							if (placeHolders[lm][im].flags & CodeSnippetPlaceHolder::Mirror &&
							    placeHolders[lm][im].id == placeHolders[l][i].id)
							editor->addPlaceHolderMirror(phId, getCursor(editor, placeHolders[lm][im], lm, baseLine, baseLineIndent, lastLineRemainingLength));
				}
				if ((placeHolders[l][i].flags & CodeSnippetPlaceHolder::AutoSelect) &&
				      ((autoSelectPlaceholder == -1) ||
					(multiLineSavedSelection && (placeHolders[l][i].flags & CodeSnippetPlaceHolder::PreferredMultilineAutoSelect))))
					autoSelectPlaceholder = editor->placeHolderCount()-1;

			}
		}
	}
	//place cursor/add \end
	if (cursorOffset!=-1) {
		int realAnchorOffset=anchorOffset; //will be moved to the right if text is already inserted on this line
		if (cursorLine>0) {
			if (cursorLine>=lines.size()) return;
			if (!selector.movePosition(cursorLine,QDocumentCursor::Down,QDocumentCursor::MoveAnchor))
				return;
			//if (editor->flag(QEditor::AutoIndent))
			realAnchorOffset += selector.line().length()-lines[cursorLine].length();
			if (cursorLine + 1 == lines.size())
				realAnchorOffset-=lastLineRemainingLength;
		} else realAnchorOffset += baseLineIndent;
		selector.setColumnNumber(realAnchorOffset);
		bool ok=true;
		if (cursorOffset>anchorOffset) 
			ok=selector.movePosition(cursorOffset-anchorOffset,QDocumentCursor::Right,QDocumentCursor::KeepAnchor);
		else if (cursorOffset<anchorOffset)
			ok=selector.movePosition(anchorOffset-cursorOffset,QDocumentCursor::Left,QDocumentCursor::KeepAnchor);
		if (!ok) return;
		editor->setCursor(selector);
	} else if (autoSelectPlaceholder!=-1) editor->setPlaceHolder(autoSelectPlaceholder, true); //this moves the cursor to that placeholder
	else {
		editor->setCursor(*cursor); //place after insertion
		return;
	}
	if (!savedSelection.isEmpty()) {
		QDocumentCursor oldCursor = editor->cursor();
		editor->cursor().insertText(savedSelection,true);
		if (!editor->cursor().hasSelection() && alwaysSelect) {
			oldCursor.movePosition(savedSelection.length(), QDocumentCursor::Right, QDocumentCursor::KeepAnchor);
			editor->setCursor(oldCursor);
		}
		if (autoSelectPlaceholder!=-1) editor->setPlaceHolder(autoSelectPlaceholder, true); //this synchronizes the placeholder mirrors with the current placeholder text
	}
}

void CodeSnippet::setName(const QString& name){
	sortWord.prepend(name);
}
QString CodeSnippet::getName(){
	return name;
}

QDocumentCursor CodeSnippet::getCursor(QEditor * editor, const CodeSnippetPlaceHolder &ph, int snippetLine, int baseLine, int baseLineIndent, int lastLineRemainingLength) const{
	QDocumentCursor cursor=editor->document()->cursor(baseLine+snippetLine, ph.offset);
	if (snippetLine==0) cursor.movePosition(baseLineIndent,QDocumentCursor::NextCharacter);
	else {
		cursor.movePosition(cursor.line().length()-lines[snippetLine].length(),QDocumentCursor::NextCharacter);
		if (snippetLine+1==lines.size())
			cursor.movePosition(lastLineRemainingLength,QDocumentCursor::PreviousCharacter);
	}
	return cursor;
}
