#ifndef TABLEMANIPULATION_T_H
#define TABLEMANIPULATION_T_H
#ifndef QT_NO_DEBUG
#include "mostQtHeaders.h"

class QEditor;
class TableManipulationTest: public QObject{
	Q_OBJECT
	public:
		TableManipulationTest(QEditor* editor);
	private:
		QEditor *ed;
	private slots:
		void addCol_data();
		void addCol();
		void addRow_data();
		void addRow();
		void remCol_data();
		void remCol();
		void remRow_data();
		void remRow();
		void getCol_data();
		void getCol();
		void getNumberOfCol_data();
		void getNumberOfCol();
		void findNextToken_data();
		void findNextToken();
		void findNextTokenBackwards_data();
		void findNextTokenBackwards();
		void addHLine_data();
		void addHLine();
};

#endif
#endif
