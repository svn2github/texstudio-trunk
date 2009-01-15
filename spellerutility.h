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

#include "hunspell/hunspell.hxx"
#include "QHash"
#include "QLinkedList"
#include "QSet"

class SpellerUtility: public QObject {
   Q_OBJECT
public:
    SpellerUtility();
    ~SpellerUtility();
    bool loadDictionary(QString dic,QString ignoreFilePrefix);
    void unload();
    void addToIgnoreList(QString toIgnore);
    
    bool check(QString word);
    QStringList suggest(QString word);
    
    int spellcheckErrorFormat;
    bool isActive(); //interactive spelling only
    void setActive(bool a);
signals:
    void reloadDictionary();
private:
    QString currentDic, ignoreListFileName, spell_encoding;
    Hunspell * pChecker;
    QTextCodec *spellCodec;
    QHash<QString, bool> checkCache;
    QLinkedList<QString> checkCacheInsertion;
    QSet<QString> ignoredWords;
    bool active;
};

#endif
