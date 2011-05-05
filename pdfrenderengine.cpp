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

#ifndef NO_POPPLER_PREVIEW

#include "pdfrenderengine.h"
#include "pdfrendermanager.h"

RenderCommand::RenderCommand(int p,double xr,double yr,int x,int y,int w, int h) :
    pageNr(p),xres(xr),yres(yr),x(x),y(y),w(w),h(h),ticket(-1)
{
}

PDFRenderEngine::PDFRenderEngine(QObject *parent) :
    QThread(parent)
{
    document=0;
    manager=qobject_cast<PDFRenderManager*>(parent);
}

PDFRenderEngine::~PDFRenderEngine(){
    delete document;
}

void PDFRenderEngine::run(){
    forever {
	//wait for enqueued lines
	manager->mCommandsAvailable.acquire();
	if(manager->stopped) break;
	// get Linedata
	manager->mQueueLock.lock();
	RenderCommand command=manager->mCommands.dequeue();
	manager->mQueueLock.unlock();
	// render Image
	if(document){
	    Poppler::Page *page=document->page(command.pageNr);
	    QImage	image=page->renderToImage(command.xres, command.yres,
					  command.x, command.y, command.w, command.h);
	    qDebug() << this << " Render page " << command.pageNr << " at " << command.ticket << "x/y" << command.x << command.y << " res "<<command.xres << ", " << command.w << command.h;
	    delete page;
	    emit sendImage(image,command.pageNr,command.ticket);
	}
    }
}

#endif
