#include "syntaxcheck.h"
#include "latexdocument.h"

SyntaxCheck::SyntaxCheck(QObject *parent) :
		QThread(parent)
{
	mLinesLock.lock();
	stopped=false;
	mLines.clear();
	mLinesLock.unlock();
	verbatimFormat=0;
	mLtxCommandLock.unlock();
}

void SyntaxCheck::setErrFormat(int errFormat){
	syntaxErrorFormat=errFormat;
}

void SyntaxCheck::putLine(QDocumentLineHandle* dlh,Environment previous,bool clearOverlay,int cols){
	Q_ASSERT(dlh);
	SyntaxLine newLine;
	dlh->ref(); // impede deletion of handle while in syntax check queue
	dlh->lockForRead();
	newLine.ticket=dlh->getCurrentTicket();
	dlh->unlock();
	newLine.dlh=dlh;
	newLine.prevEnv=previous;
	newLine.clearOverlay=clearOverlay;
	newLine.cols=cols;
	mLinesLock.lock();
	mLines.enqueue(newLine);
	mLinesLock.unlock();
	//avoid reading of any results before this execution is stopped
	//mResultLock.lock(); not possible under windows
	mLinesAvailable.release();
}

void SyntaxCheck::stop(){
	stopped=true;
	mLinesAvailable.release();
}

void SyntaxCheck::run(){
	forever {
		//wait for enqueued lines
		mLinesAvailable.acquire();
		if(stopped) break;
		// get Linedata
		mLinesLock.lock();
		SyntaxLine newLine=mLines.dequeue();
		mLinesLock.unlock();
		// do syntax check
		newLine.dlh->lockForRead();
		QDocumentLineHandle *prev=newLine.dlh->previous();
		QString line=newLine.dlh->text();
		newLine.dlh->unlock();
		QVector<int>fmts=newLine.dlh->getFormats();
		for(int i=0;i<line.length() && i < fmts.size();i++){
		    if(fmts[i]==verbatimFormat){
			line[i]=QChar(' ');
		    }
		}
		QStack<Environment> activeEnv;
		activeEnv.push(newLine.prevEnv);
		line=LatexParser::cutComment(line);
		Ranges newRanges;

		int excessCols=0;
		if(prev){
		    prev->lockForRead();
		    excessCols=prev->getCookie(0).toInt();
		    prev->unlock();
		}
		checkLine(line,newRanges,activeEnv,newLine.cols,excessCols);
		// place results
		if(newLine.clearOverlay) newLine.dlh->clearOverlays(syntaxErrorFormat);
		//if(newRanges.isEmpty()) continue;
		newLine.dlh->lockForWrite();
		if(newLine.ticket==newLine.dlh->getCurrentTicket()){ // discard results if text has been changed meanwhile
			Error elem;
			foreach(elem,newRanges){
				newLine.dlh->addOverlayNoLock(QFormatRange(elem.range.first,elem.range.second,syntaxErrorFormat));
			}
			int oldCookie=newLine.dlh->getCookie(0).toInt();
			bool cookieChanged=(oldCookie!=excessCols);
			//if excessCols has changed the subsequent lines need to be rechecked.
			if(cookieChanged){
			    newLine.dlh->setCookie(0,excessCols);
			    QDocumentLineHandle *next=newLine.dlh->next();
			    if(next){
				putLine(next,activeEnv.top(),true,newLine.cols);
			    }
			}
		}
		newLine.dlh->unlock();

		newLine.dlh->deref(); //if deleted, delete now
	}
}


void SyntaxCheck::checkLine(QString &line,Ranges &newRanges,QStack<Environment> &activeEnv,int cols,int &excessCols){
	// do syntax check on that line
	QMutexLocker locker(&mLtxCommandLock);
	QString word;
	int start=0;
	int wordstart;
	int status;
	bool inStructure=false;
	// do additional checks (not limited to commands)
	// check tabular compliance (columns)
	int count=0;
	if(activeEnv.top()==ENV_tabular){
	    count=excessCols;
	    int end=line.indexOf("\\\\");
	    int pos=-1;
	    int res=-1;
	    int lastEnd=-1;
	    while(end>=0){
		QRegExp rxMultiColumn("\\\\multicolumn\\{(\\d+)\\}\\{.+\\}\\{.+\\}");
		rxMultiColumn.setMinimal(true);
		bool mc_found=false;
		do{
		    res=rxMultiColumn.indexIn(line,pos+1);
		    int wrongPos=line.indexOf("\\&",pos+1);
		    pos=line.indexOf("&",pos+1);
		    if(wrongPos>-1 && wrongPos+1==pos)
			continue;
		    if(res>-1 && (res<pos || pos<0) ){
			// multicoulmn before &
			bool ok;
			int c=rxMultiColumn.cap(1).toInt(&ok);
			if(ok){
			    count+=c-2;
			}
			mc_found=true;
			if(pos<0){
			    count+=2;
			    break;
			}
			pos=res+1;
		    }else{
			mc_found=false;
		    }
		    count++;
		} while(pos>=0 && count<cols && pos<end);
		if((pos>=0 && pos<end &&!mc_found)||(count>cols)){
		    if(mc_found)
			pos=res-1;
		    Error elem;
		    elem.range=QPair<int,int>(pos+1,end-pos-1);
		    elem.type=ERR_tooManyCols;
		    newRanges.append(elem);
		}
		if(pos==-1 && count<cols){
		    Error elem;
		    elem.range=QPair<int,int>(end,2);
		    elem.type=ERR_tooLittleCols;
		    newRanges.append(elem);
		}
		lastEnd=end;
		end=line.indexOf("\\\\",end+1);
		count=0;
		pos=lastEnd;
	    }
	    pos=lastEnd;
	    // check for columns beyond last newline
	    QRegExp rxMultiColumn("\\\\multicolumn\\{(\\d+)\\}\\{.+\\}\\{.+\\}");
	    rxMultiColumn.setMinimal(true);
	    do{
		int res=rxMultiColumn.indexIn(line,pos+1);
		//pos=line.indexOf(QRegExp("[^\\\\]&"),pos+1);
		pos=line.indexOf(QRegExp("([^\\\\]|^)&"),pos+1);
		if(res>-1 && (res<pos || pos<0) ){
		    // multicoulmn before &
		    bool ok;
		    int c=rxMultiColumn.cap(1).toInt(&ok);
		    if(ok){
			count+=c-1;
		    }
		    pos=res+rxMultiColumn.cap().length()-1;
		}
		count++;
	    } while(pos>=0);
	    excessCols=count-1;
	}// tabular checking
	// check command-words
	while ((status=nextWord(line,start,word,wordstart,true,true,&inStructure))){
		if(status==NW_COMMAND){
			bool ignoreEnv=false;
			if(word=="\\begin"||word=="\\end"){
				QStringList options;
				LatexParser::resolveCommandOptions(line,wordstart,options);
				if(options.size()>0){
					word+=options.first();
				}
			}
			if(LatexParser::definitionCommands.contains(word)){ // don't check in command definition
				QStringList options;
				QList<int> starts;
				LatexParser::resolveCommandOptions(line,wordstart,options,&starts);
				for(int i=1;i<options.count()&&i<4;i++){
				    QString option=options.at(i);
				    if(option.startsWith("[")){
					continue;
				    }
				    start=starts.at(i)+option.length();
				    break;
				}
			}
			if(ltxCommands.refCommands.contains(word)||LatexParser::labelCommands.contains(word)||LatexParser::fileCommands.contains(word)){ //don't check syntax in reference, label or include
				QStringList options;
				LatexParser::resolveCommandOptions(line,wordstart,options);
				if(options.size()>0){
					QString first=options.takeFirst();
					if(!first.startsWith("[")){  //handling of includegraphics should be improved !!!
						start+=first.length();
					}else{
						if(!options.isEmpty()){
							QString second=options.first();
							if(second.startsWith("{")){
								second.fill(' ');
								line.replace(start+first.length(),second.length(),second);
							}
						}
					}
				}
			}
			if(LatexParser::mathStartCommands.contains(word)&&activeEnv.top()!=ENV_math){
				activeEnv.push(ENV_math);
				continue;
			}
			if(LatexParser::mathStopCommands.contains(word)&&activeEnv.top()==ENV_math){
				activeEnv.pop();
				if(activeEnv.isEmpty()) activeEnv.push(ENV_normal);
				continue;
			}
			if(ignoreEnv&&(ltxCommands.normalCommands.contains(word) || ltxCommands.tabularCommands.contains(word) || ltxCommands.userdefinedCommands.contains(word) || ltxCommands.mathCommands.contains(word)|| ltxCommands.tabbingCommands.contains(word)) )
			    continue;
			if((activeEnv.top()==ENV_tabbing)&&!ltxCommands.tabbingCommands.contains(word) &&!ltxCommands.normalCommands.contains(word) && !ltxCommands.userdefinedCommands.contains(word)){ // extend for math coammnds
				Error elem;
				elem.range=QPair<int,int>(wordstart,word.length());
				elem.type=ERR_unrecognizedCommand;
				if(ltxCommands.mathCommands.contains(word))
					elem.type=ERR_MathCommandOutsideMath;
				if(ltxCommands.tabularCommands.contains(word))
					elem.type=ERR_TabularCommandOutsideTab;
				newRanges.append(elem);
			}
			if((activeEnv.top()==ENV_normal)&&!ltxCommands.normalCommands.contains(word) && !ltxCommands.userdefinedCommands.contains(word)){ // extend for math coammnds
				Error elem;
				elem.range=QPair<int,int>(wordstart,word.length());
				elem.type=ERR_unrecognizedCommand;
				if(ltxCommands.mathCommands.contains(word))
					elem.type=ERR_MathCommandOutsideMath;
				if(ltxCommands.tabularCommands.contains(word))
					elem.type=ERR_TabularCommandOutsideTab;
				if(ltxCommands.tabbingCommands.contains(word))
					elem.type=ERR_TabbingCommandOutside;
				newRanges.append(elem);
			}
			if(activeEnv.top()==ENV_matrix && (word=="&" || word=="\\\\")) continue;
			if((activeEnv.top()==ENV_math||activeEnv.top()==ENV_matrix)&&!ltxCommands.mathCommands.contains(word) && !ltxCommands.userdefinedCommands.contains(word)&&!ignoreEnv){ // extend for math coammnds
				Error elem;
				elem.range=QPair<int,int>(wordstart,word.length());
				elem.type=ERR_unrecognizedMathCommand;
				if(ltxCommands.tabularCommands.contains(word))
					elem.type=ERR_TabularCommandOutsideTab;
				newRanges.append(elem);
			}
			if(activeEnv.top()==ENV_tabular&&!ltxCommands.normalCommands.contains(word) && !ltxCommands.tabularCommands.contains(word) && !ltxCommands.userdefinedCommands.contains(word)){ // extend for math coammnds
				Error elem;
				elem.range=QPair<int,int>(wordstart,word.length());
				elem.type=ERR_unrecognizedTabularCommand;
				if(ltxCommands.mathCommands.contains(word))
					elem.type=ERR_MathCommandOutsideMath;
				if(ltxCommands.tabbingCommands.contains(word))
					elem.type=ERR_TabbingCommandOutside;
				newRanges.append(elem);
			}
		}

	}


    }

QString SyntaxCheck::getErrorAt(QDocumentLineHandle *dlh,int pos,Environment previous,int cols){
	// do syntax check
	QString line=dlh->text();
	QVector<int>fmts=dlh->getFormats();
	for(int i=0;i<line.length() && i < fmts.size();i++){
	    if(fmts[i]==verbatimFormat){
		line[i]=QChar(' ');
	    }
	}
	QStack<Environment> activeEnv;
	activeEnv.push(previous);
	line=LatexParser::cutComment(line);
	Ranges newRanges;
	int excessCols=0;
	checkLine(line,newRanges,activeEnv,cols,excessCols);
	// find Error at Position
	ErrorType result=ERR_none;
	foreach(Error elem,newRanges){
		if(elem.range.second+elem.range.first<pos) continue;
		if(elem.range.first>pos) break;
		result=elem.type;
	}
	// now generate Error message

	QStringList messages;
	messages << tr("no error")<< tr("unrecognized command")<< tr("unrecognized math command")<< tr("unrecognized tabular command")<< tr("tabular command outside tabular env")<< tr("math command outside math env") << tr("tabbing command outside tabbing env") << tr("more cols in tabular than specified") << tr("cols in tabular missing")
		 << tr("\\\\ missing");
	return messages.value(int(result),tr("unknown"));
}
void SyntaxCheck::setLtxCommands(LatexParser cmds){
    QMutexLocker locker(&mLtxCommandLock);
    ltxCommands=cmds;
}
