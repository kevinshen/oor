/*
 * lispd_api_internals.c
 *
 * This file is part of LISPmob implementation. It implements
 * the API to interact with LISPmob internals.
 *
 * Copyright (C) The LISPmob project, 2015. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Please send any bug reports or fixes you make to the email address(es):
 *    LISPmob developers <devel@lispmob.org>
 *
 */

#include "lispd_api_internals.h"

#include "lispd_config_functions.h"
#include "lib/lmlog.h"
#include "liblisp/liblisp.h"
#include "lib/util.h"
#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/parser.h>
#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>


lisp_addr_t * lxml_lcaf_get_lisp_addr (xmlNodePtr xml_lcaf);

xmlNodePtr get_inner_xmlNodePtr(xmlNodePtr parent, char *name){
    xmlNodePtr node = NULL;
    xmlChar * xmlName = xmlCharStrdup(name);

    node = xmlFirstElementChild(parent);

    while (node != NULL){
        if(xmlStrEqual(node->name,xmlName)){
            break;
        }
        node = xmlNextElementSibling(node);
    }

    free(xmlName);
    return node;
}

inline xmlNodePtr lxml_get_next_node(xmlNodePtr node)
{
    char * name = (char *)node->name;
    do {
        node = xmlNextElementSibling(node);
    }while(node != NULL && strcmp((char *)node->name,name) != 0);
    return (node);
}

lisp_addr_t * lxml_get_lisp_addr(xmlNodePtr xml_address)
{
    lisp_addr_t *addr = NULL;
    char * str_afi = NULL;
    uint8_t mask = 0;

    str_afi = (char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_address,"afi"));


    if (strcmp(str_afi,"ipv4")==0){
        addr = lisp_addr_new();
        lisp_addr_ip_from_char((char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_address,"ipv4")),addr);
    }else if (strcmp(str_afi,"ipv6") == 0){
        addr = lisp_addr_new();
        lisp_addr_ip_from_char((char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_address,"ipv6")),addr);
    }else if (strcmp(str_afi,"lcaf") == 0){

    }else{
        LMLOG(LDBG_2,"LMAPI->lxml_get_lisp_addr: Afi not suppoted: %s",str_afi);
        return NULL;
    }

    if (get_inner_xmlNodePtr(xml_address,"mask") != NULL){
        mask = atoi((char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_address,"mask")));
        lisp_addr_ip_to_ippref(addr);
        ip_prefix_set_plen(lisp_addr_get_ippref(addr),mask);
    }

    return addr;
}


char * lxml_get_char_lisp_addr(xmlNodePtr xml_address, char *name, htable_t *lcaf_ht)
{
    char * lisp_address_str = NULL;
    char * addr = NULL;
    char * mask = NULL;
    char * str_afi = NULL;
    xmlNodePtr xml_lcaf = NULL;
    lisp_addr_t *laddr = NULL;

    str_afi = (char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_address,"afi"));

    if (strcmp(str_afi,"ipv4")==0){
        addr = (char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_address,"ipv4"));
    }else if (strcmp(str_afi,"ipv6") == 0){
        addr = (char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_address,"ipv6"));
    }else if (strcmp(str_afi,"lcaf") == 0){
        free(str_afi);
        /* Process lcaf address */
        xml_lcaf = get_inner_xmlNodePtr(xml_address,"lcaf");
        laddr = lxml_lcaf_get_lisp_addr(xml_lcaf);
        if (laddr == NULL){
            LMLOG(LDBG_2,"LMAPI->lxml_get_char_lisp_addr: Error processing lcaf address");
            return (NULL);
        }
        htable_insert(lcaf_ht,strdup(name),laddr);
        return (strdup(name));
    }else{
        LMLOG(LDBG_2,"LMAPI->lxml_get_char_lisp_addr: Afi not suppoted: %s",str_afi);
        free(str_afi);
        return (NULL);
    }
    free(str_afi);

    /* Process a prefix */
    if (get_inner_xmlNodePtr(xml_address,"mask") != NULL){
        mask = (char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_address,"mask"));
        lisp_address_str = xmalloc(strlen(addr)+strlen(mask)+1+1);
        sprintf(lisp_address_str,"%s/%s",addr,mask);
        free(addr);
        free(mask);
    }else{
        lisp_address_str = addr;
    }

    return lisp_address_str;
}

lisp_addr_t * lxml_lcaf_get_lisp_addr (xmlNodePtr xml_lcaf)
{
    lisp_addr_t *lcaf_addr = NULL;
    lisp_addr_t *addr = NULL;
    char * str_addr = NULL;
    char * lcaf_type = NULL;
    char * elp_bits = NULL;
    uint8_t lookup_bit = FALSE;
    uint8_t rloc_probe_bit = FALSE;
    uint8_t strict_bit = FALSE;

    xmlNodePtr xml_elp = NULL;
    xmlNodePtr xml_elp_node = NULL;
    elp_t       *elp    = NULL;
    elp_node_t  *enode  = NULL;

    lcaf_type = (char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_lcaf,"lcaf-type"));

    if (strcmp (lcaf_type,"explicit-locator-path") == 0){
        lcaf_addr = lisp_addr_elp_new();
        elp = (elp_t *)lisp_addr_lcaf_addr(lcaf_addr);
        xml_elp = get_inner_xmlNodePtr(xml_lcaf,"explicit-locator-path");
        xml_elp_node = get_inner_xmlNodePtr(xml_elp,"hop");
        while (xml_elp_node != NULL){
            /* Process address */
            str_addr = (char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_elp_node,"address"));
            addr = lisp_addr_new();
            lisp_addr_ip_from_char(str_addr,addr);
            free(str_addr);
            /* Process bits */
            elp_bits = (char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_elp_node,"lrs-bits"));
            if (strstr(elp_bits,"lookup") != NULL){
                lookup_bit = TRUE;
            }
            if (strstr(elp_bits,"rloc-probe") != NULL){
                rloc_probe_bit = TRUE;
            }
            if (strstr(elp_bits,"strict") != NULL){
                strict_bit = TRUE;
            }
            free (elp_bits);
            enode = elp_node_new_init(addr, lookup_bit, rloc_probe_bit, strict_bit);
            if (enode != NULL){
                elp_add_node(elp, enode);
            }
            lisp_addr_del(addr);
            xml_elp_node = lxml_get_next_node(xml_elp_node);
        }
    }else {
        LMLOG(LDBG_2,"LMAPI->lxml_lcaf_get_lisp_addr: LCAF type not suppoted: %s",lcaf_type);
        return (NULL);
    }
    free(lcaf_type);
    return (lcaf_addr);
}

conf_mapping_t * lxml_get_conf_mapping (xmlNodePtr xml_local_eid, htable_t * lcaf_ht)
{
    conf_mapping_t * conf_mapping = NULL;
    conf_loc_t * conf_loct = NULL;
    conf_loc_iface_t * conf_loct_iface = NULL;
    xmlNodePtr      xml_rlocs       = NULL;
    xmlNodePtr      xml_rloc        = NULL;
    xmlNodePtr      xml_ifce_rloc   = NULL;
    xmlNodePtr      xml_addr_rloc   = NULL;
    char *          eid = NULL;
    char *          rloc = NULL;
    char *          eid_name = NULL;
    char *          rloc_name = NULL;
    int ttl = 0;
    int prty = 255;
    int wght = 0;
    int mprty = 255;
    int mwght = 0;

    eid_name = (char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_local_eid,"id"));
    eid = lxml_get_char_lisp_addr(get_inner_xmlNodePtr(xml_local_eid,"eid-address"),eid_name,lcaf_ht);
    free(eid_name);
    if (eid == NULL){
        LMLOG(LDBG_1,"LMAPI->lmapi_nc_xtr_mapdb_add: Error processing EID");
        return NULL;
    }
    conf_mapping = conf_mapping_new();
    if (conf_mapping == NULL){
        free (eid);
        return NULL;
    }
    conf_mapping->eid_prefix = eid;
    if (get_inner_xmlNodePtr(xml_local_eid,"record-ttl") != NULL){
        ttl = atoi((char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_local_eid,"record-ttl")));
        conf_mapping->ttl = ttl;
    }
    /* Process locators */
    xml_rlocs = get_inner_xmlNodePtr(xml_local_eid,"rlocs");
    xml_rloc = get_inner_xmlNodePtr(xml_rlocs,"rloc");
    while (xml_rloc != NULL){
        if (get_inner_xmlNodePtr(xml_rloc,"priority") != NULL){
            prty = atoi((char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_rloc,"priority")));
        }
        if (get_inner_xmlNodePtr(xml_rloc,"weight") != NULL){
            wght = atoi((char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_rloc,"weight")));
        }
        if (get_inner_xmlNodePtr(xml_rloc,"mpriority") != NULL){
            mprty = atoi((char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_rloc,"multicast-priority")));
        }
        if (get_inner_xmlNodePtr(xml_rloc,"mweight") != NULL){
            mwght = atoi((char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_rloc,"multicast-weight")));
        }
        if (get_inner_xmlNodePtr(xml_rloc,"interface")!=NULL){
            xml_ifce_rloc = get_inner_xmlNodePtr(xml_rloc,"interface");
            conf_loct_iface = conf_loc_iface_new_init((char*)xmlNodeGetContent(xml_ifce_rloc),AF_INET,prty,wght,mprty,mwght);
            if (conf_loct_iface == NULL){
                conf_mapping_destroy(conf_mapping);
                return NULL;
            }
            glist_add(conf_loct_iface,conf_mapping->conf_loc_iface_list);
            conf_loct_iface = conf_loc_iface_new_init((char*)xmlNodeGetContent(xml_ifce_rloc),AF_INET6,prty,wght,mprty,mwght);
            if (conf_loct_iface == NULL){
                conf_mapping_destroy(conf_mapping);
                return NULL;
            }
            glist_add(conf_loct_iface,conf_mapping->conf_loc_iface_list);
        }else if (get_inner_xmlNodePtr(xml_rloc,"locator-address")!=NULL){
            rloc_name = (char*)xmlNodeGetContent(get_inner_xmlNodePtr(xml_rloc,"name"));
            xml_addr_rloc = get_inner_xmlNodePtr(xml_rloc,"locator-address");
            rloc = lxml_get_char_lisp_addr(xml_addr_rloc, rloc_name, lcaf_ht);
            free(rloc_name);
            if (rloc == NULL){
                conf_mapping_destroy(conf_mapping);
                return NULL;
            }
            conf_loct = conf_loc_new_init(rloc,prty,wght,mprty,mwght);
            free(rloc);
            if (conf_loct == NULL){
                conf_mapping_destroy(conf_mapping);
                return NULL;
            }
            glist_add(conf_loct,conf_mapping->conf_loc_list);
        }else{
            conf_mapping_destroy(conf_mapping);
            LMLOG(LDBG_1,"LMAPI->lmapi_nc_xtr_mapdb_add: Error processing locator");
            return NULL;
        }
        xml_rloc = lxml_get_next_node(xml_rloc);
    }

    return (conf_mapping);
}

int lxml_update_map_server_list(xmlNodePtr xml_map_servers, uint8_t proxy_reply, glist_t *map_servers_list)
{
    xmlNodePtr xml_map_sever = NULL;
    xmlNodePtr xml_address = NULL;
    xmlNodePtr xml_key_type = NULL;
    xmlNodePtr xml_key = NULL;
    map_server_elt * ms = NULL;
    char * str_addr = NULL;
    char * key_type_aux = NULL;
    int key_type = 0;
    char * key = NULL;
    lisp_addr_t * ms_addr = NULL;
    glist_entry_t *     ms_it          = NULL;

    xml_map_sever = get_inner_xmlNodePtr(xml_map_servers,"map-server");
    while (xml_map_sever != NULL){
        /* Check parameters */
        xml_address = get_inner_xmlNodePtr(xml_map_sever,"address");
        if ((xml_address) == NULL){
            LMLOG (LWRN,"lxml_update_map_server_list: No map server address configured");
            return (BAD);
        }
        if ((xml_key_type = get_inner_xmlNodePtr(xml_map_sever,"auth-key-type")) == NULL){
            LMLOG (LWRN,"lxml_update_map_server_list: No authentication key type specified");
            return (BAD);
        }
        if ((xml_key = get_inner_xmlNodePtr(xml_map_sever,"auth-key")) == NULL){
            LMLOG (LWRN,"lxml_update_map_server_list: No authentication key specified");
            return (BAD);
        }
        str_addr = (char*)xmlNodeGetContent(xml_address);
        key = (char*)xmlNodeGetContent(xml_key);
        key_type_aux = (char*)xmlNodeGetContent(xml_key_type);
        if (strcmp(key_type_aux,"none")==0){
            key_type = NO_KEY;
        }else if (strcmp(key_type_aux,"hmac-sha-1-96")==0){
            key_type = HMAC_SHA_1_96;
        }else if (strcmp(key_type_aux,"hmac-sha-256-128")==0){
            key_type = HMAC_SHA_256_128;
        }
        free(key_type_aux);
        if (key_type != HMAC_SHA_1_96){
            LMLOG(LERR, "Configuraton file: Only SHA-1 (1) authentication is supported");
            free(str_addr);
            free(key);
            return (BAD);
        }

        ms_addr = lisp_addr_new();
        if (ms_addr == NULL){
            LMLOG(LWRN,"lxml_update_map_server_list: Couldn't allocate memory for a lisp_addr_t structure");
            free(str_addr);
            free(key);
            return (BAD);
        }
        if (lisp_addr_ip_from_char(str_addr,ms_addr) != GOOD){
            LMLOG(LWRN,"lxml_update_map_server_list: Error processing address: %s",str_addr);
            free(str_addr);
            free(key);
            return (BAD);
        }
        free(str_addr);

        /* Check if the Map Server is already in the list */
        glist_for_each_entry(ms_it, map_servers_list) {
            ms = (map_server_elt *)glist_entry_data(ms_it);
            if (lisp_addr_cmp(ms->address,ms_addr) == 0){
                lisp_addr_del(ms_addr);
                free(key);
                LMLOG(LDBG_2,"lxml_update_map_server_list: Map server %s already exist. Skipping it ...",
                        lisp_addr_to_char(ms_addr));
                xml_map_sever = lxml_get_next_node(xml_map_sever);
                continue;
            }
        }

        /* Check default afi */
        if (default_rloc_afi != AF_UNSPEC && default_rloc_afi != lisp_addr_ip_afi(ms_addr)){
            LMLOG(LWRN, "The map server %s will not be added due to the selected "
                    "default rloc afi (-a option)", str_addr);
            lisp_addr_del(ms_addr);
            free(key);
            xml_map_sever = lxml_get_next_node(xml_map_sever);
            continue;
        }
        /* Create map server structure and add to the list */
        ms = map_server_elt_new_init(ms_addr,key_type,key,proxy_reply);
        free(key);
        lisp_addr_del(ms_addr);
        if (ms == NULL){
            return (BAD);
        }
        glist_add(ms,map_servers_list);
        xml_map_sever = lxml_get_next_node(xml_map_sever);
    }

    return(GOOD);
}


int lmapi_init_server(lmapi_connection_t *conn) {

	int error = 0;

    conn->context = zmq_ctx_new();
    LMLOG(LDBG_3,"LMAPI: zmq_ctx_new errno: %s\n",zmq_strerror (errno));

    //Request-Reply communication pattern (Server side)
    conn->socket = zmq_socket(conn->context, ZMQ_REP);
    LMLOG(LDBG_3,"LMAPI: zmq_socket: %s\n",zmq_strerror (errno));

    //Attachment point for other processes
    error = zmq_bind(conn->socket, IPC_FILE);

    if (error != 0){
        LMLOG(LDBG_2,"LMAPI: Error while ZMQ binding on server: %s\n",zmq_strerror (error));
    	goto err;
    }

    LMLOG(LDBG_2,"LMAPI: API server initiated using ZMQ\n");

    return (GOOD);

err:
    LMLOG(LERR,"LMAPI: The API server couldn't be initialized.\n");
    return (BAD);

}

int lmapi_xtr_mr_create(lmapi_connection_t *conn, lmapi_msg_hdr_t *hdr, uint8_t *data){

    lisp_xtr_t *    xtr              = NULL;
    uint8_t *       result_msg       = NULL;
    int             result_msg_len   = 0;
    glist_t *       list             = NULL;
    xmlDocPtr       doc              = NULL;
    xmlNodePtr      root_element     = NULL;
    xmlNodePtr      mr_list_xml      = NULL;
    xmlNodePtr      mr_addr_xml      = NULL;
    lisp_addr_t *   mr_addr          = NULL;
    int             prv_mr_list_size = 0;

    LMLOG(LDBG_1, "LMAPI: Creating new list of Map Resolvers");

    xtr = CONTAINER_OF(ctrl_dev, lisp_xtr_t, super);
    list = glist_new_managed((glist_del_fct)lisp_addr_del);

    doc =  xmlReadMemory ((const char *)data, hdr->datalen, NULL, "UTF-8", XML_PARSE_NOBLANKS|XML_PARSE_NSCLEAN|XML_PARSE_NOERROR|XML_PARSE_NOWARNING);
    root_element = xmlDocGetRootElement(doc);

    mr_list_xml = get_inner_xmlNodePtr(root_element,"map-resolvers");
    mr_list_xml = get_inner_xmlNodePtr(mr_list_xml,"map-resolver");

    prv_mr_list_size = glist_size(xtr->map_resolvers);

    while (mr_list_xml != NULL){

        mr_addr_xml = get_inner_xmlNodePtr(mr_list_xml,"map-resolver-address");
        while (mr_addr_xml != NULL){;
            mr_addr = lisp_addr_new();
            if (lisp_addr_ip_from_char((char*)xmlNodeGetContent(mr_addr_xml),mr_addr) != GOOD){
                LMLOG(LDBG_1,"lmapi_xtr_mr_create: Could not parse Map Resolver: %s", (char*)xmlNodeGetContent(mr_addr_xml));
                goto err;
            }

            if (default_rloc_afi != AF_UNSPEC && default_rloc_afi != lisp_addr_ip_afi(mr_addr)){
                LMLOG(LWRN, "lmapi_xtr_mr_create: The Map Resolver %s will not be added due to the selected "
                        "default rloc afi (-a option)", lisp_addr_to_char(mr_addr));
                goto err;
            }

            if (glist_contain_using_cmp_fct(mr_addr, list, (glist_cmp_fct)lisp_addr_cmp)){
                LMLOG(LWRN, "lmapi_xtr_mr_create: The Map Resolver %s is duplicated. Descarding all the list.",
                        lisp_addr_to_char(mr_addr));
                goto err;
            }
            glist_add_tail(lisp_addr_clone(mr_addr), list);

            mr_addr_xml = lxml_get_next_node(mr_addr_xml);
        }
        mr_list_xml = lxml_get_next_node(mr_list_xml);
    }

    xmlFreeDoc(doc);
    doc = NULL;

    //Everything fine. We replace the old list with the new one
    glist_destroy(xtr->map_resolvers);
    xtr->map_resolvers = list;

    LMLOG(LDBG_1, "LMAPI: List of Map Resolvers successfully created");
    LMLOG(LDBG_2, "************* %13s ***************", "Map Resolvers");
            glist_dump(xtr->map_resolvers, (glist_to_char_fct)lisp_addr_to_char, LDBG_1);

    result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_OK);
    lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);

    /* If there were no map resolvers, ask to map resolvers for non active entries */
    if (prv_mr_list_size == 0){
        send_map_request_for_not_active_mce(xtr);
    }

    return (GOOD);
err:
    LMLOG(LERR, "LMAPI: Error while creating Map Resolver list");

    glist_destroy(list);
    if (doc != NULL){
        xmlFreeDoc(doc);
    }

    result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_ERR);
    lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);

    return (BAD);

}

int lmapi_xtr_mr_delete(lmapi_connection_t *conn, lmapi_msg_hdr_t *hdr, uint8_t *data){

	lisp_xtr_t *xtr = NULL;
	uint8_t *result_msg;
	int result_msg_len;

	LMLOG(LDBG_2, "LMAPI: Deleting Map Resolver list");


	xtr = CONTAINER_OF(ctrl_dev, lisp_xtr_t, super);

	if (glist_size(xtr->map_resolvers) == 0){
		result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_ERR);
		lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);
		LMLOG(LWRN, "LMAPI: Trying to remove Map Resolver list, but list was already empty");
		return BAD;
	}

	glist_remove_all(xtr->map_resolvers);

	result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_OK);
	lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);

	LMLOG(LDBG_1, "LMAPI: Map Resolver list deleted");

	return GOOD;

}

int lmapi_xtr_ms_create(lmapi_connection_t *conn, lmapi_msg_hdr_t *hdr, uint8_t *data){
    lisp_xtr_t *        xtr                 = NULL;

    xmlDocPtr       doc             = NULL;
    xmlNodePtr      root_element    = NULL;
    xmlNodePtr      xml_map_servers = NULL;
    xmlNodePtr      xml_ms_proxy_reply = NULL;

    int                 result_msg_len      = 0;
    uint8_t *           result_msg          = NULL;

    glist_t * map_servers_list = glist_new_managed((glist_del_fct)map_server_elt_del);

    char * str_proxy_reply = NULL;
    uint8_t proxy_reply = FALSE;

    LMLOG(LDBG_1, "LMAPI: Creating new map servers list");

    xtr = CONTAINER_OF(ctrl_dev, lisp_xtr_t, super);

    doc =  xmlReadMemory ((const char *)data, hdr->datalen, NULL, "UTF-8", XML_PARSE_NOBLANKS|XML_PARSE_NSCLEAN|XML_PARSE_NOERROR|XML_PARSE_NOWARNING);
    root_element = xmlDocGetRootElement(doc);

    xml_map_servers = xmlFirstElementChild(root_element);
    xml_ms_proxy_reply = get_inner_xmlNodePtr(xml_map_servers,"proxy-reply");
    if (xml_ms_proxy_reply != NULL){
        str_proxy_reply = (char *)xmlNodeGetContent(xml_ms_proxy_reply);
        if (strcmp(str_proxy_reply,"true") == 0){
            proxy_reply = TRUE;
        }
    }

    if (lxml_update_map_server_list(xml_map_servers,proxy_reply, map_servers_list)!=GOOD){
        LMLOG(LDBG_1,"lmapi_xtr_ms_create: Error adding map servers");
        goto err;
    }
    xmlFreeDoc(doc);
    doc = NULL;

    /* Reprogram Map Register for local EIDs */
    program_map_register(xtr, 1);

    //Everything fine. We replace the old list with the new one
    glist_destroy(xtr->map_servers);
    xtr->map_servers = map_servers_list;

    LMLOG(LDBG_1, "LMAPI: List of Map Servers successfully created");
    map_servers_dump(xtr, LDBG_1);

    result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_OK);
    lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);

    return GOOD;

    err:
     glist_destroy(map_servers_list);

     if (doc != NULL){
         xmlFreeDoc(doc);
     }

     result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_ERR);
     lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);
     LMLOG(LWRN, "LMAPI: Error while setting new Map Servers list");

     return BAD;
}


int lmapi_xtr_ms_delete(lmapi_connection_t *conn, lmapi_msg_hdr_t *hdr, uint8_t *data){

    lisp_xtr_t *xtr = NULL;
    uint8_t *result_msg;
    int result_msg_len;

    LMLOG(LDBG_2, "LMAPI: Deleting Map Servers list");

    xtr = CONTAINER_OF(ctrl_dev, lisp_xtr_t, super);

    if (glist_size(xtr->map_servers) == 0){
        //ERROR: Already NULL
        result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_ERR);
        lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);
        LMLOG(LWRN, "LMAPI: Trying to remove Map Resolver list, but list was already empty");
        return BAD;
    }

    glist_remove_all(xtr->map_servers);

    result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_OK);
    lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);

    LMLOG(LDBG_1, "LMAPI: Map Servers list deleted");

    return GOOD;

}


int lmapi_xtr_mapdb_create(lmapi_connection_t *conn, lmapi_msg_hdr_t *hdr, uint8_t *data){
    lisp_xtr_t *        xtr                 = NULL;
    mapping_t *         processed_mapping   = NULL;
    map_local_entry_t * map_loc_e           = NULL;
    void *              fwd_info            = NULL;
    htable_t *          lcaf_ht             = NULL;
    void *              it                  = NULL;
    xmlDocPtr           doc                 = NULL;
    xmlNodePtr          root_element        = NULL;
    xmlNodePtr          xml_local_eids      = NULL;
    xmlNodePtr          xml_local_eid       = NULL;
    conf_mapping_t *    conf_mapping        = NULL;
    glist_t *           conf_mapping_list   = glist_new_managed((glist_del_fct)conf_mapping_destroy);
    glist_entry_t *     conf_map_it         = NULL;
    int                 result_msg_len      = 0;
    uint8_t *           result_msg          = NULL;
    int                 ipv4_mapings        = 0;
    int                 ipv6_mapings        = 0;
    int                 eid_ip_afi          = AF_UNSPEC;

    LMLOG(LDBG_1, "LMAPI: Creating new local data base");
    lcaf_ht = htable_new(g_str_hash, g_str_equal, free,(h_val_del_fct)lisp_addr_del);

    xtr = CONTAINER_OF(ctrl_dev, lisp_xtr_t, super);

    doc =  xmlReadMemory ((const char *)data, hdr->datalen, NULL, "UTF-8", XML_PARSE_NOBLANKS|XML_PARSE_NSCLEAN|XML_PARSE_NOERROR|XML_PARSE_NOWARNING);
    root_element = xmlDocGetRootElement(doc);

    xml_local_eids = xmlFirstElementChild(root_element);
    xml_local_eid = xmlFirstElementChild(xml_local_eids);
    while (xml_local_eid != NULL){
        conf_mapping = lxml_get_conf_mapping (xml_local_eid, lcaf_ht);
        if (conf_mapping == NULL){
            goto err;
        }

        glist_add(conf_mapping,conf_mapping_list);
        xml_local_eid = lxml_get_next_node(xml_local_eid);
    }
    xmlFreeDoc(doc);
    doc = NULL;

    /*
     * Empty previous local database
     */
    /* Remove routing configuration for the eids */
    local_map_db_foreach_entry(xtr->local_mdb, it) {
        map_loc_e = (map_local_entry_t *)it;
        ctrl_unregister_eid_prefix(ctrl_dev, map_local_entry_eid(map_loc_e));
    } local_map_db_foreach_end;

    /* Empty local database */
    local_map_db_del(xtr->local_mdb);
    xtr->local_mdb = local_map_db_new();

    /* We leverage on the LISPmob configuration subsystem to introduce
     * and process the configuration mappings into the system */
    glist_for_each_entry(conf_map_it, conf_mapping_list){
        conf_mapping = (conf_mapping_t *) glist_entry_data(conf_map_it);

        //XXX Beware the NULL in lcaf_ht. No LCAF support yet
        processed_mapping = process_mapping_config(&(xtr->super),lcaf_ht,LOCAL_LOCATOR,conf_mapping);

        if (processed_mapping == NULL){
            LMLOG(LDBG_3, "LMAPI: Couldn't process mapping %s",conf_mapping->eid_prefix);
            goto err;
        }
        /* If dev is a mobile node, we can only have one IPv4 and one IPv6 mapping */
        if (lisp_ctrl_dev_mode(ctrl_dev) == MN_MODE){
            eid_ip_afi = ip_addr_afi(lisp_addr_ip_get_addr(mapping_eid(processed_mapping)));
            if (eid_ip_afi == AF_INET){
                ipv4_mapings ++;
            }else if (eid_ip_afi == AF_INET6){
                ipv6_mapings ++;
            }
            if (ipv4_mapings >1 || ipv6_mapings >1){
                LMLOG(LWRN, "LMAPI: LISP Mobile Node only supports one IPv4 and one IPv6 EID prefix");
                break;
            }
        }

        mapping_set_auth(processed_mapping, 1);

        map_loc_e = map_local_entry_new_init(processed_mapping);
        if (map_loc_e == NULL){
            LMLOG(LDBG_3, "LMAPI: Couldn't allocate map_local_entry_t %s",conf_mapping->eid_prefix);
            goto err;
        }
        fwd_info = xtr->fwd_policy->new_map_loc_policy_inf(xtr->fwd_policy_dev_parm, processed_mapping, NULL);
        map_local_entry_set_fwd_info(map_loc_e,fwd_info,xtr->fwd_policy->del_map_loc_policy_inf);

        if (add_local_db_map_local_entry(map_loc_e,xtr) != GOOD){
            LMLOG(LDBG_3, "LMAPI: Couldn't add mapping %s to local database",
                    lisp_addr_to_char(&(processed_mapping->eid_prefix)));
            goto err;
        }

        LMLOG(LDBG_1, "LMAPI: Updating data-plane for EID prefix %s",
                lisp_addr_to_char(&(processed_mapping->eid_prefix)));

        /* Update the routing rules for the new EID */
        if (ctrl_register_eid_prefix(ctrl_dev,mapping_eid(processed_mapping))!=GOOD){
            LMLOG(LERR, "LMAPI: Unable to update data-plane for mapping %s",
                    lisp_addr_to_char(&(processed_mapping->eid_prefix)));
            goto err;
        }
    }

    /* Update control with new added interfaces */
    ctrl_update_iface_info(ctrl_dev->ctrl);

    glist_destroy(conf_mapping_list);
    htable_destroy(lcaf_ht);

    LMLOG(LDBG_1, "LMAPI: New local data base created");
    LMLOG(LDBG_2, "************* %20s ***************", "Local EID Database");
    local_map_db_dump(xtr->local_mdb, LDBG_1);


    /* Reprogram Map Register for local EIDs */
    program_map_register(xtr, 1);


    result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_OK);
    lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);

    return GOOD;

    err:
    //XXX if error, destroy mappings added to local mapdb? deattach locators from ifaces?
    glist_destroy(conf_mapping_list);
    htable_destroy(lcaf_ht);

    if (doc != NULL){
        xmlFreeDoc(doc);
    }

    result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_ERR);
    lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);
    LMLOG(LWRN, "LMAPI: Error while setting new Mapping Database content");

    return (BAD);
}


int lmapi_xtr_mapdb_delete(lmapi_connection_t *conn, lmapi_msg_hdr_t *hdr, uint8_t *data){

    lisp_xtr_t *        xtr         = NULL;
    map_local_entry_t * map_loc_e   = NULL;
    void *              it          = NULL;
    uint8_t *           result_msg  = NULL;
    int                 result_msg_len;


    LMLOG(LDBG_2, "LMAPI: Deleting local Mapping Database list");

    xtr = CONTAINER_OF(ctrl_dev, lisp_xtr_t, super);

    /* Remove routing configuration for the eids */
    local_map_db_foreach_entry(xtr->local_mdb, it) {
        map_loc_e = (map_local_entry_t *)it;
        ctrl_unregister_eid_prefix(ctrl_dev, map_local_entry_eid(map_loc_e));
    } local_map_db_foreach_end;

    /* Empty local database */
    local_map_db_del(xtr->local_mdb);
    xtr->local_mdb = local_map_db_new();

    /* Send confirmation message to the API server */
    result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_OK);
    lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);

    LMLOG(LDBG_1, "LMAPI: Local Mapping Database deleted");

    return GOOD;

}

int lmapi_xtr_petrs_create(lmapi_connection_t *conn, lmapi_msg_hdr_t *hdr, uint8_t *data){

    lisp_xtr_t *    xtr              = NULL;
    uint8_t *       result_msg       = NULL;
    int             result_msg_len   = 0;
    glist_t *       str_addr_list    = NULL;
    char *          str_addr         = NULL;
    xmlDocPtr       doc              = NULL;
    xmlNodePtr      root_element     = NULL;
    xmlNodePtr      petr_list_xml    = NULL;
    xmlNodePtr      petr_addr_xml    = NULL;
    lisp_addr_t *   petr_addr        = NULL;
    glist_entry_t * addr_it          = NULL;

    LMLOG(LDBG_1, "LMAPI: Creating new list of Proxy ETRs");

    xtr = CONTAINER_OF(ctrl_dev, lisp_xtr_t, super);
    str_addr_list = glist_new_managed(free);

    doc =  xmlReadMemory ((const char *)data, hdr->datalen, NULL, "UTF-8", XML_PARSE_NOBLANKS|XML_PARSE_NSCLEAN|XML_PARSE_NOERROR|XML_PARSE_NOWARNING);
    root_element = xmlDocGetRootElement(doc);

    petr_list_xml = get_inner_xmlNodePtr(root_element,"proxy-etrs");
    petr_list_xml = get_inner_xmlNodePtr(petr_list_xml,"proxy-etr");

    while (petr_list_xml != NULL){

        petr_addr_xml = get_inner_xmlNodePtr(petr_list_xml,"proxy-etr-address");
        while (petr_addr_xml != NULL){
            str_addr = (char*)xmlNodeGetContent(petr_addr_xml);
            /* We do some checks before adding the address to the aux list */
            petr_addr = lisp_addr_new();
            if (lisp_addr_ip_from_char(str_addr,petr_addr) != GOOD){
                LMLOG(LDBG_1,"lmapi_xtr_mr_create: Could not parse Proxy ETR address: %s", str_addr);
                goto err;
            }
            if (default_rloc_afi != AF_UNSPEC && default_rloc_afi != lisp_addr_ip_afi(petr_addr)){
                LMLOG(LWRN, "lmapi_xtr_mr_create: The Proxy ETR %s will not be added due to the selected "
                        "default rloc afi (-a option)", str_addr);
                goto err;
            }

            if (glist_contain_using_cmp_fct(str_addr, str_addr_list, (glist_cmp_fct)strcmp)){
                LMLOG(LWRN, "lmapi_xtr_petr_create: The Proxy ETR %s is duplicated. Descarding all the list.",
                        str_addr);
                goto err;
            }
            glist_add_tail(str_addr, str_addr_list);
            lisp_addr_del(petr_addr);
            petr_addr_xml = lxml_get_next_node(petr_addr_xml);
        }
        petr_list_xml = lxml_get_next_node(petr_list_xml);
    }

    xmlFreeDoc(doc);
    doc = NULL;

    //Everything fine. We replace the old list with the new one
    glist_remove_all(mapping_locators_lists(mcache_entry_mapping(xtr->petrs)));
    glist_for_each_entry(addr_it,str_addr_list){
        str_addr = (char *)glist_entry_data(addr_it);
        add_proxy_etr_entry(xtr->petrs,str_addr,1,100);
    }

    xtr->fwd_policy->updated_map_cache_inf(
            xtr->fwd_policy_dev_parm,
            mcache_entry_routing_info(xtr->petrs),
            mcache_entry_mapping(xtr->petrs));

    LMLOG(LDBG_1, "LMAPI: List of Proxy ETRs successfully created");
    LMLOG(LDBG_1, "************************* Proxy ETRs List ****************************");
    mapping_to_char(mcache_entry_mapping(xtr->petrs));

    result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_OK);
    lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);

    return (GOOD);

err:
    lisp_addr_del(petr_addr);
    free(str_addr);
    xmlFreeDoc(doc);
    LMLOG(LERR, "LMAPI: Error while creating Map Resolver list");
    glist_destroy(str_addr_list);
    result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_ERR);
    lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);

    return (BAD);

}

int lmapi_xtr_petrs_delete(lmapi_connection_t *conn, lmapi_msg_hdr_t *hdr, uint8_t *data){

    lisp_xtr_t *xtr = NULL;
    uint8_t *result_msg;
    int result_msg_len;

    LMLOG(LDBG_2, "LMAPI: Deleting Proxy ETRs list");


    xtr = CONTAINER_OF(ctrl_dev, lisp_xtr_t, super);

    glist_remove_all(mapping_locators_lists(mcache_entry_mapping(xtr->petrs)));

    result_msg_len = lmapi_result_msg_new(&result_msg,hdr->device,hdr->target,hdr->operation,LMAPI_RES_OK);
    lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);

    return GOOD;

}


int (*lmapi_get_proc_func(lmapi_msg_hdr_t* hdr))(lmapi_connection_t *,lmapi_msg_hdr_t *, uint8_t *){

    int (*process_func)(lmapi_connection_t *, lmapi_msg_hdr_t *, uint8_t *) = NULL;

    lisp_dev_type_e device = hdr->device;
    lmapi_msg_target_e target = hdr->target;
    lmapi_msg_opr_e operation = hdr->operation;


    switch (device){
    case LMAPI_DEV_XTR:
        if (lisp_ctrl_dev_mode(ctrl_dev) != xTR_MODE && lisp_ctrl_dev_mode(ctrl_dev) != MN_MODE){
            LMLOG(LDBG_1, "LMAPI call = Call API from wrong device");
            break;
        }
        switch (target){
        case LMAPI_TRGT_MRLIST:
            switch (operation){
                case LMAPI_OPR_CREATE:
                    LMLOG(LDBG_2, "LMAPI call = (Device: xTR | Target: MR list | Operation: Create)");
                    process_func = lmapi_xtr_mr_create;
                    break;
                case LMAPI_OPR_DELETE:
                    LMLOG(LDBG_2, "LMAPI call = (Device: xTR | Target: MR list | Operation: Delete)");
                	process_func = lmapi_xtr_mr_delete;
                	break;
                default:
                	LMLOG(LWRN, "LMAPI call = (Device: xTR | Target: MR list | Operation: Unsupported)");
                    break;
                }
            break;
        case LMAPI_TRGT_MSLIST:
            switch (operation){
                case LMAPI_OPR_CREATE:
                    LMLOG(LDBG_2, "LMAPI call = (Device: xTR | Target: MS list | Operation: Create)");
                    process_func = lmapi_xtr_ms_create;
                    break;
                case LMAPI_OPR_DELETE:
                    LMLOG(LDBG_2, "LMAPI call = (Device: xTR | Target: MS list | Operation: Delete)");
                    process_func = lmapi_xtr_ms_delete;
                    break;
                default:
                    LMLOG(LWRN, "LMAPI call = (Device: xTR | Target: MS list | Operation: Unsupported)");
                    break;
            }
            break;
        case LMAPI_TRGT_MAPDB:
            switch (operation){
                case LMAPI_OPR_CREATE:
                    LMLOG(LDBG_2, "LMAPI call = (Device: xTR | Target: Mapping DB | Operation: Create)");
                    process_func = lmapi_xtr_mapdb_create;
                    break;
                case LMAPI_OPR_DELETE:
                    LMLOG(LDBG_2, "LMAPI call = (Device: xTR | Target: Mapping DB | Operation: Delete)");
                    process_func = lmapi_xtr_mapdb_delete;
                    break;
                default:
                    LMLOG(LWRN, "LMAPI call = (Device: xTR | Target: Mapping DB | Operation: Unsupported)");
                    break;
            }
            break;
         case LMAPI_TRGT_PETRLIST:
            switch (operation){
            case LMAPI_OPR_CREATE:
                LMLOG(LDBG_2, "LMAPI call = (Device: xTR | Target: Proxy ETRs | Operation: Create)");
                process_func = lmapi_xtr_petrs_create;
                break;
            case LMAPI_OPR_DELETE:
                LMLOG(LDBG_2, "LMAPI call = (Device: xTR | Target: Proxy ETRs | Operation: Delete)");
                process_func = lmapi_xtr_petrs_delete;
                break;
            default:
                LMLOG(LWRN, "LMAPI call = (Device: xTR | Target: Mapping DB | Operation: Unsupported)");
                break;
            }
            break;
        default:
        	LMLOG(LWRN, "LMAPI call = (Device: xTR | Target: Unsupported)");
            break;
        }
        break;
    case LMAPI_DEV_RTR:
        if (lisp_ctrl_dev_mode(ctrl_dev) != RTR_MODE){
            LMLOG(LDBG_1, "LMAPI call = Call API from wrong device");
            break;
        }
        switch (target){
        case LMAPI_TRGT_MRLIST:
            switch (operation){
            case LMAPI_OPR_CREATE:
                LMLOG(LDBG_2, "LMAPI call = (Device: RTR | Target: MR list | Operation: Create)");
                process_func = lmapi_xtr_mr_create;
                break;
            case LMAPI_OPR_DELETE:
                LMLOG(LDBG_2, "LMAPI call = (Device: RTR | Target: MR list | Operation: Delete)");
                process_func = lmapi_xtr_mr_delete;
                break;
            default:
                LMLOG(LWRN, "LMAPI call = (Device: RTR | Target: MR list | Operation: Unsupported)");
                break;
            }
            break;
        default:
            LMLOG(LWRN, "LMAPI call = (Device: RTR | Target: Unsupported)");
            break;
        }
        break;
    default:
    	LMLOG(LWRN, "LMAPI call = (Device: Unsupported)");
        break;
    }

    return process_func;
}

void lmapi_loop(lmapi_connection_t *conn) {

    uint8_t *buffer;
    uint8_t *data;
    int nbytes;
    int datalen;
    lmapi_msg_hdr_t *header;
    int (*process_func)(lmapi_connection_t *, lmapi_msg_hdr_t *, uint8_t *) = NULL;
    uint8_t *       result_msg      = NULL;
    int             result_msg_len  = 0;

    buffer = xzalloc(MAX_API_PKT_LEN);

    nbytes = lmapi_recv(conn,buffer,LMAPI_DONTWAIT);

    if (nbytes == LMAPI_NOTHINGTOREAD){
    	goto end;
    }

    if (nbytes == LMAPI_ERROR){
    	LMLOG(LERR, "lmapi_loop: Error while trying to retrieve API packet\n");
    	goto end;
    }

    header = (lmapi_msg_hdr_t *)buffer;

    data = CO(buffer,sizeof(lmapi_msg_hdr_t));
    datalen = nbytes - sizeof(lmapi_msg_hdr_t);

    if (header->datalen < datalen){
        LMLOG(LWRN, "lmapi_loop: API packet longer than expected\n");
    }
    else if (header->datalen > datalen){
        LMLOG(LERR, "lmapi_loop: API packet shorter than expected\n");
        goto end;
    }

    process_func = lmapi_get_proc_func(header);

    if (process_func != NULL){
    	(*process_func)(conn,header,data);
    }else {
        result_msg_len = lmapi_result_msg_new(&result_msg,header->device,header->target,header->operation,LMAPI_RES_ERR);
        lmapi_send(conn,result_msg,result_msg_len,LMAPI_NOFLAGS);
    }

end:
    free(buffer);

    return;
}


