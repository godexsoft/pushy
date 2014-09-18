//
//  logging.h
//  pushy
//
//  Created by Alexander Kremer on 19/08/2014.
//  Copyright (c) 2014 godexsoft. All rights reserved.
//

#ifndef __pushy__logging__
#define __pushy__logging__

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/formatters.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/empty_deleter.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/uuid/uuid.hpp>
#include <string>

#include "database.hpp"

#define LOG_TRACE BOOST_LOG_SEV(logging::std_logger, pushy::trace)
#define LOG_DEBUG BOOST_LOG_SEV(logging::std_logger, pushy::debug)
#define LOG_INFO  BOOST_LOG_SEV(logging::std_logger, pushy::info)
#define LOG_WARN  BOOST_LOG_SEV(logging::std_logger, pushy::warning)
#define LOG_ERROR BOOST_LOG_SEV(logging::std_logger, pushy::error)

// special loggers for apns and gcm stats
#define LOG_APNS(status, uuid, msg) logging::logstash_msg(pushy::apns, status, uuid, msg)
#define LOG_APNS_DEVICE(status, uuid, time, msg) logging::logstash_dev(pushy::apns, status, uuid, time, msg)
#define LOG_APNS_GENERIC(status, time, msg) logging::logstash(pushy::apns, status, time, msg, pushy::database::push_type_apns)

#define LOG_GCM(status, uuid, msg) logging::logstash_msg(pushy::gcm, status, uuid, msg)


namespace pushy
{
    enum severity_level {
        trace,
        debug,
        info,
        warning,
        error,

        // and special levels for reporting
        apns,
        gcm
    };
 
    
    /**
     * Helper for parsing program option
     */
    struct severity_t
    {
        severity_t()
        : level(info)
        {
        }
        
        severity_t(const std::string& s)
        : level(to_severity(s))
        {
        }
        
        severity_t(const severity_t& sev) : level(sev.level)
        {
        }
        
        severity_t& operator=(const severity_t& sev)
        {
            level = sev.level;
            return *this;
        }

        bool operator==(const severity_t& sev) const
        {
            return level == sev.level;
        }
        
        bool operator!=(const severity_t& sev) const
        {
            return level != sev.level;
        }
        
        static severity_level to_severity(const std::string& name);
        const std::string to_string() const;
        
        severity_level level;
    };

    class logging
    {
    public:
        static void init_basics();
        static void init(const std::string& logfile, severity_level& loglevel,
                         const std::string& apns_logfile, const std::string& gcm_logfile);
        
        static void logstash(const severity_level& loglevel, const std::string& status,
                             const boost::posix_time::ptime& time, const std::string& msg,
                             const database::push_type& provider_type);
        
        static void logstash_dev(const severity_level& loglevel, const std::string& status,
                                 boost::uuids::uuid& uuid, const boost::posix_time::ptime& time,
                                 const std::string& msg);
        
        static void logstash_msg(const severity_level& loglevel, const std::string& status,
                                 boost::uuids::uuid& uuid, const std::string& msg);

        static boost::log::sources::severity_logger< pushy::severity_level > std_logger;
    };

    std::ostream& operator<<(std::ostream& os, const severity_t& sev);    
    std::istream& operator>>(std::istream& in, severity_t& sev);
}

#endif /* defined(__pushy__logging__) */
