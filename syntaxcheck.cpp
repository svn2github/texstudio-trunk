#include "syntaxcheck.h"

SyntaxCheck::SyntaxCheck(QObject *parent) :
    QThread(parent)
{
    mLinesLock.lock();
    stopped=false;
    mLines.clear();
    mLinesLock.unlock();
}

void SyntaxCheck::setErrFormat(int errFormat){
    syntaxErrorFormat=errFormat;
}

void SyntaxCheck::putLine(QDocumentLineHandle* dlh,Environment previous){
    SyntaxLine newLine;
    dlh->lockForRead();
    newLine.text=dlh->text();
    newLine.ticket=dlh->getCurrentTicket();
    dlh->unlock();
    newLine.dlh=dlh;
    newLine.prevEnv=previous;

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
	     // copy line
	     mLinesLock.lock();
	     SyntaxLine newLine=mLines.dequeue();
	     mLinesLock.unlock();
	     QString line=newLine.text;
             QStack<Environment> activeEnv;
	     activeEnv.push(newLine.prevEnv);
             line=LatexParser::cutComment(line);
	     Ranges newRanges;
	     // do syntax check on that lien
             QString word;
             int start=0;
             int wordstart;
             int status;
             bool inStructure=false;
             while ((status=nextWord(line,start,word,wordstart,true,true,&inStructure))){
                 if(status==NW_COMMAND){
		     bool end=false;
                     if(word=="\\begin"||word=="\\end"){
			 end= (word=="\\end");
                         QStringList options;
                         LatexParser::resolveCommandOptions(line,wordstart,options);
                         if(options.size()>0){
                             word+=options.first();
                         }
                     }
                     if(LatexParser::refCommands.contains(word)||LatexParser::labelCommands.contains(word)||LatexParser::fileCommands.contains(word)){ //don't check syntax in reference, label or include
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
		     if((activeEnv.top()==ENV_normal||end)&&!LatexParser::normalCommands.contains(word) && !LatexParser::userdefinedCommands.contains(word)){ // extend for math coammnds
                         QPair<int,int> elem(wordstart,word.length());
			 newRanges.append(elem);
                     }
		     if(activeEnv.top()==ENV_math&&!LatexParser::mathCommands.contains(word) && !LatexParser::userdefinedCommands.contains(word)&&!end){ // extend for math coammnds
                         QPair<int,int> elem(wordstart,word.length());
			 newRanges.append(elem);
                     }
                     if(activeEnv.top()==ENV_tabular&&!LatexParser::normalCommands.contains(word) && !LatexParser::tabularCommands.contains(word) && !LatexParser::userdefinedCommands.contains(word)){ // extend for math coammnds
                         QPair<int,int> elem(wordstart,word.length());
			 newRanges.append(elem);
                     }
                 }

	     }
	     // place results
	     QPair<int,int> elem;
	     foreach(elem,newRanges){
		newLine.dlh->addOverlay(QFormatRange(elem.first,elem.second,syntaxErrorFormat));
	     }
	 }
}
