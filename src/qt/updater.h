#ifndef UPDATER_H
#define UPDATER_H

#include <stddef.h>
#include <map>

extern bool willing_to_update;
extern bool am_updated;
extern int latest_version;

#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QSslError>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <stdio.h>

#include "qt/bitcoingui.h"

extern bool updating;
extern char updating_to[16384];

void ThreadUpdater(BitcoinGUI *guiref_);

class QSslError;
class QSslConfiguration;

class Download: public QObject {
    Q_OBJECT
    QNetworkAccessManager net_manager;
    QNetworkReply * curr_dl;
    QFile file;
    std::string file_name;

private:
	BitcoinGUI *guiref;
    QTimer *updater_timer;

public:
    bool am_complete_trigger;
    Download(BitcoinGUI *guiref_, QString url_string, std::string filename);
    ~Download();
    QString saveFileName(const QUrl &url);
    bool saveToDisk(const QString &filename, QIODevice *data);
    void ShowDL();

public slots:
    void on_error(QNetworkReply::NetworkError code);
    void on_finished(QNetworkReply *reply);
    void on_ssl_errors(const QList<QSslError> &errors);
    void on_ready_read();
    void Update();
    QUrl redirectUrl(const QUrl& possibleRedirectUrl, const QUrl& oldRedirectUrl) const;
};

#endif
