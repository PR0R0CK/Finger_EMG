#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "kissfft-131/kiss_fft.c"
// samo .h nie bangla, uzasadnienie poniżej
//Kissfft is not really something you need to make and install like other libraries. If you need complex ffts,
//then all you need to do is compile the kiss_fft.c in your project. If you need something more specialized
//like multidimensional or real ffts, then you should also compile the apropriate file(s) from the tools dir.

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(&thread, SIGNAL(tick()), this, SLOT(externalThread_tick()));
    connect(ui->actionRun,  SIGNAL(triggered()), this, SLOT(sendCommand()));
    connect(ui->actionLine, SIGNAL(triggered()), this, SLOT(update()));
    connect(ui->actionBar,  SIGNAL(triggered()), this, SLOT(update()));

    //            timeDataCh1.resize(DSIZE2);
    //            timeDataCh2.resize(DSIZE2);
    //            timeDataCh3.resize(DSIZE2);
    //            timeDataCh1.fill(0);
    //            timeDataCh2.fill(0);
    //            timeDataCh3.fill(0);

    ui->statusBar->showMessage("No device");
    simulation = SIMULATION_NO;

    QString portname;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos)
    {
        if(info.serialNumber()=="NXP-77")
        {
            portname=info.portName();
            serial.setPortName(portname);
            if (serial.open(QIODevice::ReadWrite))
            {
//                serial.setBaudRate( serial.Baud115200,  serial.AllDirections);
                ui->statusBar->showMessage(tr("Device: %1").arg(info.serialNumber()));
                serial.clear();
//                thread.start(thread.HighestPriority);     // zawsze startuj wątek, przeniesione poniżej
            }
            else {

                ui->statusBar->showMessage(tr("Can't open %1, error code %2") .arg(serial.portName()).arg(serial.error()));
                return;
            }
            break;
        }
    }

    if(!serial.isOpen())
      loadSettings();     // ścieżka do pliku z zapisu EMG

      thread.start(thread.HighestPriority);
//    else

    chart.gridNumX=10;
    chart.gridNumY=10;
    chart.minValueX= 1;
    chart.maxValueX= DSIZE2;
    chart.minValueY= 0;
    chart.maxValueY= 1;
    chart.chartMode=LinearChart;
    chart.dataSize=DSIZE2;

    Alloc_memory_sub_constructor();

    isinverse = 0;

    for (int i = 0; i < DSIZE2; i++)
    {
       hamming[i] = (0.54-((0.46) * cos(2*M_PI*i/(DSIZE2-1))));
    }

    setAcceptDrops(true);

    int i = 0;
    file_out.setFileName( DEBUG_FILE EXT );
    while(file_out.exists())
      {
        if( file_out.size() == 0 )  // jeśli plik pusty, to nadpisz
          break;
        i++;
        file_out.setFileName( DEBUG_FILE + QVariant(i).toString() + EXT );
      }

    file_csv.setFileName( DEBUG_FILE ".csv");

    if(simulation)
        file_in.open(QIODevice::ReadOnly);
    else
        file_out.open(QIODevice::WriteOnly | QIODevice::Append);

    file_csv.open(QIODevice::Append);
    stream.setDevice( (QIODevice*) &file_csv );

//    ui->textEdit->setVisible(ui->actionSave->isChecked());
    ui->textEdit->setText( file_out.fileName() );

#ifdef QT_DEBUG
//    ui->selectInput2->setChecked(false);
    ui->selectInput3->setChecked(false);
#endif //QT_DEBUG

    QSplitter *splitter = new QSplitter(this);

    QFileSystemModel *model = new QFileSystemModel;
       model->setRootPath(QDir::currentPath());
       QTreeView *tree = new QTreeView(splitter);
       tree->setModel(model);
//       QListView *listview = new QListView;
         QTreeView *treeview = new QTreeView;
         QTextEdit *textedit = new QTextEdit;

         splitter->resize(300,200);
         splitter->move( 0 , 150);
         tree->setVisible(false);
         splitter->setChildrenCollapsible(true);
//         splitter->


//         tree->resize(100,100);//setWindowwidth(100);

//         splitter->addWidget(listview);
//         splitter->addWidget(treeview);
//         splitter->addWidget(textedit);
//       QList a(3,2);

//       splitter->setSizes()

}

// -----------------------------------------------------------------------------
void MainWindow::Alloc_memory_sub_constructor()
{
    timeData.resize(NCH);
    spectrum.resize(NCH);
    for(int i=0; i< timeData.size();i++)        // dla wszystkich kanalow
    {
        timeData[i].resize(DSIZE2);
        timeData[i].fill(0);
        spectrum[i].resize(DSIZE2);
        spectrum[i].fill(0);
    }

    meanData.resize(NCH);
    meanData.fill(0.0);

    // Added
    cfg = kiss_fft_alloc( DSIZE2, isinverse, 0, 0 );
    if(cfg == nullptr){
        ui->statusBar->showMessage(tr("Can't alloc cfg in MainWindow(QWidget *parent) "));
        return;
    }

    for (int i = 0; i < NCH; i++)
    {
        in[i]=(kiss_fft_cpx*)KISS_FFT_MALLOC(DSIZE2*sizeof(*in[0]));
        out[i]=(kiss_fft_cpx*)KISS_FFT_MALLOC(DSIZE2*sizeof(*out[0]));
        if(in[i] == nullptr){
            ui->statusBar->showMessage(tr("Can't alloc in[%d] in MainWindow(QWidget *parent) "),i);
            return;
        }
        if(out[i] == nullptr){
            ui->statusBar->showMessage(tr("Can't alloc out[%d] in MainWindow(QWidget *parent) "), i);
            return;
        }
        memset(in[i],0,DSIZE2  * sizeof(*in[0]));
        memset(out[i],0,DSIZE2 * sizeof(*out[0]));
    }

    hamming = (kiss_fft_scalar*) KISS_FFT_MALLOC(DSIZE2*sizeof(hamming[0]));
    if (hamming == nullptr)
    {
        ui->statusBar->showMessage(tr("Can't alloc hamming in MainWindow(QWidget *parent) "));
        return;
    }
}

MainWindow::~MainWindow()
{
    thread.terminate();
    thread.wait();
    serial.close();
    if(file_in.isOpen()) file_in.close();
    if(file_out.isOpen()) file_out.close();
    if(file_csv.isOpen()) file_csv.close();
    for (int i = 0; i < NCH; i++)
    {
        free(in[i]);
        free(out[i]);
    }
    kiss_fft_free(cfg);
//Qlistv
    delete ui;
}

// =============================================================================
/* ToDo:
   Emulowanie wejścia np. odtwarzanie nagrania
   szumy, typu piła, wygasające
 * rysowanie widma 2 pola
 * RMS -
 * Github kody publikacja - does't allowed
    zapis i odczyt
    lista plikow

 * //assume the directory exists and contains some files and you want all jpg and JPG files
QDir directory("Pictures/MyPictures");
QStringList images = directory.entryList(QStringList() << "*.jpg" << "*.JPG",QDir::Files);
foreach(QString filename, images) {
//do whatever you need to do
}

QIamge
Qpixmap

 */
//double rms(double x[], int n)
double rms(double* x, int n)
{
    double sum = 0;

    for (int i = 0; i < n; i++)
        sum += pow(x[i], 2);

    return sqrt(sum / n);
}

void MainWindow::externalThread_tick()
{
  if(!simulation)
    auto_actionRun_serial_port(30);                                             // automatyczny start

  if(ui->actionRun->isChecked())
    if ( load_data() )                                                          // serial port lub z pliku
    {
//#define NCH 1 //temporyry
        uint16_t *sample=reinterpret_cast<uint16_t*>(readdata.data());
        for(int i=0; i<DSIZE2; i++)
        {
            for(int k=0; k<NCH; k++)
            {
                timeData[k][i]=(*sample++)/65535.0;
                (in[k]+i)->i = 0;
                (in[k]+i)->r = (*sample)/65535.0;
//                float freq =200;
//                test[i].r = sin(2 * M_PI * freq * i / DSIZE2), test[i].i = 0;
//                test[i].i = 0;
//                test[i].r = kiss_fft_scalar (*sample)/65535.0;

//                if(ui->pwmValue1->value())
//                    (in[k]+i)->r *=  ( hamming[i] + ui->pwmValue1->value()/100 );    // TO DO Hamming window
            }
        }

        if( ui->actionSave->isChecked() )
            save_to_file( 0 );

        for (int k = 0; k < NCH; k++)
        {
            kiss_fft( cfg, in[k], out[k] );
           // kiss_fft( cfg, test, out[k] );

            for (int i = 1; i < DSIZE2/2; i++)
            {
//                if ( i >= DSIZE2 ) // na wypadek zwiękaszania pętli np. razy 2
//                {
//                    break;
//                }
                  double normBinMag = sqrt(SQUARE((out[k]+i)->r) + SQUARE((out[k]+i)->i))/51.2 ; // do konca nie wiem czemu tu musze mnozyc przec 8
//                   normBinMag = sqrt(SQUARE(test[i].r) + SQUARE(test[i].i))  ; // do konca nie wiem czemu tu musze mnozyc przec 8
                  double mag = normBinMag;

    //              filtr_average_beside_freq_by_peak( i, freq[i], cx_out, mag ); // peak number, frequency_index[peak number], fft_out, magnitude to averange

//                  double amplitude = 20. * log10( mag );

                  i *= 2;       // przedział zmiennej i: od 0 do 512 mnożony razy dwa

                  spectrum[k][i] = mag;            // TODO nie mieści się na wykresie//                  spectrum[k][i] = (out[0]+i)->r * hamming[i];
                  spectrum[k][i-1] = mag;            // TODO nie mieści się na wykresie//                  spectrum[k][i] = (out[0]+i)->r * hamming[i];

                  int a = 10;
                  if( a < i && i < DSIZE-a) // Hz
                  {
                    timeData[1][i] = rms(&spectrum[0][i-16],a*2);
                    timeData[1][i-1] = timeData[1][i];
                  }
                  else
                  {
                    timeData[1][i] = 0;
                    timeData[1][i-1] = 0;
                  }

                   a = 20;
                  int n = DSIZE2 / a; // Hz (okno)

                  if( meanData.size() != n )
                    meanData.resize(n);

                  for(int j=0; j<n; j++) // słupki wypełniena
//                  if ( n > 0 && i % n == 0 )
                  {
              //            meanData[j] = std::accumulate( timeData[j].begin(), timeData[j].end(), 0.0)/timeData[j].size();
                      //                      meanData[i/n] = i/n;
                    meanData[j] = (double) j/n;
                  }

                  i /= 2;
//                  fprintf((FILE*) &file, "bin: %4d,   freq: %*.1f [Hz],   mag: %2.4f,   ampl.: %*f [dB]\n",
//                         (int)i*100, 12,44100*(double)i*100/(DSIZE2), normBinMag, 11, amplitude);

//                  char buffer [100];
//                  int n=sprintf (buffer, "bin: %4d,   freq: %*.1f [Hz],   mag: %2.4f,   ampl.: %*f [dB] %d %d",
//                                 (int)i, 12,1024*(double)i/(DSIZE2), normBinMag, 11, amplitude);
//                  if( i %100 == 0 )
//                    {
//                      qInfo() << buffer;
//                    if( i %500 == 0)
//                         qInfo() << endl;
//                    }
//                   qInfo() << amplitude << (out[k]+i)->r << (out[k]+i)->i;
             }
        }

        int q=16;
        int d = DSIZE2/q;


        if( meanData.size() != d )
          meanData.resize(d);

        ui->statusBar->showMessage( "Rozmiar d: " + QString::number(d));

        for (int i = 0; i < d; i++) {
//            meanData[i]=(double)i/d;
            meanData[i]=rms(&spectrum[0][d*q],0);
        }

//        20*log10(sqrt(x)) we can just do 10*log10(x)
//           window[ctr] = hamming[ctr]*y[ctr];

// furier         do 1KHz   probowanie do 45
// port 1-wszy pod 11

        update();

        if(ui->actionRun->isChecked())
            sendCommand();

        readdata.resize(0); // don't move, clear readdata only if was displayed

//        QThread::msleep(10);
    }
}

void MainWindow::sendCommand()
{
    senddata.clear();
    senddata[0]=static_cast<char>(128+(ui->actionTrigger->isChecked()<<6));
    senddata[1]=static_cast<char>(ui->pwmValue1->value());
    senddata[2]=static_cast<char>(ui->pwmValue2->value());
    senddata[3]=static_cast<char>(ui->pwmValue3->value());
    serial.write(senddata);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);

    chart.drawLinearGrid(painter, centralWidget()->geometry());

    if(ui->actionLine->isChecked())
    {
        if(ui->selectInput1->isChecked())
        {
//            for(int i = 0; i < timeData[0].size(); i++)
//                timeData[0][i] = (out[0]+i)->r * hamming[i];
            chart.plotColor=Qt::darkGray;

//            chart.drawLinearData(painter, (QVector<double>*)meanData);
//            chart.drawLinearData(painter, timeData[0]);
              chart.plotColor=Qt::red;
              chart.drawLinearData(painter, timeData[0]);

//              chart.plotColor=Qt::yellow;
//              chart.drawLinearData(painter, spectrum[0]);
        }

        if( ui->actionSpektrum->isChecked())
        {
            chart.plotColor=Qt::gray;
            chart.drawLinearData(painter, spectrum[0]);
        }

        if(ui->selectInput2->isChecked())
        {
            chart.plotColor=Qt::green;
            chart.drawLinearData(painter, timeData[1]);
        }

        if(ui->selectInput3->isChecked())
        {
            chart.plotColor=Qt::yellow;
            chart.drawLinearData(painter, timeData[2]);
        }
    }

    if(ui->actionBar->isChecked()) {
        chart.plotColor=Qt::cyan;
        chart.drawBarsData(painter, meanData);


    }


//    QThread::msleep(1);
}

// -----------------------------------------------------------------------------

inline void MainWindow::auto_actionRun_serial_port(int count_up_to)
{
    static int waiter(0);

    if( waiter >= 0 )
    {
     if(!ui->actionRun->isChecked())
     {
         if( waiter <= count_up_to ) {
             serial.clear();
             waiter++;
         }
         else {
             ui->actionRun->setChecked(true);
             ui->actionRun->triggered(true);
             waiter = -1;
         }
     }
    }
}

// -----------------------------------------------------------------------------

bool MainWindow::load_data(bool add_seconds, int milisec)
{
    if(simulation)                                  // czytaj dane z pliku
    {
       return simulation_read_data_from_file();
    }
    else if( serial.size() >= DSIZE  )              // zapisz do pliku
    {
        readdata = serial.readAll();
        return true;
    }
    else
        return false;
}

// -----------------------------------------------------------------------------

void MainWindow::save_to_file( bool add_seconds)
{
  for (int k = 0; k < NCH; k++)
  {
      if(add_seconds)
      {
          stream << "[" << QDateTime::currentMSecsSinceEpoch() << "]" << readdata << endl;
          // no csv
      }
      else
      {
  //        QTextStream out(&file_out);// out.bin
  //        for (Qvector<double>::iterator iter = timeData[0].begin(); iter != timeData[0].end(); iter++){
  //            out << *timeData[0];
  //        }
//          QByteArray dat ;//= timeData[0].data();
  //        dat.fro

  //                std::vector a = timeData[0].toStdVector();
//          file_out.write( dat );                                   // out.bin
          file_out.write(reinterpret_cast<char*>(timeData[k].data()), static_cast<uint>(timeData[k].size())*sizeof(double));

          qInfo() << k << ": " << timeData[k].size();
          for (int i = 0; i < timeData[0].size(); i++)
          {
              stream <<  timeData[k][i] << ",";    // out.csv
          }
  //        qInfo() << file.fileName();
          stream << endl;
      }
   }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

// -----------------------------------------------------------------------------

void MainWindow::dropEvent(QDropEvent *e)
{
    foreach (const QUrl &url, e->mimeData()->urls()) {
        QString fileName = url.toLocalFile();
        qDebug() << "Dropped file:" << fileName;

        if( simulation_open_file( fileName ) )
            break; // don't proces more files in drag&drop
    }
}

// -----------------------------------------------------------------------------

void MainWindow::on_textEdit_textChanged()
{
    ui->actionSave->setChecked(false);
}

void MainWindow::on_actionSave_triggered()
{
     ui->textEdit->setVisible(ui->actionSave->isChecked());
     if(ui->actionSave->isChecked() == true)
     {
         if(file_out.isOpen())
            file_out.close();
         file_out.setFileName(ui->textEdit->toPlainText());
         file_out.open(QIODevice::Append);
     }
}

// -----------------------------------------------------------------------------

void MainWindow::on_actionOpen_triggered()
{
    QDir start_dir = QCoreApplication::applicationDirPath(); // TODO: uper dir in tree
    start_dir.cdUp();

    QString fileName = QFileDialog::getOpenFileName(this,
    tr("Otwórz plik danych"), start_dir.path(),
    tr("Binary and CSV files (*.bin *.csv)\n"
       "Binary Files (*.bin)\n"
       "Comma Separated Values (*.csv)\n"
       "Wszystkie pliki (*.*)"));

    if(simulation_open_file(fileName))
    {
        ui->actionRun->setChecked(true);    // start drawing
    }
}

// -----------------------------------------------------------------------------

bool MainWindow::simulation_open_file( QString fileName )
{
    if(QFileInfo::exists(fileName) && QFileInfo(fileName).isFile())
    {
        simulation = SIMULATION_NO;
        stream.resetStatus();
        if(file_in.isOpen())
            file_in.close();                 // zamknij poprzedni plik

        file_in.setFileName( fileName );
        file_in.open(QIODevice::ReadOnly);
        if ( file_in.size() < B_SIZE )
        {
            ui->statusBar->showMessage( "Rozmiar pliku " + file_in.fileName() + " mniejszy od "
                                        + QVariant(B_SIZE).toString() + " Bajtów" );
            return false;
        }
        ui->statusBar->showMessage( fileName );

        simulation = SIMULATION_BINARY;
        if(fileName[fileName.length()-4] == '.')
          if(fileName[fileName.length()-3] == 'c')
            if(fileName[fileName.length()-2] == 's')
              if(fileName[fileName.length()-1] == 'v')
                {
                  simulation = SIMULATION_CSV;
                  stream.setDevice( static_cast<QIODevice*> (&file_csv) );
                  if( !file_csv.isOpen() )
                  {
                      simulation = SIMULATION_NO;
                      QMessageBox msgBox;
                      msgBox.setText("Nie udało się otworzyć pliku");
                      msgBox.exec();
                      return false;
                  }
                  return true;
                }
        if( !file_in.isOpen() )
        {
            simulation = SIMULATION_NO;
            QMessageBox msgBox;
            msgBox.setText("Nie udało się otworzyć pliku");
            msgBox.exec();
            return false;
        }
        return true;
    }
    ui->statusBar->showMessage( "Nie udało się otworzyć " + fileName );
    return false;
}

// -----------------------------------------------------------------------------

void MainWindow::saveSettings( const QVariant &value, const QString &key, const QString &group)
{
  QSettings settings(QSettings::IniFormat, QSettings::SystemScope,
             QCoreApplication::applicationName(), QCoreApplication::applicationName());
//    settings.setParent(this);
//    settings.setPath(settings.format(),settings.scope(),settings.applicationName());
//    settings.setSystemIniPath(QCoreApplication::applicationDirPath());
    settings.beginGroup(group);

    settings.setValue(key, value);
    settings.endGroup();
}

QVariant MainWindow::loadSettings(const QString &key, const QString &group, const QVariant &defaultValue)
{
  QSettings settings(QSettings::IniFormat, QSettings::SystemScope,
             QCoreApplication::applicationName(), QCoreApplication::applicationName());

    settings.beginGroup(group);
    QString filename = settings.value(key, defaultValue).toString();
    settings.endGroup();

    qInfo() << "loadSettings(): " << filename;

    if( filename != "" )
      if(simulation_open_file( QDir::currentPath() +"\\"+ filename))
          ui->actionRun->setChecked(true);

    return filename;
}

// -----------------------------------------------------------------------------

bool MainWindow::simulation_read_data_from_file()
{
 for (int k = 0; k < NCH; k++)
 {
  char buff[B_SIZE];
  if( simulation == SIMULATION_CSV )
    {
      for (int i = 0; i < DSIZE/B_SIZE; i++)
        {
              if(stream.status() != QTextStream::Ok)    // CSV
              {
                  qCritical() << "Error read file, buff size: " << buff << endl;
                  return false; // check status before using the data
              }
              if(stream.status() == QTextStream::ReadPastEnd)
              {
//                  stream.reset();
                  stream.seek(0);
                  qCritical() << "Error ReadPastEnd: " << endl;
                  return false; // check status before using the data
              }
              QString a;
              a = stream.readLine(B_SIZE); // TO DO
//              for (int k = 0; k < a.length(); k++)
//                {
//                  QString liczba = "";
//                  int m = 0;
//                  if (a.at(k) != ',') {
//                      liczba.append(a[k]);
//                  }
//                  else
//                    readdata.

//                  a.at(k)
//                }


              //        QByteArray buff = serial.readLine( BSIZE );
              //         stream << "siz:" << buff.length() << endl;
              //        for (int i = 0; i < buff.size(); i++) {
              //            stream << (uint16_t) buff[i] << ",";
              //        }
              //        stream << endl;
              readdata.append(a);
//              buff = reinterpret_cast<char[]>(a.data());


//              QChar *f = a.data();

//              stream >> buff;
//      qInfo() << stream.pos();
        }
    }
  else                                                                          // read binary data
    {
//          qInfo() << file.pos();
           if( file_in.atEnd() )
           {
//            if( file.pos() != 0)  // jezeli nie na początku pliku
              file_in.seek(0);     // zacznij jeszcze raz czytać plik (zapętl)
              ui->actionRun->setChecked(false);
           }

          //    QThread::msleep(milisec);
          qint64 size = file_in.read(buff, DSIZE2);
           size = file_in.read(reinterpret_cast<char*>(timeData[k].data()), static_cast<uint>(timeData[k].size())*sizeof(double));

              if( size != DSIZE2){
          //            return  false;
            qCritical() << "Error read file, buff size: " << size << endl;
            if(size == -1)      // jeśli plik pusty lub nie istnieje
              {
                simulation = SIMULATION_NO;
                return false;
              }
          }

//          readdata.append(buff);

          qInfo() << "ReadData size: " << timeData.size() << endl;

    }
  if (timeData[k].size() >= DSIZE ){ // symulator, nie wymaga płytki z EMG
      return true;
    }else {
      qWarning()<<"NIE wczytano danych\n";
      return false;
    }
 }
}

void MainWindow::on_actionOtw_rz_triggered(bool checked)
{
    system("C:\\ProgramData\\DataTransfer\\DataTransfer.ini");
}

void MainWindow::on_actionZapisz_domy_lne_triggered()
{
    saveSettings();
}

void MainWindow::on_actionKatalog_triggered()
{
    char dst[100] ;
    QString b =  QDir::currentPath();
//    dst = b.data()
//    strcpy_s(dst,100, (char*)b);
  qInfo() << dst;
    system( (const char*) b.data() ); // todo
}

void MainWindow::on_textEdit_cursorPositionChanged()
{

}

void MainWindow::draw_bars_Hz_gap(int window_length, int  rms)
{
    int a = 20;
    int n = DSIZE2 / window_length; // Hz (okno)

    meanData.resize(n);

    for(int j=0; j<n; j++) // słupki wypełniena
    {
//            meanData[j] = std::accumulate( timeData[j].begin(), timeData[j].end(), 0.0)/timeData[j].size();
        meanData[j] = (double)j/n;
    }
}