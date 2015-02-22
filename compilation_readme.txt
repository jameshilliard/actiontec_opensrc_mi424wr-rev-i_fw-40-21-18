Distributions (DIST):
=====================
    MC524WR
    MI424WR
    FEROCEON

Target Customers (ACTION_TEC_CUSTOMER):
======================================
    ACTION_TEC_VERIZON=y
    ACTION_TEC_NCS=y

Hardware Revs (CONFIG_HW_REV):
==============
    CONFIG_BHR_REV_E
    CONFIG_BHR_REV_F
    CONFIG_BHR_REV_G
    CONFIG_BHR_REV_I

Big vs Small Image:
===================
    Big Image: Nothing to be specified on command line
    Small Image: ACTION_TEC_SMALL_IMG=y

License File:
=============
    LIC=../jpkg_ixp425.lic
    LIC=../jpkg_actiontec_oct.lic
    LIC=../jpkg_actiontec_mv.lic


OpenRG Compilation
------------------
make DIST=<distribution> <ACTION_TEC_CUSTOMER=y> <CONFIG_BHR_REV_X=y> [<ACTION_TEC_SMALL_IMG=y>] LIC=../<license filename> && make


OpenRG for BHR Rev-A, C and D Compilation
------------------------------------------
make config DIST=MI424WR LIC=../jpkg_ixp425.lic && make


OpenRG for BHR Rev-E and Rev-F Compilation
-------------------------------------------
make DIST=MC524WR <ACTION_TEC_CUSTOMER=y> CONFIG_BHR_REV_E=y CONFIG_BHR_REV_F=y [<ACTION_TEC_SMALL_IMG=y>] LIC=../jpkg_actiontec_oct.lic && make


OpenRG for BHR Rev-G Compilation
--------------------------------
make DIST=MC524WR <ACTION_TEC_CUSTOMER=y> CONFIG_BHR_REV_G=y [<ACTION_TEC_SMALL_IMG=y>] LIC=../jpkg_actiontec_oct.lic && make


OpenRG for BHR Rev-I Compilation
--------------------------------
make DIST=FEROCEON <ACTION_TEC_CUSTOMER=y> CONFIG_BHR_REV_I=y LIC=../jpkg_actiontec_mv.lic && make


OpenRG for GPL distribution
---------------------------
make config DIST=<dist> CONFIG_RG_GPL=y LIC=../<license filename> && make

