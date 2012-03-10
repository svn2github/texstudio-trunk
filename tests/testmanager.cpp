#ifndef QT_NO_DEBUG
#include "testmanager.h"
//hel 
#include "smallUsefulFunctions_t.h"
#include "buildmanager_t.h"
#include "codesnippet_t.h"
#include "qdocumentcursor_t.h"
#include "qdocumentline_t.h"
#include "qdocumentsearch_t.h"
#include "qsearchreplacepanel_t.h"
#include "qeditor_t.h"
#include "latexcompleter_t.h"
#include "latexeditorview_t.h"
#include "latexeditorview_bm.h"
#include "scriptengine_t.h"
#include "structureview_t.h"
#include "tablemanipulation_t.h"
#include "syntaxcheck_t.h"
#include <QtTest/QtTest>

const QRegExp TestToken::simpleTextRegExp ("[A-Z'a-z0-9]+.?");
const QRegExp TestToken::commandRegExp ("\\\\([A-Za-z]+|.)");
const QRegExp TestToken::ignoredTextRegExp ("[$\t *!{}.\\][]+");
const QRegExp TestToken::specialCharTextRegExp ("[A-Z'\"\\\\\\{\\}a-z0-9�]+");
const QRegExp TestToken::punctationRegExp("[!():\"?,.;-]");

int totalTestTime;
char* tempResult;

QString TestManager::performTest(QObject* obj){
	char* argv[3];
	argv[0]=(char*)"texstudio";
	argv[1]=(char*)"-o";
	argv[2]=tempResult;
	QTime timing; timing.start();
	QTest::qExec(obj,3,argv);
	delete obj;
	int time = timing.elapsed();
	totalTestTime += time;
	QString testTime = QString("Time: %1 ms\n\n" ).arg(time);
	QFile f(QFile::decodeName(tempResult));
	if (!f.open(QIODevice::ReadOnly)) 
		return "\n\n!!!!!!!!!!! Couldn't find test result !!!!!!!!!!!! \n\n";
	return f.readAll()+testTime;
}

QString TestManager::execute(TestLevel level, LatexEditorView* edView, QCodeEdit* codeedit, QEditor* editor){
	QTemporaryFile tf;
	tf.setAutoRemove(false);
	tf.open();
	QByteArray tfn = QFile::encodeName(tf.fileName());
	tf.close();
	tempResult = tfn.data();
	
	
	//codeedit, editor are passed as extra parameters and not extracted from edView, so we don't have
	//to include latexeditorview.h here
	totalTestTime = 0;
	QString tr;
	QList<QObject*> tests=QList<QObject*>()
        << new SmallUsefulFunctionsTest()
		<< new BuildManagerTest()
		<< new CodeSnippetTest(editor)
		<< new QDocumentLineTest()
		<< new QDocumentCursorTest()
		<< new QDocumentSearchTest(editor,level==TL_ALL)
		<< new QSearchReplacePanelTest(codeedit,level==TL_ALL)
		<< new QEditorTest(editor,level==TL_ALL)
		<< new LatexEditorViewTest(edView)
		<< new LatexCompleterTest(edView)
		<< new ScriptEngineTest(editor,level==TL_ALL)
		<< new LatexEditorViewBenchmark(edView,level==TL_ALL)
		<< new StructureViewTest(editor,edView->document,level==TL_ALL)
		<< new TableManipulationTest(editor)
		<< new SyntaxCheckTest(edView);
	bool allPassed=true;
	if (level!=TL_ALL)
		tr="There are skipped tests. Please rerun with --execute-all-tests\n\n";
	for (int i=0; i <tests.size();i++){
		QString res=performTest(tests[i]);
		tr+=res;
		if (!res.contains(", 0 failed, 0 skipped")) allPassed=false;
	}	
	
	tr+=QString("\nTotal testing time: %1 ms\n").arg(totalTestTime);
	
	if (!allPassed) 
		tr="THERE SEEMS TO BE FAILED TESTS! \n\n\n\n\n\n\n"+tr;
	
	QFile(QFile::decodeName(tempResult)).remove();
	
	return tr;
}
#endif
