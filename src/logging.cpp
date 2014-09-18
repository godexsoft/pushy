//
//  logging.cpp
//  pushy
//
//  Created by Alexander Kremer on 19/08/2014.
//  Copyright (c) 2014 godexsoft. All rights reserved.
//

#include "logging.hpp"
#include "database.hpp"

#include <cstddef>
#include <string>
#include <ostream>
#include <fstream>
#include <iomanip>

#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/preprocessor/seq/enum.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <json_spirit/json_spirit_writer_template.h>

namespace pushy
{
    namespace logger = boost::log;
    namespace src = boost::log::sources;
    namespace sinks = boost::log::sinks;
    namespace keywords = boost::log::keywords;
    namespace expr = boost::log::expressions;

    using namespace boost::posix_time;
    using namespace pushy::database;
    
    src::severity_logger< pushy::severity_level > logging::std_logger;
 
    BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", pushy::severity_level)
    BOOST_LOG_ATTRIBUTE_KEYWORD(tag_attr, "Tag", std::string)
    
    std::ostream& operator<< (std::ostream& strm, pushy::severity_level level)
    {
        static const char* strings[] =
        {
            "TRACE",
            "DEBUG",
            "INFO",
            "WARNING",
            "ERROR",
            
            // and special levels for reporting
            "APNS",
            "GCM"
        };
        
        if (static_cast< std::size_t >(level) < sizeof(strings) / sizeof(*strings))
        {
            strm << strings[level];
        }
        else
        {
            strm << static_cast< int >(level);
        }
        
        return strm;
    }
    
    std::ostream& operator<<(std::ostream& os, const severity_t& sev)
    {
        os << sev.to_string();
        return os;
    }
    
    std::istream& operator>>(std::istream& in, severity_t& sev)
    {
        std::string name;
        in >> name;
    
        sev.level = severity_t::to_severity(name);
        return in;
    }
    
    const std::string severity_t::to_string() const
    {
        switch(level)
        {
            case trace:
                return "trace";
                
            case debug:
                return "debug";

            case info:
                return "info";

            case warning:
                return "warning";

            case error:
                return "error";

            default:
                throw std::runtime_error("can't convert severity to string");
        }
    }
    
    severity_level severity_t::to_severity(const std::string& name)
    {
        using namespace boost::program_options;

        if(name == "trace")
        {
            return trace;
        }
        
        if(name == "debug")
        {
            return debug;
        }
        
        if(name == "info")
        {
            return info;
        }

        if(name == "warning")
        {
            return warning;
        }

        if(name == "error")
        {
            return error;
        }
        
        throw validation_error(validation_error::invalid_option_value);
    }

    void logging::init_basics()
    {
        // this is just so that the format of errors emited before we setup the logger sinks match the later
        // Setup the common formatter for all sinks
        logger::formatter fmt = expr::stream
            << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
            << " [" << expr::attr< boost::log::attributes::current_thread_id::value_type >("ThreadID")
            << "] <" << severity << "> "
            << expr::if_(expr::has_attr(tag_attr))
            [
                expr::stream << "[" << tag_attr << "] "
            ]
            << expr::smessage;
        
        // console sink for now
        typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
        boost::shared_ptr< text_sink > sink = boost::make_shared< text_sink >();
        
        boost::shared_ptr< std::ostream > stream(&std::clog, logger::empty_deleter());
        sink->locked_backend()->add_stream(stream);
        
        sink->set_formatter(fmt);
        sink->set_filter( severity < pushy::apns && severity >= warning );
        
        logger::core::get()->add_sink(sink);
        logger::add_common_attributes();
    }
    
    void logging::init(const std::string& logfile, severity_level& loglevel, const std::string& apns_logfile, const std::string& gcm_logfile)
    {
        logger::core::get()->remove_all_sinks();
        
        // Setup the common formatter for all sinks
        logger::formatter fmt = expr::stream
            << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
            << " [" << expr::attr< boost::log::attributes::current_thread_id::value_type >("ThreadID")
            << "] <" << severity << "> "
            << expr::if_(expr::has_attr(tag_attr))
            [
                expr::stream << "[" << tag_attr << "] "
            ]
            << expr::smessage;

        // Setup formatter for logstash (plain text output)
        logger::formatter plain_fmt = expr::stream << expr::smessage;
        
        // Dump into files?
        if (!logfile.empty())
        {
            typedef sinks::synchronous_sink< sinks::text_file_backend > file_sink;
            boost::shared_ptr< file_sink > sink(new file_sink(
                keywords::file_name = logfile,
                keywords::rotation_size = 10 * 1024 * 1024,
                keywords::auto_flush = true
            ));
            
            // Setup where the rotated files will be stored
            sink->locked_backend()->set_file_collector(sinks::file::make_collector(
                keywords::target = "log"
            ));
            
            // Upon restart, scan the target directory for files matching the file_name pattern
            sink->locked_backend()->scan_for_files();
            sink->set_formatter(fmt);
            sink->set_filter( severity < pushy::apns && severity >= loglevel );
            
            logger::core::get()->add_sink(sink);
        }
        else
        {
            // Console sink if no file
            typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
            boost::shared_ptr< text_sink > sink = boost::make_shared< text_sink >();
        
            boost::shared_ptr< std::ostream > stream(&std::clog, logger::empty_deleter());
            sink->locked_backend()->add_stream(stream);
            
            sink->set_formatter(fmt);
            sink->set_filter( severity < pushy::apns && severity >= loglevel );
            
            logger::core::get()->add_sink(sink);
        }
        
        // APNS sink only if logfile is provided
        if (!apns_logfile.empty())
        {
            typedef sinks::synchronous_sink< sinks::text_file_backend > file_sink;
            boost::shared_ptr< file_sink > sink(new file_sink(
                keywords::file_name = apns_logfile,
                keywords::rotation_size = 10 * 1024 * 1024,
                keywords::auto_flush = true
            ));
            
            sink->locked_backend()->set_file_collector(sinks::file::make_collector(
                keywords::target = "apns_log"
            ));
            
            sink->locked_backend()->scan_for_files();
            sink->set_formatter(plain_fmt);
            sink->set_filter( severity == pushy::apns );
            
            logger::core::get()->add_sink(sink);
        }
        
        // GCM sink only if logfile is provided
        if (!gcm_logfile.empty())
        {
            typedef sinks::synchronous_sink< sinks::text_file_backend > file_sink;
            boost::shared_ptr< file_sink > sink(new file_sink(
                keywords::file_name = gcm_logfile,
                keywords::rotation_size = 10 * 1024 * 1024,
                keywords::auto_flush = true
            ));
            
            sink->locked_backend()->set_file_collector(sinks::file::make_collector(
                keywords::target = "gcm_log"
            ));
            
            sink->locked_backend()->scan_for_files();
            sink->set_formatter(plain_fmt);
            sink->set_filter( severity == pushy::gcm );
            
            logger::core::get()->add_sink(sink);
        }
    }
    
    void logging::logstash_dev(const severity_level& loglevel,
                               const std::string& status,
                               boost::uuids::uuid& uuid,
                               const ptime& time,
                               const std::string& msg)
    {
        auto rec = std_logger.open_record(keywords::severity = (loglevel));
        if(rec)
        {
            logger::record_ostream strm(rec);
            json_spirit::Object obj, fields;
            
            obj.push_back( json_spirit::Pair("@timestamp", to_iso_extended_string(time) ) );
            obj.push_back( json_spirit::Pair("message", msg) );

            fields.push_back( json_spirit::Pair("status", status) );
            fields.push_back( json_spirit::Pair("device_uuid", to_string(uuid)) );

            obj.push_back( json_spirit::Pair("@fields", fields) );
            
            strm << json_spirit::write_string(json_spirit::Value(obj), false);
            strm.flush();
            
            std_logger.push_record(boost::move(rec));
        }
        else
        {
            LOG_ERROR << "failed to create log record for logstash.";
        }
    }
    
    void logging::logstash_msg(const severity_level& loglevel,
                               const std::string& status,
                               boost::uuids::uuid& uuid,
                               const std::string& msg)
    {
        auto rec = std_logger.open_record(keywords::severity = (loglevel));
        if(rec)
        {
            logger::record_ostream strm(rec);
            json_spirit::Object obj, fields;
            
            auto m = dba::instance().get_message(uuid);
            
            obj.push_back( json_spirit::Pair("@timestamp", to_iso_extended_string(m.ts) ) );
            obj.push_back( json_spirit::Pair("message", msg) );
            
            fields.push_back( json_spirit::Pair("status", status) );
            fields.push_back( json_spirit::Pair("device_uuid", to_string(m.dev_uuid)) );
            fields.push_back( json_spirit::Pair("message_uuid", to_string(m.msg_uuid)) );
            fields.push_back( json_spirit::Pair("attempt", static_cast<uint64_t>(m.attempts)) );
            fields.push_back( json_spirit::Pair("provider", dba::type_to_str(m.provider_type)) );
            
            if(!m.tag.empty())
            {
                fields.push_back( json_spirit::Pair("tag", m.tag) );
            }
            
            obj.push_back( json_spirit::Pair("@fields", fields) );
            
            strm << json_spirit::write_string(json_spirit::Value(obj), false);
            strm.flush();
            
            std_logger.push_record(boost::move(rec));
        }
        else
        {
            LOG_ERROR << "failed to create log record for logstash.";
        }
    }
    
    void logging::logstash(const severity_level& loglevel,
                           const std::string& status,
                           const ptime& time,
                           const std::string& msg,
                           const push_type& provider_type)
    {
        auto rec = std_logger.open_record(keywords::severity = (loglevel));
        if(rec)
        {
            logger::record_ostream strm(rec);
            json_spirit::Object obj, fields;
            
            obj.push_back( json_spirit::Pair("@timestamp", to_iso_extended_string(time) ) );
            obj.push_back( json_spirit::Pair("message", msg) );
            
            fields.push_back( json_spirit::Pair("status", status) );
            fields.push_back( json_spirit::Pair("provider", dba::type_to_str(provider_type)) );
            
            obj.push_back( json_spirit::Pair("@fields", fields) );
            
            strm << json_spirit::write_string(json_spirit::Value(obj), false);
            strm.flush();
            
            std_logger.push_record(boost::move(rec));
        }
        else
        {
            LOG_ERROR << "failed to create log record for logstash.";
        }
    }
}