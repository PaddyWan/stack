/*
 * Protocol Control Information
 *
 *    Francesco Salvestrini <f.salvestrini@nextworks.it>
 *    Miquel Tarzan         <miquel.tarzan@i2cat.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/export.h>
#include <linux/types.h>

#define RINA_PREFIX "pdu"

#include "logs.h"
#include "utils.h"
#include "debug.h"
#include "pdu.h"
#include "qos.h"

/* FIXME: These externs have to disappear from here */
struct buffer * buffer_create_with_gfp(gfp_t  flags,
                                       void * data,
                                       size_t size);
struct buffer * buffer_create_from_gfp(gfp_t        flags,
                                       const void * data,
                                       size_t       size);
struct buffer * buffer_dup_gfp(gfp_t                 flags,
                               const struct buffer * b);

struct pci {
        pdu_type_t  type;

        /* If type == PDU_TYPE_MGMT, all the following fields are useless */
        address_t   destination;
        address_t   source;

        /*
         * FIXME: Group ceps and qos_id together in connection-id struct ?
         *        (See spec)
         */
        struct {
                cep_id_t source;
                cep_id_t destination;
        }           ceps;

        qos_id_t    qos_id;
        pdu_flags_t flags;
        seq_num_t   sequence_number;

        struct {
                seq_num_t ack_nack_seq_num;
                seq_num_t new_rt_wind_edge;
                seq_num_t new_left_wind_edge;
                seq_num_t left_wind_edge;
                seq_num_t rt_wind_edge;
        } control;
};

bool pci_is_ok(const struct pci * pci)
{ return pci && pdu_type_is_ok(pci->type) ? true : false; }
EXPORT_SYMBOL(pci_is_ok);

size_t pci_length_min(void)
{ return sizeof(struct pci); }
EXPORT_SYMBOL(pci_length_min);

ssize_t pci_length(const struct pci * pci)
{
        if (!pci)
                return -1;

        return sizeof(*pci);
}
EXPORT_SYMBOL(pci_length);

static struct pci * pci_create_gfp(gfp_t flags)
{
        struct pci * tmp;

        tmp = rkzalloc(sizeof(*tmp), flags);
        if (!tmp)
                return NULL;

        return tmp;
}

struct pci * pci_create(void)
{ return pci_create_gfp(GFP_KERNEL); }
EXPORT_SYMBOL(pci_create);

struct pci * pci_create_ni(void)
{ return pci_create_gfp(GFP_ATOMIC); }
EXPORT_SYMBOL(pci_create_ni);

int pci_cep_source_set(struct pci * pci,
                       cep_id_t     src_cep_id)
{
        if (!pci)
                return -1;

        if (!is_cep_id_ok(src_cep_id))
                return -1;

        pci->ceps.destination = src_cep_id;
        return 0;
}
EXPORT_SYMBOL(pci_cep_source_set);

int pci_cep_destination_set(struct pci * pci,
                            cep_id_t     dst_cep_id)
{
        if (!pci)
                return -1;

        if (!is_cep_id_ok(dst_cep_id))
                return -1;

        pci->ceps.source = dst_cep_id;

        return 0;
}
EXPORT_SYMBOL(pci_cep_destination_set);

int pci_destination_set(struct pci * pci,
                        address_t    dst_address)
{
        if (!pci)
                return -1;

        pci->destination = dst_address;

        return 0;
}
EXPORT_SYMBOL(pci_destination_set);

int pci_source_set(struct pci * pci,
                   address_t    src_address)
{
        if (!pci)
                return -1;

        pci->source = src_address;

        return 0;
}
EXPORT_SYMBOL(pci_source_set);

int pci_sequence_number_set(struct pci * pci,
                            seq_num_t    sequence_number)
{
        if (!pci)
                return -1;

        pci->sequence_number = sequence_number;

        return 0;
}
EXPORT_SYMBOL(pci_sequence_number_set);

seq_num_t pci_sequence_number_get(struct pci * pci)
{
        if (!pci)
                return -1;

        return pci->sequence_number;
}
EXPORT_SYMBOL(pci_sequence_number_get);

int pci_qos_id_set(struct pci * pci,
                   qos_id_t     qos_id)
{
        if (!pci)
                return -1;

        pci->qos_id = qos_id;

        return 0;
}
EXPORT_SYMBOL(pci_qos_id_set);

int pci_type_set(struct pci * pci, pdu_type_t type)
{
        if (!pci)
                return -1;

        pci->type = type;

        return 0;
}
EXPORT_SYMBOL(pci_type_set);

int pci_format(struct pci * pci,
               cep_id_t     src_cep_id,
               cep_id_t     dst_cep_id,
               address_t    src_address,
               address_t    dst_address,
               seq_num_t    sequence_number,
               qos_id_t     qos_id,
               pdu_type_t   type)
{
        if (pci_cep_destination_set(pci, src_cep_id)      ||
            pci_cep_source_set(pci, dst_cep_id)           ||
            pci_destination_set(pci, dst_address)         ||
            pci_source_set(pci, src_address)              ||
            pci_sequence_number_set(pci, sequence_number) ||
            pci_qos_id_set(pci, qos_id)                   ||
            pci_type_set(pci, type)) {
                return -1;
        }

        return 0;
}
EXPORT_SYMBOL(pci_format);

struct pci * pci_create_from_gfp(gfp_t        flags,
                                 const void * data)
{
        struct pci * tmp;

        if (!data)
                return NULL;

        tmp = rkmalloc(sizeof(*tmp), flags);
        if (!tmp)
                return NULL;

        if (!memcpy(tmp, data, sizeof(*tmp))) {
                rkfree(tmp);
                return NULL;
        }

        if (!pci_is_ok(tmp)) {
                LOG_ERR("Cannot create PCI from bad data");
                rkfree(tmp);
                return NULL;
        }

        return tmp;
}

struct pci * pci_create_from(const void * data)
{ return pci_create_from_gfp(GFP_KERNEL, data); }
EXPORT_SYMBOL(pci_create_from);

struct pci * pci_create_from_ni(const void * data)
{ return pci_create_from_gfp(GFP_ATOMIC, data); }
EXPORT_SYMBOL(pci_create_from_ni);

int pci_destroy(struct pci * pci)
{
        if (!pci)
                return -1;

        rkfree(pci);
        return 0;
}
EXPORT_SYMBOL(pci_destroy);

struct pci * pci_dup_gfp(gfp_t              flags,
                         const struct pci * pci)
{
        struct pci * tmp;

        if (pci_is_ok(pci))
                return NULL;

        tmp = rkmalloc(sizeof(*tmp), flags);
        if (!tmp)
                return NULL;

        if (!memcpy(tmp, pci, sizeof(*tmp))) {
                rkfree(tmp);
                return NULL;
        }

        ASSERT(pci_is_ok(tmp));

        return tmp;
}

struct pci * pci_dup(const struct pci * pci)
{ return pci_dup_gfp(GFP_KERNEL, pci); }
EXPORT_SYMBOL(pci_dup);

struct pci * pci_dup_ni(const struct pci * pci)
{ return pci_dup_gfp(GFP_ATOMIC, pci); }
EXPORT_SYMBOL(pci_dup_ni);

pdu_type_t pci_type(const struct pci * pci)
{
        ASSERT(pci); /* FIXME: Should not be an ASSERT ... */

        return pci->type;
}
EXPORT_SYMBOL(pci_type);

address_t pci_source(const struct pci * pci)
{
        ASSERT(pci); /* FIXME: Should not be an ASSERT ... */

        return pci->source;
}
EXPORT_SYMBOL(pci_source);

address_t pci_destination(const struct pci * pci)
{
        ASSERT(pci); /* FIXME: Should not be an ASSERT ... */

        return pci->destination;
}
EXPORT_SYMBOL(pci_destination);

cep_id_t pci_cep_destination(const struct pci * pci)
{
        if (!pci)
                return cep_id_bad();

        return pci->ceps.destination;
}
EXPORT_SYMBOL(pci_cep_destination);

cep_id_t pci_cep_source(const struct pci * pci)
{
        if (!pci)
                return cep_id_bad();

        return pci->ceps.source;
}
EXPORT_SYMBOL(pci_cep_source);

qos_id_t pci_qos_id(const struct pci * pci)
{ return pci ? pci->qos_id : qos_id_bad();  }
EXPORT_SYMBOL(pci_qos_id);

int pci_control_ack_seq_num_set(struct pci * pci, seq_num_t seq)
{
        if (!pci)
                return -1;

        pci->control.ack_nack_seq_num = seq;

        return 0;
}
EXPORT_SYMBOL(pci_control_ack_seq_num_set);

int pci_control_new_rt_wind_edge_set(struct pci * pci, seq_num_t seq)
{
        if (!pci)
                return -1;

        pci->control.new_rt_wind_edge = seq;

        return 0;
}
EXPORT_SYMBOL(pci_control_new_rt_wind_edge_set);

int pci_control_new_left_wind_edge_set(struct pci * pci, seq_num_t seq)
{
        if (!pci)
                return -1;

        pci->control.new_left_wind_edge = seq;

        return 0;
}
EXPORT_SYMBOL(pci_control_new_left_wind_edge_set);

int pci_control_rt_wind_edge_set(struct pci * pci, seq_num_t seq)
{
        if (!pci)
                return -1;

        pci->control.rt_wind_edge = seq;

        return 0;
}
EXPORT_SYMBOL(pci_control_rt_wind_edge_set);

int pci_control_left_wind_edge_set(struct pci * pci, seq_num_t seq)
{
        if (!pci)
                return -1;

        pci->control.left_wind_edge = seq;

        return 0;
}
EXPORT_SYMBOL(pci_control_left_wind_edge_set);

seq_num_t pci_control_ack_seq_num(const struct pci * pci)
{ return pci ? pci->control.ack_nack_seq_num : 0; }
EXPORT_SYMBOL(pci_control_ack_seq_num);

seq_num_t pci_control_new_rt_wind_edge(const struct pci * pci)
{ return pci ? pci->control.new_rt_wind_edge : 0; }
EXPORT_SYMBOL(pci_control_new_rt_wind_edge);

seq_num_t pci_control_new_left_wind_edge(const struct pci * pci)
{ return pci ? pci->control.new_left_wind_edge : 0; }
EXPORT_SYMBOL(pci_control_new_left_wind_edge);

seq_num_t pci_control_rt_wind_edge(const struct pci * pci)
{ return pci ? pci->control.rt_wind_edge : 0; }
EXPORT_SYMBOL(pci_control_rt_wind_edge);

seq_num_t pci_control_left_wind_edge(const struct pci * pci)
{ return pci ? pci->control.left_wind_edge : 0; }
EXPORT_SYMBOL(pci_control_left_wind_edge);
