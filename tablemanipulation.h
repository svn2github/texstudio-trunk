#ifndef TABLEMANIPULATION_H
#define TABLEMANIPULATION_H

#include "qdocument.h"
#include "mostQtHeaders.h"
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
	static int getNumOfColsInMultiColumn(const QString &str, QString *outAlignment=0, QString *outText=0);
    static int incNumOfColsInMultiColumn(const QString &str,int add);
    static void addHLine(QDocumentCursor &c,const int numberOfLines=-1,const bool remove=false);
	static QStringList splitColDef(QString def);
	static void simplifyColDefs(QStringList &colDefs);
    static void executeScript(QString script,QEditor *m_editor);
    static void generateTableFromTemplate(QEditor *m_editor,QString templateFileName,QString def,QList<QStringList> table,QString env);
    static QString getTableText(QDocumentCursor &cur);
    static void alignTableCols(QDocumentCursor &cur);

    static QStringList tabularNames;
    static QStringList tabularNamesWithOneOption;
};


class LatexTableLine : public QObject {
	Q_OBJECT
public:
	LatexTableLine(QObject *parent = 0);

	enum MultiColFlag {MCNone, MCStart, MCMid, MCEnd};

	void setMetaLine(const QString line) {metaLine = line;}
	void setColLine(const QString line);

	int colWidth(int col) const { return cols.at(col).length(); }
	int colCount() const { return cols.count(); }
	MultiColFlag multiColState(int col) { return mcFlag.at(col); }
	QChar multiColAlign(int col) { return mcAlign.at(col); }
	int multiColStart(int col) { for (;col>=0;col--) if (mcFlag.at(col)==MCStart) return col; return -1; }
	QString toMetaLine() const { return metaLine; }
	QString toColLine() const { return colLine; }
	QString colText(int col) const { return cols.at(col); }
	QString colText(int col, int width, const QChar &alignment);

private:
	void appendCol(const QString &col);

	QString colLine;
	QString metaLine;

	QStringList cols;
	QList<MultiColFlag> mcFlag;
	QList<QChar> mcAlign;
};
Q_DECLARE_METATYPE(LatexTableLine::MultiColFlag)


class LatexTableModel : public QAbstractTableModel {
	Q_OBJECT
public:
	LatexTableModel(QObject *parent = 0);

	void setContent(const QString &text);
	QStringList getAlignedLines(const QStringList alignment, const QString &rowIndent="\t") const;

	int rowCount(const QModelIndex &parent = QModelIndex()) const
		{ Q_UNUSED(parent) return lines.count(); }
	int columnCount(const QModelIndex &parent = QModelIndex()) const
		{ Q_UNUSED(parent) return (lines.count()>0)?lines.at(0)->colCount():0; }
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

private:
	QStringList metaLineCommands;
	QList<LatexTableLine *> lines;
};
#endif // TABLEMANIPULATION_H
