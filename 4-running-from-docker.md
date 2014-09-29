---
isTutorial: true
layout: page
title: Running with docker
permalink: /running-with-docker/
---
In this tutorial you will learn how to run __pushy__ using docker.

##Notes on MacOS X##

This section is for MacOS X users only. Skip if you are using Linux or others.  
On MacOS X docker runs on a virtual machine which is maintained by `boot2docker`.

The only real important thing to remember is that both docker `volumes` and `ip addresses`
should be specified in respect to `boot2docker`.

Try this out:

{% highlight bash %}
$ boot2docker init
$ boot2docker up
$ boot2docker ip # tells you your virtual machine's ip address
$ boot2docker ssh # ssh into the virtual machine
{% endhighlight %}

Oh, and finally, to use `scp`:

{% highlight bash %}
$ ssh-add ~/.ssh/id_boot2docker
$ scp /path/to/local/file docker@$(boot2docker ip):/path/to/remote/file
{% endhighlight %}

##Create your configuration file##

First we will have to create a configuration file for __pushy__.

> __Note:__
Refer to the configuration tutorial for more details on configuring pushy.

Let's create a directory for this tutorial and start configuring.

{% highlight bash %}
$ mkdir ~/pushy
$ cd ~/pushy
$ vim pushy.conf
{% endhighlight %}

> __Note:__
It's important to name the config `pushy.conf` as that's what docker will be expecting.

Feel free to configure APNS and/or GCM in the way you want. Just remember that the files  
should always be relative to pushy's `volume` in docker: `/pushy`.

> __Note:__
On MacOS X docker runs thru `boot2docker`, so redis' host is actually `boot2docker`'s ip, not localhost.

Here is what my configuration ended up looking like:

{% highlight ini %}
[apns]
p12=/pushy/sandbox.p12
password=123456
logfile=/pushy/apns.log

[gcm]
project=123456789
key=theunATueu3434-S8-NTHkteuhnk34nTH
logfile=/pushy/gcm.log

[redis]
host=192.168.59.103 # get yours with `boot2docker ip`
{% endhighlight %}

##The fun part##

Now lets get into business. It's time to run `redis` and `pushy` using docker.  

Again, MacOS X users will have to copy the configuration file into `boot2docker` first:

{% highlight bash %}
$ cd ~/pushy
$ ssh-add ~/.ssh/id_boot2docker # in case you didn't add it before
$ scp pushy.conf docker@$(boot2docker ip):/pushy
$ scp sandbox.p12 docker@$(boot2docker ip):/pushy # if you configured APNS
{% endhighlight %}

> __Note:__
Linux users don't need to do that.

And now we are pretty much ready to launch `redis` and `pushy`:

{% highlight bash %}
docker run -d --name redis -p 6379:6379 dockerfile/redis
docker run -d --name pushy -v /full/path:/pushy -p 7446:7446 godexsoft/pushy
{% endhighlight %}

> __Note:__
The above `/full/path` should be replaced with `/pushy` on MacOS X or with
`/home/$USER/pushy` on Linux if you followed the tutorial completely as it is.
