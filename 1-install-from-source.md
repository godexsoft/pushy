---
isTutorial: true
layout: page
title: Installing from source
permalink: /installing-from-source/
---
In this tutorial I will walk you through the process of installation of __pushy__.  
This time we will install the latest version directly from __github__.

##Cloning the repository##

First we need to `clone` the __git__ repository. Open a __terminal__ and issue
the following command:

{% highlight bash %}
$ cd /tmp
$ git clone https://github.com/godexsoft/pushy.git pushy
{% endhighlight %}

The result should `clone` the repository into the _pushy_ directory:

>
![clone the repo]({{ site.baseurl }}/images/pushy_installation_tutorial/pushy_clone.png)

##Configuring##

Navigate to the newly created _pushy_ directory and execute the `configure` script:

{% highlight bash %}
$ cd pushy
$ pwd
/tmp/pushy
$ ./configure
{% endhighlight %}

The output should be similar to this:

>
![configure pushy]({{ site.baseurl }}/images/pushy_installation_tutorial/pushy_configure.png)

##Troubleshooting##

Now, if all went right and you happened to have the dependencies installed, it's time to compile!  
Feel free to skip this section and jump directly into the compiling section below.

If it didn't work at the configuration stage it means that you lack some libraries
or a C++11-compatible compiler. Make sure you have the following preinstalled on your system:

- cmake (2.8 or newer)
- [boost](http://boost.org) (1.53 or newer). required libraries from boost:
  1. system
  2. log
  3. thread
  4. program_options
  5. date_time
- [hiredis](https://github.com/redis/hiredis) (redis client library for C/C++)

> Note:
The above libraries are expected to be installed system-wide. 

##Compiling##

All we need to do now is just use `make` to compile and build __pushy__:

{% highlight bash %}
$ make
{% endhighlight %}

My output looked like this:

>
![make pushy]({{ site.baseurl }}/images/pushy_installation_tutorial/pushy_make.png)

And that's it. Now you should have your very own copy of __pushy__.

##Checking if it works##

Lets see if your copy of pushy works as expected. The binary should be in the
same directory where we have been compiling it.  
Try the following:

{% highlight bash %}
$ ./pushy -h
{% endhighlight %}

Your output should be similar to the following:

>
![pushy help message]({{ site.baseurl }}/images/pushy_apns_tutorial/pushy_help_message.png)

You are now ready to proceed with the [APNS configuration tutorial]({{ site.baseurl }}/configuring-apns/) :-)
