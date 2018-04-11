/*-------------------------------GPL-------------------------------------//
//
// MetOcean Viewer - A simple interface for viewing hydrodynamic model data
// Copyright (C) 2015  Zach Cobell
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
#ifndef IMEDSSTATION_H
#define IMEDSSTATION_H

#include <QGeoCoordinate>
#include <QVector>

class ImedsStation {
 public:
  explicit ImedsStation();

  QGeoCoordinate coordinate() const;
  void setCoordinate(const QGeoCoordinate &coordinate);

  QString name() const;
  void setName(const QString &name);

  QString id() const;
  void setId(const QString &id);

  int numSnaps() const;
  void setNumSnaps(int numSnaps);

  int stationIndex() const;
  void setStationIndex(int stationIndex);

  qint64 date(int index) const;
  void setDate(const qint64 &date, int index);
  void setDate(const QVector<qint64> &date);

  void setNext(const qint64 &date, const double &data);

  bool isNull() const;
  void setIsNull(bool isNull);

  double data(int index) const;
  void setData(const double &data, int index);
  void setData(const QVector<double> &data);

  QVector<qint64> allDate() const;
  QVector<double> allData() const;

 private:
  QGeoCoordinate m_coordinate;

  QString m_name;
  QString m_id;

  int m_numSnaps;
  int m_stationIndex;

  QVector<qint64> m_date;
  QVector<double> m_data;

  bool m_isNull;
};

#endif  // IMEDSSTATION_H
