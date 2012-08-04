#include "updatechecker.h"
#include "smallUsefulFunctions.h"
#include "configmanager.h"
#include <QNetworkReply>
#include <QMutex>

QRegExp UpdateChecker::rxVersion("(\\d+)(?:\\.(\\d+))+");
UpdateChecker * UpdateChecker::m_Instance = 0;

UpdateChecker::UpdateChecker() :
	QObject(0), silent(true)
{
	networkManager = new QNetworkAccessManager();
}

// compares two versions strings
// Meaning of result: v1 [result] v2, e.g. v1 Older than v2
UpdateChecker::VersionCompareResult UpdateChecker::versionCompare(const QString &v1, const QString &v2) {
	if (!rxVersion.exactMatch(v1)) return Invalid;
	QStringList v1parts = rxVersion.capturedTexts();
	v1parts.removeAt(0); // first element is complete string

	if (!rxVersion.exactMatch(v2)) return Invalid;
	QStringList v2parts = rxVersion.capturedTexts();
	v2parts.removeAt(0); // first element is complete string

	for (int i=v1parts.count(); i<v2parts.count(); i++) {
		v1parts.append("0");
	}
	for (int i=v2parts.count(); i<v1parts.count(); i++) {
		v2parts.append("0");
	}

	for (int i=0; i<v1parts.count(); i++) {
		int n1 = v1parts.at(i).toInt();
		int n2 = v2parts.at(i).toInt();
		if (n1 < n2) return Lower;
		else if (n1 > n2) return Higher;
	}
	return Same;
}

QString UpdateChecker::lastCheckAsString() {
	QDateTime lastCheck = ConfigManager::getInstance()->getOption("Update/LastCheck").toDateTime();
	return lastCheck.isValid() ? lastCheck.toString("d.M.yyyy hh:mm") : tr("Never", "last update");
}

void UpdateChecker::autoCheck() {
	ConfigManagerInterface *cfg  = ConfigManager::getInstance();
	bool autoCheck = cfg->getOption("Update/AutoCheck").toBool();
	if (autoCheck) {
		QDateTime lastCheck = cfg->getOption("Update/LastCheck").toDateTime();
		int checkInterval = cfg->getOption("Update/AutoCheckInvervalDays").toInt();
		if (!lastCheck.isValid() || lastCheck.addDays(checkInterval) < QDateTime::currentDateTime()) {
			check();
		}
	}

}

void UpdateChecker::check(bool silent) {
	this->silent = silent;
	//connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(fileIsReady(QNetworkReply*)) );
	QNetworkReply *reply = networkManager->get(QNetworkRequest(QUrl("http://texstudio.sourceforge.net/update/txsUpdate.xml")));
	connect(reply, SIGNAL(finished()), this, SLOT(onRequestCompleted()));
	if (!silent)
		connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onRequestError()));
}

void UpdateChecker::onRequestError() {
	QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
	if (!reply) return;

	txsCritical(tr("Update check failed with error:\n") + reply->errorString());
}

void UpdateChecker::onRequestCompleted()
{
	QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
	if (!reply || reply->error() != QNetworkReply::NoError) return;

	QTemporaryFile tempFile;
	QByteArray ba = reply->readAll();
	parseData(ba);
}

void UpdateChecker::parseData(const QByteArray &data) {
	QDomDocument domDocument;
	if (domDocument.setContent(data)){
		QDomElement root = domDocument.documentElement();
		if (root.tagName() != "versions") {
			if (!silent) txsWarning(tr("Update check failed (invalid update file format)."));
			return;
		}
		QDomNodeList nodes = root.elementsByTagName("stable");
		if (nodes.count() == 1) {
			QDomElement latestRelease = nodes.at(0).toElement();

			VersionCompareResult res = versionCompare(latestRelease.attribute("value"), TXSVERSION);
			if (res == Invalid) {
				if (!silent) txsWarning(tr("Update check failed (invalid update file format)."));
				return;
			}
			if (res == Higher) {
				QString text = QString(tr(
					"A new version of TeXstudio is available.<br><br>"
					"Current version: %1<br>"
					"Latest version: %2<br><br>"
					"You can download it from the <a href='%3'>TeXstudio website</a>."
					)).arg(TXSVERSION).arg(latestRelease.attribute("version")).arg("http://texstudio.sourceforge.net");
				QMessageBox msgBox;
				msgBox.setWindowTitle(tr("TeXstudio Update"));
				msgBox.setTextFormat(Qt::RichText);
				msgBox.setText(text);
				msgBox.setStandardButtons(QMessageBox::Ok);
				msgBox.exec();
			} else {
				if (!silent) txsInformation(tr("TeXstudio is up-to-date."));
			}

			ConfigManager::getInstance()->setOption("Update/LastCheck", QDateTime::currentDateTime());
			emit checkCompleted();
		}
	}
}

UpdateChecker *UpdateChecker::instance() {
	static QMutex mutex;
	mutex.lock();
	if (!m_Instance)
		m_Instance = new UpdateChecker();
	mutex.unlock();
	return m_Instance;
}


