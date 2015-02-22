RGSRC=../../..
include $(RGSRC)/envir.mak
include $(RGSRC)/pkg/voip/asterisk/common.mak

RAMDISK_ETC_FILES+=$(addprefix asterisk/, adsi.conf adtranvofr.conf \
  agents.conf alarmreceiver.conf alsa.conf asterisk.adsi asterisk.conf \
  cdr_manager.conf cdr_odbc.conf cdr_pgsql.conf cdr_tds.conf enum.conf \
  extconfig.conf extensions.conf features.conf festival.conf h323.conf \
  iax.conf iaxprov.conf indications.conf logger.conf manager.conf meetme.conf \
  mgcp.conf modem.conf modules.conf musiconhold.conf osp.conf oss.conf \
  phone.conf privacy.conf queues.conf res_config_odbc.conf res_odbc.conf \
  rpt.conf rtp.conf sip.conf skinny.conf telcordia-1.adsi voicemail.conf \
  vpb.conf zapata.conf)

include $(RGMK)
