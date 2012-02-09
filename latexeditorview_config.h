#ifndef LATEXEDITORVIEWCONFIG_H
#define LATEXEDITORVIEWCONFIG_H

//having the configuration in a single file allows to change it, 
//without having a relationship between completer and configmanager
//so modifying one doesn't lead to a recompilation of the other

class LatexEditorViewConfig
{
public:
	bool parenmatch, parenComplete;
	bool autoindent, weakindent;
	bool indentWithSpaces;
	bool showWhitespace;
	int tabStop;
	int showlinemultiples;
	int cursorSurroundLines;
	bool folding, showlinestate, showcursorstate, realtimeChecking;	
	bool inlineSpellChecking, inlineCitationChecking, inlineReferenceChecking, inlineSyntaxChecking, inlineGrammarChecking;
	QString fontFamily;
	int fontSize;
	int lineWidth;
	bool displayModifyTime;
	bool closeSearchAndReplace;
	bool useLineForSearch, searchOnlyInSelection;
	static QString translateEditOperation(int key);
	static QList<int> possibleEditOperations();
	bool mouseWheelZoom;
	bool hackAutoChoose, hackDisableFixedPitch, hackDisableWidthCache, hackDisableLineCache,hackDisableAccentWorkaround;
	int hackRenderingMode; //0: normal, 1: qt (missing), 2: single letter
	int wordwrap; // 0 off, 1 soft wrap, 2 soft wrap fixed line width, 3 hard wrap fixed line width
	bool toolTipPreview;
	bool toolTipHelp;
	
	void settingsChanged();
private:
	QString lastFontFamily;
	int lastFontSize;
};

#endif
