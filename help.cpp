#include "help.h"
#include "texdocdialog.h"
#include "smallUsefulFunctions.h"
#include <QMutex>


Help * Help::m_Instance = 0;

Help::Help() :
	QObject(0)
{
}

void Help::execTexdocDialog(const QStringList &packages, const QString &defaultPackage)
{
	TexdocDialog dialog;
	dialog.setPackageNames(packages);
	if (!defaultPackage.isEmpty()) {
		dialog.setPreferredPackage(defaultPackage);
	}
	if (dialog.exec()) {
		viewTexdoc(dialog.selectedPackage());
		QString package = dialog.selectedPackage();
	}
}

void Help::viewTexdoc(QString package)
{
	if (package.isEmpty()) {
		QAction *act = qobject_cast<QAction*>(sender());
		if (!act) return;
		package = act->data().toString();
	}
	if (!package.isEmpty()) {
		QStringList args;
		args << "--view" << package;
		QProcess proc(this);
		connect(&proc, SIGNAL(readyReadStandardError()), this, SLOT(viewTexdocError()));
		proc.start("texdoc", args);
		if (!proc.waitForFinished(2000)) {
			txsWarning(QString(tr("texdoc took too long to open the documentation for the package:")+"\n%1").arg(package));
			return;
		}
	}
}

void Help::texdocAvailableRequest(const QString &package)
{
	if (package.isEmpty())
		return;

	QStringList args;
	args << "--print-only" << package;
	QProcess *proc = new QProcess(this);
	proc->setProperty("package", package);
	connect(proc, SIGNAL(finished(int)), SLOT(texdocAvailableRequestFinished(int)));
	proc->start("texdoc", args);
}

void Help::texdocAvailableRequestFinished(int exitCode)
{
	Q_UNUSED(exitCode);
	QProcess *proc = qobject_cast<QProcess *>(sender());
	if (!proc) return;
	QString package = proc->property("package").toString();
	QString docCommand = proc->readAll();

	emit texdocAvailableReply(package, !docCommand.isEmpty());
	proc->deleteLater();
}


void Help::viewTexdocError()
{
	QProcess *proc = qobject_cast<QProcess*>(sender());
	if (proc) {
		txsWarning(proc->readAllStandardError());
	}
}

Help *Help::instance()
{
	static QMutex mutex;
	mutex.lock();
	if (!m_Instance)
		m_Instance = new Help();
	mutex.unlock();
	return m_Instance;
}
