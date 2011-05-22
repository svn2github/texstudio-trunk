
#ifndef QT_NO_DEBUG
#include "mostQtHeaders.h"
#include "scriptengine_t.h"
#include "scriptengine.h"
#include "latexeditorview.h"
#include "qdocumentcursor.h"
#include "qdocument.h"
#include "qeditor.h"
#include "testutil.h"
#include <QtTest/QtTest>
ScriptEngineTest::ScriptEngineTest(QEditor* editor): ed(editor){
	ed->setCursorPosition(0,0);
	ed->setText("");
}

void ScriptEngineTest::script_data(){
	QTest::addColumn<QString>("script");
	QTest::addColumn<QString>("newText");
	
	//-------------cursor without selection--------------
	QTest::newRow("Setup Text")
		<< "editor.setText(\"Hallo\")"
		<< "Hallo";

	QTest::newRow("Insert Text")
		<< "cursor.insertText(\"a\");"
		<< "aHallo";

	QTest::newRow("Move Cursor")
		<< "cursor.moveTo(0,0);cursor.insertText(\"b\");"
		<< "baHallo";

	QTest::newRow("Delete Chars")
		<< "cursor.deletePreviousChar();cursor.deleteChar();"
		<< "Hallo";

	QTest::newRow("Undo")
		<< "editor.undo()"
		<< "baHallo";

	QTest::newRow("Redo")
		<< "editor.redo()"
		<< "Hallo";

	QTest::newRow("Select All/copy/paste")
		<< "editor.selectAll();editor.copy();editor.selectNothing();editor.paste()"
		<< "HalloHallo";

	QTest::newRow("remove Selection")
		<< "cursor.movePosition(1,cursorEnums.Start);cursor.movePosition(5,cursorEnums.Right,cursorEnums.KeepAnchor);cursor.removeSelectedText()"
		<< "Hallo";

	QTest::newRow("get selected Text")
		<< "cursor.movePosition(1,cursorEnums.Start);cursor.movePosition(2,cursorEnums.Right,cursorEnums.KeepAnchor);a=cursor.selectedText();cursor.replaceSelectedText(\"be\");cursor.clearSelection();cursor.insertText(a)"
		<< "beHallo";

	QTest::newRow("check cursor movement out of range")
		<< "cursor.movePosition(20,cursorEnums.Right);cursor.deleteChar();cursor.deletePreviousChar()"
		<< "bello";  // a movement out of range is canceled completely

	QTest::newRow("check cursor movement out of range")
		<< "cursor.moveTo(20,10);cursor.deleteChar();cursor.deletePreviousChar();cursor.eraseLine();cursor.insertText(\"as\")"
		<< "bello";  // invalid cursors are not executed

	QTest::newRow("Search/Replace Test 1")
		<< "editor.setText(\"Hallo1\\nHallo2\\nHallo3\"); editor.replace(\"a\", \"b\"); "
		<< "Hbllo1\nHbllo2\nHbllo3";
	QTest::newRow("Search/Replace Test 2")
		<< "editor.replace(\"ll\", \"tt\", editor.document().cursor(1,0,1,6)); "
		<< "Hbllo1\nHbtto2\nHbllo3";
	QTest::newRow("Search/Replace Test 3")
		<< "editor.replace(/b..o/, function(c){return editor.search(c.selectedText());}); "
		<< "H21\nH12\nH13";
	QTest::newRow("Search/Replace Test 4 (no conversion)")
		<< "editor.replace(/[0-9]*/, function(c){return 17+c.selectedText();}); "
		<<  "H1721\nH1712\nH1713";
	QTest::newRow("Search/Replace Test 5")
		<< "editor.replace(/[0-9]*/, function(c){return 17+1*c.selectedText();}); "
		<< "H1738\nH1729\nH1730";
}
void ScriptEngineTest::script(){
	QFETCH(QString, script);
	QFETCH(QString, newText);
	scriptengine eng(this);
	eng.setEditor(ed);
	eng.setScript(script);
	eng.run();

	QEQUAL(ed->document()->text(), newText);
	
}
#endif

