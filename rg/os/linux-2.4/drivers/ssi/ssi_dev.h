struct ssi_bus;

#define SSI_SPI		1
#define SSI_MICROWIRE	2
#define SSI_TISSF	3
#define SSI_USAR	4

struct ssi_dev {
	char		*name;
	u_int		id;
	u_int		clkfreq;
	u_char		cfglen;
	u_char		framelen;
	u_char		clkpol;
	u_char		proto;
	void		(*rcv)(struct ssi_dev *, u_int);
	int		(*init)(struct ssi_dev *);
	struct ssi_bus	*bus;
};


