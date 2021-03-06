/*-------------------------------GPL-------------------------------------//
//
// MetOcean Viewer - A simple interface for viewing hydrodynamic model data
// Copyright (C) 2018  Zach Cobell
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//-----------------------------------------------------------------------*/
#include "noaacoops.h"
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

NoaaCoOps::NoaaCoOps(Station &station, QDateTime startDate, QDateTime endDate,
                     QString product, QString datum, QString units,
                     QObject *parent)
    : WaterData(station, startDate, endDate, parent) {
  this->m_product = product;
  this->m_units = units;
  this->m_datum = datum;
  this->m_useJson = true;
  this->parseProduct();
}

int NoaaCoOps::parseProduct() {
  this->m_productParsed = this->m_product.split(":");
  return 0;
}

int NoaaCoOps::retrieveData(Hmdf *data) {
  QVector<QDateTime> startDateList, endDateList;
  QVector<QString> rawNoaaData;
  int ierr = this->generateDateRanges(startDateList, endDateList);
  if (ierr != 0) return ierr;
  ierr =
      this->downloadDataFromNoaaServer(startDateList, endDateList, rawNoaaData);
  if (ierr != 0) return ierr;
  ierr = this->formatNoaaResponse(rawNoaaData, data);
  if (ierr != 0) return ierr;
  return 0;
}

int NoaaCoOps::generateDateRanges(QVector<QDateTime> &startDateList,
                                  QVector<QDateTime> &endDateList) {
  int numDownloads = (this->startDate().daysTo(this->endDate()) / 30) + 1;

  this->startDate().setTime(QTime(this->startDate().time().hour(),
                                  this->startDate().time().minute(), 0));
  this->endDate().setTime(
      QTime(this->endDate().time().hour(), this->endDate().time().minute(), 0));

  // Build the list of dates in 30 day intervals
  for (int i = 0; i < numDownloads; i++) {
    QDateTime startDate;
    if (i == 0)
      startDate = this->startDate().addDays(i * 30).addDays(i);
    else
      startDate = endDateList.last();

    QDateTime endDate = startDate.addDays(30);
    if (endDate > this->endDate()) endDate = this->endDate();
    startDateList.push_back(startDate);
    endDateList.push_back(endDate);
  }
  return 0;
}

int NoaaCoOps::downloadDataFromNoaaServer(QVector<QDateTime> startDateList,
                                          QVector<QDateTime> endDateList,
                                          QVector<QString> &downloadedData) {
  QNetworkAccessManager *manager = new QNetworkAccessManager(this);

  for (int i = 0; i < startDateList.length(); i++) {
    // Make the date string
    QString startString =
        startDateList[i].toString(QStringLiteral("yyyyMMdd hh:mm"));
    QString endString =
        endDateList[i].toString(QStringLiteral("yyyyMMdd hh:mm"));

    //...Select parser type
    QString format;
    if (this->m_useJson) {
      format = "json";
    } else {
      format = "csv";
    }

    // Build the URL to request data from the NOAA CO-OPS API
    QString requestURL =
        QStringLiteral("http://tidesandcurrents.noaa.gov/api/datagetter?") +
        QStringLiteral("product=") + this->m_productParsed[0] +
        QStringLiteral("&application=metoceanviewer") +
        QStringLiteral("&begin_date=") + startString +
        QStringLiteral("&end_date=") + endString + QStringLiteral("&station=") +
        this->station().id() + QStringLiteral("&time_zone=GMT&units=") +
        this->m_units + QStringLiteral("&interval=&format=") + format;

    // Allow a different datum where allowed
    if (this->m_datum != QStringLiteral("Stnd"))
      requestURL = requestURL + QStringLiteral("&datum=") + this->m_datum;

    // Send the request
    QEventLoop loop;
    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(requestURL)));
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop,
            SLOT(quit()));
    loop.exec();

    //...Check for a redirect from NOAA. This fixes bug #26
    QVariant redirectionTargetURL =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (!redirectionTargetURL.isNull()) {
      QNetworkReply *reply2 =
          manager->get(QNetworkRequest(redirectionTargetURL.toUrl()));
      connect(reply2, SIGNAL(finished()), &loop, SLOT(quit()));
      connect(reply2, SIGNAL(error(QNetworkReply::NetworkError)), &loop,
              SLOT(quit()));
      loop.exec();
      reply->deleteLater();
      this->readNoaaResponse(reply2, downloadedData);
    } else
      this->readNoaaResponse(reply, downloadedData);
  }

  return 0;
}

int NoaaCoOps::readNoaaResponse(QNetworkReply *reply,
                                QVector<QString> &downloadedData) {
  // Catch some errors during the download
  if (reply->error() != 0) {
    this->setErrorString(QStringLiteral("ERROR: ") + reply->errorString());
    reply->deleteLater();
    return 1;
  }

  // Store the data at the back of the vector
  downloadedData.push_back((QString)reply->readAll());

  // Delete this response
  reply->deleteLater();

  return 0;
}

int NoaaCoOps::formatNoaaResponse(QVector<QString> &downloadedData,
                                  Hmdf *outputData) {
  if (this->m_useJson) {
    return this->formatNoaaResponseJson(downloadedData, outputData);
  } else {
    return this->formatNoaaResponseCsv(downloadedData, outputData);
  }
}

int NoaaCoOps::formatNoaaResponseCsv(QVector<QString> &downloadedData,
                                     Hmdf *outputData) {
  QVector<QStringList> data;
  QString error;

  data.resize(downloadedData.size());

  for (int i = 0; i < downloadedData.size(); i++) {
    data[i] = QString(downloadedData[i])
                  .split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
    error = QString(downloadedData[i]) + QStringLiteral("\n");
  }

  HmdfStation *station = new HmdfStation(outputData);
  station->setCoordinate(this->station().coordinate());
  station->setName(this->station().name());
  station->setId(this->station().id());
  station->setStationIndex(0);

  QDateTime tempDate = QDateTime();
  tempDate.setTimeSpec(Qt::UTC);

  for (int i = 0; i < data.size(); i++) {
    if (data[i].size() > 3) {
      for (int j = 1; j < data[i].size(); j++) {
        const QRegExp rx("-|:");
        QStringList t = data[i][j].split(",");
        QStringList d = t[0].replace(rx, " ").split(" ");

        int year = d[0].toInt();
        int month = d[1].toInt();
        int day = d[2].toInt();
        int hour = d[3].toInt();
        int minute = d[4].toInt();

        double value;
        if (this->m_productParsed.size() > 1) {
          if (this->m_productParsed[1] == "speed") {
            value = t[1].toDouble();
          } else if (this->m_productParsed[1] == "direction") {
            value = t[2].toDouble();
          } else if (this->m_productParsed[1] == "gusts") {
            value = t[4].toDouble();
          }
        } else {
          value = t[1].toDouble();
        }

        //...Remove duplicates when sets are downloaded back to back
        if (QDateTime(QDate(year, month, day), QTime(hour, minute, 0)) ==
            tempDate)
          continue;

        tempDate = QDateTime(QDate(year, month, day), QTime(hour, minute, 0));
        if (value != 0.0 && tempDate.isValid()) {
          station->setNext(tempDate.toMSecsSinceEpoch(), value);
        }
      }
    }
  }
  outputData->addStation(station);

  if (outputData->station(0)->numSnaps() < 5) {
    this->setErrorString(QStringLiteral("No valid data was found."));
    return 1;
  }

  return 0;
}

int NoaaCoOps::formatNoaaResponseJson(QVector<QString> &downloadedData,
                                      Hmdf *outputData) {
  HmdfStation *station = new HmdfStation(outputData);
  station->setCoordinate(this->station().coordinate());
  station->setName(this->station().name());
  station->setId(this->station().id());
  station->setStationIndex(0);

  for (int i = 0; i < downloadedData.length(); i++) {
    QString data = downloadedData[i];
    QJsonDocument jsonData = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = jsonData.object();

    QJsonArray jsonArr;
    if (jsonObj.contains("data"))
      jsonArr = jsonObj["data"].toArray();
    else if (jsonObj.contains("predictions"))
      jsonArr = jsonObj["predictions"].toArray();
    else {
      QJsonValue val = jsonData.object()["error"];
      QJsonObject obj = val.toObject();
      QJsonValue val2 = obj["message"];
      this->setErrorString(val2.toString());
    }

    //...Ditch duplicate data
    int start;
    if (i == 0)
      start = 0;
    else
      start = 1;

    for (int j = start; j < jsonArr.size(); j++) {
      QJsonObject obj = jsonArr[j].toObject();
      QDateTime t =
          QDateTime::fromString(obj["t"].toString(), "yyyy-MM-dd hh:mm");
      t.setTimeSpec(Qt::UTC);
      bool ok;
      double v = obj["v"].toString().toDouble(&ok);
      if (t.isValid() && ok) {
        station->setNext(t.toMSecsSinceEpoch(), v);
      }
    }
  }

  if (station->numSnaps() > 3) {
    outputData->addStation(station);
    return 0;
  } else {
    return 1;
  }
}
