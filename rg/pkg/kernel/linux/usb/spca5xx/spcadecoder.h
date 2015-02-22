
#ifndef SPCADECODER_H
#define SPCADECODER_H

#include "spca5xx.h"
/*********************************/


int spca50x_outpicture (struct spca50x_frame *myframe);
void init_jpeg_decoder(struct usb_spca50x *spca50x, unsigned int qIndex);
void init_sonix_decoder(void);



#endif /* SPCADECODER_H */
