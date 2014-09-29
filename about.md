---
isTutorial: false
layout: page
title: About
permalink: /about/
---
##What pushy is##

* Pushy is a standalone, yet scalable push server with support for `APNS` and `GCM`.
* It has a well-defined RESTful JSON API, automatic redelivery, `logstash` JSON formatted logs
* and It could be used as part of your technology stack in the near future.
* Pushy is trying to be simple to install and use, yet powerful enough for any deployment.

##What pushy is not##

* It's not a distributed system (it doesn't have a load balancer of its own).
* Pushy is not a database. It uses `redis` under the hood.
* Pushy can't push to anything except APNS and GCM (yet).

##Roadmap##

* Implement a _selftest_ suite (apns/gcm simulators) and make it part of the build process.
* Finish tutorials and documentation.
* Add `e-mail` and `twitter` push providers.
* Look into adding `Amazon SNS` and possibly others (Microsoft and Blackberry to name a few).

##Who is to blame##

Pushy is developed and maintained by [godexsoft](http://github.com/godexsoft).  
If you have any questions, bug reports and/or feature requests - send them
thru [the github issues page](https://github.com/godexsoft/pushy/issues).
