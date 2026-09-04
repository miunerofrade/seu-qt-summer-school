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

struct RailwayRouteStop
{
    QString name;
    QString code;
    QTime arrivalTime;
    QTime departureTime;
    int dayOffset = 0;
};

class RailwayQueryService final : public QObject
{
public:
    using QueryCallback = std::function<void(QVector<TrainQueryRow>, const QString &)>;
    using StationsCallback = std::function<void(QVector<RailwayStation>, const QString &)>;
    using RouteCallback = std::function<void(QVector<RailwayRouteStop>, const QString &)>;

    explicit RailwayQueryService(const QString &cacheDirectory, QObject *parent = nullptr);

    void query(const QString &fromCode,
               const QString &toCode,
               const QDate &date,
               QueryCallback callback);
    void refreshStations(StationsCallback callback);
    void queryRoute(const TrainQueryRow &row, RouteCallback callback);

    QVector<RailwayStation> cachedStations() const;
    bool stationCatalogNeedsRefresh(const QDateTime &now = QDateTime::currentDateTime()) const;
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
    static bool parseRouteResponse(const QByteArray &payload,
                                   const QVector<RailwayStation> &stations,
                                   QVector<RailwayRouteStop> *stops,
                                   QString *error);

private:
    QByteArray cacheContents() const;
    bool updateCache(const std::function<void(class QJsonObject *)> &update) const;
    void saveQuery(const QString &fromCode,
                   const QString &toCode,
                   const QDate &date,
                   const QVector<TrainQueryRow> &rows) const;
    void saveStations(const QVector<RailwayStation> &stations) const;
    void saveRoute(const TrainQueryRow &row, const QVector<RailwayRouteStop> &stops) const;
    QVector<RailwayRouteStop> cachedRoute(const TrainQueryRow &row) const;

    QString m_cachePath;
    QString m_stationCatalogPath;
    QNetworkAccessManager *m_network;
};

#endif // RAILWAYQUERYSERVICE_H
