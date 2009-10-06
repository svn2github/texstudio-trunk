#include "smallUsefulFunctions.h"

const QString CommonEOW="~!@#$%^&*()_+{}|:\"\\<>?,./;[]-= \t\n\r`+�";
const QString EscapedChars="%&_";
const QString CharacterAlteringChars="\"";

const QStringList refCommands = QStringList() << "\\ref" << "\\pageref" ;
const QStringList labelCommands = QStringList() << "\\label" ;
const QStringList citeCommands = QStringList() << "\\cite" << "\\nocite" << "\\citeauthor" << "\\textcite" << "\\parencite" << "\\citetitle" << "\\footcite" << "\\nptextcite" ;
const QStringList environmentCommands = QStringList() << "\\begin" << "\\end" << "\\newenvironment" << "\\renewenvironment";


QString getCommonEOW() {
	return CommonEOW;
}

QStringList findResourceFiles(const QString& dirName, const QString& filter) {
	QStringList searchFiles;
	QString dn = dirName;
	if (dn.endsWith('/')||dn.endsWith(QDir::separator())) dn=dn.left(dn.length()-1); //remove / at the end
	if (!dn.startsWith('/')&&!dn.startsWith(QDir::separator())) dn="/"+dn; //add / at beginning
	searchFiles<<":"+dn; //resource fall back
	searchFiles<<QCoreApplication::applicationDirPath() + dn; //windows new
	// searchFiles<<QCoreApplication::applicationDirPath() + "/data/"+fileName; //windows new
#if defined( Q_WS_X11 )
	searchFiles<<PREFIX"/share/texmakerx"+dn; //X_11
#endif

	QStringList result;
	foreach(const QString& fn, searchFiles) {
		QDir fic(fn);
		if (fic.exists() && fic.isReadable()) 
			result<< fic.entryList(QStringList(filter),QDir::Files,QDir::Name);
	}
	// sort and remove double entries
	result.sort();

	QMutableStringListIterator i(result);
	QString old="";
	while(i.hasNext()){
		QString cmp=i.next();
		if(cmp==old) i.remove();
		else old=cmp;
	}
	return result;
}

QString findResourceFile(const QString& fileName) {
	QStringList searchFiles;

	searchFiles<<":/"+fileName; //resource fall back
	searchFiles<<QCoreApplication::applicationDirPath() + "/../Resources/"+fileName; //macx
	searchFiles<<QCoreApplication::applicationDirPath() + "/"+fileName; //windows old
	searchFiles<<QCoreApplication::applicationDirPath() + "/dictionaries/"+fileName; //windows new
	searchFiles<<QCoreApplication::applicationDirPath() + "/translations/"+fileName; //windows new
	searchFiles<<QCoreApplication::applicationDirPath() + "/help/"+fileName; //windows new
	searchFiles<<QCoreApplication::applicationDirPath() + "/utilities/"+fileName; //windows new
	// searchFiles<<QCoreApplication::applicationDirPath() + "/data/"+fileName; //windows new
#if defined( Q_WS_X11 )
	searchFiles<<PREFIX"/share/texmakerx/"+fileName; //X_11
#endif


	foreach(const QString& fn, searchFiles) {
		QFileInfo fic(fn);
		if (fic.exists() && fic.isReadable()) {
			return fn;
			break;
		}
	}
	return "";
}

bool isFileRealWritable(const QString& filename) {
#ifdef Q_WS_WIN
	//thanks to Vistas virtual folders trying to open an unaccessable file can create it somewhere else
	return QFileInfo(filename).isWritable();
#endif
	QFile fi(filename);
	bool result=false;
	if (fi.exists()) result=fi.open(QIODevice::ReadWrite);
	else {
		result=fi.open(QIODevice::WriteOnly);
		fi.remove();
	}
	return result;
}

bool isExistingFileRealWritable(const QString& filename) {
	return QFileInfo(filename).exists() && isFileRealWritable(filename);
}


int x11desktop_env() {
	// 0 : no kde ; 3: kde ; 4 : kde4 ;
	QString kdesession= ::getenv("KDE_FULL_SESSION");
	QString kdeversion= ::getenv("KDE_SESSION_VERSION");
	if (!kdeversion.isEmpty()) return 4;
	if (!kdesession.isEmpty()) return 3;
	return 0;
}


QString latexToPlainWord(const QString& word) {
	QList<QPair<QString,QString> > replaceList;
	replaceList.append(QPair<QString, QString> ("\\-","")); //Trennung [separation] (german-babel-package also: \")
	replaceList.append(QPair<QString, QString> ("\\/","")); //ligatur preventing (german-package also: "|)
	replaceList.append(QPair<QString, QString> ("\"~","-")); //- ohne Trennung (without separation)
	//german-babel-package: "- (\- but also normal break),  "= ( like - but also normal break), "" (umbruch ohne bindestrich)
	replaceList.append(QPair<QString, QString> ("\"-",""));
	replaceList.append(QPair<QString, QString> ("\"a","\xE4"));
	replaceList.append(QPair<QString, QString> ("\"o","\xF6"));
	replaceList.append(QPair<QString, QString> ("\"u","\xFC"));
	replaceList.append(QPair<QString, QString> ("\"A","\xC4"));
	replaceList.append(QPair<QString, QString> ("\"O","\xD6"));
	replaceList.append(QPair<QString, QString> ("\"U","\xDC"));
	replaceList.append(QPair<QString, QString> ("\\\"{a}","\xE4"));
	replaceList.append(QPair<QString, QString> ("\\\"{o}","\xF6"));
	replaceList.append(QPair<QString, QString> ("\\\"{u}","\xFC"));
	replaceList.append(QPair<QString, QString> ("\\\"{A}","\xC4"));
	replaceList.append(QPair<QString, QString> ("\\\"{O}","\xD6"));
	replaceList.append(QPair<QString, QString> ("\\\"{U}","\xDC"));
	replaceList.append(QPair<QString, QString> ("\"|",""));
	replaceList.append(QPair<QString, QString> ("\"",""));
	//replaceList.append(QPair<QString, QString> ("\"\"","")); redunant
	replaceList.append(QPair<QString, QString> ("\\","")); // eliminating backslash which might remain from accents like \"a ...

	QString result=word;
	for (QList<QPair<QString,QString> >::const_iterator it=replaceList.begin(); it!=replaceList.end(); ++it)
		result.replace(it->first,it->second);

	return result;
}

QString extractSectionName(const QString& word) {
	int i=0;
	int start=word.indexOf("{",i);
	i=start;
	int stop=word.indexOf("}",i);
	i=word.indexOf("{",i+1);
	while (i>0 && stop>0 && i<stop) {
		stop=word.indexOf("}",stop+1);
		i=word.indexOf("{",i+1);
	}
	if (stop<0) stop=word.length();
	return word.mid(start+1,stop-start-1);
}

QString textToLatex(const QString& text) {
	QList<QPair<QString,QString> > replaceList;
	replaceList.append(QPair<QString, QString> ("\\","\\verb+\\+"));
	replaceList.append(QPair<QString, QString> ("{","\\{"));
	replaceList.append(QPair<QString, QString> ("}","\\}"));
	replaceList.append(QPair<QString, QString> ("#","\\#"));
	replaceList.append(QPair<QString, QString> ("$","\\$"));
	replaceList.append(QPair<QString, QString> ("%","\\%"));
	replaceList.append(QPair<QString, QString> ("&","\\&"));
	replaceList.append(QPair<QString, QString> ("~","\\~{}"));
	replaceList.append(QPair<QString, QString> ("_","\\_"));
	replaceList.append(QPair<QString, QString> ("^","\\^"));

	QString result=text;
	for (QList<QPair<QString,QString> >::const_iterator it=replaceList.begin(); it!=replaceList.end(); ++it)
		result.replace(it->first,it->second);

	return result;
}

bool localAwareLessThan(const QString &s1, const QString &s2) {
	return QString::localeAwareCompare(s1,s2)<0;
}


int nextToken(const QString &line,int &index,bool abbreviation) {
	bool inWord=false;
	bool inCmd=false;
	//bool reparse=false;
	bool singleQuoteChar=false;
	bool ignoreBrace=false;
	bool ignoreClosingBrace=false;
	int start=-1;
	int i=index;
	for (i=(i>0?i:0); i<line.size(); i++) {
		QChar cur = line.at(i);
		if(ignoreBrace && cur=='{'){
			ignoreBrace=false;
			ignoreClosingBrace=true;
			continue;
		} else ignoreBrace=false;
		if(ignoreClosingBrace && cur=='}'){
			ignoreClosingBrace=false;
			continue;
		}
		if (inCmd) {
			if (CommonEOW.indexOf(cur)>=0) {
				if (i-start==1) i++;
				break;
			}
		} else if (inWord) {
			if (cur=='\\') {
				if (i+1<line.size() && (line.at(i+1)=='-'||EscapedChars.indexOf(line.at(i+1))>=0||CharacterAlteringChars.indexOf(line.at(i+1))>=0))  {
					if(CharacterAlteringChars.indexOf(line.at(i+1))>=0) ignoreBrace=true;
					i++;//ignore word separation marker and _ respectively
					//reparse=true;
				} else break;
			} else if (cur=='"') {  //ignore " like in "-, "", "| "a
				if (i+1<line.size()){
					QChar nextChar=line.at(i+1);
					if(nextChar=='-' || nextChar=='"' || nextChar=='|')  i++; 
					else if(!nextChar.isLetterOrNumber()) break;
				}
				else break;
			} else if (cur=='\'') {
				if (singleQuoteChar) break;	 //no word's with two '' => output
				else singleQuoteChar=true;   //but accept one
			} else if (cur=='.' && abbreviation) {
				i++; //take '.' into word, so that abbreviations, at least German ones, are checked correctly
				break;
			} else if (CommonEOW.indexOf(cur)>=0) break;
		} else if (cur=='\\') {
			if (i+1<line.size() && (EscapedChars.indexOf(line.at(i+1))>=0||CharacterAlteringChars.indexOf(line.at(i+1))>=0))  {
				inWord=true;
				start=i;
				if(CharacterAlteringChars.indexOf(line.at(i+1))>=0) ignoreBrace=true;
				i++;
			}else{
				start=i;
				inCmd=true;
			}
		} else if (cur=='{' || cur=='}' || cur=='%') {
			index=i+1;
			return i;
		} else if (CommonEOW.indexOf(cur)<0 && cur!='\'' || cur=='"') {
			start=i;
			inWord=true;
		}
	}
	if (singleQuoteChar && i-1<line.size() && line.at(i-1)=='\'') 
		i--; //remove ' when a word ends with it  (starting is handled because ' does not start a word)
	index=i;
	return start;
}


NextWordFlag nextWord(const QString &line,int &index,QString &outWord,int &wordStartIndex, bool returnCommands,bool abbreviations) {
	static const QStringList optionCommands = QStringList() << "\\ref" << "\\pageref" << "\\label"  << "\\includegraphics" << "\\usepackage" << "\\documentclass" << "\\include" << "\\input" << "\\hspace" << "\\vspace";

	int reference=-1;
	QString lastCommand="";
	while ((wordStartIndex = nextToken(line, index,abbreviations))!=-1) {
		outWord=line.mid(wordStartIndex,index-wordStartIndex);
		if (outWord.length()==0) return NW_NOTHING; //should never happen
		switch (outWord.at(0).toAscii()) {
		case '%':
			return NW_COMMENT; //return comment start
		case '{':
			if (reference!=-1)
				reference=wordStartIndex+1;
			break; //ignore
		case '}':
			if (reference!=-1)
				if (refCommands.contains(lastCommand)){
					wordStartIndex=reference;
					--index;
					outWord=line.mid(reference,index-reference);
					return NW_REFERENCE;
				} else if (labelCommands.contains(lastCommand)){
					wordStartIndex=reference;
					--index;
					outWord=line.mid(reference,index-reference);
					return NW_LABEL;
				} else if (citeCommands.contains(lastCommand)){
					wordStartIndex=reference;
					--index;
					outWord=line.mid(reference,index-reference);
					return NW_CITATION;
				}
			lastCommand="";
			break;//command doesn't matter anymore
		case '\\':
			if (outWord.length()==1 || !(EscapedChars.contains(outWord.at(1)) || CharacterAlteringChars.contains(outWord.at(1)))) {
				if (returnCommands) return NW_COMMAND;
				if (!optionCommands.contains(lastCommand)) {
					lastCommand=outWord;
					if (refCommands.contains(lastCommand)||labelCommands.contains(lastCommand)||citeCommands.contains(lastCommand))
						reference=index; //todo: support for nested brackets like \cite[\xy{\ab{s}}]{miau}
				}
				break;
			} else ; //first character is escaped, fall through to default case
		default:
			if (reference==-1) {
				if (outWord.contains("\\")||outWord.contains("\""))
					outWord=latexToPlainWord(outWord); //remove special chars
				if (optionCommands.contains(lastCommand))
					; //ignore command options
				else if (environmentCommands.contains(lastCommand))
					return NW_ENVIRONMENT;
				else return NW_TEXT;
			}
		}
	}
	return NW_NOTHING;
}

bool nextTextWord(const QString & line, int &index, QString &outWord, int &wordStartIndex) {
	NextWordFlag flag;
	//flag can be nothing, text, comment, environment
	//text/comment returns false, text returns true, environment is ignored
	while ((flag=nextWord(line,index,outWord,wordStartIndex,false))==NW_ENVIRONMENT)
		;
	return flag==NW_TEXT;
}

QString findToken(const QString &line,const QString &token){
	int tagStart=line.indexOf(token);
	if (tagStart!=-1) {
		tagStart+=token.length();
		int tagEnd=line.indexOf("}",tagStart);
		if (tagEnd!=-1) return line.mid(tagStart,tagEnd-tagStart);
		else return line.mid(tagStart); //return everything after line if there is no }
	}
	return "";
}

QString findToken(const QString &line,QRegExp &token){
//ATTENTION: token is not const because, you can't call cap on const qregexp in qt < 4.5
	int tagStart=0;
	QString s=line;
	tagStart=token.indexIn(line);
	int commentStart=line.indexOf(QRegExp("(^|[^\\\\])%")); // find start of comment (if any)
	if (tagStart!=-1 && (commentStart>tagStart || commentStart==-1)) {
		s=s.mid(tagStart+token.cap(0).length(),s.length());
		return s;
	}
	return "";
}
bool findTokenWithArg(const QString &line,const QString &token, QString &outName, QString &outArg){
	outName="";
	outArg="";
	int tagStart=line.indexOf(token);
	int commentStart=line.indexOf(QRegExp("(^|[^\\\\])%")); // find start of comment (if any)
	if (tagStart!=-1 && (commentStart>tagStart || commentStart==-1)) {
		tagStart+=token.length();
		int tagEnd=line.indexOf("}",tagStart);
		if (tagEnd!=-1) {
			outName=line.mid(tagStart,tagEnd-tagStart);
			int curlyOpen=line.indexOf("{",tagEnd);
			int optionStart=line.indexOf("[",tagEnd);
			if (optionStart<curlyOpen || (curlyOpen==-1 && optionStart!=-1)) {
				int optionEnd=line.indexOf("]",optionStart);
				if (optionEnd!=-1) outArg=line.mid(optionStart+1,optionEnd-optionStart-1);
				else outArg=line.mid(optionStart+1);
			}
		} else outName=line.mid(tagStart); //return everything after line if there is no }
		return true;
	}
	return false;
	
}


QToolButton* createComboToolButton(QWidget *parent,const QStringList& list,const int height,const QFontMetrics fm,const QObject * receiver, const char * member){
	QToolButton *combo=new QToolButton(parent);
	combo->setPopupMode(QToolButton::MenuButtonPopup);
	combo->setMinimumHeight(height);

	QAction *mAction=new QAction(list[0],parent);
	QObject::connect(mAction, SIGNAL(triggered()),receiver,member);
	combo->setDefaultAction(mAction);
	QMenu *mMenu=new QMenu(parent);
	int max=0;
	foreach(QString elem,list){
		mMenu->addAction(elem,receiver,member);
		max=qMax(max,fm.width(elem+"        "));
	}
	combo->setMinimumWidth(max);
	combo->setMenu(mMenu);
	return combo;
}


bool hasAtLeastQt(int major, int minor){
	QStringList vers=QString(qVersion()).split('.');
	if (vers.count()<2) return false;
	int ma=vers[0].toInt();
	int mi=vers[1].toInt();
	return (ma>major) || (ma==major && mi>=minor);
}

QString cutComment(const QString& text){
	return text.left(LatexParser::commentStart(text)); // remove comments
}

int LatexParser::findContext(QString &line,int col){
	int start_command=col;
	int start_ref=col;
	int start_close=col;
	int stop=col;
	int bow=col;
	QString helper=line.mid(0,col);
	start_command=helper.lastIndexOf("\\");
	start_ref=helper.lastIndexOf("{");
	start_close=helper.lastIndexOf("}");
	bow=helper.lastIndexOf("[");
	if(bow<helper.lastIndexOf(" ")) bow=helper.lastIndexOf(" ");
	helper=line.mid(col,line.length());
	if((start_command>start_ref)&&(start_command>start_close)&&(start_command>bow)){
		helper=line.mid(start_command);
		stop=helper.indexOf("{");
		if(stop>-1) stop=stop;
		else {
			stop=helper.indexOf("[");
			int stop2=helper.indexOf(" ");
			if(stop==-1) stop=stop2;
			if(stop2==-1) stop2=stop;
			if(stop2<stop) stop=stop2;
		}
		if(stop==-1) stop=helper.length();
		line=helper.mid(0,stop);
		return 1;
	}
	if((start_ref>start_command)&&(start_command>start_close)){
		stop=helper.indexOf("}")+col;
		line=line.mid(start_command,stop-start_command);
		return 2;
	}
	return 0;
}

LatexParser::ContextType LatexParser::findContext(const QString &line, int column, QString &command, QString& value){
	command=line;
	int temp=findContext(command,column);
	int a=command.indexOf("{");
	if (a>=0) {
		int b=command.indexOf("}");
		if (b<0) b = command.length();
		value=command.mid(a+1,b-a-1);
		command=command.left(a);
	} else {
		int a=line.indexOf("{",column);
		int b=line.indexOf("}",column);
		value=line.mid(a+1,b-a-1);
	}
	switch (temp) {
		case 0: return Unknown;
		case 1: return Command;
		case 2: 
			if (environmentCommands.contains(command))
				return Environment;
			else if (labelCommands.contains(command)) 
				return Label;
			else if (refCommands.contains(command))
				return Reference;
			else if (citeCommands.contains(command))
				return Citation;
		default: return Unknown;
	}
}

int LatexParser::commentStart(const QString& text){
	if(text.startsWith("%")) return 0;
	QString test=text;
	test.replace("\\\\","  ");
	int cs=test.indexOf(QRegExp("[^\\\\]%")); // find start of comment (if any)
	if(cs>-1) return cs+1;
	else return -1;
}
