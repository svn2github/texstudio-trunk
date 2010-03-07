#ifndef LATEXDOCUMENT_H
#define LATEXDOCUMENT_H
#include "mostQtHeaders.h"
#include "bibtexparser.h"

class QDocumentLineHandle;
class LatexEditorView;
class LatexDocument;
class QDocument;
class QDocumentCursor;
struct QDocumentSelection;

/*class References {
public:
	References() {}
	References(QString pattern) : mPattern(pattern) {}
	void insert(QString key,QDocumentLineHandle* handle) {mReferences.insert(key,handle);}
	QList<QDocumentLineHandle*> values(QString key) {return mReferences.values(key);}
	bool contains(QString key) {return mReferences.contains(key);}
	int count(QString key) {return mReferences.count(key);}
	QStringList removeByHandle(QDocumentLineHandle* handle);
	void removeUpdateByHandle(QDocumentLineHandle* handle,References* altRefs=0);
	void updateByKeys(QStringList refs,References* altRefs=0);
	void setPattern(QString pattern) {mPattern=pattern;}
	QString pattern() {return mPattern;}
	void setFormats(int multiple,int single,int none) {
		referenceMultipleFormat=multiple;
		referencePresentFormat=single;
		referenceMissingFormat=none;
	}
	void unite(References others){
		//TODO:
	}
protected:
	QMultiHash<QString,QDocumentLineHandle*> mReferences;
	int referenceMultipleFormat,referencePresentFormat,referenceMissingFormat;
	QString mPattern;
};*/


struct StructureEntry{
	enum Type {SE_DOCUMENT_ROOT,SE_OVERVIEW,SE_SECTION,SE_BIBTEX,SE_TODO,SE_INCLUDE,SE_LABEL,SE_BLOCK=SE_LABEL};
	Type type;
	QString title;
	int level; //only used for section types
	int lineNumber;
	QDocumentLineHandle* lineHandle;
	QList<StructureEntry*> children;
	StructureEntry* parent;
	LatexDocument* document;
	bool appendix;

	StructureEntry(LatexDocument* doc, Type newType);
	StructureEntry(LatexDocument* doc, StructureEntry* parent, Type newType);
	~StructureEntry();
	void add(StructureEntry* child);
	void insert(int pos, StructureEntry* child);

	int getRealLineNumber() const;
};

//iterator for fast traversal of a structure entry tree
class StructureEntryIterator{
public:
	StructureEntryIterator(StructureEntry* entry);
	bool hasNext();
	StructureEntry* next();
private:
	QList<StructureEntry*> entryHierarchy; //hierarchy of next element (all parents and element itself)
	QList<int> indexHierarchy; //for every element in entryHierarchy the index of this element in its parent children
};

class LatexDocument: public QObject
{
	Q_OBJECT
public:
	LatexDocument();
	~LatexDocument();

	void setFileName(const QString& fileName);
	void setEditorView(LatexEditorView* edView);
	LatexEditorView *getEditorView();
	QString getFileName();
	QFileInfo getFileInfo();
	//QSet<QString> texFiles; //absolute file names, also contains fileName

//	References containedLabels,containedReferences;
	const QStringList mentionedBibTeXFiles() const{
	    return mMentionedBibTeXFiles.values();
	}

//	QMap<QString,DocumentLine> mentionedBibTeXFiles; //bibtex files imported in the tex file (absolute after updateBibFiles)
//	QSet<QString> allBibTeXIds;

	QStringList labelItem() const{
	    return mLabelItem.values();
	}
	const QStringList userCommandList() const{
	    return mUserCommandList.values();
	}
	//void includeDocument(LatexDocument* includedDocument);

	//QString getAbsoluteFilePath(const QString& relativePath); //returns the absolute file path for an included file

	StructureEntry* baseStructure;


	QDocumentSelection sectionSelection(StructureEntry* section);
private:
	QString fileName; //absolute
	QFileInfo fileInfo;

	LatexEditorView* edView;
	QDocument* text;

	StructureEntry* labelList;
	StructureEntry* todoList;
	StructureEntry* bibTeXList;
	StructureEntry* blockList;

	QMultiHash<QDocumentLineHandle*,QString>mLabelItem;
	QMultiHash<QDocumentLineHandle*,QString>mMentionedBibTeXFiles;
	QMultiHash<QDocumentLineHandle*,QString>mUserCommandList;

	QDocumentLineHandle *mAppendixLine;

public slots:
	void updateStructure();
	void patchStructure(int linenr, int count);
	void patchStructureRemoval(QDocumentLineHandle* dlh);
	void clearStructure();

signals:
	void hasBeenIncluded(const LatexDocument& newMasterDocument);
	void structureUpdated(LatexDocument* document);
	void structureLost(LatexDocument* document);
	void removeElement(StructureEntry *se,int row);
	void addElement(StructureEntry *se,int row);
};

class LatexDocuments;
class LatexDocumentsModel: public QAbstractItemModel{
	Q_OBJECT
private:
	LatexDocuments& documents;
	QIcon iconDocument, iconMasterDocument, iconBibTeX, iconInclude;
	QVector<QIcon> iconSection;
	QModelIndex mHighlightIndex;

public:
	LatexDocumentsModel(LatexDocuments& docs);
	Qt::ItemFlags flags ( const QModelIndex & index ) const;
	QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
	QVariant headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
	int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
	int columnCount ( const QModelIndex & parent = QModelIndex() ) const;
	QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
	QModelIndex index ( StructureEntry* entry ) const;
	QModelIndex parent ( const QModelIndex & index ) const;

	static StructureEntry* indexToStructureEntry(const QModelIndex & index );
	QModelIndex highlightedEntry();
	void setHighlightedEntry(StructureEntry* entry);

	void resetAll();
private slots:
	void structureUpdated(LatexDocument* document);
	void structureLost(LatexDocument* document);
	void removeElement(StructureEntry *se,int row);
	void addElement(StructureEntry *se,int row);

	friend class LatexDocuments;
};

class LatexDocuments
{
public:
	LatexDocumentsModel* model;
	LatexDocument* masterDocument;
	LatexDocument* currentDocument;
	QList<LatexDocument*> documents;

	LatexDocuments();
	~LatexDocuments();
	void addDocument(LatexDocument* document);
	void deleteDocument(LatexDocument* document);
	void setMasterDocument(LatexDocument* document);

	QString getCurrentFileName(); //returns the absolute file name of the current file or "" if none is opened
	QString getCompileFileName(); //returns the absolute file name of the file to be compiled (master or current)
	QString getAbsoluteFilePath(const QString & relName, const QString &extension="");

	LatexDocument* findDocument(const QString& fileName);

	void settingsRead();

	//support for included BibTeX-files
	QMap<QString, BibTeXFileInfo> bibTeXFiles; //bibtex files loaded by tmx
	bool bibTeXFilesModified; //true iff the BibTeX files were changed after the last compilation
	QStringList mentionedBibTeXFiles; //bibtex files imported in the tex file (absolute after updateBibFiles)
	QSet<QString> allBibTeXIds;
	void updateBibFiles();
};

void findStructureEntryBefore(QMutableListIterator<StructureEntry*> &iter,int linenr,int count);
void splitStructure(StructureEntry* se,QVector<StructureEntry*> &parent_level,QVector<QList<StructureEntry*> > &remainingChildren,QMap<StructureEntry*,int> &toBeDeleted,QMultiHash<QDocumentLineHandle*,StructureEntry*> &MapOfElements,int linenr,int count);

#endif // LATEXDOCUMENT_H
