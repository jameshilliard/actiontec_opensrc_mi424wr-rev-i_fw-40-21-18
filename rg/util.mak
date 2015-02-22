# Macro to calulate size of a file $1 according to $2: decimal/hex
# XXX: for now it returns -1 (for dec) or 0xffff (for hex) when the file
# doesn't exist. DON'T change it to 0 - it will break things (size 0 of
# variables causes object to be created with a different size).
MSIZE=$(if $(call EQ,$2,dec),,0x)$(shell if [ -f $1 ] ; then size --target=binary $1 | grep -v text | awk '{print $(if $(call EQ,$2,dec),$$4,$$5)}' ; else echo $(if $(call EQ,$2,dec),-1,ffff); fi)

# Pad $1 to be of size rounded to 4 bytes
PADD=let org_size=$(call MSIZE,$1); let pad_size=$$((org_size % 4)); if [ $$pad_size -gt 0 ]  ; then let pad_size=$$(( 4 - $$pad_size )); dd if=/dev/zero bs=1 count=$$pad_size >> $2; fi

