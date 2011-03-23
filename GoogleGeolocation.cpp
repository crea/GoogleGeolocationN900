
#include "GoogleGeolocation.h"
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
GoogleLocation::GoogleLocation(uint minInterval, uint maxInterval, QObject *parent) : QObject(parent)
{
    cellID=0;
    mnc=0;
    lac=0;
    mcc=0;
    lastLookup=0;
    latitude=0;
    longitude=0;
    accuracy=0;
    this->minInterval = minInterval;
    this->maxInterval = maxInterval;
    timer = new QTimer();
    if(maxInterval > 0)
    {
        timer->setInterval(maxInterval*1000);
        timer->start();
    }
    connect(timer, SIGNAL(timeout()), this, SLOT(refreshPosition()));
    QDBusConnection dbusConnection = QDBusConnection::systemBus();
    dbusConnection.connect("", "", "Phone.Net", "cell_info_change", this, SLOT(cellIDChanged(const QDBusMessage &)));
    dbusConnection.connect("", "", "com.nokia.wlancond.signal", "scan_results", this, SLOT(wlanScanResult(const QDBusMessage &)));
    accessManager = new QNetworkAccessManager();
    connect(accessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(serverReply(QNetworkReply*)));
}

void GoogleLocation::setMinInterval(uint minInterval)
{
    this->minInterval = minInterval;
}

uint GoogleLocation::getMinInterval() const
{
    return minInterval;
}

void GoogleLocation::setMaxInterval(uint maxInterval)
{
    this->maxInterval = maxInterval;
    if(maxInterval > 0)
    {
        timer->setInterval(maxInterval*1000);
        timer->start();
    }
    else
    {
        timer->stop();
    }
}

uint GoogleLocation::getMaxInterval() const
{
    return maxInterval;
}

double GoogleLocation::getLatitude() const
{
    return latitude;
}

double GoogleLocation::getLongitude() const
{
    return longitude;
}

double GoogleLocation::getAccuracy() const
{
    return accuracy;
}

void GoogleLocation::refreshPosition()
{
    lastLookup=0;
    QDBusMessage msg = QDBusMessage::createMethodCall("com.nokia.phone.net", "/com/nokia/phone/net", "Phone.Net", "get_registration_status");
    cellIDChanged(QDBusConnection::systemBus().call(msg));
}

void GoogleLocation::cellIDChanged(const QDBusMessage &message)
{
    lac = message.arguments().at(1).toInt();
    int newCellID = message.arguments().at(2).toInt();
    mnc = message.arguments().at(3).toInt();
    mcc = message.arguments().at(4).toInt();
    if(cellID != newCellID && QDateTime::currentDateTime().toTime_t() - lastLookup > minInterval)
    {
        lastLookup = QDateTime::currentDateTime().toTime_t();
        timer->start();
        scanWlan();
    }
    cellID = newCellID;
}

void GoogleLocation::scanWlan()
{
    QDBusMessage msg = QDBusMessage::createMethodCall("com.nokia.wlancond", "/com/nokia/wlancond/request", "com.nokia.wlancond.request", "scan");
    QList<QVariant> args;
    args.append(int(4));
    args.append(QByteArray());
    args.append(int(2));
    msg.setArguments(args);
    QDBusConnection::systemBus().call(msg);
}

void GoogleLocation::wlanScanResult(const QDBusMessage &message)
{
    int numOfAPNs = message.arguments().at(0).toInt();
    QList<QVariant> wifiTowers;
    for(int i = 1; i < 5*numOfAPNs; i = i+5)
    {
        QMap<QString, QVariant> wifiTower;
        wifiTower.insert("ssid", message.arguments().at(i).toString());
        wifiTower.insert("age", 0);
        wifiTower.insert("signal_strength", QString::number(message.arguments().at(i+2).toInt()));
        wifiTower.insert("channel", QString::number(message.arguments().at(i+3).toInt()));
        QByteArray macAddr = message.arguments().at(i+1).toByteArray();
        wifiTower.insert("mac_address", QString::number(macAddr.at(0), 16) + "-" + QString::number(macAddr.at(1), 16) + "-" + QString::number(macAddr.at(2), 16) + "-"
                         + QString::number(macAddr.at(3), 16) + "-" + QString::number(macAddr.at(4), 16) + "-" + QString::number(macAddr.at(5), 16));
        wifiTowers.append(wifiTower);
    }
    QMap<QString, QVariant> data;
    data.insert("version", "1.1.0");
    data.insert("host", "maps.google.com");
    data.insert("home_mobile_country_code", mcc);
    data.insert("home_mobile_network_code", mnc);
    data.insert("radio_type", mnc);
    QMap<QString, QVariant> cellTower;
    cellTower.insert("cell_id", cellID);
    cellTower.insert("location_area_code", lac);
    cellTower.insert("mobile_country_code", mcc);
    cellTower.insert("mobile_network_code", mnc);
    cellTower.insert("age", 0);
    QList<QVariant> cellTowers;
    cellTowers.append(cellTower);
    data.insert("cell_towers", cellTowers);
    data.insert("wifi_towers", wifiTowers);
    QNetworkRequest request(QUrl("http://www.google.com/loc/json"));
    accessManager->post(request, mapToString(data).toUtf8());
}

void GoogleLocation::serverReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    QRegExp regexp ("\\{\"latitude\":(.*),\"longitude\":(.*),\"accuracy\":(.*)\\}");
    regexp.setMinimal(1);
    regexp.indexIn(data, 1);
    latitude = regexp.capturedTexts().at(1).toDouble();
    longitude = regexp.capturedTexts().at(2).toDouble();
    accuracy = regexp.capturedTexts().at(3).toDouble();
    if(QDateTime::currentDateTime().toTime_t() - lastLookup > minInterval)
    {
        emit newLocation();
        emit newLocation(latitude, longitude, accuracy);
    }
}

GoogleLocation::~GoogleLocation()
{
    delete accessManager;
    delete timer;
}

/* helper to generate json data */

QString GoogleLocation::mapToString(QMap<QString, QVariant> map)
{
    QString result;
    result.append("{\n");
    QMapIterator<QString, QVariant> i(map);
    while(i.hasNext())
    {
        i.next();
        bool ok;
        i.value().toInt(&ok);
        if(i.value().canConvert(QVariant::Map))
        {
            result.append("\"" + i.key() + "\"" + " : " + mapToString(i.value().toMap()));
        }
        else if(i.value().canConvert(QVariant::List))
        {
            result.append("\"" + i.key() + "\"" + " : " + listToString(i.value().toList()));
        }
        else if(ok)
        {
            result.append("\"" + i.key() + "\"" + " : " + i.value().toString());
        }
        else
        {
            result.append("\"" + i.key() + "\"" + " : " + "\"" + i.value().toString() + "\"");
        }
        if(i.hasNext())
        {
            result.append(",\n");
        }
    }
    result.append("\n}");
    return result;
}

QString GoogleLocation::listToString(QList<QVariant> list)
{
    QString result;
    result.append("[\n");
    QListIterator<QVariant> i(list);
    while(i.hasNext())
    {
        if(i.peekNext().canConvert(QVariant::Map))
        {
            result.append(mapToString(i.peekNext().toMap()));
        }
        else if(i.peekNext().canConvert(QVariant::List))
        {
            result.append(listToString(i.peekNext().toList()));
        }
        if(i.hasNext())
        {
            result.append(",\n");
        }
        i.next();
    }
    result.append("\n]");
    return result;
}
