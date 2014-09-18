//
//  database.hpp
//  pushy
//
//  Created by Alexander Kremer on 25/08/2014.
//  Copyright (c) 2014 godexsoft. All rights reserved.
//

#ifndef __pushy__database__
#define __pushy__database__

#include <string>
#include <boost/uuid/uuid.hpp>
#include <push_service.hpp>
#include <redis3m/redis3m.hpp>

namespace pushy {
namespace database {
    
    enum push_type
    {
        push_type_apns = 0,
        push_type_gcm  = 1,
        push_type_invalid = 127
    };
    
    class dba
    {
    public:
        
        struct failed_msg_entry
        {
            boost::uuids::uuid msg_uuid;
            boost::uuids::uuid dev_uuid;
            std::string        reason;
            uint32_t           attempts;
        };
        
        struct msg_entry
        {
            boost::uuids::uuid          msg_uuid;
            boost::uuids::uuid          dev_uuid;
            uint32_t                    attempts;
            boost::posix_time::ptime    ts;
            push_type                   provider_type;
            std::string                 tag;
        };
        
        struct dead_device_entry
        {
            boost::uuids::uuid          dev_uuid;
            boost::posix_time::ptime    ts;
        };
        
        static dba& instance()
        {
            return inst;
        }
        
        static std::string type_to_str(const push_type& type);
        
        void init_pool(const std::string& host, int port);
        
        boost::uuids::uuid register_apns_device(const std::string& token);
        boost::uuids::uuid register_gcm_device(const std::string& token);

        push_type get_device_type(boost::uuids::uuid& dev_uuid);
        push::device get_apns_device(boost::uuids::uuid& dev_uuid);
        push::device get_gcm_device(boost::uuids::uuid& dev_uuid);
        std::string get_device_token(boost::uuids::uuid& dev_uuid);
        
        boost::uuids::uuid write_apns_push(boost::uuids::uuid& dev_uuid, const std::string& payload, const std::string& tag);
        boost::uuids::uuid write_gcm_push(boost::uuids::uuid& dev_uuid, const std::string& payload, const std::string& tag);
        uint32_t mark_push_record_failed(boost::uuids::uuid& uuid, const std::string& msg);
        bool remove_from_failed_messages(boost::uuids::uuid& uuid);
        
        void drop_push_record(boost::uuids::uuid& uuid);
        std::string get_message_payload(boost::uuids::uuid& uuid) const;
        
        boost::uuids::uuid find_device_by_token64(const std::string& token) const;
        void drop_device(boost::uuids::uuid& uuid);
        void mark_device_dead(boost::uuids::uuid& uuid, const boost::posix_time::ptime& time);
        
        /// returns a list of uuids of failed messages
        std::vector<failed_msg_entry> get_failed_messages();
        
        /// returns a list of uuids of failed messages for a given provider type
        std::vector<dba::failed_msg_entry> get_failed_messages(const push_type& type);
        
        /// returns a list of dead devices
        std::vector<dba::dead_device_entry> get_dead_devices();
        
        msg_entry get_message(boost::uuids::uuid& uuid) const;
        
    private:
        boost::uuids::uuid write_push(boost::uuids::uuid& dev_uuid, const push_type& type, const std::string& payload, const std::string& tag);
        boost::uuids::uuid register_device(const std::string& token, const push_type& type);
        
        dba()
        {}
        
        redis3m::simple_pool::ptr_t pool_;
        static dba inst;
    };
    
} // database
} // pushy

#endif /* defined(__pushy__database__) */
