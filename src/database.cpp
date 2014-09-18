//
//  database.cpp
//  pushy
//
//  Created by Alexander Kremer on 25/08/2014.
//  Copyright (c) 2014 godexsoft. All rights reserved.
//

#include "database.hpp"
#include "logging.hpp"
#include "base64.hpp"

#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/string_generator.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <redis3m/patterns/scheduler.h>

namespace pushy {
namespace database {
    
    using namespace redis3m;
    using namespace boost::posix_time;
    
    dba dba::inst;
    
    std::string dba::type_to_str(const push_type& type)
    {
        switch(type)
        {
            case push_type_apns:
                return push::apns::key;
            case push_type_gcm:
                return push::gcm::key;
            default:
                throw std::runtime_error("type_to_str failed for type "
                    + boost::lexical_cast<std::string>(type) );
        }
    }
    
    
    void dba::init_pool(const std::string& host, int port)
    {
        LOG_INFO << "opening connection to redis at " << host << ":" << port << "..";
        pool_ = simple_pool::create(host, port);
        
        try
        {
            connection::ptr_t conn = pool_->get();
        }
        catch(...)
        {
            throw std::runtime_error("can't establish connection to redis.");
        }
        
        LOG_INFO << "connection to redis established.";
    }
    
    boost::uuids::uuid dba::register_apns_device(const std::string& token)
    {
        LOG_DEBUG << "registering apns device..";
        return register_device(token, push_type_apns);
    }
    
    boost::uuids::uuid dba::register_gcm_device(const std::string& token)
    {
        LOG_DEBUG << "registering gcm device..";
        return register_device(token, push_type_gcm);
    }
    
    void dba::drop_device(boost::uuids::uuid& uuid)
    {
        LOG_INFO << "dropping device " << to_string(uuid);
        
        std::string field = "device." + to_string(uuid);
        LOG_TRACE << "trying field = " << field;

        connection::ptr_t conn = pool_->get();
        auto token = conn->run(command("HGET") << field << "token").str();
        
        patterns::script_exec scr_mapping("return redis.call('del', unpack(redis.call('keys', 'device_token."
            + util::base64::encode(token.c_str(), token.size()) + "')))");
        
        patterns::script_exec scr_dev("return redis.call('del', unpack(redis.call('keys', 'device."
            + to_string(uuid) + "')))");
        
        scr_mapping.exec(conn);
        scr_dev.exec(conn);
    }
    
    void dba::mark_device_dead(boost::uuids::uuid& uuid, const ptime& time)
    {
        LOG_INFO << "marking device " << to_string(uuid) << " as dead";
        
        connection::ptr_t conn = pool_->get();
        conn->run(command("SADD") << "dead_devices" << to_string(uuid) );
        
        std::string field = "device." + to_string(uuid);
        LOG_TRACE << "trying field = " << field;
        
        conn->run(command("HSET") << field << "death_time" << time);
    }
    
    std::vector<dba::dead_device_entry> dba::get_dead_devices()
    {
        std::vector<dba::dead_device_entry> res;
        boost::uuids::string_generator str_gen;
        
        connection::ptr_t conn = pool_->get();
        LOG_TRACE << "listing dead devices from 'dead_devices'";
        
        auto rep = conn->run(command("SMEMBERS") << "dead_devices");
        for(auto element : rep.elements())
        {
            auto item_field = "device." + element.str();
            LOG_TRACE << "item field = " << item_field;
            
            dba::dead_device_entry entry;
            
            entry.dev_uuid = str_gen(element.str());
            entry.ts = time_from_string(conn->run(command("HGET") << item_field << "death_time").str());
            
            res.push_back(entry);
        }
        
        return res;
    }
    
    push_type dba::get_device_type(boost::uuids::uuid& dev_uuid)
    {
        LOG_DEBUG << "getting device type for device " << dev_uuid;

        connection::ptr_t conn = pool_->get();
        
        std::string field = "device." + to_string(dev_uuid);
        LOG_TRACE << "trying field = " << field;

        // first check if this device exists at all
        if(conn->run(command("EXISTS") << field).integer() == 0)
        {
            return push_type_invalid;
        }

        // FIXME: this is a bit unsafe if db got a value not supported by push_type
        return static_cast<push_type>(
            boost::lexical_cast<int>(
                conn->run(command("HGET") << field << "type").str() ) );
    }
    
    dba::msg_entry dba::get_message(boost::uuids::uuid& uuid) const
    {
        LOG_TRACE << "getting push message details " << to_string(uuid);
        
        connection::ptr_t conn = pool_->get();
        boost::uuids::string_generator str_gen;
        
        std::string field = "message." + to_string(uuid);
        LOG_TRACE << "trying field = " << field;

        msg_entry entry;
        
        entry.msg_uuid = uuid;
        entry.dev_uuid = str_gen(conn->run(command("HGET") << field << "device").str());
        entry.tag = conn->run(command("HGET") << field << "tag").str();
        
        // FIXME: this is a bit unsafe if db got a value not supported by push_type
        entry.provider_type = static_cast<push_type>(
            boost::lexical_cast<int>(
                conn->run(command("HGET") << field << "type").str() ) );
        
        entry.ts = time_from_string(conn->run(command("HGET") << field << "timestamp").str());
        
        auto attempts_str = conn->run(command("HGET") << field << "attempts").str();
        entry.attempts = 1;
        
        // FIXME: this is a bit ugly :/
        // maybe we should set it to 1 on first push etc.
        
        if(!attempts_str.empty())
        {
            entry.attempts = boost::lexical_cast<uint32_t>(attempts_str);
        }
        
        return entry;
    }

    std::vector<dba::failed_msg_entry> dba::get_failed_messages()
    {
        std::vector<dba::failed_msg_entry> res;

        auto apns_msgs = get_failed_messages(push_type_apns);
        auto gcm_msgs  = get_failed_messages(push_type_gcm);
        
        auto it = res.insert(res.begin(), apns_msgs.begin(), apns_msgs.end());
        res.insert(it, gcm_msgs.begin(), gcm_msgs.end());
        
        return res;
    }
    
    std::vector<dba::failed_msg_entry> dba::get_failed_messages(const push_type& type)
    {
        std::vector<dba::failed_msg_entry> res;
        boost::uuids::string_generator str_gen;
        
        connection::ptr_t conn = pool_->get();

        auto type_str = type_to_str(type);
        LOG_TRACE << "listing failed messages from 'failed_messages." << type_str << "' set";

        auto rep = conn->run(command("SMEMBERS") << "failed_messages." + type_str);
        for(auto element : rep.elements())
        {
            auto item_field = "message." + element.str();
            LOG_TRACE << "item field = " << item_field;
            
            dba::failed_msg_entry entry;
            
            entry.msg_uuid = str_gen(element.str());
            entry.dev_uuid = str_gen(conn->run(command("HGET") << item_field << "device").str());
            entry.reason   = conn->run(command("HGET") << item_field << "reason").str();
            entry.attempts = boost::lexical_cast<uint32_t>(
                conn->run(command("HGET") << item_field << "attempts").str() );
            
            res.push_back(entry);
        }
        
        return res;
    }
    
    push::device dba::get_apns_device(boost::uuids::uuid& dev_uuid)
    {
        return push::device(push::apns::key, util::base64::decode( get_device_token(dev_uuid) ) );
    }

    push::device dba::get_gcm_device(boost::uuids::uuid& dev_uuid)
    {
        return push::device(push::gcm::key, get_device_token(dev_uuid));
    }
    
    boost::uuids::uuid dba::write_apns_push(boost::uuids::uuid& dev_uuid, const std::string& payload, const std::string& tag)
    {
        return write_push(dev_uuid, push_type_apns, payload, tag);
    }

    boost::uuids::uuid dba::write_gcm_push(boost::uuids::uuid& dev_uuid, const std::string& payload, const std::string& tag)
    {
        return write_push(dev_uuid, push_type_gcm, payload, tag);
    }

    uint32_t dba::mark_push_record_failed(boost::uuids::uuid& uuid, const std::string& msg)
    {
        LOG_DEBUG << "marking push message as failed " << uuid;
        
        std::string field = "message." + to_string(uuid);
        LOG_TRACE << "field = " << field;
        
        connection::ptr_t conn = pool_->get();
        auto msg_type = conn->run(command("HGET") << field << "type").str();
        
        // get string type to determine which set to use
        // FIXME: this is a bit unsafe if db got a value not supported by push_type
        auto msg_type_str = type_to_str( static_cast<push_type>( boost::lexical_cast<int>(msg_type) ) );
        
        conn->run(command("HSET") << field << "reason" << msg);
        conn->run(command("SADD") << "failed_messages." + msg_type_str << to_string(uuid) );

        // by default will set to 0 and inc by 1
        return static_cast<uint32_t>(conn->run(command("HINCRBY") << field << "attempts" << 1).integer());
    }
    
    boost::uuids::uuid dba::find_device_by_token64(const std::string& token) const
    {
        LOG_DEBUG << "looking up device by token (base64): " << token;
        
        std::string field = "device_token." + token;
        LOG_TRACE << "field = " << field;
        
        connection::ptr_t conn = pool_->get();
        auto uuid_str = conn->run(command("GET") << field).str();

        boost::uuids::string_generator str_gen;
        return str_gen(uuid_str);
    }
    
    bool dba::remove_from_failed_messages(boost::uuids::uuid& uuid)
    {
        LOG_DEBUG << "removing message " << uuid << " from failed set";
        
        std::string field = "message." + to_string(uuid);
        LOG_TRACE << "field = " << field;
        
        connection::ptr_t conn = pool_->get();
        auto msg_type = conn->run(command("HGET") << field << "type").str();
        
        // get string type to determine which set to use
        // FIXME: this is a bit unsafe if db got a value not supported by push_type
        auto msg_type_str = type_to_str( static_cast<push_type>( boost::lexical_cast<int>(msg_type) ) );
        return conn->run(command("SREM") << "failed_messages." + msg_type_str << to_string(uuid)).integer();
    }
    
    void dba::drop_push_record(boost::uuids::uuid& uuid)
    {
        LOG_DEBUG << "dropping push message record " << uuid;
        
        patterns::script_exec scr("return redis.call('del', unpack(redis.call('keys', 'message." + to_string(uuid) + "')))");
        
        connection::ptr_t conn = pool_->get();
        scr.exec(conn);
    }
    
    std::string dba::get_message_payload(boost::uuids::uuid& uuid) const
    {
        LOG_DEBUG << "getting message payload for " << to_string(uuid);
        
        std::string field = "message." + to_string(uuid);
        LOG_TRACE << "field = " << field;
        
        connection::ptr_t conn = pool_->get();
        return conn->run(command("HGET") << field << "payload").str();
    }

    boost::uuids::uuid dba::register_device(const std::string& token, const push_type& type)
    {
        boost::uuids::random_generator gen;
        boost::uuids::uuid uuid = gen();
        
        std::string field = "device." + to_string(uuid);
        LOG_TRACE << "field = " << field;
        
        connection::ptr_t conn = pool_->get();
        conn->run(command("HMSET") << field << "type" << type << "token" << token);

        // register token:device mapping
        field = "device_token." + token;
        LOG_TRACE << "token:device mapping field = " << field;
        
        conn->run(command("SET") << field << util::base64::encode(token.c_str(), token.size()));
        
        return uuid;
    }
    
    std::string dba::get_device_token(boost::uuids::uuid& dev_uuid)
    {
        LOG_TRACE << "getting token for device " << dev_uuid;
        
        std::string field = "device." + to_string(dev_uuid);
        LOG_TRACE << "field = " << field;
        
        connection::ptr_t conn = pool_->get();
        auto token = conn->run(command("HGET") << field << "token").str();
        LOG_TRACE << "acquired token = " << token;
        
        return token;
    }

    boost::uuids::uuid dba::write_push(boost::uuids::uuid& dev_uuid, const push_type& type,
                                       const std::string& payload, const std::string& tag)
    {
        LOG_TRACE << "writing new push message record";
        
        boost::uuids::random_generator gen;
        boost::uuids::uuid uuid = gen();
        
        std::string field = "message." + to_string(uuid);
        LOG_TRACE << "field = " << field;
        
        connection::ptr_t conn = pool_->get();
        conn->run(command("HMSET") << field << "payload" << payload
                  << "type" << type << "device" << to_string(dev_uuid)
                  << "timestamp" << microsec_clock::universal_time()
                  << "tag" << tag);
        
        return uuid;
    }
    
} // database
} // pushy