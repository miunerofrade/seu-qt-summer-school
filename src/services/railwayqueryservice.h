#ifndef RAILWAYQUERYSERVICE_H
#define RAILWAYQUERYSERVICE_H

#include "services/queryservice.h"

#include <QObject>

#include <functional>

class QNetworkAccessManager;

struct RailwayStation
{
    QString name;
    QString code;
};

class RailwayQueryService final : public QObject
{
public:
    using QueryCallback = std::function<void(QVector<TrainQueryRow>, const QString &)>;
    using StationsCallback = std::function<void(QVector<RailwayStation>, const QString &)>;

    explicit RailwayQueryService(const QString &cacheDirectory, QObject *parent = nullptr);

    void query(const QString &fromCode,
               const QString &toCode,
               const QDate &date,
               QueryCallback callback);
    void refreshStations(StationsCallback callback);

    QVector<RailwayStation> cachedStations() const;
    QVector<TrainQueryRow> cachedAll() const;
    QVector<TrainQueryRow> cachedAvailable(const QDateTime &now) const;
    QVector<TrainQueryRow> cachedQuery(const QString &fromCode,
                                       const QString &toCode,
                                       const QDate &date,
                                       const QDateTime &now) const;

    static bool parseTicketResponse(const QByteArray &payload,
                                    const QDate &queryDate,
                                    QVector<TrainQueryRow> *rows,
                                    QString *error);
    static QVector<RailwayStation> parseStationCatalog(const QByteArray &payload);

private:
    QByteArray cacheContents() const;
    bool updateCache(const std::function<void(class QJsonObject *)> &update) const;
    void saveQuery(const QString &fromCode,
                   const QString &toCode,
                   const QDate &date,
                   const QVector<TrainQueryRow> &rows) const;
    void saveStations(const QVector<RailwayStation> &stations) const;

    QString m_cachePath;
    QString m_stationCatalogPath;
    QNetworkAccessManager *m_network;
};

#endif // RAILWAYQUERYSERVICE_H
