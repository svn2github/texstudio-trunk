/***************************************************************************
 *   copyright       : (C) 2011 by Jan Sundermeyer                         *
 *   http://texmakerx.sf.net                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "pdfrendermanager.h"

PDFRenderManager::PDFRenderManager(QObject *parent) :
    QObject(parent)
{
    renderQueue=new PDFRenderEngine(this);
    connect(renderQueue,SIGNAL(sendImage(QImage,int,int)),this,SLOT(addToCache(QImage,int,int)));
    document=0;
    currentTicket=0;
}

PDFRenderManager::~PDFRenderManager(){
    delete document;
    renderQueue->stop();
    renderQueue->wait();
    document=0;
}

void PDFRenderManager::stopRendering(){
    renderQueue->stop();
    renderQueue->wait();
}

void PDFRenderManager::setDocument(QString fileName){
    renderedPages.clear();
    document=Poppler::Document::load(fileName);
    document->setRenderBackend(Poppler::Document::SplashBackend);
    document->setRenderHint(Poppler::Document::Antialiasing);
    document->setRenderHint(Poppler::Document::TextAntialiasing);
    renderQueue->setDocument(document);
    if(!renderQueue->isRunning())
	renderQueue->start();
    fillCache();
}

QImage PDFRenderManager::renderToImage(int pageNr,QObject *obj,const char *rec,double xres, double yres, int x, int y, int w, int h,bool cache){
    RecInfo info;
    info.obj=obj;
    info.slot=rec;
    info.x=x;
    info.y=y;
    info.w=w;
    info.h=h;
    info.pageNr=pageNr;
    info.cache=cache;
    info.xres=xres;
    currentTicket++;
    bool enqueue=!checkDuplicate(currentTicket,info);
    lstOfReceivers.insert(currentTicket,info);
    // return best guess/cached at once, refine later
    Poppler::Page *page=document->page(pageNr);
    QImage img;
    qreal scale=10;
    if(renderedPages.contains(pageNr)){ // try cache first
	img=*renderedPages[pageNr];
    }
    if(img.isNull()) // not cached, thumbnail present ?
	img=page->thumbnail();
    if(!img.isNull()){ // if a image was found, scale it apropriately
	QSize sz=page->pageSize();
	scale=sz.width()*xres/(72.0*img.width());
	int sx=qRound(img.width()*scale);
	int sy=qRound(img.height()*scale);
	if(scale>1.01 || scale<0.99)
	    img=img.scaled(QSize(sx,sy),Qt::KeepAspectRatio,Qt::SmoothTransformation);
	if(x>-1 && y>-1 && w>-1 && h>-1){
	    img=img.copy(x,y,w,h);
	}
    }
    if(img.isNull()){
	// generate deafult empty, to be rendered image
	if(w<0 || h<0){
	    QSize sz=page->pageSize();
	    w=sz.width();
	    h=sz.height();
	}
	img=QImage(w,h,QImage::Format_RGB32);
	img.fill(QApplication::palette().color(QPalette::Light).rgb());
	// paint something (rendering ... or similar)
    }
    if(enqueue){
	if(scale>1.1){ // don't render again if it is smaller or about right
	    RenderCommand cmd(pageNr,xres,yres);
	    cmd.ticket=currentTicket;
	    renderQueue->enqueue(cmd);
	}else{
	    lstOfReceivers.remove(currentTicket);
	}
    }
    delete page;
    return img;
}

void PDFRenderManager::addToCache(QImage img,int pageNr,int ticket){
    if(lstOfReceivers.contains(ticket)){
	QList<RecInfo> infos=lstOfReceivers.values(ticket);
	lstOfReceivers.remove(ticket);
	foreach(RecInfo info,infos){
	    if(info.cache){
		QImage *image=new QImage(img);
		renderedPages.insert(pageNr,image);
	    }
	    if(info.obj){
		if(info.x>-1 && info.y>-1 && info.w>-1 && info.h>-1)
		    img=img.copy(info.x,info.y,info.w,info.h);
		QMetaObject::invokeMethod(info.obj,info.slot,Q_ARG(QImage,img),Q_ARG(int,pageNr));
	    }
	}
    }
}

void PDFRenderManager::fillCache(){
    for(int i=0;i<document->numPages();i++){
	renderToImage(i,0,"");
    }
}

bool PDFRenderManager::checkDuplicate(int &ticket,RecInfo &info){
    //check if a similar picture is not already in the renderqueue
    QMultiMap<int,RecInfo>::const_iterator i=lstOfReceivers.constBegin();
    for(i;i!=lstOfReceivers.constEnd();i++){
	if(i.value().pageNr!=info.pageNr)
	    continue;
	if(i.value().x<0 && i.value().y<0 && i.value().w<0 && i.value().h<0
	    && info.x<0 && info.y<0 && info.w<0 && info.h<0){
	    if(i.value().xres*1.01>=info.xres){
		ticket=i.key();
		return true;
	    }
	}
    }
    return false;
}
