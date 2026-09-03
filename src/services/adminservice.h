#ifndef ADMINSERVICE_H
#define ADMINSERVICE_H

#include "common/result.h"
#include "domain/entities.h"

class DataStore;

class AdminService final
{
public:
    explicit AdminService(DataStore *dataStore);

    OperationResult addStation(const domain::Station &station);
    OperationResult updateStation(const QString &code,
                                  const QString &name,
                                  const QString &city,
                                  bool enabled);
    OperationResult removeStation(const QString &code);

    OperationResult addTrain(const domain::Train &train);
    OperationResult removeTrain(const QString &number, bool knownFromRailwayCache = false);
    OperationResult replaceStops(const QString &trainNumber,
                                 const QVector<domain::TrainStop> &stops);
    OperationResult replaceSeats(const QString &trainNumber,
                                 const QVector<domain::SeatInventory> &seats);

private:
    DataStore *m_dataStore;
};

#endif // ADMINSERVICE_H
