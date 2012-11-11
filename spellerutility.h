/***************************************************************************
 *   copyright       : (C) 2008 by Benito van der Zander                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SPELLERUTILITY_H
#define SPELLERUTILITY_H

#include "mostQtHeaders.h"

#include "hunspell/hunspell.hxx"
class SpellerUtility: public QObject {
	Q_OBJECT
public:
	friend class SpellerManager;
	void addToIgnoreList(QString toIgnore);
	void removeFromIgnoreList(QString toIgnore);
	QStringListModel* ignoreListModel();

	bool check(QString word);
	QStringList suggest(QString word);

	QString name() {return mName;}
	QString getCurrentDic() {return currentDic;}

	static int spellcheckErrorFormat;
signals:
	void aboutToDelete();
	void dictionaryLoaded();
	void ignoredWordAdded(const QString& newlyIgnoredWord);
private:
	SpellerUtility(QString name);
	~SpellerUtility();
	bool loadDictionary(QString dic, QString ignoreFilePrefix);
	void saveIgnoreList();
	void unload();

	QString mName;
	QString mLastError;
	QString currentDic, ignoreListFileName, spell_encoding;
	Hunspell * pChecker;
	QTextCodec *spellCodec;
	QHash<QString, bool> checkCache;
	QLinkedList<QString> checkCacheInsertion;
	QStringList ignoredWordList;
	QSet<QString> ignoredWords;
	QStringListModel ignoredWordsModel;
};

class SpellerManager: public QObject {
	Q_OBJECT
public:
	SpellerManager();
	~SpellerManager();
	void setIgnoreFilePrefix(const QString &ignoreFilePrefix);
	QString dictPath();
	void setDictPath(const QString &dictPath);
	QStringList availableDicts();
	static QStringList dictNamesForDir(const QString &dir);

	bool hasSpeller(const QString &name);
	bool hasSimilarSpeller(const QString &name, QString* bestName);
	SpellerUtility *getSpeller(QString name);

	QString defaultSpellerName();
	bool setDefaultSpeller(const QString &name);
	void unloadAll();

	static QString prettyName(const QString &name);
signals:
	void dictPathChanged();
	void defaultSpellerChanged();
private:
	QString m_dictPath;
	QString ignoreFilePrefix;
	QHash<QString, SpellerUtility *> dicts;
	QHash<QString, QString> dictFiles;
	SpellerUtility *emptySpeller;
	QString mDefaultSpellerName;
};


#endif
