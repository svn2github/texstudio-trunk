
#ifndef QT_NO_DEBUG
#include "mostQtHeaders.h"
#include "latexeditorview_bm.h"
#include "latexeditorview.h"
#include "qdocumentcursor.h"
#include "qdocument.h"
#include "qdocumentline.h"
#include "qdocumentline_p.h"
#include "latexdocument.h"
#include "qeditor.h"
#include "testutil.h"
#include <QtTest/QtTest>
LatexEditorViewBenchmark::LatexEditorViewBenchmark(LatexEditorView* view): edView(view){}

void LatexEditorViewBenchmark::documentChange_data(){
#if QT_VERSION >= 0x040500	
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("start");
	QTest::addColumn<int>("count");
	
	//-------------cursor without selection--------------
	QTest::newRow("one line update")
		<< "abcdefg\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("multi line update")
		<< "abcdefga\nabcdefg\nabcdefg\nxyz\nc"
		<< 0 << 3;
	QTest::newRow("long line update")
		<< "abcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefga\nabcdefg\nabcdefg\nxyz\nc"
		<< 0 << 3;
	QTest::newRow("labels")
			<< "\\label{ab} \\label{cd}\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("refs")
			<< "\\ref{ab} \\ref{cd}\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("no spellcheck in command")
			<< "\\usepackage{abcdsdfsdfds} \\usepackage{abcdsdfsdfds}\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("spellcheck in command")
			<< "\\textbf{abcdsdfsdfds} \\textbf{abcdsdfsdfds}\nhallo welt\nabcdefg"
		<< 0 << 1;
#endif
}
void LatexEditorViewBenchmark::documentChange(){
#if QT_VERSION >= 0x040500	
	QFETCH(QString, text);
	QFETCH(int, start);
	QFETCH(int, count);
	
	edView->editor->document()->setText(text);
	QBENCHMARK {
		edView->documentContentChanged(start,count);
	}
#endif
}

void LatexEditorViewBenchmark::linePaint_data(){
#if QT_VERSION >= 0x040500
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("start");
	QTest::addColumn<int>("count");

	//-------------cursor without selection--------------
	QTest::newRow("one line update")
		<< "abcdefg\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("correct german")
		<< "Haus\nAuto\nKind\nxyz\nc"
		<< 0 << 3;
	QTest::newRow("long line update")
		<< "abcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefga\nabcdefg\nabcdefg\nxyz\nc"
		<< 0 << 3;
	QTest::newRow("labels")
			<< "\\label{ab} \\label{cd}\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("refs")
			<< "\\ref{ab} \\ref{cd}\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("no spellcheck in command")
			<< "\\usepackage{abcdsdfsdfds} \\usepackage{abcdsdfsdfds}\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("spellcheck in command")
			<< "\\textbf{abcdsdfsdfds} \\textbf{abcdsdfsdfds}\nhallo welt\nabcdefg"
		<< 0 << 1;
#endif
}
void LatexEditorViewBenchmark::linePaint(){
#if QT_VERSION >= 0x040500
	QFETCH(QString, text);
	QFETCH(int, start);
	QFETCH(int, count);
        Q_UNUSED(start);
        Q_UNUSED(count);

	edView->editor->document()->setText(text);
	LatexDocument *doc=edView->document;
	QDocument::PaintContext cxt;
	cxt.xoffset = 0;
	cxt.yoffset = 0;
	cxt.width = 1000;
	cxt.height = 700;
	cxt.palette = QApplication::palette();
	cxt.fillCursorRect = false;
	cxt.blinkingCursor = false;

	QVector<int> m_cursorLines(0), m_selectionBoundaries(0);

	QBrush bg = cxt.palette.base();

	QDocumentLineHandle *dlh=doc->line(0).handle();
	bool fullSel=false;
	QPixmap *px=new QPixmap(cxt.width,12);
	//px->fill(base.color());//fullSel ? selbg.color() : bg.color());
	px->fill(bg.color());
	QPainter pnt(px);
	pnt.setFont(QApplication::font());
	pnt.translate(-cxt.xoffset,0);
	/*pnt.fillRect(0, 0,
							m_leftMargin, m_lineSpacing*(wrap+1),
							base); // fillrect executed twice, to be optimized

	*/
	QBENCHMARK {
		dlh->draw(&pnt, cxt.xoffset, cxt.width, m_selectionBoundaries, m_cursorLines, cxt.palette, fullSel);
	}
	pnt.end();
	delete px;
#endif
}


void LatexEditorViewBenchmark::paintEvent_data(){
#if QT_VERSION >= 0x040500
	QTest::addColumn<QString>("text");
	QTest::addColumn<int>("start");
	QTest::addColumn<int>("count");

	//-------------cursor without selection--------------
	QTest::newRow("one line update")
		<< "abcdefg\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("correct german")
		<< "Haus\nAuto\nKind\nxyz\nc"
		<< 0 << 3;
	QTest::newRow("long line update")
		<< "abcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefgaabcdefga\nabcdefg\nabcdefg\nxyz\nc"
		<< 0 << 3;
	QTest::newRow("labels")
			<< "\\label{ab} \\label{cd}\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("refs")
			<< "\\ref{ab} \\ref{cd}\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("no spellcheck in command")
			<< "\\usepackage{abcdsdfsdfds} \\usepackage{abcdsdfsdfds}\nhallo welt\nabcdefg"
		<< 0 << 1;
	QTest::newRow("spellcheck in command")
			<< "\\textbf{abcdsdfsdfds} \\textbf{abcdsdfsdfds}\nhallo welt\nabcdefg"
		<< 0 << 1;
#endif
}
void LatexEditorViewBenchmark::paintEvent(){
#if QT_VERSION >= 0x040500
	QFETCH(QString, text);
	QFETCH(int, start);
	QFETCH(int, count);
        Q_UNUSED(start);
        Q_UNUSED(count);

	edView->editor->document()->setText(text);
	edView->editor->setCursorPosition(0,0);
	QBENCHMARK {
		edView->editor->repaint(edView->rect());
	}
#endif
}
#endif

