//---- LGPL --------------------------------------------------------------------
//
// Copyright (C)  ARCADIS, 2011-2013.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation version 3.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// contact: Zachary Cobell, zachary.cobell@arcadis-us.com
// ARCADIS
// 4999 Pearl East Circle, Suite 200
// Boulder, CO 80301
//
// All indications and logos of, and references to, "ARCADIS" 
// are registered trademarks of ARCADIS, and remain the property of
// ARCADIS. All rights reserved.
//
//------------------------------------------------------------------------------
// $Author$
// $Date$
// $Id$
// $HeadURL$
//------------------------------------------------------------------------------
//  File: ADCvalidator_main.cpp
//
//------------------------------------------------------------------------------
        
#include "ADCvalidator.h"
#include "ui_ADCvalidator_main.h"

QString PreviousDirectory;
QColor ADCIRCIMEDSColor,OBSIMEDSColor;
QColor LineColor121Line,LineColorBounds;
QColor DotColorHWM,LineColorRegression;
QDateTime IMEDSMinDate,IMEDSMaxDate;
QVector<QVector<double> > NOAAStations;
QVector<QString> NOAAStationNames;
QVector<QString> panToLocations;
QVector<NOAAStationData> CurrentStation;
QVariant MarkerID = -1;

//Main routine which will intialize all the tabs
MainWindow::MainWindow(QWidget *parent):QMainWindow(parent),ui(new Ui::MainWindow)
{
    QString ButtonStyle;
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);

    //Setup UI
    ui->setupUi(this);

    //Load the NOAA tab and set accordingly
    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    ui->noaa_map->load(QUrl("qrc:/html/noaa_maps.html"));
    ui->noaa_map->page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal,Qt::ScrollBarAlwaysOff);
    ui->noaa_map->page()->mainFrame()->setScrollBarPolicy(Qt::Vertical,Qt::ScrollBarAlwaysOff);
    QObject::connect(ui->noaa_map,SIGNAL(loadFinished(bool)),this,SLOT(BeginGatherStations()));
    ui->noaa_map->setContextMenuPolicy(Qt::CustomContextMenu);

    //Set the initial dates to today and today minus a day
    ui->Date_StartTime->setDateTime(QDateTime::currentDateTime().addDays(-1));
    ui->Date_EndTime->setDateTime(QDateTime::currentDateTime());

    //Set unselectable combo box items
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->Combo_PanTo->model());
    QModelIndex firstIndex = model->index(3,ui->Combo_PanTo->modelColumn(),ui->Combo_PanTo->rootModelIndex());
    QStandardItem* firstItem = model->itemFromIndex(firstIndex);
    firstItem->setSelectable(false);

    //Set up the pan to combo box limits for the google map
    int NumItems = ui->Combo_PanTo->count();
    initializePanToLocations(NumItems);

    //Load the IMEDS tab and set accordingly
    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    ui->imeds_map->load(QUrl("qrc:/html/imeds_maps.html"));
    ui->imeds_map->page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal,Qt::ScrollBarAlwaysOff);
    ui->imeds_map->page()->mainFrame()->setScrollBarPolicy(Qt::Vertical,Qt::ScrollBarAlwaysOff);
    ui->imeds_map->setContextMenuPolicy(Qt::CustomContextMenu);

    //Set the initial values for the IMEDS Data input tab
    ui->browse_ADCIMEDS->setEnabled(false);
    ui->browse_OBSIMEDS->setEnabled(false);
    ui->text_ADCIMEDS->setEnabled(false);
    ui->text_OBSIMEDS->setEnabled(false);
    ui->text_ADCIMEDS->setReadOnly(true);
    ui->text_OBSIMEDS->setReadOnly(true);
    ui->button_processIMEDS->setEnabled(false);
    ADCIRCIMEDSColor.setRgb(0,255,0);
    OBSIMEDSColor.setRgb(255,0,0);
    ButtonStyle = MakeColorString(ADCIRCIMEDSColor);
    ui->button_adcselectcolor->setStyleSheet(ButtonStyle);
    ui->button_adcselectcolor->update();
    ui->button_adcselectcolor->setEnabled(false);
    ui->label_adcirclabel->setEnabled(false);
    ui->text_adcirclabel->setEnabled(false);
    ui->label_adcselectcolor->setEnabled(false);
    ButtonStyle = MakeColorString(OBSIMEDSColor);
    ui->button_imedsselectcolor->setStyleSheet(ButtonStyle);
    ui->button_imedsselectcolor->update();
    ui->button_imedsselectcolor->setEnabled(false);
    ui->label_imedslabel->setEnabled(false);
    ui->text_imedslabel->setEnabled(false);
    ui->label_imedsselectcolor->setEnabled(false);

    //Initialize Variables
    IMEDSMinDate.setDate(QDate(2900,1,1));
    IMEDSMaxDate.setDate(QDate(1820,1,1));

    //Load the High Water Mark Map and Regression Chart
    ui->map_hwm->load(QUrl("qrc:/html/hwm_map.html"));
    ui->map_hwm->page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal,Qt::ScrollBarAlwaysOff);
    ui->map_hwm->page()->mainFrame()->setScrollBarPolicy(Qt::Vertical,Qt::ScrollBarAlwaysOff);
    ui->map_regression->load(QUrl("qrc:/html/reg_plot.html"));
    ui->map_regression->page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal,Qt::ScrollBarAlwaysOff);
    ui->map_regression->page()->mainFrame()->setScrollBarPolicy(Qt::Vertical,Qt::ScrollBarAlwaysOff);
    DotColorHWM.setRgb(11,84,255);
    LineColorRegression.setRgb(7,145,0);
    LineColor121Line.setRgb(255,0,0);
    LineColorBounds.setRgb(0,0,0);
    ButtonStyle = MakeColorString(DotColorHWM);
    ui->button_hwmcolor->setStyleSheet(ButtonStyle);
    ui->button_hwmcolor->update();
    ButtonStyle = MakeColorString(LineColor121Line);
    ui->button_121linecolor->setStyleSheet(ButtonStyle);
    ui->button_121linecolor->update();
    ButtonStyle = MakeColorString(LineColorRegression);
    ui->button_reglinecolor->setStyleSheet(ButtonStyle);
    ui->button_reglinecolor->update();
    ButtonStyle = MakeColorString(LineColorBounds);
    ui->button_boundlinecolor->setStyleSheet(ButtonStyle);
    ui->button_boundlinecolor->update();

    //Setup the IMEDS Table
    SetupIMEDSTable();

}

//Main destructor routine
MainWindow::~MainWindow()
{
    delete ui;
}
