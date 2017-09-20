#include "updater.h"
#include "util.h"
#include "compat.h"
#include "get_exe_path.h"

#include <inttypes.h>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

bool willing_to_update=false;
bool ready_to_update=false;
bool am_updated=false;
int32_t latest_version = -1;

#include <QCoreApplication>
#include <QSslError>
#include "qt/overviewpage.h"

boost::filesystem::path TmpPath = boost::filesystem::temp_directory_path();

// constructor
DownloadManager::DownloadManager(BitcoinGUI *guiref_) {
	guiref = guiref_;
	QTimer *updater_timer = new QTimer(guiref);
  	QObject::connect(updater_timer, SIGNAL(timeout()), this, SLOT(Updater()));
    updater_timer->start(1000*60*60);
    Updater();
}

void DownloadManager::dl(QString url_string) {
    // QString::toLocal8Bit()
    //  - local 8-bit representation of the string as a QByteArray
    // Qurl::fromEncoded(QByteArray)
    //  - Parses input and returns the corresponding QUrl.
    //    input is assumed to be in encoded form,
    //    containing only ASCII characters.
	LogPrintf("updater: Downloading %s\n", qPrintable(url_string));
    QUrl url = QUrl::fromEncoded(url_string.toLocal8Bit());

    // makes a request
    QNetworkRequest request(url);
    QNetworkReply *reply = net_manager.get(request);
	QObject::connect(&net_manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(on_finished(QNetworkReply *)));
	QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(on_error(QNetworkReply::NetworkError)));
    //QObject::connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrors(QList<QSslError>)));

    // List of reply
    currentDownloads.append(reply);
}

void DownloadManager::on_error(QNetworkReply::NetworkError code) {
	LogPrintf("updater: Error on dl attempt: (%s).\n", code);
}

void DownloadManager::on_finished(QNetworkReply *reply) {

	QVariant possibleRedirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    /* We'll deduct if the redirection is valid in the redirectUrl function */
    QUrl _urlRedirectedTo = this->redirectUrl(possibleRedirectUrl.toUrl(), _urlRedirectedTo);
	QUrl url = reply->url();
    /* If the URL is not empty, we're being redirected. */
    if(!_urlRedirectedTo.isEmpty()) {
        QString text = QString("updater: Redirected to ").append(_urlRedirectedTo.toString()).append("\n");
        LogPrintf(qPrintable(text));
        dl(_urlRedirectedTo.toString());
        currentDownloads.removeAll(reply);
	    reply->deleteLater();
    	return;
    }

    if (reply->error()) {
        LogPrintf("updater: Download of %s failed: %s\n", url.toEncoded().constData(), qPrintable(reply->errorString()));
        currentDownloads.clear();
   		reply->deleteLater();
    	return;
    }

    LogPrintf("updater: Finished a download of %s\n", reply->url().toEncoded().constData());
	QStringList pieces = url.toString().split("/");
	QString piece = pieces.value(pieces.length() - 1);
	std::string filename = TmpPath.string() + piece.toStdString();
	LogPrintf("updater: Writing out to %s\n", filename.c_str());
    QFile file(QString(filename.c_str()));
	if (!file.open(QIODevice::WriteOnly)) {
    	LogPrintf("updater: Couldn't open %s for writing: %s\n", filename, qPrintable(file.errorString()));
   	    currentDownloads.clear();
	    reply->deleteLater();
    	return;
	}
	file.write(reply->readAll());
	file.close();
    LogPrintf("updater: Download of %s succeeded (saved to %s)\n", url.toEncoded().constData(), filename);
    if (filename == "_latest_version.txt") {
    	boost::filesystem::path ver_tmp = boost::filesystem::unique_path();
		const std::string ver_tmp_str = ver_tmp.string();
	  	FILE *fp_v = fopen(ver_tmp_str.c_str(), "wb+");
		fscanf(fp_v, "%d", &latest_version);
		fclose(fp_v);
		LogPrintf("updater: Got the latest version: %d.\n", latest_version);
		if (latest_version >= 0) {
	    	if (CLIENT_VERSION < latest_version) { // When true, we've got an upgrade ready
	    		LogPrintf("updater: Upgrade should be available on host...\n");
	    		char *s = GetExeUrl(latest_version);
	    		if (s == NULL) {
	    			LogPrintf("updater: Problem with building the URL for latest version.\n");
	    		} else {
	    			dl(s);
	    			currentDownloads.removeAll(reply);
	    			reply->deleteLater();
    				return;
	    		}
	    	} else {
	    		LogPrintf("updater: We are latest version. Good.\n");
	    		am_updated = true;
	    		guiref->overviewPage->ShowUpdateText();
	    	}
		}
    } else if (0 == strncmp(filename.c_str(), "Ember-qt", strlen("Ember-qt"))) {
    	LogPrintf("updater: Got updated version.\n");
    	std::string updatename = TmpPath.string() + "Ember-qt.exe";
    	rename(filename.c_str(), updatename.c_str());
    	LogPrintf("updater: Fixed exe into upgrade readiness.\n");
    	ready_to_update = true;
    	am_updated = false;
    	guiref->overviewPage->HideUpdateText();
    	connect(guiref->overviewPage->GetUpdateButton(), SIGNAL(clicked()), this, SLOT(Update()));
    	guiref->overviewPage->ShowUpdateButton();
    	LogPrintf("updater: We have shown the update button. Hint hint.\n");
    }

    currentDownloads.clear();
    reply->deleteLater();
    return;
}

QUrl DownloadManager::redirectUrl(const QUrl& possibleRedirectUrl, const QUrl& oldRedirectUrl) const {
    QUrl redirectUrl;
    /*
     * Check if the URL is empty and
     * that we aren't being fooled into a infinite redirect loop.
     * We could also keep track of how many redirects we have been to
     * and set a limit to it, but we'll leave that to you.
     */
    if(!possibleRedirectUrl.isEmpty() &&
       possibleRedirectUrl != oldRedirectUrl) {
        redirectUrl = possibleRedirectUrl;
    }
    return redirectUrl;
}

/*
void DownloadManager::sslErrors(const QList<QSslError> &sslErrors) {
    for (int i=0; i<sslErrors.count(); ++i) {
        LogPrintf("updater: SSL error: %s\n", qPrintable(sslErrors[i].errorString()));
    }
}
*/

char* DownloadManager::GetExeUrl(int version) {
	char *str = NULL;
	int res = 0;

	if (NULL == (str = (char*)malloc(1024))) {
		LogPrintf("updater: GetExeUrl() out of memory!\n");
		return NULL;
	}
	if (0 > (res = sprintf(str, "https://www.0xify.com/static/emb/win/x86/Ember-qt-%d.exe", version))) {
		LogPrintf("updater: GetExeUrl() couldn't sprintf()!\n");
		return NULL;
	}
	str[1023] = '\0';

	return str;
}

void DownloadManager::Update() {
  boost::filesystem::path tmp = boost::filesystem::unique_path();

  const std::string temp = tmp.string();
  remove(temp.c_str()); // ignore return code
  const std::string src_str = TmpPath.string() + "Ember-qt.exe";
  std::string src = src_str;
  const std::string dst_str = get_exe_path() + "Ember-qt.exe";
  LogPrintf("");
  std::string dst = dst_str;

  rename(dst.c_str(),temp.c_str());
  CopyFileA(src.c_str(),dst.c_str(),false);
  static char buffer[16384];
  strcpy(buffer,dst.c_str());

  /* CreateProcess API initialization */
  STARTUPINFOA siStartupInfo;
  PROCESS_INFORMATION piProcessInfo;
  memset(&siStartupInfo, 0, sizeof(siStartupInfo));
  memset(&piProcessInfo, 0, sizeof(piProcessInfo));
  siStartupInfo.cb = sizeof(siStartupInfo);

  CreateProcessA(buffer, // application name/path
    NULL, // command line (optional)
    NULL, // no process attributes (default)
    NULL, // default security attributes
    false,
    CREATE_DEFAULT_ERROR_MODE | CREATE_NEW_CONSOLE,
    NULL, // default env
    NULL, // default working dir
    &siStartupInfo,
    &piProcessInfo);


  TerminateProcess( GetCurrentProcess(),0);
  ExitProcess(0); // exit this process

  // this does not return.
}

void DownloadManager::Updater() {
	std::string latest_version_url = "http://www.0xify.com/static/emb/win/x86/_latest_version.txt";
	dl(latest_version_url.c_str());
}
