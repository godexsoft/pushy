---
isTutorial: true
layout: page
title: Configuring APNS
permalink: /configuring-apns/
---
In this tutorial I will guide you through the steps required to setup
an APNS provider in __pushy__ server.

##Verify your copy of pushy works##

First of all lets see if your pushy installation works as expected.  
Navigate to the directory where __pushy__'s binary is placed and execute:

{% highlight bash %}
$ ./pushy -h
{% endhighlight %}

Your output should be similar to the following:

>
![pushy help message](/images/pushy_apns_tutorial/pushy_help_message.png)

##Configure APNS for your iOS app##

Navigate to apple's developer portal, select your application and verify that
you can configure APNS for development or production. In this tutorial I will be
configuring APNS for production mode:

>
![apns configurable for app](/images/pushy_apns_tutorial/dev_portal_push_configurable.png)

Now when you click on the `Edit` button you will be able to configure APNS for this app.  
Just tap the `Create certificate...` button next to the configuration you need.
For this tutorial it is the `Production SSL Certificate` configuration:

>
![apns configurable in production](/images/pushy_apns_tutorial/dev_portal_push_configurable_prod.png)

When you click it, Apple will show you a little _howto_ on creating certificate requests.  
Feel free to skip it as I will be walking you through that step anyway:

>
![howto create a certificate request](/images/pushy_apns_tutorial/dev_portal_howto_create_request.png)

##Create a certificate request##

Now it's time to launch __Keychain Access__ and select the
`Certificate Assistant_` -> `Request a Certificate From a Certificate Authority` option:

>
![use keychain access](/images/pushy_apns_tutorial/keychain_create_cert_req.png)

Fill in the blanks. Make sure to select the `Saved to disk` option. And hit `continue`.  
Here is an example of what it should look like:

>
![sample request data](/images/pushy_apns_tutorial/keychain_sample_request.png)

Now you will be prompted to save the file somewhere on your disk. It's recommended
to create a directory (in my case I created a directory called _pushy_ on the Desktop)
to store all the files for this particular APNS installation.  
Anyhow, save the request and have it handy as we are going to jump back into the developer
portal very soon:

>
![save request file](/images/pushy_apns_tutorial/keychain_save_request.png)

##Back to Apple's developer portal##

Now we are back to where we left the developer portal. It's time to upload our __certificate
request__ and generate our new __push certificate__. Hit `Choose file...`, navigate
to the certificate request we saved a few seconds ago and select it:

>
![upload and generate certificate](/images/pushy_apns_tutorial/dev_portal_upload_and_generate.png)

Hit **Generate** when you are ready.  
It will take some time for Apple to generate the certificate so sit back tight:

>
![ready to generate certificate](/images/pushy_apns_tutorial/dev_portal_ready_to_generate.png)

Your certificate is now generated and ready for download.  
Hit the `Download` button:

>
![certificate is ready](/images/pushy_apns_tutorial/dev_portal_cert_ready.png)

The certificate should now be downloaded to your _Downloads_ folder.  
Check if it's there:

>
![downloaded certificate](/images/pushy_apns_tutorial/downloads_cert_file.png)

Double click the certificate file and the following dialog should appear
when **Keychain Access** automatically launches. Hit `Add` to import the APNS certificate:

>
![import into keychain](/images/pushy_apns_tutorial/keychain_import_from_downloaded_file.png)

##Generating the final .p12 file with Keychain Access##

We are almost there. Now we have to go back to **Keychain Access**, select the **Certificates** panel
on the left side and find the certificate we just imported a few moments ago.  
In my case the certificate is called `Apple Production IOS Push Services: co.sevenbit.pushy`:

>
![find your certificate](/images/pushy_apns_tutorial/keychain_find_certificate.png)

Right-click on the certificate entry and select the `Export "..."` option:

>
![export p12](/images/pushy_apns_tutorial/keychain_export_p12.png)

Save the file somewhere. It's recommended to save it into the same directory you
previously saved the _certificate request_ file to. Make sure you select the `p12` format
and hit `Save`:

>
![export p12](/images/pushy_apns_tutorial/keychain_export_save.png)

**Keychain Access** will now ask you for an export password. In this tutorial I use
`123456` as the password. Hit `OK` when ready.  
This is how it will look:

>
![create password for p12](/images/pushy_apns_tutorial/keychain_p12_create_password.png)

**Keychain Access** will ask you one more password. This time it's your current user's
password which **Keychain Access** needs in order to read the item you are exporting.  
Enter your password and hit `Allow`:

>
![enter your password](/images/pushy_apns_tutorial/keychain_enter_your_password.png)

##Finally we get to use our trusty terminal##

For this tutorial I copied the resulting `production.p12` file closer to my pushy binary
like so:

{% highlight bash %}
$ pwd
/Users/godexsoft/bin/pushy
$ cp ~/Desktop/pushy/production.p12 /Users/godexsoft/bin/pushy
$ ls
production.p12 pushy
{% endhighlight %}

Now we can launch pushy with APNS enabled! But wait. One last thing to launch is
the **redis** server.  

> Note:
In this tutorial I assume you already have **redis** installed.  

Open a new terminal window and run:

{% highlight bash %}
$ redis-server
{% endhighlight %}

That should do it. By default **pushy** will try to connect to the redis server running on _localhost_.

Finally, launch pushy with APNS support like so:

{% highlight bash %}
$ ./pushy --apns.p12 production.p12 --apns.password 123456
{% endhighlight %}

If you followed this tutorial correctly, you should now see something like this:

>
![running pushy with apns](/images/pushy_apns_tutorial/pushy_run_with_apns.png)
