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
#pragma once

#include <string>
#include <deque>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <cstdint>
#include <chrono>

#include "LineIAX2.h"

namespace kc1fsz {

class Log;

/**
 * Validates AllStarLink App Authentication (ASL002) tokens by calling the 
 * ASL API (https://api.allstarlink.org/v1/appauth/validate?t=TOKEN&u=CALLSIGN).
 * 
 * The HTTP calls are made on a background worker thread so that the 
 * LineIAX2 event loop is never blocked.
 *
 * IMPORTANT: The Log passed in must be thread-safe (ex: MTLog).
 */
class AppAuthValidatorCurl : public AppAuthValidator {
public:

    static constexpr const char* DEFAULT_URL = 
        "https://api.allstarlink.org/v1/appauth/validate";

    /**
     * @param validateUrl The base URL of the validation endpoint (no query 
     * string). If null/empty the default is used.
     */
    AppAuthValidatorCurl(Log& log, const char* validateUrl = 0);
    ~AppAuthValidatorCurl();

    AppAuthValidatorCurl(const AppAuthValidatorCurl&) = delete;
    AppAuthValidatorCurl& operator=(const AppAuthValidatorCurl&) = delete;

    // ----- AppAuthValidator ------------------------------------------------

    virtual bool requestValidation(unsigned requestId, const char* token, 
        const char* callsign);
    virtual Status getResult(unsigned requestId, fixedstring& validatedCallsign);
    virtual void cancel(unsigned requestId);

private:

    struct Request {
        unsigned id;
        std::string token;
        std::string callsign;
    };

    struct Result {
        Status status = Status::PENDING;
        std::string callsign;
        std::chrono::steady_clock::time_point created;
    };

    /**
     * Removes results that were never collected (ex: the call went away
     * while the validation was in progress). Lock must be held.
     */
    void _pruneStale();

    void _worker();
    /**
     * Does the (blocking) HTTP call.
     * @returns true if validated
     */
    bool _validate(const Request& req, std::string& validatedCallsign);

    // Limits the amount of state that can be held, in case a caller 
    // floods us with NEWs
    static const unsigned MAX_OUTSTANDING = 32;

    Log& _log;
    const std::string _url;

    std::mutex _lock;
    std::condition_variable _cv;
    std::deque<Request> _queue;
    // Keyed by request ID. An entry exists from request until the 
    // completed result is collected (or cancelled).
    std::map<unsigned, Result> _results;
    bool _stop = false;
    std::thread _thread;
};

}
