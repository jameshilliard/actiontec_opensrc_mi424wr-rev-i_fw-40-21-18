#include <linux/circ_buf.h>

struct ssi_dev;

struct ssi_bus {
	u_char		cfglen;
	u_char		framelen;
	u_char		clkpol;
	u_char		proto;
	struct ssi_dev	*dev;		/* current device */
	int		(*select)(struct ssi_bus *, struct ssi_dev *);
	int		(*trans)(struct ssi_bus *, u_int data);
	int		(*init)(struct ssi_bus *);
	void		(*exit)(struct ssi_bus *);
	char		*name;
	u_int		devices;
};

extern int ssi_core_rcv(struct ssi_bus *bus, u_int data);
extern int ssi_register_bus(struct ssi_bus *bus);
extern int ssi_unregister_bus(struct ssi_bus *bus);
