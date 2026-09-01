#ifndef OPERATIONRESULT_H
#define OPERATIONRESULT_H

#include <QString>

struct OperationResult
{
    bool success = false;
    QString error;

    static OperationResult ok()
    {
        return {true, {}};
    }

    static OperationResult failure(const QString &message)
    {
        return {false, message};
    }

    explicit operator bool() const
    {
        return success;
    }
};

#endif // OPERATIONRESULT_H
