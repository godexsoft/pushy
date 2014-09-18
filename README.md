Pushy - a scalable, standalone push server

SPECS:
	Language: c++

	Libraries:
		- boost.asio
		- boost.log
		- push.service
		- cppnet lib

	Database: redis

FEATURES:
	- highly scalable
	- rest/json api
	- supports apns and gcm with a unified api
	- logstash json output logs for apns/gcm events (used for stats via elasticsearh/kibana, see tutorial)
	- automatic redelivery (configurable)
	- tags attachable to push messages can be used for your campaign stats via elasticsearch

LICENSE: 
	MIT
