/*
 * SPCA5xx based usb camera driver (currently supports
 * yuv native stream spca501a, spca501c, spca505, spca508, spca506
 * jpeg native stream spca500, spca551, spca504a, spca504b, spca533a, spca536a, zc0301, zc0302, cx11646, sn9c102p
 * bayer native stream spca561a, sn9c101, sn9c102, tv8532 ).
 * Z-star Vimicro chips zc0301 zc0301P zc0302
 * Sunplus spca501a, spca501c, spca505, spca508, spca506, spca500, spca551, spca504a, spca504b, spca533a, spca536a
 * Sonix sn9c101, sn9c102, sn9c102p
 * Conexant cx11646
 * Transvision tv_8532 
 * Etoms Et61x151 Et61x251
 * SPCA5xx version by Michel Xhaard <mxhaard@users.sourceforge.net>
 * Based on :
 * SPCA50x version by Joel Crisp <cydergoth@users.sourceforge.net>
 * OmniVision OV511 Camera-to-USB Bridge Driver
 * Copyright (c) 1999-2000 Mark W. McClelland
 * Kernel 2.6.x port Michel Xhaard && Reza Jelveh (feb 2004)
 * Based on the Linux CPiA driver written by Peter Pregler,
 * Scott J. Bertin and Johannes Erdfelt.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

static const char version[] = SPCA5XX_VERSION;


#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>


#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/pagemap.h>
#include <usb.h>
#include <asm/io.h>
#include <asm/semaphore.h>

#include <asm/page.h>
#include <asm/uaccess.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#include <linux/wrapper.h>
#endif
#include <linux/param.h>
#ifdef SPCA50X_ENABLE_OSD
#ifdef CONFIG_FB
#include <video/font.h>
#endif /* CONFIG_FB */
#endif /* SPCA50X_ENABLE_OSD */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
#include <linux/moduleparam.h>
#endif

#include "spca5xx.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#  undef CONFIG_VIDEO_PROC_FS
#	 undef CONFIG_PROC_FS
#endif

//#define RH9_REMAP 1

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#include "spcaCompat.h"
#endif


#include "spcadecoder.h"
#include "jpeg_qtables.h"

#define PROC_NAME_LEN 10	//length of the proc name

#ifndef CONFIG_FB
/* Force OSD off if required dependencies not enabled */
/* Turning OSD on requires a kernel patch to the fbcon__ routines */
#undef SPCA50X_ENABLE_OSD
#endif /* CONFIG_FB */

/* Video Size 640 x 480 x 4 bytes for RGB */
#define MAX_FRAME_SIZE (640 * 480 * 4)
#define MAX_DATA_SIZE (MAX_FRAME_SIZE + sizeof(struct timeval))

#ifdef SPCA50X_ENABLE_OSD
struct fbcon_font_desc *osd_font = NULL;
#endif /* SPCA50X_ENABLE_OSD */

/* PARAMETER VARIABLES: */
/* autoadjust = 1 schedule a BH and break the jpeg cam with tasklet */
static int autoadjust = 0;	/* CCD dynamically changes exposure, etc... */
/* Hardware auto exposure / whiteness (PC-CAM 600) */
static int autoexpo = 1;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,5)
/* Video device number (-1 is first available) */
static int video_nr = -1;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,5) */

/* 0=no debug messages
 * 1=init/detection/unload and other significant messages,
 * 2=some warning messages
 * 3=config/control function calls
 * 4=most function calls and data parsing messages
 * 5=highly repetitive mesgs
 * NOTE: This should be changed to 0, 1, or 2 for production kernels
 */
static int debug = 0;

/* Snapshot mode enabled flag */
static int snapshot = 0;

/* Force image to be read in RGB instead of BGR. This option allow
 * programs that expect RGB data (e.g. gqcam) to work with this driver. */
static int force_rgb = 0;
static int gamma = 3;
static int OffRed = 0;
static int OffBlue = 0;
static int OffGreen = 0;
static int GRed = 256;
static int GBlue = 256;
static int GGreen = 256;


#ifdef SPCA50X_ENABLE_CAMS
/* Number of cameras to stream from simultaneously */
static int cams = 1;
#endif /* SPCA50X_ENABLE_CAMS */
static int usbgrabber = 0;


#ifdef SPCA50X_ENABLE_COMPRESSION
/* Enable compression. This is for experimentation only; compressed images
 * still cannot be decoded yet. */
static int compress = 0;
#endif /* SPCA50X_ENABLE_COMPRESSION */

#ifdef SPCA50X_ENABLE_TESTPATTERN
/* Display test pattern - doesn't work yet either */
static int testpat = 0;
#endif /* SPCA50X_ENABLE_TESTPATTERN */


/* Initial brightness & contrast (for debug purposes) */
static int bright = 0x80;
static int contrast = 0x60;

/* Internal/external CCD flag, initially internal CCD */
static int ccd = 0;

/* Parameter that enables you to set the minimal suitable bpp */
static int min_bpp = 0;

/* Parameter defines the average luminance that should be kept */
static int lum_level = 0x2d;

#ifdef SPCA50X_ENABLE_OSD
/* Enable On-screen display of some data (frame sequence, time) */
static int osd = 1;
#endif /* SPCA50X_ENABLE_OSD */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
module_param (autoadjust, int, 0644);
module_param (autoexpo, int, 0644);
module_param (debug, int, 0644);
module_param (snapshot, int, 0644);

module_param (force_rgb, int, 0644);
module_param (gamma, int, 0644);
module_param (OffRed, int, 0644);
module_param (OffBlue, int, 0644);
module_param (OffGreen, int, 0644);
module_param (GRed, int, 0644);
module_param (GBlue, int, 0644);
module_param (GGreen, int, 0644);
#ifdef SPCA50X_ENABLE_CAMS
module_param (cams, int, 0444);
#endif /* SPCA50X_ENABLE_CAMS */
#ifdef SPCA50X_ENABLE_COMPRESSION
module_param (compress, int, 0444);
#endif /* SPCA50X_ENABLE_COMPRESSION */
module_param (bright, int, 0444);
module_param (contrast, int, 0444);
module_param (ccd, int, 0444);
#ifdef SPCA50X_ENABLE_OSD
module_param (osd, int, 0444);
#endif /* SPCA50X_ENABLE_OSD */
module_param (min_bpp, int, 0444);
module_param (lum_level, int, 0444);
module_param (usbgrabber, int, 0444);

#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9) */

MODULE_PARM (autoadjust, "i");
MODULE_PARM (autoexpo, "i");
MODULE_PARM (debug, "i");
MODULE_PARM (snapshot, "i");
MODULE_PARM (force_rgb, "i");
MODULE_PARM (gamma, "i");
MODULE_PARM (OffRed, "i");
MODULE_PARM (OffBlue, "i");
MODULE_PARM (OffGreen, "i");
MODULE_PARM (GRed, "i");
MODULE_PARM (GBlue, "i");
MODULE_PARM (GGreen, "i");
#ifdef SPCA50X_ENABLE_CAMS
MODULE_PARM (cams, "i");
#endif /* SPCA50X_ENABLE_CAMS */
#ifdef SPCA50X_ENABLE_COMPRESSION
MODULE_PARM (compress, "i");
#endif /* SPCA50X_ENABLE_COMPRESSION */
MODULE_PARM (bright, "i");
MODULE_PARM (contrast, "i");
MODULE_PARM (ccd, "i");
#ifdef SPCA50X_ENABLE_OSD
MODULE_PARM (osd, "i");
#endif /* SPCA50X_ENABLE_OSD */
MODULE_PARM (min_bpp, "i");
MODULE_PARM (lum_level, "0-255i");
MODULE_PARM (usbgrabber, "i");
#endif
/***************/

MODULE_PARM_DESC (autoadjust,
		  "CCD dynamically changes exposure (spca501x only !! )");
MODULE_PARM_DESC (autoexpo,
		  "Enable/Disable hardware auto exposure / whiteness (default: enabled) (PC-CAM 600 only !!)");
MODULE_PARM_DESC (debug,
		  "Debug level: 0=none, 1=init/detection, 2=warning, 3=config/control, 4=function call, 5=max");
MODULE_PARM_DESC (snapshot, "Enable snapshot mode");
MODULE_PARM_DESC (force_rgb, "Read RGB instead of BGR");
MODULE_PARM_DESC (gamma, "gamma setting range 0 to 7 3-> gamma=1");
MODULE_PARM_DESC (OffRed, "OffRed setting range -128 to 128");
MODULE_PARM_DESC (OffBlue, "OffBlue setting range -128 to 128");
MODULE_PARM_DESC (OffGreen, "OffGreen setting range -128 to 128");
MODULE_PARM_DESC (GRed, "Gain Red setting range 0 to 512 /256 ");
MODULE_PARM_DESC (GBlue, "Gain Blue setting range 0 to 512 /256 ");
MODULE_PARM_DESC (GGreen, "Gain Green setting range 0 to 512 /256 ");
#ifdef SPCA50X_ENABLE_CAMS
MODULE_PARM_DESC (cams,
		  "Number of simultaneous cameras (not in used 3 works with good usb condition )");
#endif /* SPCA50X_ENABLE_CAMS */
#ifdef SPCA50X_ENABLE_COMPRESSION
MODULE_PARM_DESC (compress, "Turn on/off compression (not functional yet)");
#endif /* SPCA50X_ENABLE_COMPRESSION */
MODULE_PARM_DESC (bright, "Initial brightness factor (0-255) not know by all webcams !!");
MODULE_PARM_DESC (contrast, "Initial contrast factor (0-255) not know by all webcams !!");
MODULE_PARM_DESC (ccd,
		  "If zero, default to the internal CCD, otherwise use the external video input");
#ifdef SPCA50X_ENABLE_OSD
MODULE_PARM_DESC (osd,
		  "If non-zero, enable various types of on-screen display");
#endif /* SPCA50X_ENABLE_OSD */
MODULE_PARM_DESC (min_bpp,
		  "The minimal color depth that may be set (default 0)");
MODULE_PARM_DESC (lum_level,
		  "Luminance level for brightness autoadjustment (default 32)");
MODULE_PARM_DESC (usbgrabber,
		  "Is a usb grabber 0x0733:0x0430 ? (default 1) ");
/****************/
MODULE_AUTHOR
  ("Michel Xhaard <mxhaard@users.sourceforge.net> based on spca50x driver by Joel Crisp <cydergoth@users.sourceforge.net>,ov511 driver by Mark McClelland <mwm@i.am>");
MODULE_DESCRIPTION ("SPCA5XX USB Camera Driver");
MODULE_LICENSE ("GPL");



static int spca50x_move_data (struct usb_spca50x *spca50x, struct urb *urb);
static void auto_bh (void *data);


static struct usb_driver spca5xx_driver;

#ifndef max
static inline int
max (int a, int b)
{
  return (a > b) ? a : b;
}
#endif /* max */

/**********************************************************************
 * List of known SPCA50X-based cameras
 **********************************************************************/

/* Camera type jpeg yuvy yyuv yuyv grey gbrg*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static struct palette_list Plist[] = {
  {JPEG, "JPEG"},
  {JPGH, "JPEG"},
  {JPGC, "JPEG"},
  {JPGS, "JPEG"},
  {JPGM, "JPEG"},
  {YUVY, "YUVY"},
  {YYUV, "YYUV"},
  {YUYV, "YUYV"},
  {GREY, "GREY"},
  {GBRG, "GBRG"},
  {SN9C, "SN9C"},
  {GBGR, "GBGR"},
  {-1, NULL}
};
#endif

static struct bridge_list Blist[] = {

  {BRIDGE_SPCA505, "SPCA505"},
  {BRIDGE_SPCA506, "SPCA506"},
  {BRIDGE_SPCA501, "SPCA501"},
  {BRIDGE_SPCA508, "SPCA508"},
  {BRIDGE_SPCA504, "SPCA504"},
  {BRIDGE_SPCA500, "SPCA500"},
  {BRIDGE_SPCA504B, "SPCA504B"},
  {BRIDGE_SPCA533, "SPCA533"},
  {BRIDGE_SPCA504_PCCAM600, "SPCA504C"},
  {BRIDGE_SPCA561, "SPCA561"},
  {BRIDGE_SPCA536, "SPCA536"},
  {BRIDGE_SONIX, "SN9C102"},
  {BRIDGE_ZC3XX, "ZC301-2"},
  {BRIDGE_CX11646, "CX11646"},
  {BRIDGE_TV8532, "TV8532"},
  {BRIDGE_ETOMS,"ET61XX51"},
  {BRIDGE_SN9C102P,"SN9C102P"},
  {BRIDGE_MR97311,"MR97311"},
  {-1, NULL}
};

enum
{
  UnknownCamera = 0,		// 0
  IntelPCCameraPro,
  IntelCreateAndShare,
  GrandtecVcap,
  ViewQuestM318B,
  ViewQuestVQ110,
  KodakDVC325,
  MustekGsmartMini2,
  MustekGsmartMini3,
  CreativePCCam300,
  DLinkDSC350,			// 10
  CreativePCCam600,
  IntelPocketPCCamera,
  IntelEasyPCCamera,
  ThreeComHomeConnectLite,
  KodakEZ200,
  MaxellMaxPocket,
  AiptekMiniPenCam2,
  AiptekPocketDVII,
  AiptekPenCamSD,
  AiptekMiniPenCam13,		// 20
  MustekGsmartLCD3,
  MustekMDC5500Z,
  MegapixV4,
  AiptekPocketDV,
  HamaUSBSightcam,
  Arowana300KCMOSCamera,
  MystFromOriUnknownCamera,
  AiptekPocketDV3100,
  AiptekPocketCam3M,
  GeniusVideoCAMExpressV2,	// 30
  Flexcam100Camera,
  MustekGsmartLCD2,
  PureDigitalDakota,
  PetCam,
  BenqDC1500,
  LogitechClickSmart420,
  LogitechClickSmart510,
  BenqDC1300,
  HamaUSBSightcam2,
  MustekDV3000,			// 40
  CreativePccam750,
  MaxellCompactPM3,
  BenqDC3410,
  BenqDC1016,
  MicroInnovationIC200,
  LogitechTraveler,
  Flycam100Camera,
  UsbGrabberPV321c,
  ADSInstantVCD,
  Gsmartmini,			// 50
  Jenoptikjdc21lcd,
  LogitechClickSmart310,
  Terratec2move13,
  MustekDV4000,
  AiptekDV3500,
  LogitechClickSmart820,
  Enigma13,
  Sonix6025,
  Epsilon13,
  Nxultra,			//60
  AiptekPocketCam2M,
  DeMonUSBCapture,
  CreativeVista,
  PolaroidPDC2030,
  CreativeNotebook,
  CreativeMobile,
  LabtecPro,
  MustekWcam300A,
  GeniusVideoCamV2,
  GeniusVideoCamV3,
  GeniusVideoCamExpressV2b,
  CreativeNxPro,
  Sonix6029,			//73 74 75
  Vimicro,
  Digitrex2110,
  GsmartD30,
  CreativeNxPro2,
  Bs888e,
  Zc302,
  CreativeNoteBook2,
  AiptekSlim3200,		/* 83 84 85 */
  LabtecWebcam,
  QCExpress,
  ICM532cam,
  MustekGsmart300,
  CreativeLive,			//90
  MercuryDigital,
  Wcam300A,
  CreativeVista3b,
  VeoStingray1,
  VeoStingray2,
  TyphoonWebshotIIUSB300k,	//96
  PolaroidPDC3070,
  QCExpressEtch2,
  QCforNotebook,
  QCim,				//100
  WebCam320,
  AiptekPocketCam4M,
  AiptekPocketDV5100,
  AiptekPocketDV5300,
  SunplusGeneric536,
  QCimA1,
  QCchat,
  QCimB9,
  Labtec929, //109 110
  Etoms61x151,	
  Etoms61x251,
  PalmPixDC85,
  Optimedia,
  ToptroIndus,
  AgfaCl20,
  LogitechQC92c,
  SonixWC311P,
  Concord3045,
  Mercury21,
  CreativeNX,
  CreativeInstant1,
  CreativeInstant2,
  QuickCamNB,
  WCam300AN,
  LabtecWCPlus,
  GeniusVideoCamMessenger,
  Pcam,
  GeniusDsc13,
  LastCamera
};
static struct cam_list clist[] = {
  {UnknownCamera, "Unknown"},
  {IntelPCCameraPro, "Intel PC Camera Pro"},
  {IntelCreateAndShare, "Intel Create and Share"},
  {GrandtecVcap, "Grandtec V.cap"},
  {ViewQuestM318B, "ViewQuest M318B"},
  {ViewQuestVQ110, "ViewQuest VQ110"},
  {KodakDVC325, "Kodak DVC-325"},
  {MustekGsmartMini2, "Mustek gSmart mini 2"},
  {MustekGsmartMini3, "Mustek gSmart mini 3"},
  {CreativePCCam300, "Creative PC-CAM 300"},
  {DLinkDSC350, "D-Link DSC-350"},
  {CreativePCCam600, "Creative PC-CAM 600"},
  {IntelPocketPCCamera, "Intel Pocket PC Camera"},
  {IntelEasyPCCamera, "Intel Easy PC Camera"},
  {ThreeComHomeConnectLite, "3Com Home Connect Lite"},
  {KodakEZ200, "Kodak EZ200"},
  {MaxellMaxPocket, "Maxell Max Pocket LEdit. 1.3 MPixels"},
  {AiptekMiniPenCam2, "Aiptek Mini PenCam  2 MPixels"},
  {AiptekPocketDVII, "Aiptek PocketDVII  1.3 MPixels"},
  {AiptekPenCamSD, "Aiptek Pencam SD  2 MPixels"},
  {AiptekMiniPenCam13, "Aiptek mini PenCam 1.3 MPixels"},
  {MustekGsmartLCD3, "Mustek Gsmart LCD 3"},
  {MustekMDC5500Z, "Mustek MDC5500Z"},
  {MegapixV4, "Megapix V4"},
  {AiptekPocketDV, "Aiptek PocketDV "},
  {HamaUSBSightcam, "Hama USB Sightcam 100"},
  {Arowana300KCMOSCamera, "Arowana 300K CMOS Camera"},
  {MystFromOriUnknownCamera, "Unknow Ori Camera"},
  {AiptekPocketDV3100, "Aiptek PocketDV3100+ "},
  {AiptekPocketCam3M, "Aiptek PocketCam  3 M "},
  {GeniusVideoCAMExpressV2, "Genius VideoCAM Express V2"},
  {Flexcam100Camera, "Flexcam 100 Camera"},
  {MustekGsmartLCD2, "Mustek Gsmart LCD 2"},
  {PureDigitalDakota, "Pure Digital Dakota"},
  {PetCam, "PetCam"},
  {BenqDC1500, "Benq DC1500"},
  {LogitechClickSmart420, "Logitech Inc. ClickSmart 420"},
  {LogitechClickSmart510, "Logitech Inc. ClickSmart 510"},
  {BenqDC1300, "Benq DC1300"},
  {HamaUSBSightcam2, "Hama USB Sightcam 100 (2)"},
  {MustekDV3000, "Mustek DV 3000"},
  {CreativePccam750, "Creative PCcam750"},
  {MaxellCompactPM3, "Maxell Compact PC PM3"},
  {BenqDC3410, "Benq DC3410"},
  {BenqDC1016, "Benq DC1016"},
  {MicroInnovationIC200, "Micro Innovation IC200"},
  {LogitechTraveler, "Logitech QuickCam Traveler"},
  {Flycam100Camera, "FlyCam Usb 100"},
  {UsbGrabberPV321c, "Usb Grabber PV321c"},
  {ADSInstantVCD, "ADS Instant VCD"},
  {Gsmartmini, "Mustek Gsmart Mini"},
  {Jenoptikjdc21lcd, "Jenoptik DC 21 LCD"},
  {LogitechClickSmart310, "Logitech ClickSmart 310"},
  {Terratec2move13, "Terratec 2 move 1.3"},
  {MustekDV4000, "Mustek DV4000 Mpeg4"},
  {AiptekDV3500, "Aiptek DV3500 Mpeg4"},
  {LogitechClickSmart820, "Logitech ClickSmart 820"},
  {Enigma13, "Digital Dream Enigma 1.3"},
  {Sonix6025, "Xcam Shanga"},
  {Epsilon13, "Digital Dream Epsilon 1.3"},
  {Nxultra, "Creative Webcam NX ULTRA"},
  {AiptekPocketCam2M, "Aiptek PocketCam 2Mega"},
  {DeMonUSBCapture, "3DeMON USB Capture"},
  {CreativeVista, "Creative Webcam Vista"},
  {PolaroidPDC2030, "Polaroid PDC2030"},
  {CreativeNotebook, "Creative Notebook PD1171"},
  {CreativeMobile, "Creative Mobile PD1090"},
  {LabtecPro, "Labtec Webcam Pro"},
  {MustekWcam300A, "Mustek Wcam300A"},
  {GeniusVideoCamV2, "Genius Videocam V2"},
  {GeniusVideoCamV3, "Genius Videocam V3"},
  {GeniusVideoCamExpressV2b, "Genius Videocam Express V2 Firmware 2"},
  {CreativeNxPro, "Creative Nx Pro"},
  {Sonix6029, "Sonix sn9c10x + Pas106 sensor"},
  {Vimicro, "Z-star Vimicro zc0301p"},
  {Digitrex2110, "ApexDigital Digitrex2110 spca533"},
  {GsmartD30, "Mustek Gsmart D30 spca533"},
  {CreativeNxPro2, "Creative NX Pro FW2"},
  {Bs888e, "Kowa Bs888e MicroCamera"},
  {Zc302, "Z-star Vimicro zc0302"},
  {CreativeNoteBook2, "Creative Notebook PD1170"},
  {AiptekSlim3200, "Aiptek Slim 3200"},
  {LabtecWebcam, "Labtec Webcam"},
  {QCExpress, "QC Express"},
  {ICM532cam, "ICM532 cam"},
  {MustekGsmart300, "Mustek Gsmart 300"},
  {CreativeLive, "Creative Live! "},
  {MercuryDigital, "Mercury Digital Pro 3.1Mp"},
  {Wcam300A, "Mustek Wcamm300A 2"},
  {CreativeVista3b, "Creative Webcam Vista 0x403b"},
  {VeoStingray1, "Veo Stingray 1"},
  {VeoStingray2, "Veo Stingray 2"},
  {TyphoonWebshotIIUSB300k, " Typhoon Webshot II"},
  {PolaroidPDC3070, " Polaroid PDC3070"},
  {QCExpressEtch2, "Logitech QuickCam Express II"},
  {QCforNotebook, "Logitech QuickCam for Notebook"},
  {QCim,"Logitech QuickCam IM"},
  {WebCam320,"Micro Innovation WebCam 320"},
  {AiptekPocketCam4M,"Aiptek Pocket Cam 4M"},
  {AiptekPocketDV5100,"Aiptek Pocket DV5100"},
  {AiptekPocketDV5300,"Aiptek Pocket DV5300"},
  {SunplusGeneric536,"Sunplus Generic spca536a"},
  {QCimA1,"Logitech QuickCam IM + sound"},
  {QCchat,"Logitech QuickCam chat"},
  {QCimB9,"Logitech QuickCam IM ???"},
  {Labtec929,"Labtec Webcam Elch2 "},
  {Etoms61x151,"QCam Sangha"},
  {Etoms61x251,"QCam xxxxxx"},
  {PalmPixDC85,"PalmPix DC85"},
  {Optimedia,"Optimedia TechnoAME"},
  {ToptroIndus,"Toptro Industrial"},
  {AgfaCl20,"Agfa ephoto CL20"},
  {LogitechQC92c,"Logitech QuickCam chat"},
  {SonixWC311P,"Sonix sn9c102P Hv7131R"},
  {Concord3045,"Concord 3045 spca536a"},
  {Mercury21,"Mercury Peripherals Inc."},
  {CreativeNX,"Creative NX"},
  {CreativeInstant1,"Creative Instant P0620"},
  {CreativeInstant2,"Creative Instant P0620D"},
  {QuickCamNB,"Logitech QuickCam for Notebooks"},
  {WCam300AN,"Mustek WCam300AN "},
  {LabtecWCPlus,"Labtec Webcam Plus"},
  {GeniusVideoCamMessenger,"VideoCam Messenger sn9c101 Ov7630"},
  {Pcam,"Mars-Semi Pc-Camera MR97311 MI0360"},
  {GeniusDsc13,"Genius Dsc 1.3 Smart spca504B-P3"},
  {-1, NULL}
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0)
static __devinitdata struct usb_device_id device_table[] = {
  {USB_DEVICE (0x0733, 0x0430)},	/* Intel PC Camera Pro */
  {USB_DEVICE (0x0733, 0x0401)},	/* Intel Create and Share */
  {USB_DEVICE (0x99FA, 0x8988)},	/* Grandtec V.cap */
  {USB_DEVICE (0x0733, 0x0402)},	/* ViewQuest M318B */
  {USB_DEVICE (0x0733, 0x0110)},	/* ViewQuest VQ110 */
  {USB_DEVICE (0x040A, 0x0002)},	/* Kodak DVC-325 */
  {USB_DEVICE (0x055f, 0xc420)},	/* Mustek gSmart Mini 2 */
  {USB_DEVICE (0x055f, 0xc520)},	/* Mustek gSmart Mini 3 */
  {USB_DEVICE (0x041E, 0x400A)},	/* Creative PC-CAM 300 */
  {USB_DEVICE (0x084D, 0x0003)},	/* D-Link DSC-350 */
  {USB_DEVICE (0x041E, 0x400B)},	/* Creative PC-CAM 600 */
  {USB_DEVICE (0x8086, 0x0630)},	/* Intel Pocket PC Camera */
  {USB_DEVICE (0x8086, 0x0110)},	/* Intel Easy PC Camera */
  {USB_DEVICE (0x0506, 0x00df)},	/* 3Com HomeConnect Lite */
  {USB_DEVICE (0x040a, 0x0300)},	/* Kodak EZ200 */
  {USB_DEVICE (0x04fc, 0x504b)},	/* Maxell MaxPocket LE 1.3 */
  {USB_DEVICE (0x08ca, 0x2008)},	/* Aiptek Mini PenCam 2 M */
  {USB_DEVICE (0x08ca, 0x0104)},	/* Aiptek PocketDVII 1.3 */
  {USB_DEVICE (0x08ca, 0x2018)},	/* Aiptek Pencam SD 2M */
  {USB_DEVICE (0x04fc, 0x504a)},	/* Aiptek Mini PenCam 1.3 */
  {USB_DEVICE (0x055f, 0xc530)},	/* Mustek Gsmart LCD 3 */
  {USB_DEVICE (0x055f, 0xc650)},	/* Mustek MDC5500Z */
  {USB_DEVICE (0x052b, 0x1513)},	/* Megapix V4 */
  {USB_DEVICE (0x08ca, 0x0103)},	/* Aiptek PocketDV */
  {USB_DEVICE (0x0af9, 0x0010)},	/* Hama USB Sightcam 100 */
  {USB_DEVICE (0x1776, 0x501c)},	/* Arowana 300K CMOS Camera */
  {USB_DEVICE (0x08ca, 0x0106)},	/* Aiptek Pocket DV3100+ */
  {USB_DEVICE (0x08ca, 0x2010)},	/* Aiptek PocketCam 3M */
  {USB_DEVICE (0x0458, 0x7004)},	/* Genius VideoCAM Express V2 */
  {USB_DEVICE (0x04fc, 0x0561)},	/* Flexcam 100 */
  {USB_DEVICE (0x055f, 0xc430)},	/* Mustek Gsmart LCD 2 */
  {USB_DEVICE (0x04fc, 0xffff)},	/* Pure DigitalDakota */
  {USB_DEVICE (0xabcd, 0xcdee)},	/* Petcam */
  {USB_DEVICE (0x04a5, 0x3008)},	/* Benq DC 1500 */
  {USB_DEVICE (0x046d, 0x0960)},	/* Logitech Inc. ClickSmart 420 */
  {USB_DEVICE (0x046d, 0x0901)},	/* Logitech Inc. ClickSmart 510 */
  {USB_DEVICE (0x04a5, 0x3003)},	/* Benq DC 1300 */
  {USB_DEVICE (0x0af9, 0x0011)},	/* Hama USB Sightcam 100 */
  {USB_DEVICE (0x055f, 0xc440)},	/* Mustek DV 3000 */
  {USB_DEVICE (0x041e, 0x4013)},	/* Creative Pccam750 */
  {USB_DEVICE (0x060b, 0xa001)},	/* Maxell Compact Pc PM3 */
  {USB_DEVICE (0x04a5, 0x300a)},	/* Benq DC3410 */
  {USB_DEVICE (0x04a5, 0x300c)},	/* Benq DC1016 */
  {USB_DEVICE (0x0461, 0x0815)},	/* Micro Innovation IC200 */
  {USB_DEVICE (0x046d, 0x0890)},	/* Logitech QuickCam traveler */
  {USB_DEVICE (0x10fd, 0x7e50)},	/* FlyCam Usb 100 */
  {USB_DEVICE (0x06e1, 0xa190)},	/* ADS Instant VCD */
  {USB_DEVICE (0x055f, 0xc220)},	/* Gsmart Mini */
  {USB_DEVICE (0x0733, 0x2211)},	/* Jenoptik jdc 21 LCD */
  {USB_DEVICE (0x046d, 0x0900)},	/* Logitech Inc. ClickSmart 310 */
  {USB_DEVICE (0x055f, 0xc360)},	/* Mustek DV4000 Mpeg4  */
  {USB_DEVICE (0x08ca, 0x2024)},	/* Aiptek DV3500 Mpeg4  */
  {USB_DEVICE (0x046d, 0x0905)},	/* Logitech ClickSmart820  */
  {USB_DEVICE (0x05da, 0x1018)},	/* Digital Dream Enigma 1.3 */
  {USB_DEVICE (0x0c45, 0x6025)},	/* Xcam Shanga */
  {USB_DEVICE (0x0733, 0x1311)},	/* Digital Dream Epsilon 1.3 */
  {USB_DEVICE (0x041e, 0x401d)},	/* Creative Webcam NX ULTRA */
  {USB_DEVICE (0x08ca, 0x2016)},	/* Aiptek PocketCam 2 Mega */
  {USB_DEVICE (0x0734, 0x043b)},	/* 3DeMon USB Capture aka */
  {USB_DEVICE (0x041E, 0x4018)},	/* Creative Webcam Vista (PD1100) */
  {USB_DEVICE (0x0546, 0x3273)},	/* Polaroid PDC2030 */
  {USB_DEVICE (0x041e, 0x401f)},	/* Creative Webcam Notebook PD1171 */
  {USB_DEVICE (0x041e, 0x4017)},	/* Creative Webcam Mobile PD1090 */
  {USB_DEVICE (0x046d, 0x08a2)},	/* Labtec Webcam Pro */
  {USB_DEVICE (0x055f, 0xd003)},	/* Mustek WCam300A */
  {USB_DEVICE (0x0458, 0x7007)},	/* Genius VideoCam V2 */
  {USB_DEVICE (0x0458, 0x700c)},	/* Genius VideoCam V3 */
  {USB_DEVICE (0x0458, 0x700f)},	/* Genius VideoCam Web V2 */
  {USB_DEVICE (0x041e, 0x401e)},	/* Creative Nx Pro */
  {USB_DEVICE (0x0c45, 0x6029)},	/* spcaCam@150 */
  {USB_DEVICE (0x0c45, 0x6009)},	/* spcaCam@120 */
  {USB_DEVICE (0x0c45, 0x600d)},	/* spcaCam@120 */
  {USB_DEVICE (0x04fc, 0x5330)},	/* Digitrex 2110 */
  {USB_DEVICE (0x055f, 0xc540)},	/* Gsmart D30 */
  {USB_DEVICE (0x0ac8, 0x301b)},	/* Asam Vimicro */
  {USB_DEVICE (0x041e, 0x403a)},	/* Creative Nx Pro 2 */
  {USB_DEVICE (0x055f, 0xc211)},	/* Kowa Bs888e Microcamera */
  {USB_DEVICE (0x0ac8, 0x0302)},	/* Z-star Vimicro zc0302 */
  {USB_DEVICE (0x0572, 0x0041)},	/* Creative Notebook cx11646 */
  {USB_DEVICE (0x08ca, 0x2022)},	/* Aiptek Slim 3200 */
  {USB_DEVICE (0x046d, 0x0921)},	/* Labtec Webcam */
  {USB_DEVICE (0x046d, 0x0920)},	/* QC Express */
  {USB_DEVICE (0x0923, 0x010f)},	/* ICM532 cams */
  {USB_DEVICE (0x055f, 0xc200)},	/* Mustek Gsmart 300 */
  {USB_DEVICE (0x0733, 0x2221)},	/* Mercury Digital Pro 3.1p */
  {USB_DEVICE (0x041e, 0x4036)},	/* Creative Live ! */
  {USB_DEVICE (0x055f, 0xc005)},	/* Mustek Wcam300A */
  {USB_DEVICE (0x041E, 0x403b)},	/* Creative Webcam Vista (VF0010) */
  {USB_DEVICE (0x0545, 0x8333)},	/* Veo Stingray */
  {USB_DEVICE (0x0545, 0x808b)},	/* Veo Stingray */
  {USB_DEVICE (0x10fd, 0x8050)},	/* Typhoon Webshot II USB 300k */
  {USB_DEVICE (0x0546, 0x3155)},	/* Polaroid PDC3070 */
  {USB_DEVICE (0x046d, 0x0928)},	/* Logitech QC Express Etch2 */
  {USB_DEVICE (0x046d, 0x092a)},	/* Logitech QC for Notebook */
  {USB_DEVICE (0x046d, 0x08a0)},	/* Logitech QC IM */
  {USB_DEVICE (0x0461, 0x0a00)},	/* MicroInnovation WebCam320 */
  {USB_DEVICE (0x08ca, 0x2028)},	/* Aiptek PocketCam4M */
  {USB_DEVICE (0x08ca, 0x2042)},	/* Aiptek PocketDV5100 */
  {USB_DEVICE (0x08ca, 0x2060)},	/* Aiptek PocketDV5300 */
  {USB_DEVICE (0x04fc, 0x5360)},	/* Sunplus Generic */
  {USB_DEVICE (0x046d, 0x08a1)},	/* Logitech QC IM 0x08A1 +sound*/
  {USB_DEVICE (0x046d, 0x08a3)},	/* Logitech QC Chat */
  {USB_DEVICE (0x046d, 0x08b9)},	/* Logitech QC IM ??? */
  {USB_DEVICE (0x046d, 0x0929)},	/* Labtec Webcam Elch2*/
  {USB_DEVICE (0x10fd, 0x0128)},	/* Typhoon Webshot II USB 300k 0x0128 */
  {USB_DEVICE (0x102c, 0x6151)},	/* Qcam Sangha CIF */
  {USB_DEVICE (0x102c, 0x6251)},	/* Qcam xxxxxx VGA */
  {USB_DEVICE (0x04fc, 0x7333)},	/* PalmPixDC85*/
  {USB_DEVICE (0x06be, 0x0800)},	/* Optimedia*/
  {USB_DEVICE (0x2899, 0x012c)},	/* Toptro Industrial*/
  {USB_DEVICE (0x06bd, 0x0404)},	/* Agfa CL20*/
  {USB_DEVICE (0x046d, 0x092c)},	/* Logitech QC chat Elch2*/
  {USB_DEVICE (0x0c45, 0x607c)},	/* Sonix sn9c102p Hv7131R*/
  {USB_DEVICE (0x0733, 0x3261)},	/* Concord 3045 spca536a*/
  {USB_DEVICE (0x0733, 0x1314)},        /* Mercury 2.1MEG Deluxe Classic Cam*/
  {USB_DEVICE (0x041e, 0x401c)},	/* Creative NX */
  {USB_DEVICE (0x041e, 0x4034)},	/* Creative Instant P0620 */
  {USB_DEVICE (0x041e, 0x4035)},	/* Creative Instant P0620D */
  {USB_DEVICE (0x046d, 0x08ae)},	/* Logitech QuickCam for Notebooks */
  {USB_DEVICE (0x055f, 0xd004)},	/* Mustek WCam300 AN */
  {USB_DEVICE (0x046d, 0x092b)},	/* Labtec Webcam Plus*/
  {USB_DEVICE (0x0c45, 0x602e)},	/* Genius VideoCam Messenger*/
  {USB_DEVICE (0x0c45, 0x602c)},	/* Generic Sonix OV7630*/
  {USB_DEVICE (0x093A, 0x050F)},	/* Mars-Semi Pc-Camera */
  {USB_DEVICE (0x0458, 0x7006)},	/* Genius Dsc 1.3 Smart */
  {USB_DEVICE (0x0000, 0x0000)},	/* MystFromOri Unknow Camera */
  {}				/* Terminating entry */
};


MODULE_DEVICE_TABLE (usb, device_table);
/* 
 We also setup the function for getting 
 page number from the virtual address 
*/
#define VIRT_TO_PAGE virt_to_page
#else /* LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0) */
#define VIRT_TO_PAGE MAP_NR
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0) */
/*
 * Let's include the initialization data for each camera type
 */
#include "spcausb.h"
#include "spca500_init.h"
#include "spca501_init.h"
#include "spca504_init.h"
#include "spca505_init.h"
#include "spca506.h"
#include "spca508_init.h"
#include "spca561.h"
#include "spcai2c_init.h"
#include "spca533.h"
#include "spca536.h"
#include "sonix.h"
#include "zc3xx.h"
#include "cx11646.h"
#include "tv8532.h"
#include "et61xx51.h"
#include "sn9c102p.h"
#include "mr97311.h"

/* Mode list for spca50x on external input */
/* Must be in descending order as the closest match which is equal or smaller than
 * the requested size is returned */
#if 0
/* Trashcan before goes out */
	(bridge) == BRIDGE_SONIX ? sonix_ext_modes : \
	(bridge) == BRIDGE_ZC3XX ? zc3xx_ext_modes : \
	(bridge) == BRIDGE_ETOMS ? etoms_ext_modes : \
	(bridge) == BRIDGE_SPCA561 ? spca561_ext_modes : \
	
static __u16 pcam_ext_modes[][6] = {
  /* x , y , Code, Value (6), Value (7), pipe */
  {1280, 1024, 0x00, 0, 0, 1023},
  {640, 480, 0x01, 0, 0, 1023},
  {384, 288, 0x11, 16, 12, 1023},
  {352, 288, 0x11, 18, 12, 1023},
  {320, 240, 0x02, 0, 0, 896},
  {192, 144, 0x12, 8, 6, 896},
  {176, 144, 0x12, 9, 6, 896},
  {0, 0, 0, 0, 0}
};
static __u16 zc3xx_ext_modes[][6] = {
  /* x , y , Code, x multiplier, y multiplier, pipe */
  {640, 480, 0x00, 0, 0, 1023},	// altersetting 7 endpoint 0x01
  {352, 288, 0x10, 0, 0, 1023}, //0x00 SIF sensor
  {320, 240, 0x01, 0, 0, 1023},
  {176, 144, 0x11, 0, 0, 1023}, //0x01
  {0, 0, 0, 0, 0, 0}
};
static __u16 sonix_ext_modes[][6] = {
  /* x , y , Code, x multiplier, y multiplier, pipe */
  {640, 480, 0x00, 0, 0, 1023},
  {352, 288, 0x00, 0, 0, 1023},
  {320, 240, 0x01, 0, 0, 1023},
  {176, 144, 0x01, 0, 0, 1023},
  {160, 120, 0x02, 0, 0, 1023},
  {0, 0, 0, 0, 0, 0}
};
static __u16 etoms_ext_modes[][6] = {
	/* x , y , Code, x, y, pipe */
	{352, 288, 0x00, 0, 0, 1000},
	{320, 240, 0x01, 0, 0, 1000},
	{176, 144, 0x01, 0, 0, 1000},
	{0, 0, 0, 0, 0, 0}
};
static __u16 spca561_ext_modes[][6] = {
  {352, 288, 0x00, 0x27, 0x00, 1023},
  {320, 240, 0x01, 0x27, 0x00, 1023},
  {176, 144, 0x02, 0x23, 0x00, 1023},	// software mode hardware seem buggy slow shift in video
  {160, 120, 0x03, 0x23, 0x00, 1023},
  {0, 0, 0, 0, 0}
};	
#endif
#define GET_EXT_MODES(bridge) (\
	(bridge) == BRIDGE_SPCA500 ? spca500_ext_modes : \
	(bridge) == BRIDGE_SPCA501 ? spca501_ext_modes : \
	(bridge) == BRIDGE_SPCA504 ? spca504_ext_modes : \
	(bridge) == BRIDGE_SPCA504B ? spca504_ext_modes : \
	(bridge) == BRIDGE_SPCA504_PCCAM600 ? spca504_pccam600_ext_modes : \
	(bridge) == BRIDGE_SPCA506 ? spca506_ext_modes : \
	(bridge) == BRIDGE_SPCA508 ? spca508_ext_modes : \
	(bridge) == BRIDGE_SPCA533 ? spca533_ext_modes : \
	(bridge) == BRIDGE_SPCA536 ? spca536_ext_modes : \
	(bridge) == BRIDGE_CX11646 ? cx11646_ext_modes : \
	(bridge) == BRIDGE_TV8532 ? tv8532_ext_modes : \
	(bridge) == BRIDGE_SN9C102P ? sn9c102p_ext_modes : \
	spca50x_ext_modes)
	
/* code is used for harware low nibble and software hight nibble */
static __u16 sn9c102p_ext_modes[][6] = {
	/* x , y , Code, x, y, pipe */
	{640, 480, 0x00, 0, 0, 1023},
	{352, 288, 0x10, 0, 0, 1023},
	{320, 240, 0x01, 0, 0, 1023},
	{176, 144, 0x11, 0, 0, 1023},
	{0, 0, 0, 0, 0, 0}
};



static __u16 cx11646_ext_modes[][6] = {
  /* x , y , Code, x multiplier, y multiplier, pipe */
  {640, 480, 0x00, 0, 0, 1023},	// altersetting 7 endpoint 0x01
  {352, 288, 0x01, 0, 0, 1023},
  {320, 240, 0x02, 0, 0, 1023},
  {176, 144, 0x03, 0, 0, 640},	// alt 4
  // {160, 120, 0x04, 0, 0, 512}, // alt 3 only works with external decoding
  {0, 0, 0, 0, 0, 0}
};

static __u16 spca500_ext_modes[][6] = {
  /* x , y , Code, x multiplier, y multiplier, pipe */
  {640, 480, 0x00, 40, 30, 1023},
  {352, 288, 0x00, 22, 18, 1023},
  {320, 240, 0x01, 40, 30, 1023},
  {176, 144, 0x01, 22, 18, 1023},
  /* 160x120 disable jpeg %16 */
  {0, 0, 0, 0, 0, 0}
};

static __u16 spca501_ext_modes[][6] = {
  /* x , y , Code, Value (6), Value (7), pipe */
  {640, 480, 0, 0x94, 0x004A, 1023},
  {352, 288, 0x10, 0x94, 0x004A, 1023},
  {320, 240, 0, 0x94, 0x104A, 896},
  {176, 144, 0x10, 0x94, 0x104A, 896},
  {160, 120, 0, 0x94, 0x204A, 384},
  {0, 0, 0, 0, 0}
};

static __u16 spca533_ext_modes[][6] = {
  /* x , y , Code, Value (6), Value (7), pipe */
  //{ 640, 480, 0x41, 1, 0, 1023 },
  {464, 480, 0x01, 0, 0, 1023},	//PocketDVII unscaled resolution aspect ratio need to expand x axis
  {464, 352, 0x01, 0, 0, 1023},	//Gsmart LCD3 feature good aspect ratio 
  {384, 288, 0x11, 5, 4, 1023},
  {352, 288, 0x11, 7, 4, 1023},
  {320, 240, 0x02, 0, 0, 1023},
  {192, 144, 0x12, 8, 6, 1023},
  {176, 144, 0x12, 9, 6, 1023},
  //{ 160, 120, 0x22, 1, 1, 1023 },
  //{ 320, 128, 0x03, 0, 0, 1023 }, /* don't work with megapix V4*/
  {0, 0, 0, 0, 0}
};

static __u16 spca536_ext_modes[][6] = {
  /* x , y , Code, Value (6), Value (7), pipe */
  {640, 480, 0x01, 1, 0, 1023},
  //{ 464, 480, 0x01, 0, 0, 1023 },
  //{ 464, 352, 0x01, 0, 0, 1023 },
  {384, 288, 0x11, 5, 4, 1023},
  {352, 288, 0x11, 7, 4, 1023},
  {320, 240, 0x02, 0, 0, 896},
  {192, 144, 0x12, 8, 6, 896},
  {176, 144, 0x12, 9, 6, 896},
  {0, 0, 0, 0, 0}
};

static __u16 spca504_ext_modes[][6] = {
  /* x , y , Code, Value (6), Value (7), pipe */
  {640, 480, 0x01, 0, 0, 1023},
  {384, 288, 0x11, 16, 12, 1023},
  {352, 288, 0x11, 18, 12, 1023},
  {320, 240, 0x02, 0, 0, 896},
  {192, 144, 0x12, 8, 6, 896},
  {176, 144, 0x12, 9, 6, 896},
  //{ 160, 120, 0x22, 1, 1, 896 },
  {0, 0, 0, 0, 0}
};


static __u16 spca504_pccam600_ext_modes[][6] = {
  /* x , y , Code, Value (6), Value (7), pipe */
  {1024, 768, 0x00, 0, 0, 1023},
  {640, 480, 0x01, 1, 0, 1023},
  {352, 288, 0x02, 2, 0, 896},
  {320, 240, 0x03, 3, 0, 896},
  {176, 144, 0x04, 4, 0, 768},
/*	{ 160,  120, 0x05, 5, 0, 768 },  */
  {0, 0, 0, 0, 0}
};



/* default value used for spca505 and spca505b */
static __u16 spca50x_ext_modes[][6] = {
  /* x , y , Code, Value (6), Value (7), pipe */
  {640, 480, 0, 0x10, 0x10, 1023},	/* Tested spca505b+VGA sensor */
  {352, 288, 1, 0x1a, 0x1a, 1023},	/* Tested */
  {320, 240, 2, 0x1c, 0x1d, 896},	/* Tested 20:01:2004 */
  {176, 144, 4, 0x34, 0x34, 512},	/* Tested */
  {160, 120, 5, 0x40, 0x40, 384},	/* Tested */
  {0, 0, 0, 0, 0}
};
static __u16 spca506_ext_modes[][6] = {
  /* In this table, element 3 (clk) controls the
   * clock, and gets written to 0x8700.
   */
  /* x , y , Code, clk, n/a,  pipe */
  {640, 480, 0x00, 0x10, 0x10, 1023},
  {352, 288, 0x01, 0x1a, 0x1a, 1023},
  {320, 240, 0x02, 0x1c, 0x1c, 1023},	//896
  {176, 144, 0x04, 0x34, 0x34, 1023},
  {160, 120, 0x05, 0x40, 0x40, 1023},
  {0, 0, 0, 0, 0}
};

//  1a 1b 20 
static __u16 spca508_ext_modes[][6] = {
  /* In this table, element 3 (clk) controls the
   * clock, and gets written to 0x8700.
   */
  /* x , y , Code, clk, n/a,  pipe */
  {352, 288, 0x00, 0x28, 0x00, 1023},
  {320, 240, 0x01, 0x28, 0x00, 1023},
  {176, 144, 0x02, 0x23, 0x00, 1023},
  {160, 120, 0x03, 0x23, 0x00, 1023},
  {0, 0, 0, 0, 0}
};

static __u16 tv8532_ext_modes[][6] = {
  /* x , y , Code, clk, n/a,  pipe */
  {352, 288, 0x00, 0x28, 0x00, 1023},
  {320, 240, 0x10, 0x28, 0x00, 1023},
  {176, 144, 0x01, 0x23, 0x00, 1023},
  /*{160, 120, 0x03, 0x23, 0x00, 1023}, */
  {0, 0, 0, 0, 0}
};

#ifdef CONFIG_PROC_FS
/* Not sure what we should do with this. I think it is V4L level 2 stuff */
/* Currently only use RGB24 */
static struct palette_list plist[] = {
  {VIDEO_PALETTE_GREY, "GREY"},
  {VIDEO_PALETTE_HI240, "HI240"},
  {VIDEO_PALETTE_RGB565, "RGB565"},
  {VIDEO_PALETTE_RGB24, "RGB24"},
  {VIDEO_PALETTE_RGB32, "RGB32"},
  {VIDEO_PALETTE_RGB555, "RGB555"},
  {VIDEO_PALETTE_YUV422, "YUV422"},
  {VIDEO_PALETTE_YUYV, "YUYV"},
  {VIDEO_PALETTE_UYVY, "UYVY"},
  {VIDEO_PALETTE_YUV420, "YUV420"},
  {VIDEO_PALETTE_YUV411, "YUV411"},
  {VIDEO_PALETTE_RAW, "RAW"},
  {VIDEO_PALETTE_YUV422P, "YUV422P"},
  {VIDEO_PALETTE_YUV411P, "YUV411P"},
  {VIDEO_PALETTE_YUV420P, "YUV420P"},
  {VIDEO_PALETTE_YUV410P, "YUV410P"},
  {VIDEO_PALETTE_RAW_JPEG, "RJPG"},
  {VIDEO_PALETTE_JPEG, "JPEG"},
  {-1, NULL}
};
#endif /* CONFIG_PROC_FS */

/* function for the tasklet */

void outpict_do_tasklet (unsigned long ptr);

/**********************************************************************
 *
 * Memory management
 *
 * This is a shameless copy from the USB-cpia driver (linux kernel
 * version 2.3.29 or so, I have no idea what this code actually does ;).
 * Actually it seems to be a copy of a shameless copy of the bttv-driver.
 * Or that is a copy of a shameless copy of ... (To the powers: is there
 * no generic kernel-function to do this sort of stuff?)
 *
 * Yes, it was a shameless copy from the bttv-driver. IIRC, Alan says
 * there will be one, but apparentely not yet -jerdfelt
 *
 * So I copied it again for the ov511 driver -claudio
 * And again for the spca50x driver -jcrisp
 **********************************************************************/

/* Given PGD from the address space's page table, return the kernel
 * virtual mapping of the physical memory mapped at ADR.
 */
#ifndef RH9_REMAP
static inline unsigned long
uvirt_to_kva (pgd_t * pgd, unsigned long adr)
{
  unsigned long ret = 0UL;
  pmd_t *pmd;
  pte_t *ptep, pte;

  if (!pgd_none (*pgd))
    {
#if PUD_SHIFT
      pud_t *pud = pud_offset (pgd, adr);
      if (!pud_none (*pud))
	{
	  pmd = pmd_offset (pud, adr);
#else
      pmd = pmd_offset (pgd, adr);
#endif
      if (!pmd_none (*pmd))
	{
	  /* WOLT kernels appear to have changed pte_offset to pte_offset_kernel */
#ifdef pte_offset
	  ptep = pte_offset (pmd, adr);
#else /* pte_offset */
	  ptep = pte_offset_kernel (pmd, adr);
#endif /* pte_offset */
	  pte = *ptep;
	  if (pte_present (pte))
	    {
	      ret = (unsigned long) page_address (pte_page (pte));
	      ret |= (adr & (PAGE_SIZE - 1));
	    }
#if PUD_SHIFT
	}
#endif
    }
}

return ret;
}
#endif /* RH9_REMAP */
/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the
 * area and marking the pages as reserved.
 */
#ifdef RH9_REMAP
static inline unsigned long
kvirt_to_pa (unsigned long adr)
{
  unsigned long kva, ret;

  kva = (unsigned long) page_address (vmalloc_to_page ((void *) adr));
  kva |= adr & (PAGE_SIZE - 1);	/* restore the offset */
  ret = __pa (kva);
  return ret;
}

#else /* RH9_REMAP */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static inline unsigned long
kvirt_to_pa (unsigned long adr)
{
  unsigned long kva, ret;

  kva = (unsigned long) page_address (vmalloc_to_page ((void *) adr));
  kva |= adr & (PAGE_SIZE - 1);
  ret = __pa (kva);
  return ret;
}
#else
static inline unsigned long
kvirt_to_pa (unsigned long adr)
{
  unsigned long va, kva, ret;

  va = VMALLOC_VMADDR (adr);
  kva = uvirt_to_kva (pgd_offset_k (va), va);
  ret = __pa (kva);
  return ret;
}
#endif
#endif /* RH9_REMAP */


static void *
rvmalloc (unsigned long size)
{
  void *mem;
  unsigned long adr;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 23)
unsigned long page;
#endif
  size = PAGE_ALIGN (size);
  mem = vmalloc_32 (size);
  if (!mem)
    return NULL;

  memset (mem, 0, size);	/* Clear the ram out, no junk to the user */
  adr = (unsigned long) mem;
  while ((long) size > 0)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 68)
      SetPageReserved (vmalloc_to_page ((void *) adr));
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 23)
      mem_map_reserve (vmalloc_to_page ((void *) adr));
#else
	page = kvirt_to_pa (adr);
	mem_map_reserve(VIRT_TO_PAGE(__va (page)));
#endif
#endif
      adr += PAGE_SIZE;
      size -= PAGE_SIZE;
    }

  return mem;
}

static void
rvfree (void *mem, unsigned long size)
{
  unsigned long adr;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 23)
unsigned long page;
#endif
  if (!mem)
    return;

  adr = (unsigned long) mem;
  while ((long) size > 0)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 68)
      ClearPageReserved (vmalloc_to_page ((void *) adr));
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 23)
      mem_map_unreserve (vmalloc_to_page ((void *) adr));
#else
	page = kvirt_to_pa (adr);
	mem_map_unreserve(VIRT_TO_PAGE(__va (page)));
#endif
#endif
      adr += PAGE_SIZE;
      size -= PAGE_SIZE;
    }
  vfree (mem);
}


/**********************************************************************
 * Get average luminance
 **********************************************************************/
static inline __u8
get_avg_lum (struct usb_spca50x *spca50x)
{
  __u8 luminance;		//The average luminance from camera
  switch (spca50x->bridge)
    {
    case BRIDGE_SPCA501:
      luminance = spca50x_reg_read (spca50x->dev, SPCA501_REG_CCDSP,
				    0x26, 2) >> 8;
      break;
#ifdef SPCA50X_ENABLE_EXP_BRIGHTNESS
    case BRIDGE_SPCA561:
    case BRIDGE_SPCA508:
      luminance = spca50x_reg_read (spca50x->dev, 0, 0x8621, 1);
      break;
#endif /* SPCA50X_ENABLE_EXP_BRIGHTNESS */
    default:
      luminance = 0;
      break;
    }
  return luminance;
}

/**********************************************************************
 * Get average R-G and B-G
 **********************************************************************/
static inline __u8
get_avg_RG (struct usb_spca50x *spca50x)
{
  __u8 rg;			//The average R-G for window5
  switch (spca50x->bridge)
    {
    case BRIDGE_SPCA501:
      rg = spca50x_reg_read (spca50x->dev, SPCA501_REG_CCDSP, 0x30, 2) >> 8;
      break;
    default:
      rg = 0;
      break;
    }
  return rg;
}

/**********************************************************************
 * Get average B-G
 **********************************************************************/
static inline __u8
get_avg_BG (struct usb_spca50x *spca50x)
{
  __u8 bg;			//The average B-G for window5
  switch (spca50x->bridge)
    {
    case BRIDGE_SPCA501:
      bg = spca50x_reg_read (spca50x->dev, SPCA501_REG_CCDSP, 0x2f, 2) >> 8;
      break;
    default:
      bg = 0;
      break;
    }
  return bg;
}

/**********************************************************************
 * /proc interface
 * Based on the CPiA driver version 0.7.4 -claudio
 * ..and again copied from the ov511 driver for the SPCA50x driver - jac
 **********************************************************************/

#ifdef CONFIG_PROC_FS

static struct proc_dir_entry *spca50x_proc_entry = NULL;
#ifdef CONFIG_VIDEO_PROC_FS
extern struct proc_dir_entry *video_proc_entry;
#endif /* CONFIG_VIDEO_PROC_FS */

#define YES_NO(x) ((x) ? "yes" : "no")

static int
spca50x_read_proc (char *page, char **start, off_t off,
		   int count, int *eof, void *data)
{
  char *out = page;
  int i, j, len;
  struct usb_spca50x *spca50x = data;

  /* IMPORTANT: This output MUST be kept under PAGE_SIZE
   *            or we need to get more sophisticated. */

  out += sprintf (out, "driver          : SPCA50X USB Camera\n");
  out += sprintf (out, "driver_version  : %s\n", version);
  out += sprintf (out, "model           : %s\n", (spca50x->desc) ?
		  clist[spca50x->desc].description : "unknown");
  out += sprintf (out, "streaming       : %s\n", YES_NO (spca50x->streaming));
  out += sprintf (out, "grabbing        : %s\n", YES_NO (spca50x->grabbing));
  out += sprintf (out, "compress        : %s\n", YES_NO (spca50x->compress));
  out +=
    sprintf (out, "data_format     : %s\n",
	     spca50x->force_rgb ? "RGB" : "BGR");
  out += sprintf (out, "brightness      : %d\n", spca50x->brightness >> 8);
  out += sprintf (out, "colour          : %d\n", spca50x->colour >> 8);
  out += sprintf (out, "hue             : %d\n", spca50x->hue >> 8);
  out += sprintf (out, "contrast        : %d\n", spca50x->contrast);
  out += sprintf (out, "num_frames      : %d\n", SPCA50X_NUMFRAMES);
  out += sprintf (out, "curframe        : %d\n", spca50x->curframe);
  out += sprintf (out, "lastFrameRead   : %d\n", spca50x->lastFrameRead);
  spca50x->avg_lum = get_avg_lum (spca50x);
  out += sprintf (out, "Avg. luminance  : 0x%X %d\n",
		  spca50x->avg_lum, spca50x->avg_lum);
  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    {
      out += sprintf (out, "frame           : %d\n", i);
      out += sprintf (out, "  sequence      : %d\n", spca50x->frame[i].seq);
      out +=
	sprintf (out, "  grabstate     : %d\n", spca50x->frame[i].grabstate);
      out += sprintf (out, "  depth         : %d\n", spca50x->frame[i].depth);
      out += sprintf (out, "  size          : %d %d\n",
		      spca50x->frame[i].width, spca50x->frame[i].height);
      out += sprintf (out, "  format        : ");
      for (j = 0; plist[j].num >= 0; j++)
	{
	  if (plist[j].num == spca50x->frame[i].format)
	    {
	      out += sprintf (out, "%s\n", plist[j].name);
	      break;
	    }
	}
      if (plist[j].num < 0)
	out += sprintf (out, "unknown\n");
      out += sprintf (out, "  data_buffer   : 0x%p\n",
		      spca50x->frame[i].data);
    }
  out +=
    sprintf (out, "snap_enabled    : %s\n", YES_NO (spca50x->snap_enabled));
  out += sprintf (out, "packet_size     : %d\n", spca50x->packet_size);
  out += sprintf (out, "internal ccd    : %s\n", YES_NO (!spca50x->ccd));
  out += sprintf (out, "framebuffer     : 0x%p\n", spca50x->fbuf);
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
  out += sprintf (out, "stable          : %d\n", spca50x->nstable);
  out += sprintf (out, "unstable        : %d\n", spca50x->nunstable);
  out += sprintf (out, "whiteness       : %d\n", spca50x->whiteness >> 12);
  spca50x->avg_rg = get_avg_RG (spca50x);
  spca50x->avg_bg = get_avg_BG (spca50x);
  out += sprintf (out, "Avg. R-G/B-G  : 0x%X/0x%X %d/%d\n",
		  spca50x->avg_rg, spca50x->avg_bg,
		  (char) spca50x->avg_rg, (char) spca50x->avg_bg);
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */

  len = out - page;
  len -= off;
  if (len < count)
    {
      *eof = 1;
      if (len <= 0)
	return 0;
    }
  else
    len = count;

  *start = page + off;

  return len;
}

static int
spca50x_write_proc (struct file *file, const char *buffer,
		    unsigned long count, void *data)
{
  return -EINVAL;
}

/*
 * Function services read requests to control proc entry
 * and prints all the static variables
 */
static int
spca50x_ctlread_proc (char *page, char **start, off_t off,
		      int count, int *eof, void *data)
{
  char *out = page;
  int len = 0;
  struct usb_spca50x *spca50x = data;

  out += sprintf (out, "force_rgb = %d\n", spca50x->force_rgb);
  out += sprintf (out, "min_bpp = %d\n", spca50x->min_bpp);
  out += sprintf (out, "lum_level = %d\n", spca50x->lum_level);
  out += sprintf (out, "debug = %d\n", debug);
#ifdef SPCA50X_ENABLE_OSD
  out += sprintf (out, "osd = %d\n", osd);
#endif /* SPCA50X_ENABLE_OSD */

  len = out - page;
  len -= off;
  if (len < count)
    {
      *eof = 1;
      if (len <= 0)
	return 0;
    }
  else
    len = count;

  *start = page + off;

  return len;
}

/*
 * Function compares two strings.
 * Return offset in pussy where prick ends if "prick" may penetrate 
 * int "pussy" like prick into pussy, -1 otherwise.
 */
static inline int
match (const char *prick, const char *pussy, int len2)
{
  int len1 = strlen (prick);	//length of male string
  int i;			//just an index variable
  const char *tmp;		//temporary pointer for my own pleasure

  // We skip all spaces and tabs
  for (i = 0; i < len2 && (pussy[i] == ' ' || pussy[i] == '\t'); i++)
    {
    }

  tmp = pussy + i;		// pointer to pussy with skipped shit (spaces and tabs)
  len2 = strlen (tmp);		//calculate length again

  if (len1 > len2)
    return -1;			//Fuck off, no fucking

  if (!strncmp (prick, tmp, len1))
    return i + len1;

  return -1;
}

#ifdef SPCA50X_ENABLE_RAWPROCENTRY
static int
spca50x_rawread_proc (char *page,
		      char **start,
		      off_t off, int count, int *eof, void *data)
{
  struct usb_spca50x *spca50x = data;

  *start = page;

  /* check whether the buffer exists */
  if (spca50x->rawBuffer == NULL)
    {
      *eof = 1;
      return 0;
    }

  /* check offset is valid */
  if (off > spca50x->rawBufferSize)
    {
      *eof = 1;
      return 0;
    }

  /* can't read more than exists in the buffer, either */
  if ((count + off) > spca50x->rawBufferSize)
    {
      count = spca50x->rawBufferSize - off;
    }

  /* can't read more than is available in the output buffer */
  if (count > PAGE_SIZE)
    {
      count = PAGE_SIZE;
    }

  if (count == 0)
    {
      *eof = 1;
      return 0;
    }

  /* populate the output buffer */
  memcpy (page, spca50x->rawBuffer + off, count);

  /* return read count */
  return count;
}

static int
spca50x_rawwrite_proc (struct file *file,
		       const char *buffer, unsigned long count, void *data)
{
  struct usb_spca50x *spca50x = data;

  /* if anything is written, flush the buffer */
  PDEBUG (3, "flushed raw proc entry buffer");
  spca50x->rawBufferSize = 0;

  return count;
}
#endif /* SPCA50X_ENABLE_RAWPROCENTRY */

/*
 * Try to calculate value from string (atoi). Converts  
 * decimal integer
 */
static inline int
atoi (const char *str)
{
  int result = 0;		//result of the function
  int i;			//just an index variable

  for (i = 0; str[i] >= '0' && str[i] <= '9'; i++)
    {
      result *= 10;
      result += str[i] - '0';
    }
  return result;
}


static int
spca50x_ctlwrite_proc (struct file *file, const char *buffer,
		       unsigned long count, void *data)
{
  int off;			//where look for a value
  struct usb_spca50x *spca50x = data;

  if ((off = match ("lum_level=", buffer, count)) >= 0)
    spca50x->lum_level = atoi (buffer + off);
  if ((off = match ("min_bpp=", buffer, count)) >= 0)
    spca50x->min_bpp = atoi (buffer + off);
  if ((off = match ("force_rgb=", buffer, count)) >= 0)
    spca50x->force_rgb = atoi (buffer + off);
  if ((off = match ("debug=", buffer, count)) >= 0)
    debug = atoi (buffer + off);

  return count;
}

static void
create_proc_spca50x_cam (struct usb_spca50x *spca50x)
{
  char name[PROC_NAME_LEN];
  struct proc_dir_entry *ent;

  if (!spca50x_proc_entry || !spca50x)
    return;

//Create videoxx proc entry
  sprintf (name, "video%d", spca50x->vdev->minor);
  PDEBUG (4, "creating /proc/video/spca50x/%s", name);

  ent =
    create_proc_entry (name, S_IFREG | S_IRUGO | S_IWUSR, spca50x_proc_entry);

  if (!ent)
    return;

  ent->data = spca50x;
  ent->read_proc = spca50x_read_proc;
  ent->write_proc = spca50x_write_proc;
  spca50x->proc_entry = ent;

// Create the controlxx proc entry
  sprintf (name, "control%d", spca50x->vdev->minor);
  PDEBUG (4, "creating /proc/video/spca50x/%s", name);
  ent = create_proc_entry (name, S_IFREG | S_IRUGO | S_IWUSR,
			   spca50x_proc_entry);

  if (!ent)
    return;

  ent->data = spca50x;
  ent->read_proc = spca50x_ctlread_proc;
  ent->write_proc = spca50x_ctlwrite_proc;
  spca50x->ctl_proc_entry = ent;

#ifdef SPCA50X_ENABLE_RAWPROCENTRY
// Create the rawxx proc entry
  sprintf (name, "raw%d", spca50x->vdev.minor);
  PDEBUG (4, "creating /proc/video/spca50x/%s", name);
  ent = create_proc_entry (name, S_IFREG | S_IRUGO | S_IWUSR,
			   spca50x_proc_entry);

  if (!ent)
    return;

  ent->data = spca50x;
  ent->read_proc = spca50x_rawread_proc;
  ent->write_proc = spca50x_rawwrite_proc;
  spca50x->raw_proc_entry = ent;
  spca50x->rawBufferSize = 0;
  spca50x->rawBuffer = vmalloc (10 * 1024 * 1024);
  if (spca50x->rawBuffer != NULL)
    {
      spca50x->rawBufferMax = 10 * 1024 * 1024;
      PDEBUG (3, "allocated 10Mb raw proc entry buffer");
    }
  else
    {
      PDEBUG (3, "vmalloc of raw proc entry buffer failed");
      spca50x->rawBufferMax = 0;
    }
#endif /* SPCA50X_ENABLE_RAWPROCENTRY */
}

static void
destroy_proc_spca50x_cam (struct usb_spca50x *spca50x)
{
  char name[PROC_NAME_LEN];

  if (!spca50x || !spca50x_proc_entry)
    return;

  /* destroy videoxx proc entry */
  if (spca50x->proc_entry != NULL)
    {
      sprintf (name, "video%d", spca50x->vdev->minor);
      PDEBUG (4, "destroying %s", name);
      remove_proc_entry (name, spca50x_proc_entry);
      spca50x->proc_entry = NULL;
    }

  /* destroy controlxx proc entry */
  if (spca50x->ctl_proc_entry != NULL)
    {
      sprintf (name, "control%d", spca50x->vdev->minor);
      PDEBUG (4, "destroying %s", name);
      remove_proc_entry (name, spca50x_proc_entry);
      spca50x->ctl_proc_entry = NULL;
    }

#ifdef SPCA50X_ENABLE_RAWPROCENTRY
  /* destroy rawxx proc entry */
  if (spca50x->raw_proc_entry != NULL)
    {
      sprintf (name, "raw%d", spca50x->vdev.minor);
      remove_proc_entry (name, spca50x_proc_entry);
      spca50x->raw_proc_entry = NULL;
      vfree (spca50x->rawBuffer);
    }
#endif /* SPCA50X_ENABLE_RAWPROCENTRY */

}

static void
proc_spca50x_create (void)
{
  /* No current standard here. Alan prefers /proc/video/ as it keeps
   * /proc "less cluttered than /proc/randomcardifoundintheshed/"
   * -claudio
   */

#ifdef CONFIG_VIDEO_PROC_FS
  if (video_proc_entry == NULL)
    {
      err ("Unable to initialise /proc/video/spca50x");
      return;
    }
  spca50x_proc_entry =
    create_proc_entry ("spca50x", S_IFDIR, video_proc_entry);
#else /* CONFIG_VIDEO_PROC_FS */
  spca50x_proc_entry = create_proc_entry ("spca50x", S_IFDIR, 0);
#endif /* CONFIG_VIDEO_PROC_FS */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0)
  if (spca50x_proc_entry)
    spca50x_proc_entry->owner = THIS_MODULE;
  else
#ifdef CONFIG_VIDEO_PROC_FS
    err ("Unable to initialise /proc/video/spca50x");
#else /* CONFIG_VIDEO_PROC_FS */
    err ("Unable to initialise /proc/spca50x");
#endif /* CONFIG_VIDEO_PROC_FS */
#endif /* LINUX_VERSION_CODE > KERNEL_VERSION(2,3,0) */
}

static void
proc_spca50x_destroy (void)
{
#ifdef CONFIG_VIDEO_PROC_FS
  PDEBUG (3, "removing /proc/video/spca50x");
#else /* CONFIG_VIDEO_PROC_FS */
  PDEBUG (3, "removing /proc/spca50x");
#endif /* CONFIG_VIDEO_PROC_FS */

  if (spca50x_proc_entry == NULL)
    return;

#ifdef CONFIG_VIDEO_PROC_FS
  remove_proc_entry ("spca50x", video_proc_entry);
#else /* CONFIG_VIDEO_PROC_FS */
  remove_proc_entry ("spca50x", 0);
#endif /* CONFIG_VIDEO_PROC_FS */
}
#endif /* CONFIG_PROC_FS */

/**********************************************************************
 *
 * Camera interface
 *
 **********************************************************************/

/* Read a value from the I2C bus. Returns the value read */
static int
spca50x_read_i2c (struct usb_spca50x *spca50x, __u16 device, __u16 address)
{
  struct usb_device *dev = spca50x->dev;
  int err_code;
  int retry;
  int ctrl = spca50x->i2c_ctrl_reg;	//The I2C control register
  int base = spca50x->i2c_base;	//The I2C base address

  err_code = spca50x_reg_write (dev, ctrl, base + SPCA50X_I2C_DEVICE, device);
  err_code =
    spca50x_reg_write (dev, ctrl, base + SPCA50X_I2C_SUBADDR, address);
  err_code =
    spca50x_reg_write (dev, ctrl, base + SPCA50X_I2C_TRIGGER,
		       SPCA50X_I2C_TRIGGER_BIT);
  /* Hmm. 506 docs imply we should poll the ready register before reading the return value */
  /* Poll the status register for a ready status */
  /* Doesn't look like the windows driver does tho' */
  retry = 60;
  while (--retry)
    {
      err_code = spca50x_reg_read (dev, ctrl, base + SPCA50X_I2C_STATUS, 1);
      if (err_code < 0)
	PDEBUG (1, "Error reading I2C status register");
      if (!err_code)
	break;
    }
  if (!retry)
    PDEBUG (1, "Too many retries polling I2C status after write to register");
  err_code = spca50x_reg_read (dev, ctrl, base + SPCA50X_I2C_READ, 1);
  if (err_code < 0)
    PDEBUG (1, "Failed to read I2C register at %d:%d", device, address);
  PDEBUG (3, "Read %d from %d:%d", err_code, device, address);
  return err_code;
}

static int
spca50x_read_SAA7113_status (struct usb_spca50x *spca50x)
{
  int value = 0;

  value =
    spca50x_read_i2c (spca50x, SAA7113_I2C_BASE_READ, SAA7113_REG_STATUS);
  if (value < 0)
    PDEBUG (1, "Failed to read SAA7113 status");

  PDEBUG (1, "7113 status : ");
  PDEBUG (1, "  READY %s", (SAA7113_STATUS_READY (value) ? "YES" : "NO"));
  PDEBUG (1, "  COPRO %s", (SAA7113_STATUS_COPRO (value) ? "YES" : "NO"));
  /*PDEBUG(1,"  SLTCA %s",(SAA7113_STATUS_SLTCA(value)?"YES":"NO")); */
  PDEBUG (1, "  WIPA  %s", (SAA7113_STATUS_WIPA (value) ? "YES" : "NO"));
  PDEBUG (1, "  GLIMB %s", (SAA7113_STATUS_GLIMB (value) ? "YES" : "NO"));
  PDEBUG (1, "  GLIMT %s", (SAA7113_STATUS_GLIMT (value) ? "YES" : "NO"));
  PDEBUG (1, "  FIDT  %s", (SAA7113_STATUS_FIDT (value) ? "YES" : "NO"));
  PDEBUG (1, "  HLVLN %s", (SAA7113_STATUS_HLVLN (value) ? "YES" : "NO"));
  PDEBUG (1, "  INTL  %s", (SAA7113_STATUS_INTL (value) ? "YES" : "NO"));

  return value;
}

static int
spca50x_write_i2c (struct usb_spca50x *spca50x, __u16 device,
		   __u16 subaddress, __u16 data)
{
  struct usb_device *dev = spca50x->dev;
  int err_code;
  int retry;
  int ctrl = spca50x->i2c_ctrl_reg;	//The I2C control register
  int base = spca50x->i2c_base;	//The I2C base address

  /* Tell the SPCA50x i2c subsystem the device address of the i2c device */
  err_code = spca50x_reg_write (dev, ctrl, base + SPCA50X_I2C_DEVICE, device);

  /* Poll the status register for a ready status */
  retry = 60;			// Arbitrary
  while (--retry)
    {
      err_code = spca50x_reg_read (dev, ctrl, base + SPCA50X_I2C_STATUS, 1);
      if (err_code < 0)
	PDEBUG (1, "Error reading I2C status register");
      if (!err_code)
	break;
    }
  if (!retry)
    PDEBUG (1, "Too many retries polling I2C status");

  err_code =
    spca50x_reg_write (dev, ctrl, base + SPCA50X_I2C_SUBADDR, subaddress);
  err_code = spca50x_reg_write (dev, ctrl, base + SPCA50X_I2C_VALUE, data);
  if (spca50x->i2c_trigger_on_write)
    err_code = spca50x_reg_write (dev, ctrl, base + SPCA50X_I2C_TRIGGER,
				  SPCA50X_I2C_TRIGGER_BIT);

  /* Poll the status register for a ready status */
  retry = 60;
  while (--retry)
    {
      err_code = spca50x_reg_read (dev, ctrl, SPCA50X_I2C_STATUS, 2);
      if (err_code < 0)
	PDEBUG (1, "Error reading I2C status register");
      if (!err_code)
	break;
    }
  if (!retry)
    PDEBUG (1, "Too many retries polling I2C status after write to register");

  if (debug > 2)
    {
      err_code = spca50x_read_i2c (spca50x, device, subaddress);
      if (err_code < 0)
	{
	  PDEBUG (3, "Can't read back I2C register value for %d:%d",
		  device, subaddress);
	}
      else if ((err_code & 0xff) != (data & 0xff))
	PDEBUG (3, "Read back %x should be %x at subaddr %x",
		err_code, data, subaddress);
    }
  return 0;
}

static int
spca50x_write_i2c_vector (struct usb_spca50x *spca50x, __u16 device,
			  __u16 data[][2])
{
  int I = 0;

  while (data[I][0])
    {
      spca50x_write_i2c (spca50x, device, data[I][0], data[I][1]);
      I++;
    }
  return 0;
}

static int
spca50x_set_packet_size (struct usb_spca50x *spca50x, int size)
{
  int alt;
	/**********************************************************************/
	/******** Try to find real Packet size from usb struct ****************/
  struct usb_device *dev = spca50x->dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  struct usb_interface_descriptor *interface = NULL;

  struct usb_config_descriptor *config = dev->actconfig;
#else
  struct usb_host_interface *interface = NULL;
  struct usb_interface *intf;
#endif
  int mysize = 0;
  

	/**********************************************************************/

  if (size == 0)
    alt = SPCA50X_ALT_SIZE_0;
  else if (size == 128)
    alt = SPCA50X_ALT_SIZE_128;
  else if (size == 256)
    alt = SPCA50X_ALT_SIZE_256;
  else if (size == 384)
    alt = SPCA50X_ALT_SIZE_384;
  else if (size == 512)
    alt = SPCA50X_ALT_SIZE_512;
  else if (size == 640)
    alt = SPCA50X_ALT_SIZE_640;
  else if (size == 768)
    alt = SPCA50X_ALT_SIZE_768;
  else if (size == 896)
    alt = SPCA50X_ALT_SIZE_896;
  else if (size ==1000) 
    alt = ETOMS_ALT_SIZE_1000;
  else if (size == 1023)
    if (spca50x->bridge == BRIDGE_SONIX ||
    	spca50x->bridge == BRIDGE_SN9C102P ||
	spca50x->bridge == BRIDGE_MR97311 )
      {
	alt = 8;
      }
    else
      {
	alt = SPCA50X_ALT_SIZE_1023;
      }
  else
    {
      /* if an unrecognised size, default to the minimum */
      PDEBUG (5, "Set packet size: invalid size (%d), defaulting to %d",
	      size, SPCA50X_ALT_SIZE_128);
      alt = SPCA50X_ALT_SIZE_128;
    }


  PDEBUG (5, "iface alt: %d %d ", spca50x->iface, alt);
  if (usb_set_interface (spca50x->dev, spca50x->iface, alt) < 0)
    {
      err ("Set packet size: set interface error");
      return -EBUSY;
    }

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)
  intf = usb_ifnum_to_if (dev, spca50x->iface);
  if (intf)
    {
      interface = usb_altnum_to_altsetting (intf, alt);
    }
  else
    {
      PDEBUG (0, "intf not found");
      return -ENXIO;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
   mysize = le16_to_cpu(interface->endpoint[0].desc.wMaxPacketSize);
#else
   mysize = (interface->endpoint[0].desc.wMaxPacketSize);
#endif
#else
  interface = &config->interface[spca50x->iface].altsetting[alt];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
  mysize = le16_to_cpu(interface->endpoint[0].wMaxPacketSize);
#else
  mysize = (interface->endpoint[0].wMaxPacketSize);
#endif  
#endif
  
  spca50x->packet_size = mysize & 0x03ff;
  spca50x->alt = alt;
  PDEBUG (1, "set real packet size: %d, alt=%d", mysize, alt);
  return 0;
}

/* Returns number of bits per pixel (regardless of where they are located; planar or
 * not), or zero for unsupported format.
 */
static int
spca5xx_get_depth (struct usb_spca50x *spca50x, int palette)
{
  switch (palette)
    {
//      case VIDEO_PALETTE_GREY:     return 8;
    case VIDEO_PALETTE_RGB565:
      return 16;
    case VIDEO_PALETTE_RGB24:
      return 24;

//      case VIDEO_PALETTE_YUV422:   return 16;
//      case VIDEO_PALETTE_YUYV:     return 16;
//      case VIDEO_PALETTE_YUV420:   return 24;
    case VIDEO_PALETTE_YUV420P:
      return 12;		/* strange need 12 this break the read method for this planar mode (6*8/4) */
//      case VIDEO_PALETTE_YUV422P:  return 24; /* Planar */

    case VIDEO_PALETTE_RGB32:
      return 32;
    case VIDEO_PALETTE_RAW_JPEG:
      return 24;		/* raw jpeg. what should we return ?? */
    case VIDEO_PALETTE_JPEG:
      if (spca50x->cameratype == JPEG ||
	  spca50x->cameratype == JPGH ||
	  spca50x->cameratype == JPGC ||
	  spca50x->cameratype == JPGS ||
	  spca50x->cameratype == JPGM )
	{
	  return 8;
	}
      else
	return 0;
    default:
      return 0;			/* Invalid format */
    }
}

/**********************************************************************
* spca50x_isoc_irq
* Function processes the finish of the USB transfer by calling 
* spca50x_move_data function to move data from USB buffer to internal
* driver structures 
***********************************************************************/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static void
spca50x_isoc_irq (struct urb *urb, struct pt_regs *regs)
{
  int i;
#else
static void
spca50x_isoc_irq (struct urb *urb)
{
#endif
  int len;
  struct usb_spca50x *spca50x;

  if (!urb->context)
    {
      PDEBUG (4, "no context");
      return;
    }

  spca50x = (struct usb_spca50x *) urb->context;

  if (!spca50x->dev)
    {
      PDEBUG (4, "no device ");
      return;
    }
  if (!spca50x->user)
    {
      PDEBUG (4, "device not open");
      return;
    }
  if (!spca50x->streaming)
    {
      /* Always get some of these after close but before packet engine stops */
      PDEBUG (4, "hmmm... not streaming, but got interrupt");
      return;
    }
  if (!spca50x->present)
    {
      /*  */
      PDEBUG (4, "device disconnected ..., but got interrupt !!");
      return;
    }
  /* Copy the data received into our scratch buffer */
  if (spca50x->curframe >= 0)
    {
      len = spca50x_move_data (spca50x, urb);
    }
  else if (waitqueue_active (&spca50x->wq))
    {
      wake_up_interruptible (&spca50x->wq);
    }

  /* Move to the next sbuf */
  spca50x->cursbuf = (spca50x->cursbuf + 1) % SPCA50X_NUMSBUF;

  urb->dev = spca50x->dev;
  urb->status = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 4)
  if ((i = usb_submit_urb (urb, GFP_ATOMIC)) != 0)
    err ("usb_submit_urb() ret %d", i);
#else
/* If we use urb->next usb_submit_urb() is not need in 2.4.x */
  //if ((i = usb_submit_urb(urb)) != 0)
  //err("usb_submit_urb() ret %d", i);
#endif


  /* let's try to set up brightness for the next frame */
  if (autoadjust && !spca50x->bh_requested)
    {
      spca50x->task.data = spca50x;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
      spca50x->task.func = auto_bh;
      schedule_work (&spca50x->task);
#else
      spca50x->task.sync = 0;
      spca50x->task.routine = auto_bh;
      SCHEDULE_TASK (&spca50x->task);
#endif
      spca50x->bh_requested = 1;
    }

  return;
}


/**********************************************************************
* spca50x_init_isoc
* Function starts the ISO USB transfer by enabling this process
* from USB side and enabling ISO machine from the chip side
***********************************************************************/
static inline void
spcaCameraStart (struct usb_spca50x *spca50x)
{
  int err_code;
  struct usb_device *dev = spca50x->dev;
  switch (spca50x->bridge)
    {
    case BRIDGE_SN9C102P:
      sn9c102p_start(spca50x);
       break;
    case BRIDGE_ETOMS:
      Et_startCamera(spca50x);
       break;
    case BRIDGE_CX11646:
      cx11646_start (spca50x);
      break;
    case BRIDGE_ZC3XX:
      zc3xx_start (spca50x);
      break;
    case BRIDGE_SONIX:
      sonix_start (spca50x);
      break;
    case BRIDGE_SPCA500:
      {
	err_code = spca500_initialise (spca50x);
	break;
      }

    case BRIDGE_SPCA501:
      {
	/* Enable ISO packet machine CTRL reg=2,
	 * index=1 bitmask=0x2 (bit ordinal 1)
	 */
	err_code = spca50x_reg_write (dev, SPCA501_REG_CTLRL,
				      (__u16) 0x1, (__u16) 0x2);
	break;
      }

    case BRIDGE_SPCA504:
      spca504_initialize (spca50x);
      break;
    case BRIDGE_SPCA504_PCCAM600:
      {				/* specific code for clicsmart is in pccam600 */
	spca504_pccam600_initialize (spca50x);
	break;
      }
    case BRIDGE_SPCA536:
      {
	spca536_start (spca50x);
	break;
      }
    case BRIDGE_SPCA533:
    case BRIDGE_SPCA504B:
      {
	spca504B_start (spca50x);
	break;
      }
    case BRIDGE_SPCA506:
      spca506_start (spca50x);

      break;
    case BRIDGE_SPCA505:

      {
	//nessesary because without it we can see stream only once after loading module
	//stopping usb registers Tomasz change
	spca50x_reg_write (dev, 0x2, 0x0, 0x0);

	/* Enable ISO packet machine - should we do this here or in ISOC init ? */
	err_code = spca50x_reg_write (dev, SPCA50X_REG_USB,
				      SPCA50X_USB_CTRL, SPCA50X_CUSB_ENABLE);

//                      spca50x_reg_write(dev, 0x5, 0x0, 0x0);
//                      spca50x_reg_write(dev, 0x5, 0x0, 0x1);
//                      spca50x_reg_write(dev, 0x5, 0x11, 0x2);
	break;
      }

    case BRIDGE_SPCA508:
      err_code = spca50x_reg_write (dev, 0, 0x8112, 0x10 | 0x20);
      break;
    case BRIDGE_SPCA561:
      {
	/* Video ISO enable, Video Drop Packet enable: */
	spca561_start (spca50x);
	break;
      }
    case BRIDGE_TV8532:
      {
	tv8532_start (spca50x);
	break;
      }
    case BRIDGE_MR97311:
      {
       pcam_start( spca50x);
	break;
	}
    }

}

static inline void
spcaCameraStop (struct usb_spca50x *spca50x)
{

  switch (spca50x->bridge)
    {
    case BRIDGE_SN9C102P:
      sn9c102p_stop (spca50x);
      break;
    case BRIDGE_ETOMS:
	Et_stopCamera(spca50x);
	break;
    case BRIDGE_CX11646:
      /* deferred after set_alt(0,0) 
         cx11646_stop(spca50x); 
       */
      break;
    case BRIDGE_ZC3XX:
      zc3xx_stop (spca50x);
      break;
    case BRIDGE_SONIX:
      sonix_stop (spca50x);
      break;
    case BRIDGE_SPCA500:
      {
	/* stop the transfer */
	// Tha's the code need for familycam 300
	//spca50x_reg_write(spca50x->dev, 0x00, 0x8000, 0x0000);
	/*
	   spca50x_reg_write(spca50x->dev, 0xe1, 0x0001, 0x0000);
	   if (spca50x_reg_readwait(spca50x->dev, 0x06, 0, 0) != 0)
	   {
	   PDEBUG(2, "spca50x_reg_readwait() failed");
	   }

	   spca50x_reg_write(spca50x->dev, 0x0c, 0x0000, 0x0000);
	   spca50x_reg_write(spca50x->dev, 0x30, 0x0000, 0x0004);
	   if (spca500_full_reset(spca50x) < 0)
	   {
	   PDEBUG(2, "spca50x_full_reset() failed");
	   }
	 */
	break;
      }
    case BRIDGE_SPCA501:
      {
	/* Disable ISO packet machine CTRL reg=2, index=1 bitmask=0x0 (bit ordinal 1) */
	spca50x_reg_write (spca50x->dev, SPCA501_REG_CTLRL,
			   (__u16) 0x1, (__u16) 0x0);
	break;
      }
    case BRIDGE_SPCA504_PCCAM600:
    case BRIDGE_SPCA504:
      spca50x_reg_write (spca50x->dev, 0x00, 0x2000, 0x0000);

      if (spca50x->desc == AiptekMiniPenCam13)
	{
	  /* spca504a aiptek */

	  spca504A_acknowledged_command (spca50x, 0x08, 6, 0, 0x86, 1);
	  spca504A_acknowledged_command (spca50x, 0x24, 0x0000, 0x0000, 0x9d,
					 1);
	  spca504A_acknowledged_command (spca50x, 0x01, 0x000f, 0x0000, 0xFF,
					 1);
	}
      else
	{
	  spca504_acknowledged_command (spca50x, 0x24, 0x0000, 0x0000);
	  spca50x_reg_write (spca50x->dev, 0x01, 0x000f, 0x0);
	}
      break;
    case BRIDGE_SPCA536:
      {
	spca536_stop (spca50x);
      }
    case BRIDGE_SPCA533:
    case BRIDGE_SPCA504B:
      {
	spca504B_stop (spca50x);
	break;
      }
    case BRIDGE_SPCA505:
      {
	spca50x_reg_write (spca50x->dev, 0x2, 0x0, 0x0);	//Disable ISO packet machine
	break;
      }
    case BRIDGE_SPCA506:
      spca506_stop (spca50x);
      break;
    case BRIDGE_SPCA508:
      {
	// Video ISO disable, Video Drop Packet enable:
	spca50x_reg_write (spca50x->dev, 0, 0x8112, 0x20);
	break;
      }
    case BRIDGE_SPCA561:
      {
	// Video ISO disable, Video Drop Packet enable:
	spca561_stop (spca50x);
	break;
      }
    case BRIDGE_TV8532:
      {
	tv8532_stop (spca50x);
	break;
      }
     case BRIDGE_MR97311:
     {
       pcam_stop( spca50x );
	break;
      } 
    }

}
static int
spca50x_init_isoc (struct usb_spca50x *spca50x)
{

  struct urb *urb;
  int fx, err, n;
  int intpipe;
  PDEBUG (3, "*** Initializing capture ***");
/* reset iso context */
  spca50x->compress = 0;
  spca50x->curframe = 0;
  spca50x->cursbuf = 0;
  spca50x->scratchlen = 0;
  spca50x->frame[0].seq = -1;
  spca50x->lastFrameRead = -1;
  
  spca50x_set_packet_size (spca50x, spca50x->pipe_size);
  PDEBUG (2, "setpacketsize %d", spca50x->pipe_size);

  for (n = 0; n < SPCA50X_NUMSBUF; n++)
    {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
      urb = usb_alloc_urb (FRAMES_PER_DESC, GFP_KERNEL);
#else
      urb = usb_alloc_urb (FRAMES_PER_DESC);
#endif
      if (!urb)
	{
	  err ("init isoc: usb_alloc_urb ret. NULL");
	  return -ENOMEM;
	}
      spca50x->sbuf[n].urb = urb;
      urb->dev = spca50x->dev;
      urb->context = spca50x;
      urb->pipe = usb_rcvisocpipe (spca50x->dev, SPCA50X_ENDPOINT_ADDRESS);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
      urb->transfer_flags = URB_ISO_ASAP;
      urb->interval = 1;
#else
      urb->transfer_flags = USB_ISO_ASAP;
#endif
      urb->transfer_buffer = spca50x->sbuf[n].data;
      urb->complete = spca50x_isoc_irq;
      urb->number_of_packets = FRAMES_PER_DESC;
      urb->transfer_buffer_length = spca50x->packet_size * FRAMES_PER_DESC;
      for (fx = 0; fx < FRAMES_PER_DESC; fx++)
	{
	  urb->iso_frame_desc[fx].offset = spca50x->packet_size * fx;
	  urb->iso_frame_desc[fx].length = spca50x->packet_size;
	}
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,41)
  spca50x->sbuf[SPCA50X_NUMSBUF - 1].urb->next = spca50x->sbuf[0].urb;
  for (n = 0; n < SPCA50X_NUMSBUF - 1; n++)
    spca50x->sbuf[n].urb->next = spca50x->sbuf[n + 1].urb;
#endif
  spcaCameraStart (spca50x);
  if (spca50x->bridge == BRIDGE_SONIX)
    {
      intpipe = usb_rcvintpipe (spca50x->dev, 3);
      usb_clear_halt (spca50x->dev, intpipe);
    }
  PDEBUG (5, "init isoc int %d altsetting %d", spca50x->iface, spca50x->alt);
  for (n = 0; n < SPCA50X_NUMSBUF; n++)
    {
      spca50x->sbuf[n].urb->dev = spca50x->dev;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,41)
      err = usb_submit_urb (spca50x->sbuf[n].urb, GFP_KERNEL);
#else
      err = usb_submit_urb (spca50x->sbuf[n].urb);
#endif
      if (err){
	err ("init isoc: usb_submit_urb(%d) ret %d", n, err);
	return err;
	}
    }

  for (n = 0; n < SPCA50X_NUMFRAMES; n++)
    {
      spca50x->frame[n].grabstate = FRAME_UNUSED;
      spca50x->frame[n].scanstate = STATE_SCANNING;
    }

  spca50x->streaming = 1;

  return 0;

}


/**********************************************************************
* spca50x_stop_isoc
* Function stops the USB ISO pipe by stopping the chip ISO machine
* and stopping USB transfer
***********************************************************************/
static void
spca5xx_kill_isoc (struct usb_spca50x *spca50x)
{
  int n;

  if (!spca50x)
    return;

  PDEBUG (3, "*** killing capture ***");
  spca50x->streaming = 0;

  /* Unschedule all of the iso td's */
  for (n = SPCA50X_NUMSBUF - 1; n >= 0; n--)
    {
      if (spca50x->sbuf[n].urb)
	{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)
	  usb_kill_urb (spca50x->sbuf[n].urb);
#else
	  usb_unlink_urb (spca50x->sbuf[n].urb);
#endif
	  usb_free_urb (spca50x->sbuf[n].urb);
	  spca50x->sbuf[n].urb = NULL;
	}
    }

  PDEBUG (3, "*** Isoc killed ***");
}

static void
spca50x_stop_isoc (struct usb_spca50x *spca50x)
{

  if (!spca50x->streaming || !spca50x->dev)
    return;

  PDEBUG (3, "*** Stopping capture ***");
  spcaCameraStop (spca50x);
  spca5xx_kill_isoc(spca50x);
  spca50x_set_packet_size (spca50x, 0);
  if (spca50x->bridge == BRIDGE_CX11646)
    cx11646_stop (spca50x);

  PDEBUG (3, "*** Capture stopped ***");
}

/**********************************************************************
* spca50x_find_mode_index
* Function finds the mode index in the modes table
***********************************************************************/
static inline int
spca50x_find_mode_index (int width, int height, __u16 ext_modes[][6])
{
  int i = 0;
  int j = 0;
  /* search through the mode list until we hit one which has
   * a smaller or equal resolution, or we run out of modes.
   */
  for (i = 0; ext_modes[i][0] &&
       (ext_modes[i][0] >= width && ext_modes[i][1] >= height); i++)
    {
      if (((ext_modes[i][2] & 0xF0) >> 4) == 0)
	j = (i & 0x0F) << 4;
    }
  /* search to found the first hardware available mode */
  i--;				/* FIXME */
  PDEBUG (2, "find Hardware mode %d soft mode %d", j >> 4, i);
  /* check whether we found the requested mode, or ran out of
   * modes in the list.
   */
  if (!ext_modes[i][0] ||
      ext_modes[i][0] != width || ext_modes[i][1] != height)
    {
      PDEBUG (3, "Failed to find mode %d x %d", width, height);
      return -EINVAL;
    }
  i = (i & 0x0F) | j;
  PDEBUG (2, "Pass Hardware mode %d soft mode %d", (i >> 4), (i & 0x0F));
  /* return the index of the requested mode */
  return i;
}

/**********************************************************************
* spca50x_smallest_mode_index
* Function finds the mode index in the modes table of the smallest
* available mode.
***********************************************************************/
static int spca5xx_getDefaultMode(struct usb_spca50x *spca50x)
{
  int i;
  for (i = QCIF;i < TOTMODE; i++){
  if(spca50x->mode_cam[i].method == 0 && spca50x->mode_cam[i].width){
		spca50x->width = spca50x->mode_cam[i].width;
		spca50x->height = spca50x->mode_cam[i].height;
		spca50x->method = 0;
		spca50x->pipe_size = spca50x->mode_cam[i].pipe ;
		spca50x->mode = spca50x->mode_cam[i].mode;
		return 0;
		}
  }
return -EINVAL;
}
static inline int
spca50x_smallest_mode_index (__u16 ext_modes[][6],
			     int *cols, int *rows, int *pipe)
{
  int i;
  int width = INT_MAX;
  int height = INT_MAX;
  int intpipe = 1023;
  int index = -EINVAL;

  /* search through the mode list until we run out of modes */
  /* and take the smallest hardware mode and pipe size */
  for (i = 0; ext_modes[i][0]; i++)
    {
      if (ext_modes[i][0] < width || ext_modes[i][1] < height)
	{
	  if (!(ext_modes[i][2] & 0xF0))
	    {
	      width = ext_modes[i][0];
	      height = ext_modes[i][1];
	      intpipe = ext_modes[i][5];
	      index = i;
	    }
	}
    }

  /* return the index of the smallest mode */
  if (index != -EINVAL)
    {
      *pipe = intpipe;
      if (cols != NULL)
	*cols = width;
      if (rows != NULL)
	*rows = height;
    }
  return i;
}

/**********************************************************************
* spca50x_set_mode
* Function sets up the resolution directly. 
* Attention!!! index, the index in modes array is NOT checked. 
***********************************************************************/
static int spca5xx_getcapability(struct usb_spca50x *spca50x )
{
	
	int maxw,maxh,minw,minh;
	int i;
	minw = minh = 255*255;
	maxw = maxh = 0;
	for (i = QCIF; i < TOTMODE; i++){
	if( spca50x->mode_cam[i].width){
	  if (maxw < spca50x->mode_cam[i].width || maxh < spca50x->mode_cam[i].height){
	    maxw = spca50x->mode_cam[i].width;
	    maxh = spca50x->mode_cam[i].height;
	  }
	  if (minw > spca50x->mode_cam[i].width || minh > spca50x->mode_cam[i].height){
	    minw = spca50x->mode_cam[i].width;
	    minh = spca50x->mode_cam[i].height;
	  }
	 }
	}
	spca50x->maxwidth = maxw;
	spca50x->maxheight = maxh;
	spca50x->minwidth = minw;
	spca50x->minheight = minh;
	PDEBUG(0,"maxw %d maxh %d minw %d minh %d",maxw,maxh,minw,minh);	
return 0;
}
static inline int v4l_to_spca5xx(int format)
{switch(format){
    case VIDEO_PALETTE_RGB565: return P_RGB16;
    case VIDEO_PALETTE_RGB24: return P_RGB24;
    case VIDEO_PALETTE_RGB32: return P_RGB32;
    case VIDEO_PALETTE_YUV422P: return P_YUV422;
    case VIDEO_PALETTE_YUV420P: return P_YUV420;
    case VIDEO_PALETTE_RAW_JPEG: return P_RAW;
    case VIDEO_PALETTE_JPEG: return P_JPEG;
    default: return -EINVAL;
   }
} 
static inline int spca5xx_setMode(struct usb_spca50x *spca50x,int width,int height,int format)
{
  int i,j;
  int formatIn;
  int crop = 0, cropx1 = 0, cropx2 = 0, cropy1 = 0, cropy2 = 0, x = 0, y = 0;
   /* Avoid changing to already selected mode */
   /* convert V4l format to our internal format */
   PDEBUG(2,"Set mode asked w %d h %d p %d",width,height,format);
  
  if ((formatIn = v4l_to_spca5xx(format)) < 0) 
    return -EINVAL;
  for (i = QCIF; i< TOTMODE; i++){
    if((spca50x->mode_cam[i].width == width) &&
     (spca50x->mode_cam[i].height == height) &&
     (spca50x->mode_cam[i].t_palette & formatIn)){
        spca50x->width = spca50x->mode_cam[i].width;
        spca50x->height = spca50x->mode_cam[i].height;
        spca50x->pipe_size = spca50x->mode_cam[i].pipe ;
        spca50x->mode = spca50x->mode_cam[i].mode;
	spca50x->method = spca50x->mode_cam[i].method;
     if(spca50x->method){
     if (spca50x->method == 1){
      for (j = i; j < TOTMODE ; j++){
       if(spca50x->mode_cam[j].method == 0 && spca50x->mode_cam[j].width){
        spca50x->hdrwidth = spca50x->mode_cam[j].width;
        spca50x->hdrheight = spca50x->mode_cam[j].height;
	spca50x->mode = spca50x->mode_cam[j].mode; // overwrite by the hardware mode
        break;
       }
      } // end match hardware mode
      if (!spca50x->hdrwidth && !spca50x->hdrheight)
          return -EINVAL;  
     } 
     }
    /* match found */
    break;
    }
   } // end match mode
  /* initialize the hdrwidth and hdrheight for the first init_source */
  /* precompute the crop x y value for each frame */
  if (!spca50x->method)
    {
      /* nothing todo hardware found stream */
      cropx1 = cropx2 = cropy1 = cropy2 = x = y = 0;
      spca50x->hdrwidth = spca50x->width;
      spca50x->hdrheight = spca50x->height;
    }
  if (spca50x->method & 0x01)
    {
      /* cropping method */
      if (spca50x->hdrwidth > spca50x->width)
	{

	  crop = (spca50x->hdrwidth - spca50x->width);
	  if (spca50x->cameratype == JPEG || spca50x->cameratype == JPGH || spca50x->cameratype == JPGS)
	    crop = crop >> 4;
	  cropx1 = crop >> 1;
	  cropx2 = cropx1 + (crop % 2);
	}
      else
	{
	  cropx1 = cropx2 = 0;
	}
      if (spca50x->hdrheight > spca50x->height)
	{
	  crop = (spca50x->hdrheight - spca50x->height);
	  if (spca50x->cameratype == JPEG)
	    crop = crop >> 4;
	  if (spca50x->cameratype == JPGH || spca50x->cameratype == JPGS)
	    crop = crop >> 3;
	  cropy1 = crop >> 1;
	  cropy2 = cropy1 + (crop % 2);
	}
      else
	{
	  cropy1 = cropy2 = 0;
	}
    }
  if (spca50x->method & 0x02)
    {
      /* what can put here for div method */
    }
  if (spca50x->method & 0x04)
    {
      /* and here for mult */
    }
  PDEBUG (2, "Found code %d method %d", spca50x->mode, spca50x->method);
  PDEBUG (2, "Soft Win width height %d x %d", spca50x->width,
	  spca50x->height);
  PDEBUG (2, "Hard Win width height %d x %d", spca50x->hdrwidth,
	  spca50x->hdrheight);

  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    {
      spca50x->frame[i].method = spca50x->method;
      spca50x->frame[i].cameratype = spca50x->cameratype;
      spca50x->frame[i].cropx1 = cropx1;
      spca50x->frame[i].cropx2 = cropx2;
      spca50x->frame[i].cropy1 = cropy1;
      spca50x->frame[i].cropy2 = cropy2;
      spca50x->frame[i].x = x;
      spca50x->frame[i].y = y;
      spca50x->frame[i].hdrwidth = spca50x->hdrwidth;
      spca50x->frame[i].hdrheight = spca50x->hdrheight;
      spca50x->frame[i].width = spca50x->width;
      spca50x->frame[i].height = spca50x->height;
      spca50x->frame[i].scanlength = spca50x->width * spca50x->height * 3 / 2;
      // ?? assumes 4:2:0 data
    }

  return 0;  
}
static inline int
spca50x_set_mode (struct usb_spca50x *spca50x,
		  int index, __u16 ext_modes[][6])
{
  struct usb_device *dev = spca50x->dev;
  int i;
  int mode = 0;
  int method = 0;
  int crop = 0, cropx1 = 0, cropx2 = 0, cropy1 = 0, cropy2 = 0, x = 0, y = 0;
  if (spca50x->bridge == BRIDGE_ETOMS){
	spca50x->mode = (ext_modes[index][2] & 0x0f);
    }
  else if (spca50x->bridge == BRIDGE_TV8532)
    {
      mode = (ext_modes[index][2]) & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      spca50x->mode = mode;
    }
  else if (spca50x->bridge == BRIDGE_CX11646)
    {
      mode = (ext_modes[index][2]) & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      spca50x->mode = mode;
    }
#if 0
  else if (spca50x->bridge == BRIDGE_ZC3XX)
    {
      mode = (ext_modes[index][2]) & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      spca50x->mode = mode;
    }
  else if (spca50x->bridge == BRIDGE_SONIX ) 	
    {
      mode = (ext_modes[index][2] & 0x0f);
      spca50x->mode = mode;
    }
#endif
  else if (spca50x->bridge == BRIDGE_SN9C102P )
    {
      mode = (ext_modes[index][2]) & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      spca50x->mode = mode;
    }
  else if (spca50x->bridge == BRIDGE_SPCA506)
    {
      spca506_Setsize (spca50x, ext_modes[index][2], ext_modes[index][3],
		       ext_modes[index][4]);

    }
  else if (spca50x->bridge == BRIDGE_SPCA505)
    {
      spca50x_reg_write (dev, SPCA50X_REG_COMPRESS, 0x0, ext_modes[index][2]);
      spca50x_reg_write (dev, SPCA50X_REG_COMPRESS, 0x6, ext_modes[index][3]);
      spca50x_reg_write (dev, SPCA50X_REG_COMPRESS, 0x7, ext_modes[index][4]);

    }
  else if (spca50x->bridge == BRIDGE_SPCA501)
    {
      mode = (ext_modes[index][2]) & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      spca50x_reg_write (dev, SPCA50X_REG_USB, 0x6, ext_modes[index][3]);
      spca50x_reg_write (dev, SPCA50X_REG_USB, 0x7, ext_modes[index][4]);
    }
  else if (spca50x->bridge == BRIDGE_SPCA508)
    {
      spca50x_reg_write (dev, 0, 0x8500, ext_modes[index][2]);	// mode
      spca50x_reg_write (dev, 0, 0x8700, ext_modes[index][3]);	// clock
    }
#if 0
  else if (spca50x->bridge == BRIDGE_SPCA561)
    {
      spca50x->mode = (ext_modes[index][2]) & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      //spca50x_reg_write (dev, 0, 0x8500, mode);                     // mode
      //spca50x_reg_write (dev, 0, 0x8700, ext_modes[index][3]);      // clock
    }
#endif
  else if (spca50x->bridge == BRIDGE_SPCA500)
    {
      mode = (ext_modes[index][2]) & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      x = ext_modes[index][3] & 0x0F;
      y = ext_modes[index][4] & 0x0F;
      /* Experimental Wait until Marek Test 
         // set x multiplier
         spca50x_reg_write (dev, 0, 0x8001,ext_modes[index][3]);
         // set y multiplier 
         spca50x_reg_write (dev, 0, 0x8002,ext_modes[index][4]);
         // use compressed mode, VGA, with mode specific subsample 
         spca50x_reg_write (dev, 0, 0x8003,((ext_modes[index][2] & 0x0f) << 4));
       */
    }
  else if (spca50x->bridge == BRIDGE_SPCA504)
    {
      mode = (ext_modes[index][2] + 3) & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      x = ext_modes[index][3] & 0x0F;
      y = ext_modes[index][4] & 0x0F;
      if (spca50x->desc == AiptekMiniPenCam13)
	{
	  /* spca504a aiptek */
	  spca504A_acknowledged_command (spca50x, 0x8, mode, 0,
					 (0x80 | (mode & 0x0F)), 1);
	  spca504A_acknowledged_command (spca50x, 1, 3, 0, 0x9F, 0);
	}
      else
	{
	  spca504_acknowledged_command (spca50x, 0x8, mode, 0);
	}
    }
  else if (spca50x->bridge == BRIDGE_SPCA504_PCCAM600)
    {
      spca50x_reg_write (spca50x->dev, 0xa0, (0x0500 | (ext_modes[index][2] & 0x0F)), 0x0);	// capture mode
      spca50x_reg_write (spca50x->dev, 0x20, 0x1, (0x0500 | (ext_modes[index][2] & 0x0F)));	// snapshot mode
    }
  else if (spca50x->bridge == BRIDGE_SPCA504B)
    {
      mode = ext_modes[index][2] & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      x = ext_modes[index][3] & 0x0F;
      y = ext_modes[index][4] & 0x0F;
      spca504B_SetSizeType (spca50x, mode, 6);
    }
  else if (spca50x->bridge == BRIDGE_SPCA533)
    {
      mode = ext_modes[index][2] & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      x = ext_modes[index][3] & 0x0F;
      y = ext_modes[index][4] & 0x0F;
      if ((spca50x->desc == MegapixV4)
	  || (spca50x->desc == LogitechClickSmart820))
	{
	  // megapix specific stuff
	  spca533_Megapix (spca50x);
	}
      else
	{
	  spca504B_SetSizeType (spca50x, mode, 6);
	}
    }
  else if (spca50x->bridge == BRIDGE_SPCA536)
    {
      mode = ext_modes[index][2] & 0x0F;
      method = (ext_modes[index][2] & 0xF0) >> 4;
      spca536_SetSizeType (spca50x, mode, 6);
    }
  spca50x->pipe_size = ext_modes[index][5];
  spca50x->method = method;
  /* initialize the hdrwidth and hdrheight for the first init_source */
  if (!(spca50x->hdrwidth) || !(spca50x->hdrheight))
    {

      spca50x->hdrwidth = spca50x->width;
      spca50x->hdrheight = spca50x->height;
    }
  /* precompute the crop x y value for each frame */
  if (!method)
    {
      /* nothing todo hardware found stream */
      cropx1 = cropx2 = cropy1 = cropy2 = x = y = 0;
    }
  if (method & 0x01)
    {
      /* cropping method */
      if (spca50x->hdrwidth > spca50x->width)
	{

	  crop = (spca50x->hdrwidth - spca50x->width);
	  if (spca50x->cameratype == JPEG || spca50x->cameratype == JPGH || spca50x->cameratype == JPGS)
	    crop = crop >> 4;
	  cropx1 = crop >> 1;
	  cropx2 = cropx1 + (crop % 2);
	}
      else
	{
	  cropx1 = cropx2 = 0;
	}
      if (spca50x->hdrheight > spca50x->height)
	{
	  crop = (spca50x->hdrheight - spca50x->height);
	  if (spca50x->cameratype == JPEG)
	    crop = crop >> 4;
	  if (spca50x->cameratype == JPGH || spca50x->cameratype == JPGS)
	    crop = crop >> 3;
	  cropy1 = crop >> 1;
	  cropy2 = cropy1 + (crop % 2);
	}
      else
	{
	  cropy1 = cropy2 = 0;
	}
    }
  if (method & 0x02)
    {
      /* what can put here for div method */
    }
  if (method & 0x04)
    {
      /* and here for mult */
    }
  PDEBUG (2, "Found code %d method %d", mode, method);
  PDEBUG (2, "Soft Win width height %d x %d", spca50x->width,
	  spca50x->height);
  PDEBUG (2, "Hard Win width height %d x %d", spca50x->hdrwidth,
	  spca50x->hdrheight);

  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    {
      spca50x->frame[i].method = spca50x->method;
      spca50x->frame[i].cameratype = spca50x->cameratype;
      spca50x->frame[i].cropx1 = cropx1;
      spca50x->frame[i].cropx2 = cropx2;
      spca50x->frame[i].cropy1 = cropy1;
      spca50x->frame[i].cropy2 = cropy2;
      spca50x->frame[i].x = x;
      spca50x->frame[i].y = y;
      spca50x->frame[i].hdrwidth = spca50x->hdrwidth;
      spca50x->frame[i].hdrheight = spca50x->hdrheight;
      spca50x->frame[i].width = spca50x->width;
      spca50x->frame[i].height = spca50x->height;
      spca50x->frame[i].scanlength = spca50x->width * spca50x->height * 3 / 2;
      // ?? assumes 4:2:0 data
    }

  return 0;
}

/**********************************************************************
* spca50x_mode_init_regs
* Function sets up the resolution with checking if it's necessary 
***********************************************************************/
static int 
spca5xx_restartMode (struct usb_spca50x *spca50x, int width, int height, int format)
{
 int was_streaming;
 int r;
 /* Avoid changing to already selected mode */
  if (spca50x->width == width && spca50x->height == height)
    return 0;
PDEBUG (1, "Mode changing to %d,%d", width, height);

  was_streaming = spca50x->streaming;
   /* FIXME spca500 bridge is there a way to find an init like 
     Clicksmart310 ? */

  if (was_streaming)
    {
      if ((spca50x->bridge != BRIDGE_SPCA500)
	  || (spca50x->desc == LogitechClickSmart310))
	spca50x_stop_isoc (spca50x);
    }
    
  r = spca5xx_setMode(spca50x,width,height,format);
  if (r < 0)
       goto out;
  if (was_streaming)
    {
      if ((spca50x->bridge != BRIDGE_SPCA500)
	  || (spca50x->desc == LogitechClickSmart310))
	{
	  r = spca50x_init_isoc (spca50x);
	}
      else
	{
	  spcaCameraStart (spca50x);
	}
    }
out:    
  return r;
}
static int
spca50x_mode_init_regs (struct usb_spca50x *spca50x,
			int width, int height, int mode, __u16 ext_modes[][6])
{
  int i, j;
  int r;
  int was_streaming;

  /* Avoid changing to already selected mode */
  if (spca50x->width == width && spca50x->height == height)
    return 0;

  /* Can't do this yet on the CCD .. or can we ? It appears we can ! */
  /*        if(spca50x->ccd) return -ECHRNG; */

  /* find a suitable mode */
  if ((i = spca50x_find_mode_index (width, height, ext_modes)) == -EINVAL)
    return -EINVAL;
  /* keep the hardware index from the hight nibble */
  j = (i & 0xF0) >> 4;
  /* the software too */
  i = i & 0x0F;
  /* make sure that the suitable mode isn't the current mode */
  if (spca50x->width == ext_modes[i][0] && spca50x->height == ext_modes[i][1])
    return 0;

  PDEBUG (1, "Mode changing to %d,%d", width, height);
  was_streaming = spca50x->streaming;

  /* FIXME spca500 bridge is there a way to find an init like 
     Clicksmart310 ? */

  if (was_streaming)
    {
      if ((spca50x->bridge != BRIDGE_SPCA500)
	  || (spca50x->desc == LogitechClickSmart310))
	spca50x_stop_isoc (spca50x);
    }
  spca50x->hdrwidth = ext_modes[j][0];
  spca50x->hdrheight = ext_modes[j][1];
  spca50x->width = ext_modes[i][0];
  spca50x->height = ext_modes[i][1];

  spca50x_set_mode (spca50x, i, ext_modes);

  r = 0;
  if (was_streaming)
    {
      if ((spca50x->bridge != BRIDGE_SPCA500)
	  || (spca50x->desc == LogitechClickSmart310))
	{
	  r = spca50x_init_isoc (spca50x);
	}
      else
	{
	  spcaCameraStart (spca50x);
	}
    }
  return r;
}

/**********************************************************************
* spca50x_get_brightness
* Function reads the brightness from the camera
* Receives the pointer to the description structure
* returns the value of brightness
**********************************************************************/
static inline __u16
spca50x_get_brightness (struct usb_spca50x *spca50x)
{
  __u16 brightness = 0;		// value of the brightness
  if (spca50x->ccd)
    brightness = spca50x_read_i2c (spca50x, SAA7113_I2C_BASE_READ, 0xa);
  else
    {
      switch (spca50x->bridge)
	{
	case BRIDGE_SN9C102P:
	  brightness = sn9c102p_getbrightness (spca50x);
	  break;
	case BRIDGE_ETOMS:	
	  brightness = Et_getbrightness(spca50x);
	  break;
	case BRIDGE_ZC3XX:
	  brightness = zc3xx_getbrightness (spca50x);
	  break;
	case BRIDGE_SONIX:
	  brightness = sonix_getbrightness (spca50x);
	  break;
	case BRIDGE_TV8532:
	  brightness = tv8532_getbrightness (spca50x);
	  break;
	case BRIDGE_CX11646:
	  brightness = cx_getbrightness (spca50x);
	  break;
	case BRIDGE_SPCA500:
	  {
	    /* brightness is stored by the bridge as
	     * 129 -> 128, with 0 being the midpoint.
	     * v4l always keeps these values unsigned
	     * 16 bit, that is 0 to 65535.
	     * scale accordingly.
	     */
	    brightness = spca50x_reg_read (spca50x->dev, 0x00, 0x8167, 1);
	    brightness = brightness << 8;
	    break;
	  }
#if 0				// #ifdef def SPCA50X_ENABLE_EXP_BRIGHTNESS
	case BRIDGE_SPCA505:
	  brightness = spca50x_reg_read (spca50x->dev, 6, 0x51, 2);
	  break;
#endif /* SPCA50X_ENABLE_EXP_BRIGHTNESS */
	case BRIDGE_SPCA505:	//here505b
	  brightness =
	    65535 - ((spca50x_reg_read (spca50x->dev, 5, 0x01, 1) >> 2) +
		     (spca50x_reg_read (spca50x->dev, 5, 0x0, 1) << 6));

	  break;
	case BRIDGE_SPCA501:
	  {
	    brightness = spca50x_reg_read (spca50x->dev, 0x0, 0x00, 2) & 0xFF;
	    brightness -= 125;
	    brightness <<= 1;
	    break;
	  }
	case BRIDGE_SPCA536:
	  {
	    brightness = spca50x_reg_read (spca50x->dev, 0x0, 0x20f0, 2);

	    brightness = (((brightness & 0xFF) - 128) % 255) << 8;
	    break;
	  }
	case BRIDGE_SPCA533:
	case BRIDGE_SPCA504:
	case BRIDGE_SPCA504B:
	case BRIDGE_SPCA504_PCCAM600:
	  {
	    /* The chip keeps the brightness as
	     * 129 -> 128 with 0 being the midpoint.
	     * v4l always keeps these values unsigned
	     * 16 bit, that is 0 to 65535.
	     * Scale accordingly.
	     */
	    brightness = spca50x_reg_read (spca50x->dev, 0x0, 0x21a7, 2);

	    brightness = (((brightness & 0xFF) - 128) % 255) << 8;
	    break;
	  }
	case BRIDGE_SPCA561:
	  break;
#ifdef SPCA50X_ENABLE_EXP_BRIGHTNESS
	case BRIDGE_SPCA508:
	  brightness = spca50x_reg_read (spca50x->dev, 0, 0x8651, 1);
	  brightness = brightness << 8;
	  break;
#endif /* SPCA50X_ENABLE_EXP_BRIGHTNESS */
	case BRIDGE_MR97311:
	   brightness = 0x14;
	  break;
	default:
	  {
	    brightness = 0;
	    break;
	  }
	}
    }
  return brightness;
}

/**********************************************************************
* spca50x_set_brightness
* Function sets the brightness to the camera
* Receives the pointer to the description structure 
* and brightness value
**********************************************************************/
static inline void
spca50x_set_brightness (struct usb_spca50x *spca50x, __u8 brightness)
{
  if (spca50x->ccd)
    spca50x_write_i2c (spca50x, SAA7113_I2C_BASE_WRITE, 0xa, brightness);
  else
    {
      switch (spca50x->bridge)
	{
	case BRIDGE_SN9C102P:
	spca50x->brightness = brightness << 8;
	  sn9c102p_setbrightness (spca50x);
	break;
	case BRIDGE_ETOMS:
	  spca50x->brightness = brightness << 8;
	  Et_setbrightness (spca50x);
	  break;
	case BRIDGE_ZC3XX:
	  spca50x->brightness = brightness << 8;
	  zc3xx_setbrightness (spca50x);
	  break;
	case BRIDGE_SONIX:
	  spca50x->brightness = brightness << 8;
	  sonix_setbrightness (spca50x);
	  break;
	case BRIDGE_TV8532:
	  spca50x->brightness = brightness << 8;
	  tv8532_setbrightness (spca50x);
	  break;
	case BRIDGE_CX11646:
	  spca50x->brightness = brightness << 8;
	  cx_setbrightness (spca50x);
	  break;
	case BRIDGE_SPCA500:
	  {
	    /* 0 - 255 with zero at 128 */
	    spca50x_reg_write (spca50x->dev, 0x00, 0x8167, brightness);
	    break;
	  }
#if 0				//#ifdef SPCA50X_ENABLE_EXP_BRIGHTNESS
	case BRIDGE_SPCA505:
	  {
	    spca50x_reg_write (spca50x->dev, 6, 0x51, brightness);
	    spca50x_reg_write (spca50x->dev, 6, 0x52, brightness);
	    spca50x_reg_write (spca50x->dev, 6, 0x53, brightness);
	    break;
	  }
#endif /* SPCA50X_ENABLE_EXP_BRIGHTNESS */
	case BRIDGE_SPCA505:	//here505b
	  {
	    // brightness
	    spca50x_reg_write (spca50x->dev, 5, 0x0, (255 - brightness) >> 6);
	    spca50x_reg_write (spca50x->dev, 5, 0x01,
			       (255 - brightness) << 2);

	    break;
	  }
	case BRIDGE_SPCA501:
	  {
	    spca50x_reg_write (spca50x->dev, 0x0, 0x00,
			       (brightness >> 1) + 125);
	    break;
	  }
	case BRIDGE_SPCA536:
	  {
	    /* 0 - 255 with zero at 128 */
	    spca50x_reg_write (spca50x->dev, 0x0, 0x20f0, brightness);
	    break;
	  }
	case BRIDGE_SPCA533:
	case BRIDGE_SPCA504:
	case BRIDGE_SPCA504B:
	case BRIDGE_SPCA504_PCCAM600:
	  {
	    /* 0 - 255 with zero at 128 */
	    spca50x_reg_write (spca50x->dev, 0x0, 0x21a7, brightness);
	    break;
	  }
	case BRIDGE_SPCA561:
	  break;
#ifdef SPCA50X_ENABLE_EXP_BRIGHTNESS

	case BRIDGE_SPCA508:
	  {
	    spca50x_reg_write (spca50x->dev, 0, 0x8651, brightness);
	    spca50x_reg_write (spca50x->dev, 0, 0x8652, brightness);
	    spca50x_reg_write (spca50x->dev, 0, 0x8653, brightness);
	    spca50x_reg_write (spca50x->dev, 0, 0x8654, brightness);
	    /* autoadjust is set to 0 to avoid doing repeated brightness
	       calculation on the same frame. autoadjust is set to 0
	       when a new frame is ready. */
	    autoadjust = 0;
	    break;
	  }
#endif /* SPCA50X_ENABLE_EXP_BRIGHTNESS */
	case BRIDGE_MR97311:
	break;
	default:
	  {
	    break;
	  }
	}
    }
}

/**********************************************************************
* spca50x_get_whiteness
* Function reads the "whiteness parameter" from the camera
* Actually "whiteness" is a parameter that close to brighness
* but currently ununderstanded
* Receives the pointer to the description structure
* returns the value of whiteness
**********************************************************************/
static inline __u16
spca50x_get_whiteness (struct usb_spca50x *spca50x)
{
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
  __u16 whiteness = 0;		//the "whiteness" parameter
  __u16 br = 0;			//the brightness value
  int i;			// just an index variable
  __u16 mask;			//the bit mask to set bit in p.contrast
  __u8 br_table[] = { 128, 64, 32, 16, 0 };	//the brightness table

  //this parameter is known for spca501 only
  if (spca50x->bridge != BRIDGE_SPCA501)
    return 0;

  /* Getting "whiteness" (strange parameters in TG 0x0D */
  br = spca50x_reg_read (spca50x->dev, 0x0, 0x0d, 2);
  for (i = 0, mask = 1; br_table[i]; i++, mask <<= 1)
    {
      if (br & br_table[i])
	whiteness |= mask;
    }
  whiteness <<= 4;
  return whiteness;
#else /* SPCA50X_ENABLE_EXPERIMENTAL */
  return 0;
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */
}

/**********************************************************************
* spca50x_set_whiteness
* Function sets the "whiteness parameter" from the camera
* Actually "whiteness" is a parameter that close to brighness
* but currently ununderstanded
* Receives the pointer to the description structure 
* and whiteness value
**********************************************************************/
static inline void
spca50x_set_whiteness (struct usb_spca50x *spca50x, __u8 whiteness)
{
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
  __u8 br_high = 0;		//the contrast value
  __u8 br_out = 0;		// contrast output value
  int i;			// just an index variable
  __u16 mask;			//the bit mask to set bit in contrast
  __u8 br_table[] = { 128, 64, 32, 16, 0 };	//the brightness table


  //this parameter is known for spca501 only
  if (spca50x->bridge != BRIDGE_SPCA501)
    return;

  PDEBUG (3, "Setting whiteness %d", whiteness);
  br_high = whiteness >> 4;
  for (i = 0, mask = 1; br_table[i]; i++, mask <<= 1)
    {
      if (br_high & mask)
	br_out |= br_table[i];
    }
  PDEBUG (3, "Writing whiteness %d", br_out);
  spca50x_reg_write (spca50x->dev, 0x0, 0x0d, br_out);
#else /* SPCA50X_ENABLE_EXPERIMENTAL */
  return;
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */
}

/**********************************************************************
* autobrightness
* Function tries to control brightness
* Receives the pointer to camera descriptor and 
* target average luminance
***********************************************************************/
static inline void
autobrightness (struct usb_spca50x *spca50x)
{
  __u8 cur_lum = 0;		//The current luminance
  __u8 brightness = 0;
  char br_set = 0;		//If the brightness should be set
  __u8 lum = spca50x->lum_level;	//The desired luminance level
  __u8 delta = lum >> 3;	//overshooting that we shouldn't care
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
  __u8 whiteness = 0;		//whiteness level
  char wh_set = 0;		//If whiteness must be set
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */

  if (in_interrupt ())
    {
      PDEBUG (2, "In intr");
      return;
    }
  ////
  /*
   * This algorithm may be dangerous. We MUST be sure that the 
   * !spca50x->streaming will be set to 0 BEFORE kfree(spca50x)
   */
  ////
  if (!spca50x->streaming)	//No isoc
    return;
  cur_lum = get_avg_lum (spca50x);
  brightness = spca50x->brightness >> 8;
  if (cur_lum < (lum - delta))
    {
      if (brightness < 255)
	{
	  brightness++;
	  br_set = 1;
	}
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
      else
	{
	  whiteness = spca50x->whiteness >> 12;
	  if (whiteness < 15)
	    {
	      whiteness++;
	      wh_set = 1;
	    }
	}
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */
    }
  if (cur_lum > (lum + delta) && brightness > 0)
    {
      brightness--;
      br_set = 1;
    }
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
  if ((spca50x->nstable == NSTABLE_MAX ||
       spca50x->nunstable == NUNSTABLE_MAX) && brightness < MIN_BRIGHTNESS)
    {
      whiteness = spca50x->whiteness >> 12;
      if (whiteness > 0)
	{
	  whiteness--;
	  wh_set = 1;
	}
    }
  if (wh_set)
    {
      spca50x_set_whiteness (spca50x, whiteness << 4);
      spca50x->whiteness = whiteness << 12;
      spca50x->nstable = 0;
      spca50x->nunstable = 0;
      return;			//if we have set whiteness, we needn't care about brightness
    }
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */
  if (br_set)
    {
      spca50x_set_brightness (spca50x, brightness);
      spca50x->brightness = brightness << 8;
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
      if (spca50x->nunstable < NUNSTABLE_MAX)
	spca50x->nunstable++;
      spca50x->nstable = 0;
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */
    }
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
  else
    {
      if (spca50x->nstable < NSTABLE_MAX)
	spca50x->nstable++;
      else
	spca50x->nunstable = 0;
    }
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */
}

static void
auto_bh (void *data)
{
  struct usb_spca50x *spca50x = (struct usb_spca50x *) data;

  autobrightness (spca50x);
  spca50x->bh_requested = 0;	/* bottom half process finished */
}


/**********************************************************************
 *
 * SPCA50X data transfer, IRQ handler
 *
 **********************************************************************/

#ifdef SPCA50X_ENABLE_OSD

static void
spca50x_osd_text (struct spca50x_frame *frame, int x, int y, char *text)
{
  int I;
  int J;
  int K;
  int limit;
  limit = strlen (text);

  for (I = 0; I < limit; I++)
    {
      /* Offest into font table for first byte of character data */
      int charBase = 8 * text[I];
      /* Frame pointer */
      char *fp = frame->data + I * 3 * 8 + y * 8 * frame->width * 3 + x * 3;
      /* pointer to character data */
      char *font_char = ((char *) (osd_font->data)) + charBase;
      for (J = 0; J < 8; J++)
	{
	  int line = font_char[J];
	  for (K = 0; K < 8; K++)
	    {
	      int level = (line & 0x80) ? 255 : 0;
	      *(fp + 3 * K) = level;	// R
	      *(fp + 3 * K + 1) = level;	// G
	      *(fp + 3 * K + 2) = level;	// B
	      line <<= 1;
	    }
	  fp += frame->width * 3;	// skip a line of RGB
	}
    }
}

/* On screen display */
static int
spca50x_osd_render (struct spca50x_frame *frame, struct timeval *ts)
{
  char seq[40];
  int frameSeq;

  if (!osd_font)
    {
      osd_font = fbcon_find_font ("VGA8x8");
    }

  /* No font available */
  if (!osd_font || osd_font->data == NULL)
    return -EBFONT;

  frameSeq = frame->seq;
  if (frameSeq < 0)
    frameSeq += 256;
  sprintf (seq, "Frame: %3.3d ", frameSeq);

  spca50x_osd_text (frame, 0, 0, seq);

  sprintf (seq, "%dx%d", frame->width, frame->height);

  spca50x_osd_text (frame, 0, 1, seq);

  return 0;
}

#endif /* SPCA50X_ENABLE_OSD */

static struct spca50x_frame *
spca50x_next_frame (struct usb_spca50x *spca50x, unsigned char *cdata)
{
  int iFrameNext;
  struct spca50x_frame *frame = NULL;

  /* Cycle through the frame buffer looking for a free frame to overwrite */
  iFrameNext = (spca50x->curframe + 1) % SPCA50X_NUMFRAMES;
  while (frame == NULL && iFrameNext != (spca50x->curframe))
    {
      if (spca50x->frame[iFrameNext].grabstate == FRAME_READY ||
	  spca50x->frame[iFrameNext].grabstate == FRAME_UNUSED ||
	  spca50x->frame[iFrameNext].grabstate == FRAME_ERROR)
	{
	  spca50x->curframe = iFrameNext;
	  frame = &spca50x->frame[iFrameNext];
	  break;
	}
      else
	{
	  iFrameNext = (iFrameNext + 1) % SPCA50X_NUMFRAMES;
	}
    }

  if (frame == NULL)
    {
      PDEBUG (3, "Can't find a free frame to grab into...using next. "
	      "This is caused by the application not reading fast enough.");
      spca50x->curframe = (spca50x->curframe + 1) % SPCA50X_NUMFRAMES;
      frame = &spca50x->frame[spca50x->curframe];
    }

  frame->grabstate = FRAME_GRABBING;

  /* Record the frame sequence number the camera has told us */
  if (cdata)
    {
      switch (spca50x->bridge)
	{
	case BRIDGE_SPCA500:
	  frame->seq = cdata[SPCA500_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA501:
	  frame->seq = cdata[SPCA501_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA504:
	  frame->seq = cdata[SPCA50X_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA504_PCCAM600:
	  frame->seq = cdata[SPCA50X_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA504B:
	  frame->seq = cdata[SPCA50X_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA505:
	  frame->seq = cdata[SPCA50X_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA506:
	  frame->seq = cdata[SPCA50X_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA508:
	  frame->seq = cdata[SPCA508_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA533:
	  frame->seq = cdata[SPCA533_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA561:
	  frame->seq = cdata[SPCA561_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_SPCA536:
	  frame->seq = cdata[SPCA536_OFFSET_FRAMSEQ];
	  break;
	case BRIDGE_CX11646:  
	case BRIDGE_SONIX:
	case BRIDGE_ZC3XX: 
	case BRIDGE_TV8532: 
	case BRIDGE_ETOMS:
	case BRIDGE_SN9C102P:
	case BRIDGE_MR97311: 
	 frame->seq=00;
	 break;
	}
    }
  if (spca50x->pictsetting.change)
    {
      memcpy (&frame->pictsetting, &spca50x->pictsetting,
	      sizeof (struct pictparam));
      /* reset flag change */
      spca50x->pictsetting.change = 0;
      PDEBUG (2, "Picture setting change Pass to decoding   ");
    }
  /* Reset some per-frame variables */
  frame->highwater = frame->data;
  frame->scanstate = STATE_LINES;

  frame->last_packet = -1;
  frame->totlength = 0;
  spca50x->packet = 0;
  return frame;
}

/* Tasklet function to decode */

void
outpict_do_tasklet (unsigned long ptr)
{
  int err;
  struct spca50x_frame *taskletframe = (struct spca50x_frame *) ptr;

  taskletframe->scanlength = taskletframe->highwater - taskletframe->data;

  PDEBUG (2, "Tasklet ask spcadecoder hdrwidth %d hdrheight %d method %d",
	  taskletframe->hdrwidth, taskletframe->hdrheight,
	  taskletframe->method);
  err = spca50x_outpicture (taskletframe);
  if (err != 0)
    {
      PDEBUG (0, "frame decoder failed (%d)", err);
      taskletframe->grabstate = FRAME_ERROR;
    }
  else
    {
      taskletframe->grabstate = FRAME_DONE;
    }
  if (waitqueue_active (&taskletframe->wq))
    wake_up_interruptible (&taskletframe->wq);

}

/* ******************************************************************
* spca50x_move_data
* Function serves for moving data from USB transfer buffers
* to internal driver frame buffers.
******************************************************************* */
static int
spca50x_move_data (struct usb_spca50x *spca50x, struct urb *urb)
{
  unsigned char *cdata;		//Pointer to buffer where we do store next packet
  unsigned char *pData;		//Pointer to buffer where we do store next packet
  int i;
  struct spca50x_frame *frame;	//Pointer to frame data
  int iPix;			//Offset of pixel data in the ISO packet

  int totlen = 0;
  int tv8532 = 0;

  for (i = 0; i < urb->number_of_packets; i++)
    {
      int datalength = urb->iso_frame_desc[i].actual_length;
      int st = urb->iso_frame_desc[i].status;
      int sequenceNumber;
      int seqframe=0;
      int sof;
      int j, p;
      /* PDEBUG(5,"Packet data [%d,%d,%d]", datalength, st,
         urb->iso_frame_desc[i].offset); */

      urb->iso_frame_desc[i].actual_length = 0;
      urb->iso_frame_desc[i].status = 0;

      cdata = ((unsigned char *) urb->transfer_buffer) +
	urb->iso_frame_desc[i].offset;

      /* Check for zero length block or no selected frame buffer */
      if (!datalength || spca50x->curframe == -1)
	{
	  tv8532 = 0;
	  continue;
	}
      PDEBUG (5, "Packet data [%d,%d,%d] Status: %d", datalength, st,
	      urb->iso_frame_desc[i].offset, st);

      if (st)
	PDEBUG (2, "data error: [%d] len=%d, status=%d", i, datalength, st);

      frame = &spca50x->frame[spca50x->curframe];
      totlen = frame->totlength;
#ifdef SPCA50X_ENABLE_RAWPROCENTRY
      if (spca50x->rawBuffer != NULL)
	{
	  /* check that there's enough space in the buffer */
	  if ((spca50x->rawBufferMax - spca50x->rawBufferSize) <
	      (datalength + 5))
	    {
	      PDEBUG (3, "raw proc entry buffer is full");
	    }
	  else
	    {
	      strncpy (spca50x->rawBuffer + spca50x->rawBufferSize, "FRAME",
		       5);
	      memcpy (spca50x->rawBuffer + spca50x->rawBufferSize + 5, cdata,
		      datalength);
	      spca50x->rawBufferSize += datalength + 5;
	    }
	}
#endif /* SPCA50X_ENABLE_RAWPROCENTRY */


      /* read the sequence number */
      if (spca50x->bridge == BRIDGE_SONIX ||
	  spca50x->bridge == BRIDGE_ZC3XX ||
	  spca50x->bridge == BRIDGE_CX11646 ||
	  spca50x->bridge == BRIDGE_TV8532 ||
	  spca50x->bridge == BRIDGE_ETOMS ||
	  spca50x->bridge == BRIDGE_SN9C102P ||
	  spca50x->bridge == BRIDGE_MR97311)
	{
	  if (frame->last_packet == -1)
	    {
	      /*initialize a new frame */
	      sequenceNumber = 0;
	    }
	  else
	    {
	      sequenceNumber = frame->last_packet;
	    }
	}
      else
	{
	  sequenceNumber = cdata[SPCA50X_OFFSET_SEQUENCE];
	  PDEBUG (4, "Packet start  %x %x %x %x %x", cdata[0], cdata[1],
		  cdata[2], cdata[3], cdata[4]);
	}
      /* check frame start */
      switch (spca50x->bridge)
	{
	case BRIDGE_SN9C102P:
		iPix = 0;
		sof = datalength - 64;
		if (sof < 0)
			sequenceNumber++;
		else if(cdata[sof] == 0xff && cdata[sof +1] == 0xd9){
			 sequenceNumber = 0; //start of frame
			 // copy the end of data frame
	  		memcpy (frame->highwater, cdata, sof+2);
      			frame->highwater += (sof+2);
      			totlen += (sof+2);
#if 0
			spin_lock(&spca50x->v4l_lock);
			spca50x->avg_lum = (cdata[sof+24] + cdata[sof+26]) >> 1 ;
			spin_unlock(&spca50x->v4l_lock);
			PDEBUG (5,"mean luma %d",spca50x->avg_lum );
#endif
			PDEBUG (5,
			"Sonix header packet found datalength %d totlength %d!!",
			  datalength, totlen);
			  PDEBUG(5,"%03d %03d %03d %03d %03d %03d %03d %03d",cdata[sof+24], cdata[sof+25],
			  cdata[sof+26] , cdata[sof+27], cdata[sof+28], cdata[sof+29],
			  cdata[sof+30], cdata[sof+31]);	
			// setting to skip the rest of the packet
			spca50x->header_len = datalength;
			} else
				sequenceNumber++;
	break;
	case BRIDGE_ETOMS:
	{
	   iPix = 8;
	   seqframe= cdata[0] & 0x3f;
	datalength = (int)(((cdata[0] & 0xc0) << 2) | cdata[1]);
	if (seqframe == 0x3f){
	  PDEBUG(5, "Etoms header packet found datalength %d totlength %d!!",datalength,totlen);
	  PDEBUG(5,"Etoms G %d R %d G %d B %d",cdata[2],cdata[3],cdata[4],cdata[5]);
	  sequenceNumber = 0; //start of frame
	  spca50x->header_len = 30;
	} else {
	if (datalength){
		sequenceNumber++;
	 } else {
	  /* Drop Packet */
	   continue;
	 }
	}
	}
	break;
	case BRIDGE_CX11646:
	  {
	    iPix = 0;
	    if (cdata[0] == 0xFF && cdata[1] == 0xD8)
	      {
		sequenceNumber = 0;
		spca50x->header_len = 2;
		PDEBUG (5,
			"Cx11646 header packet found datalength %d totlength %d!!",
			datalength, totlen);
	      }
	    else
	      {
		sequenceNumber++;
	      }

	  }
	  break;
	case BRIDGE_ZC3XX:
	  {
	    iPix = 0;
	    if (cdata[0] == 0xFF && cdata[1] == 0xD8)
	      {
		sequenceNumber = 0;
		spca50x->header_len = 2;	//18 remove 0xff 0xd8;
		PDEBUG (5,
			"Zc301 header packet found datalength %d totlength %d!!",
			datalength, totlen);
	      }
	    else
	      {
		sequenceNumber++;
	      }

	  }
	  break;
	case BRIDGE_SONIX:
	  {
	    iPix = 0;
	    sof = 0;
	    j = 0;
	    p = 0;
	    if (datalength < 24)
	      {
		if (datalength > 6)
		  {
		    j = datalength - 6;
		  }
		else
		  {
		    j = 0;
		  }
		if (j > 0)
		  {
		    for (p = 0; p < j; p++)
		      {
			if ((cdata[0 + p] == 0xFF) && (cdata[1 + p] == 0xFF)
			    && (cdata[2 + p] == 0x00)
			    && (cdata[3 + p] == 0xC4)
			    && (cdata[4 + p] == 0xC4)
			    && (cdata[5 + p] == 0x96))
			  {
			    sof = 1;
			    break;
			  }
		      }
		  }
	      }
	    if (sof)
	      {
		sequenceNumber = 0;
		spca50x->header_len = p + 12;
		PDEBUG (5,
			"Sonix header packet found %d datalength %d totlength %d!!",
			p, datalength, totlen);
	      }
	    else
	      {
		/* drop packet */
		sequenceNumber++;
	      }

	  }
	  break;
	case BRIDGE_SPCA500:
	case BRIDGE_SPCA533:
	  {
	    iPix = 1;
	    if (sequenceNumber == SPCA50X_SEQUENCE_DROP)
	      {
		if (cdata[1] == 0x01)
		  {
		    sequenceNumber = 0;
		  }
		else
		  {
		    /* drop packet */
		    PDEBUG (5, "Dropped packet (expected seq 0x%02x)",
			    frame->last_packet + 1);
		    continue;
		  }
	      }
	    else
	      {
		sequenceNumber++;
	      }
	  }
	  break;
	case BRIDGE_SPCA536:
	  {
	    iPix = 2;
	    if (sequenceNumber == SPCA50X_SEQUENCE_DROP)
	      {
		sequenceNumber = 0;
	      }
	    else
	      {
		sequenceNumber++;
	      }
	  }
	  break;
	case BRIDGE_SPCA504:
	case BRIDGE_SPCA504B:
	case BRIDGE_SPCA504_PCCAM600:
	  {
	    iPix = 1;
	    switch (sequenceNumber)
	      {
	      case 0xfe:
		sequenceNumber = 0;
		break;
	      case SPCA50X_SEQUENCE_DROP:
		/* drop packet */
		PDEBUG (5, "Dropped packet (expected seq 0x%02x)",
			frame->last_packet + 1);
		continue;
	      default:
		sequenceNumber++;
		break;
	      }
	  }
	  break;
	case BRIDGE_TV8532:
	  {
	    //iPix = 4;     
	    iPix = 0;
	    spca50x->header_len = 0;
	    PDEBUG (1, "icm532, sequenceNumber: 0x%02x packet %d ",
		    sequenceNumber, spca50x->packet);
	    /* here we detect 0x80 */
	    if (cdata[0] == 0x80)
	      {
		/* counter is limited so we need few header for a frame :) */

		/* header 0x80 0x80 0x80 0x80 0x80 */
		/* packet  00   63  127  145  00   */
		/* sof     0     1   1    0    0   */
		/* update sequence */
		if ((spca50x->packet == 63) || (spca50x->packet == 127))
		  tv8532 = 1;
		/* is there a frame start ? */
		if (spca50x->packet >= ((spca50x->hdrheight >> 1) - 1))
		  {
		    PDEBUG (1, "SOF > %d seqnumber %d packet %d", tv8532,
			    sequenceNumber, spca50x->packet);
		    if (!tv8532)
		      {
			sequenceNumber = 0;
			spca50x->packet = 0;
		      }
		  }
		else
		  {

		    if (!tv8532)
		      {
			// Drop packet frame corrupt
			PDEBUG (1, "DROP SOF %d seqnumber %d packet %d",
				tv8532, sequenceNumber, spca50x->packet);
			frame->last_packet = sequenceNumber = 0;
			spca50x->packet = 0;
			continue;
		      }
		    tv8532 = 1;
		    sequenceNumber++;
		    spca50x->packet++;
		  }

	      }
	    else
	      {
		sequenceNumber++;
		spca50x->packet++;
	      }
	  }
	  break;
	 case BRIDGE_MR97311:
	  {
	    iPix = 0;
	    sof = 0;
	    j = 0;
	    p = 0;
	    if (datalength < 6 )
			continue;
		else
		{
		    for (p = 0; p < datalength-6; p++)
		      {
			if ((cdata[0 + p] == 0xFF) && (cdata[1 + p] == 0xFF)
			    && (cdata[2 + p] == 0x00)
			    && (cdata[3 + p] == 0xFF)
			    && (cdata[4 + p] == 0x96)
			    )
			  {
			    if((cdata[5 + p] == 0x64 ) || (cdata[5 + p] == 0x65) ||
				 (cdata[5 + p] == 0x66) || (cdata[5 + p] == 0x67) )
			    	{
			    	    sof = 1;
			    	    break;
			    	}
			  }
		      }
		  
		    if (sof)
		      {
			sequenceNumber = 0;
			spca50x->header_len = p+16;
 		       PDEBUG (5,
				"Pcam header packet found, %d datalength %d totlength %d!!",
				p, datalength, totlen);
		      }
		    else
		    {
			/* drop packet */
			sequenceNumber++;
		    }
			
		}
	  }
	  break;
	case BRIDGE_SPCA561:
	iPix = 1;
	if (sequenceNumber == SPCA50X_SEQUENCE_DROP)
	      {
		PDEBUG (3, "Dropped packet (expected seq 0x%02x)",
			frame->last_packet + 1);
		continue;
	      }
#if 0
	 if (!sequenceNumber)
	      {
	    spin_lock(&spca50x->v4l_lock);
	    spca50x->avg_lum = (75*(cdata[11]+cdata[14])+77*cdata[12]+29*cdata[13]) >> 8; 
	    spin_unlock(&spca50x->v4l_lock);
	    PDEBUG (4, "Frame %d,Win2 %02d ", cdata[4],spca50x->avg_lum);
	      }
#endif
	break;
	default:
	  {
	    iPix = 1;
	    /* check if this is a drop packet */
	    if (sequenceNumber == SPCA50X_SEQUENCE_DROP)
	      {
		PDEBUG (3, "Dropped packet (expected seq 0x%02x)",
			frame->last_packet + 1);
		continue;
	      }
	  }
	  break;
	}

      PDEBUG (3, "spca50x: Packet seqnum = 0x%02x.  curframe=%2d",
	      sequenceNumber, spca50x->curframe);
      pData = cdata;

      /* Can we find a frame start */
      if (sequenceNumber == 0)
	{
	  totlen = 0;
	  iPix = spca50x->header_len;
	  PDEBUG (3, "spca50x: Found Frame Start!, framenum = %d",
		  spca50x->curframe);
	  // Start of frame is implicit end of previous frame
	  // Check for a previous frame and finish it off if one exists
	  if (frame->scanstate == STATE_LINES){
#ifdef SPCA50X_ENABLE_OSD
	      struct timeval *ts;
	      PDEBUG (4, "Frame end, curframe = %d, has %d bytes",
		      spca50x->curframe, (frame->highwater - frame->data));
	      ts = (struct timeval *) (frame->data + MAX_FRAME_SIZE);
	      do_gettimeofday (ts);
	      if (osd)
		spca50x_osd_render (frame, ts);

#endif /* SPCA50X_ENABLE_OSD */
	      if (frame->format != VIDEO_PALETTE_RAW_JPEG)
		{
		  /* Decode the frame */
		 
		tasklet_init (&spca50x->spca5xx_tasklet, outpict_do_tasklet,
				(unsigned long) frame);
		tasklet_schedule (&spca50x->spca5xx_tasklet);
		}
	      else
		{
		  /* RAW DATA stream */
		  frame->grabstate = FRAME_DONE;
		  if (waitqueue_active (&frame->wq))
		    wake_up_interruptible (&frame->wq);
		}
	      // If someone decided to wait for ANY frame - wake him up 
	      if (waitqueue_active (&spca50x->wq))
		wake_up_interruptible (&spca50x->wq);
	      frame = spca50x_next_frame (spca50x, cdata);
	    }
	  else
	    frame->scanstate = STATE_LINES;
	}

      /* Are we in a frame? */
      if (frame == NULL || frame->scanstate != STATE_LINES)
	continue;
      if (sequenceNumber != frame->last_packet + 1 &&
	  frame->last_packet != -1)
	{
	  /* Note, may get one of these for the first packet after opening */
	  PDEBUG (2, "Out of order packet, last = %d, this = %d",
		  frame->last_packet, sequenceNumber);

	}
      frame->last_packet = sequenceNumber;

      /* This is the real conversion of the raw camera data to BGR for V4L */
      if (!(spca50x->bridge == BRIDGE_TV8532 ||
      		spca50x->bridge == BRIDGE_ETOMS))
		datalength -= iPix;	// correct length for packet header

      pData = cdata + iPix;	// Skip packet header (1 or 10 bytes)

      // Consume data
      PDEBUG (5, "Processing packet seq  %d,length %d,totlength %d",
	      frame->last_packet, datalength, frame->totlength);
      /* this copy consume input data from the isoc stream */
      if ((datalength > 0) && (datalength <= 0x3ff)){
      memcpy (frame->highwater, pData, datalength);
      frame->highwater += datalength;
      totlen += datalength;
      }
      	
      frame->totlength = totlen;
#ifdef SPCA50X_ENABLE_EXP_BRIGHTNESS
      /* autoadjust is set to 1 to so now autobrightness will be
         calculated frome this frame. autoadjust will be set to 0 when
         autobrightness has been corrected if needed. */
      autoadjust = 1;
#endif /* SPCA50X_ENABLE_EXP_BRIGHTNESS */


    }
  return totlen;
}


/****************************************************************************
 *
 * Buffer management
 *
 ***************************************************************************/
static int
spca50x_alloc (struct usb_spca50x *spca50x)
{
  int i;

  PDEBUG (4, "entered");
  down (&spca50x->buf_lock);
  spca50x->tmpBuffer = rvmalloc (MAX_FRAME_SIZE);
  if (spca50x->buf_state == BUF_ALLOCATED)
    goto out;

  spca50x->fbuf = rvmalloc (SPCA50X_NUMFRAMES * MAX_DATA_SIZE);
  if (!spca50x->fbuf)
    goto error;

  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    {
      spca50x->frame[i].tmpbuffer = spca50x->tmpBuffer;
      spca50x->frame[i].decoder = &spca50x->maindecode; //connect each frame to the main data decoding 
      spca50x->frame[i].grabstate = FRAME_UNUSED;
      spca50x->frame[i].scanstate = STATE_SCANNING;
      spca50x->frame[i].data = spca50x->fbuf + i * MAX_DATA_SIZE;
      spca50x->frame[i].highwater = spca50x->frame[i].data;
      memset (&spca50x->frame[i].pictsetting, 0, sizeof (struct pictparam));
      PDEBUG (4, "frame[%d] @ %p", i, spca50x->frame[i].data);
    }

  for (i = 0; i < SPCA50X_NUMSBUF; i++)
    {
      spca50x->sbuf[i].data = kmalloc (FRAMES_PER_DESC *
				       MAX_FRAME_SIZE_PER_DESC, GFP_KERNEL);
      if (!spca50x->sbuf[i].data)
	goto error;
      PDEBUG (4, "sbuf[%d] @ %p", i, spca50x->sbuf[i].data);
    }
  spca50x->buf_state = BUF_ALLOCATED;
out:
  up (&spca50x->buf_lock);
  PDEBUG (4, "leaving");
  return 0;
error:
  /* FIXME: IMHO, it's better to move error deallocation code here. */

  for (i = 0; i < SPCA50X_NUMSBUF; i++)
    {

      if (spca50x->sbuf[i].data)
	{
	  kfree (spca50x->sbuf[i].data);
	  spca50x->sbuf[i].data = NULL;
	}
    }
  if (spca50x->fbuf)
    {
      rvfree (spca50x->fbuf, SPCA50X_NUMFRAMES * MAX_DATA_SIZE);
      spca50x->fbuf = NULL;
    }
  if (spca50x->tmpBuffer)
    {
      rvfree (spca50x->tmpBuffer, MAX_FRAME_SIZE);
      spca50x->tmpBuffer = NULL;
    }

  spca50x->buf_state = BUF_NOT_ALLOCATED;
  up (&spca50x->buf_lock);
  PDEBUG (1, "errored");
  return -ENOMEM;
}

static void
spca5xx_dealloc (struct usb_spca50x *spca50x )
{
  int i;
  PDEBUG (2, "entered dealloc");
  down (&spca50x->buf_lock);
  if (spca50x->fbuf)
    {
      rvfree (spca50x->fbuf, SPCA50X_NUMFRAMES * MAX_DATA_SIZE);
      spca50x->fbuf = NULL;
      for (i = 0; i < SPCA50X_NUMFRAMES; i++)
	spca50x->frame[i].data = NULL;
    }

  if (spca50x->tmpBuffer)
    {
      rvfree (spca50x->tmpBuffer, MAX_FRAME_SIZE);
      spca50x->tmpBuffer = NULL;
    }

  for (i = 0; i < SPCA50X_NUMSBUF; i++)
    {
      if (spca50x->sbuf[i].data)
	{
	  kfree (spca50x->sbuf[i].data);
	  spca50x->sbuf[i].data = NULL;
	}
    }

  PDEBUG (2, "buffer memory deallocated");
  spca50x->buf_state = BUF_NOT_ALLOCATED;
  up (&spca50x->buf_lock);
  PDEBUG (2, "leaving dealloc");
}
/**
 * Reset the camera and send the correct initialization sequence for the
 * currently selected source
 */
static int
spca50x_init_source (struct usb_spca50x *spca50x)
{
  int err_code;
  struct usb_device *dev = spca50x->dev;
  int index;			// index in the modes table

  switch (spca50x->bridge)
    {
    case BRIDGE_SN9C102P:
      err_code = sn9c102p_init (spca50x);
      PDEBUG (0, "Initializing Sonix finished %d",err_code);
      if (err_code < 0) return err_code;
      break;
    case BRIDGE_ETOMS:
      err_code = Et_init(spca50x);
    break;
    case BRIDGE_CX11646:
      err_code = cx11646_init (spca50x);
      break;
    case BRIDGE_ZC3XX:
      err_code = zc3xx_init (spca50x);
      break;
    case BRIDGE_SONIX:
      err_code = sonix_init (spca50x);
      PDEBUG (2, "Initializing Sonix finished");
      break;
    case BRIDGE_SPCA500:
      {
	/* initialisation of spca500 based cameras is deferred */
	PDEBUG (2, "Initializing SPCA500 started");
	if (spca50x->desc == LogitechClickSmart310)
	  {
	    spca500_clksmart310_init (spca50x);
	  }
	else
	  {
	    spca500_initialise (spca50x);
	  }
	PDEBUG (2, "Initializing SPCA500 finished");
	break;
      }
    case BRIDGE_SPCA501:
      {
	PDEBUG (2, "Initializing SPCA501 started");
	if (spca50x->dev->descriptor.idVendor == 0x0506
	    && spca50x->dev->descriptor.idProduct == 0x00df)
	  {
	    /* Special handling for 3com data */
	    spca50x_write_vector (spca50x, spca501_3com_open_data);
	  }
	else if (spca50x->desc == Arowana300KCMOSCamera)
	  {
	    /* Arowana 300k CMOS Camera data */
	    spca50x_write_vector (spca50x, spca501c_arowana_open_data);
	  }
	else if (spca50x->desc == MystFromOriUnknownCamera)
	  {
	    /* UnKnow  CMOS Camera data */
	    spca50x_write_vector (spca50x, spca501c_mysterious_init_data);
	  }
	else
	  {
	    /* Generic 501 open data */
	    spca50x_write_vector (spca50x, spca501_open_data);
	  }
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
	spca50x->a_blue = spca50x_reg_read (spca50x->dev,
					    SPCA501_REG_CCDSP, SPCA501_A11,
					    2) & 0xFF;
	spca50x->a_green =
	  spca50x_reg_read (spca50x->dev, SPCA501_REG_CCDSP, SPCA501_A21,
			    2) & 0xFF;
	spca50x->a_red =
	  spca50x_reg_read (spca50x->dev, SPCA501_REG_CCDSP, SPCA501_A31,
			    2) & 0xFF;
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */
	PDEBUG (2, "Initializing SPCA501 finished");
	break;
      }
    case BRIDGE_SPCA504:

      {
	PDEBUG (2, "Opening SPCA504");

	if (spca50x->desc == AiptekMiniPenCam13)
	  {
	    /* spca504a aiptek */
	    spca50x_reg_write (spca50x->dev, 0, 0x2000, 0);
	    spca50x_reg_write (spca50x->dev, 0, 0x2883, 1);
	    spca504A_acknowledged_command (spca50x, 0x08, 6, 0, 0x86, 1);
	    spca504A_acknowledged_command (spca50x, 0x24, 0, 0, 0x9D, 1);
	    spca504A_acknowledged_command (spca50x, 1, 0x0f, 0, 0xFF, 0);
	  }
	/* setup qtable */
	err_code = spca50x_setup_qtable (spca50x,
					 0x00, 0x2800,
					 0x2840, qtable_spca504_default);
	if (err_code < 0)
	  {
	    PDEBUG (2, "spca50x_setup_qtable failed");
	    return err_code;
	  }
	break;
      }
    case BRIDGE_SPCA504_PCCAM600:
      {
	PDEBUG (2, "Opening SPCA504 (PC-CAM 600)");

	spca50x_reg_write (spca50x->dev, 0xe0, 0x0000, 0x0000);
	spca50x_reg_write (spca50x->dev, 0xe0, 0x0000, 0x0001);	// reset
	spca504_wait_status (spca50x);
	if (spca50x->desc == LogitechClickSmart420)
	  {			/* clicksmart 420 */
	    spca50x_write_vector (spca50x, spca504A_clicksmart420_open_data);
	  }
	else
	  {
	    spca50x_write_vector (spca50x, spca504_pccam600_open_data);
	  }

	err_code = spca50x_setup_qtable (spca50x,
					 0x00, 0x2800,
					 0x2840, qtable_creative_pccam);
	if (err_code < 0)
	  {
	    PDEBUG (2, "spca50x_setup_qtable failed");
	    return err_code;
	  }

	break;
      }
    case BRIDGE_SPCA536:
      {
	PDEBUG (2, "Opening SPCA536");
	spca536_init (spca50x);
	break;
      }
    case BRIDGE_SPCA533:
    case BRIDGE_SPCA504B:
      {
	PDEBUG (2, "Opening SPCA504B or SPCA533");
	spca504B_init (spca50x);
	break;
      }
    case BRIDGE_SPCA505:
      {
	PDEBUG (2, "Initializing SPCA505 for sensor type %s",
		((spca50x->ccd == 0) ? "CCD" : "Video In"));
	if (spca50x->desc == Nxultra)
	  {
	    if (!spca50x->ccd)
	      {
		spca50x_write_vector (spca50x, spca505b_open_data_ccd);
	      }
	    else
	      {
		// BOFF why a cam did have an videochip SAA7113
	      }
	  }
	else
	  {
	    if (!spca50x->ccd)
	      {
		spca50x_write_vector (spca50x, spca505_open_data_ccd);
	      }
	    else
	      {
		spca50x_write_vector (spca50x, spca505_open_data_ext);
		spca50x_write_i2c_vector (spca50x, SAA7113_I2C_BASE_WRITE,
					  spca50x_i2c_data);
		spca50x_write_vector (spca50x, spca505_open_data2);
	      }
	  }
	err_code = 0;
	err_code = spca50x_reg_read (dev, 6, (__u16) 0x16, 2);

	if (err_code < 0)
	  {
	    PDEBUG (1, "register read failed for after vector read err = %d",
		    err_code);
	    return -EIO;
	  }
	else
	  {
	    PDEBUG (3, "After vector read returns : 0x%x should be 0x0101",
		    err_code & 0xFFFF);
	  }

	err_code = spca50x_reg_write (dev, 6, (__u16) 0x16, (__u16) 0xa);
	if (err_code < 0)
	  {
	    PDEBUG (1, "register write failed for (6,0xa,0x16) err=%d",
		    err_code);
	    return -EIO;
	  }

	if (spca50x->ccd)
	  {
	    spca50x_write_i2c (spca50x, SAA7113_I2C_BASE_WRITE, 0xa,
			       bright & 0xff);
	    spca50x_write_i2c (spca50x, SAA7113_I2C_BASE_WRITE, 0xb,
			       contrast & 0xff);
	    spca50x_read_SAA7113_status (spca50x);
	  }
	else
	  {
	    spca50x_reg_write (spca50x->dev, 5, 0xc2, 18);
	  }

	break;
      }
    case BRIDGE_SPCA506:
      {
	spca506_init (spca50x);
	break;
      }
    case BRIDGE_SPCA561:
      {
	err_code = spca561_init (spca50x);
	break;
      }
    case BRIDGE_SPCA508:
      {
	spca50x_write_vector (spca50x, spca508_open_data);
	break;
      }
    case BRIDGE_TV8532:
      {
	/* To be sure that the init array is received by the cam, 
	   we send it 3 times. Yes, i'm tired of crashing, when trying to 
	   get a stream from a uninitalized cam... We probably should make
	   some fancy i2c stuff, but hey, if it ain't broken don't fix it ;)
	   BTW. it's broken! */
	err_code = tv8532_init (spca50x);

	break;
      }
    case BRIDGE_MR97311:
      break;
    default:
      {
	err ("Unimplemented bridge type");
	return -EINVAL;
      }
    }
  spca50x->norme = 0;
  spca50x->channel = 0;
  if(spca50x->bridge == BRIDGE_ZC3XX ||
     spca50x->bridge == BRIDGE_SONIX ||
     spca50x->bridge == BRIDGE_ETOMS ||
     spca50x->bridge == BRIDGE_SPCA561){
     err_code =spca5xx_setMode(spca50x,spca50x->width,spca50x->height,VIDEO_PALETTE_RGB24);
     PDEBUG(2,"set Mode return %d ", err_code);
     if (err_code < 0) return -EINVAL;
   } else {
   if ((index = spca50x_find_mode_index (spca50x->width,
					spca50x->height,
					GET_EXT_MODES (spca50x->bridge))) ==
      -EINVAL)
    {
      err ("Can't find proper mode?!!");
    }
  else
    {
      err_code = spca50x_set_mode (spca50x, (index & 0x0F),
				   GET_EXT_MODES (spca50x->bridge));
      if (err_code)
	{
	  err ("Can't set proper camera mode");
	  return -EINVAL;
	}
    }
   
   }
  return 0;
}


/****************************************************************************
 *
 * V4L API
 *
 ***************************************************************************/
static inline void spca5xx_setFrameDecoder(struct usb_spca50x *spca50x)
{ int i;
 /* configure the frame detector with default parameters */
  memset (&spca50x->pictsetting, 0, sizeof (struct pictparam));
  spca50x->pictsetting.change = 0x01;
  spca50x->pictsetting.force_rgb = force_rgb;
  spca50x->pictsetting.gamma = gamma;
  spca50x->pictsetting.OffRed = OffRed;
  spca50x->pictsetting.OffBlue = OffBlue;
  spca50x->pictsetting.OffGreen = OffGreen;
  spca50x->pictsetting.GRed = GRed;
  spca50x->pictsetting.GBlue = GBlue;
  spca50x->pictsetting.GGreen = GGreen;

  /* Set default sizes in case IOCTL (VIDIOCMCAPTURE) is not used
   * (using read() instead). */
  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    {
      spca50x->frame[i].width = spca50x->width;
      spca50x->frame[i].height = spca50x->height;
      spca50x->frame[i].cameratype = spca50x->cameratype;
      spca50x->frame[i].scanlength = spca50x->width * spca50x->height * 3 / 2;	// assumes 4:2:0 data
      spca50x->frame[i].depth = 24;

      /* Note: format reflects format of data as returned to
       * a process, not as read from camera hardware.
       * This might be a good idea for dumb programs always
       * assuming the following settings.
       */
      spca50x->frame[i].format = VIDEO_PALETTE_RGB24;
    }
}

static inline void spca5xx_initDecoder(struct usb_spca50x *spca50x)
{
  if (spca50x->cameratype == JPEG)
    init_jpeg_decoder (spca50x,2);
  if (spca50x->cameratype == JPGH 
     || spca50x->cameratype == JPGC
     || spca50x->cameratype == JPGS)
    init_jpeg_decoder (spca50x,1);
  if (spca50x->cameratype == JPGM)
    init_jpeg_decoder (spca50x,0);
  if (spca50x->bridge == BRIDGE_SONIX)
    init_sonix_decoder ();
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static int
spca5xx_open (struct inode *inode, struct file *file)
{
  struct video_device *vdev = video_devdata (file);
#else
static int
spca5xx_open(struct video_device *vdev, int flags)
{
#endif
  struct usb_spca50x *spca50x = video_get_drvdata (vdev);

  int err;
  
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_INC_USE_COUNT;
#endif
  PDEBUG (2, "opening");

  down (&spca50x->lock);
  /* sanity check disconnect, in use, no memory available */
  err = -ENODEV;
  if (!spca50x->present)
    goto out;
  err = -EBUSY;
  if (spca50x->user)
    goto out;
  err = -ENOMEM;
  if (spca50x_alloc (spca50x))
    goto out;
 /* initialize sensor and decoding */
  err = spca50x_init_source (spca50x);
  if (err != 0){
  	PDEBUG (0, "DEALLOC error on spca50x_init_source\n");
	up (&spca50x->lock);
  	spca5xx_dealloc (spca50x);
    goto out2;
    }
  spca5xx_initDecoder(spca50x);
   /* open always start in rgb24 a bug in gqcam 
    did not select the palette nor the size  
    v4l spec need that the camera always start on the last setting*/ 
  spca5xx_setFrameDecoder(spca50x);
 
  spca50x->user++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22) 
  file->private_data = vdev;
#endif 
  err = spca50x_init_isoc (spca50x);
  if (err)
    {
      PDEBUG (0, " DEALLOC error on init_Isoc\n");
      spca50x->user--;
      spca5xx_kill_isoc (spca50x);
      up (&spca50x->lock);
      spca5xx_dealloc (spca50x);
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22) 
      file->private_data = NULL;
#endif      
      goto out2;
    }

  /* Now, let's get brightness from the camera */
  spca50x->brightness = spca50x_get_brightness (spca50x) << 8;
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
  spca50x->nstable = 0;
  spca50x->nunstable = 0;
  spca50x->whiteness = spca50x_get_whiteness (spca50x) << 8;
#else /* SPCA50X_ENABLE_EXPERIMENTAL */
  spca50x->whiteness = 0;
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */

out:
  up (&spca50x->lock);
out2:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  if (err)
    MOD_DEC_USE_COUNT;
#endif

  if (err)
    {
      PDEBUG (2, "Open failed");
    }
  else
    {
      PDEBUG (2, "Open done");
    }

  return err;
}

static void inline
spcaCameraShutDown (struct usb_spca50x *spca50x)
{
  if (spca50x->dev)
    {
      if (spca50x->bridge == BRIDGE_SPCA505)

	{
	  spca50x_reg_write (spca50x->dev, 0x3, 0x3, 0x20);	// This maybe reset or power control
	  spca50x_reg_write (spca50x->dev, 0x3, 0x1, 0x0);
	  spca50x_reg_write (spca50x->dev, 0x3, 0x0, 0x1);
	  spca50x_reg_write (spca50x->dev, 0x5, 0x10, 0x1);
	  spca50x_reg_write (spca50x->dev, 0x5, 0x11, 0xF);
	}
      else if (spca50x->bridge == BRIDGE_SPCA501)
	{
	  // This maybe reset or power control
	  spca50x_reg_write (spca50x->dev, SPCA501_REG_CTLRL, 0x5, 0x0);
	}
      else if (spca50x->bridge == BRIDGE_ZC3XX)
	{
	  zc3xx_shutdown (spca50x);
	}
      else if (spca50x->bridge == BRIDGE_SPCA561)
	{
	  spca561_shutdown (spca50x);
	}
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static int
spca5xx_close (struct inode *inode, struct file *file)
{

  struct video_device *vdev = file->private_data;
#else
static void 
spca5xx_close( struct video_device *vdev)
{
#endif
  struct usb_spca50x *spca50x = video_get_drvdata (vdev);
  int i;
  PDEBUG (2, "spca50x_close");

  down (&spca50x->lock);

  spca50x->user--;
  spca50x->curframe = -1;
  if (spca50x->present)
    {
      spca50x_stop_isoc (spca50x);
      spcaCameraShutDown (spca50x);
    

  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    {
      if (waitqueue_active (&spca50x->frame[i].wq))
	wake_up_interruptible (&spca50x->frame[i].wq);
    }
  if (waitqueue_active (&spca50x->wq))
    wake_up_interruptible (&spca50x->wq);

  }  
  /* times to dealloc ressource */
   up (&spca50x->lock);
  spca5xx_dealloc (spca50x);
  PDEBUG(2,"Release ressources done");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_DEC_USE_COUNT;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
  file->private_data = NULL;
  return 0;
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static int
spca5xx_do_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
		  void *arg)
{
  struct video_device *vdev = file->private_data;
#else
static int 
spca5xx_ioctl (struct video_device *vdev, unsigned int cmd, void *arg)
{
#endif
  struct usb_spca50x *spca50x = video_get_drvdata (vdev);

  PDEBUG (2, "do_IOCtl: 0x%X", cmd);

  if (!spca50x->dev)
    return -EIO;

  switch (cmd)
    {
    case VIDIOCGCAP:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	 struct video_capability *b = arg;
#else
	 struct video_capability j ;
	 struct video_capability *b = &j;
#endif	
	PDEBUG (2, "VIDIOCGCAP %p :", b);

	memset (b, 0, sizeof (struct video_capability));
	snprintf (b->name, 32, "%s", clist[spca50x->desc].description);
	b->type = VID_TYPE_CAPTURE;
	b->channels = ((spca50x->bridge == BRIDGE_SPCA506) ? 8 : 1);
	b->audios = 0;
	b->maxwidth = spca50x->maxwidth;
	b->maxheight = spca50x->maxheight;

	b->minwidth = spca50x->minwidth;
	b->minheight = spca50x->minheight;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,b,sizeof(struct video_capability)))
	return -EFAULT;
#endif

	return 0;
      }
    case VIDIOCGCHAN:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	 struct video_channel *v = arg;
#else
	 struct video_channel k ;
	 struct video_channel *v = &k;
	 if(copy_from_user(v,arg,sizeof(struct video_channel)))
	 return -EFAULT;
#endif	 	
	switch (spca50x->bridge)
	  {
	  case BRIDGE_SPCA500:
	  case BRIDGE_SPCA501:
	  case BRIDGE_SPCA504:
	  case BRIDGE_SPCA504B:
	  case BRIDGE_SPCA504_PCCAM600:
	  case BRIDGE_SPCA508:
	  case BRIDGE_SPCA533:
	  case BRIDGE_SPCA536:
	  case BRIDGE_SPCA561:
	  case BRIDGE_SONIX:
	  case BRIDGE_ZC3XX:
	  case BRIDGE_CX11646:
	  case BRIDGE_TV8532:
	  case BRIDGE_ETOMS:
	  case BRIDGE_SN9C102P:
	  case BRIDGE_MR97311:
	    {
	      snprintf (v->name, 32, "%s", Blist[spca50x->bridge].name);
	      break;
	    }


	  case BRIDGE_SPCA505:
	    {
	      strncpy (v->name, ((v->channel == 0) ? "SPCA505" : "Video In"),
		       32);
	      break;
	    }
	  case BRIDGE_SPCA506:
	    {
	      spca506_GetNormeInput (spca50x,
				     (__u16 *) & (v->norm),
				     (__u16 *) & (v->channel));

	      if (v->channel < 4)
		{
		  snprintf (v->name, 32, "SPCA506-CBVS-%d", v->channel);
		}
	      else
		{
		  snprintf (v->name, 32, "SPCA506-S-Video-%d", v->channel);
		}

	      break;
	    }


	  }

	v->flags = 0;
	v->tuners = 0;
	v->type = VIDEO_TYPE_CAMERA;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,v,sizeof(struct video_channel)))
	return -EFAULT;
#endif	

	return 0;
      }
    case VIDIOCSCHAN:
      {
	/* 
	   There seems to be some confusion as to whether this is a struct 
	   video_channel or an int. This should be safe as the first element 
	   of video_channel is an int channel number
	   and we just ignore the rest 
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_channel *v = arg;
#else
	 struct video_channel k ;
	 struct video_channel *v = &k;
	 if(copy_from_user(v,arg,sizeof(struct video_channel)))
	 return -EFAULT;
#endif	
	/* exclude hardware channel reserved */
	if ((v->channel < 0) || (v->channel > 9) || (v->channel == 4)
	    || (v->channel == 5))
	  return -EINVAL;
	if (spca50x->bridge == BRIDGE_SPCA506)
	  {
	    spca506_SetNormeInput (spca50x, v->norm, v->channel);
	  }


	return 0;
      }
    case VIDIOCGPICT:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_picture *p = arg;
#else	
	struct video_picture p1 ;
	struct video_picture *p = &p1;
#endif	

	p->depth = spca50x->frame[0].depth;
	p->palette = spca50x->frame[0].format;
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
	if (!autoadjust)
	  p->brightness = spca50x_get_brightness (spca50x) << 8;
#else /* SPCA50X_ENABLE_EXPERIMENTAL */
	p->brightness = spca50x_get_brightness (spca50x) << 8;
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */

	PDEBUG (4, "VIDIOCGPICT: depth=%d, palette=%d", p->depth, p->palette);

	if (spca50x->ccd)
	  {			/* hmm one day spca50x->ccd ?? need to be clarified MX 16/04/2004 */
	    spca506_GetBrightContrastHueColors (spca50x, &p->brightness,
						&p->contrast, &p->hue,
						&p->colour);
	    p->whiteness = 0;

	  }
	else
	  {
	    switch (spca50x->bridge)
	      {
	      case BRIDGE_SN9C102P:	
		//spca50x->brightness = sn9c102p_getbrightness(spca50x);
		sn9c102p_getcontrast(spca50x);
		//spca50x->colour = sn9c102p_getcolors(spca50x);
		p->brightness = spca50x->brightness;
		p->contrast = spca50x->contrast;
		p->colour = spca50x->colour;
		break;
	      case BRIDGE_ETOMS:	
		spca50x->brightness = Et_getbrightness(spca50x);
		spca50x->contrast = Et_getcontrast(spca50x);
		spca50x->colour = Et_getcolors(spca50x);
		p->brightness = spca50x->brightness;
		p->contrast = spca50x->contrast;
		p->colour = spca50x->colour;
		break;
	      case BRIDGE_SPCA561:

		spca561_getbrightness (spca50x);
		spca561_getcontrast (spca50x);
		p->brightness = spca50x->brightness;
		p->contrast = spca50x->contrast;
		break;
	      case BRIDGE_SPCA500:
		{
		  int tmp;

		  p->colour = 0;	//0x8169
		  p->whiteness = 0;
		  p->contrast = spca50x_reg_read (spca50x->dev, 0x0, 0x8168, 1) << 8;	//0x8162
		  p->brightness = spca50x_get_brightness (spca50x);

		  /* get hue */
		  tmp =
		    (spca50x_reg_read (spca50x->dev, 0x0, 0x816b, 1) & 12) <<
		    6;
		  tmp += spca50x_reg_read (spca50x->dev, 0x0, 0x816a, 1);
		  p->hue = tmp << 6;
		  PDEBUG (2, "brightness = %d", p->brightness);
		  break;
		}
	      case BRIDGE_SPCA501:
		{
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
		  p->whiteness = spca50x_get_whiteness (spca50x) << 8;
		  if (autoadjust)
		    p->brightness = p->whiteness;
		  p->contrast = ((spca50x_reg_read (spca50x->dev,
						    SPCA501_REG_CCDSP,
						    SPCA501_A11,
						    2) & 0xFF) -
				 spca50x->a_blue) << 8;
		  printk ("Contrast = %d\n", p->contrast >> 8);
#else /* SPCA50X_ENABLE_EXPERIMENTAL */
		  p->whiteness = 0;
		  p->contrast = 0;
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */
		  p->colour = (spca50x_reg_read (spca50x->dev,
						 SPCA501_REG_CCDSP, 0x11,
						 2) & 0xFF) << 8;
		  p->hue =
		    (spca50x_reg_read
		     (spca50x->dev, SPCA501_REG_CCDSP, 0x13, 2) & 0xFF) << 8;
		  break;
		}
	      case BRIDGE_SPCA536:
		{
		  p->colour =
		    spca50x_reg_read (spca50x->dev, 0x0, 0x20f6, 2) << 8;
		  p->whiteness = 0;
		  p->contrast =
		    spca50x_reg_read (spca50x->dev, 0x0, 0x20f1, 2) << 8;
		  p->brightness = spca50x_get_brightness (spca50x);
		  p->hue =
		    spca50x_reg_read (spca50x->dev, 0x0, 0x20f5, 2) << 8;
		  /* FIXME: how do we deal with gamma? the chip knows it, v4l doesnt */
		  /* 0x20f5 HueL 0x20f6 HueH */
		  PDEBUG (0,
			  "color: 0x%02x contrast: 0x%02x brightness: 0x%02x hue: 0x%02x",
			  p->colour, p->contrast, p->brightness, p->hue);
		  break;
		}
	      case BRIDGE_SPCA533:
	      case BRIDGE_SPCA504:
	      case BRIDGE_SPCA504B:
	      case BRIDGE_SPCA504_PCCAM600:
		{
		  p->colour =
		    spca50x_reg_read (spca50x->dev, 0x0, 0x21ae, 2) << 8;
		  p->whiteness = 0;
		  p->contrast =
		    spca50x_reg_read (spca50x->dev, 0x0, 0x21a8, 2) << 8;
		  p->brightness = spca50x_get_brightness (spca50x);
		  p->hue =
		    spca50x_reg_read (spca50x->dev, 0x0, 0x21ad, 2) << 8;
		  /* FIXME: how do we deal with gamma? the chip knows it, v4l doesnt */
		  PDEBUG (0,
			  "color: 0x%02x contrast: 0x%02x brightness: 0x%02x hue: 0x%02x",
			  p->colour, p->contrast, p->brightness, p->hue);
		  break;
		}
#ifdef SPCA50X_ENABLE_EXP_BRIGHTNESS
	      case BRIDGE_SPCA505:
		{
		  p->whiteness =
		    (spca50x_reg_read (spca50x->dev, 6, 0x59, 2) * 10) << 8;
		  p->contrast =
		    spca50x_reg_read (spca50x->dev, 5, 0xc2, 2) << 8;
		  p->colour =
		    spca50x_reg_read (spca50x->dev, 5, 0xc2, 2) << 8;
		  p->hue = spca50x_reg_read (spca50x->dev, 5, 0xc1, 2) << 8;
		  break;
		}
#endif /* SPCA50X_ENABLE_EXP_BRIGHTNESS */
	      default:
		{
		  /* default to not allowing any settings to change */
		  p->colour = spca50x->colour;
		  p->whiteness = spca50x->whiteness;
		  p->contrast = spca50x->contrast;
		  p->brightness = spca50x->brightness;
		  p->hue = spca50x->hue;
		  break;
		}
	      }
	  }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,p,sizeof(struct video_picture)))
	return -EFAULT;
#endif	

	return 0;
      }
    case VIDIOCSPICT:
      {
      	int i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_picture *p = arg;
#else	
	struct video_picture p1 ;
	struct video_picture *p = &p1;
	if(copy_from_user(p,arg,sizeof(struct video_picture)))
	 return -EFAULT;
#endif	
	

	PDEBUG (4, "VIDIOCSPICT");


	if (!spca5xx_get_depth (spca50x, p->palette))
	  return -EINVAL;

	if (p->depth < spca50x->min_bpp)
	  return -EINVAL;

	PDEBUG (4, "Setting depth=%d, palette=%d", p->depth, p->palette);
	for (i = 0; i < SPCA50X_NUMFRAMES; i++)
	  {
	    spca50x->frame[i].depth = p->depth;
	    spca50x->frame[i].format = p->palette;
	  }

	if (spca50x->ccd)
	  {
	    spca506_SetBrightContrastHueColors (spca50x, p->brightness,
						p->contrast, p->hue,
						p->colour);

	  }
	else
	  {
	    switch (spca50x->bridge)
	      {
	      case BRIDGE_SN9C102P:
	        spca50x->contrast = p->contrast;
		spca50x->brightness = p->brightness;
		
		sn9c102p_setbrightness(spca50x);
		sn9c102p_setcontrast(spca50x);
		
	        break;
	      case BRIDGE_ETOMS:
	        spca50x->contrast = p->contrast;
		spca50x->brightness = p->brightness;
		spca50x->colour = p->colour;
		Et_setbrightness(spca50x);
		Et_setcontrast(spca50x);
		Et_setcolors(spca50x);
	        break;
	      case BRIDGE_ZC3XX:
		spca50x->contrast = p->contrast;
		spca50x->brightness = p->brightness;
		zc3xx_setbrightness (spca50x);
		break;
	      case BRIDGE_SONIX:
		spca50x_set_brightness (spca50x, p->brightness >> 8);
		spca50x->contrast = p->contrast;
		sonix_setcontrast (spca50x);
		break;
	      case BRIDGE_TV8532:
		spca50x_set_brightness (spca50x, p->brightness >> 8);
		spca50x->contrast = p->contrast;
		tv8532_setcontrast (spca50x);
		break;
	      case BRIDGE_CX11646:
		spca50x_set_brightness (spca50x, p->brightness >> 8);
		spca50x->contrast = p->contrast;
		cx_setcontrast (spca50x);
		spca50x->colour = p->colour;
		break;
	      case BRIDGE_SPCA561:
		spca50x->brightness = p->brightness;
		spca50x->contrast = p->contrast;
		spca561_setbrightness (spca50x);
		spca561_setcontrast (spca50x);
		break;
	      case BRIDGE_SPCA500:
		{
		  int tmp;

		  spca50x_set_brightness (spca50x, p->brightness >> 8);	//((p.brightness >> 8) + 128) % 255);
		  spca50x_reg_write (spca50x->dev, 0x00, 0x8168,
				     p->contrast >> 8);

		  /* set hue */
		  tmp = spca50x_reg_read (spca50x->dev, 0x00, 0x816b, 1);
		  tmp = tmp & 0xf3;
		  tmp += (p->hue & 0xc000) >> 12;
		  spca50x_reg_write (spca50x->dev, 0x00, 0x816b, tmp);
		  spca50x_reg_write (spca50x->dev, 0x00, 0x816a,
				     (p->hue & 0x3fff) >> 8);
		}
	      case BRIDGE_SPCA501:
		{
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
		  __u16 kred, kgreen, kblue;	//coeffs of color correction

		  if (autoadjust)
		    p->whiteness = p->brightness;
		  spca50x_set_whiteness (spca50x, p->whiteness >> 8);

		  /* Setting "contrast" (offset to color correction parameters) */
		  p->contrast >>= 8;
		  kred = spca50x->a_red + p->contrast;
		  kgreen = spca50x->a_green + p->contrast;
		  kblue = spca50x->a_blue + p->contrast;
		  PDEBUG (4, "Setting red = %d, green = %d, blue = %d",
			  (__u8) (kred & 0xFF), (__u8) (kgreen & 0xFF),
			  (__u8) (kblue & 0xFF));
		  if (kred < 256 && kgreen < 256 && kblue < 256)
		    {
		      spca50x_reg_write (spca50x->dev,
					 SPCA501_REG_CCDSP, SPCA501_A11,
					 (__u8) (kblue & 0xFF));
		      spca50x_reg_write (spca50x->dev, SPCA501_REG_CCDSP,
					 SPCA501_A21, (__u8) (kgreen & 0xFF));
		      spca50x_reg_write (spca50x->dev, SPCA501_REG_CCDSP,
					 SPCA501_A31, (__u8) (kred & 0xFF));
		    }
		  spca50x_reg_write (spca50x->dev, SPCA501_REG_CCDSP, 0x11,
				     (__u8) ((p->colour >> 8) & 0xFF));
		  spca50x_reg_write (spca50x->dev, SPCA501_REG_CCDSP, 0x13,
				     (__u8) ((p->hue >> 8) & 0xFF));
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */
		  /* Setting brightness */
		  PDEBUG (4, "Setting brightness = %d", p->brightness);
//// This call must be moved to better place
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
		  if (!autoadjust)
		    spca50x_set_brightness (spca50x, p->brightness >> 8);
#else /* SPCA50X_ENABLE_EXPERIMENTAL */
		  spca50x_set_brightness (spca50x, p->brightness >> 8);
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */
		  break;
		}
	      case BRIDGE_SPCA536:
		{
		  spca50x_reg_write (spca50x->dev, 0x0, 0x20f6,
				     p->colour >> 8);
		  p->whiteness = 0;
		  spca50x_reg_write (spca50x->dev, 0x0, 0x20f1,
				     p->contrast >> 8);
		  spca50x_set_brightness (spca50x,
					  ((p->brightness >> 8) + 128) % 255);
		  //spca50x_reg_write(spca50x->dev, 0x0, 0x20f5, p.hue >> 8);
		  break;
		}
	      case BRIDGE_SPCA533:
	      case BRIDGE_SPCA504:
	      case BRIDGE_SPCA504B:
	      case BRIDGE_SPCA504_PCCAM600:
		{
		  /* v4l docs tell me that all parameters are scaled 0-65535 
		   * This means we keep them at that scale internally and adjust
		   * to the actual hardware capabilites on write. */

		  /* 0 - 8 default on win: 1 */
		  spca50x_reg_write (spca50x->dev, 0x0, 0x21ae,
				     p->colour >> 8);
		  p->whiteness = 0;
		  /* 1 - 8 default on win: 1 */
		  spca50x_reg_write (spca50x->dev, 0x0, 0x21a8,
				     p->contrast >> 8);

		  /* The chip keeps the brightness as 129 -> 128 with 0 being the midpoint.
		   * v4l always keeps these values unsigned 16 bit, that is 0 to 65535. 
		   * Scale accordingly. */

		  spca50x_set_brightness (spca50x,
					  ((p->brightness >> 8) + 128) % 255);

		  /* 0 - 360 default on win: 0 */
		  //spca50x_reg_write(spca50x->dev, 0x0, 0x21ad, p.hue >> 8);
		  ;;;
		  break;
		}
	      case BRIDGE_SPCA505:
		{
		  spca50x_set_brightness (spca50x, p->brightness >> 8);
#if 0				//#ifdef SPCA50X_ENABLE_EXP_BRIGHTNESS

		  spca50x_reg_write (spca50x->dev, 6, 0x1f, 0x16);
		  spca50x_reg_write (spca50x->dev, 6, 0x20, 0x0);

		  spca50x_reg_write (spca50x->dev, 5, 0xc1, p->hue >> 8);
		  spca50x_reg_write (spca50x->dev, 5, 0xc2, p->contrast >> 8);
		  spca50x_reg_write (spca50x->dev, 5, 0xca, 0x0);
////I have no this type of camera. Please, check this and move this
// call to better place
		  spca50x_set_brightness (spca50x, p->brightness >> 8);
////
		  spca50x_reg_write (spca50x->dev, 6, 0x59,
				     max ((p->whiteness >> 8) / 10, 20));
#endif /* SPCA50X_ENABLE_EXP_BRIGHTNESS */
		  break;
		}
	      default:
		{
		  /* default to not allowing any settings to change ?? */
		  p->colour = spca50x->colour;
		  p->whiteness = spca50x->whiteness;
		  p->contrast = spca50x->contrast;
		  p->brightness = spca50x->brightness;
		  p->hue = spca50x->hue;
		  break;
		}
	      }
	  }
	spca50x->contrast = p->contrast;
	spca50x->brightness = p->brightness;
	spca50x->colour = p->colour;
	spca50x->hue = p->hue;
	spca50x->whiteness = p->whiteness;
#ifdef SPCA50X_ENABLE_EXPERIMENTAL
	spca50x->nstable = 0;
	spca50x->nunstable = 0;
#endif /* SPCA50X_ENABLE_EXPERIMENTAL */

	return 0;
      }
    case VIDIOCGCAPTURE:
      {
	int *vf = arg;

	PDEBUG (4, "VIDIOCGCAPTURE");
	*vf = 0;
	// no subcapture
	return -EINVAL;
      }
    case VIDIOCSCAPTURE:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_capture *vc = arg;
#else	
	struct video_capture vc1 ;
	struct video_capture *vc = &vc1;
	if(copy_from_user(vc,arg,sizeof(struct video_capture)))
	 return -EFAULT;
#endif	

	if (vc->flags)
	  return -EINVAL;
	if (vc->decimation)
	  return -EINVAL;

	return -EINVAL;
      }
    case VIDIOCSWIN:
      {
      	int result;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_window *vw = arg;
#else	
	struct video_window vw1 ;
	struct video_window *vw = &vw1;
	if(copy_from_user(vw,arg,sizeof(struct video_window)))
	 return -EFAULT;
#endif	
	
	PDEBUG (3, "VIDIOCSWIN: width=%d, height=%d, flags=%d",
		vw->width, vw->height, vw->flags);
	if (vw->x)
	  return -EINVAL;
	if (vw->y)
	  return -EINVAL;
	if (vw->width > (unsigned int) spca50x->maxwidth)
	  return -EINVAL;
	if (vw->height > (unsigned int) spca50x->maxheight)
	  return -EINVAL;
	if (vw->width < (unsigned int) spca50x->minwidth)
	  return -EINVAL;
	if (vw->height < (unsigned int) spca50x->minheight)
	  return -EINVAL;

       if ( spca50x->bridge == BRIDGE_ZC3XX ||
             spca50x->bridge == BRIDGE_SONIX ||
	     spca50x->bridge == BRIDGE_ETOMS ||
	     spca50x->bridge == BRIDGE_SPCA561){
	 result = spca5xx_restartMode(spca50x,vw->width,vw->height,spca50x->frame[0].format);
	} else {
	  result = spca50x_mode_init_regs (spca50x, vw->width, vw->height,
					 spca50x->frame[0].format,
					 GET_EXT_MODES (spca50x->bridge));
	
	}

	if (result == 0)
	  {
	    spca50x->frame[0].width = vw->width;
	    spca50x->frame[0].height = vw->height;
	  }

	return result;
      }
    case VIDIOCGWIN:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_window *vw = arg;
#else	
	struct video_window vw1 ;
	struct video_window *vw = &vw1;
#endif		

	memset (vw, 0, sizeof (struct video_window));
	vw->x = 0;
	vw->y = 0;
	vw->width = spca50x->frame[0].width;
	vw->height = spca50x->frame[0].height;
	vw->flags = 0;

	PDEBUG (4, "VIDIOCGWIN: %dx%d", vw->width, vw->height);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,vw,sizeof(struct video_capture)))
	return -EFAULT;
#endif	

	return 0;
      }
    case VIDIOCGMBUF:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_mbuf *vm=arg;
#else	
	struct video_mbuf vm1;
	struct video_mbuf *vm = &vm1;
#endif		
	int i;
	PDEBUG (2, "VIDIOCGMBUF: %p ", vm);
	memset (vm, 0, sizeof (struct video_mbuf));
	vm->size = SPCA50X_NUMFRAMES * MAX_DATA_SIZE;
	vm->frames = SPCA50X_NUMFRAMES;

	for (i = 0; i < SPCA50X_NUMFRAMES; i++)
	  {
	    vm->offsets[i] = MAX_DATA_SIZE * i;
	  }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,vm,sizeof(struct video_mbuf)))
	return -EFAULT;
#endif	

	return 0;
      }
    case VIDIOCMCAPTURE:
      {
      	int ret, depth;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
	struct video_mmap *vm = arg;
#else	
	struct video_mmap vm1;
	struct video_mmap *vm = &vm1;
	if(copy_from_user(vm,arg,sizeof(struct video_mmap)))
	 return -EFAULT;
#endif	
	

	PDEBUG (2, "CMCAPTURE");
	PDEBUG (2, "frame: %d, size: %dx%d, format: %d",
		vm->frame, vm->width, vm->height, vm->format);
	depth = spca5xx_get_depth (spca50x, vm->format);
	if (!depth || depth < spca50x->min_bpp)
	  {
	    err ("VIDIOCMCAPTURE: invalid format (%d)", vm->format);
	    return -EINVAL;
	  }

	if ((vm->frame < 0) || (vm->frame > 3))
	  {
	    err ("VIDIOCMCAPTURE: invalid frame (%d)", vm->frame);
	    return -EINVAL;
	  }

	if (vm->width > spca50x->maxwidth || vm->height > spca50x->maxheight)
	  {
	    err ("VIDIOCMCAPTURE: requested dimensions too big");
	    return -EINVAL;
	  }
	if (vm->width < spca50x->minwidth || vm->height < spca50x->minheight)
	  {
	    err ("VIDIOCMCAPTURE: requested dimensions too small");
	    return -EINVAL;
	  }
	/* 
	 * If we are grabbing the current frame, let it pass
	 */
	if (spca50x->frame[vm->frame].grabstate == FRAME_GRABBING)
	  {
	    PDEBUG (4, "MCAPTURE: already grabbing");
	    ret = wait_event_interruptible (spca50x->wq,
					    (spca50x->frame[vm->frame].
					     grabstate != FRAME_GRABBING));
	    if (ret)
	      return -EINTR;

	  }
	if ((spca50x->frame[vm->frame].width != vm->width) ||
	    (spca50x->frame[vm->frame].height != vm->height) ||
	    (spca50x->frame[vm->frame].format != vm->format))
	  {
	if(spca50x->bridge == BRIDGE_ZC3XX || 
	   spca50x->bridge == BRIDGE_SONIX ||
	   spca50x->bridge == BRIDGE_ETOMS ||
	   spca50x->bridge == BRIDGE_SPCA561){
	     ret = spca5xx_restartMode(spca50x,vm->width,vm->height,vm->format);
	} else {
	     ret = spca50x_mode_init_regs (spca50x, vm->width, vm->height,
					  vm->format,
					  GET_EXT_MODES (spca50x->bridge));
	  
	}
	    if (ret < 0)
	      return ret;
	    spca50x->frame[vm->frame].width = vm->width;
	    spca50x->frame[vm->frame].height = vm->height;
	    spca50x->frame[vm->frame].format = vm->format;
	    spca50x->frame[vm->frame].depth = depth;

	  }
/* set the autoexpo here */
if(spca50x->bridge == BRIDGE_SPCA561)
	spca561_setAutobright(spca50x);
if (spca50x->bridge == BRIDGE_ETOMS){
	PDEBUG(5, "Etoms AutoBrightness");
	Et_setAutobright (spca50x);	
		}
#if 0
if (spca50x->bridge == BRIDGE_SN9C102P)
	sn9c102p_setAutobright (spca50x);
#endif
	/* Mark it as ready */
	spca50x->frame[vm->frame].grabstate = FRAME_READY;
	return 0;
      }
    case VIDIOCSYNC:
      {
      	int ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
		 unsigned int frame = *((unsigned int *) arg);
#else	
		 unsigned int frame ;
		if(copy_from_user((void*)&frame,arg,sizeof(int)))
	 return -EFAULT;
#endif	
	

	PDEBUG (3, "syncing to frame %d, grabstate = %d", frame,
		spca50x->frame[frame].grabstate);

	switch (spca50x->frame[frame].grabstate)
	  {
	  case FRAME_UNUSED:
	    return -EINVAL;
	  case FRAME_ABORTING:
	    return -ENODEV;
	  case FRAME_READY:
	  case FRAME_GRABBING:
	  case FRAME_ERROR:
	  redo:
	    if (!spca50x->dev)
	      return -EIO;


	    ret = wait_event_interruptible (spca50x->wq,
					    (spca50x->frame[frame].
					     grabstate == FRAME_DONE));
	    if (ret)
	      return -EINTR;

	    PDEBUG (5, "Synched on frame %d, grabstate = %d",
		    frame, spca50x->frame[frame].grabstate);

	    if (spca50x->frame[frame].grabstate == FRAME_ERROR)
	      {
		goto redo;
	      }
	    /* Fallthrough.
	     * We have waited in state FRAME_GRABBING until it
	     * becomes FRAME_DONE, so now we can move along.
	     */
	  case FRAME_DONE:
	    if (spca50x->snap_enabled && !spca50x->frame[frame].snapshot)
	      {
		goto redo;
	      }
	    PDEBUG (3, "Already synched %d\n", frame);

	    /* Release the current frame. This means that it
	     * will be reused as soon as all other frames are
	     * full, so the app better be done with it quickly.
	     * Can this be avoided somehow?
	     */
	    spca50x->frame[frame].grabstate = FRAME_UNUSED;

	    break;
	  }			/* end switch */

	return 0;
      }
    case VIDIOCGFBUF:
      {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
		struct video_buffer *vb=arg;
#else	
		struct video_buffer vb1;
		struct video_buffer *vb= &vb1;
#endif	
	memset (vb, 0, sizeof (struct video_buffer));
	vb->base = NULL;	/* frame buffer not supported, not used */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
	if(copy_to_user(arg,vb,sizeof(struct video_buffer)))
	return -EFAULT;
#endif	

	return 0;
      }
    case VIDIOCKEY:
      return 0;
    case VIDIOCCAPTURE:
      return -EINVAL;
    case VIDIOCSFBUF:
      return -EINVAL;
    case VIDIOCGTUNER:
    case VIDIOCSTUNER:
      return -EINVAL;
    case VIDIOCGFREQ:
    case VIDIOCSFREQ:
      return -EINVAL;
    case VIDIOCGAUDIO:
    case VIDIOCSAUDIO:
      return -EINVAL;
    default:
      return -ENOIOCTLCMD;
    }				/* end switch */

  return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static int
spca5xx_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
	       unsigned long arg)
{
	struct video_device *vdev = file->private_data;
	struct usb_spca50x *spca50x = video_get_drvdata(vdev);
	int rc;

	if (down_interruptible(&spca50x->lock))
		return -EINTR;
  rc = video_usercopy (inode, file, cmd, arg, spca5xx_do_ioctl);

	up(&spca50x->lock);
  return rc;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static ssize_t
spca5xx_read (struct file *file, char *buf, size_t cnt, loff_t * ppos)
{

  struct video_device *dev = file->private_data;
  int noblock = file->f_flags & O_NONBLOCK;
  unsigned long count = cnt;
#else
static long 
spca5xx_read(struct video_device *dev, char * buf, unsigned long
	count,int noblock)
{
#endif
  struct usb_spca50x *spca50x = video_get_drvdata (dev);

  int i;
  int frmx = -1;
  int rc;
  volatile struct spca50x_frame *frame;

  PDEBUG (4, "%ld bytes, noblock=%d", count, noblock);
if (down_interruptible(&spca50x->lock))
		return -EINTR;

  if (!dev || !buf){
   	up(&spca50x->lock);
    return -EFAULT;
}
  if (!spca50x->dev){
   	up(&spca50x->lock);
    return -EIO;
}
  if (!spca50x->streaming){
   	up(&spca50x->lock);
    return -EIO;
}

  /* Wait while we're grabbing the image */
	PDEBUG(4, "Waiting for a frame done");
	if((rc = wait_event_interruptible(spca50x->wq,
		spca50x->frame[0].grabstate == FRAME_DONE ||
		spca50x->frame[1].grabstate == FRAME_DONE ||
		spca50x->frame[2].grabstate == FRAME_DONE ||
		spca50x->frame[3].grabstate == FRAME_DONE ))){
	 	up(&spca50x->lock);
		return rc;
	}
/* set the autoexpo here */
if(spca50x->bridge == BRIDGE_SPCA561)
	spca561_setAutobright(spca50x);

  /* 
   * Ok, for the jpeg cams, we know we have received a whole
   * frame now. The rest of the code in this routine confuses 
   * frames and sequence numbers, so we do our own thing here.
   * */

  /* One frame has just been set to DONE. Find it. */
  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    if (spca50x->frame[i].grabstate == FRAME_DONE)
      frmx = i;

  PDEBUG (4, "Frame number: %d", frmx);
  if (frmx < 0)
    {
      /* We havent found a frame that is DONE. Damn. Should
       * not happen. */
      PDEBUG (2, "Couldnt find a frame ready to be read.");
      	up(&spca50x->lock);
      return -EFAULT;
    }
  frame = &spca50x->frame[frmx];
  PDEBUG (2, "count asked: %d available: %d", (int) count,
	  (int) frame->scanlength);

  if (count > frame->scanlength)
    count = frame->scanlength;

  if ((i = copy_to_user (buf, frame->data, count)))
    {
      PDEBUG (2, "Copy failed! %d bytes not copied", i);
      	up(&spca50x->lock);
      return -EFAULT;
    }
  /* Release the frame */
  frame->grabstate = FRAME_READY;
up(&spca50x->lock);
  return count;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,22)
static int
spca5xx_mmap (struct file *file, struct vm_area_struct *vma)
{

  struct video_device *dev = file->private_data;
  unsigned long start = vma->vm_start;
  unsigned long size = vma->vm_end - vma->vm_start;
#else
static int 
spca5xx_mmap(struct video_device *dev,const char *adr, unsigned long size)
{
  unsigned long start=(unsigned long) adr;
#endif  
  struct usb_spca50x *spca50x = video_get_drvdata (dev);
  unsigned long page, pos;

  if (spca50x->dev == NULL)
    return -EIO;

  PDEBUG (4, "mmap: %ld (%lX) bytes", size, size);

  if (size >
      (((SPCA50X_NUMFRAMES * MAX_DATA_SIZE) + PAGE_SIZE - 1) & ~(PAGE_SIZE -
								 1)))
    return -EINVAL;
	if (down_interruptible(&spca50x->lock))
		return -EINTR;

  pos = (unsigned long) spca50x->fbuf;
  while (size > 0)
    {
      page = kvirt_to_pa (pos);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) || defined (RH9_REMAP)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
      if (remap_pfn_range
	  (vma, start, page >> PAGE_SHIFT, PAGE_SIZE, PAGE_SHARED)){
#else
      if (remap_page_range (vma, start, page, PAGE_SIZE, PAGE_SHARED)){
#endif /* KERNEL 2.6.9 */
#else /* RH9_REMAP */
      if (remap_page_range (start, page, PAGE_SIZE, PAGE_SHARED)){
#endif /* RH9_REMAP */
	up(&spca50x->lock);
	return -EAGAIN;
	}
      start += PAGE_SIZE;
      pos += PAGE_SIZE;
      if (size > PAGE_SIZE)
	size -= PAGE_SIZE;
      else
	size = 0;
    }
up(&spca50x->lock);
  return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,4,22)
static struct file_operations spca5xx_fops = {
  .owner = THIS_MODULE,
  .open = spca5xx_open,
  .release = spca5xx_close,
  .read = spca5xx_read,
  .mmap = spca5xx_mmap,
  .ioctl = spca5xx_ioctl,
  .llseek = no_llseek,
};
static struct video_device spca50x_template = {
  .owner = THIS_MODULE,
  .name = "SPCA5XX USB Camera",
  .type = VID_TYPE_CAPTURE,
  .hardware = VID_HARDWARE_SPCA5XX,
  .fops = &spca5xx_fops,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
  .release = video_device_release,
#endif
  .minor = -1,
};
#else
static struct video_device spca50x_template = {
	name: "SPCA5XX USB Camera",
	type:	VID_TYPE_CAPTURE,
	hardware:VID_HARDWARE_SPCA5XX,
	open:   spca5xx_open,
	close:  spca5xx_close,
  	read:   spca5xx_read,
  	mmap:   spca5xx_mmap,
  	ioctl:  spca5xx_ioctl
};
#endif //KERNEL VERSION 2,4,22
/****************************************************************************
 *
 * SPCA50X configuration
 *
 ***************************************************************************/

static int
spca50x_configure_sensor (struct usb_spca50x *spca50x)
{
 
 int err;
  PDEBUG (4, "starting configuration");
  /* Set sensor-specific vars */
  switch (spca50x->bridge)
    {
    case BRIDGE_SPCA500:
      if (spca50x->desc == LogitechClickSmart310)
	{
	  spca50x->maxwidth = 352;
	  spca50x->maxheight = 288;
	  spca50x->minwidth = 176;
	  spca50x->minheight = 144;
	}
      else
	{
	  spca50x->maxwidth = 640;
	  spca50x->maxheight = 480;
	  spca50x->minwidth = 176;
	  spca50x->minheight = 144;
	}
      break;
    case BRIDGE_ETOMS:
    case BRIDGE_ZC3XX:
    case BRIDGE_SONIX:
    case BRIDGE_SPCA561:
    case BRIDGE_MR97311:
     err = spca5xx_getcapability(spca50x);
    break;
    case BRIDGE_SPCA504_PCCAM600:
    case BRIDGE_SPCA504:
    case BRIDGE_SPCA504B:
    case BRIDGE_SPCA536:
    case BRIDGE_CX11646:
      {
	spca50x->maxwidth = 640;
	spca50x->maxheight = 480;
	spca50x->minwidth = 176;
	spca50x->minheight = 144;
	break;
      }

    case BRIDGE_SPCA533:
      {
	if ((spca50x->desc == MegapixV4) ||
	    (spca50x->desc == LogitechClickSmart820))
	  {
	    spca50x->maxwidth = 320;
	    spca50x->maxheight = 240;
	    spca50x->minwidth = 176;
	    spca50x->minheight = 144;
	  }
	else
	  {
	    spca50x->maxwidth = 464;
	    spca50x->maxheight = 480;
	    spca50x->minwidth = 176;
	    spca50x->minheight = 144;
	  }
	break;
      }
    case BRIDGE_SPCA501:
      {
	spca50x->maxwidth = 640;
	spca50x->maxheight = 480;
	spca50x->minwidth = 160;
	spca50x->minheight = 120;
	break;
      }
    case BRIDGE_SPCA506:
      {
	spca50x->maxwidth = 640;
	spca50x->maxheight = 480;
	spca50x->minwidth = 160;
	spca50x->minheight = 120;
	break;
      }
    case BRIDGE_SPCA505:
      {
	if (spca50x->desc == Nxultra)
	  {
	    spca50x->maxwidth = 640;
	    spca50x->maxheight = 480;

	  }
	else
	  {
	    spca50x->maxwidth = 352;
	    spca50x->maxheight = 288;
	  }
	spca50x->minwidth = 160;
	spca50x->minheight = 120;
	break;
      }
    case BRIDGE_SPCA508:
      {
	spca50x->maxwidth = 352;
	spca50x->maxheight = 288;
	spca50x->minwidth = 160;
	spca50x->minheight = 120;
	break;
      }
    case BRIDGE_TV8532:
      {
	spca50x->maxwidth = 352;
	spca50x->maxheight = 288;
	spca50x->minwidth = 176;
	spca50x->minheight = 144;
	break;
      }
     case BRIDGE_SN9C102P:
     	spca50x->maxwidth = 640;
	spca50x->maxheight = 480;
	spca50x->minwidth = 176;
	spca50x->minheight = 144;
     break;
    default:
      {
	spca50x->maxwidth = 640;
	spca50x->maxheight = 480;
	spca50x->minwidth = 160;
	spca50x->minheight = 120;
	break;
      }
    }
  return 0;
}


static int
spca50x_configure (struct usb_spca50x *spca50x)
{
  int i;
  int defaultcols;
  int defaultrows;
  int defaultpipe;
/* DEPRECATED spca5xx_getDefaultMode() should be use instead */
  /* dynamically look up the smallest available frame size */
  /* That is not really true with Hardware size need smallest and hardware */
  /* Also give the pipe size for the hardware resolution */
  i = spca50x_smallest_mode_index (GET_EXT_MODES (spca50x->bridge),
				   &defaultcols, &defaultrows, &defaultpipe);
  if (i < 0)
    return i;

  PDEBUG (2, "video_register_device succeeded");
  /* Initialise the camera bridge */
  switch (spca50x->bridge)
    {
    
    case BRIDGE_SONIX:
    if (ccd)
	PDEBUG (1, "Forcing CCD to zero ?? !");
      spca50x->ccd = 0;
      if(sonix_config(spca50x) < 0)
      	goto error;
    break;
    case BRIDGE_ZC3XX:
    if (ccd)
	PDEBUG (1, "Forcing CCD to zero ?? !");
      spca50x->ccd = 0;
      if(zc3xx_config(spca50x) < 0)
      	goto error;
    break;
    case BRIDGE_ETOMS:
    if (ccd)
	PDEBUG (1, "Forcing CCD to zero ?? !");
      spca50x->ccd = 0;
      if(Etxx_config(spca50x) < 0)
      	goto error;
    break;
    case BRIDGE_SPCA561:
      {
	if (ccd)
	  PDEBUG (1,
		  "Forcing CCD to zero ?? !");
	spca50x->ccd = 0;
	if (spca561_config (spca50x) < 0)
	  goto error;
      }
    break;
    case BRIDGE_MR97311:
      {
	if (ccd)
	  PDEBUG (1,
		  "Forcing CCD to zero ?? !");
	spca50x->ccd = 0;
	if (mr97311_config (spca50x) < 0)
	  goto error;
      }
    break;
    case BRIDGE_CX11646:
    case BRIDGE_SPCA500:
    case BRIDGE_SPCA536:
    case BRIDGE_SPCA533:
    case BRIDGE_SPCA504:
    case BRIDGE_SPCA504B:
    case BRIDGE_SPCA504_PCCAM600: 
    case BRIDGE_SN9C102P:
      if (ccd)
	PDEBUG (1, "Forcing CCD to zero ?? !");
      spca50x->ccd = 0;
      break;
    case BRIDGE_TV8532:
      tv8532_configure (spca50x);
      if (ccd)
	PDEBUG (1, "Forcing CCD to zero ?? !");
      spca50x->ccd = 0;
      break;
    case BRIDGE_SPCA501:
      {
	/* Determine the sensor type for cameras with an
	 * internal CCD or external input
	 */
	if (ccd)
	  PDEBUG (1,
		  "Forcing CCD to zero for SPCA501? which doesn't have external input !");
	spca50x->ccd = 0;
	if (spca50x->desc == Arowana300KCMOSCamera)
	  {
	    /* Arowana 300k CMOS Camera data */
	    if (spca50x_write_vector (spca50x, spca501c_arowana_init_data))
	      goto error;
	  }
	else if (spca50x->desc == MystFromOriUnknownCamera)
	  {
	    /* UnKnow Ori CMOS Camera data */
	    if (spca50x_write_vector (spca50x, spca501c_mysterious_open_data))
	      goto error;
	  }
	else
	  {
	    /* generic spca501 init data */
	    if (spca50x_write_vector (spca50x, spca501_init_data))
	      goto error;
	  }
	break;
      }


    case BRIDGE_SPCA505:
      {
	/* Determine the sensor type for cameras with an internal CCD or external input */
	spca50x->ccd = ccd;
	if (spca50x->desc == Nxultra)
	  {
	    if (spca50x_write_vector (spca50x, spca505b_init_data))
	      goto error;
	  }
	else
	  {
	    if (spca50x_write_vector (spca50x, spca505_init_data))
	      goto error;
	  }
	break;
      }
    case BRIDGE_SPCA506:
      {
	if (ccd)
	  PDEBUG (1, "Forcing CCD to one for SPCA506 which can't use one !");
	/* Determine the sensor type for cameras with an internal CCD or external input */
	spca50x->ccd = 1;

	break;
      }
    case BRIDGE_SPCA508:
      {
	/* FIXME: I've changed the following check to force ccd to be zero, instead of one.
	 *        Since I don't have one of these cameras, I don't know whether this is
	 *        the right thing to do.
	 *        NWG: Mon 12th August 2002.
	 */
	if (ccd)
	  PDEBUG (1,
		  "Forcing CCD to zero for SPCA508 which doesn't have external input !");
	spca50x->ccd = 0;
	if (config_spca508 (spca50x))
	  goto error;
	break;
      }
    default:
      {
	err ("Unknown bridge type");
	goto error;
      }
    }

  spca50x_set_packet_size (spca50x, 0);
  /* Set an initial pipe size; this will be overridden by
   * spca50x_set_mode(), called indirectly by the open routine.
   */
   if(spca50x->bridge == BRIDGE_ZC3XX ||
      spca50x->bridge == BRIDGE_SONIX ||
      spca50x->bridge == BRIDGE_ETOMS ||
      spca50x->bridge == BRIDGE_SPCA561){
      i = spca5xx_getDefaultMode(spca50x);
      if (i < 0) return i;
      
   } else {
      spca50x->pipe_size = defaultpipe;
      spca50x->width = defaultcols;
      spca50x->height = defaultrows;
   }
  spca50x->snap_enabled = snapshot;
  spca50x->force_rgb = force_rgb;
  spca50x->min_bpp = min_bpp;
  spca50x->lum_level = lum_level;
  
  if (spca50x_configure_sensor (spca50x) < 0)
    {
      err ("failed to configure");
      goto error;
    }
  /* configure the frame detector with default parameters */
  
  spca5xx_setFrameDecoder(spca50x);
  
  PDEBUG (2, "Spca5xx Configure done !!");
  return 0;

error:

  return -EBUSY;
}

/**************************************************************************
All That stuff need to be rewrite to go in spca500.h
***************************************************************************/
static void
spca500_ping310 (struct usb_spca50x *spca50x)
{
  __u8 Data[2] = { 0, 0 };
  spca5xxRegRead (spca50x->dev, 0, 0, 0x0d04, Data, 2);
  PDEBUG (5, "ClickSmart310 ping 0x0d04 0x%02X  0x%02X ", Data[0], Data[1]);
}

static int
spca500_synch310 (struct usb_spca50x *spca50x)
{
/* Synchro the Bridge with sensor */
/* Maybe that will work on all spca500 chip */
/* because i only own a clicksmart310 try for that chip */
/* using spca50x_set_packet_size() cause an Ooops here */
/* usb_set_interface from kernel 2.6.x clear all the urb stuff */
/* up-port the same feature as in 2.4.x kernel */

  __u8 Data;


  if (spca_set_interface (spca50x->dev, spca50x->iface, 0) < 0)
    {
      err ("Set packet size: set interface error");
      goto error;
    }
  spca500_ping310 (spca50x);

  spca5xxRegRead (spca50x->dev, 0, 0, 0x0d00, &Data, 1);

  /* need alt setting here */
  PDEBUG (5, "ClickSmart310 sync pipe %d altsetting %d ", spca50x->pipe_size,
	  spca50x->alt);
  /* Windoze use pipe with altsetting 6 why 7 here */
  if (spca_set_interface (spca50x->dev, spca50x->iface, spca50x->alt) < 0)
    {
      err ("Set packet size: set interface error");

      goto error;

    }

  return 0;
error:

  return -EBUSY;
}

static void
spca500_clksmart310_init (struct usb_spca50x *spca50x)
{
  __u8 Data[2] = { 0, 0 };
  spca5xxRegRead (spca50x->dev, 0, 0, 0x0d05, Data, 2);
  PDEBUG (5, "ClickSmart310 init 0x0d05 0x%02X  0x%02X ", Data[0], Data[1]);
  spca50x_reg_write (spca50x->dev, 0x00, 0x8167, 0x5a);
  spca500_ping310 (spca50x);

  spca50x_reg_write (spca50x->dev, 0x00, 0x8168, 0x22);
  spca50x_reg_write (spca50x->dev, 0x00, 0x816a, 0xc0);
  spca50x_reg_write (spca50x->dev, 0x00, 0x816b, 0x0b);
  spca50x_reg_write (spca50x->dev, 0x00, 0x8169, 0x25);
  spca50x_reg_write (spca50x->dev, 0x00, 0x8157, 0x5b);
  spca50x_reg_write (spca50x->dev, 0x00, 0x8158, 0x5b);
  spca50x_reg_write (spca50x->dev, 0x00, 0x813f, 0x03);
  spca50x_reg_write (spca50x->dev, 0x00, 0x8151, 0x4a);
  spca50x_reg_write (spca50x->dev, 0x00, 0x8153, 0x78);
  spca50x_reg_write (spca50x->dev, 0x00, 0x0d01, 0x04);	//00 for adjust shutter
  spca50x_reg_write (spca50x->dev, 0x00, 0x0d02, 0x01);
  spca50x_reg_write (spca50x->dev, 0x00, 0x8169, 0x25);
  spca50x_reg_write (spca50x->dev, 0x00, 0x0d01, 0x02);
}

void
spca500_reinit (struct usb_spca50x *spca50x)
{
  int err;
  __u8 Data;

  // some unknow command from Aiptek pocket dv and family300

  spca50x_reg_write (spca50x->dev, 0x00, 0x0d01, 0x01);
  spca50x_reg_write (spca50x->dev, 0x00, 0x0d03, 0x00);
  spca50x_reg_write (spca50x->dev, 0x00, 0x0d02, 0x01);

  /* enable drop packet */
  spca50x_reg_write (spca50x->dev, 0x00, 0x850a, 0x0001);

  err = spca50x_setup_qtable (spca50x, 0x00, 0x8800, 0x8840, qtable_pocketdv);
  if (err < 0)
    {
      PDEBUG (2, "spca50x_setup_qtable failed on init");
    }
  /* set qtable index */
  spca50x_reg_write (spca50x->dev, 0x00, 0x8880, 2);
  /* family cam Quicksmart stuff */
  spca50x_reg_write (spca50x->dev, 0x00, 0x800a, 0x00);
  //Set agc transfer: synced inbetween frames
  spca50x_reg_write (spca50x->dev, 0x00, 0x820f, 0x01);
  //Init SDRAM - needed for SDRAM access
  spca50x_reg_write (spca50x->dev, 0x00, 0x870a, 0x04);
  /*Start init sequence or stream */

  spca50x_reg_write (spca50x->dev, 0, 0x8003, 0x00);
  /* switch to video camera mode */
  err = spca50x_reg_write (spca50x->dev, 0x00, 0x8000, 0x0004);
  wait_ms (2000);
  if (spca50x_reg_readwait (spca50x->dev, 0, 0x8000, 0x44) != 0)

    spca5xxRegRead (spca50x->dev, 0, 0, 0x816b, &Data, 1);
  spca50x_reg_write (spca50x->dev, 0x00, 0x816b, Data);

}

static int
spca500_initialise (struct usb_spca50x *spca50x)
{

  int index;
  int err;
  __u8 Data;
  /* read mode index */
  index = spca50x_find_mode_index (spca50x->width, spca50x->height,
				   GET_EXT_MODES (spca50x->bridge));
  if (index == -EINVAL)
    {
      PDEBUG (2, "Can't find proper mode?!!");
      return -EINVAL;
    }
  /* EXPERIMENTAL Wait until test Marek */
  index = index & 0x0F;
  spca50x_set_mode (spca50x, index, GET_EXT_MODES (spca50x->bridge));

  // spca50x_write_vector(spca50x, spca500_read_stats);

  /* is there a sensor here ? */
  spca5xxRegRead (spca50x->dev, 0, 0, 0x8a04, &Data, 1);
  PDEBUG (0, "Spca500 Sensor Address  0x%02X ", Data);

  /* setup qtable */
  switch (spca50x->desc)
    {
    case LogitechClickSmart310:
      //err = spca500_synch310(spca50x );

      spca5xxRegRead (spca50x->dev, 0, 0, 0x0d00, &Data, 1);

      /* set x multiplier */
      spca50x_reg_write (spca50x->dev, 0, 0x8001,
			 GET_EXT_MODES (spca50x->bridge)[index][3]);

      /* set y multiplier */
      spca50x_reg_write (spca50x->dev, 0, 0x8002,
			 GET_EXT_MODES (spca50x->bridge)[index][4]);

      /* use compressed mode, VGA, with mode specific subsample */
      spca50x_reg_write (spca50x->dev, 0, 0x8003,
			 ((GET_EXT_MODES (spca50x->bridge)[index][2] & 0x0f)
			  << 4));

      /* enable drop packet */
      if ((err = spca50x_reg_write (spca50x->dev, 0x00, 0x850a, 0x0001)) < 0)
	{
	  PDEBUG (2, "failed to enable drop packet");
	  return err;
	}
      err = spca50x_reg_write (spca50x->dev, 0x00, 0x8880, 3);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_reg_write failed");
	  return err;
	}
      err = spca50x_setup_qtable (spca50x,
				  0x00, 0x8800, 0x8840,
				  qtable_creative_pccam);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_setup_qtable failed");
	  return err;
	}

      //Init SDRAM - needed for SDRAM access
      spca50x_reg_write (spca50x->dev, 0x00, 0x870a, 0x04);



      /* switch to video camera mode */
      err = spca50x_reg_write (spca50x->dev, 0x00, 0x8000, 0x0004);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_reg_write camera mode failed");
	  return err;
	}
      wait_ms (500);
      if (spca50x_reg_readwait (spca50x->dev, 0, 0x8000, 0x44) != 0)
	{
	  PDEBUG (2, "spca50x_reg_readwait() failed");
	  return -EIO;
	}

      spca5xxRegRead (spca50x->dev, 0, 0, 0x816b, &Data, 1);
      spca50x_reg_write (spca50x->dev, 0x00, 0x816b, Data);

      err = spca500_synch310 (spca50x);

      spca50x_write_vector (spca50x, spca500_visual_defaults);

      /* set x multiplier */
      spca50x_reg_write (spca50x->dev, 0, 0x8001,
			 GET_EXT_MODES (spca50x->bridge)[index][3]);

      /* set y multiplier */
      spca50x_reg_write (spca50x->dev, 0, 0x8002,
			 GET_EXT_MODES (spca50x->bridge)[index][4]);

      /* use compressed mode, VGA, with mode specific subsample */
      spca50x_reg_write (spca50x->dev, 0, 0x8003,
			 ((GET_EXT_MODES (spca50x->bridge)[index][2] & 0x0f)
			  << 4));

      /* enable drop packet */
      if ((err = spca50x_reg_write (spca50x->dev, 0x00, 0x850a, 0x0001)) < 0)
	{
	  PDEBUG (2, "failed to enable drop packet");
	  return err;
	}
      err = spca50x_reg_write (spca50x->dev, 0x00, 0x8880, 3);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_reg_write failed");
	  return err;
	}
      err = spca50x_setup_qtable (spca50x,
				  0x00, 0x8800, 0x8840,
				  qtable_creative_pccam);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_setup_qtable failed");
	  return err;
	}
      //Init SDRAM - needed for SDRAM access
      spca50x_reg_write (spca50x->dev, 0x00, 0x870a, 0x04);


      /* switch to video camera mode */
      err = spca50x_reg_write (spca50x->dev, 0x00, 0x8000, 0x0004);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_reg_write camera mode failed");
	  return err;
	}

      if (spca50x_reg_readwait (spca50x->dev, 0, 0x8000, 0x44) != 0)
	{
	  PDEBUG (2, "spca50x_reg_readwait() failed");
	  return -EIO;
	}

      spca5xxRegRead (spca50x->dev, 0, 0, 0x816b, &Data, 1);
      spca50x_reg_write (spca50x->dev, 0x00, 0x816b, Data);

      break;

    case CreativePCCam300:	/* Creative PC-CAM 300 */
    case IntelPocketPCCamera:	/* FIXME: Temporary fix for Intel Pocket PC Camera - NWG (Sat 29th March 2003) */

      /* do a full reset */
      if ((err = spca500_full_reset (spca50x)) < 0)
	{
	  PDEBUG (2, "spca500_full_reset failed");
	  return err;
	}

      if ((err = spca50x_reg_write (spca50x->dev, 0xe0, 0x0000, 0x0000)) < 0)
	{
	  PDEBUG (2, "spca50x_reg_write() failed");
	  return err;
	}
      if ((err = spca50x_reg_readwait (spca50x->dev, 0x06, 0, 0)) < 0)
	{
	  PDEBUG (2, "spca50x_reg_readwait() failed");
	  return err;
	}

      /* enable drop packet */
      if ((err = spca50x_reg_write (spca50x->dev, 0x00, 0x850a, 0x0001)) < 0)
	{
	  PDEBUG (2, "failed to enable drop packet");
	  return err;
	}
      err = spca50x_reg_write (spca50x->dev, 0x00, 0x8880, 3);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_reg_write failed");
	  return err;
	}
      err = spca50x_setup_qtable (spca50x,
				  0x00, 0x8800, 0x8840,
				  qtable_creative_pccam);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_setup_qtable failed");
	  return err;
	}

      /* set x multiplier */
      spca50x_reg_write (spca50x->dev, 0, 0x8001,
			 GET_EXT_MODES (spca50x->bridge)[index][3]);

      /* set y multiplier */
      spca50x_reg_write (spca50x->dev, 0, 0x8002,
			 GET_EXT_MODES (spca50x->bridge)[index][4]);

      /* use compressed mode, VGA, with mode specific subsample */
      spca50x_reg_write (spca50x->dev, 0, 0x8003,
			 ((GET_EXT_MODES (spca50x->bridge)[index][2] & 0x0f)
			  << 4));

      spca50x_reg_write (spca50x->dev, 0x20, 0x0001, 0x0004);

      /* switch to video camera mode */
      err = spca50x_reg_write (spca50x->dev, 0x00, 0x8000, 0x0004);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_reg_write camera mode failed");
	  return err;
	}

      if (spca50x_reg_readwait (spca50x->dev, 0, 0x8000, 0x44) != 0)
	{
	  PDEBUG (2, "spca50x_reg_readwait() failed");
	  return -EIO;
	}

      spca5xxRegRead (spca50x->dev, 0, 0, 0x816b, &Data, 1);
      spca50x_reg_write (spca50x->dev, 0x00, 0x816b, Data);

      spca50x_write_vector (spca50x, spca500_visual_defaults);

      break;
    case KodakEZ200:		/* Kodak EZ200 */

      /* do a full reset */
      if ((err = spca500_full_reset (spca50x)) < 0)
	{
	  PDEBUG (2, "spca500_full_reset failed");
	  return err;
	}

      if ((err = spca50x_reg_write (spca50x->dev, 0xe0, 0x0000, 0x0000)) < 0)
	{
	  PDEBUG (2, "spca50x_reg_write() failed");
	  return err;
	}
      if ((err = spca50x_reg_readwait (spca50x->dev, 0x06, 0, 0)) < 0)
	{
	  PDEBUG (2, "spca50x_reg_readwait() failed");
	  return err;
	}

      /* enable drop packet */
      if ((err = spca50x_reg_write (spca50x->dev, 0x00, 0x850a, 0x0001)) < 0)
	{
	  PDEBUG (2, "failed to enable drop packet");
	  return err;
	}
      err = spca50x_reg_write (spca50x->dev, 0x00, 0x8880, 0);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_reg_write failed");
	  return err;
	}
      err = spca50x_setup_qtable (spca50x,
				  0x00, 0x8800, 0x8840, qtable_kodak_ez200);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_setup_qtable failed");
	  return err;
	}
      /* set x multiplier */
      spca50x_reg_write (spca50x->dev, 0, 0x8001,
			 GET_EXT_MODES (spca50x->bridge)[index][3]);

      /* set y multiplier */
      spca50x_reg_write (spca50x->dev, 0, 0x8002,
			 GET_EXT_MODES (spca50x->bridge)[index][4]);

      /* use compressed mode, VGA, with mode specific subsample */
      spca50x_reg_write (spca50x->dev, 0, 0x8003,
			 ((GET_EXT_MODES (spca50x->bridge)[index][2] & 0x0f)
			  << 4));

      spca50x_reg_write (spca50x->dev, 0x20, 0x0001, 0x0004);

      /* switch to video camera mode */
      err = spca50x_reg_write (spca50x->dev, 0x00, 0x8000, 0x0004);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_reg_write camera mode failed");
	  return err;
	}

      if (spca50x_reg_readwait (spca50x->dev, 0, 0x8000, 0x44) != 0)
	{
	  PDEBUG (2, "spca50x_reg_readwait() failed");
	  return -EIO;
	}

      spca5xxRegRead (spca50x->dev, 0, 0, 0x816b, &Data, 1);
      spca50x_reg_write (spca50x->dev, 0x00, 0x816b, Data);

      //spca50x_write_vector(spca50x, spca500_visual_defaults);

      break;

    case BenqDC1016:
    case DLinkDSC350:		/* FamilyCam 300 */
    case AiptekPocketDV:	/* Aiptek PocketDV */
    case Gsmartmini:		/*Mustek Gsmart Mini */
    case MustekGsmart300:	// Mustek Gsmart 300
    case PalmPixDC85:
    case Optimedia:
    case ToptroIndus:
    case AgfaCl20:
    
      spca500_reinit (spca50x);
      spca50x_reg_write (spca50x->dev, 0x00, 0x0d01, 0x01);
      /* enable drop packet */
      spca50x_reg_write (spca50x->dev, 0x00, 0x850a, 0x0001);

      err = spca50x_setup_qtable (spca50x,
				  0x00, 0x8800, 0x8840, qtable_pocketdv);
      if (err < 0)
	{
	  PDEBUG (2, "spca50x_setup_qtable failed");
	  return err;
	}
      spca50x_reg_write (spca50x->dev, 0x00, 0x8880, 2);

      /* familycam Quicksmart pocketDV stuff */
      spca50x_reg_write (spca50x->dev, 0x00, 0x800a, 0x00);
      //Set agc transfer: synced inbetween frames
      spca50x_reg_write (spca50x->dev, 0x00, 0x820f, 0x01);
      //Init SDRAM - needed for SDRAM access
      spca50x_reg_write (spca50x->dev, 0x00, 0x870a, 0x04);


      /* set x multiplier */
      spca50x_reg_write (spca50x->dev, 0, 0x8001,
			 GET_EXT_MODES (spca50x->bridge)[index][3]);

      /* set y multiplier */
      spca50x_reg_write (spca50x->dev, 0, 0x8002,
			 GET_EXT_MODES (spca50x->bridge)[index][4]);

      /* use compressed mode, VGA, with mode specific subsample */
      spca50x_reg_write (spca50x->dev, 0, 0x8003,
			 ((GET_EXT_MODES (spca50x->bridge)[index][2] & 0x0f)
			  << 4));

      /* switch to video camera mode */
      err = spca50x_reg_write (spca50x->dev, 0x00, 0x8000, 0x0004);


      spca50x_reg_readwait (spca50x->dev, 0, 0x8000, 0x44);

      spca5xxRegRead (spca50x->dev, 0, 0, 0x816b, &Data, 1);
      spca50x_reg_write (spca50x->dev, 0x00, 0x816b, Data);

      break;
    case LogitechTraveler:
    case LogitechClickSmart510:
      {

	spca50x_reg_write (spca50x->dev, 0x02, 0x00, 0x00);
	/* enable drop packet */
	if ((err =
	     spca50x_reg_write (spca50x->dev, 0x00, 0x850a, 0x0001)) < 0)
	  {
	    PDEBUG (2, "failed to enable drop packet");
	    return err;
	  }

	err = spca50x_setup_qtable (spca50x,
				    0x00, 0x8800,
				    0x8840, qtable_creative_pccam);
	if (err < 0)
	  {
	    PDEBUG (2, "spca50x_setup_qtable failed");
	    return err;
	  }
	err = spca50x_reg_write (spca50x->dev, 0x00, 0x8880, 3);
	if (err < 0)
	  {
	    PDEBUG (2, "spca50x_reg_write failed");
	    return err;
	  }
	spca50x_reg_write (spca50x->dev, 0x00, 0x800a, 0x00);
	//Init SDRAM - needed for SDRAM access
	spca50x_reg_write (spca50x->dev, 0x00, 0x870a, 0x04);
	/* set x multiplier */
	spca50x_reg_write (spca50x->dev, 0, 0x8001,
			   GET_EXT_MODES (spca50x->bridge)[index][3]);

	/* set y multiplier */
	spca50x_reg_write (spca50x->dev, 0, 0x8002,
			   GET_EXT_MODES (spca50x->bridge)[index][4]);

	/* use compressed mode, VGA, with mode specific subsample */
	spca50x_reg_write (spca50x->dev, 0, 0x8003,
			   ((GET_EXT_MODES (spca50x->bridge)[index][2] & 0x0f)
			    << 4));


	/* switch to video camera mode */
	err = spca50x_reg_write (spca50x->dev, 0x00, 0x8000, 0x0004);


	spca50x_reg_readwait (spca50x->dev, 0, 0x8000, 0x44);

	spca5xxRegRead (spca50x->dev, 0, 0, 0x816b, &Data, 1);
	spca50x_reg_write (spca50x->dev, 0x00, 0x816b, Data);
	spca50x_write_vector (spca50x, Clicksmart510_defaults);
      }

      break;

    default:
      return -EINVAL;
    }

  /* all ok */
  return 0;
}

static int
spca500_full_reset (struct usb_spca50x *spca50x)
{
  int err;

  /* send the reset command */
  err = spca50x_reg_write (spca50x->dev, 0xe0, 0x0001, 0x0000);
  if (err < 0)
    {
      return err;
    }

  /* wait for the reset to complete */
  err = spca50x_reg_readwait (spca50x->dev, 0x06, 0x0000, 0x0000);
  if (err < 0)
    {
      return err;
    }

  /* all ok */
  return 0;
}


/************************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
/****************************************************************************
 *  sysfs
 ***************************************************************************/

static inline struct usb_spca50x *
cd_to_spca50x (struct class_device *cd)
{
  struct video_device *vdev = to_video_device (cd);
  return video_get_drvdata (vdev);
}

static ssize_t
show_stream_id (struct class_device *cd, char *buf)
{
  struct usb_spca50x *spca50x = cd_to_spca50x (cd);
  return snprintf (buf, 5, "%s\n", Plist[spca50x->cameratype].name);
}

static CLASS_DEVICE_ATTR (stream_id, S_IRUGO, show_stream_id, NULL);

static ssize_t
show_model (struct class_device *cd, char *buf)
{
  struct usb_spca50x *spca50x = cd_to_spca50x (cd);
  return snprintf (buf, 32, "%s\n", (spca50x->desc) ?
		   clist[spca50x->desc].description : " Unknow ");
}

static CLASS_DEVICE_ATTR (model, S_IRUGO, show_model, NULL);

static ssize_t
show_pictsetting (struct class_device *cd, char *buf)
{
  struct usb_spca50x *spca50x = cd_to_spca50x (cd);
  struct pictparam *gcorrect = &spca50x->pictsetting;
  return snprintf (buf, 128,
		   "force_rgb=%d, gamma=%d, OffRed=%d, OffBlue=%d, OffGreen=%d, GRed=%d, GBlue=%d, GGreen= %d \n",
		   gcorrect->force_rgb, gcorrect->gamma, gcorrect->OffRed,
		   gcorrect->OffBlue, gcorrect->OffGreen, gcorrect->GRed,
		   gcorrect->GBlue, gcorrect->GGreen);
}

static CLASS_DEVICE_ATTR (pictsetting, S_IRUGO, show_pictsetting, NULL);
/*
static ssize_t store_RGB(struct class_device *cd, char *buf,size_t count)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	struct pictparam *gcorrect = &spca50x->pictsetting;
	int ret;
	ret= sscanf(buf,"%d",&gcorrect->force_rgb);
	if (ret != 1) return -EINVAL;
	gcorrect->change = 0x10;
	return strlen(buf);
} 
static CLASS_DEVICE_ATTR(RGB, S_IWUGO, NULL, store_RGB);

static ssize_t show_brightness(struct class_device *cd, char *buf)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	unsigned short x;

	if (!spca50x->dev)
		return -ENODEV;
	sensor_get_brightness(spca50x, &x);
	return sprintf(buf, "%d\n", x >> 8);
} 
static CLASS_DEVICE_ATTR(brightness, S_IRUGO, show_brightness, NULL);

static ssize_t show_saturation(struct class_device *cd, char *buf)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	unsigned short x;

	if (!spca50x->dev)
		return -ENODEV;
	sensor_get_saturation(spca50x, &x);
	return sprintf(buf, "%d\n", x >> 8);
} 
static CLASS_DEVICE_ATTR(saturation, S_IRUGO, show_saturation, NULL);

static ssize_t show_contrast(struct class_device *cd, char *buf)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	unsigned short x;

	if (!spca50x->dev)
		return -ENODEV;
	sensor_get_contrast(spca50x, &x);
	return sprintf(buf, "%d\n", x >> 8);
} 
static CLASS_DEVICE_ATTR(contrast, S_IRUGO, show_contrast, NULL);

static ssize_t show_hue(struct class_device *cd, char *buf)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	unsigned short x;

	if (!spca50x->dev)
		return -ENODEV;
	sensor_get_hue(spca50x, &x);
	return sprintf(buf, "%d\n", x >> 8);
} 
static CLASS_DEVICE_ATTR(hue, S_IRUGO, show_hue, NULL);

static ssize_t show_exposure(struct class_device *cd, char *buf)
{
	struct usb_spca50x *spca50x = cd_to_spca50x(cd);
	unsigned char exp;

	if (!spca50x->dev)
		return -ENODEV;
	sensor_get_exposure(spca50x, &exp);
	return sprintf(buf, "%d\n", exp >> 8);
} 
static CLASS_DEVICE_ATTR(exposure, S_IRUGO, show_exposure, NULL);
*/

static void
spca50x_create_sysfs (struct video_device *vdev)
{

  video_device_create_file (vdev, &class_device_attr_stream_id);
  video_device_create_file (vdev, &class_device_attr_model);

  video_device_create_file (vdev, &class_device_attr_pictsetting);
/*	
	video_device_create_file(vdev, &class_device_attr_RGB);

	video_device_create_file(vdev, &class_device_attr_brightness);
	video_device_create_file(vdev, &class_device_attr_saturation);
	video_device_create_file(vdev, &class_device_attr_contrast);
	video_device_create_file(vdev, &class_device_attr_hue);
	video_device_create_file(vdev, &class_device_attr_exposure);
*/
}
#endif
/****************************************************************************
 *
 *  USB routines
 *
 ***************************************************************************/
static int
spcaDetectCamera (struct usb_spca50x *spca50x)
{
  struct usb_device *dev = spca50x->dev;
  __u8 fw = 0;
  __u16 vendor;
  __u16 product;
  /* Is it a recognised camera ? */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
vendor = le16_to_cpu(dev->descriptor.idVendor);
product = le16_to_cpu(dev->descriptor.idProduct);
#else
vendor = dev->descriptor.idVendor;
product = dev->descriptor.idProduct;
#endif
  switch ( vendor )
    {
    case 0x0733:		/* Rebadged ViewQuest (Intel) and ViewQuest cameras */
      switch ( product )
	{
	case 0x430:
	  if (usbgrabber)
	    {
	      spca50x->desc = UsbGrabberPV321c;
	      spca50x->bridge = BRIDGE_SPCA506;
	      spca50x->sensor = SENSOR_SAA7113;
	      spca50x->header_len = SPCA50X_OFFSET_DATA;
	      spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	      spca50x->i2c_base = 0;
	      spca50x->i2c_trigger_on_write = 0;
	      spca50x->cameratype = YYUV;
	      info
		("USB SPCA5XX grabber found. Daemon PV321c(SPCA506+SAA7113)");
	    }
	  else
	    {
	      spca50x->desc = IntelPCCameraPro;
	      spca50x->bridge = BRIDGE_SPCA505;
	      spca50x->sensor = SENSOR_SAA7113;
	      spca50x->header_len = SPCA50X_OFFSET_DATA;
	      spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	      spca50x->i2c_base = 0;
	      spca50x->i2c_trigger_on_write = 0;
	      spca50x->cameratype = YYUV;
	      info
		("USB SPCA5XX camera found. Type Intel PC Camera Pro (SPCA505+SAA7113)");
	    }
	  break;
	  
	case 0x1314:
		spca50x->desc = Mercury21;
		spca50x->bridge = BRIDGE_SPCA533;
		spca50x->sensor = SENSOR_INTERNAL;
		spca50x->header_len = SPCA533_OFFSET_DATA;
		spca50x->i2c_ctrl_reg = 0;
		spca50x->i2c_base = 0;
		spca50x->i2c_trigger_on_write = 0;
		spca50x->cameratype = JPEG;
		info ("USB SPCA5XX camera found. Mercury Digital Pro 2.1Mp ");
		break;
		
	case 0x2211:
	  spca50x->desc = Jenoptikjdc21lcd;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Jenoptik JDC 21 LCD");
	  break;

	case 0x2221:
	  spca50x->desc = MercuryDigital;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Mercury Digital Pro 3.1Mp ");
	  break;

	case 0x1311:
	  spca50x->desc = Epsilon13;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Digital Dream Epsilon 1.3");
	  break;
	case 0x401:
	  spca50x->desc = IntelCreateAndShare;
	  spca50x->bridge = BRIDGE_SPCA501;	/* This is a guess. At least the chip looks closer to the 501 than the 505 */

	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA501_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YUYV;
	  info
	    ("USB SPCA5XX camera found. Type Intel Create and Share (SPCA501?+SAA7113?)");
	  break;
	case 0x402:
	  spca50x->desc = ViewQuestM318B;
	  spca50x->bridge = BRIDGE_SPCA501;	/* This is a guess. At least the chip looks closer to the 501 than the 505 */
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA501_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YUYV;
	  info ("USB SPCA5XX camera found.  ViewQuest M318B (SPCA501a)");
	  /*
	     Looks not perfectly but until we understand the difference
	     between spca501 and spca500 we'll treat them as one
	   */
	  break;
	case 0x110:
	  spca50x->desc = ViewQuestVQ110;
	  spca50x->bridge = BRIDGE_SPCA508;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA508_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = SPCA508_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = YUVY;
	  info
	    ("USB SPCA5XX camera found. Type ViewQuest (SPCA508?+SAA7113?)");
	  break;
	 case 0x3261:
	  spca50x->desc = Concord3045;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found.Concord 3045 Spca536 Mpeg4");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x0734:
      switch (product)
	{
	case 0x043b:
	  spca50x->desc = DeMonUSBCapture;
	  spca50x->bridge = BRIDGE_SPCA506;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YYUV;
	  info ("Detected DeMonUsbCapture  (SPCA506+SAA7113)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x99FA:		/* GrandTec cameras */
      switch (product)
	{
	case 0x8988:
	  spca50x->desc = GrandtecVcap;
	  spca50x->bridge = BRIDGE_SPCA506;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA501_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YYUV;
	  info
	    ("USB SPCA5XX camera found. Grandtec V.cap (SPCA506+SAA7113?)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x0AF9:		/* Hama cameras */
      switch (product)
	{
	case 0x0010:
	  spca50x->desc = HamaUSBSightcam;
	  spca50x->bridge = BRIDGE_SPCA508;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA508_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YUVY;
	  info
	    ("USB SPCA5XX camera found. Hama Sightcam 100 (SPCA508A+PAS106B)");
	  break;
	case 0x0011:
	  spca50x->desc = HamaUSBSightcam2;
	  spca50x->bridge = BRIDGE_SPCA508;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA508_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YUVY;
	  info
	    ("USB SPCA5XX camera found. Hama Sightcam 100 (2) (SPCA508A+? )");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x040A:		/* Kodak cameras */
      switch (product)
	{
	case 0x0002:
	  spca50x->desc = KodakDVC325;
	  spca50x->bridge = BRIDGE_SPCA501;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA501_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YUYV;
	  info
	    ("USB SPCA5XX camera found. Type Kodak DVC-325 (SPCA501A+SAA7113?)");
	  break;
	case 0x0300:

	  spca50x->desc = KodakEZ200;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Kodak EZ200 (SPCA500+unknown CCD)");
	  break;


	default:
	  goto error;
	};
      break;
    case 0x04a5:		/* Benq */
    case 0x08ca:		/* Aiptek */
    case 0x055f:		/* Mustek cameras */
    case 0x04fc:		/* SunPlus */
    case 0x052b:		/* ?? Megapix */
      switch (product)
	{
	case 0xc520:
	  spca50x->desc = MustekGsmartMini3;
	  spca50x->bridge = BRIDGE_SPCA504;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Mustek gSmart Mini 3(SPCA504A)");
	  break;
	case 0xc420:
	  spca50x->desc = MustekGsmartMini2;
	  spca50x->bridge = BRIDGE_SPCA504;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Mustek gSmart Mini 2(SPCA504A)");
	  break;

	case 0xc360:
	  spca50x->desc = MustekDV4000;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Mustek DV4000 Spca536 Mpeg4");
	  break;

	case 0xc211:
	  spca50x->desc = Bs888e;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Kowa Bs-888e Spca536 Mpeg4");
	  break;

	case 0xc005:		// zc302 chips 
	  spca50x->desc = Wcam300A;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Mustek Wcam300a Zc0301 ");
	  break;

	case 0xd003:		// zc302 chips 
	  spca50x->desc = MustekWcam300A;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Mustek PCCam300a Zc0301 ");
	  break;
	 case 0xd004:		// zc302 chips 
	  spca50x->desc = WCam300AN;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Mustek WCam300aN Zc0302 ");
	 break;
	case 0x504a:
	  /*try to get the firmware as some cam answer 2.0.1.2.2 
	     and should be a spca504b then overwrite that setting */
	  spca5xxRegRead (dev, 0x20, 0, 0, &fw, 1);
	  if (fw == 1)
	    {
	      spca50x->desc = AiptekMiniPenCam13;
	      spca50x->bridge = BRIDGE_SPCA504;
	      spca50x->sensor = SENSOR_INTERNAL;
	      spca50x->header_len = SPCA50X_OFFSET_DATA;
	      spca50x->i2c_ctrl_reg = 0;
	      spca50x->i2c_base = 0;
	      spca50x->i2c_trigger_on_write = 0;
	      spca50x->cameratype = JPEG;
	      info
		("USB SPCA5XX camera found. Type Aiptek mini PenCam 1.3(SPCA504A)");
	    }
	  else if (fw == 2)
	    {
	      spca50x->desc = Terratec2move13;
	      spca50x->bridge = BRIDGE_SPCA504B;
	      spca50x->sensor = SENSOR_INTERNAL;
	      spca50x->header_len = SPCA50X_OFFSET_DATA;
	      spca50x->i2c_ctrl_reg = 0;
	      spca50x->i2c_base = 0;
	      spca50x->i2c_trigger_on_write = 0;
	      spca50x->cameratype = JPEG;
	      info
		("USB SPCA5XX camera found. Terratec 2 move1.3(SPCA504A FW2)");
	    }
	  else
	    return -ENODEV;
	  break;

	case 0x2018:

	  spca50x->desc = AiptekPenCamSD;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PenCam SD(SPCA504A FW2)");
	  break;

	case 0x2008:

	  spca50x->desc = AiptekMiniPenCam2;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PenCam 2M(SPCA504A FW2)");
	  break;

	case 0x504b:

	  spca50x->desc = MaxellMaxPocket;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Maxell MaxPocket 1.3 (SPCA504A FW2)");
	  break;

	case 0xffff:

	  spca50x->desc = PureDigitalDakota;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Pure Digital Dakota (SPCA504A FW2)");
	  break;

	case 0x0103:

	  spca50x->desc = AiptekPocketDV;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PocketDV");
	  break;

	case 0x0104:

	  spca50x->desc = AiptekPocketDVII;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PocketDVII 1.3Mp");
	  break;

	case 0x0106:

	  spca50x->desc = AiptekPocketDV3100;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PocketDV3100+");
	  break;

	case 0x5330:

	  spca50x->desc = Digitrex2110;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. ApexDigital Digitrex 2110 spca533");
	  break;

	case 0x2022:

	  spca50x->desc = AiptekSlim3200;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found type: Aiptek Slim3 200 spca533");
	  break;
	  
	case 0x2028:

	  spca50x->desc = AiptekPocketCam4M;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found type: Aiptek PocketCam 4M spca533");
	  break; 
	   
	case 0x5360:
	
	  spca50x->desc = SunplusGeneric536;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek Generic spca536a");
	  break;
	  
	case 0x2024:

	  spca50x->desc = AiptekDV3500;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek DV3500 Mpeg4");
	  break;
	  
	case 0x2042:
	
	  spca50x->desc = AiptekPocketDV5100;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek DV5100 Mpeg4");
	  break;
	  
	  case 0x2060:
	
	  spca50x->desc = AiptekPocketDV5300;
	  spca50x->bridge = BRIDGE_SPCA536;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA536_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek DV5300 Mpeg4");
	  break;
	  
	case 0x3008:

	  spca50x->desc = BenqDC1500;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Benq DC 1500 Spca533");
	  break;

	case 0x3003:

	  spca50x->desc = BenqDC1300;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Benq DC 1300 Spca504b");
	  break;

	case 0x300a:

	  spca50x->desc = BenqDC3410;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Benq DC 3410 Spca533");
	  break;

	case 0x300c:

	  spca50x->desc = BenqDC1016;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Benq DC 1016 Spca500c ");
	  break;
	case 0x2010:

	  spca50x->desc = AiptekPocketCam3M;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Aiptek PocketCam 3M");
	  break;

	case 0x2016:

	  spca50x->desc = AiptekPocketCam2M;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Aiptek PocketCam 2 Mega (SPCA504A FW2)");
	  break;
	case 0x0561:

	  spca50x->desc = Flexcam100Camera;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info ("USB SPCA5XX camera found. Type Flexcam 100 (SPCA561A)");
	  break;

	case 0xc200:
	  spca50x->desc = MustekGsmart300;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Mustek Gsmart 300");
	  break;
	  
	case 0x7333:
	  spca50x->desc = PalmPixDC85;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. PalmPix DC85");
	  break;
	  
	case 0xc220:
	  spca50x->desc = Gsmartmini;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Mustek Gsmart Mini Spca500c ");
	  break;

	case 0xc530:
	  spca50x->desc = MustekGsmartLCD3;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Mustek Gsmart LCD 3");
	  break;

	case 0xc430:
	  spca50x->desc = MustekGsmartLCD2;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Mustek Gsmart LCD 2");
	  break;

	case 0xc440:

	  spca50x->desc = MustekDV3000;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. DV3000");
	  break;

	case 0xc540:

	  spca50x->desc = GsmartD30;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found.Mustek Gsmart D30");
	  break;
	case 0xc650:

	  spca50x->desc = MustekMDC5500Z;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found Mustek MDC5500Z");
	  break;

	case 0x1513:


	  spca50x->desc = MegapixV4;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found Megapix V4");

	  break;

	default:
	  goto error;
	};
      break;
    case 0x046d:		/* Logitech Labtec*/
    case 0x041E:		/* Creative cameras */
      switch (product)
	{
	case 0x400A:

	  spca50x->desc = CreativePCCam300;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Creative PC-CAM 300 (SPCA500+unknown CCD)");
	  break;

	case 0x0890:

	  spca50x->desc = LogitechTraveler;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Logitech QuickCam Traveler (SPCA500+unknown CCD)");
	  break;
	case 0x08a0:
	  spca50x->desc = QCim;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC IM ");
	  break;
	  
	case 0x08a1:
	
	  spca50x->desc = QCimA1;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC IM ");
	  break;
	  
	case 0x08a2:		// zc302 chips 
	
	  spca50x->desc = LabtecPro;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_HDCS2020;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Labtec Webcam Pro Zc0302 + Hdcs2020");
	  break;
	  
	case 0x08a3:
	
	  spca50x->desc = QCchat;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC Chat ");
	  break;
	case 0x08ae:
	
	  spca50x->desc = QuickCamNB;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_HDCS2020;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC for Notebooks ");
	  break;  
	case 0x08b9:
	  spca50x->desc = QCimB9;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Logitech QC IM ??? ");
	  break;
	  
	case 0x0900:

	  spca50x->desc = LogitechClickSmart310;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_HDCS1020;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Logitech ClickSmart 310 (SPCA551+ Agilent HDCS1020)");
	  break;
	  
	case 0x0901:

	  spca50x->desc = LogitechClickSmart510;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Logitech ClickSmart 510 (SPCA500+unknown CCD)");
	  break;

	case 0x0905:

	  spca50x->desc = LogitechClickSmart820;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Logitech ClickSmart 820 (SPCA533+unknown CCD)");
	  break;

	case 0x400B:
	  spca50x->desc = CreativePCCam600;
	  spca50x->bridge = BRIDGE_SPCA504_PCCAM600;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA504_PCCAM600_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Creative PC-CAM 600 (SPCA504+unknown CCD)");
	  break;
	case 0x4013:
	  spca50x->desc = CreativePccam750;
	  spca50x->bridge = BRIDGE_SPCA504_PCCAM600;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA504_PCCAM600_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Creative PC-CAM 750 (SPCA504+unknown CCD)");
	  break;
	case 0x0960:
	  spca50x->desc = LogitechClickSmart420;
	  spca50x->bridge = BRIDGE_SPCA504_PCCAM600;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA504_PCCAM600_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Logitech Clicksmart 420 (SPCA504+unknown CCD)");
	  break;
	case 0x4018:
	  spca50x->desc = CreativeVista;
	  spca50x->bridge = BRIDGE_SPCA508;
	  spca50x->sensor = SENSOR_PB100_BA;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YUVY;
	  info
	    ("USB SPCA5XX camera found. Type Creative Vista (SPCA508A+PB100)");
	  break;
	case 0x401d:		//here505b
	  spca50x->desc = Nxultra;
	  spca50x->bridge = BRIDGE_SPCA505;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YYUV;
	  info
	    ("USB SPCA5XX camera found. Type Creative Webcam NX Ultra (SPCA505b+unknown CCD)");
	  break;
	case 0x401c:		// zc301 chips 
	  spca50x->desc = CreativeNX;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_PAS106;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative NX Zc301+ CCD PAS106B");
	  break;
	case 0x401e:		// zc301 chips 
	  spca50x->desc = CreativeNxPro;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_HV7131B;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative NX Pro Zc301+hv7131b");
	  break;
	case 0x4034:		// zc301 chips 
	  spca50x->desc = CreativeInstant1;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_PAS106;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Instant P0620");
	  break; 
	case 0x4035:		// zc301 chips 
	  spca50x->desc = CreativeInstant2;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_PAS106;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Instant P0620D");
	  break;    
	case 0x403a:
	  spca50x->desc = CreativeNxPro2;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Nx Pro FW2 Zc301+Tas5130c");
	  break;
	case 0x403b:
	  spca50x->desc = CreativeVista3b;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info ("USB SPCA5XX camera found. Creative Vista VF0010 (SPCA561A)");
	  break;
	case 0x4036:
	  spca50x->desc = CreativeLive;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Live! Zc301+Tas5130c");
	  break;
	case 0x401f:		// zc301 chips 
	  spca50x->desc = CreativeNotebook;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Webcam Notebook Zc301+Tas5130c");
	  break;
	case 0x4017:		// zc301 chips 
	  spca50x->desc = CreativeMobile;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_ICM105A;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Creative Webcam Mobile Zc301+Icm105a");
	  break;
	case 0x0920:
	  spca50x->desc = QCExpress;
	  spca50x->bridge = BRIDGE_TV8532;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = 4;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = GBGR;
	  info ("USB SPCA5xx camera found. Type QC Express (unknown CCD)");
	  break;
	case 0x0921:
	  spca50x->desc = LabtecWebcam;
	  spca50x->bridge = BRIDGE_TV8532;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = 4;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = GBGR;
	  info ("USB SPCA5xx camera found. Type Labtec Webcam (unknown CCD)");
	  break;
	case 0x0928:
	  spca50x->desc = QCExpressEtch2;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info
	    ("USB SPCA5XX camera found.Logitech QuickCam Express II(SPCA561A)");
	  break;
	case 0x0929:
	  spca50x->desc = Labtec929;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info
	    ("USB SPCA5XX camera found.Labtec WebCam Elch 2(SPCA561A)");
	  break;
	case 0x092a:
	  spca50x->desc = QCforNotebook;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info
	    ("USB SPCA5XX camera found.Logitech QuickCam for Notebook (SPCA561A)");
	  break;
	case 0x092b:
	  spca50x->desc = LabtecWCPlus;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info
	    ("USB SPCA5XX camera found.Labtec Webcam Plus (SPCA561A)");
	  break;
	case 0x092c:
	  spca50x->desc = LogitechQC92c;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info
	    ("USB SPCA5XX camera found.Logitech QuickCam chat (SPCA561A)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x0AC8:		/* Vimicro z-star */
      switch (product)
	{
	case 0x301b:		/* Wasam 350r */
	  spca50x->desc = Vimicro;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_PB0330;	//overwrite by the sensor detect routine
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info ("USB SPCA5XX camera found. Type Vimicro Zc301P ");

	  break;
	case 0x0302:		/* Generic */
	  spca50x->desc = Zc302;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_ICM105A;	//overwrite by the sensor detect routine
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info ("USB SPCA5XX camera found. Type Vimicro Zc302 ");

	  break;

	default:
	  goto error;
	};
      break;
    case 0x084D:		/* D-Link / Minton */
      switch (product)
	{
	case 0x0003:		/* DSC-350 / S-Cam F5 */

	  spca50x->desc = DLinkDSC350;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type D-Link DSC-350 / Minton S-Cam F5 (SPCA500+unknown CCD)");
	  break;

	default:
	  goto error;
	};
      break;
    case 0x0923:		/* ICM532 cams */
      switch (product)
	{
	case 0x010f:
	  spca50x->desc = ICM532cam;
	  spca50x->bridge = BRIDGE_TV8532;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = 4;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = GBGR;
	  info ("USB SPCA5xx camera found. Type ICM532 cam (unknown CCD)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x0545:		/* tv8532 cams */
      switch (product)
	{
	case 0x808b:
	  spca50x->desc = VeoStingray2;
	  spca50x->bridge = BRIDGE_TV8532;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = 4;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = GBGR;
	  info ("USB SPCA5xx camera found. Type Veo Stingray (unknown CCD)");
	  break;
	case 0x8333:
	  spca50x->desc = VeoStingray1;
	  spca50x->bridge = BRIDGE_TV8532;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = 4;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = GBGR;
	  info ("USB SPCA5xx camera found. Type Veo Stingray (unknown CCD)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x102c:	/* Etoms */
      switch (product) {
	case 0x6151:
	  spca50x->desc = Etoms61x151;
	  spca50x->bridge = BRIDGE_ETOMS;
	  spca50x->sensor = SENSOR_PAS106;
	  spca50x->header_len = 0;
	  spca50x->cameratype =GBRG ;
	  info ("USB Etx61xx51 camera found.Qcam Sangha Et61x151+Pas 106 ");
	break;
	case 0x6251:
	  spca50x->desc = Etoms61x251;
	  spca50x->bridge = BRIDGE_ETOMS;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = 0;
	  spca50x->cameratype =GBRG ;
	  info ("USB Etx61xx51 camera found.Qcam xxxxxx Et61x251+Tas 5130c");
	break;
	default:
	 goto error;
	};
      break;
    case 0x1776:		/* Arowana */
      switch (product)
	{
	case 0x501c:		/* Arowana 300k CMOS Camera */
	  spca50x->desc = Arowana300KCMOSCamera;
	  spca50x->bridge = BRIDGE_SPCA501;
	  spca50x->sensor = SENSOR_HV7131B;
	  spca50x->header_len = SPCA501_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YUYV;
	  info
	    ("USB SPCA5XX camera found. Type Arowana 300k CMOS Camera (SPCA501C+HV7131B)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x0000:		/* Unknow Camera */
      switch (product)
	{
	case 0x0000:		/* UnKnow from Ori CMOS Camera */
	  spca50x->desc = MystFromOriUnknownCamera;
	  spca50x->bridge = BRIDGE_SPCA501;
	  spca50x->sensor = SENSOR_HV7131B;
	  spca50x->header_len = SPCA501_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YUYV;
	  info
	    ("USB SPCA5XX camera found. UnKnow CMOS Camera (SPCA501C+HV7131B)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x8086:		/* Intel */
      switch (product)
	{
	case 0x0110:
	  spca50x->desc = IntelEasyPCCamera;
	  spca50x->bridge = BRIDGE_SPCA508;
	  spca50x->sensor = SENSOR_PB100_BA;
	  spca50x->header_len = SPCA508_OFFSET_DATA;

	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA508_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = YUVY;
	  info
	    ("USB SPCA5XX camera found. Type Intel Easy PC Camera CS110 (SPCA508+PB100)");
	  break;
	case 0x0630:		/* Pocket PC Camera */

	  spca50x->desc = IntelPocketPCCamera;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Intel Pocket PC Camera (SPCA500+unknown CCD)");
	  break;

	default:
	  goto error;
	};
      break;
    case 0x0506:		/* 3COM cameras */
      switch (product)
	{
	case 0x00DF:
	  spca50x->desc = ThreeComHomeConnectLite;
	  spca50x->bridge = BRIDGE_SPCA501;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA501_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YUYV;
	  info
	    ("USB SPCA5XX camera found. Type 3Com HomeConnect Lite (SPCA501A+?)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x0458:		/* Genius KYE cameras */
      switch (product)
	{
	case 0x7004:
	  spca50x->desc = GeniusVideoCAMExpressV2;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info
	    ("USB SPCA5XX camera found. Type Genius VideoCAM Express V2 (SPCA561A)");
	  break;
	case 0x7006:
	  spca50x->desc = GeniusDsc13;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info
	    ("USB SPCA5XX camera found. Type Genius DSC 1.3 Smart Spca504B");
	  break;
	case 0x7007:		// zc301 chips 
	  spca50x->desc = GeniusVideoCamV2;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Genius VideoCam V2 Zc301+Tas5130c");
	  break;

	case 0x700c:		// zc301 chips 
	  spca50x->desc = GeniusVideoCamV3;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Genius VideoCam V3 Zc301+Tas5130c");
	  break;

	case 0x700f:		// zc301 chips 
	  spca50x->desc = GeniusVideoCamExpressV2b;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Genius VideoCam Express V2 Zc301+Tas5130c");
	  break;
	default:
	  goto error;
	};
      break;
    case 0xabcd:		/* PetCam  */
      switch (product)
	{
	case 0xcdee:
	  spca50x->desc = PetCam;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info ("USB SPCA5XX camera found. Type Petcam (SPCA561A)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x060b:		/* Maxell  */
      switch (product)
	{
	case 0xa001:
	  spca50x->desc = MaxellCompactPM3;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info
	    ("USB SPCA5XX camera found. Type Maxell Compact PCPM3 (SPCA561A)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x10fd:		/* FlyCam usb 100  */
      switch (product)
	{
	case 0x7e50:
	  spca50x->desc = Flycam100Camera;
	  spca50x->bridge = BRIDGE_SPCA561;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA561_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA561_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = GBRG;
	  info ("USB SPCA5XX camera found. FlyCam Usb100 (SPCA561A)");
	  break;
	case 0x0128:
	case 0x8050:		// zc301 chips
	  spca50x->desc = TyphoonWebshotIIUSB300k;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Typhoon Webshot II Zc301p Tas5130c");
	  break;

	default:
	  goto error;
	};
      break;
    case 0x0461:		/* MicroInnovation  */
      switch (product)
	{
	case 0x0815:
	  spca50x->desc = MicroInnovationIC200;
	  spca50x->bridge = BRIDGE_SPCA508;
	  spca50x->sensor = SENSOR_PB100_BA;
	  spca50x->header_len = SPCA508_OFFSET_DATA;

	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = SPCA508_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 1;
	  spca50x->cameratype = YUVY;
	  info
	    ("USB SPCA5XX camera found. Type MicroInnovation IC200 (SPCA508+PB100)");
	  break;
	case 0x0a00:		// zc301 chips 
	  spca50x->desc = WebCam320;
	  spca50x->bridge = BRIDGE_ZC3XX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGH;
	  info
	    ("USB SPCA5XX camera found. Type Micro Innovation PC Cam 300A Zc301");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x06e1:		/* ADS Technologies  */
      switch (product)
	{
	case 0xa190:
	  spca50x->desc = ADSInstantVCD;
	  spca50x->bridge = BRIDGE_SPCA506;
	  spca50x->sensor = SENSOR_SAA7113;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;	//SPCA508_INDEX_I2C_BASE;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = YYUV;
	  info
	    ("USB SPCA5XX camera found. Type ADS Instant VCD (SPCA506+SAA7113)");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x05da:		/* Digital Dream cameras */
      switch (product)
	{
	case 0x1018:
	  spca50x->desc = Enigma13;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;

	  info ("USB SPCA5XX camera found. Digital Dream Enigma 1.3");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x0c45:		/* Sonix6025 TAS 5130d1b */
      switch (product)
	{
	case 0x6025:
	  spca50x->desc = Sonix6025;
	  spca50x->bridge = BRIDGE_SONIX;
	  spca50x->sensor = SENSOR_TAS5130C;
	  spca50x->header_len = 0;
	  //SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = SN9C;
	  info ("USB SPCA5XX camera found. SONIX sn9c102 +Tas 5130d1b ");
	  break;
	case 0x602c: 
	case 0x602e:
	  spca50x->desc = GeniusVideoCamMessenger;
	  spca50x->bridge = BRIDGE_SONIX;
	  spca50x->sensor = SENSOR_OV7630;
	  spca50x->header_len = 0;
	  //SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = SN9C;
	  info ("USB SPCA5XX camera found. SONIX sn9c101 Ov7630 ");
	  break;
	case 0x6009:
	case 0x600d:
	case 0x6029:
	  spca50x->desc = Sonix6029;
	  spca50x->bridge = BRIDGE_SONIX;
	  spca50x->sensor = SENSOR_PAS106;
	  spca50x->header_len = 0;
	  //SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = SN9C;	//GBRG ;
	  info ("USB SPCA5XX camera found. SONIX sn9c102 + Pas106 ");
	  break;
	case 0x607c:
	  spca50x->desc = SonixWC311P;
	  spca50x->bridge = BRIDGE_SN9C102P;
	  spca50x->sensor = SENSOR_HV7131R;
	  spca50x->header_len = 0;
	  //SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = SPCA50X_REG_I2C_CTRL;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGS;	// jpeg 4.2.2 whithout header ;
	  info ("USB SPCA5XX camera found. SONIX sn9c102p + Hv7131R ");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x0546:		/* Polaroid */
      switch (product)
	{
	case 0x3273:
	  spca50x->desc = PolaroidPDC2030;
	  spca50x->bridge = BRIDGE_SPCA504B;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA50X_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;

	  info ("USB SPCA5XX camera found. Polaroid PDC 2030");
	  break;

	case 0x3155:

	  spca50x->desc = PolaroidPDC3070;
	  spca50x->bridge = BRIDGE_SPCA533;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA533_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;
	  info ("USB SPCA5XX camera found. Polaroid PDC 3070");
	  break;


	default:
	  goto error;
	};
      break;
    case 0x0572:		/* Connexant */
      switch (product)
	{
	case 0x0041:
	  spca50x->desc = CreativeNoteBook2;
	  spca50x->bridge = BRIDGE_CX11646;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = 0;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGC;

	  info ("USB SPCA5XX camera found. Creative NoteBook PD1170");
	  break;
	default:
	  goto error;
	};
	break;
    case 0x06be:		/* Optimedia */
      switch (product)
	{
	case 0x0800:
	  spca50x->desc = Optimedia;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;

	  info ("USB SPCA5XX camera found. Optimedia spca500a");
	  break;
	default:
	  goto error;
	};
      break;
    case 0x2899:		/* ToptroIndustrial */
      switch (product)
	{
	case 0x012c:
	  spca50x->desc = ToptroIndus;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;

	  info ("USB SPCA5XX camera found. Toptro Industrial spca500a");
	  break;
	default:
	  goto error;
	};
      break;
     case 0x06bd:		/* Agfa Cl20 */
      switch (product)
	{
	case 0x0404:
	  spca50x->desc = AgfaCl20;
	  spca50x->bridge = BRIDGE_SPCA500;
	  spca50x->sensor = SENSOR_INTERNAL;
	  spca50x->header_len = SPCA500_OFFSET_DATA;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPEG;

	  info ("USB SPCA5XX camera found. Agfa ephoto CL20 spca500a");
	  break;
	default:
	  goto error;
	};
      break;
      case 0x093a:		/* Mars-semi */
      switch (product)
	{
	case 0x050f:
	  spca50x->desc = Pcam;
	  spca50x->bridge = BRIDGE_MR97311;
	  spca50x->sensor = SENSOR_MI0360;
	  spca50x->header_len = 16;
	  spca50x->i2c_ctrl_reg = 0;
	  spca50x->i2c_base = 0;
	  spca50x->i2c_trigger_on_write = 0;
	  spca50x->cameratype = JPGM;

	  info ("USB SPCA5XX camera found.Mars-Semi MR97311 MI0360 ");
	  break;
	default:
	  goto error;
	};
      break;
    default:
      goto error;
    }
  return 0;
error:
  return -ENODEV;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static int
spca5xx_probe (struct usb_interface *intf, const struct usb_device_id *id)
#else
static void *
spca5xx_probe (struct usb_device *dev, unsigned int ifnum,
	       const struct usb_device_id *id)
#endif
{
  struct usb_interface_descriptor *interface;
  struct usb_spca50x *spca50x;
  int err_probe;
  int i;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
  struct usb_device *dev = interface_to_usbdev (intf);
#endif
  /* We don't handle multi-config cameras */
  if (dev->descriptor.bNumConfigurations != 1)
    goto nodevice;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  /* 2.4.x test Only work on Interface 0 */
  if (ifnum > 0)
    goto nodevice;

  interface = &dev->actconfig->interface[ifnum].altsetting[0];
  /* Since code below may sleep, we use this as a lock */
  MOD_INC_USE_COUNT;
#else
  /* 2.6.x test Only work on Interface 0 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,6)
  interface = &intf->cur_altsetting->desc;
#else
  interface = &intf->altsetting[0].desc;
#endif
  if (interface->bInterfaceNumber > 0)
    goto nodevice;
#endif


  if ((spca50x = kmalloc (sizeof (struct usb_spca50x), GFP_KERNEL)) == NULL)
    {
      err ("couldn't kmalloc spca50x struct");
      goto error;
    }

  memset (spca50x, 0, sizeof (struct usb_spca50x));

  spca50x->dev = dev;
  spca50x->iface = interface->bInterfaceNumber;
  if ((err_probe = spcaDetectCamera (spca50x)) < 0)
    {
      err (" Devices not found !! ");
      /* FIXME kfree spca50x and goto nodevice */
      goto error;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  PDEBUG (0, "Camera type %s ", Plist[spca50x->cameratype].name);
#endif

  for (i = 0; i < SPCA50X_NUMFRAMES; i++)
    init_waitqueue_head (&spca50x->frame[i].wq);
  init_waitqueue_head (&spca50x->wq);

  if (!spca50x_configure (spca50x))
    {
      spca50x->user = 0;
      init_MUTEX (&spca50x->lock);	/* to 1 == available */
      init_MUTEX (&spca50x->buf_lock);
      spca50x->v4l_lock = SPIN_LOCK_UNLOCKED;
      spca50x->buf_state = BUF_NOT_ALLOCATED;
    }
  else
    {
      err ("Failed to configure camera");
      goto error;
    }
  /* Init video stuff */
  spca50x->vdev = video_device_alloc ();
  if (!spca50x->vdev)
    goto error;
  memcpy (spca50x->vdev, &spca50x_template, sizeof (spca50x_template));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
  spca50x->vdev->dev = &dev->dev;
#endif
  video_set_drvdata (spca50x->vdev, spca50x);

  PDEBUG (2, "setting video device = %p, spca50x = %p", spca50x->vdev,
	  spca50x);

  if (video_register_device (spca50x->vdev, VFL_TYPE_GRABBER, video_nr) < 0)
    {
      err ("video_register_device failed");
      goto error;
    }
  /* test on disconnect */
  spca50x->present = 1;
  /* Workaround for some applications that want data in RGB
   * instead of BGR */
  if (spca50x->force_rgb)
    info ("data format set to RGB");
#ifdef CONFIG_PROC_FS
  create_proc_spca50x_cam (spca50x);
#endif /* CONFIG_PROC_FS */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
  spca50x->task.func = auto_bh;
  usb_set_intfdata (intf, spca50x);
  spca50x_create_sysfs (spca50x->vdev);

#else
  spca50x->task.sync = 0;
  spca50x->task.routine = auto_bh;
#endif
  spca50x->task.data = spca50x;
  spca50x->bh_requested = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_DEC_USE_COUNT;
  return spca50x;
#else
  return 0;
#endif

error:
  if (spca50x->vdev)
    {
      if (spca50x->vdev->minor == -1)
	video_device_release (spca50x->vdev);
      else
	video_unregister_device (spca50x->vdev);
      spca50x->vdev = NULL;
    }
  if (spca50x)
    {
      kfree (spca50x);
      spca50x = NULL;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_DEC_USE_COUNT;
  return NULL;
#else
  return -EIO;
#endif

nodevice:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  return NULL;
#else
  return -ENODEV;
#endif
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
static void
spca5xx_disconnect (struct usb_interface *intf)
{
  struct usb_spca50x *spca50x = usb_get_intfdata (intf);
#else
static void
spca5xx_disconnect (struct usb_device *dev, void *ptr)
{
  struct usb_spca50x *spca50x = (struct usb_spca50x *) ptr;
#endif
  int n;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_INC_USE_COUNT;
#endif
  if (!spca50x)
    return;
  down (&spca50x->lock);
  spca50x->present = 0;
  for (n = 0; n < SPCA50X_NUMFRAMES; n++)
    spca50x->frame[n].grabstate = FRAME_ABORTING;
  spca50x->curframe = -1;

  /* This will cause the process to request another frame */
  for (n = 0; n < SPCA50X_NUMFRAMES; n++)
    if (waitqueue_active (&spca50x->frame[n].wq))
      wake_up_interruptible (&spca50x->frame[n].wq);

  if (waitqueue_active (&spca50x->wq))
    wake_up_interruptible (&spca50x->wq);

  spca5xx_kill_isoc(spca50x);

  PDEBUG (3,"Disconnect Kill isoc done");
  up (&spca50x->lock);
  while(spca50x->user) 
  	schedule();
    {
      down (&spca50x->lock);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
      /* be sure close did not use &intf->dev ? */
      dev_set_drvdata (&intf->dev, NULL);
#endif
      /* We don't want people trying to open up the device */
      if (spca50x->vdev)
	video_unregister_device (spca50x->vdev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
      usb_driver_release_interface (&spca5xx_driver,
				    &spca50x->dev->actconfig->
				    interface[spca50x->iface]);
#endif
      spca50x->dev = NULL;
      up (&spca50x->lock);
#ifdef CONFIG_PROC_FS
      destroy_proc_spca50x_cam (spca50x);
#endif /* CONFIG_PROC_FS */

      /* Free the memory */
      if (spca50x && !spca50x->user)
	{
	  spca5xx_dealloc (spca50x);
	  kfree (spca50x);
	  spca50x = NULL;
	}
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  MOD_DEC_USE_COUNT;
#endif
  PDEBUG (3, "Disconnect complete");

}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
static struct usb_driver spca5xx_driver = {
  .owner = THIS_MODULE,
  .name = "spca5xx",
  .id_table = device_table,
  .probe = spca5xx_probe,
  .disconnect = spca5xx_disconnect
};
#else
static struct usb_driver spca5xx_driver = {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,20)
	  THIS_MODULE,
#endif
	"spca5xx",
	spca5xx_probe,
	spca5xx_disconnect,
	{NULL,NULL}
};
#endif


/****************************************************************************
 *
 *  Module routines
 *
 ***************************************************************************/

static int __init
usb_spca5xx_init (void)
{
#ifdef CONFIG_PROC_FS
  proc_spca50x_create ();
#endif /* CONFIG_PROC_FS */

  if (usb_register (&spca5xx_driver) < 0)
    return -1;

  info ("spca5xx driver %s registered", version);

  return 0;
}

static void __exit
usb_spca5xx_exit (void)
{
  usb_deregister (&spca5xx_driver);
  info ("driver spca5xx deregistered");

#ifdef CONFIG_PROC_FS
  proc_spca50x_destroy ();
#endif /* CONFIG_PROC_FS */
}

module_init (usb_spca5xx_init);
module_exit (usb_spca5xx_exit);

//eof
