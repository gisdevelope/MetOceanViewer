//-------------------------------GPL-------------------------------------//
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
// The name "MetOcean Viewer" is specific to this project and may not be
// used for projects "forked" or derived from this work.
//
//-----------------------------------------------------------------------//
#include <noaa.h>

int noaa::fetchNOAAData(QVector<QString> &javascript)
{
    QEventLoop loop;
    qint64 Duration;
    QString PlotTitle,RequestURL,StartString,EndString,Product,Product2;
    QString XLabel,YLabel;
    QDateTime StartDate,EndDate;
    int i,j,ierr,NumDownloads,NumData;
    QVector<QDateTime> StartDateList,EndDateList;

    javascript.resize(5);

    if(this->StartDate==this->EndDate||this->EndDate<this->StartDate)
        return NOAA_ERR_INVALIDDATERANGE;

    //Begin organizing the dates for download
    Duration = StartDate.daysTo(EndDate);
    NumDownloads = (Duration / 30) + 1;
    StartDateList.resize(NumDownloads);
    EndDateList.resize(NumDownloads);

    //Build the list of dates in 30 day intervals
    for(i=0;i<NumDownloads;i++)
    {
        StartDateList[i] = this->StartDate.addDays(i*30).addDays(i);
        StartDateList[i].setTime(QTime(0,0,0));
        EndDateList[i]   = StartDateList[i].addDays(30);
        EndDateList[i].setTime(QTime(23,59,59));
        if(EndDateList[i]>this->EndDate)
            EndDateList[i] = this->EndDate;
    }

    ierr = this->retrieveProduct(2,Product,Product2);

    if(this->ProductIndex==0)
        NumData = 2;
    else
        NumData = 1;

    if(this->ProductIndex == 4 || this->ProductIndex == 5 || this->ProductIndex == 6
                               || this->ProductIndex == 7 || this->ProductIndex == 8)
        this->Datum = "Stnd";

    //Allocate the NOAA array
    this->NOAAWebData.clear();
    this->NOAAWebData.resize(NumData);
    for(i=0;i<NumData;i++)
        this->NOAAWebData[i].resize(NumDownloads);

    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    for(j=0;j<NumData;j++)
    {
        for(i=0;i<NumDownloads;i++)
        {
            //Make the date string
            StartString = StartDateList[i].toString("yyyyMMdd hh:mm");
            EndString = EndDateList[i].toString("yyyyMMdd hh:mm");

            //Build the URL to request data from the NOAA CO-OPS API
            if(j==0)
                RequestURL = QString("http://tidesandcurrents.noaa.gov/api/datagetter?")+
                             QString("product="+Product+"&application=metoceanviewer")+
                             QString("&begin_date=")+StartString+QString("&end_date=")+EndString+
                             QString("&station=")+QString::number(NOAAMarkerID)+
                             QString("&time_zone=GMT&units=")+Units+
                             QString("&interval=&format=csv");
            else
                RequestURL = QString("http://tidesandcurrents.noaa.gov/api/datagetter?")+
                             QString("product="+Product2+"&application=metoceanviewer")+
                             QString("&begin_date=")+StartString+QString("&end_date=")+EndString+
                             QString("&station=")+QString::number(NOAAMarkerID)+
                             QString("&time_zone=GMT&units=")+Units+
                             QString("&interval=&format=csv");

            //Allow a different datum where allowed
            if(this->Datum != "Stnd")RequestURL = RequestURL+QString("&datum=")+Datum;

            //Send the request
            QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(RequestURL)));
            connect(reply,SIGNAL(finished()),&loop,SLOT(quit()));
            connect(reply,SIGNAL(error(QNetworkReply::NetworkError)),&loop,SLOT(quit()));
            loop.exec();
            this->ReadNOAAResponse(reply,i,j);

        }
    }

    if(this->ProductIndex == 0)
    {
        if(this->Units=="metric")
            this->Units="m";
        else
            this->Units="ft";
        YLabel = "Water Level ("+this->Units+", "+this->Datum+")";
    }
    else if(this->ProductIndex == 1 || this->ProductIndex == 2 || this->ProductIndex == 3)
    {
        if(this->Units=="metric")
            this->Units="m";
        else
            this->Units="ft";
        YLabel = Product+" ("+Units+", "+this->Datum+")";
    }
    else if(this->ProductIndex == 6)
    {
        if(this->Units=="metric")
            this->Units="m/s";
        else
            this->Units="knots";
        this->Datum = "Stnd";
        YLabel = Product+" ("+this->Units+")";
    }
    else if(this->ProductIndex == 4 || this->ProductIndex == 5)
    {
        if(this->Units=="metric")
            this->Units="Celcius";
        else
            this->Units="Fahrenheit";
        this->Datum = "Stnd";
        YLabel = Product+" ("+this->Units+")";
    }
    else if(this->ProductIndex == 7)
    {
        this->Units = "%";
        this->Datum = "Stnd";
        YLabel = Product+" ("+this->Units+")";
    }
    else if(this->ProductIndex == 8)
    {
        this->Units = "mb";
        this->Datum = "Stnd";
        YLabel = Product+" ("+this->Units+")";
    }

    XLabel = "Date";
    PlotTitle = "Station "+QString::number(this->NOAAMarkerID)+": "+this->CurrentNOAAStationName;
    javascript[0] = "setGlobal('"+PlotTitle+"','"+XLabel+"','"+YLabel+"','"+QString::number(MOV_NULL_TS)+"')";

    if(NumData==2)
    {
        javascript[1] = "SetSeriesOptions(0,'Observed Water Level','#0101DF')";
        javascript[2] = "SetSeriesOptions(1,'Predicted Water Level','#00FF00')";
    }
    else
    {
        javascript[3] = "SetSeriesOptions(0,'"+Product+"','#0101DF')";
    }

    javascript[4] = "allocateData("+QString::number(NumData)+")";

    return 0;
}

