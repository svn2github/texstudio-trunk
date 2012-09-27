#ifndef SMALLUSEFULFUNCTIONS_T_H
#define SMALLUSEFULFUNCTIONS_T_H
#ifndef QT_NO_DEBUG
#include "mostQtHeaders.h"
#include "smallUsefulFunctions.h"
#include "testutil.h"
#include <QtTest/QtTest>

const int NW_IGNORED_TOKEN=-2; //token that are not words,  { and }
const int NW_OPTION=-3; //option text like in \include
const int NW_OPTION_PUNCTATION=-4; //option punctation like in \include

class TestToken: public QString{
	static const QRegExp simpleTextRegExp; //defined in testmanager.cpp
	static const QRegExp commandRegExp;
	static const QRegExp ignoredTextRegExp;
	static const QRegExp specialCharTextRegExp;
	static const QRegExp punctationRegExp;
	void guessType(){
		if (simpleTextRegExp.exactMatch(*this)) type=LatexReader::NW_TEXT;
		else if (commandRegExp.exactMatch(*this)) type=LatexReader::NW_COMMAND;
		else if (punctationRegExp.exactMatch(*this)) type=LatexReader::NW_PUNCTATION;
		else if (ignoredTextRegExp.exactMatch(*this)) type=NW_IGNORED_TOKEN;
		else if (*this=="%") type=LatexReader::NW_COMMENT;
		else if (specialCharTextRegExp.exactMatch(*this)) type=LatexReader::NW_TEXT;
		else QVERIFY2(false, QString("invalid test data: \"%1\"").arg(*this).toLatin1().constData());
	}
public:
	int type,position;
	TestToken():QString(), type(NW_IGNORED_TOKEN),soll(QString()){ }
	TestToken(const TestToken& token):QString(token), type(token.type), position(token.position), soll(token.soll) { }
	TestToken(const QString& str):QString(str),soll(str) {
		guessType();
	}
	TestToken(const char* cstr):QString(cstr), soll(QString(cstr)){
		guessType();
	}
	TestToken(const QString& str, int atype):QString(str),type(atype),soll(str){}
	TestToken(const QString& str, const QString result, int atype):QString(str),type(atype),soll(result){}
	bool operator ==(const QString &other) {
		return other.operator ==(soll);
	}
private:
	QString soll;
};

Q_DECLARE_METATYPE(TestToken);
Q_DECLARE_METATYPE(QList<TestToken>);

/*QList<TestToken>& operator<< (QList<TestToken> & list, const char* str){
	return list << TestToken(str);
}*/

class SmallUsefulFunctionsTest: public QObject{
	Q_OBJECT
	TestToken env(const QString& str){
		return TestToken(str, LatexReader::NW_ENVIRONMENT);
	}
	TestToken option(const QString& str){
		return TestToken(str, NW_OPTION);
	}
	enum TokenFilter {FILTER_NEXTTOKEN,FILTER_NEXTWORD_WITH_COMMANDS,FILTER_NEXTWORD,FILTER_NEXTTEXTWORD};
	void addRow(const char* name, TokenFilter filter, QList<TestToken> tokens){
		QString str;
		int curpos=0;
		int firstComment=tokens.size();
		for (int i=0;i<tokens.size();i++){
			tokens[i].position=curpos;
			str+=tokens[i];
			curpos+=tokens[i].size();
			if (tokens[i].type==LatexReader::NW_COMMENT && i<firstComment) firstComment=i;
		}
		//remove all tokens which don't belong to the current test
		switch (filter){
			case FILTER_NEXTTOKEN:
				for (int i=tokens.size()-1;i>=0;i--)
					if (tokens[i].type==NW_IGNORED_TOKEN && tokens[i]!="{" && tokens[i]!="}" && tokens[i]!="[" && tokens[i]!="]")
						tokens.removeAt(i);
				break;
			case FILTER_NEXTWORD_WITH_COMMANDS:
				for (int i=tokens.size()-1;i>=0;i--)
					if (tokens[i].type==NW_IGNORED_TOKEN)	tokens.removeAt(i);
					else if (tokens[i].type==LatexReader::NW_ENVIRONMENT || tokens[i].type==NW_OPTION)
						tokens[i].type=LatexReader::NW_TEXT;//nextWord in command mode don't distinguish between text, environments and options
					else if (tokens[i].type==NW_OPTION_PUNCTATION)
						tokens[i].type=LatexReader::NW_PUNCTATION;
					//else if (tokens[i].contains("\\") && tokens[i].type==NW_TEXT)
						//tokens[i].replace("\\%","%"); //unescape escaped characters
				break;
			case FILTER_NEXTWORD:
				for (int i=tokens.size()-1;i>=0;i--) 
					if (tokens[i].type==LatexReader::NW_COMMAND || tokens[i].type==NW_OPTION || tokens[i].type==NW_OPTION_PUNCTATION || tokens[i].type==NW_IGNORED_TOKEN) 
						tokens.removeAt(i);//remove tokens not returned by nextWord in text mode
					//else if (tokens[i].contains("\\") && tokens[i].type==LatexReader::NW_TEXT)
						//tokens[i].replace("\\",""); //unescape escaped characters
				break;
			case FILTER_NEXTTEXTWORD:
				for (int i=tokens.size()-1;i>=0;i--)
					if (tokens[i].type!=LatexReader::NW_TEXT || i>=firstComment)  
						tokens.removeAt(i);//remove all except text before comment start
					//else if (tokens[i].contains("\\") && tokens[i].type==LatexReader::NW_TEXT)
						//tokens[i].replace("\\",""); //unescape escaped characters
				break;
			default:
				QVERIFY2(false, "Invalid filter");
		}
		QTest::newRow(name) << str << tokens;
	}
	void addComplexData(TokenFilter filter){
		QTest::addColumn<QString >("str");
		QTest::addColumn<QList<TestToken> >("tokens");

		addRow("simple whitespace", filter,
			QList<TestToken>() << "abcde" << "    " << "fghik" << "\t" << "Mice");
		addRow("simple eow", filter,
		       QList<TestToken>() << "abcde" << ";" << ":" << ";" << "fghik" << TestToken("##", NW_IGNORED_TOKEN) << "Mice" << TestToken("///", NW_IGNORED_TOKEN) << "\\\\" << TestToken("+++", NW_IGNORED_TOKEN) << "axy" << TestToken("---", "-", LatexReader::NW_PUNCTATION));
		addRow("simple eow", filter,
		       QList<TestToken>() << "abcde." << ";" << ":" << ";" << "fghik" << TestToken("##", NW_IGNORED_TOKEN) << "Mice" << TestToken("///", NW_IGNORED_TOKEN) << "\\\\" << TestToken("+++", NW_IGNORED_TOKEN) << "axy" << TestToken("---", "-", LatexReader::NW_PUNCTATION));
		addRow("environment+comment",filter,
			QList<TestToken>() << "Test1234" << "\\begin" << "{" << env("environment") << "}" << "{" << "add" << "}" << "XYZ" << "!" << "!" << "!" << "\\command" << "%"     << "comment" << "\\COMMENT");
		addRow("some environments", filter,
			QList<TestToken>() << "\\newenvironment" << "{" << env("env") << "}" << " " << "\\begin" << "{" << env("env2") << "}" << " " << "\\end" << "{" << env("env3") << "}" << "  " << "\\renewenvironment" << "{" << env("env4") << "}");
		addRow("misc", filter, //was only for nextWord, other test will of course not ignore \\ignoreMe 
			QList<TestToken>() << "hallo" << " " << "welt" << "\\section" << "{" << "text" << "}" << "     " << "\\begin" << "{" << env("I'mXnotXthere") << "}" << " *" << "g"  << "* " << "%"     << " " << "more" << " " << "\\comment");
		addRow("command as option", filter,
			QList<TestToken>() << "\\includegraphics" << "[" << option("ab.") << "\\linewidth" << "]" << "{" << "\\abc" << " " << option("dfdf") << "\\xyz" << "}" << "continue");
		addRow("command as option", filter,
		       QList<TestToken>() << "\\includegraphics" << "[" << option("ab") << TestToken(":", NW_OPTION_PUNCTATION) << "\\linewidth" << "]" << "{" << "\\abc" << " " << option("dfdf") << "\\xyz" << "}" << "continue");
		addRow("comments", filter, QList<TestToken>() << "hallo" << " " << "welt" <<  "  " << "\\\\" << "normaltext" <<  "  " << TestToken("\\%",NW_IGNORED_TOKEN) << "!" << "!" << "!" << "stillNoComment" << "\\\\" << TestToken("\\%","%",NW_IGNORED_TOKEN) <<"  "<< "none" << "\\\\" << "%" << "comment" << "   " << "more" << " " << "comment");
		addRow("escaped characters", filter, QList<TestToken>() << "hallo" << TestToken("\\%",NW_IGNORED_TOKEN) << "abc");
		addRow("escaped characters", filter, QList<TestToken>() << "1234" << TestToken("\\%\\&\\_",NW_IGNORED_TOKEN)  << "567890");
		addRow("special characters", filter,
			   QList<TestToken>() << "l�sbar" << " " << TestToken("l\"osbar","l�sbar",LatexReader::NW_TEXT) << " " << TestToken("l\\\"osbar","l�sbar",LatexReader::NW_TEXT) << " " << TestToken("l\\\"{o}sbar","l�sbar",LatexReader::NW_TEXT) << " " << "�rtlich" <<" " <<TestToken("\"ortlich","�rtlich",LatexReader::NW_TEXT)<<" " <<TestToken("\\\"ortlich","�rtlich",LatexReader::NW_TEXT)<<" " <<TestToken("\\\"{o}rtlich","�rtlich",LatexReader::NW_TEXT) );
	}
	void nextWord_complex_test(bool commands){
		//get data
		QFETCH(QString, str);	
		QFETCH(QList<TestToken>, tokens);	
		int pos=0; int type;
		LatexReader lr(str);
		while ((type=lr.nextWord(commands))!=LatexReader::NW_NOTHING) {
			const int& startIndex = lr.wordStartIndex;
			const QString& token = lr.word;
			if (pos>=tokens.size()) {
				QFAIL(QString("Found additional token: %1 at %2").arg(token).arg(startIndex).toLatin1().constData());
			} else {
				QVERIFY2(tokens[pos]==token, QString("Invalid token: \"%1\" at \"%2\" expected \"%3\"").arg(token).arg(startIndex).arg(tokens[pos]).toLatin1().constData());
				QVERIFY2(startIndex==tokens[pos].position, QString("Invalid startIndex: %2 for %1").arg(token).arg(startIndex).toLatin1().constData());
				QVERIFY2(type==tokens[pos].type, QString("Invalid type: %2 for %1").arg(token).arg(type).toLatin1().constData());
			}
			pos++;
		}
		QVERIFY2 (pos==tokens.size(), QString("Didn't find all tokens, missing: %1").arg(tokens[qMin(pos, tokens.size()-1)]).toLatin1().constData());
	}
private slots:
	void nextToken_complex_data(){ addComplexData(FILTER_NEXTTOKEN); }
	void nextToken_complex(){
		//get data
		QFETCH(QString, str);	
		QFETCH(QList<TestToken>, tokens);	
		
		//check
		int index=0;int startIndex=0;
		int pos=0;
		while ((startIndex = LatexReader::nextToken(str,index,false,false)) != -1) {
			QString token=str.mid(startIndex,index-startIndex);
			if (pos>=tokens.size()) {
				QFAIL(QString("Found additional token: %1 at %2").arg(token).arg(startIndex).toLatin1().constData());
			} else {
				QVERIFY2(token==tokens[pos], QString("Invalid token: %1 at %2 expected %3").arg(token).arg(startIndex).arg(tokens[pos]).toLatin1().constData());
				QVERIFY2(startIndex==tokens[pos].position, QString("Invalid startIndex: %2 for %1").arg(token).arg(startIndex).toLatin1().constData());
			}
			pos++;
		}
		QVERIFY2 (pos==tokens.size(), "Didn't found all tokens");
	}
	void nextWordWithCommands_complex_data(){ addComplexData(FILTER_NEXTWORD_WITH_COMMANDS); }
	void nextWordWithCommands_complex(){ 
		nextWord_complex_test(true);
	}
	void nextWord_complex_data(){ addComplexData(FILTER_NEXTWORD); }
	void nextWord_complex(){ 
		nextWord_complex_test(false);
	}
	void nextTextWord_complex_data(){ addComplexData(FILTER_NEXTTEXTWORD); }
	void nextTextWord_complex(){ 
		//get data
		QFETCH(QString, str);	
		QFETCH(QList<TestToken>, tokens);	
		
		int pos=0;
		LatexReader lr(str);
		while (lr.nextTextWord()) {
			const int& startIndex = lr.wordStartIndex;
			const QString& token = lr.word;
			if (pos>=tokens.size()) QFAIL(QString("Found additional token: %1 at %2").arg(token).arg(startIndex).toLatin1().constData());
			else {
				QVERIFY2(tokens[pos]==token, QString("Invalid token: %1 at %2, expected %3").arg(token).arg(startIndex).arg(tokens[pos]).toLatin1().constData());
				QVERIFY2(startIndex==tokens[pos].position, QString("Invalid startIndex: %2 for %1").arg(token).arg(startIndex).toLatin1().constData());
			}
			pos++;
		}
		
		if (pos < tokens.size())
			QVERIFY2 (pos==tokens.size(), QString("Didn't find all tokens, missing: %1").arg(tokens[qMin(pos, tokens.size()-1)]).toLatin1().constData());
	}
	
	
	void nextWord_simple_data(){
		QTest::addColumn<QString >("line");
		QTest::addColumn<int>("inIndex");
		QTest::addColumn<bool>("commands");
		QTest::addColumn<bool>("abbreviations");

		QTest::addColumn<int>("result");
		QTest::addColumn<int>("outIndex");
		QTest::addColumn<QString>("outWord");
		QTest::addColumn<int>("wordStartIndex");
		
		QTest::newRow("reference") << "bummerang\\ref{  xyz  }abcdef" << 9 << false << false
		                           << (int)LatexReader::NW_REFERENCE << 21 << "  xyz  " << 14;
		QTest::newRow("unknown")   << "bummerang\\adxas{  x:y:z  }abcdef" << 9 << false << false
		                           << (int)LatexReader::NW_TEXT << 32 << "abcdef" << 26;
		QTest::newRow("label")     << "bummerang\\label{  x:y:z  }abcdef" << 9 << false << false
		                           << (int)LatexReader::NW_LABEL << 25 << "  x:y:z  " << 16;
		QTest::newRow("citation0") << "012345\\cite{aaaHallob}abcdef" << 6 << false << false
		                           << (int)LatexReader::NW_CITATION << 21 << "aaaHallob" << 12;
		QTest::newRow("citation1") << "012345\\cite{   Hallo   }abcdef" << 6 << false << false
		                           << (int)LatexReader::NW_CITATION << 23 << "   Hallo   " << 12;
		QTest::newRow("citation2") << "012345\\cite{ Hallo:Welt! }abcdef" <<6  << false << false
					   << (int)LatexReader::NW_CITATION << 25 << " Hallo:Welt! " << 12;
		QTest::newRow("citation3") << "012345\\cite[aasasadaa]{Hallo:Welt!,miau!}abcdef" << 6 << false << false
		                           << (int)LatexReader::NW_CITATION << 40 << "Hallo:Welt!,miau!" << 23;
		//QTest::newRow("no abbre.") << "+++TEST.---" << 0 << false << false << (int)LatexReader::NW_TEXT << 7 << "TEST" << 3;
		QTest::newRow("abbrev.")   << "+++TEST.---" << 0 << false << true 
		                           << (int)LatexReader::NW_TEXT << 8 << "TEST." << 3;
		QTest::newRow("in cmd.")   << "\\section{text}" << 0 << false << false 
		                           << (int)LatexReader::NW_TEXT << 13 << "text" << 9;
		QTest::newRow("' chars")   << " can't " << 0 << false << false 
		                           << (int)LatexReader::NW_TEXT << 6 << "can't" << 1;
		QTest::newRow("' char2")   << " 'abc def' " << 0 << false << false 
		                           << (int)LatexReader::NW_TEXT << 5 << "abc" << 2;
		QTest::newRow("' char3")   << " 'abc def' " << 5 << false << false << (int)LatexReader::NW_TEXT << 9 << "def" << 6;
		QTest::newRow("sepchars")  << " ha\\-llo " << 0 << false << false << (int)LatexReader::NW_TEXT << 8 << "hallo" << 1;
		QTest::newRow("sepchar2")  << " la\"-tex " << 0 << false << false << (int)LatexReader::NW_TEXT << 8 << "latex" << 1;
		QTest::newRow("sepchar3a")  << "!ab\"\"xyz!" << 0 << false << false << (int)LatexReader::NW_PUNCTATION << 1 << "!" << 0;
		QTest::newRow("sepchar3b")  << "!ab\"\"xyz!" << 1 << false << false << (int)LatexReader::NW_TEXT << 8 << "abxyz" << 1;
		QTest::newRow("sepchar4a")  << "?oz\"|di?" << 0 << false << false << (int)LatexReader::NW_PUNCTATION << 1 << "?" << 0;
		QTest::newRow("sepchar4b")  << "?oz\"|di?" << 1 << false << false << (int)LatexReader::NW_TEXT << 7 << "ozdi" << 1;
		QTest::newRow("sepchar5")  << "?oz\"adi?" << 1 << false << false << (int)LatexReader::NW_TEXT << 7 << QString("oz%1di").arg(QChar(0xE4)) << 1;
		QTest::newRow("sepchar6")  << "?oz\\\"{a}di?" << 1 << false << false << (int)LatexReader::NW_TEXT << 10 << QString("oz%1di").arg(QChar(0xE4)) << 1;
		QTest::newRow("sepchar7")  << "?oz\\\"adi?" << 1 << false << false << (int)LatexReader::NW_TEXT << 8 << QString("oz%1di").arg(QChar(0xE4)) << 1;
		QTest::newRow("sepchar8")  << "?oz\"\"adi?" << 1 << false << false << (int)LatexReader::NW_TEXT << 8 << "ozadi" << 1;
		QTest::newRow("sepchar8")  << "?oz\"yxdi?" << 1 << false << false << (int)LatexReader::NW_TEXT << 8 << "ozyxdi" << 1;
		QTest::newRow("sepchar8")  << "?oz\"y?" << 1 << false << false << (int)LatexReader::NW_TEXT << 5 << "ozy" << 1;
		QTest::newRow("word end")  << "?no\"<di?" << 1 << false << false << (int)LatexReader::NW_TEXT << 3 << "no" << 1;
		QTest::newRow("word end")  << "?yi''di?" << 1 << false << false << (int)LatexReader::NW_TEXT << 3 << "yi" << 1;
		QTest::newRow("umlauts")  << "\"a\"o\"u\"A\"O\"U\\\"{a}\\\"{o}\\\"{u}\\\"{A}\\\"{O}\\\"{U}" << 0 << false << false << (int)LatexReader::NW_TEXT << 42 << (QString(QChar(0xE4))+QString(QChar(0xF6))+QString(QChar(0xFC))+QString(QChar(0xC4))+QString(QChar(0xD6))+QString(QChar(0xDC))+QString(QChar(0xE4))+QString(QChar(0xF6))+QString(QChar(0xFC))+QString(QChar(0xC4))+QString(QChar(0xD6))+QString(QChar(0xDC))) << 0; //unicode to be independent from c++ character encoding
	}
	void nextWord_simple(){
		QFETCH(QString, line);
		QFETCH(int, inIndex);
		QFETCH(bool, commands);

		QFETCH(int, result);
		QFETCH(int, outIndex);
		QFETCH(QString, outWord);
		QFETCH(int, wordStartIndex);
		
		LatexReader lr(line);
		lr.index = inIndex;
		int rs=(int)(lr.nextWord(commands));

		QEQUAL(lr.word,outWord);
		QEQUAL(rs, result);
		QEQUAL(lr.index,outIndex);
		QEQUAL(lr.wordStartIndex,wordStartIndex);
	}
	void cutComment_simple_data(){
		QTest::addColumn<QString >("in");
		QTest::addColumn<QString >("out");

		QTest::newRow("comment") << "ab%cd" << "ab";
		QTest::newRow("nocomment") << "ab\\%cd" << "ab\\%cd";
		QTest::newRow("comment and newline") << "ab\\\\%cd" << "ab\\\\";
		QTest::newRow("nocomment and newline") << "ab\\\\\\%cd" << "ab\\\\\\%cd";
		QTest::newRow("comment at start of line") << "%abcd" << "";
		QTest::newRow("comment at start+1 of line") << "a%abcd" << "a";

	}
	void cutComment_simple(){
		QFETCH(QString, in);
		QFETCH(QString, out);
		QString res=LatexParser::cutComment(in);
		QEQUAL(res,out);
	}
	void test_findContext_data(){
		QTest::addColumn<QString >("in");
		QTest::addColumn<int>("pos");
		QTest::addColumn<int>("out");

		QTest::newRow("command") << "\\begin{test}" << 3 << 1;
		QTest::newRow("content") << "\\begin{test}" << 8 << 2;
                QTest::newRow("option") << "\\begin[abc]{test}" << 8 << 3;
		QTest::newRow("content with option") << "\\begin[abc]{test}" << 13 << 2;
                QTest::newRow("content with option2") << "\\begin[\\abc]{test}" << 14 << 2;
		QTest::newRow("command with option") << "\\begin[abc]{test}" << 3 << 1;
		QTest::newRow("nothing") << "\\begin{test}" << 0 << 0;
	}
	void test_findContext(){
		QFETCH(QString, in);
		QFETCH(int, pos);
		QFETCH(int, out);
		int res=LatexParser::getInstance().findContext(in,pos);
		QEQUAL(res,out);
	}
	void test_findContext2_data(){
		QTest::addColumn<QString >("in");
		QTest::addColumn<int>("pos");
		QTest::addColumn<int>("out");
		QTest::addColumn<QString>("command");
		QTest::addColumn<QString>("value");

		QTest::newRow("command") << "\\begin{test}" << 3 << (int)LatexParser::Command << "\\begin" <<"test";
		QTest::newRow("content") << "\\begin{test}" << 8 << (int)LatexParser::Environment << "\\begin" <<"test";
		QTest::newRow("ref") << "\\ref{test}" << 8 << (int)LatexParser::Reference << "\\ref" <<"test";
		QTest::newRow("label") << "\\label{test}" << 8 << (int)LatexParser::Label << "\\label" <<"test";
		QTest::newRow("cite") << "\\cite{test}" << 8 << (int)LatexParser::Citation << "\\cite" <<"test";
		QTest::newRow("cite") << "\\cite{test}" << 3 << (int)LatexParser::Command << "\\cite" <<"test";
                QTest::newRow("abcd option") << "\\abcd{test}" << 7 << (int)LatexParser::Option << "\\abcd" <<"test";
                QTest::newRow("abcd option2") << "\\abcd[abc]{test}" << 12 << (int)LatexParser::Option << "\\abcd" <<"test";
                QTest::newRow("abcd option3") << "\\abcd[\\abc]{test}" << 12 << (int)LatexParser::Option << "\\abcd" <<"test";
	}
	void test_findContext2(){
		QFETCH(QString, in);
		QFETCH(int, pos);
		QFETCH(int, out);
		QFETCH(QString, command);
		QFETCH(QString, value);
		QString cmd;
		QString val;
		LatexParser::ContextType res=LatexParser::getInstance().findContext(in,pos,cmd,val);
		QEQUAL((int)res,out);
		QEQUAL(cmd,command);
		QEQUAL(val,value);
	}
	void test_findClosingBracket_data(){
		QTest::addColumn<QString>("line");
		QTest::addColumn<int>("start");
		QTest::addColumn<QChar>("oc");
		QTest::addColumn<QChar>("cc");
		QTest::addColumn<int>("out");

		QTest::newRow("simple bracket") << "{ simple} text" << 0 << QChar('{') << QChar('}') << 8;
		QTest::newRow("simple bracket2") << "  simple} text" << 0 << QChar('{') << QChar('}') << 8;
		QTest::newRow("nested bracket") << "{a {nested} simple} text" << 0 << QChar('{') << QChar('}') << 18;
		QTest::newRow("nested bracket2") << "{a {nested} simple} text" << 3 << QChar('{') << QChar('}') << 10;
		QTest::newRow("nested bracket3") << "{a {nested} simple} text" << 16 << QChar('{') << QChar('}') << 18;
		QTest::newRow("nested bracket3") << "{a {nested} simple} text" << 19 << QChar('{') << QChar('}') << -1;
		QTest::newRow("no bracket") << "No bracket in here" << 0 << QChar('{') << QChar('}') << -1;
		QTest::newRow("no bracket2") << "No {proper} bracket in here" << 0 << QChar('{') << QChar('}') << -1;
	}
	void test_findClosingBracket(){
		QFETCH(QString, line);
		QFETCH(int, start);
		QFETCH(QChar, oc);
		QFETCH(QChar, cc);
		QFETCH(int, out);
		int ret = findClosingBracket(line, start, oc, cc);
		QEQUAL(ret, out);
	}
	void test_getCommand_data(){
		QTest::addColumn<QString>("line");
		QTest::addColumn<int>("pos");
		QTest::addColumn<QString>("outCmd");
		QTest::addColumn<int>("out");

		QTest::newRow("before") << "test \\section{sec}" << 0 << QString() << 0;
		QTest::newRow("before 2") << "test \\section{sec}" << 3 << QString() << 3;
		QTest::newRow("before 3") << "test \\section{sec}" << 4 << QString() << 4;
		QTest::newRow("at cmd start") << "\\section{sec}" << 0 << "\\section" << 8;
		QTest::newRow("at cmd start 2") << "test \\section{sec}" << 5 << "\\section" << 13;
		QTest::newRow("at cmd end") << "\\section{sec} end" << 7 << "\\section" << 8;
		QTest::newRow("at cmd end2") << "\\section nothing" << 7 << "\\section" << 8;
		QTest::newRow("at cmd end3") << "\\section" << 7 << "\\section" << 8;
		QTest::newRow("on brace") << "\\section{sec}" << 8 << QString() << 8;
		QTest::newRow("on optional brace") << "\\section[opt]{sec}" << 8 << QString() << 8;
		QTest::newRow("in argument") << "\\section{sec}" << 10 << QString() << 10;
	}
	void test_getCommand(){
		QFETCH(QString, line);
		QFETCH(int, pos);
		QFETCH(QString, outCmd);
		QFETCH(int, out);
		QString retCmd;
		int ret = getCommand(line, retCmd, pos);
		QEQUAL(ret, out);
		QEQUAL(retCmd, outCmd);
	}
	void test_getSimplifiedSVNVersion_data(){
		QTest::addColumn<QString>("versionString");
		QTest::addColumn<int>("out");

		QTest::newRow("plain") << "1314" << 1314;
		QTest::newRow("modified") << "1314M" << 1314;
		QTest::newRow("mixed") << "1314:1315" << 1314;
		QTest::newRow("unknown") << "Unknown" << 0;
	}
	void test_getSimplifiedSVNVersion(){
		QFETCH(QString, versionString);
		QFETCH(int, out);
		QEQUAL(getSimplifiedSVNVersion(versionString), out);
	}
	void test_minimalJsonParse_data(){
		QTest::addColumn<QString>("jsonData");
		QTest::addColumn<bool>("retVal");
		QTest::addColumn<QStringList>("keys");
		QTest::addColumn<QStringList>("vals");

		QTest::newRow("empty") << "" << true << QStringList() << QStringList();
		QTest::newRow("empty") << "{}" << true << QStringList() << QStringList();
		QTest::newRow("single") << " { \"key\"  : \"val\" } " << true << (QStringList() << "key") << (QStringList() << "val");
		QTest::newRow("two") << "{\"key\":\"val\",\"key2\":\"val2\"} " << true << (QStringList() << "key" << "key2") << (QStringList() << "val" << "val2");
		QTest::newRow("escapedQoute") << "{\"key\":\"val\\\"more\"}" << true << (QStringList() << "key") << (QStringList() << "val\"more");
		QTest::newRow("missingClose") << "{\"key\":\"val\"" << false << (QStringList() << "key") << (QStringList() << "val");
		QTest::newRow("missingQuote1") << "{key:\"val\"}" << false << QStringList() << QStringList();
		QTest::newRow("missingQuote2") << "{\"key:\"val\"}" << false << QStringList() << QStringList();
		QTest::newRow("missingQuote3") << "{key\":\"val\"}" << false << QStringList() << QStringList();
	}
	void test_minimalJsonParse(){
		QFETCH(QString, jsonData);
		QFETCH(bool, retVal);
		QFETCH(QStringList, keys);
		QFETCH(QStringList, vals);

		QHash<QString, QString> data;
		QEQUAL(minimalJsonParse(jsonData, data), retVal);
		for (int i=0; i<keys.count(); i++) {
			QEQUAL2(data[keys[i]], vals[i], QString("for key: %1").arg(keys[i]));
		}
	}
};


#endif
#endif
