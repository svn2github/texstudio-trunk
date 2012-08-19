#include "latexdocument.h"
#include "qdocument.h"
#include "qformatscheme.h"
#include "qdocumentline.h"
#include "qdocumentline_p.h"
#include "qdocumentcursor.h"
#include "qeditor.h"
#include "latexcompleter.h"
#include "latexcompleter_config.h"
#include "smallUsefulFunctions.h"

//FileNamePair::FileNamePair(const QString& rel):relative(rel){};
FileNamePair::FileNamePair(const QString& rel, const QString& abs):relative(rel),absolute(abs){};

LatexDocument::LatexDocument(QObject *parent):QDocument(parent),edView(0),mAppendixLine(0)
{
	baseStructure = new StructureEntry(this,StructureEntry::SE_DOCUMENT_ROOT);
	magicCommentList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	labelList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	todoList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	bibTeXList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	blockList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	
	magicCommentList->title=tr("MAGIC_COMMENTS");
	labelList->title=tr("LABELS");
	todoList->title=tr("TODO");
	bibTeXList->title=tr("BIBLIOGRAPHY");
	blockList->title=tr("BLOCKS");
	mLabelItem.clear();
	mBibItem.clear();
	mUserCommandList.clear();
	mMentionedBibTeXFiles.clear();
	masterDocument=0;
	this->parent=0;
	mSpellingLanguage=QLocale::c();
}
LatexDocument::~LatexDocument(){
	if (!magicCommentList->parent) delete magicCommentList;
	if (!labelList->parent) delete labelList;
	if (!todoList->parent) delete todoList;
	if (!bibTeXList->parent) delete bibTeXList;
	if (!blockList->parent) delete blockList;
	delete baseStructure;
}

void LatexDocument::setFileName(const QString& fileName){
	//clear all references to old editor
	if (this->edView){
		StructureEntryIterator iter(baseStructure);
		while (iter.hasNext()) iter.next()->setLine(0);
	}
	
	this->fileName=fileName;
	this->fileInfo=QFileInfo(fileName);
	this->edView=0;
}
void LatexDocument::setEditorView(LatexEditorView* edView){
	this->fileName=edView->editor->fileName();
	this->fileInfo=edView->editor->fileInfo();
	this->edView=edView;
	if(baseStructure){
		baseStructure->title=fileName;
		emit updateElement(baseStructure);
	}
}
LatexEditorView *LatexDocument::getEditorView() const{
	return this->edView;
}

QString LatexDocument::getFileName() const{
	return fileName;
}
QFileInfo LatexDocument::getFileInfo() const{
	return fileInfo;
}

QMultiHash<QDocumentLineHandle*,FileNamePair>& LatexDocument::mentionedBibTeXFiles(){
	return mMentionedBibTeXFiles;
}
const QMultiHash<QDocumentLineHandle*,FileNamePair>& LatexDocument::mentionedBibTeXFiles() const{
	return mMentionedBibTeXFiles;
}

QDocumentSelection LatexDocument::sectionSelection(StructureEntry* section){
	QDocumentSelection result;
	result.endLine=-1;result.startLine=-1;
	
	if (section->type!=StructureEntry::SE_SECTION) return result;
	int startLine=section->getRealLineNumber();
	
	// find next section or higher
	StructureEntry* parent;
	int index;
	do {
		parent=section->parent;
		if (parent) {
			index=section->getRealParentRow();
			section=parent;
		} else index=-1;
	} while ((index>=0)&&(index>=parent->children.count()-1)&&(parent->type==StructureEntry::SE_SECTION));
	
	int endingLine=-1;
	if (index>=0 && index<parent->children.count()-1) {
		endingLine=parent->children.at(index+1)->getRealLineNumber();
	} else {
		// no ending section but end of document
		endingLine=findLineContaining("\\end{document}",startLine,Qt::CaseInsensitive);
		if (endingLine<0) endingLine=lines();
	}
	
	result.startLine=startLine;
	result.endLine=endingLine;
	result.end=0;
	result.start=0;
	return result;
}

void LatexDocument::clearStructure() {
	mUserCommandList.clear();
	mLabelItem.clear();
	mBibItem.clear();
	mRefItem.clear();
	mMentionedBibTeXFiles.clear();
	
	mAppendixLine=0;
	
	if(baseStructure){
		emit structureUpdated(this,0);
		
		if (!magicCommentList->parent) delete magicCommentList;
		if (!labelList->parent) delete labelList;
		if (!todoList->parent) delete todoList;
		if (!bibTeXList->parent) delete bibTeXList;
		if (!blockList->parent) delete blockList;
		int row=parent->documents.indexOf(this);
		
		if(parent->model->getSingleDocMode()){
			if(parent->currentDocument==this){
				removeElement(baseStructure,0);
				delete baseStructure;
				removeElementFinished();
			}else{
				LatexDocument *doc=parent->currentDocument;
				parent->currentDocument=this;
				parent->updateStructure();
				removeElement(baseStructure,0);
				delete baseStructure;
				removeElementFinished();
				parent->currentDocument=doc;
				parent->updateStructure();
			}
		}else{
			removeElement(baseStructure,row);
			delete baseStructure;
			removeElementFinished();
		}
	}
#ifndef QT_NO_DEBUG
	Q_ASSERT(StructureContent.isEmpty());
#endif
	baseStructure=0;
	
}

void LatexDocument::initStructure(){
	clearStructure();
	
	baseStructure = new StructureEntry(this,StructureEntry::SE_DOCUMENT_ROOT);
	baseStructure->title=fileName;
	magicCommentList = new StructureEntry(this,StructureEntry::SE_OVERVIEW);
	magicCommentList->title=tr("MAGIC_COMMENTS");
	labelList = new StructureEntry(this,StructureEntry::SE_OVERVIEW);
	labelList->title=tr("LABELS");
	todoList = new StructureEntry(this,StructureEntry::SE_OVERVIEW);
	todoList->title=tr("TODO");
	bibTeXList = new StructureEntry(this,StructureEntry::SE_OVERVIEW);
	bibTeXList->title=tr("BIBLIOGRAPHY");
	blockList = new StructureEntry(this,StructureEntry::SE_OVERVIEW);
	blockList->title=tr("BLOCKS");
}

void LatexDocument::updateStructure() {
	initStructure();
	
	patchStructure(0, lineCount());
	
	emit structureLost(this);
}

/* Removes a deleted line from the structure view */
void LatexDocument::patchStructureRemoval(QDocumentLineHandle* dlh) {
	if(!baseStructure) return;
	bool completerNeedsUpdate=false;
	bool bibTeXFilesNeedsUpdate=false;
	bool updateSyntaxCheck=false;
	if (mLabelItem.contains(dlh)) {
		QList<ReferencePair> labels=mLabelItem.values(dlh);
		completerNeedsUpdate = true;
		mLabelItem.remove(dlh);
		foreach(const ReferencePair& rp,labels)
			updateRefsLabels(rp.name);
	}
	mRefItem.remove(dlh);
	if(mMentionedBibTeXFiles.remove(dlh))
		bibTeXFilesNeedsUpdate=true;
    if (mBibItem.contains(dlh)) {
        mBibItem.remove(dlh);
        bibTeXFilesNeedsUpdate=true;
    }

	QStringList commands=mUserCommandList.values(dlh);
	foreach(QString elem,commands){
		int i=elem.indexOf("{");
		if(i>=0) elem=elem.left(i);
		ltxCommands.possibleCommands["user"].remove(elem);
		updateSyntaxCheck=true;
	}
	mUserCommandList.remove(dlh);
	
	QStringList removedUsepackages;
	removedUsepackages << mUsepackageList.values(dlh);
	mUsepackageList.remove(dlh);
	
	if(dlh==mAppendixLine){
		updateAppendix(mAppendixLine,0);
		mAppendixLine=0;
	}
	
	int linenr = indexOf(dlh);
	if (linenr==-1) linenr=lines();

    // check if line contains bookmark
    if(edView){
        for(int i=-1;i<10;i++){
            if(edView->hasBookmark(linenr-1,i)){
                emit bookmarkRemoved(linenr-1);
                edView->removeBookmark(dlh,i);
                break;
            }
        }
    }
	
	QList<StructureEntry*> categories=QList<StructureEntry*>() << magicCommentList << labelList << todoList << blockList << bibTeXList;
	foreach (StructureEntry* sec, categories) {
		int l=0;
		QMutableListIterator<StructureEntry*> iter(sec->children);
		while (iter.hasNext()){
			StructureEntry* se=iter.next();
			if(dlh==se->getLineHandle()) {
				emit removeElement(se,l);
				iter.remove();
				emit removeElementFinished();
				delete se;
			} else l++;
		}
	}

	LatexParser& latexParser = LatexParser::getInstance();	
	QVector<StructureEntry*> parent_level(latexParser.structureCommands.count());
	
	QList<StructureEntry*> ls;
	mergeStructure(baseStructure, parent_level, ls, linenr, 1);
	
	// rehighlight current cursor position
	StructureEntry *newSection=0;
	if(edView){
		int i=edView->editor->cursor().lineNumber();
		if(i>=0) {
			newSection=findSectionForLine(i);
		}
	}
	
	emit structureUpdated(this,newSection);
	
	if (bibTeXFilesNeedsUpdate)
		emit updateBibTeXFiles();
	
	if (completerNeedsUpdate || bibTeXFilesNeedsUpdate)
		emit updateCompleter();
	
	if(!removedUsepackages.isEmpty() || updateSyntaxCheck){
		QStringList files=mUsepackageList.values();
		updateCompletionFiles(files,updateSyntaxCheck);
	}
}

void LatexDocument::patchStructure(int linenr, int count) {
	
	if (!baseStructure) return;
	
	emit toBeChanged();
	
	bool completerNeedsUpdate=false;
	bool bibTeXFilesNeedsUpdate=false;
    bool bibItemsChanged=false;
	
	QDocumentLineHandle *oldLine=mAppendixLine; // to detect a change in appendix position
	
	QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfMagicComments;
	QMutableListIterator<StructureEntry*> iter_magicComment(magicCommentList->children);
	findStructureEntryBefore(iter_magicComment,MapOfMagicComments,linenr,count);
	
	QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfLabels;
	QMutableListIterator<StructureEntry*> iter_label(labelList->children);
	findStructureEntryBefore(iter_label,MapOfLabels,linenr,count);
	
	QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfTodo;
	QMutableListIterator<StructureEntry*> iter_todo(todoList->children);
	findStructureEntryBefore(iter_todo,MapOfTodo,linenr,count);
	
	QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfBlock;
	QMutableListIterator<StructureEntry*> iter_block(blockList->children);
	findStructureEntryBefore(iter_block,MapOfBlock,linenr,count);
	QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfBibtex;
	QMutableListIterator<StructureEntry*> iter_bibTeX(bibTeXList->children);
	findStructureEntryBefore(iter_bibTeX,MapOfBibtex,linenr,count);
	
	LatexParser& latexParser = LatexParser::getInstance();
	int verbatimFormat=QDocument::formatFactory()->id("verbatim");
	bool updateSyntaxCheck=false;
	
	QList<StructureEntry*> flatStructure;
	
	// usepackage list
	QStringList removedUsepackages;
	QStringList addedUsepackages;
	QStringList removedUserCommands,addedUserCommands;
	
	//TODO: This assumes one command per line, which is not necessary true
	for (int i=linenr; i<linenr+count; i++) {
        //update bookmarks
        if(edView && edView->hasBookmark(i,-1)){
            emit bookmarkLineUpdated(i);
        }

		QString curLine = line(i).text(); //TODO: use this instead of s
		QVector<int> fmts=line(i).getFormats();
		
		for(int j=0;j<curLine.length() && j < fmts.size();j++){
			if(fmts[j]==verbatimFormat){
				curLine[j]=QChar(' ');
			}
		}
		
		// remove command,bibtex,labels at from this line
		QDocumentLineHandle* dlh=line(i).handle();
		QStringList commands=mUserCommandList.values(dlh);
		foreach(QString elem,commands){
			int i=elem.indexOf("{");
			if(i>=0) elem=elem.left(i);
			ltxCommands.possibleCommands["user"].remove(elem);
			removedUserCommands << elem;
			updateSyntaxCheck=true;
		}
		if (mLabelItem.contains(dlh)) {
			QList<ReferencePair> labels=mLabelItem.values(dlh);
			completerNeedsUpdate = true;
			mLabelItem.remove(dlh);
			foreach(const ReferencePair& rp,labels)
				updateRefsLabels(rp.name);
		}
		mRefItem.remove(dlh);
		if (mUserCommandList.remove(dlh)>0) completerNeedsUpdate = true;
        if(mBibItem.remove(dlh))
            bibTeXFilesNeedsUpdate=true;
		
		removedUsepackages << mUsepackageList.values(dlh);
		if (mUsepackageList.remove(dlh)>0) completerNeedsUpdate = true;
		
		//remove old bibs files from hash, but keeps a temporary copy
		QStringList oldBibs;
		while (mMentionedBibTeXFiles.contains(dlh)) {
			QMultiHash<QDocumentLineHandle*, FileNamePair>::iterator it = mMentionedBibTeXFiles.find(dlh);
			Q_ASSERT(it.key() == dlh);
			Q_ASSERT(it != mMentionedBibTeXFiles.end());
			if (it == mMentionedBibTeXFiles.end()) break;
			oldBibs.append(it.value().relative);
			mMentionedBibTeXFiles.erase(it);
		}
		
		//find entries prior to changed lines
		
		QString name;
		QString arg;
		QString cmd;
		QString remainder;
		int optionStart;
		
		//// TODO marker
		QString s=curLine;
		int l=s.indexOf("%TODO");
		if (l>=0) {
			s=s.mid(l+6,s.length());
			bool reuse=false;
			StructureEntry *newTodo;
			if(MapOfTodo.contains(dlh)){
				newTodo=MapOfTodo.value(dlh);
				//parent->add(newTodo);
				newTodo->type=StructureEntry::SE_TODO;
				MapOfTodo.remove(dlh,newTodo);
				reuse=true;
			}else{
				newTodo=new StructureEntry(this, StructureEntry::SE_TODO);
			}
			newTodo->title=s;
			newTodo->setLine(line(i).handle(), i);
			newTodo->parent=todoList;
			if(!reuse) emit addElement(todoList,todoList->children.size()); //todo: why here but not in label?
			iter_todo.insert(newTodo);
		}
		//// magic comment
		s=curLine;
		l=s.indexOf("% !TeX");
		if (l>=0) {
			s=s.mid(l+6,s.length());
			bool reuse=false;
			StructureEntry *newMagicComment;
			if(MapOfMagicComments.contains(dlh)){
				newMagicComment=MapOfMagicComments.value(dlh);
				newMagicComment->type=StructureEntry::SE_MAGICCOMMENT;
				MapOfMagicComments.remove(dlh,newMagicComment);
				reuse=true;
			}else{
				newMagicComment=new StructureEntry(this, StructureEntry::SE_MAGICCOMMENT);
			}
			QString name;
			QString val;
			splitMagicComment(s, name, val);
			
			parseMagicComment(name, val, newMagicComment);
			newMagicComment->title=s;
			newMagicComment->setLine(line(i).handle(), i);
			newMagicComment->parent=magicCommentList;
			if(!reuse) emit addElement(magicCommentList,magicCommentList->children.size()); //todo: why here but not in label?
			iter_magicComment.insert(newMagicComment);
		}
		////Ref
		//for reference counting (can be placed in command options as well ...
        foreach(QString cmd,latexParser.possibleCommands["%ref"]){
			QString name;
			cmd.append('{');
			int start=0;
			do{
				name=findToken(curLine,cmd,start);
				if(!name.isEmpty()){
					ReferencePair elem;
					elem.name=name;
					elem.start=start;
					mRefItem.insert(line(i).handle(),elem);
				}
			}while(start>=0);
		}
		//// label ////
		//TODO: Use label from dynamical reference checker
        foreach(QString cmd,latexParser.possibleCommands["%label"]){
			QString name;
			cmd.append('{');
			int start=0;
			do{
				name=findToken(curLine,cmd,start);
				if(!name.isEmpty()){
					ReferencePair elem;
					elem.name=name;
					elem.start=start;
					mLabelItem.insert(line(i).handle(),elem);
					completerNeedsUpdate=true;
					StructureEntry *newLabel;
					if(MapOfLabels.contains(dlh)){
						newLabel=MapOfLabels.value(dlh);
						newLabel->type=StructureEntry::SE_LABEL;
						MapOfLabels.remove(dlh,newLabel);
					}else{
						newLabel=new StructureEntry(this, StructureEntry::SE_LABEL);
					}
					newLabel->title=name;
					newLabel->setLine(line(i).handle(), i);
					newLabel->parent=labelList;
					iter_label.insert(newLabel);
				}
			}while(start>=0);
		}


		// check also in command argument, als references might be put there as well...
		//// Appendix keyword
		if (curLine=="\\appendix") {
			oldLine=mAppendixLine;
			mAppendixLine=line(i).handle();
			
		}
		if(line(i).handle()==mAppendixLine && curLine!="\\appendix"){
			oldLine=mAppendixLine;
			mAppendixLine=0;
		}
		//let %\include be processed
		if(curLine.startsWith("%\\include")||curLine.startsWith("%\\input")){
			curLine.replace(0,1,' ');
		}
		int totalLength=curLine.length();
		while(findCommandWithArg(curLine,cmd,name,arg,remainder,optionStart)){
			//update offset
			//store optional arguments []
			
			//copy remainder to curLine for next round
			curLine=remainder;
			int offset=totalLength-curLine.length(); //TODO?? (line was commented out, with todo before)
			//// newcommand ////
			//TODO: handle optional arguments
            if (latexParser.possibleCommands["%definition"].contains(cmd)||ltxCommands.possibleCommands["%definition"].contains(cmd)) {
				completerNeedsUpdate=true;
				QRegExp rx("^\\s*\\[(\\d+)\\](\\[.+\\])?");
				int options=0;
				int def=0;
				if(rx.indexIn(remainder)>-1){
					options=rx.cap(1).toInt(); //returns 0 if conversion fails
					if(!rx.cap(2).isEmpty())
						def=1;
				}
				ltxCommands.possibleCommands["user"].insert(name);
				addedUserCommands << name;
				if(def==1){
					QString helper=name;
					for (int j=0; j<options; j++) {
						if (j==1)
							helper.append("{%<arg1%|%>}");
						if(j>1)
							helper.append(QString("{%<arg%1%>}").arg(j));
					}
					mUserCommandList.insert(line(i).handle(),helper);
					
				}
				for (int j=0; j<options; j++) {
					if (j==0) {
						if(def==0)
							name.append("{%<arg1%|%>}");
						else
							name.append("[%<opt. arg1%|%>]");
					} else
						name.append(QString("{%<arg%1%>}").arg(j+1));
				}
				mUserCommandList.insert(line(i).handle(),name);
				// remove obsolete Overlays (maybe this can be refined
				updateSyntaxCheck=true;
				continue;
			}
			// special treatment \def
			if (cmd=="\\def") {
				completerNeedsUpdate=true;
				QRegExp rx("(\\\\\\w+)\\s*(#\\d+)?");
				if(rx.indexIn(remainder)>-1){
					QString name=rx.cap(1);
					QString optionStr=rx.cap(2);
                    //qDebug()<< name << ":"<< optionStr;
					int options=optionStr.mid(1).toInt(); //returns 0 if conversion fails
					ltxCommands.possibleCommands["user"].insert(name);
					addedUserCommands << name;
					for (int j=0; j<options; j++) {
						if (j==0) name.append("{%<arg1%|%>}");
						else name.append(QString("{%<arg%1%>}").arg(j+1));
					}
					mUserCommandList.insert(line(i).handle(),name);
					// remove obsolete Overlays (maybe this can be refined
					updateSyntaxCheck=true;
				}
				continue;
			}
			
			//// newenvironment ////
			static const QStringList envTokens = QStringList() << "\\newenvironment" << "\\renewenvironment";
			if (envTokens.contains(cmd)) {
				completerNeedsUpdate=true;
				int options=arg.toInt(); //returns 0 if conversion fails
				name.append("}");
				mUserCommandList.insert(line(i).handle(),"\\end{"+name);
				QStringList lst;
				lst << "\\begin{"+name << "\\end{"+name;
				foreach(const QString& elem,lst){
					ltxCommands.possibleCommands["user"].insert(elem);
					if(!removedUserCommands.removeAll(elem)){
						addedUserCommands << elem;
					}
				}
				for (int j=0; j<options; j++) {
					if (j==0) name.append("{%<1%|%>}");
					else name.append(QString("{%<%1%>}").arg(j+1));
				}
				//mUserCommandList.insert(line(i).handle(),name);//???
				mUserCommandList.insert(line(i).handle(),"\\begin{"+name);
				continue;
			}
			//// newtheorem ////
			if (cmd=="\\newtheorem" || cmd=="\\newtheorem*") {
				completerNeedsUpdate=true;
				QStringList lst;
				lst << "\\begin{"+name+"}" << "\\end{"+name+"}";
				foreach(const QString& elem,lst){
					mUserCommandList.insert(line(i).handle(),elem);
					ltxCommands.possibleCommands["user"].insert(elem);
					if(!removedUserCommands.removeAll(elem)){
						addedUserCommands << elem;
					}
				}
				continue;
			}
            //// newcounter ////
            if (cmd=="\\newcounter") {
                completerNeedsUpdate=true;
                QStringList lst;
                lst << "\\the"+name ;
                foreach(const QString& elem,lst){
                    mUserCommandList.insert(line(i).handle(),elem);
                    ltxCommands.possibleCommands["user"].insert(elem);
                    if(!removedUserCommands.removeAll(elem)){
                        addedUserCommands << elem;
                    }
                }
                continue;
            }
            /// bibitem ///
            if(latexParser.possibleCommands["%bibitem"].contains(cmd)){
                if(!name.isEmpty()){
                    ReferencePair elem;
                    elem.name=name;
                    elem.start=optionStart;
                    mBibItem.insert(line(i).handle(),elem);
                    bibItemsChanged=true;
                    continue;
                }
            }
			///usepackage
            if (latexParser.possibleCommands["%usepackage"].contains(cmd)) {
		completerNeedsUpdate=true;
		QStringList packagesHelper=name.split(",");
                if(cmd.endsWith("theme")){ // special treatment for  \usetheme
                    QString preambel=cmd;
                    preambel.remove(0,4);
                    preambel.prepend("beamer");
                    packagesHelper.replaceInStrings(QRegExp("^"),preambel);
                }
		QStringList packages;
		foreach(const QString& elem,packagesHelper)
		    if(latexParser.packageAliases.contains(elem))
			packages << latexParser.packageAliases.values(elem);
		else
		    packages << elem;

		foreach(const QString& elem,packages){
		    if(!removedUsepackages.removeAll(elem))
			addedUsepackages << elem;
		    mUsepackageList.insertMulti(dlh,elem);
		}
		continue;
	    }
	    //// bibliography ////
            if (latexParser.possibleCommands["%bibliography"].contains(cmd)) {
				QStringList bibs=name.split(',',QString::SkipEmptyParts);
				//add new bibs and set bibTeXFilesNeedsUpdate if there was any change
				foreach(const QString& elem,bibs){ //latex doesn't seem to allow any spaces in file names
					mMentionedBibTeXFiles.insert(line(i).handle(),FileNamePair(elem,getAbsoluteFilePath(elem,"bib")));					
					if (oldBibs.removeAll(elem) == 0)
						bibTeXFilesNeedsUpdate = true;
				}
				//write bib tex in tree
				foreach (const QString& bibFile, bibs) {
					StructureEntry *newFile;
					if(MapOfBibtex.contains(dlh)){
						newFile=MapOfBibtex.value(dlh);
						newFile->type=StructureEntry::SE_BIBTEX;
						MapOfBibtex.remove(dlh,newFile);
					}else{
						newFile=new StructureEntry(this, StructureEntry::SE_BIBTEX);
					}
					newFile->title=bibFile;
					newFile->setLine(line(i).handle(), i);
					newFile->parent=bibTeXList;
					iter_bibTeX.insert(newFile);
				}
				continue;
			}
			
			//// beamer blocks ////
			
			if (cmd=="\\begin" && name=="block") {
				QString s=extractSectionName(remainder,true);
				StructureEntry *newBlock;
				if(MapOfBlock.contains(dlh)){
					newBlock=MapOfBlock.value(dlh);
					newBlock->type=StructureEntry::SE_BLOCK;
					MapOfBlock.remove(dlh,newBlock);
				}else{
					newBlock=new StructureEntry(this, StructureEntry::SE_BLOCK);
				}
				newBlock->title=s;
				newBlock->setLine(line(i).handle(), i);
				newBlock->parent=blockList;
				iter_block.insert(newBlock);
				continue;
			}
			
			//// include,input ////
			//static const QStringList inputTokens = QStringList() << "\\input" << "\\include";
			
			if (latexParser.includeCommands.contains(cmd)) {
				StructureEntry *newInclude=new StructureEntry(this, StructureEntry::SE_INCLUDE);
				newInclude->title=name;
				QString fname=findFileName(name);
				LatexDocument* dc=parent->findDocumentFromName(fname);
				if(dc)	dc->setMasterDocument(this);
				newInclude->valid = dc; 
				newInclude->setLine(line(i).handle(), i);
				newInclude->columnNumber = offset;
				flatStructure << newInclude;
				continue;
			}
			//// all sections ////
			if(cmd.endsWith("*"))
				cmd=cmd.left(cmd.length()-1);
			int header=latexParser.structureCommands.indexOf(cmd);
			if (header>-1) {
				StructureEntry *newSection = new StructureEntry(this,StructureEntry::SE_SECTION);
				if(mAppendixLine &&indexOf(mAppendixLine)<i) newSection->appendix=true;
				else newSection->appendix=false;
				newSection->title=parseTexOrPDFString(name);
				newSection->level=header;
				newSection->setLine(line(i).handle(), i);
				newSection->columnNumber = offset;
				flatStructure << newSection;
			}
		}// for each command
		
		if (!oldBibs.isEmpty())
			bibTeXFilesNeedsUpdate = true; //file name removed
		
	}//for each line handle
	
	QVector<StructureEntry*> parent_level(latexParser.structureCommands.count());
	mergeStructure(baseStructure, parent_level, flatStructure, linenr, count);
	
	const QList<StructureEntry*> categories=
	              QList<StructureEntry*>() << magicCommentList << blockList << labelList << todoList << bibTeXList;
		
	for (int i=categories.size()-1;i>=0;i--) {
		StructureEntry *cat = categories[i];
		if (cat->children.isEmpty() == (cat->parent == 0)) continue;
		if (cat->children.isEmpty()) removeElementWithSignal(cat);
		else insertElementWithSignal(baseStructure, 0, cat);
	}
	
	//update appendix change
	if(oldLine!=mAppendixLine){
		updateAppendix(oldLine,mAppendixLine);
	}
	
	// rehighlight current cursor position
	StructureEntry *newSection=0;
	if(edView){
		int i=edView->editor->cursor().lineNumber();
		if(i>=0) {
			newSection=findSectionForLine(i);
		}
	}
	
	emit structureUpdated(this,newSection);

	StructureEntry* se;	
	foreach(se,MapOfTodo.values())
		delete se;
	
	foreach(se,MapOfBibtex.values())
		delete se;
	
	foreach(se,MapOfBlock.values())
		delete se;
	
	foreach(se,MapOfLabels.values())
		delete se;
	
	foreach(se,MapOfMagicComments.values())
		delete se;
	
	if(!addedUsepackages.isEmpty() || !removedUsepackages.isEmpty() || !addedUserCommands.isEmpty() || !removedUserCommands.isEmpty()){
		bool forceUpdate=!addedUserCommands.isEmpty() || !removedUserCommands.isEmpty();
		QStringList files=mUsepackageList.values();
		updateCompletionFiles(files,forceUpdate);
	}
	
	if (bibTeXFilesNeedsUpdate)
		emit updateBibTeXFiles();

    if(bibItemsChanged)
        parent->updateBibFiles(false);
	
	if (completerNeedsUpdate || bibTeXFilesNeedsUpdate)
		emit updateCompleter();
	
	if(updateSyntaxCheck) {
		foreach(LatexDocument* elem,getListOfDocs()){
			//getEditorView()->reCheckSyntax();//todo: signal
			if(elem->edView)
				elem->edView->reCheckSyntax();
		}
	}
	
	//update view
	if(edView)
		edView->documentContentChanged(linenr, count);
	
	
#ifndef QT_NO_DEBUG
	checkForLeak();
#endif
	
	
}

#ifndef QT_NO_DEBUG

void LatexDocument::checkForLeak(){
	StructureEntryIterator iter(baseStructure);
	QSet<StructureEntry*>zw=StructureContent;
	while (iter.hasNext()){
		zw.remove(iter.next());
	}
	
	// filter top level elements
	QMutableSetIterator<StructureEntry *> i(zw);
	while (i.hasNext())
		if(i.next()->type==StructureEntry::SE_OVERVIEW) i.remove();
	
	if(zw.count()>0){
		qDebug("Memory leak in structure");
	}
}

#endif

StructureEntry * LatexDocument::findSectionForLine(int currentLine){
	StructureEntryIterator iter(baseStructure);
	StructureEntry *newSection=0;
	
	while (/*iter.hasNext()*/true){
		StructureEntry *curSection=0;
		while (iter.hasNext()){
			curSection=iter.next();
			if (curSection->type==StructureEntry::SE_SECTION)
				break;
		}
		if (curSection==0 || curSection->type!=StructureEntry::SE_SECTION)
			break;
		
		if (curSection->getRealLineNumber() > currentLine) break; //curSection is after newSection where the cursor is
		else newSection=curSection;
	}
	if(newSection && newSection->getRealLineNumber()>currentLine) newSection=0;
	
	return newSection;
}

void LatexDocument::setTemporaryFileName(const QString& fileName){
	temporaryFileName=fileName;
}

QString LatexDocument::getTemporaryFileName() const{
	return temporaryFileName;
}

int LatexDocument::countLabels(const QString& name){
	int result=0;
	foreach(const LatexDocument *elem,getListOfDocs()){
		QStringList items=elem->labelItems();
		result+=items.count(name);
	}
	return result;
}

int LatexDocument::countRefs(const QString& name){
	int result=0;
	foreach(const LatexDocument *elem,getListOfDocs()){
		QStringList items=elem->refItems();
		result+=items.count(name);
	}
	return result;
}

QMultiHash<QDocumentLineHandle*,int> LatexDocument::getBibItems(const QString& name){
    QHash<QDocumentLineHandle*,int> result;
    foreach(const LatexDocument *elem,getListOfDocs()){
        QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
        for (it = elem->mBibItem.constBegin(); it != elem->mBibItem.constEnd(); ++it){
            ReferencePair rp=it.value();
            if(rp.name==name){
                result.insert(it.key(),rp.start);
            }
        }
    }
    return result;
}


QMultiHash<QDocumentLineHandle*,int> LatexDocument::getLabels(const QString& name){
	QHash<QDocumentLineHandle*,int> result;
	foreach(const LatexDocument *elem,getListOfDocs()){
		QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
		for (it = elem->mLabelItem.constBegin(); it != elem->mLabelItem.constEnd(); ++it){
			ReferencePair rp=it.value();
			if(rp.name==name){
				result.insert(it.key(),rp.start);
			}
		}
	}
	return result;
}

QMultiHash<QDocumentLineHandle*,int> LatexDocument::getRefs(const QString& name){
	QHash<QDocumentLineHandle*,int> result;
	foreach(const LatexDocument *elem,getListOfDocs()){
		QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
		for (it = elem->mRefItem.constBegin(); it != elem->mRefItem.constEnd(); ++it){
			ReferencePair rp=it.value();
			if(rp.name==name){
				result.insert(it.key(),rp.start);
			}
		}
	}
	return result;
}

void LatexDocument::setMasterDocument(LatexDocument* doc){
	masterDocument=doc;
	QList<LatexDocument *>listOfDocs=getListOfDocs();
	foreach(LatexDocument *elem,listOfDocs){
		elem->recheckRefsLabels();
	}
}

QList<LatexDocument *>LatexDocument::getListOfDocs(QSet<LatexDocument*> *visitedDocs){
	QList<LatexDocument *>listOfDocs;
	bool deleteVisitedDocs=false;
	if(parent->masterDocument){
		listOfDocs=parent->documents;
	}else{
		LatexDocument *master=this;
		if(!visitedDocs){
			visitedDocs=new QSet<LatexDocument*>();
			deleteVisitedDocs=true;
		}
		foreach(LatexDocument *elem,parent->documents){ // check children
			if(elem!=master && elem->masterDocument!=master) continue;
			if(visitedDocs && !visitedDocs->contains(elem)){
				listOfDocs << elem;
				visitedDocs->insert(elem);
				listOfDocs << elem->getListOfDocs(visitedDocs);
			}
		}
		if(masterDocument){ //check masters
			master=masterDocument;
			if(!visitedDocs->contains(master))
				listOfDocs << master->getListOfDocs(visitedDocs);
		}
	}
	if(deleteVisitedDocs)
		delete visitedDocs;
	return listOfDocs;
}

void LatexDocument::recheckRefsLabels(){
	// get occurences (refs)
	int referenceMultipleFormat=QDocument::formatFactory()->id("referenceMultiple");
	int referencePresentFormat=QDocument::formatFactory()->id("referencePresent");
	int referenceMissingFormat=QDocument::formatFactory()->id("referenceMissing");
	
	QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
	for(it=mLabelItem.constBegin();it!=mLabelItem.constEnd();++it){
		QDocumentLineHandle* dlh=it.key();
		foreach(const ReferencePair& rp,mLabelItem.values(dlh)){
			int cnt=countLabels(rp.name);
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referenceMultipleFormat));
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referencePresentFormat));
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referenceMissingFormat));
			if (cnt>1) {
				dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referenceMultipleFormat));
			} else if (cnt==1) dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referencePresentFormat));
			else dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referenceMissingFormat));
		}
	}
	for(it=mRefItem.constBegin();it!=mRefItem.constEnd();++it){
		QDocumentLineHandle* dlh=it.key();
		foreach(const ReferencePair& rp,mRefItem.values(dlh)){
			int cnt=countLabels(rp.name);
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referenceMultipleFormat));
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referencePresentFormat));
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referenceMissingFormat));
			if (cnt>1) {
				dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referenceMultipleFormat));
			} else if (cnt==1) dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referencePresentFormat));
			else dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referenceMissingFormat));
		}
	}
}

QStringList LatexDocument::someItems(const QMultiHash<QDocumentLineHandle*,ReferencePair>& list){
	QList<ReferencePair> lst=list.values();
	QStringList result;
	foreach(const ReferencePair& elem,lst){
		result << elem.name;
	}
	
	return result;
}

QStringList LatexDocument::labelItems() const{
	return someItems(mLabelItem);
}

QStringList LatexDocument::refItems() const{
	return someItems(mRefItem);
}

QStringList LatexDocument::bibItems() const{
    return someItems(mBibItem);
}

void LatexDocument::updateRefsLabels(const QString& ref){
	// get occurences (refs)
	int referenceMultipleFormat=QDocument::formatFactory()->id("referenceMultiple");
	int referencePresentFormat=QDocument::formatFactory()->id("referencePresent");
	int referenceMissingFormat=QDocument::formatFactory()->id("referenceMissing");
	
	int cnt=countLabels(ref);
	QMultiHash<QDocumentLineHandle*,int> occurences=getLabels(ref);
	occurences+=getRefs(ref);
	QMultiHash<QDocumentLineHandle*,int>::const_iterator it;
	for(it=occurences.constBegin();it!=occurences.constEnd();++it){
		QDocumentLineHandle* dlh=it.key();
		foreach(const int pos,occurences.values(dlh)){
			dlh->removeOverlay(QFormatRange(pos,ref.length(),referenceMultipleFormat));
			dlh->removeOverlay(QFormatRange(pos,ref.length(),referencePresentFormat));
			dlh->removeOverlay(QFormatRange(pos,ref.length(),referenceMissingFormat));
			if (cnt>1) {
				dlh->addOverlay(QFormatRange(pos,ref.length(),referenceMultipleFormat));
			} else if (cnt==1) dlh->addOverlay(QFormatRange(pos,ref.length(),referencePresentFormat));
			else dlh->addOverlay(QFormatRange(pos,ref.length(),referenceMissingFormat));
		}
	}
}

/*
void LatexDocument::includeDocument(LatexDocument* includedDocument){
 includedDocument->deleteLater();
 if (texFiles.contains(includedDocument->fileName))
  return; //this should never happen
  
 texFiles.unite(includedDocument->texFiles);
 containedLabels.unite(includedDocument->containedLabels);
 containedReferences.unite(includedDocument->containedReferences);
 mentionedBibTeXFiles.unite(includedDocument->mentionedBibTeXFiles);
 allBibTeXIds.unite(includedDocument->allBibTeXIds);
 
}
*/
StructureEntry::StructureEntry(LatexDocument* doc, Type newType):type(newType),level(0), parent(0), document(doc),appendix(false), parentRow(-1), lineHandle(0), lineNumber(-1){
#ifndef QT_NO_DEBUG
	Q_ASSERT(document);
	document->StructureContent.insert(this);
#endif
}
StructureEntry::~StructureEntry(){
	level=-1; //invalidate entry
	foreach (StructureEntry* se, children)
		delete se;
#ifndef QT_NO_DEBUG
	Q_ASSERT(document);
	bool removed = document->StructureContent.remove(this);
	Q_ASSERT(removed); //prevent double deletion
#endif
}
void StructureEntry::add(StructureEntry* child){
	Q_ASSERT(child!=0);
	children.append(child);
	child->parent=this;
}
void StructureEntry::insert(int pos, StructureEntry* child){
	Q_ASSERT(child!=0);
	children.insert(pos,child);
	child->parent=this;
}
void StructureEntry::setLine(QDocumentLineHandle* handle, int lineNr){
	lineHandle = handle;
	lineNumber = lineNr;
}

QDocumentLineHandle* StructureEntry::getLineHandle() const{
	return lineHandle;
}

int StructureEntry::getCachedLineNumber() const{
	return lineNumber;
}
int StructureEntry::getRealLineNumber() const{
	lineNumber = document->indexOf(lineHandle, lineNumber);
	Q_ASSERT(lineNumber == -1 || document->line(lineNumber).handle() == lineHandle);
	return lineNumber;
}

template <typename T> inline int hintedIndexOf (const QList<T*>& list, const T* elem, int hint) {
	if (hint < 2) return list.indexOf(const_cast<T*>(elem));
	int backward = hint, forward = hint + 1;
	for (;backward >= 0 && forward < list.size();
	     backward--, forward++) {
		if (list[backward] == elem) return backward;
		if (list[forward] == elem) return forward;
	}
	if (backward >= list.size()) backward = list.size() - 1;
	for (;backward >= 0; backward--)
		if (list[backward] == elem) return backward;
	if (forward < 0) forward = 0;
	for (;forward < list.size(); forward++)
		if (list[forward] == elem) return forward;
	return -1;
}

int StructureEntry::getRealParentRow() const{
	REQUIRE_RET(parent, -1);
	parentRow = hintedIndexOf<StructureEntry>(parent->children, this, parentRow);
	return parentRow;
}

void StructureEntry::debugPrint(const char* message) const{
	qDebug("%s %p",message, this);
	if (!this) return;
	qDebug("   level: %i",level);
	qDebug("   type: %i",(int)type);
	qDebug("   line nr: %i", lineNumber);
	qDebug("   title: %s", qPrintable(title));
}

StructureEntryIterator::StructureEntryIterator(StructureEntry* entry){
	if (!entry) return;
	while (entry->parent){
		entryHierarchy.prepend(entry);
		indexHierarchy.prepend(entry->getRealParentRow());
		entry=entry->parent;
	}
	entryHierarchy.prepend(entry);
	indexHierarchy.prepend(0);
}
bool StructureEntryIterator::hasNext(){
	return !entryHierarchy.isEmpty();
}
StructureEntry* StructureEntryIterator::next(){
	if (!hasNext()) return 0;
	StructureEntry* result=entryHierarchy.last();
	if (!result->children.isEmpty()) { //first child is next element, go a level deeper
		entryHierarchy.append(result->children.at(0));
		indexHierarchy.append(0);
	} else { //no child, go to next on same level
		entryHierarchy.removeLast();
		indexHierarchy.last()++;
		while (!entryHierarchy.isEmpty() && indexHierarchy.last()>=entryHierarchy.last()->children.size()) {
			//doesn't exists, proceed to travel upwards
			entryHierarchy.removeLast();
			indexHierarchy.removeLast();
			indexHierarchy.last()++;
		}
		if (!entryHierarchy.isEmpty())
			entryHierarchy.append(entryHierarchy.last()->children.at(indexHierarchy.last()));
	}
	return result;
}

LatexDocumentsModel::LatexDocumentsModel(LatexDocuments& docs):documents(docs),
       iconDocument(":/images/doc.png"), iconMasterDocument(":/images/masterdoc.png"), iconBibTeX(":/images/bibtex.png"), iconInclude(":/images/include.png"),
       iconWarning(getRealIcon("warning")), m_singleMode(false){
	mHighlightIndex=QModelIndex();
	iconSection.resize(LatexParser::getInstance().structureCommands.count());
	for (int i=0;i<LatexParser::getInstance().structureCommands.count();i++)
		iconSection[i]=QIcon(":/images/"+LatexParser::getInstance().structureCommands[i].mid(1)+".png");
}
Qt::ItemFlags LatexDocumentsModel::flags ( const QModelIndex & index ) const{
	if (index.isValid()) return Qt::ItemIsEnabled|Qt::ItemIsSelectable;
	else return 0;
}
QVariant LatexDocumentsModel::data ( const QModelIndex & index, int role) const{
	if (!index.isValid()) return QVariant();
	StructureEntry* entry = (StructureEntry*) index.internalPointer();
	if (!entry) return QVariant();
	QString result;
	switch (role) {
	case Qt::DisplayRole:
		if (entry->type==StructureEntry::SE_DOCUMENT_ROOT){ //show only base file name
			QString title=entry->title.mid(1+qMax(entry->title.lastIndexOf("/"), entry->title.lastIndexOf(QDir::separator())));
			if(title.isEmpty()) title=tr("untitled");
			return QVariant(title);
		}
		//show full title in other cases
		if(documents.showLineNumbersInStructure && entry->getCachedLineNumber()>-1){
			result=entry->title+QString(tr(" (Line %1)").arg(entry->getRealLineNumber()+1));
		}else{
			result=entry->title;
		}
		return QVariant(result);
	case Qt::ToolTipRole:
		//qDebug("data %x",entry);
		if (!entry->tooltip.isNull()) {
			return QVariant(entry->tooltip);
		}
		if (entry->type==StructureEntry::SE_DOCUMENT_ROOT) {
			return QVariant(entry->document->getFileName());
		}
		if (entry->type==StructureEntry::SE_SECTION) {
			QString tooltip(entry->title);
			if (entry->getCachedLineNumber()>-1)
				tooltip.append("\n"+tr("Line")+QString(": %1").arg(entry->getRealLineNumber()+1));
			StructureEntry *se = LatexDocumentsModel::labelForStructureEntry(entry);
			if (se)
				tooltip.append("\n"+tr("Label")+": "+se->title);
			return QVariant(tooltip);
		}
		if (entry->getCachedLineNumber()>-1)
			return QVariant(entry->title+QString(tr(" (Line %1)").arg(entry->getRealLineNumber()+1)));
		else
			return QVariant();
	case Qt::DecorationRole:
		switch (entry->type){
		case StructureEntry::SE_BIBTEX: return iconBibTeX;
		case StructureEntry::SE_INCLUDE: return iconInclude;
		case StructureEntry::SE_MAGICCOMMENT:
			if (entry->valid)
				return QVariant();
			else
				return iconWarning;
		case StructureEntry::SE_SECTION:
			if (entry->level>=0 && entry->level<iconSection.count())
				return iconSection[entry->level];
			else
				return QVariant();
		case StructureEntry::SE_DOCUMENT_ROOT:
			if (documents.masterDocument==entry->document)
				return iconMasterDocument;
			else
				return iconDocument;
		default: return QVariant();
		}
	case Qt::BackgroundRole:
		if (index==mHighlightIndex) return QVariant(Qt::lightGray);
		if (entry->appendix) return QVariant(QColor(200,230,200));
		else return QVariant();
	case Qt::ForegroundRole:
		if((entry->type==StructureEntry::SE_INCLUDE) && (entry->valid)) {
			return QVariant(Qt::red);
		}else return QVariant();
	case Qt::FontRole:
		if(entry->type==StructureEntry::SE_DOCUMENT_ROOT) {
			QFont f=QApplication::font();
			if(entry->document==documents.currentDocument) f.setBold(true);
			if(entry->title.isEmpty()) f.setItalic(true);
			return QVariant(f);
		}
		return QVariant();
	default:
		return QVariant();
	}
}
QVariant LatexDocumentsModel::headerData ( int section, Qt::Orientation orientation, int role ) const{
	Q_UNUSED(orientation);
	if (section!=0) return QVariant();
	if (role!=Qt::DisplayRole) return QVariant();
	return QVariant("Structure");
}
int LatexDocumentsModel::rowCount ( const QModelIndex & parent ) const{
	if (!parent.isValid()) return documents.documents.count();
	else {
		StructureEntry* entry = (StructureEntry*) parent.internalPointer();
		if (!entry) return 0;
		return entry->children.size();
	}
}
int LatexDocumentsModel::columnCount ( const QModelIndex & parent ) const{
	Q_UNUSED(parent);
	return 1;
}
QModelIndex LatexDocumentsModel::index ( int row, int column, const QModelIndex & parent ) const{
	if (column!=0) return QModelIndex(); //one column
	if (row<0) return QModelIndex(); //shouldn't happen
	if (parent.isValid()) {
		const StructureEntry* entry = (StructureEntry*) parent.internalPointer();
		if (!entry) return QModelIndex(); //should never happen
		if (row>=entry->children.size()) return QModelIndex(); //shouldn't happen in a correct view
		return createIndex(row,column, entry->children.at(row));
	} else {
		if (row>=documents.documents.size()) return QModelIndex();
		if(m_singleMode){
			if(row!=0 || !documents.currentDocument )
				return QModelIndex();
			else
				return createIndex(row, column, documents.currentDocument->baseStructure);
		}else{
			return createIndex(row, column, documents.documents.at(row)->baseStructure);
		}
	}
}
QModelIndex LatexDocumentsModel::index ( StructureEntry* entry ) const{
	if (!entry) return QModelIndex();
	if (entry->parent==0 && entry->type==StructureEntry::SE_DOCUMENT_ROOT) {
		int row=documents.documents.indexOf(entry->document);
		if(m_singleMode){
			row=0;
		}
		if (row<0) return QModelIndex();
		return createIndex(row, 0, entry);
	} else if (entry->parent!=0 && entry->type!=StructureEntry::SE_DOCUMENT_ROOT) {
		int row=entry->getRealParentRow();
		if (row<0) return QModelIndex(); //shouldn't happen
		return createIndex(row, 0, entry);
	} else return QModelIndex(); //shouldn't happen
}

QModelIndex LatexDocumentsModel::parent ( const QModelIndex & index ) const{
	if (!index.isValid()) return QModelIndex();
	const StructureEntry* entry = (StructureEntry*) index.internalPointer();
#ifndef QT_NO_DEBUG
	const LatexDocument* found = 0;
	foreach (const LatexDocument* ld, documents.documents)
		if (ld->StructureContent.contains(const_cast<StructureEntry*>(entry))) {
			found = ld;
			break;
		}
	if (!found) entry->debugPrint("No document for entry:");
	Q_ASSERT(found);
	Q_ASSERT(entry->document == found);
#endif
	if (!entry) return QModelIndex();
	if (!entry->parent) return QModelIndex();
	
	if(entry->level>LatexParser::getInstance().structureCommands.count() || entry->level<0){
		entry->debugPrint("Structure broken!");
		//qDebug("Title %s",qPrintable(entry->title));
		return QModelIndex();
	}
	if(entry->parent->level>LatexParser::getInstance().structureCommands.count() || entry->parent->level<0){
		entry->debugPrint("Structure broken (b)!");
		//qDebug("Title %s",qPrintable(entry->title));
		return QModelIndex();
	}
	if (entry->parent->parent)
		return createIndex(entry->parent->getRealParentRow(), 0, entry->parent);
	else {
		if(m_singleMode)
			return createIndex(0, 0, entry->parent);
		for (int i=0; i < documents.documents.count(); i++)
			if (documents.documents.at(i)->baseStructure==entry->parent)
				return createIndex(i, 0, entry->parent);
		return QModelIndex();
	}
}

StructureEntry* LatexDocumentsModel::indexToStructureEntry(const QModelIndex & index ){
	if (!index.isValid()) return 0;
	StructureEntry* result=(StructureEntry*)index.internalPointer();
	if (!result || !result->document) return 0;
	return result;
}

/*!
	Returns an associated SE_LABEL entry for a structure element if one exists, otherwise 0.
	TODO: currently association is checked, by checking, if the label is on the same line.
	This is not necessarily correct. It fails if:
	 - there are multiple labels on one line (always the first label is chosen)
	 - the label is on a later line (label not detected)
*/
StructureEntry *LatexDocumentsModel::labelForStructureEntry(const StructureEntry *entry)
{
	StructureEntryIterator iter(entry->document->baseStructure);
	QDocumentLineHandle *dlh = entry->getLineHandle();

	while (iter.hasNext()){
		StructureEntry *se = iter.next();
		if (se->type==StructureEntry::SE_LABEL) {
			if (se->getLineHandle() == dlh) {
				return se;
			}
		}
	}
	return 0;
}

QModelIndex LatexDocumentsModel::highlightedEntry(){
	return mHighlightIndex;
}
void LatexDocumentsModel::setHighlightedEntry(StructureEntry* entry){
	
	QModelIndex i1=mHighlightIndex;
	QModelIndex i2=index(entry);
	if (i1==i2) return;
	emit dataChanged(i1,i1);
	mHighlightIndex=i2;
	emit dataChanged(i2,i2);
}

void LatexDocumentsModel::resetAll(){
	mHighlightIndex=QModelIndex();
	reset();
}

void LatexDocumentsModel::resetHighlight(){
	mHighlightIndex=QModelIndex();
}

void LatexDocumentsModel::structureUpdated(LatexDocument* document,StructureEntry *highlight){
	Q_UNUSED(document);
	//resetAll();
	if(highlight){
		mHighlightIndex=index(highlight);
	}else{
		mHighlightIndex=QModelIndex();
	}
	emit layoutChanged();
	//emit resetAll();
}
void LatexDocumentsModel::structureLost(LatexDocument* document){
	Q_UNUSED(document);
	resetAll();
}

void LatexDocumentsModel::removeElement(StructureEntry *se,int row){
	REQUIRE(se);
	if (!se->parent) 
		beginRemoveRows(QModelIndex(),row,row); // remove from root (documents)
	else {
		if(row<0) row=se->getRealParentRow();
		else Q_ASSERT(row < se->parent->children.size()), Q_ASSERT(se->parent->children[row] == se);
		beginRemoveRows(index(se->parent),row,row);
	}
}

void LatexDocumentsModel::removeElementFinished(){
	endRemoveRows();
}

void LatexDocumentsModel::addElement(StructureEntry *se, int row){
	beginInsertRows(index(se),row,row);
}

void LatexDocumentsModel::addElementFinished(){
	endInsertRows();
}

void LatexDocumentsModel::updateElement(StructureEntry *se){
	emit dataChanged(index(se),index(se));
}
void LatexDocumentsModel::setSingleDocMode(bool singleMode){
	if(m_singleMode!=singleMode){
		//resetAll();
		if(singleMode){
			foreach(LatexDocument *doc,documents.documents){
				changePersistentIndex(index(doc->baseStructure),createIndex(0,0,doc->baseStructure));
			}
		}else{
			for(int i=0;i<documents.documents.count();i++){
				StructureEntry *se=documents.documents.at(i)->baseStructure;
				changePersistentIndex(index(se),createIndex(i,0,se));
			}
		}
		m_singleMode=singleMode;
	}
	structureUpdated(documents.currentDocument,0);
}

void LatexDocumentsModel::moveDocs(int from,int to){ //work only for adjacent elements !!!
    Q_ASSERT(abs(from-to)==1);
    StructureEntry *se=documents.documents.at(from)->baseStructure;
    changePersistentIndex(index(se),createIndex(to,0,se));
    se=documents.documents.at(to)->baseStructure;
    changePersistentIndex(index(se),createIndex(from,0,se));
}

bool LatexDocumentsModel::getSingleDocMode(){
	return m_singleMode;
}

LatexDocuments::LatexDocuments(): model(new LatexDocumentsModel(*this)), masterDocument(0), currentDocument(0), bibTeXFilesModified(false){
	showLineNumbersInStructure=false;
	indentationInStructure=-1;
}

LatexDocuments::~LatexDocuments(){
	delete model;
}

void LatexDocuments::addDocument(LatexDocument* document){
	documents.append(document);
	connect(document, SIGNAL(updateBibTeXFiles()), SLOT(bibTeXFilesNeedUpdate()));
	connect(document,SIGNAL(structureLost(LatexDocument*)),model,SLOT(structureLost(LatexDocument*)));
	connect(document,SIGNAL(structureUpdated(LatexDocument*,StructureEntry*)),model,SLOT(structureUpdated(LatexDocument*,StructureEntry*)));
	connect(document,SIGNAL(toBeChanged()),model,SIGNAL(layoutAboutToBeChanged()));
	connect(document,SIGNAL(removeElement(StructureEntry*,int)),model,SLOT(removeElement(StructureEntry*,int)));
	connect(document,SIGNAL(removeElementFinished()),model,SLOT(removeElementFinished()));
	connect(document,SIGNAL(addElement(StructureEntry*,int)),model,SLOT(addElement(StructureEntry*,int)));
	connect(document,SIGNAL(addElementFinished()),model,SLOT(addElementFinished()));
	connect(document,SIGNAL(updateElement(StructureEntry*)),model,SLOT(updateElement(StructureEntry*)));
	document->parent=this;
	if(masterDocument){
		// repaint all docs
		foreach(const LatexDocument *doc,documents){
			LatexEditorView *edView=doc->getEditorView();
			if (edView) edView->documentContentChanged(0,edView->editor->document()->lines());
		}
	}
	model->structureUpdated(document,0);
}
void LatexDocuments::deleteDocument(LatexDocument* document){
	emit aboutToDeleteDocument(document);
	LatexEditorView *view=document->getEditorView();
	if(view)
		view->closeCompleter();
	if (document!=masterDocument) {
		// get list of all affected documents
		QList<LatexDocument*> lstOfDocs=document->getListOfDocs();
		// set document.masterdocument = 0
		foreach(LatexDocument* elem,lstOfDocs){
			if(elem->getMasterDocument()==document){
				elem->setMasterDocument(0);
			}
		}
		document->setMasterDocument(0);
		//recheck labels in all open documents connected to this document
		foreach(LatexDocument* elem,lstOfDocs){
			elem->recheckRefsLabels();
		}
		int row=documents.indexOf(document);
        //qDebug()<<document->getFileName()<<row;
		if (!document->baseStructure) row = -1; //may happen directly after reload (but won't)
		if(model->getSingleDocMode()){
			row=0;
		}
		if(row>=0 ){//&& !model->getSingleDocMode()){
			model->resetHighlight();
			model->removeElement(document->baseStructure,row); //remove from root
		}
		documents.removeAll(document);
		if (document==currentDocument){
			currentDocument=0;
		}
		if(row>=0 ){//&& !model->getSingleDocMode()){
			model->removeElementFinished();
		}
		//model->resetAll();
		if (view) delete view;
		delete document;
	} else {
		document->setFileName(document->getFileName());
		model->resetAll();
		document->clearAppendix();
		if (view) delete view;
		if (document==currentDocument)
			currentDocument=0;
	}
}
void LatexDocuments::setMasterDocument(LatexDocument* document){
	if (document==masterDocument) return;
	if (masterDocument!=0 && masterDocument->getEditorView()==0){
		documents.removeAll(masterDocument);
		delete masterDocument;
	}
	masterDocument=document;
	if (masterDocument!=0) {
		documents.removeAll(masterDocument);
		documents.prepend(masterDocument);
		// repaint doc
		foreach(LatexDocument *doc,documents){
			LatexEditorView *edView=doc->getEditorView();
			if (edView) edView->documentContentChanged(0,edView->editor->document()->lines());
		}
	}
	model->resetAll();
	emit masterDocumentChanged(masterDocument);
}
LatexDocument* LatexDocuments::getCurrentDocument() const{
	return currentDocument;
}
LatexDocument* LatexDocuments::getMasterDocument() const{
	return masterDocument;
}
QList<LatexDocument*> LatexDocuments::getDocuments() const{
	return documents;
}

void LatexDocuments::move(int from, int to){
    model->layoutAboutToBeChanged();
    model->moveDocs(from,to);
    documents.move(from,to);
    model->layoutChanged();
}

QString LatexDocuments::getCurrentFileName() {
	if (!currentDocument) return "";
	return currentDocument->getFileName();
}
QString LatexDocuments::getCompileFileName(){
	if (masterDocument)
		return masterDocument->getFileName();
	if (!currentDocument)
		return "";
	const LatexDocument* masterDoc=currentDocument->getTopMasterDocument();
	QString curDocFile = currentDocument->getFileName();
	if(masterDoc)
		curDocFile=masterDoc->getFileName();
	return curDocFile;
}
QString LatexDocuments::getTemporaryCompileFileName(){
	QString temp = getCompileFileName();
	if (!temp.isEmpty()) return temp;
	if (masterDocument) return masterDocument->getTemporaryFileName();
	else if (currentDocument) return currentDocument->getTemporaryFileName();
	return "";
}

QString LatexDocuments::getAbsoluteFilePath(const QString & relName, const QString &extension){
	QString s=relName;
	QString ext = extension;
	if (!ext.isEmpty() && !ext.startsWith(".")) ext = "." + ext;
	if (!s.endsWith(ext,Qt::CaseInsensitive)) s+=ext;
	QFileInfo fi(s);
	if (!fi.isRelative()) return s;
	QString compileFileName=getTemporaryCompileFileName();
	if (compileFileName.isEmpty()) return s; //what else can we do?
	QString compilePath=QFileInfo(compileFileName).absolutePath();
	if (!compilePath.endsWith("\\") && !compilePath.endsWith("/"))
		compilePath+=QDir::separator();
	return  compilePath+s;
}

LatexDocument* LatexDocuments::findDocumentFromName(const QString& fileName){
	foreach(LatexDocument *doc,documents){
		if(doc->getFileName()==fileName) return doc;
	}
	return 0;
}

LatexDocument* LatexDocuments::findDocument(const QDocument *qDoc){
	foreach(LatexDocument *doc,documents){
		LatexEditorView *edView=doc->getEditorView();
		if(edView && edView->editor->document()==qDoc) return doc;
	}
	return 0;
}

LatexDocument* LatexDocuments::findDocument(const QString& fileName, bool checkTemporaryNames){
	if (fileName=="") return 0;
	if (checkTemporaryNames) {
		LatexDocument* temp = findDocument(fileName, false);
		if (temp) return temp;
	}
	QString fnorm = fileName;
	fnorm.replace("/",QDir::separator()).replace("\\",QDir::separator());
	//fast check for absolute file names
#ifdef Q_WS_WIN
	Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
	Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
	foreach (LatexDocument* document, documents)
		if (document->getFileName().compare(fnorm,cs)==0)
			return document;
	
	if (checkTemporaryNames)
		foreach (LatexDocument* document, documents)
			if (document->getFileName().isEmpty() &&
			              document->getTemporaryFileName().compare(fnorm,cs)==0)
				return document;
	
	//check for relative file names
	QFileInfo fi(getAbsoluteFilePath(fileName));
	if (!fi.exists()) {
		if (QFileInfo(getAbsoluteFilePath(fileName),".tex").exists())
			fi=QFileInfo(getAbsoluteFilePath(fileName),".tex");
		else if (QFileInfo(getAbsoluteFilePath(fileName),".bib").exists())
			fi=QFileInfo(getAbsoluteFilePath(fileName),".bib");
		else return 0;
	}
	//check for same file infos (is not reliable in qt < 4.5, because they just compare absoluteFilePath)
	foreach (LatexDocument* document, documents)
		if (document->getFileInfo().exists() && document->getFileInfo()==fi)
			return document;
	
	//check for canonical file path (unnecessary in qt 4.5)
	fnorm = fi.canonicalFilePath();
	foreach (LatexDocument* document, documents)
		if (document->getFileInfo().canonicalFilePath().compare(fnorm,cs)==0)
			return document;
	
	return 0;
}
void LatexDocuments::settingsRead(){
	model->iconSection.resize(LatexParser::getInstance().structureCommands.count());
	for (int i=0;i<LatexParser::getInstance().structureCommands.count();i++)
		model->iconSection[i]=QIcon(":/images/"+LatexParser::getInstance().structureCommands[i].mid(1)+".png");
}
bool LatexDocuments::singleMode(){
	return !masterDocument;
}

void LatexDocuments::updateBibFiles(bool updateFiles){
	mentionedBibTeXFiles.clear();
    QSet<QString> newBibItems;
	foreach (LatexDocument* doc, documents) {
        if(updateFiles){
            QMultiHash<QDocumentLineHandle*,FileNamePair>::iterator it = doc->mentionedBibTeXFiles().begin();
            QMultiHash<QDocumentLineHandle*,FileNamePair>::iterator itend = doc->mentionedBibTeXFiles().end();
            for (; it != itend; ++it){
                if (it.value().absolute.isEmpty()) it.value().absolute = getAbsoluteFilePath(it.value().relative,".bib").replace(QDir::separator(), "/"); //store absolute
                mentionedBibTeXFiles << it.value().absolute;
            }
        }
        newBibItems.unite(QSet<QString>::fromList(doc->bibItems()));
	}
	
	bool changed=false;
    if(updateFiles){
        for (int i=0; i<mentionedBibTeXFiles.count();i++){
            QString &fileName=mentionedBibTeXFiles[i];
            QFileInfo fi(fileName);
            if (!fi.isReadable()) continue; //ups...
            if (!bibTeXFiles.contains(fileName))
                bibTeXFiles.insert(fileName,BibTeXFileInfo());
            BibTeXFileInfo& bibTex=bibTeXFiles[mentionedBibTeXFiles[i]];
            if (bibTex.loadIfModified(fileName))
                changed = true;
            if (bibTex.ids.empty() && !bibTex.linksTo.isEmpty())
                //handle obscure bib tex feature, a just line containing "link fileName"
                mentionedBibTeXFiles.append(bibTex.linksTo);
        }
    }
    if (changed || (newBibItems!=bibItems)) {
		allBibTeXIds.clear();
        bibItems=newBibItems;
		for (QMap<QString, BibTeXFileInfo>::const_iterator it=bibTeXFiles.constBegin(); it!=bibTeXFiles.constEnd();++it)
			foreach (const QString& s, it.value().ids)
				allBibTeXIds << s;
        allBibTeXIds.unite(bibItems);
		for (int i=0;i<documents.size();i++)
			if (documents[i]->getEditorView())
				documents[i]->getEditorView()->setBibTeXIds(&allBibTeXIds);
		bibTeXFilesModified=true;
    }
}

QString LatexDocuments::findFileFromBibId(const QString& bibId)
{
    QStringList keys=bibTeXFiles.keys();
    foreach(const QString key,keys){
        if(bibTeXFiles.value(key).ids.contains(bibId)){
            return key;
        }
    }
    return QString();
}

void LatexDocument::findStructureEntryBefore(QMutableListIterator<StructureEntry*> &iter,QMultiHash<QDocumentLineHandle*,StructureEntry*> &MapOfElements,int linenr,int count){
	bool goBack=false;
	int l=0;
	while(iter.hasNext()){
		StructureEntry* se=iter.next();
		int realline = se->getRealLineNumber();
		Q_ASSERT(realline >= 0);
		if(realline>=linenr && (realline<linenr+count) ){
			emit removeElement(se,l);
			iter.remove();
			emit removeElementFinished();
			MapOfElements.insert(se->getLineHandle(),se);
			l--;
		}
		if(realline>=linenr+count) {
			goBack=true;
			break;
		}
		l++;
	}
	if(goBack && iter.hasPrevious()) iter.previous();
}


void LatexDocument::mergeStructure(StructureEntry* se, QVector<StructureEntry*> &parent_level, QList<StructureEntry*>& flatStructure, const int linenr, const int count){
	if (!se) return;
	if (se->type != StructureEntry::SE_DOCUMENT_ROOT && se->type != StructureEntry::SE_SECTION && se->type != StructureEntry::SE_INCLUDE) return;
	if (se == baseStructure) parent_level.fill(se);
	int se_line = se->getRealLineNumber();
	if (se_line < linenr || se == baseStructure) {
		//se is before updated region, but children might still be in it
		updateParentVector(parent_level, se);
		
		//if (!se->children.isEmpty() && se->children.last()->getRealLineNumber() >= linenr) {
		int start = -1;
		for (int i=0;i<se->children.size();i++){
			StructureEntry* c = se->children[i];
			if (c->type != StructureEntry::SE_SECTION && c->type != StructureEntry::SE_INCLUDE) continue;
			if (c->getRealLineNumber() < linenr) 
				updateParentVector(parent_level, c);
			start = i;
			break;
		}
		if (start >=0) {
			if (start > 0) start--;

			QList<StructureEntry*> oldChildren = se->children;
			for (int i=start;i<oldChildren.size();i++)
				mergeStructure(oldChildren[i], parent_level, flatStructure, linenr, count);
		}
	} else {		
		//se is within or after the region 
		// => insert flatStructure.first() before se or replace se with it (don't insert after since there might be another "se" to replace)
		while (!flatStructure.isEmpty() && se->getRealLineNumber() >= flatStructure.first()->getRealLineNumber() ) {
			if (se->getRealLineNumber() == flatStructure.first()->getRealLineNumber()) {
				flatStructure.first()->parent = se->parent;
				flatStructure.first()->children = se->children;
				*se = *flatStructure.first();
				flatStructure.first()->children.clear();
				delete flatStructure.takeFirst();
				emit updateElement(se);
				moveToAppropiatePositionWithSignal(parent_level, se);
			//	qDebug()<<"a"<<se->children.size() << ":"<<se->title<<" von "<<linenr<<count;
				QList<StructureEntry*> oldChildren = se->children;
				foreach (StructureEntry* next, oldChildren) 
					mergeStructure(next, parent_level, flatStructure, linenr, count);
				return;
			}
			moveToAppropiatePositionWithSignal(parent_level, flatStructure.takeFirst());
		}

		if (se_line < linenr + count) {
			//se is within the region (delete if necessary and then merge children)
			if (flatStructure.isEmpty() || se->getRealLineNumber() < flatStructure.first()->getRealLineNumber()) {
				QList<StructureEntry*> oldChildren = se->children;
				int oldrow = se->getRealParentRow();
				for (int i=se->children.size()-1;i>=0;i--)
					moveElementWithSignal(se->children[i], se->parent, oldrow);
				removeElementWithSignal(se);
				delete se;
				for (int i=1;i<parent_level.size();i++)
					if (parent_level[i] == se) 
						parent_level[i] = parent_level[i-1];
				foreach (StructureEntry* next, oldChildren)
					mergeStructure(next, parent_level, flatStructure, linenr, count);
				return;
			}
		}
		
		//se not replaced or deleted => se is after everything the region => keep children
		moveToAppropiatePositionWithSignal(parent_level, se);
		QList<StructureEntry*> oldChildren = se->children;
		foreach (StructureEntry* c, oldChildren)
			moveToAppropiatePositionWithSignal(parent_level, c); 
		
	}
	
	//insert unprocessed elements of flatStructure at the end of the structure
	if (se == baseStructure && !flatStructure.isEmpty()) {
		foreach (StructureEntry* s, flatStructure){
			addElementWithSignal(parent_level[s->level], s);
			updateParentVector(parent_level, s);
		}
		flatStructure.clear();
	}
	
	
	return;
}

void LatexDocument::removeElementWithSignal(StructureEntry* se){
	int parentRow = se->getRealParentRow();
	REQUIRE(parentRow >= 0);
	emit removeElement(se, parentRow);
	se->parent->children.removeAt(parentRow);
	se->parent = 0;
	emit removeElementFinished();
}

void LatexDocument::addElementWithSignal(StructureEntry* parent, StructureEntry* se){
	emit addElement(parent, parent->children.size());
	parent->children.append(se);
	se->parent = parent;
	emit addElementFinished();
}

void LatexDocument::insertElementWithSignal(StructureEntry* parent, int pos, StructureEntry* se){
	emit addElement(parent, pos);
	parent->children.insert(pos, se);
	se->parent = parent;
	emit addElementFinished();
}

void LatexDocument::moveElementWithSignal(StructureEntry* se, StructureEntry* parent, int pos){
	removeElementWithSignal(se);
	insertElementWithSignal(parent, pos, se);
}


void LatexDocument::updateParentVector(QVector<StructureEntry*> &parent_level, StructureEntry* se){
	REQUIRE(se);
	if (se->type == StructureEntry::SE_DOCUMENT_ROOT || se->type == StructureEntry::SE_INCLUDE) 
		parent_level.fill(baseStructure);
	else if (se->type == StructureEntry::SE_SECTION)
		for (int j=se->level+1;j<parent_level.size();j++)
			parent_level[j] = se;
}

class LessThanRealLineNumber
{
public:
	inline bool operator()(const StructureEntry * const se1, const StructureEntry * const se2) const
	{
		int l1 = se1->getRealLineNumber();
		int l2 = se2->getRealLineNumber();
		if (l1 < l2) return true;
		if (l1 == l2 && (se1->columnNumber < se2->columnNumber)) return true;
		return false;
	}
};


StructureEntry* LatexDocument::moveToAppropiatePositionWithSignal(QVector<StructureEntry*> &parent_level, StructureEntry* se){
	REQUIRE_RET(se, 0);
	StructureEntry* newParent = parent_level[se->level];
	if (se->parent == newParent) {
		updateParentVector(parent_level, se);
		return 0;
	}

	int newPos = newParent->children.size();
	if (newParent->children.size() > 0 && 
	    newParent->children.last()->getRealLineNumber() >= se->getRealLineNumber()) 
		newPos = qUpperBound(newParent->children.begin(), newParent->children.end(), se, LessThanRealLineNumber()) - newParent->children.begin();		
	
	
	//qDebug() << "auto insert at " << newPos;
	if (se->parent) moveElementWithSignal(se, newParent, newPos);
	else insertElementWithSignal(newParent, newPos, se);
	
	updateParentVector(parent_level, se);
	return newParent;
}

/*!
  Splits a [name] = [val] string into \a name and \a val removing extra spaces.
  
  \return true if splitting successful, false otherwise (in that case name and val are empty)
 */
bool LatexDocument::splitMagicComment(const QString &comment, QString &name, QString &val) {
	int sep = comment.indexOf("=");
	if (sep < 0) return false;
	name = comment.left(sep).trimmed();
	val = comment.mid(sep+1).trimmed();
	return true;
}

/*!
  Formats the StructureEntry and modifies the document according to the MagicComment contents
  */
void LatexDocument::parseMagicComment(const QString &name, const QString &val, StructureEntry* se) {
	if (name.isEmpty()) {
		se->tooltip = QString();
		se->valid = false;
	}
	
	if (name == "spellcheck") {
		QString lang=val;
		lang.replace("-", "_"); // QLocale expects "_". This is to stay compatible with texworks which uses "-"
		mSpellingLanguage = QLocale(lang);
		if (mSpellingLanguage.language() == QLocale::C) {
			se->tooltip = tr("Invalid language format");
			return;
		}
		emit spellingLanguageChanged(mSpellingLanguage);
		
		se->valid = true;
		/* TODO: set master document
 } else if (type == "texroot") {
  se->valid = true;
 */
	} else if (name == "encoding") {
		QTextCodec *codec = QTextCodec::codecForName(val.toAscii());
		if (!codec) {
			se->tooltip = tr("Invalid codec");
			return;
		}
		setCodec(codec);
		se->valid = true;
	} else {
		se->tooltip = tr("Unknown magic comment");
		return;
	}
	se->valid = true;
}


void LatexDocument::updateAppendix(QDocumentLineHandle *oldLine,QDocumentLineHandle *newLine){
	int endLine=newLine?indexOf(newLine):-1 ;
	int startLine=-1;
	if(oldLine){
		startLine=indexOf(oldLine);
		if(endLine<0 || endLine>startLine){
			// remove appendic marker
			StructureEntry *se=baseStructure;
			setAppendix(se,startLine,endLine,false);
		}
	}
	
	if(endLine>-1 && (endLine<startLine || startLine<0)){
		StructureEntry *se=baseStructure;
		setAppendix(se,endLine,startLine,true);
	}
}

void LatexDocument::setAppendix(StructureEntry *se,int startLine,int endLine,bool state){
	bool first=false;
	for(int i=0;i<se->children.size();i++){
		StructureEntry *elem=se->children[i];
		if(endLine>=0 && elem->getLineHandle() && elem->getRealLineNumber()>endLine) break;
		if(elem->type==StructureEntry::SE_SECTION && elem->getRealLineNumber()>startLine){
			if(!first && i>0) setAppendix(se->children[i-1],startLine,endLine,state);
			elem->appendix=state;
			emit updateElement(elem);
			setAppendix(se->children[i],startLine,endLine,state);
			first=true;
		}
	}
	if(!first && !se->children.isEmpty()) {
		StructureEntry *elem=se->children.last();
		if(elem->type==StructureEntry::SE_SECTION) setAppendix(elem,startLine,endLine,state);
	}
}

bool LatexDocument::fileExits(QString fname){
	QString curPath=ensureTrailingDirSeparator(getFileInfo().absolutePath());
	bool exist=QFile(getAbsoluteFilePath(fname,".tex")).exists();
	if (!exist) exist=QFile(getAbsoluteFilePath(curPath+fname,".tex")).exists();
	if (!exist) exist=QFile(getAbsoluteFilePath(curPath+fname,"")).exists();
	return exist;
}

QString LatexDocument::findFileName(QString fname){
	QString curPath=ensureTrailingDirSeparator(getFileInfo().absolutePath());
	QString result;
	if(QFile(getAbsoluteFilePath(fname,".tex")).exists())
		result=QFileInfo(getAbsoluteFilePath(fname,".tex")).absoluteFilePath();
	if (result.isEmpty() && QFile(getAbsoluteFilePath(curPath+fname,".tex")).exists())
		result=QFileInfo(getAbsoluteFilePath(curPath+fname,".tex")).absoluteFilePath();
	if (result.isEmpty() && QFile(getAbsoluteFilePath(curPath+fname,"")).exists())
		result=QFileInfo(getAbsoluteFilePath(curPath+fname,"")).absoluteFilePath();
	return result;
}

void LatexDocuments::updateStructure(){
	foreach(const LatexDocument* doc,documents){
		model->updateElement(doc->baseStructure);
	}
	if(model->getSingleDocMode()){
		model->structureUpdated(currentDocument,0);
	}
}

void LatexDocuments::bibTeXFilesNeedUpdate(){
	bibTeXFilesModified=true;
}

void LatexDocuments::updateMasterSlaveRelations(LatexDocument *doc){
	//update Master/Child relations
	//remove old settings ...
	doc->setMasterDocument(0);
	foreach(LatexDocument* elem,this->documents){
		if(elem->getMasterDocument()==doc){
			elem->setMasterDocument(0);
			elem->recheckRefsLabels();
		}
	}
	
	//check whether document is child of other docs
	QString fname=doc->getFileName();
	foreach(LatexDocument* elem,this->documents){
		if(elem==doc)
			continue;
		QStringList includedFiles=elem->includedFiles();
		if(includedFiles.contains(fname)){
			doc->setMasterDocument(elem);
			break;
		}
	}
	
	// check for already open child documents (included in this file)
	QStringList includedFiles=doc->includedFiles();
	foreach(const QString& fname,includedFiles){
		LatexDocument* child=this->findDocumentFromName(fname);
		if(child){
			child->setMasterDocument(doc);
			LatexEditorView *edView=child->getEditorView();
			if(edView)
				edView->reCheckSyntax(); // redo syntax checking (in case of defined commands)
		}
	}
	
	//recheck references
	doc->recheckRefsLabels();
}

const LatexDocument* LatexDocument::getTopMasterDocument(QSet<const LatexDocument*> *visitedDocs) const {
	const LatexDocument *result=this;
	bool deleteVisitedDocs=false;
	if(!visitedDocs){
		visitedDocs=new QSet<const LatexDocument*>();
		deleteVisitedDocs=true;
	}
	visitedDocs->insert(this);
	if(masterDocument && !visitedDocs->contains(masterDocument))
		result=masterDocument->getTopMasterDocument(visitedDocs);
	if (result->getFileName().endsWith("bib"))
		foreach (const LatexDocument* d, parent->documents) {
			QMultiHash<QDocumentLineHandle*,FileNamePair>::const_iterator it = d->mentionedBibTeXFiles().constBegin();
			QMultiHash<QDocumentLineHandle*,FileNamePair>::const_iterator itend = d->mentionedBibTeXFiles().constEnd();
			for (; it != itend; ++it) {
				//qDebug() << it.value().absolute << " <> "<<result->getFileName();
				if (it.value().absolute == result->getFileName()){
					result = d->getTopMasterDocument(visitedDocs);
					break;
				}
			}
			if (result == d) break;
		}	
	if(deleteVisitedDocs)
		delete visitedDocs;
	return result;
}

LatexDocument* LatexDocument::getTopMasterDocument(){
	return const_cast<LatexDocument*>(getTopMasterDocument(0));
}

QStringList LatexDocument::includedFiles(){
	QStringList result;
	foreach(const StructureEntry* se,baseStructure->children){
		if(se->type==StructureEntry::SE_INCLUDE){
			QString fname=findFileName(se->title);
			if(!fname.isEmpty())
				result << fname;
		}
	}
	return result;
}

void LatexDocument::updateCompletionFiles(QStringList &added,QStringList &removed,bool forceUpdate){
	// remove
	QStringList filtered;
	LatexEditorView *edView=getEditorView();
	LatexCompleterConfig *config=edView->getCompleter()->getConfig();
	bool update=forceUpdate;
	foreach(QString elem,removed){
		if(!mUsepackageList.keys(elem).isEmpty())
			continue;
		elem.append(".cwl");
		if(!filtered.contains(elem)){
            if(!elem.isEmpty())
				filtered << elem;
		}
	}
	if(!filtered.isEmpty()){
		LatexParser cmds;
        //QStringList removedWords=loadCwlFiles(filtered,&cmds,config);
        foreach(QString elem,filtered){
            LatexPackage pck=parent->cachedPackages.value(elem);
            cmds.possibleCommands=pck.possibleCommands;
            ltxCommands.substract(cmds);
            foreach(const QString& elem,pck.completionWords)
                mCompleterWords.remove(elem);
        }
		//recheck syntax of ALL documents ...
		update=true;
	}
	// add
	filtered.clear();
	foreach(QString elem,added){
		elem.append(".cwl");
		if(!filtered.contains(elem)){
            if(parent->cachedPackages.contains(elem)){
                filtered << elem;
                continue;
            }
			QString fn=findResourceFile("completion/"+elem,false,QStringList(config->importedCwlBaseDir));
			if(!fn.isEmpty())
				filtered << elem;
			else {
				emit importPackage(elem);
			}
		}
	}
	if(!filtered.isEmpty()){
		LatexParser cmds;
        foreach(QString elem,filtered){
            LatexPackage pck;
            if(parent->cachedPackages.contains(elem)){
                pck=parent->cachedPackages.value(elem);
            }else{
                pck=loadCwlFile(elem,config);
                if(pck.packageName!="<notFound>"){
                    parent->cachedPackages.insert(elem,pck); // cache package
                }else{
                    LatexPackage zw;
                    zw.packageName=elem;
                    parent->cachedPackages.insert(elem,zw); // cache package as empty/not found package
                }
            }
            cmds.possibleCommands=pck.possibleCommands;
            ltxCommands.append(cmds);
            mCompleterWords.unite(pck.completionWords.toSet());
        }
		//recheck syntax of ALL documents ...
		update=true;
	}
	if(update){
		foreach(LatexDocument* elem,getListOfDocs()){
			LatexEditorView *edView=elem->getEditorView();
			if(edView){
				edView->updateLtxCommands();
				edView->reCheckSyntax();
			}
		}
	}
}

void LatexDocument::updateCompletionFiles(QStringList &files,bool forceUpdate,bool forceLabelUpdate){
	// remove
	LatexEditorView *edView=getEditorView();
	LatexCompleterConfig *completerConfig=edView->getCompleter()->getConfig();
	bool update=forceUpdate;
	
	//recheck syntax of ALL documents ...
	LatexPackage pck;
	QStringList loadedFiles;
	for(int i=0;i<files.count();i++){
		if(!files.at(i).endsWith(".cwl"))
			files[i]=files[i]+".cwl";
	}
	files.append(completerConfig->getLoadedFiles());
	gatherCompletionFiles(files,loadedFiles,pck);
	update=true;
	
	completerConfig->words=pck.completionWords;
	ltxCommands.optionCommands=pck.optionCommands;
	ltxCommands.possibleCommands=pck.possibleCommands;
	ltxCommands.environmentAliases=pck.environmentAliases;
	
	// user commands
	QStringList commands=mUserCommandList.values();
	foreach(QString elem,commands){
        if(!elem.startsWith("\\begin{")&&!elem.startsWith("\\end{")){
            int i=elem.indexOf("{");
            if(i>=0) elem=elem.left(i);
        }
		ltxCommands.possibleCommands["user"].insert(elem);
	}
    //patch lines for new commands (ref,def, etc)
    LatexParser& latexParser = LatexParser::getInstance();
    QStringList categories;
    categories<< "%ref" << "%label" << "%definition" << "%cite" << "%usepackage" << "%graphics" << "%file" << "%bibliography";
    QStringList newCmds;
    foreach(const QString elem,categories){
        QStringList cmds=ltxCommands.possibleCommands[elem].values();
        foreach(const QString cmd,cmds){
            if(!latexParser.possibleCommands[elem].contains(cmd) || forceLabelUpdate){
                newCmds << cmd;
                latexParser.possibleCommands[elem]<< cmd;
            }
        }
    }
    if(!newCmds.isEmpty()){
        patchLinesContaining(newCmds);
    }
	
	if(update){
		foreach(LatexDocument* elem,getListOfDocs()){
			LatexEditorView *edView=elem->getEditorView();
			if(edView){
				edView->updateLtxCommands();
				edView->reCheckSyntax();
			}
		}
	}
}

void LatexDocument::gatherCompletionFiles(QStringList &files,QStringList &loadedFiles,LatexPackage &pck){
	LatexPackage zw;
	LatexCompleterConfig *completerConfig=edView->getCompleter()->getConfig();
	foreach(const QString& elem,files){
		if(loadedFiles.contains(elem))
			continue;
        if(parent->cachedPackages.contains(elem)){
            zw=parent->cachedPackages.value(elem);
        }else{
            zw=loadCwlFile(elem,completerConfig);
            if(zw.packageName!="<notFound>"){
                parent->cachedPackages.insert(elem,zw); // cache package
            }else{
                LatexPackage zw;
                zw.packageName=elem;
                parent->cachedPackages.insert(elem,zw); // cache package as empty/not found package
            }
        }
		if(zw.packageName=="<notFound>"){
			emit importPackage(elem);
		} else {
			pck.unite(zw);
			loadedFiles.append(elem);
			if(!zw.requiredPackages.isEmpty())
				gatherCompletionFiles(zw.requiredPackages,loadedFiles,pck);
		}
	}
}

QString LatexDocument::getMagicComment(const QString& name) {
	QString seName;
	QString val;
	StructureEntryIterator iter(magicCommentList);
	while (iter.hasNext()) {
		StructureEntry *se = iter.next();
		splitMagicComment(se->title, seName, val);
		if (seName==name)
			return val;
	}
	return QString();
}

QDocumentLineHandle* LatexDocument::getMagicCommentLineHandle(const QString& name) {
	QString seName;
	QString val;
	
	if(!magicCommentList) return NULL;
	
	StructureEntryIterator iter(magicCommentList);
	while (iter.hasNext()) {
		StructureEntry *se = iter.next();
		splitMagicComment(se->title, seName, val);
		if (seName==name)
			return se->getLineHandle();
	}
	return NULL;
}

/*!
  replaces the value of the magic comment
 */
void LatexDocument::updateMagicComment(const QString &name, const QString &val, bool createIfNonExisting) {
	QString line(QString("\% !TeX %1 = %2").arg(name).arg(val));
	
	QDocumentLineHandle* dlh = getMagicCommentLineHandle(name);
	if(dlh) {
		QDocumentCursor cur(this, dlh->line());
		cur.select(QDocumentCursor::LineUnderCursor);
		cur.replaceSelectedText(line);
	} else {
		if (createIfNonExisting) {
			QDocumentCursor cur(this);
			cur.beginEditBlock();
			cur.insertText(line);
			cur.insertLine();
			cur.endEditBlock();
		}
	}
}

void LatexDocument::updateMagicCommentScripts(){
	if (!magicCommentList) return;
	
	localMacros.clear();
	
	QRegExp trigger(" *// *(Trigger) *[:=](.*)");
	
	StructureEntryIterator iter(magicCommentList);
	while (iter.hasNext()) {
		StructureEntry *se = iter.next();
		QString seName, val;
		splitMagicComment(se->title, seName, val);
		if (seName=="TXS-SCRIPT") {
			Macro newMacro;
			newMacro.name = val;
			newMacro.trigger = "";
			newMacro.abbrev = "";
			newMacro.tag = "%SCRIPT\n";
			
			int l = se->getRealLineNumber() + 1;
			for (; l < lineCount(); l++) {
				QString lt = line(l).text().trimmed();
				if (lt.endsWith("TXS-SCRIPT-END") || !(lt.isEmpty() || lt.startsWith("%"))  ) break;
				lt.remove(0,1);
				newMacro.tag += lt + "\n";
				if (trigger.exactMatch(lt)) 
					newMacro.trigger = trigger.cap(2).trimmed();
			}
			
			newMacro.init(newMacro.name,newMacro.tag,newMacro.abbrev,newMacro.trigger);
			newMacro.document = this;
			localMacros.append(newMacro);
		}
	}

}


bool LatexDocument::containsPackage(const QString& name){
	return mUsepackageList.keys(name).count()>0;
}

LatexDocument *LatexDocuments::getMasterDocumentForDoc(LatexDocument *doc) const { // doc==0 means current document
	if(masterDocument)
		return masterDocument;
	LatexDocument *current=currentDocument;
	if(doc)
		current=doc;
	if(!current)
		return current;
	return current->getTopMasterDocument();
}

QString LatexDocument::getAbsoluteFilePath(const QString & relName, const QString &extension) const {
	QString s=relName;
	QString ext = extension;
	if (!ext.isEmpty() && !ext.startsWith(".")) ext = "." + ext;
	if (!s.endsWith(ext,Qt::CaseInsensitive)) s+=ext;
	QFileInfo fi(s);
	if (!fi.isRelative()) return s;
	const LatexDocument *masterDoc=getTopMasterDocument();
	QString compileFileName=masterDoc->getFileName();
	if (compileFileName.isEmpty()) return s; //what else can we do?
	QString compilePath=QFileInfo(compileFileName).absolutePath();
	if (!compilePath.endsWith("\\") && !compilePath.endsWith("/"))
		compilePath+=QDir::separator();
	return  compilePath+s;
}

void LatexDocuments::lineGrammarChecked(const void* doc,const void* line,int lineNr, const QList<GrammarError>& errors){
	int d = documents.indexOf(static_cast<LatexDocument*>(const_cast<void*>(doc)));
	if (d == -1) return;
	if (!documents[d]->getEditorView()) return;
	documents[d]->getEditorView()->lineGrammarChecked(doc,line,lineNr,errors);
}

void LatexDocument::patchLinesContaining(const QStringList cmds){
    foreach(LatexDocument *elem,getListOfDocs()){
        // search all cmds in all lines, patch line if cmd is found
        for (int i=0;i<elem->lines();i++){
            QString text=elem->line(i).text();
            foreach(const QString cmd,cmds){
                if(text.contains(cmd)){
                    //elem->patchStructure(i,1);
                    patchStructure(i,1);
                    break;
                }
            }
        }
    }
}
