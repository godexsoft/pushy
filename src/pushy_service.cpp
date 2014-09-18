//
//  pushy_service.cpp
//  pushy
//
//  Created by Alexander Kremer on 18/08/2014.
//  Copyright (c) 2014 godexsoft. All rights reserved.
//

#include "pushy_service.hpp"
#include "logging.hpp"
#include "base64.hpp"

#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

#include <push_service.hpp>

using namespace push;
using namespace pushy::database;

namespace pushy
{
    /*
     * APNS handlers
     */
    void pushy_service::on_apns(const boost::system::error_code& err, const uint32_t& ident)
    {
        // get the message uuid using this ident from local cache
        if(!apns_cache_.count(ident))
        {
            LOG_ERROR << "APNS message identifier not found in local node's cache. Fatal error which should never happen.";
            throw std::runtime_error("APNS message identifier not found in local node's cache. Fatal error which should never happen.");
        }
        
        auto uuid = apns_cache_[ident];
        
        if(!err)
        {
            LOG_INFO << "message " << uuid
                << " is sent successfully thru APNS.";
            
            LOG_APNS("sent", uuid, "sent successfully");
            dba::instance().drop_push_record(uuid);
        }
        else
        {
            LOG_WARN << "error for message " << to_string(uuid) << ": "
                << err.message();
            
            auto attempts = dba::instance().mark_push_record_failed(uuid, err.message());
            if(redeliver_ && redeliver_attempts_ <= attempts)
            {
                LOG_INFO << "message " << to_string(uuid) << " exceeded redelivery attempts. removing it completely.";
                if(dba::instance().remove_from_failed_messages(uuid))
                {
                    // no other node beat us to it
                    LOG_APNS("permanent_failure", uuid, "permanently failed. reason: " + err.message());
                    dba::instance().drop_push_record(uuid);
                }
            }
            else
            {
                LOG_APNS("redeliverable_failure", uuid, "failed. will try to redeliver. reason: " + err.message());
            }
        }
    }
    
    void pushy_service::on_apns_feed(const boost::system::error_code& err,
                                     const std::string& token,
                                     const boost::posix_time::ptime& time)
    {
        if(!err)
        {
            LOG_TRACE << "feedback time: " << time << " for token " << token;

            auto uuid = dba::instance().find_device_by_token64(
                util::base64::encode(token.c_str(), token.size() ) );

            LOG_APNS_DEVICE("device_unsubscribed", uuid, time, "device reported as unsubscribed");
            
            if(deregister_)
            {
                // automatically remove device
                dba::instance().drop_device(uuid);
                
                LOG_APNS("device_dropped", uuid, "device automatically dropped from redis db");
            }
            else
            {
                // add device to removed devices list instead of removing
                dba::instance().mark_device_dead(uuid, time);
                
                LOG_APNS_DEVICE("device_marked_unsubscribed", uuid, time, "device marked as dead");
            }
        }
        else if(err == push::error::shutdown)
        {
            LOG_WARN << "socket of feedback channel was shutdown by remote host.";
            LOG_APNS_GENERIC("socket_shutdown", time, "socket of feedback channel was shutdown by remote host");
        }
    }

    /*
     * GCM handlers
     */
    void pushy_service::on_gcm(const boost::system::error_code& err, const uint32_t& ident)
    {
        // get the message uuid using this ident from local cache
        if(!gcm_cache_.count(ident))
        {
            LOG_ERROR << "GCM message identifier not found in local node's cache. Fatal error which should never happen.";
            throw std::runtime_error("GCM message identifier not found in local node's cache. Fatal error which should never happen.");
        }
        
        auto uuid = gcm_cache_[ident];

        if(!err)
        {
            LOG_INFO << "message " << uuid
                << " is sent successfully thru GCM.";
            
            LOG_GCM("sent", uuid, "sent successfully");
            dba::instance().drop_push_record(uuid);
        }
        else
        {
            LOG_WARN << "GCM error for message " << uuid << ": "
                << err.message();
            
            auto attempts = dba::instance().mark_push_record_failed(uuid, err.message());
            if(redeliver_ && redeliver_attempts_ <= attempts)
            {
                LOG_INFO << "message " << to_string(uuid) << " exceeded redelivery attempts. removing it completely.";
                if(dba::instance().remove_from_failed_messages(uuid))
                {
                    LOG_GCM("permanent_failure", uuid, "permanently failed. reason: " + err.message());
                    
                    // no other node beat us to it
                    dba::instance().drop_push_record(uuid);
                }
            }
            else
            {
                LOG_GCM("redeliverable_failure", uuid, "failed. will try to redeliver. reason: " + err.message());
            }
        }
    }

    
    /*
     * Setup
     */
    void pushy_service::setup_apns(const std::string& mode, const std::string& p12_file,
                                   const std::string& password, int poolsize)
    {
        apns::config ap = apns::config::sandbox(p12_file);
        ap.p12_pass = password;
        
        apns_feedback::config apf = apns_feedback::config::sandbox(p12_file);
        apf.p12_pass = password;
        
        if(mode == "production")
        {
            ap = apns::config::production(p12_file);
            ap.p12_pass = password;
            
            apf = apns_feedback::config::production(p12_file);
            apf.p12_pass = password;
        }
        
        ap.pool_size = poolsize;
        
        ap.callback =
            boost::bind(&pushy_service::on_apns, this, _1, _2);
        
        // create the apns push service runner
        apns_ = boost::shared_ptr<push::apns>( new push::apns(ps_, ap) );
        
        apf.callback =
            boost::bind(&pushy_service::on_apns_feed, this, _1, _2, _3);
        
        // create the apns feedback listener
        apns_feedback_ = boost::shared_ptr<apns_feedback>( new apns_feedback(ps_, apf) );
    }

    void pushy_service::setup_gcm(const std::string& gcm_project_id, const std::string& gcm_api_key, int poolsize)
    {
        // create the gcm push service runner
        gcm_ = boost::shared_ptr<push::gcm>(
            new push::gcm(ps_,
                gcm_project_id, gcm_api_key, poolsize,
                    boost::bind(&pushy_service::on_gcm, this, _1, _2)
            )
        );
    }
    
    void pushy_service::run()
    {
        if(apns_feedback_)
        {
            // start getting the apns feed
            apns_feedback_->start();
        }
        
        // and finally run the whole service
        io_.run();
    }
    
    void pushy_service::reset_redelivery_timer(uint32_t sec)
    {
        redelivery_timer_.expires_from_now(boost::posix_time::seconds(sec));
        redelivery_timer_.async_wait(
            boost::bind(&pushy_service::on_check_redelivery,
                this, boost::asio::placeholders::error) );
    }
    
    void pushy_service::on_check_redelivery(const boost::system::error_code& err)
    {
        if(err != boost::asio::error::operation_aborted)
        {
            LOG_TRACE << "redelivery timer fired. check redelivery..";
            
            if(apns_)
            {
                auto apns_msgs = dba::instance().get_failed_messages(push_type_apns);

                for(auto msg : apns_msgs)
                {
                    LOG_TRACE << "APNS message to redeliver: " << to_string(msg.msg_uuid);
                    redeliver(msg.msg_uuid, msg.dev_uuid, push_type_apns);
                }
            }

            if(gcm_)
            {
                auto gcm_msgs  = dba::instance().get_failed_messages(push_type_gcm);
                
                for(auto msg : gcm_msgs)
                {
                    LOG_TRACE << "GCM message to redeliver: " << to_string(msg.msg_uuid);
                    
                    // lets say we redeliverd it and it failed
                    // dba::instance().mark_push_record_failed(msg.msg_uuid, "retry failure"); // for now it's here
                    redeliver(msg.msg_uuid, msg.dev_uuid, push_type_gcm);
                }
            }
            
            // and keep going
            reset_redelivery_timer(5); // TODO: configurable?
        }
        else
        {
            LOG_TRACE << "redelivery timer aborted.";
        }
    }

    
    /*
     * API
     */
    // TODO: this runs on multiple threads so look carefully
    // about usage of apns_cache_ and apns_identifier_.
    boost::uuids::uuid pushy_service::push(boost::uuids::uuid& dev_uuid, const std::string& msg, const std::string& tag)
    {
        LOG_INFO << "trying to push message to " << to_string(dev_uuid);
        
        // find out if it's apns or gcm, or maybe does not exist
        auto type = dba::instance().get_device_type(dev_uuid);
        if(type == push_type_apns)
        {
            if(!apns_)
            {
                LOG_ERROR << "APNS device detected but APNS push service is not setup properly.";
                throw std::runtime_error("APNS is not setup properly. unable to send.");
            }
            
            LOG_DEBUG << "APNS device detected. pushing thru apns.";
            
            apns_message push_msg;
            push_msg.alert = msg;
            
            auto payload = push_msg.to_json();
            auto uuid = dba::instance().write_apns_push(dev_uuid, payload, tag);
            auto dev = dba::instance().get_apns_device(dev_uuid);
            
            int32_t ident = apns_identifier_++;

            // cache this identifier mapped to uuid of message
            // TODO: this is not safe in multithreaded env?
            apns_cache_[ident] = uuid;
            apns_->post(dev, payload, 0, ident);
            
            return uuid;
        }
        else if(type == push_type_gcm)
        {
            if(!gcm_)
            {
                LOG_ERROR << "GCM device detected but GCM push service is not setup properly.";
                throw std::runtime_error("GCM is not setup properly. unable to send.");
            }
            
            LOG_DEBUG << "GCM device detected. pushing thru gcm.";
            int32_t ident = gcm_identifier_++;
            
            gcm_message push_msg(ident);
            push_msg.add("msg", msg);
            
            auto dev = dba::instance().get_gcm_device(dev_uuid);
            
            push_msg.add_reg_id(dev.token);
            auto payload = push_msg.to_json();
            auto uuid = dba::instance().write_gcm_push(dev_uuid, payload, tag);
            
            // cache this identifier mapped to uuid of message
            // TODO: this is not safe in multithreaded env?
            gcm_cache_[ident] = uuid;

            gcm_->post(dev, payload, 0, ident);
            
            return uuid;
        }
        
        throw std::runtime_error("requested to push for unknown device type");
    }
    
    void pushy_service::redeliver(boost::uuids::uuid& msg_uuid,
                                  boost::uuids::uuid& dev_uuid,
                                  const push_type& type)
    {
        // if remove_from_failed_messages returns false it means that
        // another node has taken this for redelivery, so we just skip it here.
        if(! dba::instance().remove_from_failed_messages(msg_uuid))
        {
            LOG_TRACE << "message " << to_string(msg_uuid) << " was already taken"
                << " for redelivery by another node.";
            return;
        }
        
        if(type == push_type_apns)
        {
            LOG_DEBUG << "APNS message. pushing thru apns.";
            
            auto dev = dba::instance().get_apns_device(dev_uuid);
            auto payload = dba::instance().get_message_payload(msg_uuid);

            int32_t ident = apns_identifier_++;
            
            // cache this identifier mapped to uuid of message
            // TODO: this is not safe in multithreaded env?
            apns_cache_[ident] = msg_uuid;
            apns_->post(dev, payload, 0, ident);
        }
        else if(type == push_type_gcm)
        {
            LOG_DEBUG << "GCM message. pushing thru gcm.";

            auto dev = dba::instance().get_gcm_device(dev_uuid);
            auto payload = dba::instance().get_message_payload(msg_uuid);
            
            int32_t ident = gcm_identifier_++;
            
            // cache this identifier mapped to uuid of message
            // TODO: this is not safe in multithreaded env?
            gcm_cache_[ident] = msg_uuid;
            gcm_->post(dev, payload, 0, ident);
        }
    }
}
