#ifndef FILTERORDERDOWNCOMMAND_H
#define FILTERORDERDOWNCOMMAND_H
#include "gitlabstractcommand.h"

class FilterOrderDownCommand : public GitlAbstractCommand
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit FilterOrderDownCommand(QObject *parent = 0);

    Q_INVOKABLE bool execute(GitlCommandParameter &rcInputArg, GitlCommandParameter &rcOutputArg);

signals:
    
public slots:
    
};

#endif // FILTERORDERDOWNCOMMAND_H
