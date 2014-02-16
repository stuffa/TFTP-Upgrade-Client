TFTP-Upgrade-Client
===================

TFTP Upgrade client: is a Win32 and linux TFTP client especially designed for installing open firmware of home based routers.
It expressly address the issues that are found by users trying to install the firmware via TFTP using the Microsoft TFTP client.

The source will comple for both linux as well as windows command line.
to comple for:

* linux use:   make upgrade-linux
* windows use: make upgrade-win32
* both use:    make all   

for windows you will need to install mingwmsvc,  for linux use gcc


### Usage

    usage: upgrade [options] <destination> <file_name>
    
    <destination>  : FQDN or IP address of destination
    <file_name>    : name of the file to send
    -h             : display this message
    -v             : display the version
    -p port_number : use a different port.  Default = UDP port 69
    -a <mac_addr>  : aggressive: specify the mac address to get a faster connection
                     (not fully implimemted yet)
    
    All transfers are binary
    Retries 2 times every second
    Timeout after 30 seconds, if not coneected

1. It will attempt make a TFTP connection every 500ms (2 times a second) for 30 seconds, giving you ample time to reboot the router and ensure that you are able to hit the routers TFTP server within the boot_wait period
1. If a TFTP server is already running running it will immediately connect -  if it finds that there is no response - it will display a message after 4 attempts indicating that you need to reboot your router to start the TFTP server.
1. Displays "Sending:........." to let you know that a transfer is under-way
1. Display a message to let you know if the transfer completed OK (or not)