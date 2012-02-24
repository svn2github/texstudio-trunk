
#ifndef QT_NO_DEBUG
#include "mostQtHeaders.h"
#include "tablemanipulation_t.h"
#include "tablemanipulation.h"
#include "qdocumentcursor.h"
#include "qdocument.h"
#include "qeditor.h"
#include "testutil.h"
#include <QtTest/QtTest>
TableManipulationTest::TableManipulationTest(QEditor* editor): ed(editor){}

void TableManipulationTest::addCol_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<QString>("newText");
	
	//-------------cursor without selection--------------
	QTest::newRow("add first col")
		<< "\\begin{tabular}{xy}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "\\begin{tabular}{lxy}\n &a&b\\\\\n &c&d\\\\\n &e&f\\\\\n\\end{tabular}\n";

	QTest::newRow("add second col")
		<< "\\begin{tabular}{xy}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 1
		<< "\\begin{tabular}{xly}\na& &b\\\\\nc& &d\\\\\ne& &f\\\\\n\\end{tabular}\n";

	QTest::newRow("add third col")
		<< "\\begin{tabular}{xy}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 2
		<< "\\begin{tabular}{xyl}\na&b& \\\\\nc&d& \\\\\ne&f& \\\\\n\\end{tabular}\n";

	QTest::newRow("add 4th col")
		<< "\\begin{tabular}{xy}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 3
		<< "\\begin{tabular}{xyl}\na&b& \\\\\nc&d& \\\\\ne&f& \\\\\n\\end{tabular}\n";


}
void TableManipulationTest::addCol(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(QString, newText);
	
	ed->setText(text, false);
	LatexTables::addColumn(ed->document(),row,col,0);

	QEQUAL(ed->document()->text(), newText);
	
}

void TableManipulationTest::addRow_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<QString>("newText");

	//-------------cursor without selection--------------
	QTest::newRow("add row")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "\\begin{tabular}{ll}\na&b\\\\\n & \\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("add row, cursor at end of line")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 5
		<< "\\begin{tabular}{ll}\na&b\\\\\n & \\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("add row")
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "\\begin{tabular}{ll}\na&b\\\\\n & \\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("add row")
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 6
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\n & \\\\\ne&f\\\\\n\\end{tabular}\n";

}
void TableManipulationTest::addRow(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(QString, newText);

	ed->setText(text, false);
	ed->setCursorPosition(row,col);
	QDocumentCursor c(ed->cursor());
	LatexTables::addRow(c,2);

	QEQUAL(ed->document()->text(), newText);

}

void TableManipulationTest::remCol_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<QString>("newText");

	//-------------cursor without selection--------------
	QTest::newRow("rem col 0")
		<< "\\begin{tabular}{xy}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "\\begin{tabular}{y}\nb\\\\\nd\\\\\nf\\\\\n\\end{tabular}\n";

	QTest::newRow("rem col 1")
		<< "\\begin{tabular}{xy}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 1
		<< "\\begin{tabular}{x}\na\\\\\nc\\\\\ne\\\\\n\\end{tabular}\n";

	QTest::newRow("rem col 0, multicolumn")
		<< "\\begin{tabular}{ll}\na&b\\\\\n\\multicolumn{2}{c}\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "\\begin{tabular}{l}\nb\\\\\n\\multicolumn{1}{c}\\\\\nf\\\\\n\\end{tabular}\n";

	QTest::newRow("rem col 1, multicolumn")
		<< "\\begin{tabular}{ll}\na&b\\\\\n\\multicolumn{2}{c}\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 1
		<< "\\begin{tabular}{l}\na\\\\\n\\multicolumn{1}{c}\\\\\ne\\\\\n\\end{tabular}\n";

	QTest::newRow("rem col 1, multicolumn plus col")
		<< "\\begin{tabular}{ll}\na&b\\\\\n\\multicolumn{2}{c}&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 1
		<< "\\begin{tabular}{l}\na\\\\\n\\multicolumn{1}{c}&d\\\\\ne\\\\\n\\end{tabular}\n";

	QTest::newRow("rem col 2, multicolumn")
		<< "\\begin{tabular}{ll}\na&b\\\\\n\\multicolumn{2}{c}&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 2
		<< "\\begin{tabular}{ll}\na&b\\\\\n\\multicolumn{2}{c}\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("rem col 0, row over multiple lines")
		<< "\\begin{tabular}{ll}\na&\nb\\\\\nc\n&\nd\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "\\begin{tabular}{l}\n\nb\\\\\n\nd\\\\\nf\\\\\n\\end{tabular}\n";

	QTest::newRow("rem col 1, row over multiple lines")
		<< "\\begin{tabular}{ll}\na&\nb\\\\\nc\n&\nd\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 1
		<< "\\begin{tabular}{l}\na\\\\\nc\n\\\\\ne\\\\\n\\end{tabular}\n";

        QTest::newRow("rem col 0 containing \\hline")
                        << "\\begin{tabular}{ll}\na&b\\\\ \\hline\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
                        << 1 << 0
			<< "\\begin{tabular}{l}\nb\\\\ \\hline\nd\\\\\nf\\\\\n\\end{tabular}\n";

	QTest::newRow("rem col 0 containing \\hline")
			<< "\\begin{tabular}{ll}\na&b\\\\ \\hline\nc&d\\\\\ne&f\\\\ \\hline\n\\end{tabular}\n"
			<< 1 << 0
			<< "\\begin{tabular}{l}\nb\\\\ \\hline\nd\\\\\nf\\\\ \\hline\n\\end{tabular}\n";

	QTest::newRow("rem col 0 containing \\hline")
			<< "\\begin{tabular}{ll}\na&b\\\\ \\hline\nc&d\\\\\ne&f\\\\\\hline\n\\end{tabular}\n"
			<< 1 << 0
			<< "\\begin{tabular}{l}\nb\\\\ \\hline\nd\\\\\nf\\\\ \\hline\n\\end{tabular}\n";

	QTest::newRow("rem last col")
			<< "\\begin{tabular}{l}\na\\\\ \\hline\nd\\\\\nf\\\\\\hline\n\\end{tabular}\n"
			<< 1 << 0
			<< "\\begin{tabular}{}\n\\\\ \\hline\n\\\\\n\\\\ \\hline\n\\end{tabular}\n";

}
void TableManipulationTest::remCol(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(QString, newText);

	ed->setText(text, false);
	LatexTables::removeColumn(ed->document(),row,col);
	
	QEQUAL(ed->document()->text(),newText);
	
}

void TableManipulationTest::remRow_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<QString>("newText");

	//-------------cursor without selection--------------
	QTest::newRow("rem row")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "\\begin{tabular}{ll}\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("rem row, cursor at end of line")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 5
		<< "\\begin{tabular}{ll}\na&b\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("rem row, second row")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 0
		<< "\\begin{tabular}{ll}\na&b\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("rem row, third row")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 3 << 0
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\n\\end{tabular}\n";

	QTest::newRow("rem row, multi rows in one line 1")
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "\\begin{tabular}{ll}\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("rem row, multi rows in one line 2")
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 6
		<< "\\begin{tabular}{ll}\na&b\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("rem row, multi rows in one line 3")
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 6
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\n\\end{tabular}\n";

	QTest::newRow("rem row, multi rows in one line 4")
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 3 << 6
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n";


}
void TableManipulationTest::remRow(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(QString, newText);

	ed->setText(text, false);
	ed->setCursorPosition(row,col);
	QDocumentCursor c(ed->cursor());
	LatexTables::removeRow(c);

	QEQUAL(ed->document()->text(), newText);

}

void TableManipulationTest::getCol_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<int>("colFound");

	//-------------cursor without selection--------------
	QTest::newRow("col 0")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< 0;

	QTest::newRow("col 0")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 1
		<< 0;

	QTest::newRow("col 1")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 2
		<< 1;

	QTest::newRow("col 1")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 3
		<< 1;

	QTest::newRow("col 1")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 4
		<< 1;

	QTest::newRow("col -1")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 5
		<< -1;

	QTest::newRow("row 2,col 1")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 0
		<< 0;

	QTest::newRow("row 3,col 1")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 3 << 0
		<< 0;

	QTest::newRow("row 2,col 2")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 3 << 2
		<< 1;

	QTest::newRow("row 2,col -1")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 3 << 5
		<< -1;

	QTest::newRow("row 1,col 1,multi row per line")
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< 0;

	QTest::newRow("row 1,col 1,multi row per line")
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 5
		<< 0;

	QTest::newRow("row 1,col 2,multi row per line")
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 7
		<< 1;

	QTest::newRow("no row")
		<< "\\begin{tabular}{ll}\na\n\\end{tabular}\n"
		<< 1 << 1
		<< 0;

	QTest::newRow("no row, col 1")
		<< "\\begin{tabular}{ll}\na&b\n\\end{tabular}\n"
		<< 1 << 2
		<< 1;

	QTest::newRow("no row, col 1")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\n\\end{tabular}\n"
		<< 2 << 2
		<< 1;

}
void TableManipulationTest::getCol(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(int, colFound);

	ed->setText(text, false);
	ed->setCursorPosition(row,col);
	QDocumentCursor c(ed->cursor());
	int nc=LatexTables::getColumn(c);

	QEQUAL(nc,colFound);

}

void TableManipulationTest::getNumberOfCol_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<int>("colFound");

	//-------------cursor without selection--------------
	QTest::newRow("cols 2")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< 2;

	QTest::newRow("cols 4")
		<< "\\begin{tabular}{|l|l|cc}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 0
		<< 4;

	QTest::newRow("cols 0")
		<< "\\begin{tabular}{}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< 0;

	QTest::newRow("spaced in definition")
		<< "\\begin{tabular}{l l c}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< 3;

	QTest::newRow("p")
		<< "\\begin{tabular}{llp{3cm}}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< 3;

	QTest::newRow("m")
		<< "\\begin{tabular}{llm{3cm}}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< 3;

	QTest::newRow("col commands (array)")
		<< "\\begin{tabular}{>{\\bfseries}ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< 2;

	QTest::newRow("no Table")
		<< "test\na&b\\\\\nc&d\\\\\ne&f\\\\\ntest\n"
		<< 1 << 0
		<< -1;

	QTest::newRow("separators")
		<< "\\begin{tabular}{|l|l|@{ll}cc}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 0
		<< 4;

	QTest::newRow("multipliers")
		<< "\\begin{tabular}{|l|l|@{ll}c*{2}{lc}}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 0
		<< 7;

	QTest::newRow("multipliers, nested")
		<< "\\begin{tabular}{|l|l|@{ll}c*{2}{*{2}{l}}}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 0
		<< 7;

}
void TableManipulationTest::getNumberOfCol(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(int, colFound);

	ed->setText(text, false);
	ed->setCursorPosition(row,col);
	QDocumentCursor c(ed->cursor());
	int nc=LatexTables::getNumberOfColumns(c);

	QEQUAL(nc,colFound);

}

void TableManipulationTest::findNextToken_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<int>("result");
	QTest::addColumn<int>("newRow");
	QTest::addColumn<int>("newCol");

	QTest::newRow("find &")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< 2 << 1 << 2;

	QTest::newRow("find \\")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 2
		<< 0 << 1 << 5;

	QTest::newRow("find \\, multi line")
		<< "\\begin{tabular}{ll}\na&\nb\n\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 2
		<< 0 << 3 << 2;

	QTest::newRow("run into \\end")
		<< "\\begin{tabular}{ll}\na\ncd\n\\end{tabular}\n"
		<< 1 << 2
		<< -2 << 3 << 0;

	QTest::newRow("run into eof")
		<< "\\begin{tabular}{ll}\na\ncd\nnd{tabular}\n"
		<< 1 << 2
		<< -1 << 4 << 0;

}
void TableManipulationTest::findNextToken(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(int, result);
	QFETCH(int, newRow);
	QFETCH(int, newCol);


	ed->setText(text, false);
	ed->setCursorPosition(row,col);
	QDocumentCursor c(ed->cursor());
	QStringList tokens;
	tokens<<"\\\\"<<"\\&"<<"&";
	int res=LatexTables::findNextToken(c,tokens);

	QEQUAL(res,result);
	QEQUAL(c.lineNumber(),newRow);
	QEQUAL(c.columnNumber(),newCol);

}

void TableManipulationTest::findNextTokenBackwards_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<int>("result");
	QTest::addColumn<int>("newRow");
	QTest::addColumn<int>("newCol");

	QTest::newRow("find &")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 3
		<< 2 << 1 << 1;

	QTest::newRow("find \\")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 1
		<< 0 << 1 << 3;

	QTest::newRow("find &, multi line")
		<< "\\begin{tabular}{ll}\na&\nb\n\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 3 << 0
		<< 2 << 1 << 1;

	QTest::newRow("run into \\begin")
		<< "\\begin{tabular}{ll}\na\ncd\n\\end{tabular}\n"
		<< 1 << 1
		<< -2 << 0 << 19;

	QTest::newRow("run into eof")
		<< "egin{tabular}{ll}\na\ncd\nnd{tabular}\n"
		<< 1 << 1
		<< -1 << 0 << 17;

}
void TableManipulationTest::findNextTokenBackwards(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(int, result);
	QFETCH(int, newRow);
	QFETCH(int, newCol);


	ed->setText(text, false);
	ed->setCursorPosition(row,col);
	QDocumentCursor c(ed->cursor());
	QStringList tokens;
	tokens<<"\\\\"<<"\\&"<<"&";
	int res=LatexTables::findNextToken(c,tokens,false,true);

	QEQUAL(res,result);
	QEQUAL(c.lineNumber(),newRow);
	QEQUAL(c.columnNumber(),newCol);

}

void TableManipulationTest::addHLine_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<int>("numberOfLines");
	QTest::addColumn<bool>("remove");
	QTest::addColumn<QString>("newText");

	//-------------cursor without selection--------------
	QTest::newRow("add to all")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0 << -1 << false
		<< "\\begin{tabular}{ll}\na&b\\\\ \\hline\nc&d\\\\ \\hline\ne&f\\\\ \\hline\n\\end{tabular}\n";

	QTest::newRow("add to 2 (in single line)")
		<< "\\begin{tabular}{ll}\na&b\\\\c&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0 << 2 << false
		<< "\\begin{tabular}{ll}\na&b\\\\ \\hline\nc&d\\\\ \\hline\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("remove all, none present")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0 << -1 << true
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("remove all")
		<< "\\begin{tabular}{ll}\na&b\\\\ \\hline\nc&d\\\\ \\hline\ne&f\\\\ \\hline\n\\end{tabular}\n"
		<< 1 << 0 << -1 << true
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("remove all, missing hlines")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\ \\hline\ne&f\\\\ \\hline\n\\end{tabular}\n"
		<< 1 << 0 << -1 << true
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n";

	QTest::newRow("remove some")
		<< "\\begin{tabular}{ll}\na&b\\\\ \\hline\nc&d\\\\\\hline\ne&f\\\\ \\hline\n\\end{tabular}\n"
		<< 1 << 0 << 2 << true
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\ \\hline\n\\end{tabular}\n";

}
void TableManipulationTest::addHLine(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(int, numberOfLines);
	QFETCH(bool, remove);
	QFETCH(QString, newText);

	ed->setText(text, false);
	ed->setCursorPosition(row,col);
	QDocumentCursor c(ed->cursor());
	LatexTables::addHLine(c,numberOfLines,remove);

	QEQUAL(ed->document()->text(), newText);

}

void TableManipulationTest::splitCol_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("colFound");

	//-------------cursor without selection--------------
	QTest::newRow("cols 2")
		<< "ll"
		<< 2;

	QTest::newRow("cols 4")
		<< "|l|l|cc"
		<< 4;

	QTest::newRow("cols 0")
		<< ""
		<< 0;

	QTest::newRow("spaced in definition")
		<< "l l c"
		<< 3;

	QTest::newRow("p")
		<< "llp{3cm}"
		<< 3;

	QTest::newRow("p")
		<< "llm{3cm}"
		<< 3;

	QTest::newRow("col commands (array)")
		<< ">{\\bfseries}ll"
		<< 2;

	QTest::newRow("separators")
		<< "|l|l|@{ll}cc"
		<< 4;

	QTest::newRow("multipliers")
		<< "|l|l|@{ll}c*{2}{lc}"
		<< 7;
}
void TableManipulationTest::splitCol(){
	QFETCH(QString, text);
	QFETCH(int, colFound);


	QStringList res=LatexTables::splitColDef(text);

	QEQUAL(res.count(),colFound);

}

void TableManipulationTest::getDef_data(){
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("row");
	QTest::addColumn<int>("col");
	QTest::addColumn<QString>("def_soll");

	//-------------cursor without selection--------------
	QTest::newRow("cols 2")
		<< "\\begin{tabular}{ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "ll";

	QTest::newRow("cols 4")
		<< "\\begin{tabular}{|l|l|cc}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 0
		<< "|l|l|cc";

	QTest::newRow("cols 0")
		<< "\\begin{tabular}{}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "";

	QTest::newRow("spaced in definition")
		<< "\\begin{tabular}{l l c}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "l l c";

	QTest::newRow("p")
		<< "\\begin{tabular}{llp{3cm}}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< "llp{3cm}";

	QTest::newRow("tabularx")
		<< "\\begin{tabularx}{\\linewidth}{llm{3cm}}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabularx}\n"
		<< 1 << 0
		<< "llm{3cm}";

	QTest::newRow("col commands (array)")
		<< "\\begin{tabular}{>{\\bfseries}ll}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 1 << 0
		<< ">{\\bfseries}ll";

	QTest::newRow("no Table")
		<< "test\na&b\\\\\nc&d\\\\\ne&f\\\\\ntest\n"
		<< 1 << 0
		<< "";

	QTest::newRow("separators")
		<< "\\begin{tabular}{|l|l|@{ll}cc}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 0
		<< "|l|l|@{ll}cc";

	QTest::newRow("multipliers")
		<< "\\begin{tabular}{|l|l|@{ll}c*{2}{lc}}\na&b\\\\\nc&d\\\\\ne&f\\\\\n\\end{tabular}\n"
		<< 2 << 0
		<< "|l|l|@{ll}c*{2}{lc}";

}
void TableManipulationTest::getDef(){
	QFETCH(QString, text);
	QFETCH(int, row);
	QFETCH(int, col);
	QFETCH(QString, def_soll);

	ed->setText(text, false);
	ed->setCursorPosition(row,col);
	QDocumentCursor c(ed->cursor());
	QString def=LatexTables::getDef(c);
	QString sel_def=c.selectedText();

	QEQUAL(def,def_soll);
	QEQUAL(sel_def,def_soll);

}

#endif

