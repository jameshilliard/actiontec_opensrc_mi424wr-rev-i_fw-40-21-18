/*
 * SPCA533 chip based cameras initialization data
 *
 */
#ifndef SPCA533_H
#define SPCA533_H
/* Frame packet header offsets for the spca533 */
#define SPCA533_OFFSET_DATA      16
#define SPCA533_OFFSET_FRAMSEQ	15
/*********************** Specific spca533 MegapixV4  ************************/

static void spca533_Megapix(struct usb_spca50x *spca50x );
static void spca533_Megapix(struct usb_spca50x *spca50x )
{
	
	__u8 Type ;
	__u8 DataReady;
	
	
	if ( spca50x->bridge == BRIDGE_SPCA533) {
		
		spca50x_GetFirmware( spca50x );
		Type = 2;
		spca5xxRegWrite(spca50x->dev,0x24,0 ,8 ,&Type ,1 );
		spca5xxRegRead(spca50x->dev,0x24,0 ,8 ,&Type ,1 );
		spca5xxRegRead(spca50x->dev,0x21,0, 0, &DataReady,1);
		spca5xxRegWrite(spca50x->dev,0xF0 ,0 ,0 ,NULL ,0 );
		spca504B_WaitCmdStatus( spca50x );
		spca5xxRegRead(spca50x->dev,0xF0 ,0 ,4 ,NULL ,0 );
		spca504B_WaitCmdStatus( spca50x );	
	}
	
	return ;
}


#endif /* SPCA533_H */
