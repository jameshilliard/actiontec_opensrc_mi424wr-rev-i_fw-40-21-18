# Create pkg/build/include subdir. We need this to enable the include/linux 
# and include/asm to be differnt for each tree even though all use 
# /usr/local/openrg/<arch>/include subdir.
# Glibc support is only for i386 for ensure/mpatrol needs.
create_includes:
	# Preparing basic include files in include dir
	$(MKDIR) $(RG_LIB)
ifdef CONFIG_GLIBC
	$(MKDIR) $(RG_BUILD)/glibc/include
    ifeq ($(GLIBC_IN_TOOLCHAIN),y)
	$(RG_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_SO)/* $(RG_LIB)
	$(RG_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_CRT)/* $(RG_LIB)
	$(RG_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_INC)/* $(RG_BUILD)/glibc/include
    endif
endif
ifdef CONFIG_ULIBC
	$(MKDIR) $(RG_BUILD)/ulibc/include
	ls -lF $(RG_BUILD)/ulibc/include
    ifeq ($(ULIBC_IN_TOOLCHAIN),y)
	$(RG_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_SO)/* $(RG_LIB)
	$(RG_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_CRT)/* $(RG_LIB)
	$(RG_CP_LN) $(LIBC_IN_TOOLCHAIN_PATH_INC)/* $(RG_BUILD)/ulibc/include
    else
        # For non-toolchain ulibc libs are linked to $(RG_LIB) in pkg/ulibc/Makefile
	cp -f -R --symbolic-link $(BUILDDIR)/pkg/ulibc/include/* $(RG_BUILD)/ulibc/include
    endif
endif
	# We want to use OpenRG linux kernel includes instead of default libc
	$(RM) -rf $(RG_BUILD)/[ug]libc/include/linux \
	  $(RG_BUILD)/[ug]libc/include/asm \
	  $(RG_BUILD)/[ug]libc/include/asm-generic \
	  $(RG_BUILD)/[ug]libc/include/scsi \
	  $(RG_BUILD)/[ug]libc/include/mtd

create_includes_os_%:
	# Linking linux headers to include dir
	$(MKDIR) $(RG_BUILD)/$*/include
ifdef CONFIG_RG_OS_LINUX
ifdef CONFIG_RG_OS_LINUX_26
ifndef CONFIG_RG_OS_LINUX_24
	$(RG_CP_LN) -L $(RGSRC)/os/linux-2.6/include/linux $(RG_BUILD)/$*/include
	$(RG_CP_LN) -L $(BUILDDIR)/os/linux-2.6/include/linux $(RG_BUILD)/$*/include
	$(MKDIR) $(RG_BUILD)/$*/include/asm
	-$(RG_CP_LN) -L $(BUILDDIR)/os/linux-2.6/include/asm-$(LIBC_ARCH)/* $(RG_BUILD)/$*/include/asm
	$(RG_CP_LN) -L $(RGSRC)/os/linux-2.6/include/asm-$(LIBC_ARCH)/* $(RG_BUILD)/$*/include/asm
	$(RG_CP_LN) -L $(RGSRC)/os/linux-2.6/include/asm-generic $(RG_BUILD)/$*/include
	$(RG_CP_LN) -L $(RGSRC)/os/linux-2.6/include/scsi $(RG_BUILD)/$*/include
	$(RG_CP_LN) -L $(RGSRC)/os/linux-2.6/include/mtd $(RG_BUILD)/$*/include
	$(RG_CP_LN) -L $(RGSRC)/os/linux-2.6/include/net/bluetooth $(RG_BUILD)/$*/net
endif
endif
ifdef CONFIG_RG_OS_LINUX_24
	@$(RG_LN) $(BUILDDIR)/os/linux-2.4/include/linux $(RG_BUILD)/$*/include/linux
	@$(RG_LN) \
	  $(BUILDDIR)/os/linux-2.4/include/asm$(if $(CONFIG_RG_OS_LINUX_24),$(if $(CONFIG_RG_UML),-i386,)) $(RG_BUILD)/$*/include/asm
	@$(RG_LN) $(BUILDDIR)/os/linux-2.4/include/scsi $(RG_BUILD)/$*/include/scsi
	@$(RG_LN) $(BUILDDIR)/os/linux-2.4/include/net/bluetooth $(RG_BUILD)/$*/include/net
endif
endif

