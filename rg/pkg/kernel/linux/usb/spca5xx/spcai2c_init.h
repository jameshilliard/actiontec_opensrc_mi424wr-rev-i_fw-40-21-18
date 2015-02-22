/*
 * I2C initialization data
 *
 */
#ifndef SPCA50X_I2C_INIT_H
#define SPCA50X_I2C_INIT_H

/* I assume that they think all registers are cleared to zero on a 7113 reset so
 * don't explicity write the zeros. These comments are accurate as Philips documents the 7113.
 * Data to initialize the SAA7113 (seems to only be used in external mode)
 * As this data is specific to the SAA7113 it applies to all SPCA50X twinned with a 7113
 */
static __u16 spca50x_i2c_data[][2] =
{
  { 0x1,0x8 }, /* Increment delay */
  { 0x2,0xc2 }, /* Analog input control 1 */
  { 0x3,0x33 }, /* Analog input control 2 */
  { 0x6,0xD },  /* Horizontal sync start */
  { 0x7,0xf0 }, /* Horizontal sync end */
  { 0x8,0x98 }, /* Sync control */
  { 0x9,0x1 }, /* Luminance control */
  { 0xa,0x80 }, /* Luminance brightness */
  { 0xb,0x60 }, /* Luminance contrast */
  { 0xc,0x40 }, /* Chrominance saturation */
  { 0xe,0x1 }, /* Chrominance control */
  { 0xf,0x2a }, /* Chrominance gain */
  { 0x10,0x40 }, /* Format/delay control */
  { 0x11,0xc }, /* Output control 1 */
  { 0x12,0xb8 }, /* Output control 2 */
  { 0x59,0x54 }, /* Horizontal offset for slicer */
  { 0x5a,0x7 }, /* Vertical offset for slicer */
  { 0x5b,0x83 }, /* field offset and MSB of h/v slicer offsets */
  { 0,0}
};

#endif /* SPCA50X_I2C_INIT_H */
//eof
