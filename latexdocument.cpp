#include "latexdocument.h"
#include "qdocument.h"
#include "qformatscheme.h"
#include "qdocumentline.h"
#include "qdocumentline_p.h"
#include "qdocumentcursor.h"
#include "qeditor.h"
#include "smallUsefulFunctions.h"

FileNamePair::FileNamePair(const QString& rel):relative(rel){};

LatexDocument::LatexDocument(QObject *parent):QDocument(parent),edView(0),mAppendixLine(0)
{
	baseStructure = new StructureEntry(this,StructureEntry::SE_DOCUMENT_ROOT);
	labelList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	todoList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	bibTeXList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	blockList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);

	labelList->title=tr("LABELS");
	todoList->title=tr("TODO");
	bibTeXList->title=tr("BIBTEX");
	blockList->title=tr("BLOCKS");
	mLabelItem.clear();
	mUserCommandList.clear();
	mMentionedBibTeXFiles.clear();
	m_magicPlaceHolder=-1;
	m_mirrorInLine=-1;
	masterDocument=0;
	this->parent=0;
}
LatexDocument::~LatexDocument(){
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
		while (iter.hasNext()) iter.next()->lineHandle=0;
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
			index=parent->children.indexOf(section);
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
        mRefItem.clear();
	mMentionedBibTeXFiles.clear();

	mAppendixLine=0;

	if(baseStructure){
		emit structureUpdated(this,0);

		if (!labelList->parent) delete labelList;
		if (!todoList->parent) delete todoList;
		if (!bibTeXList->parent) delete bibTeXList;
		if (!blockList->parent) delete blockList;
		int row=parent->documents.indexOf(this);

		removeElement(0,row);
		delete baseStructure;
		removeElementFinished();
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
	labelList = new StructureEntry(this,StructureEntry::SE_OVERVIEW);
	labelList->title=tr("LABELS");
	todoList = new StructureEntry(this,StructureEntry::SE_OVERVIEW);
	todoList->title=tr("TODO");
	bibTeXList = new StructureEntry(this,StructureEntry::SE_OVERVIEW);
	bibTeXList->title=tr("BIBTEX");
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
	    foreach(const ReferencePair rp,labels){
		updateRefsLabels(rp.name);
	    }
	}
	mRefItem.remove(dlh);
	if(mMentionedBibTeXFiles.remove(dlh))
	    bibTeXFilesNeedsUpdate=true;

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

	QList<StructureEntry*> categories=QList<StructureEntry*>() << labelList << todoList << blockList << bibTeXList;
	foreach (StructureEntry* sec, categories) {
		int l=0;
		QMutableListIterator<StructureEntry*> iter(sec->children);
		while (iter.hasNext()){
			StructureEntry* se=iter.next();
			if(dlh==se->lineHandle) {
				emit removeElement(se,l);
				iter.remove();
				emit removeElementFinished();
				delete se;
			} else l++;
		}
	}

	QVector<StructureEntry*> parent_level(LatexParser::structureCommands.count());
	QVector<QList<StructureEntry*> > remainingChildren(LatexParser::structureCommands.count());
	QMap<StructureEntry*,int> toBeDeleted;
	QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfElements;
	StructureEntry* se=baseStructure;
	splitStructure(se,parent_level,remainingChildren,toBeDeleted,MapOfElements,linenr,1);

	// append structure remainder ...
	for(int i=parent_level.size()-1;i>=0;i--){
		while(!remainingChildren[i].isEmpty() && remainingChildren[i].first()->level>i){
			se=remainingChildren[i].takeFirst();
			parent_level[se->level]->add(se);
		}
		parent_level[i]->children << remainingChildren[i];
		foreach(StructureEntry *elem,remainingChildren[i]){
			elem->parent=parent_level[i];
		}
	}
	// purge unconnected elements
	foreach(se,toBeDeleted.keys()){
		emit removeElement(se,toBeDeleted.value(se));
		delete se;
		emit removeElementFinished();
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

	if (bibTeXFilesNeedsUpdate)
		emit updateBibTeXFiles();

	if (completerNeedsUpdate || bibTeXFilesNeedsUpdate)
		emit updateCompleter();

	if(!removedUsepackages.isEmpty() || updateSyntaxCheck){
		QStringList addedUsepackages;
		updateCompletionFiles(addedUsepackages,removedUsepackages,updateSyntaxCheck);
	}
}

void LatexDocument::patchStructure(int linenr, int count) {

	if (!baseStructure) return;

	emit toBeChanged();

	bool completerNeedsUpdate=false;
	bool bibTeXFilesNeedsUpdate=false;

	QDocumentLineHandle *oldLine=mAppendixLine; // to detect a change in appendix position

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

	QVector<StructureEntry*> parent_level(LatexParser::structureCommands.count());
	QVector<QList<StructureEntry*> > remainingChildren(LatexParser::structureCommands.count());
	QMap<StructureEntry*,int> toBeDeleted;
	QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfElements;
	StructureEntry* se=baseStructure;
	splitStructure(se,parent_level,remainingChildren,toBeDeleted,MapOfElements,linenr,count);
	bool sectionAdded=false;
	int verbatimFormat=QDocument::formatFactory()->id("verbatim");
	bool updateSyntaxCheck=false;

	// usepackage list
	QStringList removedUsepackages;
	QStringList addedUsepackages;
	QStringList removedUserCommands,addedUserCommands;

	//TODO: This assumes one command per line, which is not necessary true
	for (int i=linenr; i<linenr+count; i++) {
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
		    foreach(const ReferencePair rp,labels){
			updateRefsLabels(rp.name);
		    }
		}
		mRefItem.remove(dlh);
		if (mUserCommandList.remove(dlh)>0) completerNeedsUpdate = true;

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
			newTodo->lineNumber=i;
			newTodo->lineHandle=line(i).handle();
			newTodo->parent=todoList;
			if(!reuse) emit addElement(todoList,todoList->children.size()); //todo: why here but not in label?
			iter_todo.insert(newTodo);
		}
		////Ref
		//for reference counting (can be placed in command options as well ...
		foreach(QString cmd,LatexParser::refCommands){
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
		foreach(QString cmd,LatexParser::labelCommands){
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
			    newLabel->lineNumber=i;
			    newLabel->lineHandle=line(i).handle();
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
		int offset=0;
		int totalLength=curLine.length();
		while(findCommandWithArg(curLine,cmd,name,arg,remainder,optionStart)){
			//update offset
			offset=totalLength-curLine.length();
			//copy remainder to curLine for next round
			curLine=remainder;
			//// newcommand ////
			//TODO: handle optional arguments
			if (LatexParser::definitionCommands.contains(cmd)) {
				completerNeedsUpdate=true;
				QRegExp rx("^\\s*\\[(\\d+)\\]");
				int options=0;
				if(rx.indexIn(remainder)>-1)
				    options=rx.cap(1).toInt(); //returns 0 if conversion fails
				ltxCommands.possibleCommands["user"].insert(name);
				addedUserCommands << name;
				for (int j=0; j<options; j++) {
					if (j==0) name.append("{%<arg1%|%>}");
					else name.append(QString("{%<arg%1%>}").arg(j+1));
				}
				mUserCommandList.insert(line(i).handle(),name);
				// remove obsolete Overlays (maybe this can be refined
				updateSyntaxCheck=true;
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
				foreach(const QString elem,lst){
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
			if (cmd=="\\newtheorem") {
				completerNeedsUpdate=true;
				QStringList lst;
				lst << "\\begin{"+name+"}" << "\\end{"+name+"}";
				foreach(const QString elem,lst){
				    mUserCommandList.insert(line(i).handle(),elem);
				    ltxCommands.possibleCommands["user"].insert(elem);
				    if(!removedUserCommands.removeAll(elem)){
					addedUserCommands << elem;
				    }
				}
				continue;
			}
			///usepackage
			if (LatexParser::usepackageCommands.contains(cmd)) {
				completerNeedsUpdate=true;
				QStringList packagesHelper=name.split(",");
				QStringList packages;
				foreach(const QString elem,packagesHelper){
				    if(LatexParser::packageAliases.contains(elem)){
					packages << LatexParser::packageAliases.values(elem);
				    }else{
					packages << elem;
				    }
				}

				foreach(const QString elem,packages){
				    if(!removedUsepackages.removeAll(elem)){
					addedUsepackages << elem;
				    }
				    mUsepackageList.insertMulti(dlh,elem);
				}
				continue;
			}
			//// bibliography ////
			if (cmd=="\\bibliography") {
				QStringList bibs=name.split(',',QString::SkipEmptyParts);
				//add new bibs and set bibTeXFilesNeedsUpdate if there was any change
				foreach(const QString& elem,bibs){ //latex doesn't seem to allow any spaces in file names
					mMentionedBibTeXFiles.insert(line(i).handle(),FileNamePair(elem));					
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
					newFile->lineNumber=i;
					newFile->lineHandle=line(i).handle();
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
				newBlock->lineNumber=i;
				newBlock->lineHandle=line(i).handle();
				newBlock->parent=blockList;
				iter_block.insert(newBlock);
				continue;
			}

			//// include,input ////
			//static const QStringList inputTokens = QStringList() << "\\input" << "\\include";

			if (LatexParser::includeCommands.contains(cmd)) {
				StructureEntry *newInclude=new StructureEntry(this, StructureEntry::SE_INCLUDE);
				baseStructure->add(newInclude);
				newInclude->title=name;
				newInclude->lineNumber=i;
				QString fname=findFileName(name);
				LatexDocument* dc=parent->findDocumentFromName(fname);
				if(dc)
				    dc->setMasterDocument(this);
				newInclude->level=!fname.isEmpty()? 0 : 1;
				newInclude->lineHandle=line(i).handle();
				//new parent for following sections is base !
				for(int j=0;j<parent_level.size();j++)
					parent_level[j]=baseStructure;
				continue;
			}
			//// all sections ////
			if(cmd.endsWith("*"))
			    cmd=cmd.left(cmd.length()-1);
			int header=LatexParser::structureCommands.indexOf(cmd);
			if (header>-1) {
				StructureEntry *newSection;
				StructureEntry* parent=header == 0 ? baseStructure : parent_level[header];
				if(MapOfElements.contains(dlh)){
					newSection=MapOfElements.value(dlh);
					newSection->type=StructureEntry::SE_SECTION;
					toBeDeleted.remove(newSection);
					MapOfElements.remove(dlh,newSection);
					for (int i=newSection->children.size()-1;i>=0;i--){
						removeAndDeleteElement(newSection->children[i],i);
						newSection->children.removeAt(i);
					}
				}else{
					emit addElement(parent,parent->children.size());
					newSection=new StructureEntry(this,StructureEntry::SE_SECTION);
					sectionAdded=true;
				}
				parent->add(newSection);

				if(mAppendixLine &&indexOf(mAppendixLine)<i) newSection->appendix=true;
				else newSection->appendix=false;
				newSection->title=name;
				newSection->level=header;
				newSection->lineNumber=i;
				newSection->lineHandle=line(i).handle();
				if(header+1<parent_level.size()) parent_level[header+1]=newSection;
				for(int j=header+2;j<parent_level.size();j++)
					parent_level[j]=newSection;
			}
		}// for each command

		if (!oldBibs.isEmpty())
			bibTeXFilesNeedsUpdate = true; //file name removed

	}//for each line handle
	// append structure remainder ...
	for(int i=parent_level.size()-1;i>=0;i--){
		if (!parent_level[i]) break;
		while(!remainingChildren[i].isEmpty() && remainingChildren[i].first()->level>i){
		    se=remainingChildren[i].takeFirst();
		    parent_level[se->level]->add(se);
		}
		int off=0;
		int end=remainingChildren[i].size();
		for(int j=0;j<end;j++){
			se=remainingChildren[i].value(j-off);
			if(se->level<i && parent_level[se->level]!=parent_level[i]){
			    parent_level[se->level]->add(se);
			    se->parent=parent_level[se->level];
			    remainingChildren[i].removeAt(j-off);
			    off++;
			}
		}
		parent_level[i]->children << remainingChildren[i];
		foreach(StructureEntry *elem,remainingChildren[i]){
			elem->parent=parent_level[i];
		}
	}
	if(sectionAdded) {
		emit addElementFinished();
	}
	// purge unconnected elements
	foreach(se,toBeDeleted.keys())
		removeAndDeleteElement(se, toBeDeleted[se]);

	bibTeXList->parent = labelList->parent = todoList->parent = blockList->parent = 0;

	int tempPos = -1;
	tempPos = baseStructure->children.indexOf(bibTeXList);
	if (tempPos != -1) baseStructure->children.removeAt(tempPos);
	tempPos = baseStructure->children.indexOf(labelList);
	if (tempPos != -1) baseStructure->children.removeAt(tempPos);
	tempPos = baseStructure->children.indexOf(todoList);
	if (tempPos != -1) baseStructure->children.removeAt(tempPos);
	tempPos = baseStructure->children.indexOf(blockList);
	if (tempPos != -1) baseStructure->children.removeAt(tempPos);

	if (!bibTeXList->children.isEmpty()) baseStructure->insert(0, bibTeXList);
	if (!todoList->children.isEmpty()) baseStructure->insert(0, todoList);
	if (!labelList->children.isEmpty()) baseStructure->insert(0, labelList);
	if (!blockList->children.isEmpty()) baseStructure->insert(0, blockList);

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

	foreach(se,MapOfTodo.values())
		delete se;

	foreach(se,MapOfBibtex.values())
		delete se;

	foreach(se,MapOfBlock.values())
		delete se;

	foreach(se,MapOfLabels.values())
		delete se;

	if(!addedUsepackages.isEmpty() || !removedUsepackages.isEmpty() || !addedUserCommands.isEmpty() || !removedUserCommands.isEmpty()){
		bool forceUpdate=!addedUserCommands.isEmpty() || !removedUserCommands.isEmpty();
		updateCompletionFiles(addedUsepackages,removedUsepackages,forceUpdate);
	}

	if (bibTeXFilesNeedsUpdate)
		emit updateBibTeXFiles();

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

QString LatexDocument::getTemporaryFileName(){
	return temporaryFileName;
}

int LatexDocument::countLabels(QString name){
    int result=0;
    foreach(const LatexDocument *elem,getListOfDocs()){
	QStringList items=elem->labelItem();
	result+=items.count(name);
    }
    return result;
}

int LatexDocument::countRefs(QString name){
    int result=0;
    foreach(const LatexDocument *elem,getListOfDocs()){
	QStringList items=elem->refItem();
	result+=items.count(name);
    }
    return result;
}

QMultiHash<QDocumentLineHandle*,int> LatexDocument::getLabels(QString name){
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

QMultiHash<QDocumentLineHandle*,int> LatexDocument::getRefs(QString name){
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

QList<LatexDocument *>LatexDocument::getListOfDocs(){
    QList<LatexDocument *>listOfDocs;
    if(parent->masterDocument){
	listOfDocs=parent->documents;
    }else{
	LatexDocument *master=this;
	if(masterDocument){
	    master=masterDocument;
	}
	foreach(LatexDocument *elem,parent->documents){
	    if(elem!=master && elem->masterDocument!=master) continue;
	    listOfDocs << elem;
	}
    }
    return listOfDocs;
}

void LatexDocument::recheckRefsLabels(){
    // get occurences (refs)
    int referenceMultipleFormat=QDocument::formatFactory()->id("referenceMultiple");
    int referencePresentFormat=QDocument::formatFactory()->id("referencePresent");
    int referenceMissingFormat=QDocument::formatFactory()->id("referenceMissing");

    QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
    for(it=mLabelItem.constBegin();it!=mLabelItem.constEnd();it++){
	QDocumentLineHandle* dlh=it.key();
	foreach(const ReferencePair rp,mLabelItem.values(dlh)){
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
    for(it=mRefItem.constBegin();it!=mRefItem.constEnd();it++){
	QDocumentLineHandle* dlh=it.key();
	foreach(const ReferencePair rp,mRefItem.values(dlh)){
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

void LatexDocument::updateRefsLabels(const QString ref){
    // get occurences (refs)
    int referenceMultipleFormat=QDocument::formatFactory()->id("referenceMultiple");
    int referencePresentFormat=QDocument::formatFactory()->id("referencePresent");
    int referenceMissingFormat=QDocument::formatFactory()->id("referenceMissing");

    int cnt=countLabels(ref);
    QMultiHash<QDocumentLineHandle*,int> occurences=getLabels(ref);
    occurences+=getRefs(ref);
    QMultiHash<QDocumentLineHandle*,int>::const_iterator it;
    for(it=occurences.constBegin();it!=occurences.constEnd();it++){
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
StructureEntry::StructureEntry(LatexDocument* doc, Type newType):type(newType),level(0), lineNumber(-1), lineHandle(0), parent(0), document(doc),appendix(false){
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
int StructureEntry::getRealLineNumber(){
	if (document->line(lineNumber).handle() == lineHandle)
		return lineNumber; //return cached line number if it is still correct
	if (lineHandle) {
		int nr = QDocumentLine(lineHandle).lineNumber();
		if (nr>=0) {
			lineNumber=nr; //correct cached line number if necessary
			return nr;
		}
	}
	return lineNumber;
}

StructureEntryIterator::StructureEntryIterator(StructureEntry* entry){
	if (!entry) return;
	while (entry->parent){
		entryHierarchy.prepend(entry);
		indexHierarchy.prepend(entry->parent->children.indexOf(entry));
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
iconDocument(":/images/doc.png"), iconMasterDocument(":/images/masterdoc.png"), iconBibTeX(":/images/bibtex.png"), iconInclude(":/images/include.png"){
	mHighlightIndex=QModelIndex();
	iconSection.resize(LatexParser::structureCommands.count());
	for (int i=0;i<LatexParser::structureCommands.count();i++)
		iconSection[i]=QIcon(":/images/"+LatexParser::structureCommands[i].mid(1)+".png");
}
Qt::ItemFlags LatexDocumentsModel::flags ( const QModelIndex & index ) const{
	if (index.isValid()) return Qt::ItemIsEnabled|Qt::ItemIsSelectable;
	else return 0;
}
QVariant LatexDocumentsModel::data ( const QModelIndex & index, int role) const{
	if (!index.isValid()) return QVariant();
	StructureEntry* entry = (StructureEntry*) index.internalPointer();
	if (!entry) return QVariant();
	switch (role) {
	case Qt::DisplayRole:
		if (entry->type==StructureEntry::SE_DOCUMENT_ROOT){ //show only base file name
			QString title=entry->title.mid(1+qMax(entry->title.lastIndexOf("/"), entry->title.lastIndexOf(QDir::separator())));
			if(title.isEmpty()) title=tr("untitled");
			return QVariant(title);
		}
		//fall through to show full title in other cases
		case Qt::ToolTipRole:
		//qDebug("data %x",entry);
		if (entry->lineNumber>-1)
			return QVariant(entry->title+QString(tr(" (Line %1)").arg(entry->getRealLineNumber()+1)));
		else
			return QVariant(entry->title);
		case Qt::DecorationRole:
		switch (entry->type){
		case StructureEntry::SE_BIBTEX: return iconBibTeX;
		case StructureEntry::SE_INCLUDE: return iconInclude;
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
		if((entry->type==StructureEntry::SE_INCLUDE) && (entry->level==1)) {
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
		return createIndex(row, column, documents.documents.at(row)->baseStructure);
	}
}
QModelIndex LatexDocumentsModel::index ( StructureEntry* entry ) const{
	if (!entry) return QModelIndex();
	if (entry->parent==0 && entry->type==StructureEntry::SE_DOCUMENT_ROOT) {
		int row=documents.documents.indexOf(entry->document);
		if (row<0) return QModelIndex();
		return createIndex(row, 0, entry);
	} else if (entry->parent!=0 && entry->type!=StructureEntry::SE_DOCUMENT_ROOT) {
		int row=entry->parent->children.indexOf(entry);
		if (row<0) return QModelIndex(); //shouldn't happen
		return createIndex(row, 0, entry);
	} else return QModelIndex(); //shouldn't happen
}
QModelIndex LatexDocumentsModel::parent ( const QModelIndex & index ) const{
	if (!index.isValid()) return QModelIndex();
	const StructureEntry* entry = (StructureEntry*) index.internalPointer();
	if (!entry) return QModelIndex();
	if (!entry->parent) return QModelIndex();
	if(entry->level>LatexParser::structureCommands.count() || entry->level<0){
		qDebug("Structure broken! %p",entry);
		//qDebug("Title %s",qPrintable(entry->title));
		return QModelIndex();
	}
	if(entry->parent->level>LatexParser::structureCommands.count() || entry->parent->level<0){
		qDebug("Structure broken! %p",entry);
		//qDebug("Title %s",qPrintable(entry->title));
		return QModelIndex();
	}
	if (entry->parent->parent)
		return createIndex(entry->parent->parent->children.indexOf(entry->parent), 0, entry->parent);
	else {
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
	/* after structure update, a reset is imperative !
	if (document) emit layoutChanged();
	else {
		//Q_ASSERT(false); //branch shouldn't be reachable?
		resetAll();
	}*/
}

void LatexDocumentsModel::removeElement(StructureEntry *se,int row){
	/*
	foreach(QModelIndex ind,persistentIndexList()){
		qDebug("%x",ind.internalPointer());
	}
	*/
	if(!se){ // remove from root (documents)
	    beginRemoveRows(QModelIndex(),row,row);
	}else{
	    StructureEntry *par_se=se->parent;

	    if(row<0){
		    row=par_se->children.indexOf(se);
	    }
	    //removeRow(row,index(par_se));
	    beginRemoveRows(index(par_se),row,row);
	}
}

void LatexDocumentsModel::removeElementFinished(){
	endRemoveRows();
	/*
	foreach(QModelIndex ind,persistentIndexList()){
		qDebug("%x",ind.internalPointer());
	}
	*/
}

void LatexDocumentsModel::addElement(StructureEntry *se,int row){
	//insertRow(row,index(se));
	beginInsertRows(index(se),row,row);
}

void LatexDocumentsModel::addElementFinished(){
	endInsertRows();
}

void LatexDocumentsModel::updateElement(StructureEntry *se){
	emit dataChanged(index(se),index(se));
}

LatexDocuments::LatexDocuments(): model(new LatexDocumentsModel(*this)), masterDocument(0), currentDocument(0), bibTeXFilesModified(false){
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
	LatexEditorView *view=document->getEditorView();
	if(view)
	    view->closeCompleter();
	if (document!=masterDocument) {
		// set document.masterdocument = 0
		foreach(LatexDocument* elem,documents){
		    if(elem->getMasterDocument()==document){
			elem->setMasterDocument(0);
			elem->recheckRefsLabels();
		    }
		}
		//check whether child document was deleted (and recheck labels on all remaining)
		if(document->getMasterDocument()!=0){
		    LatexDocument *md=document->getMasterDocument();
		    document->setMasterDocument(0);
		    foreach(LatexDocument* elem,documents){
			if(elem->getMasterDocument()==md ||elem==md){
			    elem->recheckRefsLabels();
			}
		    }

		}
		int row=documents.indexOf(document);
		if(row>=0){
			model->resetHighlight();
			model->removeElement(0,row); //remove from root
			//model->removeElement(document->baseStructure,row);
		}
		documents.removeAll(document);
		if(row>=0){
			model->removeElementFinished();
		}
		//model->resetAll();
		if (document==currentDocument)
			currentDocument=0;
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

QString LatexDocuments::getCurrentFileName() {
	if (!currentDocument) return "";
	return currentDocument->getFileName();
}
QString LatexDocuments::getCompileFileName(){
	if (masterDocument)
		return masterDocument->getFileName();
	if (!currentDocument)
		return "";
	LatexDocument* masterDoc=currentDocument->getMasterDocument();
	if(masterDoc)
	    return masterDoc->getFileName();
	QString curDocFile = currentDocument->getFileName();
	if (curDocFile.endsWith(".bib"))
		foreach (const LatexDocument* d, documents) {
			QMultiHash<QDocumentLineHandle*,FileNamePair>::const_iterator it = d->mentionedBibTeXFiles().constBegin();
			QMultiHash<QDocumentLineHandle*,FileNamePair>::const_iterator itend = d->mentionedBibTeXFiles().constEnd();
			for (; it != itend; ++it)
				if (it.value().absolute == curDocFile)
					return d->getFileName();
		}
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
	if (!s.endsWith(extension,Qt::CaseInsensitive)) s+=extension;
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
	model->iconSection.resize(LatexParser::structureCommands.count());
	for (int i=0;i<LatexParser::structureCommands.count();i++)
		model->iconSection[i]=QIcon(":/images/"+LatexParser::structureCommands[i].mid(1)+".png");
}
bool LatexDocuments::singleMode(){
	return !masterDocument;
}

void LatexDocuments::updateBibFiles(){
	mentionedBibTeXFiles.clear();
	foreach (LatexDocument* doc, documents) {
		QMultiHash<QDocumentLineHandle*,FileNamePair>::iterator it = doc->mentionedBibTeXFiles().begin();
		QMultiHash<QDocumentLineHandle*,FileNamePair>::iterator itend = doc->mentionedBibTeXFiles().end();
		for (; it != itend; ++it){
			if (it.value().absolute.isEmpty()) it.value().absolute = getAbsoluteFilePath(it.value().relative,".bib").replace(QDir::separator(), "/"); //store absolute
			mentionedBibTeXFiles << it.value().absolute;
		}
	}

	bool changed=false;
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
	if (changed) {
		allBibTeXIds.clear();
		for (QMap<QString, BibTeXFileInfo>::const_iterator it=bibTeXFiles.constBegin(); it!=bibTeXFiles.constEnd();++it)
			foreach (const QString& s, it.value().ids)
				allBibTeXIds << s;
		for (int i=0;i<documents.size();i++)
			if (documents[i]->getEditorView())
				documents[i]->getEditorView()->setBibTeXIds(&allBibTeXIds);
		bibTeXFilesModified=true;
	}
}

void LatexDocument::findStructureEntryBefore(QMutableListIterator<StructureEntry*> &iter,QMultiHash<QDocumentLineHandle*,StructureEntry*> &MapOfElements,int linenr,int count){
	bool goBack=false;
	int l=0;
	while(iter.hasNext()){
		StructureEntry* se=iter.next();
		QDocumentLineHandle* dlh=se->lineHandle;
		int realline = indexOf(dlh);
		Q_ASSERT(realline >= 0);
		if(realline>=linenr && (realline<linenr+count) ){
			emit removeElement(se,l);
			iter.remove();
			emit removeElementFinished();
			MapOfElements.insert(se->lineHandle,se);
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

void LatexDocument::splitStructure(StructureEntry* se,QVector<StructureEntry*> &parent_level,QVector<QList<StructureEntry*> > &remainingChildren,QMap<StructureEntry*,int> &toBeDeleted,QMultiHash<QDocumentLineHandle*,StructureEntry*> &MapOfElements,int linenr,int count,int lvl,bool front,bool back){
	if (!se) return;

	if(front){
		for(int k=lvl;k<parent_level.size();k++){
			parent_level[k]=se;
		}
	}

	if(se->children.empty()) return;

	StructureEntry* parent=se;

	if(lvl>=parent_level.size()) return;

	int countChildren=se->children.size();

	// determine range of structure entry which encompass the to be updated region
	int start=-1;
	int end=-1;
	for(int l=0;l<se->children.size();l++){
		StructureEntry *elem=se->children.at(l);
		//TODO: remove line()/indexOf() call, it is too slow
		int realline = elem->lineHandle?indexOf(elem->lineHandle):-1;
		if(realline<linenr) {
			start=l;
			continue;
		}
		if(realline>=linenr+count){
			end=l;
			break;
		}
	}

	if(start>-1){
		StructureEntry *elem=se->children.at(start);
		if(elem->type==StructureEntry::SE_SECTION){
			for(;lvl<elem->level;++lvl){
				parent_level[lvl+1]=parent;
			}
		}
	}

	// store remainder of children
	if(back && (end>-1))
		remainingChildren[se->children.at(end)->level]<<se->children.mid(end);
	// get StructureEntry to look deeper in
	StructureEntry *next=0;
	if(end>0) {
		next=se->children.value(end-1,0);// needs changing
		if(next && next->type!=StructureEntry::SE_SECTION) next=0;
		parent=se->children.value(start,parent_level[lvl]);
		if(parent->type!=StructureEntry::SE_SECTION) parent=parent_level[lvl];
	}
	if(end<0){
		next=se->children.value(se->children.size()-1,0);// needs changing
		if(next && next->type!=StructureEntry::SE_SECTION) next=0;
		parent=next;
		if(!parent) parent=parent_level[lvl];
		if(parent->type!=StructureEntry::SE_SECTION) parent=parent_level[lvl];
	}

	// add elements which are deleted later to a list
	if(end-1>start) {
		toBeDeleted.insert(se->children[end-1],end-1);
		MapOfElements.insert(se->children[end-1]->lineHandle,se->children[end-1]);
	}
	//delete elements which are completely embedded in the to be updated region
	int tmp_end=end;
	if(tmp_end<0) tmp_end=se->children.size()+1;
	for(int l=start+1;l<tmp_end-1;l++) {
		toBeDeleted.insert(se->children[l],l);
		MapOfElements.insert(se->children[l]->lineHandle,se->children[l]);
		//delete se->children[l];
	}

	int tmp=se->children.size();
	for(int l=start+1;l<tmp;l++) {
		se->children.removeAt(start+1);
	}

	// take a look a children
	bool newFront=start>-1;
	if(newFront && (end-start==1 || start==countChildren-1))
		splitStructure(next,parent_level,remainingChildren,toBeDeleted,MapOfElements,linenr,count,lvl+1);
	else{
		if(newFront && (se->children[start]->type==StructureEntry::SE_SECTION)) splitStructure(se->children[start],parent_level,remainingChildren,toBeDeleted,MapOfElements,linenr,count,lvl+1,newFront,false);
		int next_level= next ? next->level+1 : lvl;
		splitStructure(next,parent_level,remainingChildren,toBeDeleted,MapOfElements,linenr,count,next_level,false,true);
	}

}

void LatexDocument::removeAndDeleteElement(StructureEntry* se, int row){
	emit removeElement(se,row);
	//qDebug("Structure deleted! %p %d",se,toBeDeleted[se]);
	//qDebug() << se->title;
	delete se;
	emit removeElementFinished();
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
		if(endLine>=0 && elem->lineHandle && indexOf(elem->lineHandle)>endLine) break;
		if(elem->type==StructureEntry::SE_SECTION && indexOf(elem->lineHandle)>startLine){
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
	bool exist=QFile(parent->getAbsoluteFilePath(fname,".tex")).exists();
	if (!exist) exist=QFile(parent->getAbsoluteFilePath(curPath+fname,".tex")).exists();
	if (!exist) exist=QFile(parent->getAbsoluteFilePath(curPath+fname,"")).exists();
	return exist;
}

QString LatexDocument::findFileName(QString fname){
	QString curPath=ensureTrailingDirSeparator(getFileInfo().absolutePath());
	QString result;
	if(QFile(parent->getAbsoluteFilePath(fname,".tex")).exists())
	    result=QFileInfo(parent->getAbsoluteFilePath(fname,".tex")).absoluteFilePath();
	if (result.isEmpty() && QFile(parent->getAbsoluteFilePath(curPath+fname,".tex")).exists())
	    result=QFileInfo(parent->getAbsoluteFilePath(curPath+fname,".tex")).absoluteFilePath();
	if (result.isEmpty() && QFile(parent->getAbsoluteFilePath(curPath+fname,"")).exists())
	    result=QFileInfo(parent->getAbsoluteFilePath(curPath+fname,"")).absoluteFilePath();
	return result;
}

void LatexDocuments::updateStructure(){
	foreach(const LatexDocument* doc,documents){
		model->updateElement(doc->baseStructure);
	}
}

void LatexDocuments::updateLayout(){
	model->layoutChanged();
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
	foreach(const QString fname,includedFiles){
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
    bool update=forceUpdate;
    foreach(QString elem,removed){
	if(!mUsepackageList.keys(elem).isEmpty())
	    continue;
	elem.append(".cwl");
	if(!filtered.contains(elem)){
	    QString fn=findResourceFile("completion/"+elem);
	    if(!fn.isEmpty())
		filtered << elem;
	}
    }
    if(!filtered.isEmpty()){
	LatexParser cmds;
	QStringList removedWords=loadCwlFiles(filtered,&cmds);
	ltxCommands.substract(cmds);
	foreach(const QString elem,removedWords){
	    mCompleterWords.removeAll(elem);
	}
	//recheck syntax of ALL documents ...
	update=true;
    }
    // add
    filtered.clear();
    foreach(QString elem,added){
	elem.append(".cwl");
	if(!filtered.contains(elem)){
	    QString fn=findResourceFile("completion/"+elem);
	    if(!fn.isEmpty())
		filtered << elem;
	}
    }
    if(!filtered.isEmpty()){
	LatexParser cmds;
	QStringList addedWords=loadCwlFiles(filtered,&cmds);
	ltxCommands.append(cmds);
	mCompleterWords << addedWords;
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


