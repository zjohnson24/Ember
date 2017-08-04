// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "db.h"

#include "addrman.h"
#include "hash.h"
#include "util.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/version.hpp>
#include <openssl/rand.h>
#include <string.h>


using namespace std;
using namespace boost;


unsigned int nWalletDBUpdated;

//
// CDB
//

CDBEnv bitdb;

void CDBEnv::EnvShutdown()
{
    if (!fDbEnvInit)
        return;

    fDbEnvInit = false;
    int ret = dbenv->close(dbenv, 0);
    if (ret != 0)
        LogPrintf("EnvShutdown exception: %s (%d)\n", db_strerror(ret), ret);
    if (!fMockDb)
		dbenv->close(dbenv, 0);
}

CDBEnv::CDBEnv() {
	int ret;
	if ((ret = db_env_create(&dbenv, 0)) != 0) {
		cerr << "db_env_create error:" << db_strerror(ret) << endl;
		exit(1);
	}
    fDbEnvInit = false;
    fMockDb = false;
}

CDBEnv::~CDBEnv()
{
    EnvShutdown();
}

void CDBEnv::Close()
{
    EnvShutdown();
}

bool CDBEnv::Open(boost::filesystem::path pathEnv_)
{
    if (fDbEnvInit)
        return true;

    boost::this_thread::interruption_point();

    pathEnv = pathEnv_;
    filesystem::path pathDataDir = pathEnv;
    strPath = pathDataDir.generic_string();
    filesystem::path pathLogDir = pathDataDir / "database";
    filesystem::create_directory(pathLogDir);
    filesystem::path pathErrorFile = pathDataDir / "db.log";
    LogPrintf("dbenv.open LogDir=%s ErrorFile=%s\n", pathLogDir.generic_string(), pathErrorFile.generic_string());

    //int nDbCache = GetArg("-dbcache", 25);
	dbenv->set_lg_dir(dbenv, pathLogDir.generic_string().c_str());
    //dbenv.set_cachesize(nDbCache / 1024, (nDbCache % 1024)*1048576, 1);
    //dbenv.set_lg_bsize(1048576);
    //dbenv.set_lg_max(10485760);
    //dbenv.set_lk_max_locks(10000);
    //dbenv.set_lk_max_objects(10000);
	dbenv->set_errfile(dbenv, fopen(pathErrorFile.generic_string().c_str(), "a")); /// debug
//#ifdef DB_LOG_AUTO_REMOVE
//	dbenv.log_set_config(DB_LOG_AUTO_REMOVE, 1);
//#endif
	const std::string& string_thing = pathDataDir.generic_string();
	LogPrintf("string_thing: '%s'\n", string_thing);

	const char *str_p = string_thing.c_str();
	LogPrintf("str_p: %s\n", str_p);

    int ret = dbenv->open(dbenv, str_p,
					 DB_CREATE      |
                     DB_INIT_LOCK   |
                     DB_INIT_LOG    |
                     DB_INIT_MPOOL  |
                     DB_INIT_TXN    |
                     DB_THREAD      |
                     DB_RECOVER     |
				     DB_AUTO_COMMIT | 
		             DB_PRIVATE,
                     S_IRUSR | S_IWUSR);
    if (ret != 0)
        return error("CDB() : error %s (%d) opening database environment", db_strerror(ret), ret);

    fDbEnvInit = true;
    fMockDb = false;

    return true;
}

void CDBEnv::MakeMock()
{
    if (fDbEnvInit)
        throw runtime_error("CDBEnv::MakeMock(): already initialized");

    boost::this_thread::interruption_point();

    LogPrint("db", "CDBEnv::MakeMock()\n");

    //dbenv.set_cachesize(1, 0, 1);
    //dbenv.set_lg_bsize(10485760*4);
    //dbenv.set_lg_max(10485760);
    //dbenv.set_lk_max_locks(10000);
    //dbenv.set_lk_max_objects(10000);
#ifdef DB_LOG_IN_MEMORY
    dbenv->log_set_config(dbenv, DB_LOG_IN_MEMORY, 1);
#endif
    int ret = dbenv->open(dbenv, NULL,
                     DB_CREATE      |
                     DB_INIT_LOCK   |
                     DB_INIT_LOG    |
                     DB_INIT_MPOOL  |
                     DB_INIT_TXN    |
                     DB_THREAD      |
					 DB_AUTO_COMMIT |
                     DB_PRIVATE,
                     S_IRUSR | S_IWUSR);
    if (ret > 0)
        throw runtime_error(strprintf("CDBEnv::MakeMock(): error %d opening database environment", ret));

    fDbEnvInit = true;
    fMockDb = true;
}

CDBEnv::VerifyResult CDBEnv::Verify(std::string strFile, bool (*recoverFunc)(CDBEnv& dbenv, std::string strFile))
{
    LOCK(cs_db);
    assert(mapFileUseCount.count(strFile) == 0);

	DB *db = NULL;
	int ret = db_create(&db, dbenv, 0);
	if (ret != 0) {
		LogPrintf("Verify Failed for: %s\n", strFile.c_str());
		return RECOVER_FAIL;
	}
    int result = db->verify(db, strFile.c_str(), NULL, NULL, 0);
	if (result == 0) {
		return VERIFY_OK;
	} else if (recoverFunc == NULL) {
		return RECOVER_FAIL;
	}

    // Try to recover:
    bool fRecovered = (*recoverFunc)(*this, strFile);
    return (fRecovered ? RECOVER_OK : RECOVER_FAIL);
}

int get_tmp_filename(char *p_filename, char *p_basepath, char *p_tmp_filename, uint64_t tmp_filename_size) {
	wchar_t tmp_path[_MAX_PATH] = { 0 };
	LPWSTR tmp_path_p = &tmp_path[0];
	wchar_t tmp_name[_MAX_PATH] = { 0 };
	LPWSTR tmp_name_p = &tmp_name[0];

	// Parameter Validation
	if (p_tmp_filename == NULL) {
		return -1; // FAILURE_NULL_ARGUMENT
	}

	// Get a basepath
	if (p_basepath != NULL) {
		wcscpy_s(&tmp_path[0], _MAX_PATH, p_basepath);
	} else { // Use the CWD if a basepath wasn't supplied
		wcscpy_s(&tmp_path[0], _MAX_PATH, ".\\");
	}
	if (!directory_exists(tmp_path)) {
		return -4; // FAILURE_INVALID_PATH
	}

	// Form the full filename
	if (p_filename != NULL)	{
		wcscpy_s(&tmp_name[0], MAX_PATH, &tmp_path[0]);
		wcscat_s(&tmp_name[0], MAX_PATH, "\\");
		wcscat_s(tmp_name, MAX_PATH, p_filename);
	} else { // Get a temporary filename if one wasn't supplied
		if (GetTempFileName(tmp_path_p, NULL, 0, tmp_name_p) == 0) {
			LogPrintf("Error getting temporary filename in %s.\n", tmp_path);
			return -3; // FAILURE_API_CALL
		}
	}

	// Copy over the result
	switch (strcpy_s(p_tmp_filename, tmp_filename_size, tmp_name)) {
	case 0:
		// Make sure that the file doesn't already exist before we suggest it as a tempfile.
		// They will still get the name in-case they intend to use it, but they have been warned.
		if (file_exists(tmp_name)) {
			return -5; // FAILURE_FILE_ALREADY_EXISTS
		}
		return 0;
		break;
	case ERANGE:
		return -2; // FAILURE_INSUFFICIENT_BUFFER
		break;
	default:
		return -3; // FAILURE_API_CALL
		break;
	}
}

bool CDBEnv::Salvage(std::string strFile, bool fAggressive,
                     std::vector<CDBEnv::KeyValPair >& vResult)
{
    LOCK(cs_db);
    assert(mapFileUseCount.count(strFile) == 0);

    u_int32_t flags = DB_SALVAGE;
    if (fAggressive) flags |= DB_AGGRESSIVE;

	HANDLE h_file;
	int ret;
	char tmpfilename[_MAX_PATH] = { 0 };

	int ret = get_tmp_filename(NULL, NULL, &tmpfilename[0], _MAX_PATH);
	if (ret != 0)	{
		LogPrintf("Error retrieveng tmp name for salvage operation: %i", ret);

	}

	// Extract the DLL to disk
	h_file = CreateFile(tmpfilename,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_DELETE_ON_CLOSE,
		NULL);
	if (h_file == INVALID_HANDLE_VALUE)
	{
		_ftprintf(stderr, TEXT("Error creating temporary file %s.\n"), tmpfilename);
		return GetLastError();
	}

	DB *db = NULL;
	int ret = db_create(&db, dbenv, 0);
	if (ret != 0) {
		LogPrintf("Error: Cannot open %s\n", strFile.c_str());
		return false;
	}
    int result = db->verify(db, strFile.c_str(), NULL, strDump, flags);
    if (result == DB_VERIFY_BAD)
    {
        LogPrintf("Error: Salvage found errors, all data may not be recoverable.\n");
        if (!fAggressive)
        {
            LogPrintf("Error: Rerun with aggressive mode to ignore errors and continue.\n");
            return false;
        }
    }
    if (result != 0 && result != DB_VERIFY_BAD)
    {
        LogPrintf("ERROR: db salvage failed: %d\n",result);
        return false;
    }

    // Format of bdb dump is ascii lines:
    // header lines...
    // HEADER=END
    // hexadecimal key
    // hexadecimal value
    // ... repeated
    // DATA=END

    string strLine;
    while (!strDump.eof() && strLine != "HEADER=END")
        getline(strDump, strLine); // Skip past header

    std::string keyHex, valueHex;
    while (!strDump.eof() && keyHex != "DATA=END")
    {
        getline(strDump, keyHex);
        if (keyHex != "DATA_END")
        {
            getline(strDump, valueHex);
            vResult.push_back(make_pair(ParseHex(keyHex),ParseHex(valueHex)));
        }
    }

    return (result == 0);
}


void CDBEnv::CheckpointLSN(const std::string& strFile)
{
    dbenv.txn_checkpoint(0, 0, 0);
    if (fMockDb)
        return;
    dbenv.lsn_reset(strFile.c_str(), 0);
}


CDB::CDB(const std::string& strFilename, const char* pszMode) :
    pdb(NULL), activeTxn(NULL)
{
    int ret;
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));
    if (strFilename.empty())
        return;

    bool fCreate = strchr(pszMode, 'c');
    unsigned int nFlags = DB_THREAD;
    if (fCreate)
        nFlags |= DB_CREATE;

    {
        LOCK(bitdb.cs_db);
        if (!bitdb.Open(GetDataDir()))
            throw runtime_error("env open failed");

        strFile = strFilename;
        ++bitdb.mapFileUseCount[strFile];
        pdb = bitdb.mapDb[strFile];
        if (pdb == NULL)
        {
            pdb = new Db(&bitdb.dbenv, 0);

            bool fMockDb = bitdb.IsMock();
            if (fMockDb)
            {
                DbMpoolFile*mpf = pdb->get_mpf();
                ret = mpf->set_flags(DB_MPOOL_NOFILE, 1);
                if (ret != 0)
                    throw runtime_error(strprintf("CDB : Failed to configure for no temp file backing for database %s", strFile));
            }

            ret = pdb->open(NULL, // Txn pointer
                            fMockDb ? NULL : strFile.c_str(), // Filename
                            fMockDb ? strFile.c_str() : "main", // Logical db name
                            DB_BTREE, // Database type
                            nFlags, // Flags
                            0);

            if (ret != 0)
            {
                delete pdb;
                pdb = NULL;
                --bitdb.mapFileUseCount[strFile];
                strFile = "";
                throw runtime_error(strprintf("CDB : Error %d, can't open database %s", ret, strFile));
            }

            if (fCreate && !Exists(string("version")))
            {
                bool fTmp = fReadOnly;
                fReadOnly = false;
                WriteVersion(CLIENT_VERSION);
                fReadOnly = fTmp;
            }

            bitdb.mapDb[strFile] = pdb;
        }
    }
}

void CDB::Close()
{
    if (!pdb)
        return;
    if (activeTxn)
        activeTxn->abort();
    activeTxn = NULL;
    pdb = NULL;

    // Flush database activity from memory pool to disk log
    unsigned int nMinutes = 0;
    if (fReadOnly)
        nMinutes = 1;

    bitdb.dbenv.txn_checkpoint(nMinutes ? GetArg("-dblogsize", 100)*1024 : 0, nMinutes, 0);

    {
        LOCK(bitdb.cs_db);
        --bitdb.mapFileUseCount[strFile];
    }
}

void CDBEnv::CloseDb(const string& strFile)
{
    {
        LOCK(cs_db);
        if (mapDb[strFile] != NULL)
        {
            // Close the database handle
            Db* pdb = mapDb[strFile];
            pdb->close(0);
            delete pdb;
            mapDb[strFile] = NULL;
        }
    }
}

bool CDBEnv::RemoveDb(const string& strFile)
{
    this->CloseDb(strFile);

    LOCK(cs_db);
    int rc = dbenv.dbremove(NULL, strFile.c_str(), NULL, DB_AUTO_COMMIT);
    return (rc == 0);
}

bool CDB::Rewrite(const string& strFile, const char* pszSkip)
{
    while (true)
    {
        {
            LOCK(bitdb.cs_db);
            if (!bitdb.mapFileUseCount.count(strFile) || bitdb.mapFileUseCount[strFile] == 0)
            {
                // Flush log data to the dat file
                bitdb.CloseDb(strFile);
                bitdb.CheckpointLSN(strFile);
                bitdb.mapFileUseCount.erase(strFile);

                bool fSuccess = true;
                LogPrintf("Rewriting %s...\n", strFile);
                string strFileRes = strFile + ".rewrite";
                { // surround usage of db with extra {}
                    CDB db(strFile.c_str(), "r");
                    Db* pdbCopy = new Db(&bitdb.dbenv, 0);

                    int ret = pdbCopy->open(NULL,                 // Txn pointer
                                            strFileRes.c_str(),   // Filename
                                            "main",    // Logical db name
                                            DB_BTREE,  // Database type
                                            DB_CREATE,    // Flags
                                            0);
                    if (ret > 0)
                    {
                        LogPrintf("Cannot create database file %s\n", strFileRes);
                        fSuccess = false;
                    }

                    Dbc* pcursor = db.GetCursor();
                    if (pcursor)
                        while (fSuccess)
                        {
                            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
                            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
                            int ret = db.ReadAtCursor(pcursor, ssKey, ssValue, DB_NEXT);
                            if (ret == DB_NOTFOUND)
                            {
                                pcursor->close();
                                break;
                            }
                            else if (ret != 0)
                            {
                                pcursor->close();
                                fSuccess = false;
                                break;
                            }
                            if (pszSkip &&
                                strncmp(&ssKey[0], pszSkip, std::min(ssKey.size(), strlen(pszSkip))) == 0)
                                continue;
                            if (strncmp(&ssKey[0], "\x07version", 8) == 0)
                            {
                                // Update version:
                                ssValue.clear();
                                ssValue << CLIENT_VERSION;
                            }
                            Dbt datKey(&ssKey[0], ssKey.size());
                            Dbt datValue(&ssValue[0], ssValue.size());
                            int ret2 = pdbCopy->put(NULL, &datKey, &datValue, DB_NOOVERWRITE);
                            if (ret2 > 0)
                                fSuccess = false;
                        }
                    if (fSuccess)
                    {
                        db.Close();
                        bitdb.CloseDb(strFile);
                        if (pdbCopy->close(0))
                            fSuccess = false;
                        delete pdbCopy;
                    }
                }
                if (fSuccess)
                {
                    Db dbA(&bitdb.dbenv, 0);
                    if (dbA.remove(strFile.c_str(), NULL, 0))
                        fSuccess = false;
                    Db dbB(&bitdb.dbenv, 0);
                    if (dbB.rename(strFileRes.c_str(), NULL, strFile.c_str(), 0))
                        fSuccess = false;
                }
                if (!fSuccess)
                    LogPrintf("Rewriting of %s FAILED!\n", strFileRes);
                return fSuccess;
            }
        }
        MilliSleep(100);
    }
    return false;
}


void CDBEnv::Flush(bool fShutdown)
{
    int64_t nStart = GetTimeMillis();
    // Flush log data to the actual data file
    //  on all files that are not in use
    LogPrint("db", "Flush(%s)%s\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " db not started");
    if (!fDbEnvInit)
        return;
    {
        LOCK(cs_db);
        map<string, int>::iterator mi = mapFileUseCount.begin();
        while (mi != mapFileUseCount.end())
        {
            string strFile = (*mi).first;
            int nRefCount = (*mi).second;
            LogPrint("db", "%s refcount=%d\n", strFile, nRefCount);
            if (nRefCount == 0)
            {
                // Move log data to the dat file
                CloseDb(strFile);
                LogPrint("db", "%s checkpoint\n", strFile);
                dbenv.txn_checkpoint(0, 0, 0);
                LogPrint("db", "%s detach\n", strFile);
                if (!fMockDb)
                    dbenv.lsn_reset(strFile.c_str(), 0);
                LogPrint("db", "%s closed\n", strFile);
                mapFileUseCount.erase(mi++);
            }
            else
                mi++;
        }
        LogPrint("db", "DBFlush(%s)%s ended %15dms\n", fShutdown ? "true" : "false", fDbEnvInit ? "" : " db not started", GetTimeMillis() - nStart);
        if (fShutdown)
        {
            char** listp;
            if (mapFileUseCount.empty())
            {
                dbenv.log_archive(&listp, DB_ARCH_REMOVE);
                Close();
            }
        }
    }
}
