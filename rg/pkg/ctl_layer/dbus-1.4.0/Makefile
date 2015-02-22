DBUS_DIRS = dbus bus tools test

att:
	../tr69fw/config_att.sh
	make -C dbus
	make -C bus
	make -C tools
	make -C test
	./install.sh att

bcm:
	../tr69fw/config_bcm.sh
	make -C dbus
	make -C bus
	make -C tools
	make -C test
	./install.sh bcm

bcm_telus:
	../tr69fw/config_bcm.sh telus
	make -C dbus
	make -C bus
	make -C tools
	make -C test
	./install.sh bcm telus

bhr2:
	../tr69fw/config_bhr2.sh
	make -C dbus
	make -C bus
	make -C tools
	make -C test
	./install.sh bhr2

bhr2_refi:
	../tr69fw/config_bhr2.sh bhr2_refi
	make -C dbus
	make -C bus
	make -C tools
	make -C test
	./install.sh bhr2_refi

x86:
	../tr69fw/config_x86.sh
	make -C dbus
	make -C bus
	make -C tools
	make -C test
	./install.sh x86
	
clean:
	$(foreach dir, $(DBUS_DIRS), \
	if [ -e $(dir) ]; then make -C $(dir) clean; fi;\
	if [ -L $(dir) ]; then rm -f $(dir); fi; )
          
