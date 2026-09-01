#ifndef JSONREPOSITORY_H
#define JSONREPOSITORY_H

#include "data/idatarepository.h"

class JsonRepository final : public IDataRepository
{
public:
    explicit JsonRepository(QString filePath);

    LoadResult load() const override;
    OperationResult save(const domain::AppData &data) const override;
    QString dataFilePath() const override;

private:
    QString m_filePath;
};

#endif // JSONREPOSITORY_H
