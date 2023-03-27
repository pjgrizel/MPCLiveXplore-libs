

# Compile tkgl_mpcmapper
build:
	CC=/root/x-tools/armv7-unknown-linux-gnueabihf/bin/armv7-unknown-linux-gnueabihf-cc
	$(CC) -Wall -L /root/x-tools/armv7-unknown-linux-gnueabihf/lib ./tkgl_mpcmapper.c ./iamforce.c -o iamforce2.so -I /root/alsa-lib/include/ -shared -fPIC -ldl -lasound
	
install: build
	scp ./iamforce2.so root@192.168.1.85:/media/Akai/tkgl_bootstrap_[ProjectData]/lib/

