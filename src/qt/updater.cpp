#include "updater.h"
#include "util.h"
#include "compat.h"
#include "get_exe_path.h"
#include "init.h"

#include <inttypes.h>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

bool willing_to_update=false;
bool ready_to_update=false;
bool am_updated=false;
bool downloading_update=false;
bool updating=false;
char updating_to[16384] = {'\0'};
int32_t latest_version = -1;

#include <QCoreApplication>
#include <QSslError>
#include <QSslConfiguration>
#include "qt/overviewpage.h"

boost::filesystem::path TmpPath = boost::filesystem::temp_directory_path();

// constructor
Download::Download(BitcoinGUI *guiref_, QString url_string, std::string filename) {
	guiref = guiref_;
	file_name = filename;
	am_complete_trigger = false;

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
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setProtocol(QSsl::AnyProtocol);
    request.setSslConfiguration(config);
    QNetworkReply *reply = net_manager.get(request);
	QObject::connect(&net_manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(on_finished(QNetworkReply *)));
	QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(on_error(QNetworkReply::NetworkError)));
    QObject::connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(on_ssl_errors(QList<QSslError>)));
    QObject::connect(reply, SIGNAL(readyRead()), this, SLOT(on_ready_read()));

    LogPrintf("updater: Preparing file to write out to at: \"%s\"\n", filename.c_str());
    QFile file(QString(file_name.c_str()));
    if (!file.open(QIODevice::WriteOnly)) {
        LogPrintf("updater: Couldn't open \"%s\" for writing: %s\n", filename, qPrintable(file.errorString()));
        reply->deleteLater();
        return;
    }

    curr_dl = reply;
    return;
}

Download::~Download() {
}


void Download::on_ready_read() {
	file.write(curr_dl->readAll());
    file.close();
}

void Download::on_error(QNetworkReply::NetworkError code) {
	LogPrintf("updater: Error on dl attempt: (%s).\n", code);
}

inline std::string slurp (const std::string& path) {
	std::ostringstream buf; std::ifstream input (path.c_str()); buf << input.rdbuf(); return buf.str();
}

void Download::on_finished(QNetworkReply *reply) {
	QVariant possibleRedirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    /* We'll deduct if the redirection is valid in the redirectUrl function */
    QUrl _urlRedirectedTo = this->redirectUrl(possibleRedirectUrl.toUrl(), _urlRedirectedTo);
	QUrl url = reply->url();
    /* If the URL is not empty, we're being redirected. */
    if(!_urlRedirectedTo.isEmpty()) {
        QString text = QString("updater: Redirected to ").append(_urlRedirectedTo.toString()).append("\n");
        LogPrintf(qPrintable(text));
        //dl(_urlRedirectedTo.toString());
	    curr_dl->deleteLater();
    	return;
    }

    if (reply->error()) {
        LogPrintf("updater: Download of %s failed: %s\n", url.toEncoded().constData(), qPrintable(reply->errorString()));
   		curr_dl->deleteLater();
    	return;
    }

    file.write(curr_dl->readAll());
    file.close();
    LogPrintf("updater: Download of %s succeeded (saved to %s)\n", url.toEncoded().constData(), file_name);
    curr_dl->deleteLater();
    am_complete_trigger = true;
    return;
}

QUrl Download::redirectUrl(const QUrl& possibleRedirectUrl, const QUrl& oldRedirectUrl) const {
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


void Download::on_ssl_errors(const QList<QSslError> &sslErrors) {
    for (int i=0; i<sslErrors.count(); ++i) {
        LogPrintf("updater: SSL error: %s\n", qPrintable(sslErrors[i].errorString()));
    }
}
void Download::ShowDL() {
    QObject::connect(guiref->overviewPage->GetUpdateButton(), SIGNAL(clicked()), this, SLOT(Update()));
    guiref->overviewPage->HideUpdateText();
    guiref->overviewPage->ShowUpdateButton();
}

void Download::Update() {
	boost::filesystem::path tmp = boost::filesystem::unique_path();
	const std::string dst_str = get_exe_path();
	const std::string tmp_str = TmpPath.string()+tmp.string();
	const std::string src_str = TmpPath.string()+"Ember-qt.exe";
	LogPrintf("updater: Currently running exe: \"%s\"\n", dst_str);
	LogPrintf("updater: Temporary name for ^: \"%s\"\n", tmp_str);
	LogPrintf("updater: Newly downloaded exe: \"%s\"\n", src_str);
	remove(tmp_str.c_str()); // ignore return code

	LogPrintf("updater: Renaming currently running exe to temporary name.\n");
	rename(dst_str.c_str(),tmp_str.c_str());
	LogPrintf("updater: Copying newly downloaded to currently running name.\n");
	CopyFileA(src_str.c_str(),dst_str.c_str(),false);
	LogPrintf("updater: Getting the currently running name (who is now replaced).\n");
	strcpy(updating_to,dst_str.c_str());
	updating_to[16383] = '\0';
	updating = true;
	StartShutdown();
	return;
}


char* GetExeUrl(int version) {
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
    LogPrintf("updater: Got exe url as \"%s\".\n", str);
	return str;
}

void ThreadUpdater(BitcoinGUI *guiref_) {
    Download ver_dl(guiref_, "https://www.0xify.com/static/emb/win/x86/_latest_version.txt", TmpPath.string()+"Ember-latest-version.txt");
    while (!ver_dl.am_complete_trigger) {
    	MilliSleep(5*1000);
    }
    std::string latest_version_str = slurp(TmpPath.string()+"Ember-latest-version.txt");
    LogPrintf("updater: Latest version string of file contents: \"%s\"\n", latest_version_str);
	latest_version = std::stoi(latest_version_str);
	LogPrintf("updater: Here is the latest version: %d.\n", latest_version);
	if (latest_version >= 0) {
        if (ready_to_update) {
            LogPrintf("updater: Ready for upgrade process.\n");
        } else if (CLIENT_VERSION < latest_version) { // When true, we've got an upgrade ready to dl, and we haven't already
	  		LogPrintf("updater: Upgrade should be available on host...\n");
	  		char *s = GetExeUrl(latest_version);
	   		if (s == NULL) {
	  			LogPrintf("updater: Problem with building the URL for latest version.\n");
	   			return;
	   		} else {
	   			downloading_update = true;
    			Download dl(guiref_, s, TmpPath.string()+"Ember-qt.exe");
    			while (!dl.am_complete_trigger) {
    				MilliSleep(5*1000);
    			}
    			LogPrintf("updater: Got updated version downloaded and primed.\n");
				ready_to_update = true;
				am_updated = false;
				downloading_update = false;
                dl.ShowDL();
				LogPrintf("updater: We have shown the update button. Hint hint.\n");
				return;
    		}
    	} else {
    		LogPrintf("updater: We are latest version. Good.\n");
    		am_updated = true;
            guiref_->overviewPage->ShowUpdateText();
    	}
	}
}
