TFTP-Upgrade-Client
===================

TFTP Upgrade client for for firmware up of network devices

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
