
/***************************************************************************
 *   copyright       : (C) 2008 by Benito van der Zander                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "spellerutility.h"
#include "smallUsefulFunctions.h"

int SpellerUtility::spellcheckErrorFormat = -1;

SpellerUtility::SpellerUtility(QString name): mName(name), currentDic(""), pChecker(0), spellCodec(0) {
	checkCache.reserve(1020);
}
bool SpellerUtility::loadDictionary(QString dic,QString ignoreFilePrefix) {
	if (dic==currentDic) return true;
	else unload();
	QString base = dic.left(dic.length()-4);
	QString dicFile = base+".dic";
	QString affFile = base+".aff";
	if (!QFileInfo(dicFile).exists() || !QFileInfo(affFile).exists()) {
		if (!QFileInfo(affFile).exists())
			mLastError = tr("Missing .aff file:\n%1").arg(affFile);
		else
			mLastError = tr("Dictionary does not exist.");
		emit dictionaryLoaded(); //remove spelling error marks from errors with previous dictionary (because these marks could lead to crashes if not removed)
		return false;
	}
	currentDic=dic;
	pChecker = new Hunspell(affFile.toLocal8Bit(),dicFile.toLocal8Bit());
	if (!pChecker) {
		currentDic="";
		ignoreListFileName="";
		mLastError = "Creation of Hunspell object failed.";
		REQUIRE_RET(false,false);
	}
	spell_encoding=QString(pChecker->get_dic_encoding());
	spellCodec = QTextCodec::codecForName(spell_encoding.toLatin1());
	if (spellCodec==0) {
		mLastError = "Could not determine encoding.";
		unload();
		emit dictionaryLoaded();
		return false;
	}

	checkCache.clear();
	ignoredWords.clear();
	ignoredWordList.clear();
	ignoredWordsModel.setStringList(QStringList());
	ignoreListFileName=base+".ign";
	if (!isFileRealWritable(ignoreListFileName))
		ignoreListFileName=ignoreFilePrefix+QFileInfo(dic).baseName()+".ign";
	if (!isFileRealWritable(ignoreListFileName)) {
		ignoreListFileName="";
		mLastError.clear();
		emit dictionaryLoaded();
		return true;
	}
	QFile f(ignoreListFileName);
	if (!f.open(QFile::ReadOnly)) {
		mLastError.clear();
		emit dictionaryLoaded();
		return true;
	}
	ignoredWordList=QTextCodec::codecForName("UTF-8")->toUnicode(f.readAll()).split("\n",QString::SkipEmptyParts);
	// add words in user dic
	QByteArray encodedString;
	QString spell_encoding=QString(pChecker->get_dic_encoding());
	QTextCodec *codec = QTextCodec::codecForName(spell_encoding.toLatin1());
	foreach(const QString &elem,ignoredWordList){
		encodedString = codec->fromUnicode(elem);
		pChecker->add(encodedString.data());
	}
	qSort(ignoredWordList.begin(),ignoredWordList.end(),localAwareLessThan);
	while (!ignoredWordList.empty() && ignoredWordList.first().startsWith("%")) ignoredWordList.removeFirst();
	ignoredWordsModel.setStringList(ignoredWordList);
	ignoredWords=ignoredWordList.toSet();
	mLastError.clear();
	emit dictionaryLoaded();
	return true;

}
void SpellerUtility::saveIgnoreList() {
	if (ignoreListFileName!="" && ignoredWords.count()>0) {
		QFile f(ignoreListFileName);
		if (f.open(QFile::WriteOnly)) {
			QTextCodec* utf8=QTextCodec::codecForName("UTF-8");
			f.write(utf8->fromUnicode("%Ignored-Words;encoding:utf-8;version:" TEXSTUDIO ":1.8\n"));
			foreach(const QString str, ignoredWordList)
				if (!str.startsWith("%"))
					f.write(utf8->fromUnicode(str+"\n"));
		}
	}
}
void SpellerUtility::unload() {
	saveIgnoreList();
	currentDic="";
	ignoreListFileName="";
	if (pChecker!=0) {
		delete pChecker;
		pChecker=0;
	}
}
void SpellerUtility::addToIgnoreList(QString toIgnore) {
	QString word=latexToPlainWord(toIgnore);
	QByteArray encodedString;
	QString spell_encoding=QString(pChecker->get_dic_encoding());
	QTextCodec *codec = QTextCodec::codecForName(spell_encoding.toLatin1());
	encodedString = codec->fromUnicode(word);
	pChecker->add(encodedString.data());
	ignoredWords.insert(word);
	if (!ignoredWordList.contains(word))
		ignoredWordList.insert(qLowerBound(ignoredWordList.begin(),ignoredWordList.end(), word, localAwareLessThan), word);
	ignoredWordsModel.setStringList(ignoredWordList);
	saveIgnoreList();
	emit ignoredWordAdded(word);
}
void SpellerUtility::removeFromIgnoreList(QString toIgnore) {
	QByteArray encodedString;
	QString spell_encoding=QString(pChecker->get_dic_encoding());
	QTextCodec *codec = QTextCodec::codecForName(spell_encoding.toLatin1());
	encodedString = codec->fromUnicode(toIgnore);
	pChecker->remove(encodedString.data());
	ignoredWords.remove(toIgnore);
	ignoredWordList.removeAll(toIgnore);
	ignoredWordsModel.setStringList(ignoredWordList);
	saveIgnoreList();
}
QStringListModel* SpellerUtility::ignoreListModel() {
	return &ignoredWordsModel;
}

SpellerUtility::~SpellerUtility() {
	emit aboutToDelete();
	unload();
}
bool SpellerUtility::check(QString word) {
	if (currentDic=="" || pChecker==0) return true; //no speller => everything is correct
	if (word.length()<=1) return true;
	if (ignoredWords.contains(word)) return true;
	if (word.endsWith('.')&&ignoredWords.contains(word.left(word.length()-1))) return true;
	if (checkCache.contains(word)) return checkCache.value(word);
	QByteArray encodedString = spellCodec->fromUnicode(word);
	bool result=pChecker->spell(encodedString.data());
	while (checkCacheInsertion.size()>1000) checkCacheInsertion.removeFirst();
	checkCache.insert(word,result);
	checkCacheInsertion.append(word);
	return result;
}
QStringList SpellerUtility::suggest(QString word) {
	Q_ASSERT(pChecker);
	if (currentDic=="" || pChecker==0) return QStringList(); //no speller => everything is correct
	QByteArray encodedString = spellCodec->fromUnicode(word);
	char ** wlst;
	int ns = pChecker->suggest(&wlst,encodedString.data());
	QStringList suggestion;
	if (ns > 0) {
		for (int i=0; i < ns; i++) {
			suggestion << spellCodec->toUnicode(wlst[i]);
			free(wlst[i]);
		}
		free(wlst);
	}
	return suggestion;
}



SpellerManager::SpellerManager() {
	emptySpeller = new SpellerUtility("<none>");
	mDefaultSpellerName = emptySpeller->name();
}

SpellerManager::~SpellerManager() {
	unloadAll();
	if (emptySpeller) {
		delete emptySpeller;
		emptySpeller = 0;
	}
}

void SpellerManager::setIgnoreFilePrefix(const QString &prefix) {
	ignoreFilePrefix = prefix;
}

void SpellerManager::setDictPath(const QString &dictPath) {
	if (dictPath == m_dictPath) return;
	m_dictPath = dictPath;

	QList<SpellerUtility *> oldDicts = dicts.values();

	dicts.clear();
	dictFiles.clear();
	QDir dir(dictPath);
	QMap<QString, QString> usedFiles;
	foreach (QFileInfo fi, dir.entryInfoList(QStringList() << "*.dic", QDir::Files, QDir::Name)) {
		QString realDictFile;
		if (fi.isSymLink()) realDictFile = QFileInfo(fi.symLinkTarget()).canonicalFilePath();
		else realDictFile = fi.canonicalFilePath();
		
		if ( usedFiles.value(fi.baseName().replace("_", "-"), "") == realDictFile )
			continue;
		else 
			usedFiles.insert(fi.baseName().replace("_", "-"), realDictFile);
			
		
		dictFiles.insert(fi.baseName(), fi.canonicalFilePath());
	}
	

	// delete after new dict files are identified so a user can reload the new dict in response to a aboutToDelete signal
	foreach (SpellerUtility *su, oldDicts) {
		delete su;
	}

	emit dictPathChanged();
}

QStringList SpellerManager::availableDicts() {
	if (dictFiles.keys().isEmpty())
		return QStringList() << emptySpeller->name();
	return QStringList(dictFiles.keys());
}

QStringList SpellerManager::dictNamesForDir(const QString &dir) {
	QStringList dictNames;

	foreach (QFileInfo fi, QDir(dir).entryInfoList(QStringList() << "*.dic", QDir::Files, QDir::Name)) {
		dictNames << fi.baseName();
	}
	if (dictNames.isEmpty())
		return QStringList() << "<none>";
	return dictNames;
}

bool SpellerManager::hasSpeller(const QString &name) {
	if (name==emptySpeller->name()) return true;
	return dictFiles.contains(name);
}

bool SpellerManager::hasSimilarSpeller(const QString &name, QString* bestName){
	REQUIRE_RET(bestName, false);

	QList<QString> keys = dictFiles.keys();
	for (int i=0;i<keys.length();i++)
		if (0==QString::compare(keys[i], name, Qt::CaseInsensitive)) {
			*bestName = keys[i];
			return true;
		}
	
	*bestName = name;
	bestName->replace("_", "-");

	if (!bestName->contains('-')) return false;
	
	for (int i=0;i<keys.length();i++)
		if (0==QString::compare(keys[i], *bestName, Qt::CaseInsensitive)) {
			*bestName = keys[i];
			return true;
		}
	
	*bestName = bestName->left(bestName->indexOf('-'));
	for (int i=0;i<keys.length();i++)
		if (keys[i].startsWith(*bestName, Qt::CaseInsensitive)) {
			*bestName = keys[i];
			return true;
		}
	return false;	
}

/*!
    If the language has not been used yet, a SpellerUtility for the language is loaded. Otherwise
    the existing SpellerUtility is returned. Possible names are the dictionary file names without ".dic"
    ending and "<default>" for the default speller
*/
SpellerUtility *SpellerManager::getSpeller(QString name) {
	if (name == "<default>") name = mDefaultSpellerName;
	if (!dictFiles.contains(name)) return emptySpeller;

	SpellerUtility *su = dicts.value(name, 0);
	if (!su) {
		su = new SpellerUtility(name);
		if (!su->loadDictionary(dictFiles.value(name), ignoreFilePrefix)) {
			txsWarning(QString("Loading of dictionary failed:\n%1\n\n%2").arg(dictFiles.value(name)).arg(su->mLastError));
			delete su;
			return 0;
		}
		dicts.insert(name, su);
	}
	return su;
}

QString SpellerManager::defaultSpellerName() {
	return mDefaultSpellerName;
}

bool SpellerManager::setDefaultSpeller(const QString &name) {
	if (dictFiles.contains(name)) {
		mDefaultSpellerName = name;
		emit defaultSpellerChanged();
		return true;
	}
	return false;
}

void SpellerManager::unloadAll() {
	foreach(SpellerUtility *su, dicts.values()) {
		delete su;
	}
	dicts.clear();
}

QString SpellerManager::prettyName(const QString &name) {
	QLocale loc(name);
	if (loc == QLocale::c()) {
		return name;
	} else {
		return QString("%1 - %2 (%3)").arg(name).arg(QLocale::languageToString(loc.language())).arg(QLocale::countryToString(loc.country()));
	}
}

