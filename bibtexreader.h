#ifndef BIBTEXREADER_H
#define BIBTEXREADER_H
#include "smallUsefulFunctions.h"
#include "mostQtHeaders.h"
#include <QThread>

class bibtexReader : public SafeThread
{
    Q_OBJECT
public:
    explicit bibtexReader(QObject *parent = 0);

signals:
    void sectionFound(QString bibId,QString content);

public slots:
    void searchSection(QString file,QString bibId);
};

#endif // BIBTEXREADER_H
