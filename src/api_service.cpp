//
//  api_service.cpp
//  pushy
//
//  Created by Alexander Kremer on 19/08/2014.
//  Copyright (c) 2014 godexsoft. All rights reserved.
//

#include "api_service.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

#include <json_spirit/json_spirit_writer_template.h>
#include <json_spirit/json_spirit_reader_template.h>

#include "pushy_service.hpp"
#include "logging.hpp"

namespace pushy
{
    using namespace boost::network::http;
    using namespace pushy::database;
    
    const std::string api_service::handler::error_json(const std::string& msg)
    {
        json_spirit::Object obj;
        
        obj.push_back( json_spirit::Pair("success", false) );
        obj.push_back( json_spirit::Pair("message", msg) );
        
        return json_spirit::write_string( json_spirit::Value(obj), false );
    }
    
    const std::string api_service::handler::reg_apns(const std::string& body)
    {
        auto uuid = dba::instance().register_apns_device(body);
        json_spirit::Object obj;
        
        obj.push_back( json_spirit::Pair("success", true) );
        obj.push_back( json_spirit::Pair("uuid", to_string(uuid)) );
        
        return json_spirit::write_string( json_spirit::Value(obj), false );
    }

    const std::string api_service::handler::reg_gcm(const std::string& body)
    {
        auto uuid = dba::instance().register_gcm_device(body);
        json_spirit::Object obj;
        
        obj.push_back( json_spirit::Pair("success", true) );
        obj.push_back( json_spirit::Pair("uuid", to_string(uuid)) );
        
        return json_spirit::write_string( json_spirit::Value(obj), false );
    }
    
    const std::string api_service::handler::send_push(const std::string& body)
    {
        json_spirit::Value input;
        if (!json_spirit::read_string(body, input))
        {
            LOG_WARN << "Couldn't parse JSON input: '" << body << "'";
            return error_json("Couldn't parse JSON input");
        }
        
        if(input.type() != json_spirit::obj_type)
        {
            LOG_WARN << "Passed JSON is not an Object: '" << body << "'";
            return error_json("Not an Object");
        }
        
        auto input_obj = input.get_obj();

        boost::uuids::string_generator str_gen;
        boost::uuids::uuid dev_uuid;
        std::string msg, tag;
        
        for(auto entry : input_obj)
        {
            LOG_TRACE << "parsing entry " << entry.name_;
            
            if(entry.name_ == "uuid")
            {
                dev_uuid = str_gen(entry.value_.get_str());
            }
            else if(entry.name_ == "msg")
            {
                msg = entry.value_.get_str();
            }
            else if(entry.name_ == "tag")
            {
                tag = entry.value_.get_str();
            }
        }
        
        LOG_TRACE << "parsed uuid = " << to_string(dev_uuid);
        LOG_TRACE << "parsed msg = '" << msg << "', tag = '" << tag << "'";
        
        // push to pushy_service
        auto msg_uuid = push_service_.push(dev_uuid, msg, tag);
        
        json_spirit::Object obj;
        
        obj.push_back( json_spirit::Pair("success", true) );
        obj.push_back( json_spirit::Pair("uuid", to_string(msg_uuid)) );
        
        return json_spirit::write_string( json_spirit::Value(obj), false );
    }
    
    const std::string api_service::handler::redeliver(const std::string& body)
    {
        json_spirit::Value input;
        if (!json_spirit::read_string(body, input))
        {
            LOG_WARN << "Couldn't parse JSON input: '" << body << "'";
            return error_json("Couldn't parse JSON input");
        }
        
        if(input.type() != json_spirit::array_type)
        {
            LOG_WARN << "Passed JSON is not an Array: '" << body << "'";
            return error_json("Not an Array");
        }
        
        auto input_arr = input.get_array();
        
        boost::uuids::string_generator str_gen;
        std::vector<boost::uuids::uuid> msg_uuids;
        
        for(auto entry : input_arr)
        {
            if(entry.type() != json_spirit::str_type)
            {
                LOG_WARN << "Entry is not a string UUID";
                return error_json("Entry is not a String UUID");
            }
            
            msg_uuids.push_back( str_gen(entry.get_str()) );
        }
        
        for(auto uuid : msg_uuids)
        {
            LOG_TRACE << "will try to redeliver " << to_string(uuid);

            auto fm = dba::instance().get_message(uuid);
            push_service_.redeliver(fm.msg_uuid, fm.dev_uuid, fm.provider_type);
        }
        
        json_spirit::Object obj;
        
        obj.push_back( json_spirit::Pair("success", true) );
        return json_spirit::write_string( json_spirit::Value(obj), false );
    }
    
    const std::string api_service::handler::list_leavers()
    {
        json_spirit::Array arr;
        
        for(auto entry : dba::instance().get_dead_devices())
        {
            json_spirit::Object obj;
            
            obj.push_back( json_spirit::Pair("uuid", to_string(entry.dev_uuid)) );
            obj.push_back( json_spirit::Pair("timestamp", to_string(entry.ts)) );
            
            arr.push_back(obj);
        }
        
        return json_spirit::write_string( json_spirit::Value(arr), false );
    }
    
    const std::string api_service::handler::remove_device(const std::string& body)
    {
        json_spirit::Value input;
        if(!json_spirit::read_string(body, input))
        {
            LOG_WARN << "Couldn't parse JSON input: '" << body << "'";
            return error_json("Couldn't parse JSON input");
        }
        
        if(input.type() != json_spirit::array_type)
        {
            LOG_WARN << "Passed JSON is not an Array: '" << body << "'";
            return error_json("Not an Array");
        }
        
        auto input_arr = input.get_array();
        
        boost::uuids::string_generator str_gen;
        std::vector<boost::uuids::uuid> dev_uuids;
        
        for(auto entry : input_arr)
        {
            if(entry.type() != json_spirit::str_type)
            {
                LOG_WARN << "Entry is not a string UUID";
                return error_json("Entry is not a String UUID");
            }
            
            dev_uuids.push_back( str_gen(entry.get_str()) );
        }

        LOG_DEBUG << "Parsed " << dev_uuids.size() << " UUIDs of devices to remove";

        json_spirit::Object obj;

        // remove each device
        for(auto uuid : dev_uuids)
        {
            dba::instance().drop_device(uuid);
        }
        
        obj.push_back( json_spirit::Pair("success", true) );
        return json_spirit::write_string( json_spirit::Value(obj), false );
    }
    
    const std::string api_service::handler::list_failed()
    {
        json_spirit::Array arr;
        
        for(auto entry : dba::instance().get_failed_messages())
        {
            json_spirit::Object obj;
            
            obj.push_back( json_spirit::Pair("uuid", to_string(entry.msg_uuid)) );
            obj.push_back( json_spirit::Pair("device", to_string(entry.dev_uuid)) );
            obj.push_back( json_spirit::Pair("msg", entry.reason) );
            
            arr.push_back(obj);
        }
        
        return json_spirit::write_string( json_spirit::Value(arr), false );
    }
    
    const std::string api_service::handler::list_failed_for(const push_type& type)
    {
        json_spirit::Array arr;
        
        for(auto entry : dba::instance().get_failed_messages(type))
        {
            json_spirit::Object obj;
            
            obj.push_back( json_spirit::Pair("uuid", to_string(entry.msg_uuid)) );
            obj.push_back( json_spirit::Pair("device", to_string(entry.dev_uuid)) );
            obj.push_back( json_spirit::Pair("msg", entry.reason) );
            
            arr.push_back(obj);
        }
        
        return json_spirit::write_string( json_spirit::Value(arr), false );
    }
    
    void api_service::handler::operator() (http_service::request const &request,
                                           http_service::response &response)
    try
    {
        std::string uri = request.destination;
        std::string body = request.body;
        
        // destination must start with the api base specified by the client
        if(!boost::starts_with(uri, api_base_))
        {
            response = http_service::response::stock_reply(http_service::response::forbidden);
            return;
        }
        
        // trim base from uri
        uri = uri.substr(api_base_.size());
        
        // check what api we are trying to call
        if(boost::starts_with(uri, "/device/register/apns"))
        {
            std::string res = reg_apns(body);
            
            response = http_service::response::stock_reply(
                http_service::response::ok, res);
            return;
        }

        if(boost::starts_with(uri, "/device/register/gcm"))
        {
            std::string res = reg_gcm(body);
            
            response = http_service::response::stock_reply(
                http_service::response::ok, res);
            return;
        }
        
        if(boost::starts_with(uri, "/device/remove"))
        {
            std::string res = remove_device(body);
            
            response = http_service::response::stock_reply(
                http_service::response::ok, res);
            return;
        }
        
        if(boost::starts_with(uri, "/send"))
        {
            std::string res = send_push(body);
            
            response = http_service::response::stock_reply(
                http_service::response::ok, res);
            return;
        }
        
        if(boost::starts_with(uri, "/redeliver"))
        {
            std::string res = redeliver(body);
            
            response = http_service::response::stock_reply(
                http_service::response::ok, res);
            return;
        }
        
        if(boost::starts_with(uri, "/list_apns"))
        {
            std::string res = list_failed_for(push_type_apns);
            
            response = http_service::response::stock_reply(
                http_service::response::ok, res);
            return;
        }

        if(boost::starts_with(uri, "/list_gcm"))
        {
            std::string res = list_failed_for(push_type_gcm);
            
            response = http_service::response::stock_reply(
                http_service::response::ok, res);
            return;
        }

        if(boost::starts_with(uri, "/list"))
        {
            std::string res = list_failed();
            
            response = http_service::response::stock_reply(
                http_service::response::ok, res);
            return;
        }
        
        if(boost::starts_with(uri, "/leavers"))
        {
            std::string res = list_leavers();
            
            response = http_service::response::stock_reply(
                http_service::response::ok, res);
            return;
        }
        
        
        response = http_service::response::stock_reply(
            http_service::response::not_found);
    }
    catch(std::exception& e)
    {
        LOG_ERROR << "Exception in JSON API: " << e.what();
        response = http_service::response::stock_reply(
            http_service::response::ok, error_json(e.what()));
    }
            
    void api_service::handler::log(http_service::string_type const &info)
    {
        LOG_ERROR << info;
    }
    
    void api_service::run()
    {
        for(int i = 0; i < workers_; ++i)
        {
            // thread_group takes ownership as per documentation
            threads_.add_thread( new boost::thread( boost::bind(&http_service::run, &service_) ) );
            LOG_DEBUG << "JSON API worker spawned.";
        }
    }
}