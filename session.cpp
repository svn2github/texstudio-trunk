#include "session.h"
#include "configmanager.h"
#include "smallUsefulFunctions.h"
#include "mostQtHeaders.h"

Session::Session(const Session &s)
{
	m_files.append(s.m_files);
	m_masterFile = s.m_masterFile;
	m_currentFile = s.m_currentFile;
}

bool Session::load(const QString &file) {
	if (!QFileInfo(file).isReadable()) return false;
	QSettings s(file, QSettings::IniFormat);
	if (!s.childGroups().contains("Session")) return false;
	m_files.clear();
	s.beginGroup("Session");
	QStringList groups = s.childGroups();
	for (int i=0; i<1000; i++) {
		if (!groups.contains(QString("File%1").arg(i)))
			break;
		s.beginGroup(QString("File%1").arg(i));
		FileInSession f;
		f.fileName = s.value("FileName").toString();
		f.cursorLine = s.value("Line", 0).toInt();
		f.cursorCol = s.value("Col", 0).toInt();
		f.firstLine = s.value("FirstLine", 0).toInt();
		m_files.append(f);
		s.endGroup();
	}
	m_masterFile = s.value("MasterFile").toString();
	m_currentFile = s.value("CurrentFile").toString();
	m_bookmarks = s.value("Bookmarks").value<QList<QVariant> >();
	s.endGroup();
	return true;
}

// legacy code to support reading the session information which was previously stored in the config file (TXS <= 2.5.1)
// may be removed later on
bool Session::load(const ConfigManager &config) {
	QStringList sessionFilesToRestore = config.getOption("Files/Session/Files").toStringList();
	QList<QVariant> sessionCurRowsToRestore = config.getOption("Files/Session/curRows").value<QList<QVariant> >();
	QList<QVariant> sessionCurColsToRestore = config.getOption("Files/Session/curCols").value<QList<QVariant> >();
	QList<QVariant> sessionFirstLinesToRestore = config.getOption("Files/Session/firstLines").value<QList<QVariant> >();
	QString sessionCurrent = config.getOption("Files/Session/CurrentFile").toString();
	QString sessionMaster = config.getOption("Files/Session/MasterFile").toString();
	QList<QVariant> bookmarkList = config.getOption("Files/Bookmarks").value<QList<QVariant> >();

	for (int i=0; i<sessionFilesToRestore.size(); i++){
		FileInSession f;
		f.fileName = sessionFilesToRestore[i];
		f.cursorLine = sessionCurRowsToRestore.value(i,QVariant(0)).toInt();
		f.cursorCol = sessionCurColsToRestore.value(i,0).toInt();
		f.firstLine = sessionFirstLinesToRestore.value(i,0).toInt();
		m_files.append(f);
	}
	m_masterFile = sessionMaster;
	m_currentFile = sessionCurrent;
	m_bookmarks = bookmarkList;
	return true;
}


bool Session::save(const QString &file) const {
	if (!isFileRealWritable(file)) return false;
	QSettings s(file, QSettings::IniFormat);
	s.clear();
	s.beginGroup("Session");
	s.setValue("FileVersion", 1); // increment if format changes are applied later on. This might be used for version-dependent loading.
	for (int i=0; i<m_files.count(); i++) {
		s.beginGroup(QString("File%1").arg(i));
		s.setValue("FileName", m_files[i].fileName);
		s.setValue("Line", m_files[i].cursorLine);
		s.setValue("Col", m_files[i].cursorCol);
		s.setValue("FirstLine", m_files[i].firstLine);
		s.endGroup();
	}
	s.setValue("MasterFile", m_masterFile);
	s.setValue("CurrentFile", m_currentFile);
	s.setValue("Bookmarks", m_bookmarks);

	s.endGroup();
	return true;
}

void Session::addFile(FileInSession f) {
	m_files.append(f);
}
