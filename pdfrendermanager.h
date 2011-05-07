#ifndef PDFRENDERMANAGER_H
#define PDFRENDERMANAGER_H

#include "mostQtHeaders.h"

 #include <QCache>

#include "pdfrenderengine.h"

class RecInfo{
public:
    QObject *obj;
    const char *slot;
    int x,y,w,h;
    int pageNr;
    qreal xres;
    bool cache;
};

class CachePixmap: public QPixmap
{
public:
    CachePixmap(const QPixmap &pixmap);
    CachePixmap();
    void setRes(qreal res,int x,int y);
    qreal getRes(){
	return resolution;
    }
    QPoint getCoord(){
	return QPoint(x,y);
    }

private:
    qreal resolution;
    int x,y;
};

class PDFRenderManager : public QObject
{
    Q_OBJECT
public:
    explicit PDFRenderManager(QObject *parent);
    ~PDFRenderManager();
    QPixmap renderToImage(int pageNr,QObject *obj,const char *rec,double xres=72.0, double yres=72.0, int x=-1, int y=-1, int w=-1, int h=-1,bool cache=true,bool priority=false);
    void setDocument(QString fileName);
    void stopRendering();
    void fillCache();

public slots:
    void addToCache(QImage img,int pageNr,int ticket);

private:
    friend class PDFRenderEngine;
    bool checkDuplicate(int &ticket,RecInfo &info);
    void enqueue(RenderCommand cmd,bool priority);

    Poppler::Document *document;

    QList<PDFRenderEngine *>renderQueues;
    QCache<int,CachePixmap> renderedPages;
    QMultiMap<int,RecInfo> lstOfReceivers;
    int currentTicket;

    QQueue<RenderCommand> mCommands;
    QSemaphore mCommandsAvailable;
    QMutex mQueueLock;
    bool stopped;

    bool firstThreadRunning;

    int num_renderQueues;
};

#endif // PDFRENDERMANAGER_H
