#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include "ui_mainwindow.h"
#include <QThread>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QVector>
#include <string>
#include <vector>
#include <memory>

// LSL API
#include <lsl_cpp.h>

// eegoSports
//#define WIN32_LEAN_AND_MEAN
#define EEGO_SDK_BIND_STATIC
//#include <windows.h>
#include "eemagine/sdk/factory.h"

class Reader : public QObject {
	Q_OBJECT

public:
	Reader();
	~Reader();

	public slots:
	void read();
	void setStop(bool stop) {
		this->stop = stop;
	}
	void setParams(int samplingRate);
    void setParams(QString chs);

signals:
	void finished();
	void timeout();
	void ampNotFound();
	void connectionLost();
    void displayImp(QVector<double>);

private:
    std::shared_ptr<eemagine::sdk::factory> fact;
    std::shared_ptr<eemagine::sdk::amplifier> amp;
    std::shared_ptr<eemagine::sdk::stream> eegStream;
	int samplingRate;
    std::vector<int> channels;
	bool stop;
};

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent, const std::string &config_file, const bool linkOnStart);
	~MainWindow();

	private slots:
	void threadFinished();
	void threadTimeout();
	void connectionLost();
	void ampNotFound();
    void displayImp(QVector<double>);
	void load_config_dialog();
	void save_config_dialog();

	// start the eegosports connection
	void link();

	// close event (potentially disabled)
	void closeEvent(QCloseEvent *ev);
private:
	// background data reader thread
	void read_thread(std::string deviceNumber, int chunkSize, int samplingRate, bool isSlave, std::string serialNumber, int channelCount, std::vector<std::string> channelLabels);


	// raw config file IO
	void load_config(const std::string &filename);
	void save_config(const std::string &filename);

	Reader *reader;
	QThread *thread;					
    QVector<QLabel*> imps;
	Ui::MainWindowClass ui;
};

#endif // MAINWINDOW_H



