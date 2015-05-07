Compile tools from:
https://www.kernel.org/pub/linux/utils/cpu/msr-tools/
http://linux.koolsolutions.com/2009/09/19/howto-using-cpu-msr-tools-rdmsrwrmsr-in-debian-linux/

Edit:
http://luisjdominguezp.tumblr.com/post/19610447111/disabling-turbo-boost-in-linux


To know if the Turbo Boost feature is disabled, run:

rdmsr -pi 0x1a0 -f 38:38

1=disabled
0=enabled
