#include "scriptobject.h"

#include "smallUsefulFunctions.h"
#include "qcryptographichash.h"
#include "configmanagerinterface.h"
#include "buildmanager.h"
#include "QtUiTools/QUiLoader"
#include "QBuffer"
QStringList privilegedReadScripts, privilegedWriteScripts;
int readSecurityMode, writeSecurityMode;

ScriptObject::ScriptObject(const QString& script, BuildManager* buildManager, Texmaker* app): backgroundScript(false), script(script), buildManager(buildManager), app(app)
{
	ConfigManagerInterface::getInstance()->registerOption("Scripts/Privileged Read Scripts", &privilegedReadScripts);	
	ConfigManagerInterface::getInstance()->registerOption("Scripts/Read Security Mode", &readSecurityMode, 1);	
	ConfigManagerInterface::getInstance()->registerOption("Scripts/Privileged Write Scripts", &privilegedWriteScripts);	
	ConfigManagerInterface::getInstance()->registerOption("Scripts/Write Security Mode", &writeSecurityMode, 1);	
	this->subScriptObject.script = this;
}

void ScriptObject::alert(const QString& message){ txsInformation(message); }
void ScriptObject::information(const QString& message){	txsInformation(message); }
void ScriptObject::critical(const QString& message){ txsCritical(message); }
void ScriptObject::warning(const QString& message){ txsWarning(message); }
bool ScriptObject::confirm(const QString& message){ return txsConfirm(message); }
bool ScriptObject::confirmWarning(const QString& message){ return txsConfirmWarning(message); }
void ScriptObject::debug(const QString& message){ qDebug() << message; }

#ifndef QT_NO_DEBUG
void ScriptObject::crash_assert(){Q_ASSERT(false);}
void ScriptObject::crash_sigsegv(){char *c = 0; *c = 'A';}
int global0 = 0;
void ScriptObject::crash_sigfpe(){int x = 1 / global0;  Q_UNUSED(x);}
void ScriptObject::crash_stack(){ int temp = global0; crash_stack(); Q_UNUSED(temp);}
void ScriptObject::crash_loop(){ 
	register int a = 1, b = 2, c = 3, d = 4;
	while (1) {
		void * x = malloc(16); free(x);  
		Q_ASSERT(a == 1); Q_ASSERT(b == 2); Q_ASSERT(c == 3); Q_ASSERT(d == 4); //make sure, no register suddenly change
	}; 
}
#endif

ProcessX* ScriptObject::system(const QString& commandline){
	if (!buildManager || !needWritePrivileges("system",commandline))
		return 0;
	ProcessX* p = 0;
	if (commandline.contains(BuildManager::TXS_CMD_PREFIX) || !commandline.contains("|")) 
		p = buildManager->firstProcessOfDirectExpansion(commandline, QFileInfo());
	else
		p = buildManager->newProcessInternal(commandline, QFileInfo()); //use internal, so people can pass | to sh
	if (!p) return 0;
	connect(p, SIGNAL(finished(int)), p, SLOT(deleteLater()));
	p->startCommand();
	p->waitForStarted();
	return p;
}

void ScriptObject::writeFile(const QString& filename, const QString& content){
	if (!needWritePrivileges("writeFile",filename))
		return;
	QFile f(filename);
	f.open(QFile::WriteOnly);
	f.write(content.toUtf8());
	f.close();
}

QVariant ScriptObject::readFile(const QString& filename){
	if (!needReadPrivileges("readFile",filename))
		return QVariant();
	QFile f(filename);
	if (!f.open(QFile::ReadOnly)) 
		return QVariant();
	QTextStream ts(&f);
	ts.setAutoDetectUnicode(true);
	ts.setCodec(QTextCodec::codecForMib(MIB_UTF8));
	return ts.readAll();
}

static QHash<QString, QVariant> globalGlobals;

bool ScriptObject::hasGlobal(const QString& name){ 
	return globalGlobals.contains(name); 
}
void ScriptObject::setGlobal(const QString& name, const QVariant& value){ 
	globalGlobals.insert(name,value);
}
QVariant ScriptObject::getGlobal(const QString& name){ 
	return globalGlobals.value(name); 
}

bool ScriptObject::hasPersistent(const QString& name){
	return ConfigManagerInterface::getInstance()->existsOption(name);
}
void ScriptObject::setPersistent(const QString& name, const QVariant& value){
	if (!needWritePrivileges("setPersistent",name+"="+value.toString())) return;
	ConfigManagerInterface::getInstance()->setOption(name, value);
}
QVariant ScriptObject::getPersistent(const QString& name){
	if (!needReadPrivileges("getPersistent",name)) return QVariant();
	return ConfigManagerInterface::getInstance()->getOption(name);
}


void ScriptObject::registerAsBackgroundScript(const QString& name){
	static QMap<QString, QPointer<ScriptObject> > backgroundScripts;
	
	QString realName = name.isEmpty() ? getScriptHash() : name;
	if (!backgroundScripts.value(realName, QPointer<ScriptObject>(0)).isNull()) 
		delete backgroundScripts.value(realName, QPointer<ScriptObject>(0)).data();
	backgroundScripts.insert(realName, this);
	backgroundScript = true;
}
QWidget* ScriptObject::createUI(const QString& path, QWidget* parent){
	QFile f(path);
	if (!f.open(QFile::ReadOnly)) return 0;
	return QUiLoader().load(&f, parent);
}
QWidget* ScriptObject::createUIFromString(const QString& str, QWidget* parent){
	QByteArray ba(str.toLocal8Bit());
	QBuffer buffer(&ba);
	return QUiLoader().load(&buffer, parent);
}

QByteArray SubScriptObject::getScriptHash(){
	Q_ASSERT(script);
	return script->getScriptHash();
}

static QHash<QString, QVariant> globals;

bool SubScriptObject::hasGlobal(const QString& name){
	return globals.contains(getScriptHash()+"/"+name);
}

void SubScriptObject::setGlobal(const QString& name, const QVariant& value){
	globals.insert(getScriptHash()+"/"+name, value);
}
QVariant SubScriptObject::getGlobal(const QString& name){
	return globals.value(getScriptHash()+"/"+name, QVariant());
}

static QHash<QString, QVariant*> persistents;

bool SubScriptObject::hasPersistent(const QString& name){
	QString id = getScriptHash()+"/"+name;
	if (persistents.contains(id)) return true;
	return ConfigManagerInterface::getInstance()->existsOption("Script Data/"+id);
}

void SubScriptObject::setPersistent(const QString& name, const QVariant& value){
	QString id = getScriptHash()+"/"+name;
	if (!persistents.contains(id)) {
		QVariant* v = new QVariant;
		persistents.insert(id, v);
		ConfigManagerInterface::getInstance()->registerOption("Script Data/"+id, v);
	}
	*persistents.value(id) = value;
}
QVariant SubScriptObject::getPersistent(const QString& name){
	if (!hasPersistent(name)) return QVariant();
	QString id = getScriptHash()+"/"+name;
	if (!persistents.contains(id)) {
		QVariant* v = new QVariant;
		persistents.insert(id, v);
		ConfigManagerInterface::getInstance()->registerOption("Script Data/"+id, v);
	}
	return *persistents.value(id);
}

bool ScriptObject::hasReadPrivileges(){
	if (readSecurityMode == 0) 
		return false;
	if (readSecurityMode == 2
	    || privilegedReadScripts.contains(getScriptHash())
	    || privilegedWriteScripts.contains(getScriptHash())) 
		return true;
	return false;
	
}

bool ScriptObject::hasWritePrivileges(){
	if (writeSecurityMode == 0) 
		return false;
	if (writeSecurityMode == 2
	    || privilegedWriteScripts.contains(getScriptHash())) 
		return true;
	return false;
	
}

QByteArray ScriptObject::getScriptHash(){
	if (scriptHash.isEmpty()) 
		scriptHash = QCryptographicHash::hash(script.toLatin1(), QCryptographicHash::Sha1).toHex();
	return scriptHash;
}

bool ScriptObject::needWritePrivileges(const QString& fn, const QString& param){
	if (writeSecurityMode == 0) return false;
	if (hasWritePrivileges()) return true;
	int t = QMessageBox::question(0, "TeXstudio script watcher", 
	                              tr("The current script has requested to enter privileged write mode and call following function:\n%1\n\nDo you trust this script?").arg(fn+"(\""+param+"\")"), tr("Yes, allow this call"), 
	                              tr("Yes, allow all calls it will ever make"), tr("No, abort the call"), 0, 2);
	if (t == 0) return true; //only now
	if (t != 1) return false;
	privilegedWriteScripts.append(getScriptHash());
	return true;
}

bool ScriptObject::needReadPrivileges(const QString& fn, const QString& param){
	if (readSecurityMode == 0) return false;
	if (hasReadPrivileges()) return true;
	int t = QMessageBox::question(0, "TeXstudio script watcher", 
	                              tr("The current script has requested to enter privileged mode and read the following value:\n%1\n\nDo you trust this script?").arg(fn+"(\""+param+"\")"), tr("Yes, allow this reading"), 
	                              tr("Yes, grant permanent read access to everything"), tr("No, abort the call"), 0, 2);
	if (t == 0) return true; //only now
	if (t != 1) return false;
	privilegedReadScripts.append(getScriptHash());
	return true;
}

SubScriptObject* ScriptObject::getScript(){
	return &subScriptObject;
}
Texmaker* ScriptObject::getApp(){
	return app;
}
