#include "maketemplatedialog.h"
#include "ui_maketemplatedialog.h"
#include "smallUsefulFunctions.h"

MakeTemplateDialog::MakeTemplateDialog(QString templateDir, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MakeTemplateDialog),
	m_templateDir(templateDir)
{
	ui->setupUi(this);

	connect(ui->buttonBox, SIGNAL(accepted()), SLOT(tryAccept()));
	connect(ui->buttonBox, SIGNAL(rejected()), SLOT(reject()));

	ui->leVersion->setText("1.0");
}

MakeTemplateDialog::~MakeTemplateDialog()
{
	delete ui;
}

void MakeTemplateDialog::tryAccept()
{
	QString fn = ui->leName->text();
	QString invalidChars = "\\/:*?\"<>|"; // for windows, OSX and linux is less restrictive but we use this to guarantee compatibility
	foreach (const QChar &c, invalidChars)
		fn.remove(c);
	if (fn.length() > 80) {
		fn.remove(80);
	}
	fn.prepend("template_");
	fn.append(".tex");
	m_suggestedFile = QFileInfo(QDir(m_templateDir), fn);
	if (m_suggestedFile.exists()) {
		bool abort = txsConfirmWarning(tr("A template with the given name already exists.\nDo you want to overwrite it?")+"\n"+m_suggestedFile.canonicalFilePath());
		if (abort)
			return;
	}
	accept();
}

QString MakeTemplateDialog::generateMetaData()
{
	QString s = "{\n";
	s += formatJsonStringParam("Title", ui->leName->text(), 13) + ",\n";
	s += formatJsonStringParam("Author", ui->leAuthor->text(), 13) + ",\n";
	s += formatJsonStringParam("Date", QDate::currentDate().toString(Qt::SystemLocaleShortDate), 13) + ",\n";
	s += formatJsonStringParam("Version", ui->leVersion->text(), 13) + ",\n";
	s += formatJsonStringParam("Description", ui->leDescription->toPlainText(), 13) + "\n"; // last entry does not have colon
	s += "}";
	return s;
}
