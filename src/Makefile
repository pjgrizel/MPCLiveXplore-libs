

# Compile tkgl_mpcmapper
build:
	CC=/root/x-tools/armv7-unknown-linux-gnueabihf/bin/armv7-unknown-linux-gnueabihf-cc
	$(CC) -Wall -L /root/x-tools/armv7-unknown-linux-gnueabihf/lib ./tmm-plugins/iamforce-4live.c ./tmm-plugins/iamforce-4live-callbacks.c -o ./tmm-plugins/iamforce-4live.so -I . -I /root/alsa-lib/include/ -shared -fPIC -ldl -lasound
	
install: build
	scp ./iamforce4live.so root@192.168.1.85:/media/Akai/tkgl_bootstrap_[ProjectData]/lib/

