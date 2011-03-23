#ifndef GOOGLELOCATION_H
#define GOOGLELOCATION_H

#include <QObject>
#include <QtDBus>
#include <QNetworkReply>
class QNetworkAccessManager;
class QTimer;

class GoogleLocation : public QObject
{
    Q_OBJECT
public:
    /* minInterval and maxInterval are values in seconds.
       No positon updates are triggert until minInterval is reached.
       A position update is performed not later than maxInterval. */
    explicit GoogleLocation(uint minInterval = 0, uint maxInterval = 0, QObject *parent = 0);
    virtual ~GoogleLocation();
signals:
    void newLocation();
    void newLocation(double latitude, double longitude, double accuracy);
private:
    QNetworkAccessManager *accessManager;
    QTimer *timer;
    int cellID;
    int lac;
    int mnc;
    int mcc;
    uint minInterval;
    uint maxInterval;
    uint lastLookup;
    double latitude;
    double longitude;
    double accuracy;
    QString mapToString(QMap<QString, QVariant> map);
    QString listToString(QList<QVariant> list);
public slots:
    void setMinInterval(uint minInterval);
    uint getMinInterval() const;
    void setMaxInterval(uint maxInterval);
    uint getMaxInterval() const;
    double getLatitude() const;
    double getLongitude() const;
    double getAccuracy() const;
    void refreshPosition();
private slots:
    void cellIDChanged(const QDBusMessage &message);
    void scanWlan();
    void wlanScanResult(const QDBusMessage &message);
    void serverReply(QNetworkReply *reply);
};

#endif // GOOGLELOCATION_H
