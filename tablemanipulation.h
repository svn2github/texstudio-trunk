#ifndef TABLEMANIPULATION_H
#define TABLEMANIPULATION_H

#include "qdocument.h"
class QEditor;

class LatexTables{
public:
    static void addRow(QDocumentCursor &c,const int numberOfColumns );
    static void addColumn(QDocument *doc,const int lineNumber,const int afterColumn,QStringList *cutBuffer=0);
    static void removeColumn(QDocument *doc,const int lineNumber,const int column,QStringList *cutBuffer=0);
    static void removeRow(QDocumentCursor &c);
    static int findNextToken(QDocumentCursor &cur,QStringList tokens,bool keepAnchor=false,bool backwards=false);
    static int getColumn(QDocumentCursor &cur);
    static QString getDef(QDocumentCursor &cur);
    static QString getSimplifiedDef(QDocumentCursor &cur);
    static int getNumberOfColumns(QDocumentCursor &cur);
    static int getNumberOfColumns(QStringList values);
    static bool inTableEnv(QDocumentCursor &cur);
    static int getNumOfColsInMultiColumn(const QString &str);
    static int incNumOfColsInMultiColumn(const QString &str,int add);
    static void addHLine(QDocumentCursor &c,const int numberOfLines=-1,const bool remove=false);
    static QStringList splitColDef(QString def);
    static void executeScript(QString script,QEditor *m_editor);
    static void generateTableFromTemplate(QEditor *m_editor,QString templateFileName,QString def,QList<QStringList> table,QString env);
    static QString getTableText(QDocumentCursor &cur);
    static void alignTableCols(QDocumentCursor &cur);

    static QStringList tabularNames;
    static QStringList tabularNamesWithOneOption;
};
#endif // TABLEMANIPULATION_H
