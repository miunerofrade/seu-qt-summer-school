#ifndef IDATAREPOSITORY_H
#define IDATAREPOSITORY_H

#include "common/result.h"
#include "domain/entities.h"

#include <QString>

struct LoadResult
{
    bool success = false;
    bool fileMissing = false;
    domain::AppData data;
    QString error;
};

class IDataRepository
{
public:
    virtual ~IDataRepository() = default;

    virtual LoadResult load() const = 0;
    virtual OperationResult save(const domain::AppData &data) const = 0;
    virtual QString dataFilePath() const = 0;
};

#endif // IDATAREPOSITORY_H
