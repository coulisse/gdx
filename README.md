GTK 3


If you are trying to compile a gtk-3 application and if you don’t have development package installed here is the error you can get:

    configure: error: Package requirements (gtk+-3.0 >= 3.0.0) were not met:

    No package ‘gtk+-3.0’ found

    Consider adjusting the PKG_CONFIG_PATH environment variable if you
    installed software in a non-standard prefix.

    Alternatively, you may set the environment variables GTK_CFLAGS
    and GTK_LIBS to avoid the need to call pkg-config.
    See the pkg-config man page for more details. 

If you encounter this error in Fedora, you should install gtk3-devel. In terminal, type in the following:

su -c 'yum install gtk3-devel'

If you encounter this in Ubuntu, you should install libgtk-3-dev. To do so, type in the following in terminal:

sudo apt-get install libgtk-3-dev


