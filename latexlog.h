#ifndef LATEXLOG_H
#define LATEXLOG_H
#include "mostQtHeaders.h"

#include "latexoutputfilter.h"
class LatexLogModel: public QAbstractTableModel {
	Q_OBJECT
private:
	QList<LatexLogEntry> log;
	bool foundType[4];
	int markIDs[4];
public:
	LatexLogModel(QObject * parent = 0);

	int columnCount(const QModelIndex & parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	void reset();

	int count();
	void clear();
	const LatexLogEntry& at(int i);
	//	void append(QString aFile, LogType aType, QString aOldline, int aLogline, QString aMessage);

	void parseLogDocument(QTextDocument* doc, QString baseFileName, QString overrideFileName);

	bool found(LogType lt) const;
	int markID(LogType lt) const;
	int logLineNumberToLogEntryNumber(int logLine) const; //returns the last entry with has a logline number <= logLine, or -1 if none exist
	bool existsReRunWarning() const;
	QStringList getMissingCitations() const;
};

#endif
