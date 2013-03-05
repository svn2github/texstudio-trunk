#ifndef PDFDOCUMENT_CONFIG_H
#define PDFDOCUMENT_CONFIG_H

#include <QByteArray>
struct PDFDocumentConfig{
	int windowLeft, windowTop;
	int windowWidth, windowHeight;
	bool windowMaximized;
	QByteArray windowState;

	int cacheSizeMB;

	int dpi;
	int scaleOption;
	int scale;

	double zoomStepFactor;

	int magnifierSize, magnifierShape;
	bool magnifierBorder;



	QString syncFileMask;


//live options
	bool continuous,singlepagestep;
	bool followFromCursor, followFromScroll, syncViews;
	int gridx,gridy;
};

#endif // PDFDOCUMENT_CONFIG_H
