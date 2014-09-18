//
//  pushy_service.hpp
//  pushy
//
//  Created by Alexander Kremer on 18/08/2014.
//  Copyright (c) 2014 godexsoft. All rights reserved.
//

#ifndef pushy_pushy_service_hpp
#define pushy_pushy_service_hpp

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>

#include <push_service.hpp>
#include "database.hpp"

namespace pushy
{
    namespace io = boost::asio;

    /**
     * Pushy service
     */
    class pushy_service
    {
    public:
        pushy_service(bool auto_redeliver, uint32_t auto_redeliver_attempts, bool auto_deregister)
        : ps_(io_)
        , work_(io_)
        , apns_identifier_(0)
        , gcm_identifier_(0)
        , redelivery_timer_(io_) 
        , redeliver_(auto_redeliver)
        , redeliver_attempts_(auto_redeliver_attempts)
        , deregister_(auto_deregister)
        {
            if(redeliver_)
            {
                reset_redelivery_timer(5); // TODO: configurable?
            }
        }
        
        void setup_apns(const std::string& mode,
                        const std::string& p12_file,
                        const std::string& password,
                        int poolsize);
        
        void setup_gcm(const std::string& gcm_project_id,
                       const std::string& gcm_api_key,
                       int poolsize);
        
        boost::uuids::uuid push(boost::uuids::uuid& dev_uuid, const std::string& msg, const std::string& tag);
        void redeliver(boost::uuids::uuid& msg_uuid, boost::uuids::uuid& dev_uuid, const database::push_type& type);
        
        void run();
        
    private:
        
        // APNS handlers
        void on_apns(const boost::system::error_code& err, const uint32_t& ident);
        void on_apns_feed(const boost::system::error_code& err,
                          const std::string& token,
                          const boost::posix_time::ptime& time);
        
        // GCM handlers
        void on_gcm(const boost::system::error_code& err, const uint32_t& ident);
        
        void reset_redelivery_timer(uint32_t sec);
        void on_check_redelivery(const boost::system::error_code& err);
        
        io::io_service          io_;
        push::push_service      ps_;
        io::io_service::work    work_;
        
        // plugins
        boost::shared_ptr<push::apns>           apns_;
        boost::shared_ptr<push::apns_feedback>  apns_feedback_;
        std::atomic_int_fast32_t                apns_identifier_;
        std::map<uint32_t, boost::uuids::uuid>  apns_cache_;
        
        boost::shared_ptr<push::gcm>            gcm_;
        std::atomic_int_fast32_t                gcm_identifier_;
        std::map<uint32_t, boost::uuids::uuid>  gcm_cache_;
        
        // automation
        bool        redeliver_;
        uint32_t    redeliver_attempts_;
        bool        deregister_;
        
        io::deadline_timer redelivery_timer_;
    };
}

#endif
