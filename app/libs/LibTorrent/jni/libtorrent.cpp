//-----------------------------------------------------------------------------
#include "libtorrent.h"
//-----------------------------------------------------------------------------
#include "libtorrent/bencode.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/size_type.hpp"
//-----------------------------------------------------------------------------
#include "boost/filesystem.hpp"
//-----------------------------------------------------------------------------
void JniToStdString(JNIEnv *env, std::string* StdString, jstring JniString);
//-----------------------------------------------------------------------------
class TorrentFileInfo {
public:
	std::string SavePath;
	std::string TorrentFileName;
	std::string ContentFileName;
public:
	TorrentFileInfo(JNIEnv *env, jstring savePath, jstring torrentFile){
		JniToStdString(env, &SavePath, savePath);
		JniToStdString(env, &TorrentFileName, torrentFile);
		SetContentFileName();
	}
	TorrentFileInfo(JNIEnv *env, jstring contentFile){
		JniToStdString(env, &ContentFileName, contentFile);
	}
private:
	void SetContentFileName(){
		boost::intrusive_ptr<libtorrent::torrent_info> t;
		libtorrent::error_code ec;
		t = new libtorrent::torrent_info(TorrentFileName.c_str(), ec);
		if (ec){
			std::string errorMessage = ec.message();
			LOG_ERR("%s: %s\n", TorrentFileName.c_str(), errorMessage.c_str());
		}
		else{
			ContentFileName = t->name();
		}
	}
public:
	bool operator<(const TorrentFileInfo& tfi){return ContentFileName < tfi.ContentFileName;}
	bool operator<(const TorrentFileInfo& tfi) const {return ContentFileName < tfi.ContentFileName;}
};
//-----------------------------------------------------------------------------
static std::map<TorrentFileInfo, libtorrent::torrent_handle> gTorrents;
static libtorrent::session  	  gSession;
static libtorrent::proxy_settings gProxy;
static volatile bool			  gSessionState = false;
//-----------------------------------------------------------------------------
libtorrent::torrent_handle* GetTorrentHandle(JNIEnv *env, jstring ContentFile){
	libtorrent::torrent_handle* result = NULL;
	std::map<TorrentFileInfo, libtorrent::torrent_handle>::iterator iter = gTorrents.find(TorrentFileInfo(env,ContentFile));
    if(iter != gTorrents.end()) {
    	result = &iter->second;
    }
    else{
		LOG_ERR("Failed to get torrent handle");
    }
    return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_SetSession
	(JNIEnv *env, jobject obj, jint ListenPort, jint UploadLimit, jint DownloadLimit)
{
	jboolean result = JNI_FALSE;
	try{
		gSession.set_alert_mask(libtorrent::alert::all_categories
			& ~(libtorrent::alert::dht_notification
			+ libtorrent::alert::progress_notification
			+ libtorrent::alert::debug_notification
			+ libtorrent::alert::stats_notification));

		int listenPort = 54321;
		if(ListenPort > 0)
			listenPort = ListenPort;
			gSession.listen_on(std::make_pair(listenPort, listenPort+10));

		int uploadLimit = UploadLimit;
		if(uploadLimit > 0){
			gSession.set_upload_rate_limit(uploadLimit * 1000);
		}
		else{
			gSession.set_upload_rate_limit(0);
		}
		int downloadLimit = DownloadLimit;
		if(downloadLimit > 0){
			gSession.set_download_rate_limit(downloadLimit * 1000);
		}
		else{
			gSession.set_download_rate_limit(0);
		}

		libtorrent::session_settings sets = gSession.settings();

		sets.announce_to_all_trackers = true;
		sets.announce_to_all_tiers = true;
		sets.prefer_udp_trackers = false;
		sets.max_peerlist_size = 0;

		gSession.set_settings(sets);

		LOG_INFO("ListenPort: %d\n", listenPort);
		LOG_INFO("DownloadLimit: %d\n", downloadLimit);
		LOG_INFO("UploadLimit: %d\n", uploadLimit);

		gSessionState=true;
	}catch(...){
		LOG_ERR("Exception: failed to set session");
		gSessionState=false;
	}
	if(!gSessionState) LOG_ERR("LibTorrent.SetSession SessionState==false");
	gSessionState==true ? result=JNI_TRUE : result=JNI_FALSE;
	return result;
}
//-----------------------------------------------------------------------------
void JniToStdString(JNIEnv *env, std::string* StdString, jstring JniString){
	if(JniString){
		StdString->clear();
		jboolean isCopy = false;
		const char* ch = env->GetStringUTFChars(JniString, &isCopy);
		int chLen =  env->GetStringUTFLength(JniString);
		for(int i=0; i<chLen; i++) StdString->push_back(ch[i]);
		env->ReleaseStringUTFChars(JniString,ch);
	}
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_SetProxy
	(JNIEnv *env, jobject obj, jint Type, jstring HostName, jint Port, jstring UserName, jstring Password)
{
	jboolean result = JNI_FALSE;
	try{
		if(gSessionState){
			int type = Type;
//			if(type > 0){
				std::string hostName;
				JniToStdString(env, &hostName, HostName);
				int port = Port;
				std::string userName;
				JniToStdString(env, &userName, UserName);
				std::string password;
				JniToStdString(env, &password, Password);

				gProxy.type = libtorrent::proxy_settings::proxy_type(type);
				gProxy.hostname = hostName;
				gProxy.port = port;
				gProxy.username = userName;
				gProxy.password = password;

				gSession.set_proxy(gProxy);

				LOG_INFO("ProxyType: %d\n", type);
//				LOG_INFO("HostName: %s\n", hostName.c_str());
//				LOG_INFO("ProxyPort: %d\n", port);
//				LOG_INFO("UserName: %s\n", userName.c_str());
//				LOG_INFO("Password: %s\n", password.c_str());
//			}
		}
	}catch(...){
		LOG_ERR("Exception: failed to set proxy");
		gSessionState=false;
	}
	if(!gSessionState) LOG_ERR("LibTorrent.SetProxy SessionState==false");
	gSessionState==true ? result=JNI_TRUE : result=JNI_FALSE;
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_SetSessionOptions
	(JNIEnv *env, jobject obj, jboolean LSD, jboolean UPNP, jboolean NATPMP)
{
	jboolean result = JNI_FALSE;
	try{
		if(gSessionState){
			if(LSD == JNI_TRUE)
				gSession.start_lsd();
			else
				gSession.stop_lsd();
			if(UPNP == JNI_TRUE)
				gSession.start_upnp();
			else
				gSession.stop_upnp();
			if(NATPMP == JNI_TRUE)
				gSession.start_natpmp();
			else
				gSession.stop_natpmp();

			LOG_INFO("LSD: %d\n", LSD);
			LOG_INFO("UPNP: %d\n", UPNP);
			LOG_INFO("NATPMP: %d\n", NATPMP);
		}
	}catch(...){
		LOG_ERR("Exception: failed to set session options");
		gSessionState=false;
	}
	if(!gSessionState) LOG_ERR("LibTorrent.SetSessionOptions SessionState==false");
	gSessionState==true ? result=JNI_TRUE : result=JNI_FALSE;
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_AddTorrent
	(JNIEnv *env, jobject obj, jstring SavePath, jstring TorrentFile, jint StorageMode)
{
	jboolean result = JNI_FALSE;
	try{
		if(gSessionState){
			TorrentFileInfo torrentFileInfo(env, SavePath, TorrentFile);
			std::map<TorrentFileInfo, libtorrent::torrent_handle>::iterator iter = gTorrents.find(torrentFileInfo);
			if(iter != gTorrents.end()) {
				LOG_INFO("Torrent file already presents: %s", torrentFileInfo.TorrentFileName.c_str());
			}
			else{
				LOG_INFO("SavePath: %s", torrentFileInfo.SavePath.c_str());
				LOG_INFO("TorrentFile: %s", torrentFileInfo.TorrentFileName.c_str());

				boost::intrusive_ptr<libtorrent::torrent_info> t;
				libtorrent::error_code ec;
				t = new libtorrent::torrent_info(torrentFileInfo.TorrentFileName.c_str(), ec);
				if (ec){
					std::string errorMessage = ec.message();
					LOG_ERR("%s: %s\n", torrentFileInfo.TorrentFileName.c_str(), errorMessage.c_str());
				}
				else{
					LOG_INFO("%s\n", t->name().c_str());
					LOG_INFO("StorageMode: %d\n", StorageMode);

					libtorrent::add_torrent_params torrentParams;
					libtorrent::lazy_entry resume_data;

					boost::filesystem::path save_path = torrentFileInfo.SavePath;
					std::string filename = torrentFileInfo.SavePath + "/" +  t->name() +  ".resume";
					std::vector<char> buf;
					boost::system::error_code errorCode;
					if (libtorrent::load_file(filename.c_str(), buf, errorCode) == 0)
						torrentParams.resume_data = &buf;

					torrentParams.ti = t;
					torrentParams.save_path = torrentFileInfo.SavePath;
					torrentParams.duplicate_is_error = false;
					torrentParams.auto_managed = true;
					libtorrent::storage_mode_t storageMode = libtorrent::storage_mode_sparse;
					switch(StorageMode){
					case 0: storageMode = libtorrent::storage_mode_allocate; break;
					case 1: storageMode = libtorrent::storage_mode_sparse; break;
					case 2: storageMode = libtorrent::storage_mode_compact; break;
					}
					torrentParams.storage_mode = storageMode;
					libtorrent::torrent_handle th = gSession.add_torrent(torrentParams,ec);
					th.move_storage(save_path.string());
					if(ec) {
						std::string errorMessage = ec.message();
						LOG_ERR("failed to add torrent: %s\n", errorMessage.c_str());
					}
					else{
						if(th.is_paused()){
							th.resume();
						}
						if(!th.is_auto_managed()){
							th.auto_managed(true);
						}
						gTorrents[torrentFileInfo] = th;
						result=JNI_TRUE;
					}
				}
			}
		}
	}catch(...){
		LOG_ERR("Exception: failed to add torrent");
		try	{
			TorrentFileInfo torrentFileInfo(env, SavePath, TorrentFile);
			gTorrents.erase(torrentFileInfo);
		}catch(...){}
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_PauseSession
	(JNIEnv *, jobject)
{
	jboolean result = JNI_FALSE;
	try {
		if(gSessionState){
			gSession.pause();
			bool paused = gSession.is_paused();
			if(paused) result = JNI_TRUE;
		}
	} catch(...){
		LOG_ERR("Exception: failed to pause session");
		gSessionState=false;
	}
	if(!gSessionState) LOG_ERR("LibTorrent.PauseSession SessionState==false");
	gSessionState==true ? result=JNI_TRUE : result=JNI_FALSE;
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_ResumeSession
	(JNIEnv *, jobject)
{
	jboolean result = JNI_FALSE;
	try {
		if(gSessionState){
			gSession.resume();
			bool paused = gSession.is_paused();
			if(!paused) result = JNI_TRUE;
		}
	} catch(...){
		LOG_ERR("Exception: failed to resume session");
		gSessionState=false;
	}
	if(!gSessionState) LOG_ERR("LibTorrent.ResumeSession SessionState==false");
	gSessionState==true ? result=JNI_TRUE : result=JNI_FALSE;
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_AbortSession
	(JNIEnv *, jobject)
{
	jboolean result = JNI_FALSE;
	try {
		if(gSessionState)
			gSession.abort();
	} catch(...){
		LOG_ERR("Exception: failed to abort session");
		gSessionState=false;
	}
	if(!gSessionState) LOG_ERR("LibTorrent.AbortSession SessionState==false");
	gSessionState==true ? result=JNI_TRUE : result=JNI_FALSE;
	return result;
}
//-----------------------------------------------------------------------------
int SaveFile(const std::string& filename, std::vector<char>& v)
{
	std::string saveFileLog = "SaveFile: " + filename;
	//LOG_INFO(saveFileLog.c_str());
	libtorrent::file f;
	libtorrent::error_code ec;
	if (!f.open(filename, libtorrent::file::write_only, ec))
		return -1;
	if (ec) return -1;
	libtorrent::file::iovec_t b = {&v[0], v.size()};
	libtorrent::size_type written = f.writev(0, &b, 1, ec);
	if (written != v.size()) return -3;
	if (ec) return -3;
	return 0;
}
//-----------------------------------------------------------------------------
/*void HandleAlert(libtorrent::alert* a){
	libtorrent::torrent_finished_alert* finishedAlert = libtorrent::alert_cast<libtorrent::torrent_finished_alert>(a);
	if(finishedAlert){
		// write resume data for the finished torrent
		// the alert handler for save_resume_data_alert
		// will save it to disk
		finishedAlert->handle.set_max_connections(10);
		libtorrent::torrent_handle h = finishedAlert->handle;
		h.save_resume_data();
		LOG_INFO("torrent_finished_alert: save_resume_data");
	}
	else {
		libtorrent::save_resume_data_alert* resumeDataAlert = libtorrent::alert_cast<libtorrent::save_resume_data_alert>(a);
		if(resumeDataAlert) {
			libtorrent::torrent_handle h = resumeDataAlert->handle;
			TORRENT_ASSERT(p->resume_data);
			if(resumeDataAlert->resume_data){
				std::vector<char> out;
				libtorrent::bencode(std::back_inserter(out), *resumeDataAlert->resume_data);
				std::string savePath = h.save_path();//.string();
				std::string fileName = h.name();
				SaveFile((savePath + "/" + fileName + ".resume"), out);
				gSession.remove_torrent(h);
				LOG_INFO("save_resume_data_alert: remove_torrent");
			}
		}
		else {
			libtorrent::save_resume_data_failed_alert* dataFailedAlert = libtorrent::alert_cast<libtorrent::save_resume_data_failed_alert>(a);
			if(dataFailedAlert){
				libtorrent::torrent_handle h = dataFailedAlert->handle;
				gSession.remove_torrent(h);
				LOG_INFO("save_resume_data_failed_alert: remove_torrent");
			}
		}
	}
}*/
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_RemoveTorrent
	(JNIEnv *env, jobject obj, jstring ContentFile)
{
	jboolean result = JNI_FALSE;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				LOG_INFO("Remove torrent name %s", pTorrent->name().c_str());
				pTorrent->auto_managed(false);
				pTorrent->pause();
				// the alert handler for save_resume_data_alert
				// will save it to disk
				/*pTorrent->save_resume_data();
				// loop through the alert queue to see if anything has happened.
				std::auto_ptr<libtorrent::alert> a;
				a = gSession.pop_alert();
				while (a.get()){
					//LOG_INFO("RemoveTorrent Alert: %s", a->message().c_str());
					HandleAlert(a.get());
					a = gSession.pop_alert();
				}*/
				gSession.remove_torrent(*pTorrent);
				LOG_INFO("remove_torrent");
				gTorrents.erase(TorrentFileInfo(env,ContentFile));
				result = JNI_TRUE;
			}
		}
	} catch(...){
		LOG_ERR("Exception: failed to remove torrent");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_PauseTorrent
	(JNIEnv *env, jobject obj, jstring ContentFile)
{
	jboolean result = JNI_FALSE;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				LOG_INFO("Pause torrent name %s", pTorrent->name().c_str());
				pTorrent->auto_managed(false);
				pTorrent->pause();
				bool paused = pTorrent->is_paused();
				if(paused) result = JNI_TRUE;
			}
		}
	} catch(...){
		LOG_ERR("Exception: failed to pause torrent");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_ResumeTorrent
	(JNIEnv *env, jobject obj, jstring ContentFile)
{
	jboolean result = JNI_FALSE;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				LOG_INFO("Resume torrent name %s", pTorrent->name().c_str());
				pTorrent->resume();
				pTorrent->auto_managed(true);
				bool paused = pTorrent->is_paused();
				if(!paused) result = JNI_TRUE;
			}
		}
	} catch(...){
		LOG_ERR("Exception: failed to resume torrent");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jint JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetTorrentProgress
	(JNIEnv *env, jobject obj, jstring ContentFile)
{
	jint result = -1;
	try {
		if(gSessionState) {
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				libtorrent::torrent_status s = pTorrent->status();
				if(s.state != libtorrent::torrent_status::seeding && pTorrent->has_metadata()) {
					std::vector<libtorrent::size_type> file_progress;
					pTorrent->file_progress(file_progress);
					libtorrent::torrent_info const& info = pTorrent->get_torrent_info();
					int files_num = info.num_files();
					for (int i = 0; i < info.num_files(); ++i){
						int progress = info.file_at(i).size > 0 ? file_progress[i] * 1000 / info.file_at(i).size : 1000;
						result += progress;
					}
					result = result/files_num;
				}
				else if(s.state == libtorrent::torrent_status::seeding && pTorrent->has_metadata())
						result = 1000;
			}
		}
	}catch(...){
		LOG_ERR("Exception: failed to progress torrent");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jlong JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetTorrentProgressSize
	(JNIEnv *env, jobject obj, jstring ContentFile)
{
	jlong result = -1;
	try {
		if(gSessionState) {
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				libtorrent::torrent_status s = pTorrent->status();
				if(s.state != libtorrent::torrent_status::seeding && pTorrent->has_metadata()) {
					std::vector<libtorrent::size_type> file_progress;
					pTorrent->file_progress(file_progress);
					libtorrent::torrent_info const& info = pTorrent->get_torrent_info();
					int files_num = info.num_files();
					long long bytes_size = 0;
					for (int i = 0; i < info.num_files(); ++i){
						if(info.file_at(i).size > 0){
							bytes_size += file_progress[i];
						}
					}
					long long megabytes_size = 0;
					if(bytes_size > 0)
						megabytes_size = bytes_size / 1048576;
					result = megabytes_size;
				}
				else if(s.state == libtorrent::torrent_status::seeding && pTorrent->has_metadata()){
					libtorrent::torrent_info const& info = pTorrent->get_torrent_info();
					long long bytes_size = info.total_size();
					long long megabytes_size = 0;
					if(bytes_size > 0)
						megabytes_size = bytes_size / 1048576;
					result = megabytes_size;
				}
			}
		}
	}catch(...){
		LOG_ERR("Exception: failed to progress torrent size");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jint JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetTorrentState
	(JNIEnv *env, jobject, jstring ContentFile)
{
	jint result = -1;
	try {
		if(gSessionState) {
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				libtorrent::torrent_status t_s = pTorrent->status();
				bool paused = pTorrent->is_paused();
				bool auto_managed = pTorrent->is_auto_managed();
				if(paused) {
					result = 8; //paused
				} else {
					result = t_s.state;
				}
			}
		}
	}catch(...){
		LOG_ERR("Exception: failed to get torrent state");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	return result;
}
//-----------------------------------------------------------------------------
std::string add_suffix(float val, char const* suffix = 0)
{
	std::string ret;
	if (val == 0)
	{
		ret.resize(4 + 2, ' ');
		if (suffix) ret.resize(4 + 2 + strlen(suffix), ' ');
		return ret;
	}

	const char* prefix[] = {"kB", "MB", "GB", "TB"};
	const int num_prefix = sizeof(prefix) / sizeof(const char*);
	char temp[30];
	for (int i = 0; i < num_prefix; ++i)
	{
		val /= 1000.f;
		if (std::fabs(val) < 1000.f)
		{
			memset(temp, 0, 30);
		    sprintf(temp, "%.4g", val);
			ret = temp;
			ret += prefix[i];
			if (suffix) ret += suffix;
			return ret;
		}
	}
	memset(temp, 0, 30);
    sprintf(temp, "%.4g", val);
	ret =  temp;
	ret += "PB";
	if (suffix) ret += suffix;
	return ret;
}
//-----------------------------------------------------------------------------
static char const* state_str[] =
{"checking (q)", "checking", "dl metadata", "downloading", "finished", "seeding", "allocating", "checking (r)"};
//-----------------------------------------------------------------------------
JNIEXPORT jstring JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetTorrentStatusText
	(JNIEnv *env, jobject obj, jstring ContentFile)
{
	jstring result = NULL;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				std::string out;
				char str[500]; memset(str,0,500);

				libtorrent::torrent_status t_s = pTorrent->status();

				//------- ERROR --------
				if (!t_s.error.empty())
				{
					out += "error ";
					out += t_s.error;
					out += "\n";
					return env->NewStringUTF(out.c_str());
				}

				if (t_s.state != libtorrent::torrent_status::queued_for_checking && t_s.state != libtorrent::torrent_status::checking_files){
					snprintf(str, sizeof(str),
						  "%22s%20d/%d\n"
						  "%26s%20s\n"
						, "peers/cand:"
						, t_s.num_peers, t_s.connect_candidates
						, "speed:"
						, add_suffix(t_s.download_payload_rate, "/s").c_str());
					out += str;
				}
				result = env->NewStringUTF(out.c_str());
			}
		}
	} catch(...){
		LOG_ERR("Exception: failed to get torrent status text");
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jstring JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetSessionStatusText
	(JNIEnv *env, jobject obj)
{
	jstring result = NULL;
	try {
		if(gSessionState){
			std::string out;
			char str[500]; memset(str,0,500);
			libtorrent::session_status s_s = gSession.status();
			snprintf(str, sizeof(str),
					  "%25s%20d\n"
					  "%22s%20s/%s\n"
					  "%25s%20s/%s\n"
					  "%18s%20s/%s\n"
					  "%15s%20s/%s\n"
					  "%19s%20s/%s\n"
				,"conns:"
				, s_s.num_peers
				, "down/rate:"
				, add_suffix(s_s.total_download).c_str(), add_suffix(s_s.download_rate, "/s").c_str()
				, "up/rate:"
				, add_suffix(s_s.total_upload).c_str(), add_suffix(s_s.upload_rate, "/s").c_str()
				, "ip rate down/up:"
				, add_suffix(s_s.ip_overhead_download_rate, "/s").c_str(), add_suffix(s_s.ip_overhead_upload_rate, "/s").c_str()
				, "dht rate down/up:"
				, add_suffix(s_s.dht_download_rate, "/s").c_str(), add_suffix(s_s.dht_upload_rate, "/s").c_str()
				, "tr rate down/up:"
				, add_suffix(s_s.tracker_download_rate, "/s").c_str(), add_suffix(s_s.tracker_upload_rate, "/s").c_str());
			out += str;
			result = env->NewStringUTF(out.c_str());
		}
	} catch(...){
		LOG_ERR("Exception: failed to get session status");
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jstring JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetTorrentFiles
	(JNIEnv *env, jobject obj, jstring ContentFile)
{
	jstring result = NULL;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				std::string out;
				libtorrent::torrent_status s = pTorrent->status();
				if(pTorrent->has_metadata()) {
					libtorrent::torrent_info const& info = pTorrent->get_torrent_info();
					int files_num = info.num_files();
					for (int i = 0; i < info.num_files(); ++i) {
						char out_size[30];
						memset(out_size, 0, 30);
						sprintf(out_size, "%ld", info.file_at(i).size);
						out += info.file_at(i).path;
						out += "-->";
						out += out_size;
						out += "\n";
					}
				}
				result = env->NewStringUTF(out.c_str());
			}
		}
	} catch(...){
		LOG_ERR("Exception: failed to get torrent files");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_SetTorrentFilesPriority
	(JNIEnv *env, jobject obj, jbyteArray FilesPriority, jstring ContentFile)
{
	jboolean result = JNI_FALSE;
	jbyte* filesPriority  = NULL;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				std::string out;
				libtorrent::torrent_status s = pTorrent->status();
				if(pTorrent->has_metadata()) {
					libtorrent::torrent_info const& info = pTorrent->get_torrent_info();
					int files_num = info.num_files();
					jsize arr_size = env->GetArrayLength(FilesPriority);
					if(files_num == arr_size){
						filesPriority = env->GetByteArrayElements(FilesPriority, 0);
						const unsigned char* prioritiesBytes = (const unsigned char*)filesPriority;
						std::vector<int> priorities;
						for (int i = 0; i < info.num_files(); ++i) {
							priorities.push_back(int(filesPriority[i]));
						}
						pTorrent->prioritize_files(priorities);
						result = JNI_TRUE;
					} else {
						LOG_ERR("LibTorrent.SetTorrentFilesPriority priority array size failed");
					}
				}
			}
		}
	} catch(...){
		LOG_ERR("Exception: failed to set files priority");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	if(filesPriority)
		env->ReleaseByteArrayElements(FilesPriority, filesPriority, JNI_ABORT);
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jbyteArray JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetTorrentFilesPriority
	(JNIEnv *env, jobject obj, jstring ContentFile)
{
	jbyteArray result = NULL;
	jbyte* result_array = NULL;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				libtorrent::torrent_status s = pTorrent->status();
				if(pTorrent->has_metadata()) {
					libtorrent::torrent_info const& info = pTorrent->get_torrent_info();
					int files_num = info.num_files();
					std::vector<int> priorities = pTorrent->file_priorities();
					if(files_num == priorities.size() ){
						result_array = new jbyte[files_num];
						for(int i=0;i<files_num;i++) result_array[i] = (jbyte)priorities[i];
						result = env->NewByteArray(files_num);
						env->SetByteArrayRegion(result,0,files_num,result_array);
					} else {
						LOG_ERR("LibTorrent.GetTorrentFilesPriority priority array size failed");
					}
				}
			}
		}
	} catch(...){
		LOG_ERR("Exception: failed to get files priority");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	if(result_array)
		delete [] result_array;
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jstring JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetTorrentName
	(JNIEnv *env, jobject obj, jstring TorrentFile)
{
	jstring result = NULL;
	try{
		std::string torrentFile;
		JniToStdString(env, &torrentFile, TorrentFile);

		boost::intrusive_ptr<libtorrent::torrent_info> t;
		libtorrent::error_code ec;
		t = new libtorrent::torrent_info(torrentFile.c_str(), ec);
		if (ec){
			std::string errorMessage = ec.message();
			LOG_ERR("%s: %s\n", torrentFile.c_str(), errorMessage.c_str());
		}
		else{
			//LOG_INFO("%s\n", t->name().c_str());
			result = env->NewStringUTF((t->name()).c_str());
		}
	}catch(...){
		LOG_ERR("Exception: failed to get torrent name");
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jlong JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetTorrentSize
	(JNIEnv *env, jobject obj, jstring TorrentFile)
{
	jlong result = -1;
	try{
		std::string torrentFile;
		JniToStdString(env, &torrentFile, TorrentFile);

		boost::intrusive_ptr<libtorrent::torrent_info> info;
		libtorrent::error_code ec;
		info = new libtorrent::torrent_info(torrentFile.c_str(), ec);
		if (ec){
			std::string errorMessage = ec.message();
			LOG_ERR("%s: %s\n", torrentFile.c_str(), errorMessage.c_str());
		}
		else{
			long long bytes_size = 0;
			long long megabytes_size = 0;
			bytes_size = info->total_size();
			if(bytes_size > 0)
				megabytes_size = bytes_size / 1048576;
			result = megabytes_size;
		}
	}catch(...){
		LOG_ERR("Exception: failed to get torrent size");
	}
	return result;
}
//-----------------------------------------------------------------------------
// Additional logic
//-----------------------------------------------------------------------------
JNIEXPORT jintArray JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetPiecePriorities
	(JNIEnv *env, jobject obj, jstring ContentFile)
{
	jintArray result = NULL;
	jint* result_array = NULL;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				if(pTorrent->has_metadata()) {
					libtorrent::torrent_info const& info = pTorrent->get_torrent_info();
						int pices_num = info.num_pieces();
						std::vector<int> priorities = pTorrent->piece_priorities();
						if(pices_num == priorities.size() ){
							result_array = new jint[pices_num];
							for(int i=0; i<pices_num; i++) {
								result_array[i] = priorities[i];
							}
							result = env->NewIntArray(pices_num);
							env->SetIntArrayRegion(result, 0, pices_num, result_array);
						} else {
							LOG_ERR("LibTorrent.GetPiecePriorities priority array size failed");
						}
					}
				}
		}
	} catch(...){
		LOG_ERR("Exception: failed to get pieces priority");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	if(result_array) {
		delete [] result_array;
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_SetPiecePriorities
	(JNIEnv *env, jobject obj, jstring ContentFile, jintArray Priorities)
{
	jboolean result = JNI_FALSE;
	jint* piecesPriority  = NULL;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				if(pTorrent->has_metadata()) {
					libtorrent::torrent_info const& info = pTorrent->get_torrent_info();
					int pieces_num = info.num_pieces();
					jsize arr_size = env->GetArrayLength(Priorities);
					if(pieces_num == arr_size){
						piecesPriority = env->GetIntArrayElements(Priorities, 0);
						std::vector<int> priorities;
						for (int i = 0; i < pieces_num; ++i) {
							priorities.push_back(piecesPriority[i]);
						}
						pTorrent->prioritize_pieces(priorities);
						result = JNI_TRUE;
					} else {
						LOG_ERR("LibTorrent.SetPiecePriorities priority array size failed");
					}
				}
			}
		}
	} catch(...){
		LOG_ERR("Exception: failed to set pieces priority");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	if(piecesPriority) {
		env->ReleaseIntArrayElements(Priorities, piecesPriority, JNI_ABORT);
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_HavePiece
	(JNIEnv *env, jobject obj, jstring ContentFile, jint index)
{
	jboolean result = JNI_FALSE;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				if (pTorrent->have_piece(index)) {
					result = JNI_TRUE;
				}
			}
		}
	} catch(...){
		LOG_ERR("Exception: failed to check if piece is have");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	return result;
}
//-----------------------------------------------------------------------------
JNIEXPORT jlong JNICALL Java_com_softwarrior_libtorrent_LibTorrent_GetPieceSize
	(JNIEnv *env, jobject obj, jstring ContentFile, jint PieceIndex)
{
	jlong pieceSize = -1;
	try {
		if(gSessionState){
			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
			if(pTorrent){
				libtorrent::torrent_info const& info = pTorrent->get_torrent_info();
				int pices_num = info.num_pieces();
				if (PieceIndex < pices_num) {
					pieceSize = info.piece_size(PieceIndex);
				} else {
					LOG_ERR("LibTorrent.GetPieceSize not correct piece index");
				}
			}
		}
	} catch(...){
		LOG_ERR("Exception: failed to get piece size");
		try	{
			gTorrents.erase(TorrentFileInfo(env,ContentFile));
		}catch(...){}
	}
	return pieceSize;
}
//-----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_softwarrior_libtorrent_LibTorrent_SetSavePath
	(JNIEnv *env, jobject obj, jstring SavePath)
{
	jboolean result = JNI_FALSE;
//	try {
//		if(gSessionState){
//			libtorrent::torrent_handle* pTorrent = GetTorrentHandle(env,ContentFile);
//			if(pTorrent){
//				pTorrent->move_storage();
//			}
//		}
//	} catch(...){
//		LOG_ERR("Exception: failed to set save path");
//		try	{
//			gTorrents.erase(TorrentFileInfo(env,ContentFile));
//		}catch(...){}
//	}
	return result;
}
//-----------------------------------------------------------------------------

