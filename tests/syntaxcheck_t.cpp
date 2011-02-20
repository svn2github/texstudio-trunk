
#ifndef QT_NO_DEBUG
#include "mostQtHeaders.h"
#include "syntaxcheck_t.h"
#include "tablemanipulation.h"
#include "qdocumentcursor.h"
#include "qdocument.h"
#include "qeditor.h"
#include "latexdocument.h"
//#include "syntaxcheck.h"
#include "testutil.h"
#include <QtTest/QtTest>
SyntaxCheckTest::SyntaxCheckTest(LatexEditorView* ed): edView(ed){}

void SyntaxCheckTest::checktabular_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<QString>("expectedMessage");
	
	//-------------cursor without selection--------------
	QTest::newRow("all okay")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "no error";

	QTest::newRow("too little cols")
		<< "\\begin{tabular}{ll}\na\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 1
		<< "cols in tabular missing";

	QTest::newRow("too many cols")
		<< "\\begin{tabular}{ll}\na&b&b2\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 6
		<< "more cols in tabular than specified";

	QTest::newRow("too many cols")
		<< "\\begin{tabular}{ll}\na&b&b2\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 6
		<< "more cols in tabular than specified";

	QTest::newRow("no error second col")
		<< "\\begin{tabular}{ll}\na&b&b2\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 11
		<< "no error";

	QTest::newRow("no error second col")
		<< "\\begin{tabular}{ll}\na&b&b2\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 13
		<< "no error";

	QTest::newRow("too many cols second col")
		<< "\\begin{tabular}{lll}\na&b&b2\\\\c&d&e&f\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 15
		<< "more cols in tabular than specified";

	QTest::newRow("too many cols ")
		<< "\\begin{tabular}{lll}\n&&&\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 3
		<< "more cols in tabular than specified";

	QTest::newRow("no error 3 cols")
		<< "\\begin{tabular}{lll}\na&b&b2\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 1 << 6
		<< "no error";

	QTest::newRow("no error 3 cols")
		<< "\\begin{tabular}{lll}\na&b&b2\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 1 << 8
		<< "no error";

	QTest::newRow("no error 3 cols, multicolumn")
		<< "\\begin{tabular}{lll}\n\\multicolumn{2}{c}{Hallo}&b2\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 1 << 29
		<< "no error";

	QTest::newRow("no error 3 cols, multicolumn")
		<< "\\begin{tabular}{lll}\n\\multicolumn{2}{c}{Hallo}&b2\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 1 << 27
		<< "no error";

	QTest::newRow("too many cols, 3 cols, multicolumn")
		<< "\\begin{tabular}{lll}\n\\multicolumn{2}{c}{Hallo}&b2&b3\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 1 << 30
		<< "more cols in tabular than specified";

	QTest::newRow("too little cols, 3 cols, multicolumn")
		<< "\\begin{tabular}{lll}\n\\multicolumn{2}{c}{Hallo}\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 1 << 26
		<< "cols in tabular missing";

	QTest::newRow("no error 3 cols")
		<< "\\begin{tabular}{lll}\n&&\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 1 << 3
		<< "no error";

	QTest::newRow("no error 3 cols")
		<< "\\begin{tabular}{lll}\n&&\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 1 << 2
		<< "no error";

	QTest::newRow("no error 3 cols,multiple lines")
		<< "\\begin{tabular}{lll}\na&\n&\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 2 << 1
		<< "no error";

	QTest::newRow("no error 3 cols,multiple lines")
		<< "\\begin{tabular}{lll}\na&\n&\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 2 << 2
		<< "no error";

	QTest::newRow("too little cols, 3 cols,multiple lines")
		<< "\\begin{tabular}{lll}\na&\n\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 2 << 1
		<< "cols in tabular missing";

	QTest::newRow("too many cols, 3 cols,multiple lines")
		<< "\\begin{tabular}{lll}\na&\n&&a\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 2 << 3
		<< "more cols in tabular than specified";

	QTest::newRow("too many cols, 3 cols,multiple lines")
		<< "\\begin{tabular}{lll}\na&\n&&\na\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 2 << 2
		<< "more cols in tabular than specified";

	QTest::newRow("too many cols, 3 cols,multiple lines")
		<< "\\begin{tabular}{lll}\na&&\\multicolumn{2}{c}{test}\na\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 1 << 4
		<< "more cols in tabular than specified";

	QTest::newRow("too many cols, 3 cols,multiple lines")
		<< "\\begin{tabular}{lll}\na\\multicolumn{4}{c}{test}\na\\\\c&d&e&f\\\\\n\\end{tabular}\n"
		<< 1 << 4
		<< "more cols in tabular than specified";

	QTest::newRow("hline")
		<< "\\begin{tabular}{ll}\na&b\\\\\\hline\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 4
		<< "no error";
}
void SyntaxCheckTest::checktabular(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(QString, expectedMessage);
	
	expectedMessage = QApplication::translate("SyntaxCheck", qPrintable(expectedMessage));

	edView->editor->setText(text);
	edView->SynChecker.waitForQueueProcess(); // wait for syntax checker to finish (as it runs in a parallel thread)
	int cols;
	SyntaxCheck::Environment env;
	edView->getEnv(row,env,cols);
	QString message=edView->SynChecker.getErrorAt(edView->document->line(row).handle(),col,env,cols);
	QEQUAL(message, expectedMessage);
	
}

#endif

