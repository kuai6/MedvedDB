#include "mdv_peer.h"
#include "mdv_p2pmsg.h"
#include "mdv_config.h"
#include <mdv_version.h>
#include <mdv_timerfd.h>
#include <mdv_alloc.h>
#include <mdv_log.h>
#include <mdv_socket.h>
#include <mdv_time.h>
#include <mdv_limits.h>
#include <mdv_topology.h>
#include <mdv_rollbacker.h>
#include <string.h>
#include "storage/async/mdv_nodes.h"
#include "mdv_gossip.h"


static mdv_errno mdv_peer_connected(mdv_peer *peer, char const *addr, mdv_uuid const *uuid, uint32_t *id);
static void      mdv_peer_disconnected(mdv_peer *peer, mdv_uuid const *uuid);


static mdv_errno mdv_peer_reply(mdv_peer *peer, mdv_msg const *msg);


static mdv_errno mdv_peer_hello_reply(mdv_peer *peer, uint16_t id, mdv_msg_p2p_hello const *msg)
{
    binn hey;

    if (!mdv_binn_p2p_hello(msg, &hey))
        return MDV_FAILED;

    mdv_msg const message =
    {
        .hdr =
        {
            .id     = mdv_message_id(p2p_hello),
            .number = id,
            .size   = binn_size(&hey)
        },
        .payload = binn_ptr(&hey)
    };

    mdv_errno err = mdv_peer_reply(peer, &message);

    binn_free(&hey);

    return err;
}


static mdv_errno mdv_peer_topodiff_reply(mdv_peer *peer, uint16_t id, mdv_msg_p2p_topodiff const *msg)
{
    binn obj;

    if (!mdv_binn_p2p_topodiff(msg, &obj))
        return MDV_FAILED;

    mdv_msg const message =
    {
        .hdr =
        {
            .id     = mdv_message_id(p2p_topodiff),
            .number = id,
            .size   = binn_size(&obj)
        },
        .payload = binn_ptr(&obj)
    };

    mdv_errno err = mdv_peer_reply(peer, &message);

    binn_free(&obj);

    return err;
}


static mdv_errno mdv_peer_hello_handler(mdv_msg const *msg, void *arg)
{
    mdv_peer *peer = arg;

    binn binn_msg;

    if(!binn_load(msg->payload, &binn_msg))
    {
        MDV_LOGW("Message '%s' reading failed", mdv_p2p_msg_name(msg->hdr.id));
        return MDV_FAILED;
    }

    mdv_msg_p2p_hello req;

    if (!mdv_unbinn_p2p_hello(&binn_msg, &req))
    {
        binn_free(&binn_msg);
        return MDV_FAILED;
    }

    MDV_LOGI("<<<<< %s '%s'", mdv_uuid_to_str(&req.uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));

    if(req.version != MDV_VERSION)
    {
        MDV_LOGE("Invalid peer version");
        binn_free(&binn_msg);
        return MDV_FAILED;
    }

    peer->peer_uuid = req.uuid;

    mdv_errno err = MDV_OK;

    if(peer->conctx->dir == MDV_CHIN)
    {
        mdv_msg_p2p_hello const hello =
        {
            .version = MDV_VERSION,
            .uuid    = peer->conctx->cluster->uuid,
            .listen  = MDV_CONFIG.server.listen.ptr
        };

        err = mdv_peer_hello_reply(peer, msg->hdr.number, &hello);
    }

    if (err == MDV_OK)
        err = mdv_peer_connected(peer, req.listen, &req.uuid, &peer->peer_id);
    else
        MDV_LOGE("Peer reply failed with error '%s' (%d)", mdv_strerror(err), err);

    binn_free(&binn_msg);

    if (err != MDV_OK)
        MDV_LOGE("Peer registration failed with error '%s' (%d)", mdv_strerror(err), err);

    return err;
}


static mdv_errno mdv_peer_linkstate_handler(mdv_msg const *msg, void *arg)
{
    mdv_peer *peer = arg;
    mdv_core *core = peer->core;

    MDV_LOGI("<<<<< %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));

    mdv_errno err = mdv_gossip_linkstate_handler(core, msg);

    // Update routing table
    if (err == MDV_OK)
        mdv_datasync_update_routes(&core->datasync);

    return err;
}


static void mdv_peer_toposync_node_add(mdv_peer *peer, mdv_uuid const *uuid, char const *addr)
{
    mdv_core *core = peer->core;
    mdv_tracker *tracker = core->tracker;

    size_t const addr_len = strlen(addr);
    size_t const node_size = offsetof(mdv_node, addr) + addr_len + 1;

    if (addr_len > MDV_ADDR_LEN_MAX)
    {
        MDV_LOGE("Invalid address length: %zu", addr_len);
        return;
    }

    char buf[node_size];

    mdv_node *node = (mdv_node *)buf;

    memset(node, 0, sizeof *node);

    node->size      = node_size;
    node->uuid      = *uuid;
    node->userdata  = 0;
    node->id        = 0;
    node->connected = 0;
    node->accepted  = 0;
    node->active    = 1;

    memcpy(node->addr, addr, addr_len + 1);

    // Save peer information and connection state in memory
    if (mdv_tracker_append(tracker, node, true))
        mdv_nodes_store_async(core->jobber, core->storage.metainf, node);
}


static mdv_errno mdv_peer_toposync_handler(mdv_msg const *msg, void *arg)
{
    mdv_peer *peer = arg;
    mdv_core *core = peer->core;
    mdv_tracker *tracker = core->tracker;

    MDV_LOGI("<<<<< %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));

    mdv_rollbacker *rollbacker = mdv_rollbacker_create(4);

    binn binn_msg;

    if(!binn_load(msg->payload, &binn_msg))
    {
        MDV_LOGW("Message '%s' reading failed", mdv_p2p_msg_name(msg->hdr.id));
        mdv_rollback(rollbacker);
        return MDV_FAILED;
    }

    mdv_rollbacker_push(rollbacker, binn_free, &binn_msg);

    mdv_msg_p2p_toposync req;

    if (!mdv_unbinn_p2p_toposync(&binn_msg, &req))
    {
        MDV_LOGE("Topology synchronization request processing failed");
        mdv_rollback(rollbacker);
        return MDV_FAILED;
    }

    mdv_rollbacker_push(rollbacker, mdv_p2p_toposync_free, &req);

    mdv_topology *topology = mdv_tracker_topology(tracker);

    if (!topology)
    {
        MDV_LOGE("Topology synchronization request processing failed");
        mdv_rollback(rollbacker);
        return MDV_FAILED;
    }

    mdv_rollbacker_push(rollbacker, mdv_topology_release, topology);

    // Topologies difference calculation
    mdv_topology_delta *delta = mdv_topology_diff(topology, req.topology);

    if (!delta)
    {
        MDV_LOGE("Topology synchronization request processing failed");
        mdv_rollback(rollbacker);
        return MDV_FAILED;
    }

    mdv_rollbacker_push(rollbacker, mdv_topology_delta_free, delta);

    mdv_msg_p2p_topodiff const topodiff =
    {
        .topology = delta->ab
    };

    mdv_errno err = mdv_peer_topodiff_reply(peer, msg->hdr.number, &topodiff);

    mdv_vector *ba_links = mdv_topology_links(delta->ba);

    if (delta->ba && !mdv_vector_empty(ba_links))
    {
        // Two isolated segments joined
        // TODO: Broadcast delta->ba to own segment.

        mdv_vector *ba_nodes = mdv_topology_nodes(delta->ba);

        mdv_vector_foreach(ba_nodes, mdv_toponode, node)
        {
            mdv_peer_toposync_node_add(peer, &node->uuid, node->addr);
        }

        mdv_vector_foreach(ba_links, mdv_topolink, link)
        {
            mdv_toponode const *src_node = mdv_vector_at(ba_nodes, link->node[0]);
            mdv_toponode const *dst_node = mdv_vector_at(ba_nodes, link->node[1]);
            mdv_tracker_linkstate(tracker, &src_node->uuid, &dst_node->uuid, true);
        }

        mdv_vector_release(ba_nodes);
    }

    mdv_vector_release(ba_links);

    mdv_rollback(rollbacker);

    return err;
}


static mdv_errno mdv_peer_topodiff_handler(mdv_msg const *msg, void *arg)
{
    mdv_peer *peer = arg;
    mdv_core *core = peer->core;
    mdv_tracker *tracker = core->tracker;

    MDV_LOGI("<<<<< %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));

    mdv_rollbacker *rollbacker = mdv_rollbacker_create(2);

    binn binn_msg;

    if(!binn_load(msg->payload, &binn_msg))
    {
        MDV_LOGW("Message '%s' reading failed", mdv_p2p_msg_name(msg->hdr.id));
        mdv_rollback(rollbacker);
        return MDV_FAILED;
    }

    mdv_rollbacker_push(rollbacker, binn_free, &binn_msg);

    mdv_msg_p2p_topodiff req;

    if (!mdv_unbinn_p2p_topodiff(&binn_msg, &req))
    {
        MDV_LOGE("Topology difference processing failed");
        mdv_rollback(rollbacker);
        return MDV_FAILED;
    }

    mdv_rollbacker_push(rollbacker, mdv_p2p_topodiff_free, &req);

    mdv_topology *topology = req.topology;

    if (topology)
    {
        // TODO: Broadcast topology to own segment if segment was isolated.

        mdv_vector *toponodes = mdv_topology_nodes(topology);
        mdv_vector *topolinks = mdv_topology_links(topology);

        mdv_vector_foreach(toponodes, mdv_toponode, node)
        {
            mdv_peer_toposync_node_add(peer, &node->uuid, node->addr);
        }

        mdv_vector_foreach(topolinks, mdv_topolink, link)
        {
            mdv_toponode const *src_node = mdv_vector_at(toponodes, link->node[0]);
            mdv_toponode const *dst_node = mdv_vector_at(toponodes, link->node[1]);
            mdv_tracker_linkstate(tracker, &src_node->uuid, &dst_node->uuid, true);
        }

        mdv_vector_release(toponodes);
        mdv_vector_release(topolinks);
    }

    mdv_rollback(rollbacker);

    // Update routing table
    mdv_datasync_update_routes(&core->datasync);

    return MDV_OK;
}



static mdv_errno mdv_peer_cfslog_sync_handler(mdv_msg const *msg, void *arg)
{
    mdv_peer *peer = arg;
    mdv_core *core = peer->core;

    MDV_LOGI("<<<<< %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));

    return mdv_datasync_cfslog_sync_handler(&core->datasync, peer->peer_id, msg);
}


static mdv_errno mdv_peer_cfslog_state_handler(mdv_msg const *msg, void *arg)
{
    mdv_peer *peer = arg;
    mdv_core *core = peer->core;

    MDV_LOGI("<<<<< %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));

    return mdv_datasync_cfslog_state_handler(&core->datasync, peer->peer_id, msg);
}


static mdv_errno mdv_peer_cfslog_data_handler(mdv_msg const *msg, void *arg)
{
    mdv_peer *peer = arg;
    mdv_core *core = peer->core;

    MDV_LOGI("<<<<< %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));

    mdv_errno err = mdv_datasync_cfslog_data_handler(&core->datasync, peer->peer_id, msg);

    if (err == MDV_OK)
        mdv_committer_start(&core->committer);

    return err;
}


/**
 * @brief Post hello message
 */
static mdv_errno mdv_peer_hello(mdv_peer *peer)
{
    mdv_msg_p2p_hello hello =
    {
        .version = MDV_VERSION,
        .uuid    = peer->conctx->cluster->uuid,
        .listen  = MDV_CONFIG.server.listen.ptr
    };

    binn hey;

    if (!mdv_binn_p2p_hello(&hello, &hey))
        return MDV_FAILED;

    mdv_msg message =
    {
        .hdr =
        {
            .id = mdv_message_id(p2p_hello),
            .size = binn_size(&hey)
        },
        .payload = binn_ptr(&hey)
    };

    mdv_errno err = mdv_peer_post(peer, &message);

    binn_free(&hey);

    return err;
}


/**
 * @brief Post topology synchronization message
 */
static mdv_errno mdv_peer_toposync(mdv_peer *peer)
{
    mdv_core *core = peer->core;
    mdv_tracker *tracker = core->tracker;

    mdv_topology *topology = mdv_tracker_topology(tracker);

    if (!topology)
    {
        MDV_LOGE("Topology synchronization request failed");
        return MDV_FAILED;
    }

    mdv_msg_p2p_toposync const toposync =
    {
        .topology = topology
    };

    binn obj;

    if (!mdv_binn_p2p_toposync(&toposync, &obj))
    {
        MDV_LOGE("Topology synchronization request failed");
        mdv_topology_release(topology);
        return MDV_FAILED;
    }

    mdv_msg message =
    {
        .hdr =
        {
            .id = mdv_message_id(p2p_toposync),
            .size = binn_size(&obj)
        },
        .payload = binn_ptr(&obj)
    };

    mdv_errno err = mdv_peer_post(peer, &message);

    binn_free(&obj);

    mdv_topology_release(topology);

    return err;
}


mdv_errno mdv_peer_init(void *ctx, mdv_conctx *conctx, void *userdata)
{
    mdv_peer *peer = ctx;

    MDV_LOGD("Peer %p initialize", peer);

    peer->core          = userdata;
    peer->conctx        = conctx;
    peer->peer_id       = 0;

    mdv_dispatcher_handler const handlers[] =
    {
        { mdv_message_id(p2p_hello),        &mdv_peer_hello_handler,        peer },
        { mdv_message_id(p2p_linkstate),    &mdv_peer_linkstate_handler,    peer },
        { mdv_message_id(p2p_toposync),     &mdv_peer_toposync_handler,     peer },
        { mdv_message_id(p2p_topodiff),     &mdv_peer_topodiff_handler,     peer },
        { mdv_message_id(p2p_cfslog_sync),  &mdv_peer_cfslog_sync_handler,  peer },
        { mdv_message_id(p2p_cfslog_state), &mdv_peer_cfslog_state_handler, peer },
        { mdv_message_id(p2p_cfslog_data),  &mdv_peer_cfslog_data_handler,  peer },
    };

    for(size_t i = 0; i < sizeof handlers / sizeof *handlers; ++i)
    {
        if (mdv_dispatcher_reg(conctx->dispatcher, handlers + i) != MDV_OK)
        {
            MDV_LOGE("Messages dispatcher handler not registered");
            return MDV_FAILED;
        }
    }

    if (conctx->dir == MDV_CHOUT)
    {
        if (mdv_peer_hello(peer) != MDV_OK)
        {
            MDV_LOGD("Peer handshake message failed");
            return MDV_FAILED;
        }
    }

    MDV_LOGD("Peer %p initialized", peer);

    return MDV_OK;
}


mdv_errno mdv_peer_send(mdv_peer *peer, mdv_msg *req, mdv_msg *resp, size_t timeout)
{
    MDV_LOGI(">>>>> %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(req->hdr.id));

    mdv_errno err = MDV_CLOSED;

    mdv_conctx *conctx = mdv_cluster_conctx_retain(peer->conctx);

    if (conctx)
    {
        err = mdv_dispatcher_send(peer->conctx->dispatcher, req, resp, timeout);
        mdv_cluster_conctx_release(conctx);
    }

    return err;
}


mdv_errno mdv_peer_post(mdv_peer *peer, mdv_msg *msg)
{
    MDV_LOGI(">>>>> %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));

    mdv_errno err = MDV_CLOSED;

    mdv_conctx *conctx = mdv_cluster_conctx_retain(peer->conctx);

    if (conctx)
    {
        err = mdv_dispatcher_post(peer->conctx->dispatcher, msg);
        mdv_cluster_conctx_release(conctx);
    }

    return err;
}


mdv_errno mdv_peer_node_post(mdv_node *peer, void *msg)
{
    return mdv_peer_post((mdv_peer *)peer->userdata, (mdv_msg *)msg);
}


static mdv_errno mdv_peer_reply(mdv_peer *peer, mdv_msg const *msg)
{
    MDV_LOGI(">>>>> %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));
    return mdv_dispatcher_reply(peer->conctx->dispatcher, msg);
}


void mdv_peer_free(void *ctx, mdv_conctx *conctx)
{
    mdv_peer *peer = ctx;
    MDV_LOGD("Peer %p freed", peer);
    mdv_peer_disconnected(peer, &peer->peer_uuid);
}


static mdv_errno mdv_peer_connected(mdv_peer *peer, char const *addr, mdv_uuid const *uuid, uint32_t *id)
{
    mdv_core *core = peer->core;
    mdv_tracker *tracker = core->tracker;

    size_t const addr_len = strlen(addr);
    size_t const node_size = offsetof(mdv_node, addr) + addr_len + 1;

    char buf[node_size];

    mdv_node *node = (mdv_node *)buf;

    memset(node, 0, sizeof *node);

    node->size      = node_size;
    node->uuid      = *uuid;
    node->userdata  = peer;
    node->id        = 0;
    node->connected = 1;
    node->accepted  = peer->conctx->dir == MDV_CHIN;
    node->active    = 1;

    memcpy(node->addr, addr, addr_len + 1);

    // Save peer information and connection state in memory
    mdv_errno err = mdv_tracker_peer_connected(tracker, node);

    // Update routing table
    if (err == MDV_OK)
    {
        mdv_datasync_update_routes(&core->datasync);
        mdv_datasync_start(&core->datasync);
    }

    if (peer->conctx->dir == MDV_CHOUT)
    {
        // Synchronize network topology
        mdv_peer_toposync(peer);

        // Link-state broadcasting to all network
        mdv_gossip_linkstate(core,
                            &core->metainf.uuid.value, MDV_CONFIG.server.listen.ptr,
                            &node->uuid, node->addr,
                            true);
    }
    else
    {
        // Save peer address in DB
        mdv_nodes_store_async(core->jobber, core->storage.metainf, node);
    }

    if (err == MDV_OK)
        *id = node->id;

    return err;
}


static void mdv_peer_disconnected(mdv_peer *peer, mdv_uuid const *uuid)
{
    mdv_core *core = peer->core;
    mdv_tracker *tracker = core->tracker;

    // Save peer connection state in memory
    mdv_tracker_peer_disconnected(tracker, uuid);

    // Update routing table
    mdv_datasync_update_routes(&core->datasync);

    mdv_node *node = mdv_tracker_node_by_id(tracker, peer->peer_id);

    // Link-state broadcasting to all network
    mdv_gossip_linkstate(core,
                        &core->metainf.uuid.value, MDV_CONFIG.server.listen.ptr,
                        uuid, node ? node->addr : "UNKNOWN",
                        false);
}

