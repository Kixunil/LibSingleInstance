LibSingleInstance
=================

About
-----

The aim of this library is to simplify programming applications which should have single instance per user. The problem appears easy at the first glance but it's much more complicated if you want your application to be robust and portable. Traditional pid files are vulnerable to application crashes and power failures. This library has no problem with them. Also, this library aims to be platform-independent, while currently it has only POSIX implementation.

Building
--------

To build the library you just need to run make. (This may change in future versions.) To build the documentation run make doc.

Usage
-----

To use the library you just need to #include <singleinstance.h>. Read the documentation and have a look at test source files to see how instance can be created and how arguments from other instances can be received. The algorithm can be generally briefly described like this:

```
handle = singleInstanceCreate(...);
if(handle) {
	// Do stuff while periodically calling singleInstanceCheck() and singleInstancePop()
	singleInstanceDestroy(handle);
} else {
	exit_application();
}
```

In case you are programming in C++, use of SingleInstance class is strongly encouraged.

In multithreaded application, be careful not to call different calls at once. The only call which is guaranteed to be safe running concurrently with any call except singleInstanceDestroy() is singleInstanceStopWait().

Notes for POSIX systems
-----------------------

The library uses directory ~/.appName to store lock file and communication fifo. Be careful not to conflict with them. You may place another files related to your application inside ~/.appName. It is advised to use the ~/.appName directory after call to singleInstanceCreate() (or SingleInstance() constructor), since it will create the directory if it doesn't exist.

The permissions are set so only the owner of lock file and fifo has access to them. The directory has mode 0755. You may restrict permissions using umask.

It is safe to call singleInstanceStopWait() from signal handler. Calling any other function from signal handler may lead to undefined behavior - avoid doing it!

Authors
-------

Martin Habovštiak <martin.habovstiak@gmail.com>

Released under the terms of MIT License.
