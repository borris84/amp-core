/**
 * Copyright (C) 2026, Ampersand project contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <cstring>
#include <string>

#include <curl/curl.h>

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/raiiholder.h"

#include "IAX2Util.h"
#include "AppAuthValidatorCurl.h"

#ifdef _WIN32
// The xxd-generated resource which was downloaded from the Curl website
// at build time.
#include "cacerts.h"
#endif

using namespace std;

namespace kc1fsz {

static const char* AST_CURL_USER_AGENT = "asterisk-libcurl-agent/1.0";
// The response is either OHYES<callsign>, "" or "?" so this is plenty
static const size_t MAX_RESPONSE_SIZE = 256;

static size_t writeCallback(char* contents, size_t size, size_t nmemb, void* userp) {
    string* s = (string*)userp;
    size_t total = size * nmemb;
    if (s->size() + total > MAX_RESPONSE_SIZE)
        // Returning a short count makes curl fail the transfer
        return 0;
    s->append(contents, total);
    return total;
}

AppAuthValidatorCurl::AppAuthValidatorCurl(Log& log, const char* validateUrl) 
:   _log(log),
    _url((validateUrl == 0 || validateUrl[0] == 0) ? DEFAULT_URL : validateUrl) {
    _thread = std::thread(&AppAuthValidatorCurl::_worker, this);
}

AppAuthValidatorCurl::~AppAuthValidatorCurl() {
    {
        lock_guard<mutex> g(_lock);
        _stop = true;
    }
    _cv.notify_all();
    if (_thread.joinable())
        _thread.join();
}

bool AppAuthValidatorCurl::requestValidation(unsigned requestId, const char* token, 
    const char* callsign) {
    {
        lock_guard<mutex> g(_lock);
        _pruneStale();
        if (_results.size() >= MAX_OUTSTANDING) 
            return false;
        Result r;
        r.created = chrono::steady_clock::now();
        _results[requestId] = r;
        _queue.push_back({ requestId, token, callsign });
    }
    _cv.notify_one();
    return true;
}

AppAuthValidator::Status AppAuthValidatorCurl::getResult(unsigned requestId, 
    fixedstring& validatedCallsign) {
    lock_guard<mutex> g(_lock);
    auto it = _results.find(requestId);
    // Unknown is treated as a failure
    if (it == _results.end())
        return Status::INVALID;
    Status st = it->second.status;
    if (st != Status::PENDING) {
        validatedCallsign = it->second.callsign.c_str();
        _results.erase(it);
    }
    return st;
}

void AppAuthValidatorCurl::cancel(unsigned requestId) {
    lock_guard<mutex> g(_lock);
    _results.erase(requestId);
    // The worker will notice that the result slot is gone 
    for (auto it = _queue.begin(); it != _queue.end(); it++) {
        if (it->id == requestId) {
            _queue.erase(it);
            break;
        }
    }
}

void AppAuthValidatorCurl::_pruneStale() {
    // Much longer than any call would wait for a result
    const auto cutoff = chrono::steady_clock::now() - chrono::seconds(60);
    for (auto it = _results.begin(); it != _results.end(); ) {
        if (it->second.created < cutoff)
            it = _results.erase(it);
        else
            it++;
    }
}

void AppAuthValidatorCurl::_worker() {
    while (true) {
        Request req;
        {
            unique_lock<mutex> g(_lock);
            _cv.wait(g, [this] { return _stop || !_queue.empty(); });
            if (_stop)
                return;
            req = _queue.front();
            _queue.pop_front();
        }

        string validatedCallsign;
        bool valid = _validate(req, validatedCallsign);

        {
            lock_guard<mutex> g(_lock);
            auto it = _results.find(req.id);
            // Could have been cancelled while we were working
            if (it != _results.end()) {
                it->second.status = valid ? Status::VALID : Status::INVALID;
                it->second.callsign = validatedCallsign;
            }
        }
    }
}

bool AppAuthValidatorCurl::_validate(const Request& req, string& validatedCallsign) {

    CURL* curl = curl_easy_init();
    if (!curl) {
        _log.error("AppAuth unable to initialize curl");
        return false;
    }
    // Responsible for freeing resource on exit
    raiiholder<CURL> curlHolder(curl, [](CURL* c) { curl_easy_cleanup(c); });

    // NOTE: The token/callsign were already restricted to [A-Za-z0-9] 
    // by parseAppAuthCallingName(), but escape anyway.
    char* t = curl_easy_escape(curl, req.token.c_str(), 0);
    char* u = curl_easy_escape(curl, req.callsign.c_str(), 0);
    string url = _url + "?t=" + (t ? t : "") + "&u=" + (u ? u : "");
    curl_free(t);
    curl_free(u);

    string response;

    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, AST_CURL_USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
#ifdef _WIN32    
    struct curl_blob blob;
    blob.data = curl_cacerts;
    blob.len = strlen((const char*)curl_cacerts);
    blob.flags = CURL_BLOB_COPY;
    curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &blob);
#endif
    curl_easy_setopt(curl, CURLOPT_CA_CACHE_TIMEOUT, 604800L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        _log.error("AppAuth validation request failed: %s", curl_easy_strerror(res));
        return false;
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    char cs[65];
    // NOTE: ASL002 says that all successful validations come back with 
    // HTTP 200, but the body is what matters.
    if (httpCode == 200 && 
        interpretAppAuthResponse(response.c_str(), req.callsign.c_str(), cs, sizeof(cs))) {
        validatedCallsign = cs;
        return true;
    }

    _log.info("AppAuth validation rejected (HTTP %ld) for %s", httpCode, 
        req.callsign.empty() ? "(legacy client)" : req.callsign.c_str());
    return false;
}

}
