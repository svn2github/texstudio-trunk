#ifndef QDOCUMENTSEARCH_T_H
#define QDOCUMENTSEARCH_T_H
#ifndef QT_NO_DEBUG
#include "mostQtHeaders.h"
#include <QtTest/QtTest>

class QEditor;
class QDocumentSearch;
class QDocumentSearchTest: public QObject{
	Q_OBJECT
	public:
		QDocumentSearchTest(QEditor* editor);
	private:
		QDocumentSearch *ds;
		QEditor *ed;
	private slots:
		void initTestCase();
		void next_sameText_data();
		void next_sameText();
		void replaceAll_data();
		void replaceAll();
		void searchAndFolding_data();
		void searchAndFolding();
		void cleanupTestCase();
};

#endif
#endif
