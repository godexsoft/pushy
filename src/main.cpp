//
// PUSHY - standalone push server
// godexsoft 2014
//

#include <boost/program_options.hpp>
#include <boost/asio.hpp>

#include <push_service.hpp>
#include <vector>

#include "logging.hpp"
#include "api_service.hpp"
#include "pushy_service.hpp"
#include "database.hpp"

namespace po = boost::program_options;

using namespace boost::asio;
using namespace push;
using namespace pushy;
using namespace pushy::database;


int main(int ac, char **av)
try
{
    // global options
    std::string config_path;
    std::string logfile;
    severity_t  loglevel;
    
    // db options
    std::string redis_host;
    int         redis_port;
    
    // automation options
    bool auto_redeliver;
    int  auto_redeliver_attempts;
    bool auto_deregister;
    
    // api options
    std::string api_base_uri;
    std::string api_addr;
    std::string api_port;
    int         api_workers;
    
    // apns options
    std::string apns_key; // pem
    std::string apns_cert; // pem
    std::string apns_password; // p12 password
    std::string apns_p12_cert_key; // p12 file with both cert and key
    std::string apns_mode;
    int         apns_poolsize;
    std::string apns_logfile;
    
    // gcm options
    std::string gcm_project_id;
    std::string gcm_api_key;
    int         gcm_poolsize;
    std::string gcm_logfile;
    
    // a banner just for fun
    static char banner[] = "\n"
    "  ██████╗ ██╗   ██╗███████╗██╗  ██╗██╗   ██╗\n"
    "  ██╔══██╗██║   ██║██╔════╝██║  ██║╚██╗ ██╔╝\n"
    "  ██████╔╝██║   ██║███████╗███████║ ╚████╔╝ \n"
    "  ██╔═══╝ ██║   ██║╚════██║██╔══██║  ╚██╔╝  \n"
    "  ██║     ╚██████╔╝███████║██║  ██║   ██║   \n"
    "  ╚═╝      ╚═════╝ ╚══════╝╚═╝  ╚═╝   ╚═╝   \n";
    
    // option parsing
    po::options_description generic_config("Generic");
    generic_config.add_options()
        ("help,h", "produce help message")
        ("config,c", po::value<std::string>(&config_path), "config file to read")
        ("loglevel,L", po::value<severity_t>(&loglevel)->default_value(severity_t("info")),
            "log level (trace, debug, info, warning, error)")
        ("logfile,l", po::value<std::string>(&logfile), "logfile to use instead of standard output; see docs for format options")
    ;

    po::options_description redis_config("Redis");
    redis_config.add_options()
        ("redis.host", po::value<std::string>(&redis_host)->default_value("127.0.0.1"),
            "redis host")
        ("redis.port", po::value<int>(&redis_port)->default_value(6379),
            "redis port")
    ;
    
    po::options_description auto_config("Automation");
    auto_config.add_options()
        ("auto.redeliver,r", po::value<bool>(&auto_redeliver)->default_value(true),
            "automatically redeliver undelivered messages")
        ("auto.attempts,t", po::value<int>(&auto_redeliver_attempts)->default_value(3),
            "times to try deliver a message before giving up (only if auto.redeliver is on)")
        ("auto.deregister,d", po::value<bool>(&auto_deregister)->default_value(true),
            "automatically deregister devices which reported as unreachable")
    ;

    po::options_description api_config("JSON API");
    api_config.add_options()
        ("api.base,b", po::value<std::string>(&api_base_uri)->default_value("/api"), "base uri")
        ("api.address,a", po::value<std::string>(&api_addr)->default_value("0.0.0.0"), "address")
        ("api.port,p", po::value<std::string>(&api_port)->default_value("7446"), "port")
        ("api.workers,w", po::value<int>(&api_workers)->default_value(1), "workers to spawn (threads)")
    ;
    
    po::options_description apns_config("APNS");
    apns_config.add_options()
        ("apns.p12", po::value<std::string>(&apns_p12_cert_key), "cert and key p12 file path")
        ("apns.password", po::value<std::string>(&apns_password), "password for the key if needed")
        ("apns.mode", po::value<std::string>(&apns_mode)->default_value("sandbox"), "mode ('production' or 'sandbox')")
        ("apns.pool", po::value<int>(&apns_poolsize)->default_value(1), "pool size (connections count)")
        ("apns.logfile", po::value<std::string>(&apns_logfile), "logstash JSON format logfile for APNS stats")
    ;

    po::options_description gcm_config("GCM");
    gcm_config.add_options()
        ("gcm.project", po::value<std::string>(&gcm_project_id), "project id")
        ("gcm.key", po::value<std::string>(&gcm_api_key), "api key")
        ("gcm.pool", po::value<int>(&gcm_poolsize)->default_value(1), "pool size (connections count)")
        ("gcm.logfile", po::value<std::string>(&gcm_logfile), "logstash JSON format logfile for GCM stats")
    ;

    po::options_description desc("Pushy server options");
    desc.add(generic_config).add(redis_config).add(auto_config)
        .add(api_config).add(apns_config).add(gcm_config);

    // initialize logger's basic properties
    logging::init_basics();
    
    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);
    po::notify(vm);

    std::cout << banner << "\n";
    
    if(vm.count("help"))
    {
        std::cout << desc << "\n";
        return 1;
    }
    
    if(vm.count("config"))
    {
        po::store(po::parse_config_file<char>(config_path.c_str(), desc), vm);
        po::notify(vm);
    }
    
    // initialize logger to files etc.
    logging::init(logfile, loglevel.level, apns_logfile, gcm_logfile);
    
    // initialize redis database connection pool
    dba::instance().init_pool(redis_host, redis_port);
    
    // the push service
    pushy_service service(auto_redeliver, auto_redeliver_attempts, auto_deregister);
        
    // now check if apns, gcm, etc. are enabled
    if(vm.count("apns.p12"))
    {
        if(apns_mode != "sandbox" && apns_mode != "production")
        {
            throw std::runtime_error("apns.mode must be either 'sandbox' or 'production'");
        }
        
        service.setup_apns(apns_mode, apns_p12_cert_key, apns_password, apns_poolsize);
        LOG_DEBUG << "configure apns with " << apns_mode << " p12=" << apns_p12_cert_key;
    }
    
    // gcm
    if(vm.count("gcm.project") || vm.count("gcm.key"))
    {
        if(!vm.count("gcm.project") || !vm.count("gcm.key"))
        {
            throw std::runtime_error("both gcm.project and gcm.key must be defined in order to use GCM");
        }
        
        LOG_DEBUG << "configure gcm with project_id=" << gcm_project_id << ", key=" << gcm_api_key;
        service.setup_gcm(gcm_project_id, gcm_api_key, gcm_poolsize);
        LOG_DEBUG << "configuration of gcm done";
    }
        
    LOG_INFO << "running json api service.";
        
    // create the api service. it runs on background thread automatically
    api_service api(service, api_addr, api_port, api_base_uri, api_workers);

    LOG_INFO << "running pushy service.";
        
    // run pushy service
    service.run();
}
catch(push::exception::push_exception& e)
{
    LOG_ERROR << "push_service error: " << e.what();
}
catch(std::exception& e)
{
    LOG_ERROR << "fatal: " << e.what();
}
    