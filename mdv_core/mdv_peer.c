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
#include <stddef.h>
#include <string.h>
#include "storage/mdv_nodes.h"
#include "mdv_gossip.h"


static mdv_errno mdv_peer_connected(mdv_peer *peer, char const *addr, mdv_uuid const *uuid, uint32_t *id);
static void      mdv_peer_disconnected(mdv_peer *peer, mdv_uuid const *uuid);


static mdv_errno mdv_peer_reply(mdv_peer *peer, mdv_msg const *msg);


static mdv_errno mdv_peer_hello_reply(mdv_peer *peer, uint16_t id, mdv_msg_p2p_hello const *msg)
{
    binn hey;

    if (!mdv_binn_p2p_hello(msg, &hey))
        return MDV_FAILED;

    mdv_msg message =
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


static mdv_errno mdv_peer_hello_handler(mdv_msg const *msg, void *arg)
{
    mdv_peer *peer = arg;

    binn binn_msg;

    if(!binn_load(msg->payload, &binn_msg))
    {
        MDV_LOGW("Message '%s' reading failed", mdv_p2p_msg_name(msg->hdr.id));
        return MDV_FAILED;
    }

    mdv_uuid *uuid = mdv_unbinn_p2p_hello_uuid(&binn_msg);
    uint32_t *version = mdv_unbinn_p2p_hello_version(&binn_msg);
    char const *listen = mdv_unbinn_p2p_hello_listen(&binn_msg);

    if (!uuid
        || !version
        || !listen)
    {
        binn_free(&binn_msg);
        return MDV_FAILED;
    }

    MDV_LOGI("<<<<< %s '%s'", mdv_uuid_to_str(uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));

    if(*version != MDV_VERSION)
    {
        MDV_LOGE("Invalid peer version");
        binn_free(&binn_msg);
        return MDV_FAILED;
    }

    peer->peer_uuid = *uuid;

    mdv_errno err = MDV_OK;

    if(peer->conctx->dir == MDV_CHIN)
    {
        mdv_msg_p2p_hello hello =
        {
            .version = MDV_VERSION,
            .uuid    = peer->conctx->cluster->uuid,
            .listen  = MDV_CONFIG.server.listen.ptr
        };

        err = mdv_peer_hello_reply(peer, msg->hdr.number, &hello);
    }

    if (err == MDV_OK)
        err = mdv_peer_connected(peer, listen, uuid, &peer->peer_id);
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

    MDV_LOGI("<<<<< %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));

    return mdv_gossip_linkstate_handler(peer->core, msg);
}


static mdv_errno mdv_peer_hello(mdv_peer *peer)
{
    // Post hello message

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
        { mdv_message_id(p2p_linkstate),    &mdv_peer_linkstate_handler,    peer }
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
    return mdv_dispatcher_send(peer->conctx->dispatcher, req, resp, timeout);
}


mdv_errno mdv_peer_post(mdv_peer *peer, mdv_msg *msg)
{
    MDV_LOGI(">>>>> %s '%s'", mdv_uuid_to_str(&peer->peer_uuid).ptr, mdv_p2p_msg_name(msg->hdr.id));
    return mdv_dispatcher_post(peer->conctx->dispatcher, msg);
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
    mdv_cluster *cluster = peer->conctx->cluster;

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

    mdv_errno err = mdv_core_peer_connected(peer->core, node);

    if (err == MDV_OK)
        *id = node->id;

    return err;
}


static void mdv_peer_disconnected(mdv_peer *peer, mdv_uuid const *uuid)
{
    mdv_core_peer_disconnected(peer->core, uuid);
}

