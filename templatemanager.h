#ifndef TEMPLATEMANAGER_H
#define TEMPLATEMANAGER_H

#include "mostQtHeaders.h"

class Template;

class TemplateHandle {
public:
	friend class Template;
	TemplateHandle() : m_tmpl(0) {}
	TemplateHandle(const TemplateHandle &th);
	explicit TemplateHandle(Template *tmpl);
	~TemplateHandle();

	TemplateHandle& operator = (const TemplateHandle& th);

	bool isValid() const;
	QString name() const;
	QString description() const;
	QString author() const;
	QString version() const;
	QDate date() const;
	QString license() const;
	QPixmap previewImage() const;
	QString file() const;
	bool isEditable() const;
private:
	void setTmpl(Template *tmpl);
	Template * m_tmpl; // only write via setTmpl()
};
Q_DECLARE_METATYPE( TemplateHandle )


class AbstractTemplateRessource {
public:
	virtual QList<TemplateHandle> getTemplates() = 0;
	virtual bool isAccessible() = 0;
	virtual QString name() = 0;
	virtual QString description() = 0;
	virtual QIcon icon() = 0;
protected:
	AbstractTemplateRessource() {}
};
Q_DECLARE_INTERFACE( AbstractTemplateRessource , "TeXstudio/AbstractTemplateRessource")
Q_DECLARE_METATYPE( AbstractTemplateRessource * )


class TemplateManager : public QObject
{
	Q_OBJECT
public:
	explicit TemplateManager(QObject *parent = 0);

	static void setConfigBaseDir(const QString &dir) { configBaseDir = dir; }
	static QString userTemplateDir() { return configBaseDir + "templates/user/"; }
	static QString builtinTemplateDir();
	static bool ensureUserTemplateDirExists();
	static void checkForOldUserTemplates();

	bool latexTemplateDialogExec();
	bool tableTemplateDialogExec();
	QString selectedTemplateFile() { return selectedFile; }

signals:
	void editRequested(const QString &filename) ;
public slots:

private slots:
	void editTemplate(TemplateHandle th);
	void editTemplateInfo(TemplateHandle th);

private:
	static QString configBaseDir;
	QString selectedFile;
};

#endif // TEMPLATEMANAGER_H
