---
layout: default
---

A scalable, cross-platform, standalone push server. Pushy can scale using a simple round-robin balancer.  
Scroll down to start pushing with __pushy__'s tutorials.

### SPECS

* Language: c++
* Libraries:
  - Boost.Asio
  - Boost.Thread
  - Boost.Log
  - Boost.ProgramOptions
  - Push.Service
  - Cppnet lib
  - OpenSSL
  - Hiredis
* Database: Redis

### FEATURES

- Scalable
- REST/JSON api
- Supports apns and gcm with a unified api
- Logstash JSON output logs for apns/gcm events (used for stats via elasticsearh/kibana, see tutorial)
- Automatic redelivery (configurable)
- Tags attachable to push messages can be used for your campaign stats via elasticsearch

### LICENSE

MIT. 


### TUTORIALS
{% for page in site.pages %}{% if page.isTutorial %}- [{{page.title}}]({{site.baseurl}}{{page.url}}){% endif %}
{% endfor %}
