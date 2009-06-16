/***************************************************************************
 *   copyright       : (C) 2008 by Benito van der Zander                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SMALLUSEFULFUNCTIONS_H
#define SMALLUSEFULFUNCTIONS_H
#include <QString>
#include <QToolButton>
#include <QScrollBar>

QString getCommonEOW();

QStringList findResourceFiles(QString dirName, QString filter);
//returns the real name of a resource file (allowing the user to override resource files with local files)
QString findResourceFile(QString fileName);
//returns if the file is writable (QFileInfo.isWritable works in different ways on Windows and Linux)
bool isFileRealWritable(QString filename);
//returns if the file exists and is writable
bool isExistingFileRealWritable(QString filename);
//get relative path for a given file
QString getRelativePath(const QString basepath, const QString & file);

//returns kde version 0,3,4
int x11desktop_env();


//searches the next token in the line line after/at the index index
//there are these possible kind of tokens % (which starts a comment), { or } (as parantheses), \.* (command) or .* (text)
//index returns the index of the first character after the word
//return: start index of the token (or -1 if last)
int nextToken(const QString &line,int &index);


enum NextWordFlag {
	NW_NOTHING=0,
	NW_TEXT=1,
	NW_COMMAND=2,
	NW_COMMENT=3,
	NW_ENVIRONMENT=4 //environment name, e.g. in \begin or \newenvironment
};
//Returns the next word (giving meaning to the nextToken tokens)
//line: line to be examined
//index: start index as input and returns the first character after the found word
//outWord: found word (length can differ from index - wordStartIndex for text words)
//wordStartIndex: start of the word
//returnCommands: if this is true it returns \commands (NW_COMMAND), "normal" "text"  NW_TEXT and % (NW_COMMENT)  [or NW_NOTHING at the end]
//                "    "  is false it only returns normal text (NW_TEXT, without things like filenames after \include), environment names
//                          (NW_ENVIRONMENT, they are treated as text in the other mode) and % (NW_COMMENT)       [or NW_NOTHING at the end]
//returns the type of outWord
NextWordFlag nextWord(const QString & line, int &index, QString &outWord, int &wordStartIndex, bool returnCommands);

//searches the next text words and ignores command options, environments or comments
//returns false if none is found
bool nextTextWord(const QString & line, int &index, QString &outWord, int &wordStartIndex);

//removes special latex characters
QString latexToPlainWord(QString word);
//extracts the section name after \section is removed (brackets removal)
QString extractSectionName(QString word);
//replaces character with corresponding LaTeX commands
QString textToLatex(QString text);

//compares two strings local aware
bool localAwareLessThan(const QString &s1, const QString &s2);

// find token (e.g. \label \input \section and return content (\section{content})
QString findToken(const QString &line,const QString &token);
QString findToken(const QString &line,const QRegExp &token);
// find token (e.g. \label \input \section and return content (\newcommand{name}[arg]), returns true if outName!=""
bool findTokenWithArg(const QString &line,const QString &token, QString &outName, QString &outArg);

//setup toolbutton as substitute for const combobox
QToolButton* createComboToolButton(QWidget *parent,QStringList list,const int height,const QFontMetrics fm,const QObject * receiver, const char * member);

// realizes whether col is in a reference or in a command
int findContext(QString &line,int col);
#endif
