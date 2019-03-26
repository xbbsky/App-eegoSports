#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream> 
#include "wrapper.cc"
#include <QDebug>
#include <algorithm>

using namespace eemagine::sdk;

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
MainWindow::MainWindow(QWidget *parent, const std::string &config_file, const bool linkOnStart) : QMainWindow(parent)
{
    qRegisterMetaType<QVector<double>>("QVector<double>");
    reader = nullptr;
	ui.setupUi(this);

	// make GUI connections
	QObject::connect(ui.actionLoad_Configuration, SIGNAL(triggered()), this, SLOT(load_config_dialog()));
	QObject::connect(ui.actionSave_Configuration, SIGNAL(triggered()), this, SLOT(save_config_dialog()));
	QObject::connect(ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));
	QObject::connect(ui.linkButton, SIGNAL(clicked()), this, SLOT(link()));
	if(linkOnStart) {
		link();
	}
}

void MainWindow::load_config_dialog() {
	QString sel = QFileDialog::getOpenFileName(this, "Load Configuration File", "", "Configuration Files (*.cfg)");
	if (!sel.isEmpty())
		load_config(sel.toStdString());
}

void MainWindow::save_config_dialog() {
	QString sel = QFileDialog::getSaveFileName(this, "Save Configuration File", "", "Configuration Files (*.cfg)");
	if (!sel.isEmpty())
		save_config(sel.toStdString());
}

void MainWindow::closeEvent(QCloseEvent *ev) {
    if (reader != nullptr) {
		ev->ignore();
	}
}

void MainWindow::load_config(const std::string &filename) {
	using boost::property_tree::ptree;
	ptree pt;

	// parse file
	try {
		read_xml(filename, pt);
	}
	catch (std::exception &e) {
		QMessageBox::information(this, "Error", (std::string("Cannot read config file: ") += e.what()).c_str(), QMessageBox::Ok);
		return;
	}

	// get config values
	try {
		ui.samplingRate->setCurrentIndex(pt.get<int>("settings.samplingrate", 3));

	}
	catch (std::exception &) {
		QMessageBox::information(this, "Error in Config File", "Could not read out config parameters.", QMessageBox::Ok);
		return;
	}
}

void MainWindow::save_config(const std::string &filename) {
	using boost::property_tree::ptree;
	ptree pt;

	// transfer UI content into property tree
	try {
		pt.put("settings.samplingrate", ui.samplingRate->currentIndex());
	}
	catch (std::exception &e) {
		QMessageBox::critical(this, "Error", (std::string("Could not prepare settings for saving: ") += e.what()).c_str(), QMessageBox::Ok);
	}

	// write to disk
	try {
		write_xml(filename, pt);
	}
	catch (std::exception &e) {
		QMessageBox::critical(this, "Error", (std::string("Could not write to config file: ") += e.what()).c_str(), QMessageBox::Ok);
	}
}


void MainWindow::link() {
    static int status = 0;
    if (reader != nullptr) {
		reader->setStop(true);
        if(status==1){
            ui.linkButton->setText("Stop Sending Data");
            status=0;
        }else{
            ui.linkButton->setText("Link");
        }
	}
	else {
		// === perform link action ===
		try {
			// get the UI parameters...
			QString sr = ui.samplingRate->currentText();
			int samplingRate = sr.toInt();// boost::lexical_cast<int>(sr);
			
			thread = new QThread;
			reader = new Reader();

			reader->setParams(samplingRate);

			reader->moveToThread(thread);
			connect(thread, SIGNAL(started()), reader, SLOT(read()));
			connect(reader, SIGNAL(finished()), thread, SLOT(quit()));
			connect(thread, SIGNAL(finished()), this, SLOT(threadFinished()));
			connect(reader, SIGNAL(finished()), reader, SLOT(deleteLater()));
			connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
			connect(reader, SIGNAL(timeout()), this, SLOT(threadTimeout()));
			connect(reader, SIGNAL(connectionLost()), this, SLOT(connectionLost()));
			connect(reader, SIGNAL(ampNotFound()), this, SLOT(ampNotFound()));
            connect(reader, &Reader::displayImp, this, &MainWindow::displayImp);
			thread->start();
		}
		catch (std::exception &e) {
			// try to decode the error message
			std::string msg = "Could not query driver message because the device is not open";
			QMessageBox::critical(this, "Error", ("Could not initialize the eego Sport interface: " + (e.what() + (" (driver message: " + msg + ")"))).c_str(), QMessageBox::Ok);
			return;
		}

		// done, all successful
        ui.linkButton->setText("Stop Preparing");
        status = 1;
	}
}
void MainWindow::displayImp(QVector<double> impData){
    if(impData.size()!=imps.size()){
        for(int i=0;i<imps.size();i++){
            imps[i]->deleteLater();
        }
        imps.clear();
        imps.resize(impData.size());
        for(int i=0;i<impData.size();i++){
            imps[i]=new QLabel(ui.groupBox);
        }
    }
    for(int i=0;i<impData.size();i++){
        imps[i]->setNum(impData[i]);
    }
    return;
}
void MainWindow::threadFinished() {
    reader = nullptr;
	delete thread;
    thread = nullptr;
	ui.linkButton->setText("Link");
}

void MainWindow::threadTimeout() {
	QMessageBox::critical(this, "Error", (std::string("Error: eego Sport Conncetion timed out")).c_str(), QMessageBox::Ok);
}

void MainWindow::ampNotFound() {
	QMessageBox::critical(this, "Error", (std::string("Error: Amp not found or license file not present.Please connect the amplifier and make sure that the amp is turned on and a license file is present in 'My Documents/eego/")).c_str(), QMessageBox::Ok);
}

void MainWindow::connectionLost() {
	QMessageBox::critical(this, "Error", (std::string("Error: Amp connection lost")).c_str(), QMessageBox::Ok);
}


// --- CONSTRUCTOR ---
Reader::Reader() {
	stop = false;
	// you could copy data from constructor arguments to internal variables here.
}

// --- DECONSTRUCTOR ---
Reader::~Reader() {
	// free resources
}

void Reader::setParams(int samplingRate) {
	this->samplingRate = samplingRate;
}

void Reader::setParams(QString chs) {
    auto ss = std::stringstream(chs.toStdString());
    channels.clear();
    int num;
    while(ss>>num){
        channels.push_back(num);
    }
}

// --- PROCESS ---

// Start processing data.
void Reader::read() {
	try {
        qDebug()<<"start to construct";
        fact.reset(new factory());
        qDebug()<<"constructed";
        amp.reset(fact->getAmplifier());

        // get imp data
        qDebug()<<"Getting impStream";
        std::shared_ptr<stream> impStream(amp->OpenImpedanceStream());
        qDebug()<<"Get impStream";
        // get impedance data
        while(!stop){
            auto buf = impStream->getData();
            //qDebug()<<buf.getChannelCount()<<buf.getSampleCount();
            auto data = buf.data();
            auto max_value = std::max_element(data, data+32);
            auto min_value = std::min_element(data, data+32);
            qDebug()<<min_value-data<<"\t"<<*min_value<<"\t"<<max_value-data<<"\t"<<*max_value;
        }
        impStream.reset();
        setStop(false);
        
        eegStream.reset(amp->OpenEegStream(samplingRate));
		std::vector<channel> channelList = eegStream->getChannelList();
        for(auto ch:channelList){
            qDebug()<<ch.getType()<<ch.getIndex();
        }

		// create data streaminfo and append some meta-data
		lsl::stream_info data_info("eegoSports " + amp->getSerialNumber(), "EEG", channelList.size() - 2, samplingRate, lsl::cf_float32, "eegoSports_" + amp->getSerialNumber());
		lsl::xml_element channels = data_info.desc().append_child("channels");

		for (int k = 0; k < channelList.size() - 2; k++) {
			channels.append_child("channel")
                .append_child_value("label", "Ch" + std::to_string(k))
				.append_child_value("type", "EEG")
				.append_child_value("unit", "microvolts");
            //qDebug()<<("ch"+std::to_string(k)).c_str();
		}
		data_info.desc().append_child("acquisition")
			.append_child_value("manufacturer", "antneuro")
            //.append_child_value("serial_number", boost::lexical_cast<std::string>(amp->getSerialNumber()).c_str());
            .append_child_value("serial_number", amp->getSerialNumber());

		// make a data outlet
		lsl::stream_outlet data_outlet(data_info);

		// create marker streaminfo and outlet
		lsl::stream_info marker_info("eegoSports-" + amp->getSerialNumber() + "_markers" + "Markers", "Markers", 1, 0, lsl::cf_string, "eegoSports_" + boost::lexical_cast<std::string>(amp->getSerialNumber()) + "_markers");
		lsl::stream_outlet marker_outlet(marker_info);

		while (!stop) {
			buffer buffer = eegStream->getData();
            auto channelCount = buffer.getChannelCount();
            auto sampleCount = buffer.size() / channelCount;

			std::vector<std::vector<float>> send_buffer(sampleCount, std::vector<float>(channelCount - 2));
			for (int s = 0; s < sampleCount; s++) {
				for (int c = 0; c < channelCount - 2; c++) {
					send_buffer[s][c] = buffer.getSample(c, s);
				}
			}
			double now = lsl::local_clock();
			data_outlet.push_chunk(send_buffer, now);

            if(sampleCount>0){
                qDebug()<<now<<"\t"<<channelCount<<"\t"<<sampleCount<<"\t"<<buffer.getSampleCount();
            }

			int last_mrk = 0;
			for (int s = 0; s < sampleCount; s++) {
				//if (int mrk = src_buffer[channelCount + s*(channelCount + 1)]) {
				int mrk = buffer.getSample(channelCount - 2, s);
				if (mrk != last_mrk) {
					std::string mrk_string = boost::lexical_cast<std::string>(mrk);
					marker_outlet.push_sample(&mrk_string, now + (s + 1 - sampleCount) / samplingRate);
					last_mrk = mrk;
				}
			}


		}
	}
    catch (exceptions::notFound &e) {
        qDebug()<<"notFound:"<<e.what();
		emit ampNotFound();
	}
    catch (exceptions::notConnected &e) {
        qDebug()<<"notConnected:"<<e.what();
		emit connectionLost();
	}
    catch (exceptions::incorrectValue &e){
        qDebug()<<"incorrectValue:"<<e.what();
    }
    catch (exceptions::alreadyExists &e){
        qDebug()<<"alreadyExists:"<<e.what();
    }

	catch (std::exception &e) {
		int i = 0;
        qDebug()<<"unknown:"<<e.what();
	}
	emit finished();
}

MainWindow::~MainWindow() {
}
