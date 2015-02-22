/*
 * pnpacpi -- PnP ACPI driver
 *
 * Copyright (c) 2004 Matthieu Castet <castet.matthieu@free.fr>
 * Copyright (c) 2004 Li Shaohua <shaohua.li@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/pci.h>
#include "pnpacpi.h"

#ifdef CONFIG_IA64
#define valid_IRQ(i) (1)
#else
#define valid_IRQ(i) (((i) != 0) && ((i) != 2))
#endif

/*
 * Allocated Resources
 */
static int irq_flags(int triggering, int polarity)
{
	int flag;
	if (triggering == ACPI_LEVEL_SENSITIVE) {
		if(polarity == ACPI_ACTIVE_LOW)
			flag = IORESOURCE_IRQ_LOWLEVEL;
		else
			flag = IORESOURCE_IRQ_HIGHLEVEL;
	}
	else {
		if(polarity == ACPI_ACTIVE_LOW)
			flag = IORESOURCE_IRQ_LOWEDGE;
		else
			flag = IORESOURCE_IRQ_HIGHEDGE;
	}
	return flag;
}

static void decode_irq_flags(int flag, int *triggering, int *polarity)
{
	switch (flag) {
	case IORESOURCE_IRQ_LOWLEVEL:
		*triggering = ACPI_LEVEL_SENSITIVE;
		*polarity = ACPI_ACTIVE_LOW;
		break;
	case IORESOURCE_IRQ_HIGHLEVEL:	
		*triggering = ACPI_LEVEL_SENSITIVE;
		*polarity = ACPI_ACTIVE_HIGH;
		break;
	case IORESOURCE_IRQ_LOWEDGE:
		*triggering = ACPI_EDGE_SENSITIVE;
		*polarity = ACPI_ACTIVE_LOW;
		break;
	case IORESOURCE_IRQ_HIGHEDGE:
		*triggering = ACPI_EDGE_SENSITIVE;
		*polarity = ACPI_ACTIVE_HIGH;
		break;
	}
}

static void
pnpacpi_parse_allocated_irqresource(struct pnp_resource_table * res, u32 gsi,
	int triggering, int polarity)
{
	int i = 0;
	int irq;

	if (!valid_IRQ(gsi))
		return;

	while (!(res->irq_resource[i].flags & IORESOURCE_UNSET) &&
			i < PNP_MAX_IRQ)
		i++;
	if (i >= PNP_MAX_IRQ)
		return;

	res->irq_resource[i].flags = IORESOURCE_IRQ;  // Also clears _UNSET flag
	irq = acpi_register_gsi(gsi, triggering, polarity);
	if (irq < 0) {
		res->irq_resource[i].flags |= IORESOURCE_DISABLED;
		return;
	}

	res->irq_resource[i].start = irq;
	res->irq_resource[i].end = irq;
	pcibios_penalize_isa_irq(irq, 1);
}

static void
pnpacpi_parse_allocated_dmaresource(struct pnp_resource_table * res, u32 dma)
{
	int i = 0;
	while (i < PNP_MAX_DMA &&
			!(res->dma_resource[i].flags & IORESOURCE_UNSET))
		i++;
	if (i < PNP_MAX_DMA) {
		res->dma_resource[i].flags = IORESOURCE_DMA;  // Also clears _UNSET flag
		if (dma == -1) {
			res->dma_resource[i].flags |= IORESOURCE_DISABLED;
			return;
		}
		res->dma_resource[i].start = dma;
		res->dma_resource[i].end = dma;
	}
}

static void
pnpacpi_parse_allocated_ioresource(struct pnp_resource_table * res,
	u32 io, u32 len)
{
	int i = 0;
	while (!(res->port_resource[i].flags & IORESOURCE_UNSET) &&
			i < PNP_MAX_PORT)
		i++;
	if (i < PNP_MAX_PORT) {
		res->port_resource[i].flags = IORESOURCE_IO;  // Also clears _UNSET flag
		if (len <= 0 || (io + len -1) >= 0x10003) {
			res->port_resource[i].flags |= IORESOURCE_DISABLED;
			return;
		}
		res->port_resource[i].start = io;
		res->port_resource[i].end = io + len - 1;
	}
}

static void
pnpacpi_parse_allocated_memresource(struct pnp_resource_table * res,
	u64 mem, u64 len)
{
	int i = 0;
	while (!(res->mem_resource[i].flags & IORESOURCE_UNSET) &&
			(i < PNP_MAX_MEM))
		i++;
	if (i < PNP_MAX_MEM) {
		res->mem_resource[i].flags = IORESOURCE_MEM;  // Also clears _UNSET flag
		if (len <= 0) {
			res->mem_resource[i].flags |= IORESOURCE_DISABLED;
			return;
		}
		res->mem_resource[i].start = mem;
		res->mem_resource[i].end = mem + len - 1;
	}
}


static acpi_status pnpacpi_allocated_resource(struct acpi_resource *res,
	void *data)
{
	struct pnp_resource_table * res_table = (struct pnp_resource_table *)data;
	int i;

	switch (res->type) {
	case ACPI_RESOURCE_TYPE_IRQ:
		/*
		 * Per spec, only one interrupt per descriptor is allowed in
		 * _CRS, but some firmware violates this, so parse them all.
		 */
		for (i = 0; i < res->data.irq.interrupt_count; i++) {
			pnpacpi_parse_allocated_irqresource(res_table,
				res->data.irq.interrupts[i],
				res->data.irq.triggering,
				res->data.irq.polarity);
		}
		break;

	case ACPI_RESOURCE_TYPE_DMA:
		if (res->data.dma.channel_count > 0)
			pnpacpi_parse_allocated_dmaresource(res_table,
					res->data.dma.channels[0]);
		break;

	case ACPI_RESOURCE_TYPE_IO:
		pnpacpi_parse_allocated_ioresource(res_table,
				res->data.io.minimum,
				res->data.io.address_length);
		break;

	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
	case ACPI_RESOURCE_TYPE_END_DEPENDENT:
		break;

	case ACPI_RESOURCE_TYPE_FIXED_IO:
		pnpacpi_parse_allocated_ioresource(res_table,
				res->data.fixed_io.address,
				res->data.fixed_io.address_length);
		break;

	case ACPI_RESOURCE_TYPE_VENDOR:
		break;

	case ACPI_RESOURCE_TYPE_END_TAG:
		break;

	case ACPI_RESOURCE_TYPE_MEMORY24:
		pnpacpi_parse_allocated_memresource(res_table,
				res->data.memory24.minimum,
				res->data.memory24.address_length);
		break;
	case ACPI_RESOURCE_TYPE_MEMORY32:
		pnpacpi_parse_allocated_memresource(res_table,
				res->data.memory32.minimum,
				res->data.memory32.address_length);
		break;
	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
		pnpacpi_parse_allocated_memresource(res_table,
				res->data.fixed_memory32.address,
				res->data.fixed_memory32.address_length);
		break;
	case ACPI_RESOURCE_TYPE_ADDRESS16:
		pnpacpi_parse_allocated_memresource(res_table,
				res->data.address16.minimum,
				res->data.address16.address_length);
		break;
	case ACPI_RESOURCE_TYPE_ADDRESS32:
		pnpacpi_parse_allocated_memresource(res_table,
				res->data.address32.minimum,
				res->data.address32.address_length);
		break;
	case ACPI_RESOURCE_TYPE_ADDRESS64:
		pnpacpi_parse_allocated_memresource(res_table,
		res->data.address64.minimum,
		res->data.address64.address_length);
		break;

	case ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64:
		break;

	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		for (i = 0; i < res->data.extended_irq.interrupt_count; i++) {
			pnpacpi_parse_allocated_irqresource(res_table,
				res->data.extended_irq.interrupts[i],
				res->data.extended_irq.triggering,
				res->data.extended_irq.polarity);
		}
		break;

	case ACPI_RESOURCE_TYPE_GENERIC_REGISTER:
		break;

	default:
		pnp_warn("PnPACPI: unknown resource type %d", res->type);
		return AE_ERROR;
	}
			
	return AE_OK;
}

acpi_status pnpacpi_parse_allocated_resource(acpi_handle handle, struct pnp_resource_table * res)
{
	/* Blank the resource table values */
	pnp_init_resource_table(res);

	return acpi_walk_resources(handle, METHOD_NAME__CRS, pnpacpi_allocated_resource, res);
}

static void pnpacpi_parse_dma_option(struct pnp_option *option, struct acpi_resource_dma *p)
{
	int i;
	struct pnp_dma * dma;

	if (p->channel_count == 0)
		return;
	dma = kcalloc(1, sizeof(struct pnp_dma), GFP_KERNEL);
	if (!dma)
		return;

	for(i = 0; i < p->channel_count; i++)
		dma->map |= 1 << p->channels[i];
	dma->flags = 0;
	if (p->bus_master)
		dma->flags |= IORESOURCE_DMA_MASTER;
	switch (p->type) {
	case ACPI_COMPATIBILITY:
		dma->flags |= IORESOURCE_DMA_COMPATIBLE;
		break;
	case ACPI_TYPE_A:
		dma->flags |= IORESOURCE_DMA_TYPEA;
		break;
	case ACPI_TYPE_B:
		dma->flags |= IORESOURCE_DMA_TYPEB;
		break;
	case ACPI_TYPE_F:
		dma->flags |= IORESOURCE_DMA_TYPEF;
		break;
	default:
		/* Set a default value ? */
		dma->flags |= IORESOURCE_DMA_COMPATIBLE;
		pnp_err("Invalid DMA type");
	}
	switch (p->transfer) {
	case ACPI_TRANSFER_8:
		dma->flags |= IORESOURCE_DMA_8BIT;
		break;
	case ACPI_TRANSFER_8_16:
		dma->flags |= IORESOURCE_DMA_8AND16BIT;
		break;
	case ACPI_TRANSFER_16:
		dma->flags |= IORESOURCE_DMA_16BIT;
		break;
	default:
		/* Set a default value ? */
		dma->flags |= IORESOURCE_DMA_8AND16BIT;
		pnp_err("Invalid DMA transfer type");
	}

	pnp_register_dma_resource(option,dma);
	return;
}

	
static void pnpacpi_parse_irq_option(struct pnp_option *option,
	struct acpi_resource_irq *p)
{
	int i;
	struct pnp_irq * irq;
	
	if (p->interrupt_count == 0)
		return;
	irq = kcalloc(1, sizeof(struct pnp_irq), GFP_KERNEL);
	if (!irq)
		return;

	for(i = 0; i < p->interrupt_count; i++)
		if (p->interrupts[i])
			__set_bit(p->interrupts[i], irq->map);
	irq->flags = irq_flags(p->triggering, p->polarity);

	pnp_register_irq_resource(option, irq);
	return;
}

static void pnpacpi_parse_ext_irq_option(struct pnp_option *option,
	struct acpi_resource_extended_irq *p)
{
	int i;
	struct pnp_irq * irq;

	if (p->interrupt_count == 0)
		return;
	irq = kcalloc(1, sizeof(struct pnp_irq), GFP_KERNEL);
	if (!irq)
		return;

	for(i = 0; i < p->interrupt_count; i++)
		if (p->interrupts[i])
			__set_bit(p->interrupts[i], irq->map);
	irq->flags = irq_flags(p->triggering, p->polarity);

	pnp_register_irq_resource(option, irq);
	return;
}

static void
pnpacpi_parse_port_option(struct pnp_option *option,
	struct acpi_resource_io *io)
{
	struct pnp_port * port;

	if (io->address_length == 0)
		return;
	port = kcalloc(1, sizeof(struct pnp_port), GFP_KERNEL);
	if (!port)
		return;
	port->min = io->minimum;
	port->max = io->maximum;
	port->align = io->alignment;
	port->size = io->address_length;
	port->flags = ACPI_DECODE_16 == io->io_decode ?
		PNP_PORT_FLAG_16BITADDR : 0;
	pnp_register_port_resource(option,port);
	return;
}

static void
pnpacpi_parse_fixed_port_option(struct pnp_option *option,
	struct acpi_resource_fixed_io *io)
{
	struct pnp_port * port;

	if (io->address_length == 0)
		return;
	port = kcalloc(1, sizeof(struct pnp_port), GFP_KERNEL);
	if (!port)
		return;
	port->min = port->max = io->address;
	port->size = io->address_length;
	port->align = 0;
	port->flags = PNP_PORT_FLAG_FIXED;
	pnp_register_port_resource(option,port);
	return;
}

static void
pnpacpi_parse_mem24_option(struct pnp_option *option,
	struct acpi_resource_memory24 *p)
{
	struct pnp_mem * mem;

	if (p->address_length == 0)
		return;
	mem = kcalloc(1, sizeof(struct pnp_mem), GFP_KERNEL);
	if (!mem)
		return;
	mem->min = p->minimum;
	mem->max = p->maximum;
	mem->align = p->alignment;
	mem->size = p->address_length;

	mem->flags = (ACPI_READ_WRITE_MEMORY == p->write_protect) ?
			IORESOURCE_MEM_WRITEABLE : 0;

	pnp_register_mem_resource(option,mem);
	return;
}

static void
pnpacpi_parse_mem32_option(struct pnp_option *option,
	struct acpi_resource_memory32 *p)
{
	struct pnp_mem * mem;

	if (p->address_length == 0)
		return;
	mem = kcalloc(1, sizeof(struct pnp_mem), GFP_KERNEL);
	if (!mem)
		return;
	mem->min = p->minimum;
	mem->max = p->maximum;
	mem->align = p->alignment;
	mem->size = p->address_length;

	mem->flags = (ACPI_READ_WRITE_MEMORY == p->write_protect) ?
			IORESOURCE_MEM_WRITEABLE : 0;

	pnp_register_mem_resource(option,mem);
	return;
}

static void
pnpacpi_parse_fixed_mem32_option(struct pnp_option *option,
	struct acpi_resource_fixed_memory32 *p)
{
	struct pnp_mem * mem;

	if (p->address_length == 0)
		return;
	mem = kcalloc(1, sizeof(struct pnp_mem), GFP_KERNEL);
	if (!mem)
		return;
	mem->min = mem->max = p->address;
	mem->size = p->address_length;
	mem->align = 0;

	mem->flags = (ACPI_READ_WRITE_MEMORY == p->write_protect) ?
			IORESOURCE_MEM_WRITEABLE : 0;

	pnp_register_mem_resource(option,mem);
	return;
}

static void
pnpacpi_parse_address_option(struct pnp_option *option, struct acpi_resource *r)
{
	struct acpi_resource_address64 addr, *p = &addr;
	acpi_status status;
	struct pnp_mem * mem;
	struct pnp_port * port;

	status = acpi_resource_to_address64(r, p);
	if (!ACPI_SUCCESS(status)) {
		pnp_warn("PnPACPI: failed to convert resource type %d", r->type);
		return;
	}

	if (p->address_length == 0)
		return;

	if (p->resource_type == ACPI_MEMORY_RANGE) {
		mem = kcalloc(1, sizeof(struct pnp_mem), GFP_KERNEL);
		if (!mem)
			return;
		mem->min = mem->max = p->minimum;
		mem->size = p->address_length;
		mem->align = 0;
		mem->flags = (p->info.mem.write_protect ==
		    ACPI_READ_WRITE_MEMORY) ? IORESOURCE_MEM_WRITEABLE : 0;
		pnp_register_mem_resource(option,mem);
	} else if (p->resource_type == ACPI_IO_RANGE) {
		port = kcalloc(1, sizeof(struct pnp_port), GFP_KERNEL);
		if (!port)
			return;
		port->min = port->max = p->minimum;
		port->size = p->address_length;
		port->align = 0;
		port->flags = PNP_PORT_FLAG_FIXED;
		pnp_register_port_resource(option,port);
	}
}

struct acpipnp_parse_option_s {
	struct pnp_option *option;
	struct pnp_option *option_independent;
	struct pnp_dev *dev;
};

static acpi_status pnpacpi_option_resource(struct acpi_resource *res,
	void *data)
{
	int priority = 0;
	struct acpipnp_parse_option_s *parse_data = (struct acpipnp_parse_option_s *)data;
	struct pnp_dev *dev = parse_data->dev;
	struct pnp_option *option = parse_data->option;

	switch (res->type) {
		case ACPI_RESOURCE_TYPE_IRQ:
			pnpacpi_parse_irq_option(option, &res->data.irq);
			break;

		case ACPI_RESOURCE_TYPE_DMA:
			pnpacpi_parse_dma_option(option, &res->data.dma);	
			break;

		case ACPI_RESOURCE_TYPE_START_DEPENDENT:
			switch (res->data.start_dpf.compatibility_priority) {
				case ACPI_GOOD_CONFIGURATION:
					priority = PNP_RES_PRIORITY_PREFERRED;
					break;
					
				case ACPI_ACCEPTABLE_CONFIGURATION:
					priority = PNP_RES_PRIORITY_ACCEPTABLE;
					break;

				case ACPI_SUB_OPTIMAL_CONFIGURATION:
					priority = PNP_RES_PRIORITY_FUNCTIONAL;
					break;
				default:
					priority = PNP_RES_PRIORITY_INVALID;
					break;
			}
			/* TBD: Considering performace/robustness bits */
			option = pnp_register_dependent_option(dev, priority);
			if (!option)
				return AE_ERROR;
			parse_data->option = option;	
			break;

		case ACPI_RESOURCE_TYPE_END_DEPENDENT:
			/*only one EndDependentFn is allowed*/
			if (!parse_data->option_independent) {
				pnp_warn("PnPACPI: more than one EndDependentFn");
				return AE_ERROR;
			}
			parse_data->option = parse_data->option_independent;
			parse_data->option_independent = NULL;
			break;

		case ACPI_RESOURCE_TYPE_IO:
			pnpacpi_parse_port_option(option, &res->data.io);
			break;

		case ACPI_RESOURCE_TYPE_FIXED_IO:
			pnpacpi_parse_fixed_port_option(option,
				&res->data.fixed_io);
			break;

		case ACPI_RESOURCE_TYPE_VENDOR:
		case ACPI_RESOURCE_TYPE_END_TAG:
			break;

		case ACPI_RESOURCE_TYPE_MEMORY24:
			pnpacpi_parse_mem24_option(option, &res->data.memory24);
			break;

		case ACPI_RESOURCE_TYPE_MEMORY32:
			pnpacpi_parse_mem32_option(option, &res->data.memory32);
			break;

		case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
			pnpacpi_parse_fixed_mem32_option(option,
				&res->data.fixed_memory32);
			break;

		case ACPI_RESOURCE_TYPE_ADDRESS16:
		case ACPI_RESOURCE_TYPE_ADDRESS32:
		case ACPI_RESOURCE_TYPE_ADDRESS64:
			pnpacpi_parse_address_option(option, res);
			break;

		case ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64:
			break;

		case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
			pnpacpi_parse_ext_irq_option(option,
				&res->data.extended_irq);
			break;

		case ACPI_RESOURCE_TYPE_GENERIC_REGISTER:
			break;

		default:
			pnp_warn("PnPACPI: unknown resource type %d", res->type);
			return AE_ERROR;
	}
			
	return AE_OK;
}

acpi_status pnpacpi_parse_resource_option_data(acpi_handle handle,
	struct pnp_dev *dev)
{
	acpi_status status;
	struct acpipnp_parse_option_s parse_data;

	parse_data.option = pnp_register_independent_option(dev);
	if (!parse_data.option)
		return AE_ERROR;
	parse_data.option_independent = parse_data.option;
	parse_data.dev = dev;
	status = acpi_walk_resources(handle, METHOD_NAME__PRS,
		pnpacpi_option_resource, &parse_data);

	return status;
}

/*
 * Set resource
 */
static acpi_status pnpacpi_count_resources(struct acpi_resource *res,
	void *data)
{
	int *res_cnt = (int *)data;
	switch (res->type) {
	case ACPI_RESOURCE_TYPE_IRQ:
	case ACPI_RESOURCE_TYPE_DMA:
	case ACPI_RESOURCE_TYPE_IO:
	case ACPI_RESOURCE_TYPE_FIXED_IO:
	case ACPI_RESOURCE_TYPE_MEMORY24:
	case ACPI_RESOURCE_TYPE_MEMORY32:
	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
	case ACPI_RESOURCE_TYPE_ADDRESS16:
	case ACPI_RESOURCE_TYPE_ADDRESS32:
	case ACPI_RESOURCE_TYPE_ADDRESS64:
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		(*res_cnt) ++;
	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
	case ACPI_RESOURCE_TYPE_END_DEPENDENT:
	case ACPI_RESOURCE_TYPE_VENDOR:
	case ACPI_RESOURCE_TYPE_END_TAG:
	case ACPI_RESOURCE_TYPE_GENERIC_REGISTER:
	default:
		return AE_OK;
	}
	return AE_OK;
}

static acpi_status pnpacpi_type_resources(struct acpi_resource *res,
	void *data)
{
	struct acpi_resource **resource = (struct acpi_resource **)data;	
	switch (res->type) {
	case ACPI_RESOURCE_TYPE_IRQ:
	case ACPI_RESOURCE_TYPE_DMA:
	case ACPI_RESOURCE_TYPE_IO:
	case ACPI_RESOURCE_TYPE_FIXED_IO:
	case ACPI_RESOURCE_TYPE_MEMORY24:
	case ACPI_RESOURCE_TYPE_MEMORY32:
	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
	case ACPI_RESOURCE_TYPE_ADDRESS16:
	case ACPI_RESOURCE_TYPE_ADDRESS32:
	case ACPI_RESOURCE_TYPE_ADDRESS64:
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		(*resource)->type = res->type;
		(*resource)++;
	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
	case ACPI_RESOURCE_TYPE_END_DEPENDENT:
	case ACPI_RESOURCE_TYPE_VENDOR:
	case ACPI_RESOURCE_TYPE_END_TAG:
	case ACPI_RESOURCE_TYPE_GENERIC_REGISTER:
	default:
		return AE_OK;
	}

	return AE_OK;
}

int pnpacpi_build_resource_template(acpi_handle handle,
	struct acpi_buffer *buffer)
{
	struct acpi_resource *resource;
	int res_cnt = 0;
	acpi_status status;

	status = acpi_walk_resources(handle, METHOD_NAME__CRS,
		pnpacpi_count_resources, &res_cnt);
	if (ACPI_FAILURE(status)) {
		pnp_err("Evaluate _CRS failed");
		return -EINVAL;
	}
	if (!res_cnt)
		return -EINVAL;
	buffer->length = sizeof(struct acpi_resource) * (res_cnt + 1) + 1;
	buffer->pointer = kcalloc(1, buffer->length - 1, GFP_KERNEL);
	if (!buffer->pointer)
		return -ENOMEM;
	pnp_dbg("Res cnt %d", res_cnt);
	resource = (struct acpi_resource *)buffer->pointer;
	status = acpi_walk_resources(handle, METHOD_NAME__CRS,
		pnpacpi_type_resources, &resource);
	if (ACPI_FAILURE(status)) {
		kfree(buffer->pointer);
		pnp_err("Evaluate _CRS failed");
		return -EINVAL;
	}
	/* resource will pointer the end resource now */
	resource->type = ACPI_RESOURCE_TYPE_END_TAG;

	return 0;
}

static void pnpacpi_encode_irq(struct acpi_resource *resource,
	struct resource *p)
{
	int triggering, polarity;
	
	decode_irq_flags(p->flags & IORESOURCE_BITS, &triggering,
		&polarity);
	resource->type = ACPI_RESOURCE_TYPE_IRQ;
	resource->length = sizeof(struct acpi_resource);
	resource->data.irq.triggering = triggering;
	resource->data.irq.polarity = polarity;
	if (triggering == ACPI_EDGE_SENSITIVE)
		resource->data.irq.sharable = ACPI_EXCLUSIVE;
	else
		resource->data.irq.sharable = ACPI_SHARED;
	resource->data.irq.interrupt_count = 1;
	resource->data.irq.interrupts[0] = p->start;
}

static void pnpacpi_encode_ext_irq(struct acpi_resource *resource,
	struct resource *p)
{
	int triggering, polarity;
	
	decode_irq_flags(p->flags & IORESOURCE_BITS, &triggering,
		&polarity);
	resource->type = ACPI_RESOURCE_TYPE_EXTENDED_IRQ;
	resource->length = sizeof(struct acpi_resource);
	resource->data.extended_irq.producer_consumer = ACPI_CONSUMER;
	resource->data.extended_irq.triggering = triggering;
	resource->data.extended_irq.polarity = polarity;
	if (triggering == ACPI_EDGE_SENSITIVE)
		resource->data.irq.sharable = ACPI_EXCLUSIVE;
	else
		resource->data.irq.sharable = ACPI_SHARED;
	resource->data.extended_irq.interrupt_count = 1;
	resource->data.extended_irq.interrupts[0] = p->start;
}

static void pnpacpi_encode_dma(struct acpi_resource *resource,
	struct resource *p)
{
	resource->type = ACPI_RESOURCE_TYPE_DMA;
	resource->length = sizeof(struct acpi_resource);
	/* Note: pnp_assign_dma will copy pnp_dma->flags into p->flags */
	if (p->flags & IORESOURCE_DMA_COMPATIBLE)
		resource->data.dma.type = ACPI_COMPATIBILITY;
	else if (p->flags & IORESOURCE_DMA_TYPEA)
		resource->data.dma.type = ACPI_TYPE_A;
	else if (p->flags & IORESOURCE_DMA_TYPEB)
		resource->data.dma.type = ACPI_TYPE_B;
	else if (p->flags & IORESOURCE_DMA_TYPEF)
		resource->data.dma.type = ACPI_TYPE_F;
	if (p->flags & IORESOURCE_DMA_8BIT)
		resource->data.dma.transfer = ACPI_TRANSFER_8;
	else if (p->flags & IORESOURCE_DMA_8AND16BIT)
		resource->data.dma.transfer = ACPI_TRANSFER_8_16;
	else if (p->flags & IORESOURCE_DMA_16BIT)
		resource->data.dma.transfer = ACPI_TRANSFER_16;
	resource->data.dma.bus_master = p->flags & IORESOURCE_DMA_MASTER;
	resource->data.dma.channel_count = 1;
	resource->data.dma.channels[0] = p->start;
}

static void pnpacpi_encode_io(struct acpi_resource *resource,
	struct resource *p)
{
	resource->type = ACPI_RESOURCE_TYPE_IO;
	resource->length = sizeof(struct acpi_resource);
	/* Note: pnp_assign_port will copy pnp_port->flags into p->flags */
	resource->data.io.io_decode = (p->flags & PNP_PORT_FLAG_16BITADDR)?
		ACPI_DECODE_16 : ACPI_DECODE_10;
	resource->data.io.minimum = p->start;
	resource->data.io.maximum = p->end;
	resource->data.io.alignment = 0; /* Correct? */
	resource->data.io.address_length = p->end - p->start + 1;
}

static void pnpacpi_encode_fixed_io(struct acpi_resource *resource,
	struct resource *p)
{
	resource->type = ACPI_RESOURCE_TYPE_FIXED_IO;
	resource->length = sizeof(struct acpi_resource);
	resource->data.fixed_io.address = p->start;
	resource->data.fixed_io.address_length = p->end - p->start + 1;
}

static void pnpacpi_encode_mem24(struct acpi_resource *resource,
	struct resource *p)
{
	resource->type = ACPI_RESOURCE_TYPE_MEMORY24;
	resource->length = sizeof(struct acpi_resource);
	/* Note: pnp_assign_mem will copy pnp_mem->flags into p->flags */
	resource->data.memory24.write_protect =
		(p->flags & IORESOURCE_MEM_WRITEABLE) ?
		ACPI_READ_WRITE_MEMORY : ACPI_READ_ONLY_MEMORY;
	resource->data.memory24.minimum = p->start;
	resource->data.memory24.maximum = p->end;
	resource->data.memory24.alignment = 0;
	resource->data.memory24.address_length = p->end - p->start + 1;
}

static void pnpacpi_encode_mem32(struct acpi_resource *resource,
	struct resource *p)
{
	resource->type = ACPI_RESOURCE_TYPE_MEMORY32;
	resource->length = sizeof(struct acpi_resource);
	resource->data.memory32.write_protect =
		(p->flags & IORESOURCE_MEM_WRITEABLE) ?
		ACPI_READ_WRITE_MEMORY : ACPI_READ_ONLY_MEMORY;
	resource->data.memory32.minimum = p->start;
	resource->data.memory32.maximum = p->end;
	resource->data.memory32.alignment = 0;
	resource->data.memory32.address_length = p->end - p->start + 1;
}

static void pnpacpi_encode_fixed_mem32(struct acpi_resource *resource,
	struct resource *p)
{
	resource->type = ACPI_RESOURCE_TYPE_FIXED_MEMORY32;
	resource->length = sizeof(struct acpi_resource);
	resource->data.fixed_memory32.write_protect =
		(p->flags & IORESOURCE_MEM_WRITEABLE) ?
		ACPI_READ_WRITE_MEMORY : ACPI_READ_ONLY_MEMORY;
	resource->data.fixed_memory32.address = p->start;
	resource->data.fixed_memory32.address_length = p->end - p->start + 1;
}

int pnpacpi_encode_resources(struct pnp_resource_table *res_table,
	struct acpi_buffer *buffer)
{
	int i = 0;
	/* pnpacpi_build_resource_template allocates extra mem */
	int res_cnt = (buffer->length - 1)/sizeof(struct acpi_resource) - 1;
	struct acpi_resource *resource = (struct acpi_resource*)buffer->pointer;
	int port = 0, irq = 0, dma = 0, mem = 0;

	pnp_dbg("res cnt %d", res_cnt);
	while (i < res_cnt) {
		switch(resource->type) {
		case ACPI_RESOURCE_TYPE_IRQ:
			pnp_dbg("Encode irq");
			pnpacpi_encode_irq(resource,
				&res_table->irq_resource[irq]);
			irq++;
			break;

		case ACPI_RESOURCE_TYPE_DMA:
			pnp_dbg("Encode dma");
			pnpacpi_encode_dma(resource,
				&res_table->dma_resource[dma]);
			dma ++;
			break;
		case ACPI_RESOURCE_TYPE_IO:
			pnp_dbg("Encode io");
			pnpacpi_encode_io(resource,
				&res_table->port_resource[port]);
			port ++;
			break;
		case ACPI_RESOURCE_TYPE_FIXED_IO:
			pnp_dbg("Encode fixed io");
			pnpacpi_encode_fixed_io(resource,
				&res_table->port_resource[port]);
			port ++;
			break;
		case ACPI_RESOURCE_TYPE_MEMORY24:
			pnp_dbg("Encode mem24");
			pnpacpi_encode_mem24(resource,
				&res_table->mem_resource[mem]);
			mem ++;
			break;
		case ACPI_RESOURCE_TYPE_MEMORY32:
			pnp_dbg("Encode mem32");
			pnpacpi_encode_mem32(resource,
				&res_table->mem_resource[mem]);
			mem ++;
			break;
		case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
			pnp_dbg("Encode fixed mem32");
			pnpacpi_encode_fixed_mem32(resource,
				&res_table->mem_resource[mem]);
			mem ++;
			break;
		case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
			pnp_dbg("Encode ext irq");
			pnpacpi_encode_ext_irq(resource,
				&res_table->irq_resource[irq]);
			irq++;
			break;
		case ACPI_RESOURCE_TYPE_START_DEPENDENT:
		case ACPI_RESOURCE_TYPE_END_DEPENDENT:
		case ACPI_RESOURCE_TYPE_VENDOR:
		case ACPI_RESOURCE_TYPE_END_TAG:
		case ACPI_RESOURCE_TYPE_ADDRESS16:
		case ACPI_RESOURCE_TYPE_ADDRESS32:
		case ACPI_RESOURCE_TYPE_ADDRESS64:
		case ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64:
		case ACPI_RESOURCE_TYPE_GENERIC_REGISTER:
		default: /* other type */
			pnp_warn("unknown resource type %d", resource->type);
			return -EINVAL;
		}
		resource ++;
		i ++;
	}
	return 0;
}
