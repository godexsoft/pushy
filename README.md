Pushy.Ninja - a scalable, standalone push server

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
	- logstash json output logs for apns/gcm events (used for stats via kibana, see tutorial)

LICENSE: 
	TBD
