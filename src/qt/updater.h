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
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <stdio.h>

#include "qt/bitcoingui.h"

//class QSslError;

class DownloadManager: public QObject {
    Q_OBJECT
    QNetworkAccessManager net_manager;
    QList<QNetworkReply *> currentDownloads;

private:
	BitcoinGUI *guiref;

public:
    DownloadManager(BitcoinGUI *guiref_);
    virtual ~DownloadManager() {};
    QString saveFileName(const QUrl &url);
    bool saveToDisk(const QString &filename, QIODevice *data);
    char* GetExeUrl(int version);

public slots:
    void dl(QString url_string);
    void on_error(QNetworkReply::NetworkError code);
    void on_finished(QNetworkReply *reply);
    void Update();
    void Updater();
    QUrl redirectUrl(const QUrl& possibleRedirectUrl, const QUrl& oldRedirectUrl) const;
    //void sslErrors(const QList<QSslError> &errors);
};

#endif