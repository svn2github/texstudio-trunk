/***************************************************************************
 *   copyright       : (C) 2003-2007 by Pascal Brachet                     *
 *   http://www.xm1math.net/texmaker/                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "aboutdialog.h"

#include "smallUsefulFunctions.h"
extern const char* TEXSTUDIO_SVN_VERSION;

AboutDialog::AboutDialog(QWidget *parent)
		:QDialog(parent) {
	ui.setupUi(this);
	ui.textBrowser->setOpenExternalLinks(true);
	ui.textBrowser->setHtml(QString("<b>%1 %2</b> (SVN %3)").arg(TEXSTUDIO).arg(TXSVERSION).arg(TEXSTUDIO_SVN_VERSION?TEXSTUDIO_SVN_VERSION:"??") + "<br>" +
				tr("Using Qt Version %1, compiled with Qt %2").arg(qVersion()).arg(QT_VERSION_STR) + "<br>" +
				tr("Copyright (c) (original TexMaker) 2004-2010 by Pascal Brachet<br>")+
				tr(TEXSTUDIO ": Benito van der Zander, Jan Sundermeyer, Daniel Braun<br>QCodeEdit: Luc Bruant <br>html conversion: ")+QString::fromUtf8("Joël Amblard.</i><br>")+
	                        tr(TEXSTUDIO " contains code from the Hunspell (GPL), QtCreator (GPL, Copyright (C) Nokia), KILE (GPL) and SyncTeX (by Jerome Laurens) program.<br>TeXstudio uses the pdf viewer of TeXworks.<br> TeXstudio uses the DSingleApplication class (Author: Dima Fedorov Levit - Copyright (C) BioImage Informatics - Licence: GPL)<br>")+
				tr("Thanks to ")+QString::fromUtf8("Frederic Devernay, Denis Bitouzé, Jean-Côme Charpentier, Luis Silvestre, Enrico Vittorini, Aleksandr Zolotarev, David Sichau, Grigory Mozhaev, mattgk, A. Weder, Pavel Fric, Tim Hoffmann, András Somogyi, István Blahota & Edson Henriques.<br><br>")+
				tr("Project home site : <a href=\"http://texmakerx.sourceforge.net/\">http://texmakerx.sourceforge.net/</a><br><br>")+
				tr("Home site of original Texmaker: <a href=\"http://www.xm1math.net/texmaker/\">http://www.xm1math.net/texmaker/</a><br><br>")+
				tr("This program is licensed to you under the terms of the GNU General Public License Version 2 as published by the Free Software Foundation."));
}

AboutDialog::~AboutDialog() {
}
