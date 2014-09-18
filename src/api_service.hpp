//
//  api_service.hpp
//  pushy
//
//  Created by Alexander Kremer on 17/08/2014.
//  Copyright (c) 2014 godexsoft. All rights reserved.
//

#ifndef pushy_api_service_h
#define pushy_api_service_h

#include <boost/thread.hpp>
#include <boost/network.hpp>
#include <boost/network/protocol/http/server.hpp>

#include "database.hpp"

namespace pushy
{
    namespace http = boost::network::http;
    
    class pushy_service;
    
    /**
     * API service
     */
    class api_service
    {
    public:
        struct handler;
        typedef http::server<handler> http_service;
        
        class handler
        {
        public:
            handler(pushy_service& ps, const std::string& base)
            : push_service_(ps)
            , api_base_(base)
            {
            }
            
            void operator() (http_service::request const &request,
                             http_service::response &response);
            
            void log(http_service::string_type const &info);

            // helpers
            const std::string error_json(const std::string& msg);
            
            // apis
            const std::string reg_apns(const std::string& body);
            const std::string reg_gcm(const std::string& body);
            const std::string send_push(const std::string& body);
            const std::string redeliver(const std::string& body);

            const std::string list_leavers();
            const std::string remove_device(const std::string& body);

            const std::string list_failed();
            const std::string list_failed_for(const database::push_type& type);
            
        private:
            pushy_service&      push_service_;
            std::string         api_base_;
        };
        
        api_service(pushy_service& ps,
                    const std::string& addr, const std::string& port,
                    const std::string& base, int workers)
        : handler_(ps, base)
        , options_(handler_)
        , service_(options_.address(addr).port(port))
        , workers_(workers)
        {
            run();
        }
        
        ~api_service()
        {
            stop();
        }
        
        /**
         * Request service halt
         */
        void stop()
        {
            service_.stop();
        }
        
    private:
        void run();
        
        handler                 handler_;
        http_service::options   options_;
        http_service            service_;
        
        uint8_t                 workers_;
        boost::thread_group     threads_;
    };
}

#endif
