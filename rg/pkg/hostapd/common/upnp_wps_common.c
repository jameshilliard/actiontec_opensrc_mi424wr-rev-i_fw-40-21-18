/**************************************************************************
//
//  Copyright (c) 2006-2007 Sony Corporation. All Rights Reserved.
//
//  File Name: upnp_wps_common.c
//  Description: EAP-WPS UPnP common source
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//     * Neither the name of Sony Corporation nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************/

#include "includes.h"
#include "os.h"

#include <upnp/ithread.h>
#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ixml.h>

int
upnp_get_element_value(IXML_Element *element, char **value)
{
	int ret = -1;
	IXML_Node *child;
	do {
		if (!value)
			break;
		*value = 0;

		if (!element)
			break;

		child = ixmlNode_getFirstChild((IXML_Node *)element);
		if (!child)
			break;

		if (eTEXT_NODE != ixmlNode_getNodeType(child))
			break;

		*value = os_strdup(ixmlNode_getNodeValue(child));
		if (!*value)
			break;

		ret = 0;
	} while (0);

	if (ret) {
		if (value && *value) {
			os_free(*value);
			*value = 0;
		}
	}

	return ret;
}

IXML_NodeList *
upnp_get_first_service_list(IXML_Document *doc)
{
	IXML_NodeList *service_list = 0;
	IXML_NodeList *node_list = 0;
	IXML_Node *node = 0;

	do {
		if (!doc)
			break;

		node_list = ixmlDocument_getElementsByTagName(doc, "serviceList");
		if (!node_list)
			break;

		if (!ixmlNodeList_length(node_list))
			break;
		node = ixmlNodeList_item(node_list, 0);

		service_list = ixmlElement_getElementsByTagName((IXML_Element *)node,
													    "service");
		if (!service_list)
			break;
	} while (0);

	if (node_list)
		ixmlNodeList_free(node_list);

	return service_list;
}


int
upnp_get_first_document_item(IXML_Document *doc,
							 const char *item,
							 char **value)
{
	int ret = -1;
	IXML_NodeList *node_list = 0;
	IXML_Node *item_node;
	IXML_Node *txt_node;

	do {
		if (!value)
			break;
		*value = 0;

		if (!doc || !item)
			break;

		node_list = ixmlDocument_getElementsByTagName(doc, (char *)item);
		if (!node_list)
			break;

		item_node = ixmlNodeList_item(node_list, 0);
		if (!item_node)
			break;

		txt_node = ixmlNode_getFirstChild(item_node);
		*value = os_strdup(ixmlNode_getNodeValue(txt_node));
		if (!*value)
			break;

		ret = 0;
	} while (0);

	if (node_list)
		ixmlNodeList_free(node_list);

	if (ret) {
		if (value && *value) {
			os_free(*value);
			*value = 0;
		}
	}

	return ret;
}


int
upnp_get_first_element_item(IXML_Element *element,
							const char *item,
							char **value)
{
	int ret = -1;
	IXML_NodeList *node_list = 0;
	IXML_Node *item_node;
	IXML_Node *txt_node;

	do {
		if (!value)
			break;
		*value = 0;

		if (!element || !item)
			break;

		node_list = ixmlElement_getElementsByTagName(element, (char *)item);
		if (!node_list)
			break;

		item_node = ixmlNodeList_item(node_list, 0);
		if (!item_node)
			break;

		txt_node = ixmlNode_getFirstChild(item_node);
		*value = os_strdup(ixmlNode_getNodeValue(txt_node));
		if (!*value)
			break;

		ret = 0;
	} while (0);

	if (node_list)
		ixmlNodeList_free(node_list);

	if (ret) {
		if (value && *value) {
			os_free(*value);
			*value = 0;
		}
	}

	return ret;
}

int
upnp_find_service(IXML_Document *desc_doc,
				  char *location,
				  char *service_type,
				  char **service_id,
				  char **scpd_url,
				  char **control_url,
				  char **event_url)
{
	char *base_url = 0;
	char *base;
	char *type = 0;
	char *scpd = NULL, *ctrl_url = NULL, *ev_url = NULL;
	IXML_NodeList *service_list = NULL;
	IXML_Element *service = NULL;
	int i, length, found = 0;

	(void)upnp_get_first_document_item(desc_doc, "URLBase", &base_url);
	if(base_url)
		base = base_url;
	else
		base = location;

	service_list = upnp_get_first_service_list(desc_doc);
	length = ixmlNodeList_length(service_list);
	for( i = 0; i < length; i++ ) {
		service = (IXML_Element *)ixmlNodeList_item(service_list, i);
		(void)upnp_get_first_element_item(service, "serviceType", &type);
		if(0 == os_strcmp(type, service_type)) {
			(void)upnp_get_first_element_item(service, "serviceId", service_id);
			(void)upnp_get_first_element_item(service, "SCPDURL", &scpd);
			(void)upnp_get_first_element_item(service, "controlURL", &ctrl_url);
			(void)upnp_get_first_element_item(service, "eventSubURL", &ev_url);
			*scpd_url = os_malloc(os_strlen(base) + os_strlen(scpd) + 1);
			if (*scpd_url) {
				if(UPNP_E_SUCCESS  != UpnpResolveURL(base, scpd, *scpd_url))
					;
			}

			*control_url = os_malloc(os_strlen(base) + os_strlen(ctrl_url) + 1);
			if(*control_url) {
				if(UPNP_E_SUCCESS  != UpnpResolveURL(base, ctrl_url, *control_url))
					;
			}

			*event_url = os_malloc(os_strlen(base) + os_strlen(ev_url) + 1);
			if(*event_url) {
				
				if(UPNP_E_SUCCESS  != UpnpResolveURL(base, ev_url, *event_url))
					;
			}

			if (scpd)
				os_free(scpd);
			if (ctrl_url)
				os_free(ctrl_url);
			if (ev_url)
				os_free(ev_url);
			scpd = ctrl_url = ev_url = 0;

			found = 1;
			break;
		}
	}

	if(type)
		os_free(type);
    if(service_list)
        ixmlNodeList_free(service_list);
    if(base_url)
        os_free(base_url);

    return found;
}

