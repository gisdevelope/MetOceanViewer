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
#include "noaa.h"

noaa::noaa(QWebEngineView *inMap, mov_QChartView *inChart,
           QDateEdit *inStartDateEdit, QDateEdit *inEndDateEdit,
           QComboBox *inNoaaProduct, QComboBox *inNoaaUnits,
           QComboBox *inNoaaDatum, QStatusBar *inStatusBar, QObject *parent) : QObject(parent)
{
    map = inMap;
    chart = inChart;
    startDateEdit = inStartDateEdit;
    endDateEdit = inEndDateEdit;
    noaaProduct = inNoaaProduct;
    noaaDatum = inNoaaDatum;
    noaaUnits = inNoaaUnits;
    statusBar = inStatusBar;
}

noaa::~noaa()
{

}

void noaa::handleLegendMarkerClicked()
{
    QLegendMarker* marker = qobject_cast<QLegendMarker*> (sender());

    Q_ASSERT(marker);

    switch (marker->type())
    {
        case QLegendMarker::LegendMarkerTypeXY:
        {
            // Toggle visibility of series
            marker->series()->setVisible(!marker->series()->isVisible());

            // Turn legend marker back to visible, since hiding series also hides the marker
            // and we don't want it to happen now.
            marker->setVisible(true);

            // Dim the marker, if series is not visible
            qreal alpha = 1.0;

            if (!marker->series()->isVisible())
                alpha = 0.5;

            QColor color;
            QBrush brush = marker->labelBrush();
            color = brush.color();
            color.setAlphaF(alpha);
            brush.setColor(color);
            marker->setLabelBrush(brush);

            brush = marker->brush();
            color = brush.color();
            color.setAlphaF(alpha);
            brush.setColor(color);
            marker->setBrush(brush);

            QPen pen = marker->pen();
            color = pen.color();
            color.setAlphaF(alpha);
            pen.setColor(color);
            marker->setPen(pen);

            break;
        }
        default:
        {
            qDebug() << "Unknown marker type";
            break;
        }
    }
}

int noaa::fetchNOAAData()
{
    QEventLoop loop;
    qint64 Duration;
    QString RequestURL,StartString,EndString,Product,Product2;
    int i,j,ierr,NumDownloads,NumData;
    QVector<QDateTime> StartDateList,EndDateList;

    if(this->StartDate==this->EndDate||this->EndDate<this->StartDate)
        return ERR_NOAA_INVALIDDATERANGE;

    //Begin organizing the dates for download
    Duration = this->StartDate.daysTo(this->EndDate);
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
            this->readNOAAResponse(reply,i,j);
        }
    }
    return 0;
}


QString noaa::formatNOAAResponse(QVector<QByteArray> Input,QString &ErrorString,int index)
{
    int i,j,k;
    int dataCount;
    QString Output,TempData,DateS,YearS,MonthS,DayS,HourMinS,HourS,MinS,WLS;
    QStringList TimeSnap;
    QVector<QString> InputData;
    QVector<QStringList> DataList;
    QVector<QString> Temp;

    InputData.resize(Input.length());
    DataList.resize(Input.length());
    Temp.resize(Input.length());

    for(i=0;i<DataList.length();i++)
    {
        InputData[i] = QString(Input[i]);
        DataList[i] = InputData[i].split(QRegExp("[\r\n]"),QString::SkipEmptyParts);
        Temp[i] = QString(Input[i]);
        ErrorString = Temp[i] + "\n";
    }

    Output = "'";

    dataCount = 0;
    for(i=0;i<DataList.length();i++)
        dataCount = dataCount+DataList[i].length()-1;

    this->CurrentNOAAStation[index].resize(dataCount);

    k = 0;
    for(j=0;j<DataList.length();j++)
    {
        for(i=1;i<DataList[j].length();i++)
        {
            TempData = DataList[j].value(i);
            TimeSnap = TempData.split(",");
            DateS    = TimeSnap.value(0);
            YearS    = DateS.mid(0,4);
            MonthS   = DateS.mid(5,2);
            DayS     = DateS.mid(8,2);
            HourMinS = DateS.mid(11,5);
            HourS    = HourMinS.mid(0,2);
            MinS     = HourMinS.mid(3,2);
            WLS      = TimeSnap.value(1);
            Output=Output+YearS+":"+MonthS+":"+
                   DayS+":"+HourS+":"+MinS+":"+WLS+";";
            TempData = YearS+"/"+MonthS+"/"+DayS;
            this->CurrentNOAAStation[index][k].Date.setDate(YearS.toInt(),MonthS.toInt(),DayS.toInt());
            this->CurrentNOAAStation[index][k].Time = QTime(HourS.toInt(),MinS.toInt(),0);
            this->CurrentNOAAStation[index][k].value = WLS.toDouble();
            k = k + 1;
        }
    }
    Output = Output+"'";

    return Output;
}


int noaa::getDataBounds(double &ymin, double &ymax)
{
    int i,j;

    ymin = DBL_MAX;
    ymax = -DBL_MAX;

    for(i=0;i<this->CurrentNOAAStation.length();i++)
    {
        for(j=0;j<this->CurrentNOAAStation[i].length();j++)
        {
            if(this->CurrentNOAAStation[i][j].value<ymin)
                ymin = this->CurrentNOAAStation[i][j].value;
            if(this->CurrentNOAAStation[i][j].value>ymax)
                ymax = this->CurrentNOAAStation[i][j].value;
        }
    }
    return 0;
}


int noaa::generateLabels()
{
    int ierr;
    QString Product,Product2;

    ierr = this->retrieveProduct(1,Product,Product2);

    if(this->ProductIndex == 0)
    {
        if(this->Units=="metric")
            this->Units="m";
        else
            this->Units="ft";
        this->yLabel = "Water Level ("+this->Units+", "+this->Datum+")";
    }
    else if(this->ProductIndex == 1 || this->ProductIndex == 2 || this->ProductIndex == 3)
    {
        if(this->Units=="metric")
            this->Units="m";
        else
            this->Units="ft";
        this->yLabel = Product+" ("+Units+", "+this->Datum+")";
    }
    else if(this->ProductIndex == 6)
    {
        if(this->Units=="metric")
            this->Units="m/s";
        else
            this->Units="knots";
        this->Datum = "Stnd";
        this->yLabel = Product+" ("+this->Units+")";
    }
    else if(this->ProductIndex == 4 || this->ProductIndex == 5)
    {
        if(this->Units=="metric")
            this->Units="Celcius";
        else
            this->Units="Fahrenheit";
        this->Datum = "Stnd";
        this->yLabel = Product+" ("+this->Units+")";
    }
    else if(this->ProductIndex == 7)
    {
        this->Units = "%";
        this->Datum = "Stnd";
        this->yLabel = Product+" ("+this->Units+")";
    }
    else if(this->ProductIndex == 8)
    {
        this->Units = "mb";
        this->Datum = "Stnd";
        this->yLabel = Product+" ("+this->Units+")";
    }

    this->plotTitle = "Station "+QString::number(this->NOAAMarkerID)+": "+this->CurrentNOAAStationName;

    return 0;
}


QString noaa::getNOAAErrorString()
{
    return this->NOAAErrorString;
}

int noaa::getLoadedNOAAStation()
{
    return this->NOAAMarkerID;
}

int noaa::getClickedNOAAStation()
{
    QString JunkString;
    double JunkDouble1,JunkDouble2;
    return getNOAAStation(JunkString,JunkDouble1,JunkDouble2);
}

int noaa::setNOAAStation()
{
    this->NOAAMarkerID = getNOAAStation(this->CurrentNOAAStationName,
                                        this->CurrentNOAALon,this->CurrentNOAALat);
    return 0;
}

int noaa::getNOAAStation(QString &NOAAStationName, double &longitude, double &latitude)
{
    QVariant eval = QVariant();
    this->map->page()->runJavaScript("returnStationID()",[&eval](const QVariant &v){eval = v;});
    while(eval.isNull())
        delayM(5);
    QStringList evalList = eval.toString().split(";");

    NOAAStationName = evalList.value(1).simplified();
    latitude = evalList.value(3).toDouble();
    longitude = evalList.value(2).toDouble();

    return evalList.value(0).toInt();
}


int noaa::plotChart()
{
    int i,j,ierr;
    double ymin,ymax;
    QString S1,S2,format;
    QDateTime minDateTime,maxDateTime;

    maxDateTime = QDateTime(QDate(1000,1,1),QTime(0,0,0));
    minDateTime = QDateTime(QDate(3000,1,1),QTime(0,0,0));

    //...Create the line series
    ierr = this->generateLabels();
    ierr = this->retrieveProduct(3,S1,S2);
    ierr = this->getDataBounds(ymin,ymax);

    //...Create the chart
    this->thisChart = new QChart();
    this->chart->m_chart = this->thisChart;

    QLineSeries *series1 = new QLineSeries(this);
    QLineSeries *series2 = new QLineSeries(this);
    series1->setName(S1);
    series2->setName(S2);
    series1->setPen(QPen(QColor(0,0,255),3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    series2->setPen(QPen(QColor(0,255,0),3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));

    minDateTime = this->startDateEdit->dateTime();
    maxDateTime = this->endDateEdit->dateTime().addDays(1);

    minDateTime.setTime(QTime(0,0,0));
    maxDateTime.setTime(QTime(0,0,0));

    QDateTimeAxis *axisX = new QDateTimeAxis(this);
    axisX->setTickCount(5);
    if(this->StartDate.daysTo(this->EndDate)>90)
        axisX->setFormat("MM/yyyy");
    else if(this->StartDate.daysTo(this->EndDate)>4)
        axisX->setFormat("MM/dd/yyyy");
    else
        axisX->setFormat("MM/dd/yyyy hh:mm");
    axisX->setTitleText("Date (GMT)");
    axisX->setTitleFont(QFont("Helvetica",10,QFont::Bold));
    axisX->setMin(minDateTime);
    axisX->setMax(maxDateTime);
    this->thisChart->addAxis(axisX, Qt::AlignBottom);

    QValueAxis *axisY = new QValueAxis(this);
    axisY->setLabelFormat(format);
    axisY->setTitleText(this->yLabel);
    axisY->setTitleFont(QFont("Helvetica",10,QFont::Bold));
    axisY->setMin(ymin);
    axisY->setMax(ymax);
    this->thisChart->addAxis(axisY, Qt::AlignLeft);

    for(i=0;i<this->CurrentNOAAStation.length();i++)
    {
        if(i==0)
        {
            for(j=0;j<this->CurrentNOAAStation[i].length();j++)
                series1->append(QDateTime(this->CurrentNOAAStation[i][j].Date,
                                          this->CurrentNOAAStation[i][j].Time).toMSecsSinceEpoch(),
                                          this->CurrentNOAAStation[i][j].value);
            this->thisChart->addSeries(series1);
            series1->attachAxis(axisX);
            series1->attachAxis(axisY);
        }
        else if(i==1)
        {
            for(j=0;j<this->CurrentNOAAStation[i].length();j++)
                series2->append(QDateTime(this->CurrentNOAAStation[i][j].Date,
                                          this->CurrentNOAAStation[i][j].Time).toMSecsSinceEpoch(),
                                          this->CurrentNOAAStation[i][j].value);
            this->thisChart->addSeries(series2);
            series2->attachAxis(axisX);
            series2->attachAxis(axisY);
        }
    }

    for(i=0;i<this->thisChart->legend()->markers().length();i++)
        this->thisChart->legend()->markers().at(i)->setFont(QFont("Helvetica",10,QFont::Bold));

    axisY->setTickCount(10);
    axisY->applyNiceNumbers();
    axisX->setGridLineColor(QColor(200,200,200));
    axisY->setGridLineColor(QColor(200,200,200));
    axisY->setShadesPen(Qt::NoPen);
    axisY->setShadesBrush(QBrush(QColor(240,240,240)));
    axisY->setShadesVisible(true);

    this->thisChart->setAnimationOptions(QChart::SeriesAnimations);
    this->thisChart->legend()->setAlignment(Qt::AlignBottom);
    this->thisChart->setTitle("NOAA Station "+QString::number(this->NOAAMarkerID)+": "+this->CurrentNOAAStationName);
    this->thisChart->setTitleFont(QFont("Helvetica",14,QFont::Bold));
    this->chart->setRenderHint(QPainter::Antialiasing);
    this->chart->setChart(this->thisChart);

    foreach (QLegendMarker* marker, this->thisChart->legend()->markers())
    {
        // Disconnect possible existing connection to avoid multiple connections
        QObject::disconnect(marker, SIGNAL(clicked()), this, SLOT(handleLegendMarkerClicked()));
        QObject::connect(marker, SIGNAL(clicked()), this, SLOT(handleLegendMarkerClicked()));
    }

    this->chart->m_style = 1;
    this->chart->m_coord = new QGraphicsSimpleTextItem(this->thisChart);
    this->chart->m_coord->setPos(this->chart->size().width()/2 - 100, this->chart->size().height() - 20);
    this->chart->initializeAxisLimits();
    this->chart->setStatusBar(this->statusBar);

    return 0;
}


int noaa::plotNOAAStation()
{

    //...get the latest station
    int ierr = this->setNOAAStation();

    //...Grab the options from the UI
    this->StartDate = this->startDateEdit->dateTime();
    this->StartDate.setTime(QTime(0,0,0));
    this->EndDate = this->endDateEdit->dateTime();
    this->EndDate = this->EndDate.addDays(1);
    this->EndDate.setTime(QTime(0,0,0));
    this->Units = this->noaaUnits->currentText();
    this->Datum = this->noaaDatum->currentText();
    this->ProductIndex = this->noaaProduct->currentIndex();

    //Update status
    statusBar->showMessage("Downloading data from NOAA...",0);

    //...Generate the javascript calls in this array
    ierr = this->fetchNOAAData();

    //...Update the status bar
    statusBar->showMessage("Plotting the data from NOAA...");

    //...Generate prep the data for plotting
    ierr = this->prepNOAAResponse();

    //...Check for valid data
    if(this->CurrentNOAAStation[0].length()<5)
    {
        this->NOAAErrorString = this->ErrorString[0];
        return -1;
    }

    //...Plot the chart
    ierr = this->plotChart();

    statusBar->clearMessage();

    return 0;

}


int noaa::prepNOAAResponse()
{
    QVector<QString> NOAAData;
    int i;

    for(i=0;i<this->CurrentNOAAStation.length();i++)
        this->CurrentNOAAStation[i].clear();
    this->CurrentNOAAStation.clear();

    this->CurrentNOAAStation.resize(NOAAWebData.length());

    NOAAData.resize(NOAAWebData.length());
    this->ErrorString.resize(NOAAWebData.length());
    for(i=0;i<NOAAWebData.length();i++)
    {
        NOAAData[i] = this->formatNOAAResponse(this->NOAAWebData[i],this->ErrorString[i],i);
        this->ErrorString[i].remove(QRegExp("[\\n\\t\\r]"));
    }
    return 0;
}


void noaa::readNOAAResponse(QNetworkReply *reply, int index, int index2)
{
    QByteArray Data;

    //Catch some errors during the download
    if(reply->error()!=0)
    {
        QMessageBox::information(0,"ERROR","ERROR: "+reply->errorString());
        reply->deleteLater();
        return;
    }

    //Read the data received from NOAA server
    Data=reply->readAll();

    //Save the data into an array and increment the counter
    this->NOAAWebData[index2][index] = Data;

    //Delete this response
    reply->deleteLater();

    return;
}


int noaa::retrieveProduct(int type, QString &Product, QString &Product2)
{
    Product2 = "null";
    if(type==1)
    {
        switch(this->ProductIndex)
        {
            case(0):Product = "6 Min Observed Water Level vs. Predicted"; break;
            case(1):Product = "6 Min Observed Water Level"; break;
            case(2):Product = "Hourly Observed Water Level"; break;
            case(3):Product = "Predicted Water Level"; break;
            case(4):Product = "Air Temperature"; break;
            case(5):Product = "Water Temperature"; break;
            case(6):Product = "Wind Speed"; break;
            case(7):Product = "Relative Humidity"; break;
            case(8):Product = "Air Pressure"; break;
        }
    }
    else if(type==2)
    {
        switch(this->ProductIndex)
        {
            case(0):Product = "water_level"; Product2 = "predictions"; break;
            case(1):Product = "water_level"; break;
            case(2):Product = "hourly_height"; break;
            case(3):Product = "predictions"; break;
            case(4):Product = "air_temperature"; break;
            case(5):Product = "water_temperature"; break;
            case(6):Product = "wind"; break;
            case(7):Product = "humidity"; break;
            case(8):Product = "air_pressure"; break;
        }
    }
    if(type==3)
    {
        switch(this->ProductIndex)
        {
            case(0):Product = "Observed"; Product2 = "Predicted"; break;
            case(1):Product = "6 Min Observed Water Level"; break;
            case(2):Product = "Hourly Observed Water Level"; break;
            case(3):Product = "Predicted Water Level"; break;
            case(4):Product = "Air Temperature"; break;
            case(5):Product = "Water Temperature"; break;
            case(6):Product = "Wind Speed"; break;
            case(7):Product = "Relative Humidity"; break;
            case(8):Product = "Air Pressure"; break;
        }
    }
    return 0;
}


int noaa::saveNOAAImage(QString filename, QString filter)
{

    if(filter == "PDF (*.pdf)")
    {
        QPrinter printer(QPrinter::HighResolution);
        printer.setPageSize(QPrinter::Letter);
        printer.setResolution(400);
        printer.setOrientation(QPrinter::Landscape);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(filename);

        QPainter painter(&printer);
        painter.setRenderHint(QPainter::Antialiasing,true);
        painter.begin(&printer);

        //...Page 1 - Chart
        this->chart->render(&painter);

        //...Page 2 - Map
        printer.newPage();
        QPixmap renderedMap = this->map->grab();
        QPixmap mapScaled = renderedMap.scaledToWidth(printer.width());
        if(mapScaled.height()>printer.height())
            mapScaled = renderedMap.scaledToHeight(printer.height());
        int cw = (printer.width()-mapScaled.width())/2;
        int ch = (printer.height()-mapScaled.height())/2;
        painter.drawPixmap(cw,ch,mapScaled.width(),mapScaled.height(),mapScaled);

        painter.end();
    }
    else if(filter == "JPG (*.jpg *.jpeg)")
    {
        QFile outputFile(filename);
        QSize imageSize(this->map->size().width()+this->chart->size().width(),this->map->size().height());
        QRect chartRect(this->map->size().width(),0,this->chart->size().width(),this->chart->size().height());

        QImage pixmap(imageSize, QImage::Format_ARGB32);
        pixmap.fill(Qt::white);
        QPainter imagePainter(&pixmap);
        imagePainter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        this->map->render(&imagePainter,QPoint(0,0));
        this->chart->render(&imagePainter,chartRect);

        outputFile.open(QIODevice::WriteOnly);
        pixmap.save(&outputFile,"JPG",100);
    }

    return 0;
}

int noaa::saveNOAAData(QString filename, QString PreviousDirectory, QString format)
{
    int index;
    QString value,filename2;

    for(index=0;index<this->CurrentNOAAStation.length();index++)
    {
        if(this->CurrentNOAAStation.length()==2)
        {
            if(index==0)
                filename2 = PreviousDirectory+"/Observation_"+filename;
            else
                filename2 = PreviousDirectory+"/Predictions_"+filename;
        }
        else
            filename2 = filename;

        QFile NOAAOutput(filename2);

        QTextStream Output(&NOAAOutput);
        NOAAOutput.open(QIODevice::WriteOnly);

        if(format.compare("CSV")==0)
        {
            Output << "Station: "+QString::number(this->NOAAMarkerID)+"\n";
            Output << "Datum: "+this->noaaDatum->currentText()+"\n";
            Output << "Units: "+this->noaaUnits->currentText()+"\n";
            Output << "\n";
            for(int i=0;i<this->CurrentNOAAStation.length();i++)
            {
                value.sprintf("%10.4e",this->CurrentNOAAStation[index][i].value);
                Output << this->CurrentNOAAStation[index][i].Date.toString("MM/dd/yyyy")+","+
                          this->CurrentNOAAStation[index][i].Time.toString("hh:mm")+","+
                          value+"\n";
            }
        }
        else if(format.compare("IMEDS")==0)
        {
            QString datum = this->noaaDatum->currentText();
            QString units = this->noaaUnits->currentText();
            QString units2;
            if(units.compare("metric")==0)
            {
                units = "meters";
                units2 = "m";
            }
            else
            {
                units = "feet";
                units2 = "ft";
            }
            Output << "% IMEDS generic format - Water Level\n";
            Output << "% year month day hour min sec watlev("+units2+")\n";
            Output << "NOAA    UTC    "+datum+"\n";
            Output << "NOAA_"+QString::number(this->NOAAMarkerID)+
                      "   "+QString::number(this->CurrentNOAALat)+
                      "   "+QString::number(this->CurrentNOAALon)+"\n";
            for(int i=0;i<this->CurrentNOAAStation[index].length();i++)
            {
                value.sprintf("%10.4e",this->CurrentNOAAStation[index][i].value);
                Output << this->CurrentNOAAStation[index][i].Date.toString("yyyy")+"    "+
                            this->CurrentNOAAStation[index][i].Date.toString("MM")+"    "+
                            this->CurrentNOAAStation[index][i].Date.toString("dd")+"    "+
                            this->CurrentNOAAStation[index][i].Time.toString("hh")+"    "+
                            this->CurrentNOAAStation[index][i].Time.toString("mm")+"    "+
                                                            "00" +"    "+value+"\n";
            }

        }
        NOAAOutput.close();
    }

    return 0;
}
