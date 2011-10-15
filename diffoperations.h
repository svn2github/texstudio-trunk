#ifndef DIFFOPERATIONS_H
#define DIFFOPERATIONS_H

struct DiffOp {
    enum DiffType{
        Insert,
        Delete,
        Replace
    };

    int start,length;
    DiffType type;
    QString text;
};

typedef QList<DiffOp> DiffList;

Q_DECLARE_METATYPE(DiffList);

#endif // DIFFOPERATIONS_H
