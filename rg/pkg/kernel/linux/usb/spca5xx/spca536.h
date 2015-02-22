/*
 * SPCA536 chip based cameras initialization data
 *
 */
#ifndef SPCA536_H
#define SPCA536_H


/* Frame packet header offsets for the spca536 */
#define SPCA536_OFFSET_DATA      4
#define SPCA536_OFFSET_FRAMSEQ	 1
/*********************** Specific spca536           ************************/

static void spca536_init(struct usb_spca50x *spca50x );	
static void spca536_start(struct usb_spca50x *spca50x);
static void spca536_stop (struct usb_spca50x *spca50x);
static void spca536_SetSizeType(struct usb_spca50x *spca50x,
			 __u16 extSize,
			 __u16 extType); 
/***************************************************************************/
static void spca536_setContBrigHueRegisters (struct usb_spca50x *spca50x)
{	
	int rc ;
	spca5xxRegWrite(spca50x->dev,0 ,0 ,0x20f0 ,NULL ,0 );
	spca5xxRegWrite(spca50x->dev,0 ,0x21 ,0x20f1 ,NULL ,0 );
	spca5xxRegWrite(spca50x->dev,0 ,0x40 ,0x20f5 ,NULL ,0 );
	spca5xxRegWrite(spca50x->dev,0 ,1 ,0x20f4 ,NULL ,0 );
	spca5xxRegWrite(spca50x->dev,0 ,0x40 ,0x20f6 ,NULL ,0 );
	spca5xxRegWrite(spca50x->dev,0 ,0 ,0x2089 ,NULL ,0 );
	rc = spca504B_PollingDataReady( spca50x->dev );
	return ;
}

static void spca536_init(struct usb_spca50x *spca50x )
{	
	int rc ;
	__u8 Data = 0;
	spca50x_GetFirmware( spca50x );
	spca5xxRegRead(spca50x->dev,0x00 ,0 ,0x5002 , &Data ,1);
	Data = 0;
	spca5xxRegWrite(spca50x->dev,0x24 ,0 ,0 ,&Data ,1 );
	spca5xxRegRead(spca50x->dev,0x24 ,0 ,0 , &Data ,1);
	rc = spca504B_PollingDataReady( spca50x->dev );
	spca5xxRegWrite(spca50x->dev,0x34 ,0 ,0 ,NULL ,0 );
	spca504B_WaitCmdStatus(spca50x);	
}
	
static void spca536_start(struct usb_spca50x *spca50x)
{
	spca5xxRegWrite(spca50x->dev,0x31,0 ,4 ,NULL ,0 );
	spca504B_WaitCmdStatus(spca50x);
	spca536_setContBrigHueRegisters ( spca50x );
}

static void spca536_stop (struct usb_spca50x *spca50x)
{	/* stop cam */
	spca5xxRegWrite(spca50x->dev,0x31,0 ,0 ,NULL ,0 );
	spca504B_WaitCmdStatus(spca50x);
	/* set cam Idle */
	//spca5xxRegWrite(spca50x->dev,0x32,0 ,0 ,NULL ,0 );
	//spca504B_WaitCmdStatus(spca50x);
	/* Gpio Power off */
	//spca5xxRegWrite(spca50x->dev,0x34,0 ,1 ,NULL ,0 );
	//spca504B_WaitCmdStatus(spca50x);

}
static void spca536_SetSizeType(struct usb_spca50x *spca50x,
			 __u16 extSize,
			 __u16 extType)
{
	__u8 Size ;
	__u8 Type ;
	int rc;
	Size = (__u8) extSize;
	Type = (__u8) extType;
		spca5xxRegWrite(spca50x->dev,0x25,0 ,4 ,&Size ,1 );
		spca5xxRegRead(spca50x->dev,0x25,0 ,4 ,&Size ,1 );
		spca5xxRegWrite(spca50x->dev,0x27,0 ,0 ,&Type ,1 );
		spca5xxRegRead(spca50x->dev,0x27,0 ,0 ,&Type ,1 );
		
		rc = spca504B_PollingDataReady ( spca50x->dev );
} 

#endif /* SPCA536_H */
