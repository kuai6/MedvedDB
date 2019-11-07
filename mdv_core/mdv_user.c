#include "mdv_user.h"
#include "mdv_config.h"
#include "mdv_conctx.h"
#include "event/mdv_types.h"
#include "event/mdv_topology.h"
#include <mdv_messages.h>
#include <mdv_version.h>
#include <mdv_alloc.h>
#include <mdv_log.h>
#include <mdv_dispatcher.h>
#include <mdv_rollbacker.h>
#include <mdv_ctypes.h>
#include <mdv_mutex.h>
#include <stdatomic.h>


struct mdv_user
{
    mdv_conctx              base;           ///< connection context base type
    atomic_uint             rc;             ///< References counter
    mdv_dispatcher         *dispatcher;     ///< Messages dispatcher
    mdv_ebus               *ebus;           ///< Events bus
    mdv_mutex               topomutex;      ///< Mutex for topology guard
    mdv_topology           *topology;       ///< Current network topology
};


static mdv_errno mdv_user_reply(mdv_user *user, mdv_msg const *msg);


static mdv_errno mdv_user_wave_reply(mdv_user *user, uint16_t id, mdv_msg_hello const *msg)
{
    binn hey;

    if (!mdv_binn_hello(msg, &hey))
        return MDV_FAILED;

    mdv_msg message =
    {
        .hdr =
        {
            .id     = mdv_msg_hello_id,
            .number = id,
            .size   = binn_size(&hey)
        },
        .payload = binn_ptr(&hey)
    };

    mdv_errno err = mdv_user_reply(user, &message);

    binn_free(&hey);

    return err;
}


static mdv_errno mdv_user_status_reply(mdv_user *user, uint16_t id, mdv_msg_status const *msg)
{
    binn status;

    if (!mdv_binn_status(msg, &status))
        return MDV_FAILED;

    mdv_msg message =
    {
        .hdr =
        {
            .id = mdv_msg_status_id,
            .number = id,
            .size = binn_size(&status)
        },
        .payload = binn_ptr(&status)
    };

    mdv_errno err = mdv_user_reply(user, &message);

    binn_free(&status);

    return err;
}


static mdv_errno mdv_user_table_info_reply(mdv_user *user, uint16_t id, mdv_msg_table_info const *msg)
{
    binn table_info;

    if (!mdv_binn_table_info(msg, &table_info))
        return MDV_FAILED;

    mdv_msg message =
    {
        .hdr =
        {
            .id = mdv_msg_table_info_id,
            .number = id,
            .size = binn_size(&table_info)
        },
        .payload = binn_ptr(&table_info)
    };

    mdv_errno err = mdv_user_reply(user, &message);

    binn_free(&table_info);

    return err;
}


static mdv_errno mdv_user_topology_reply(mdv_user *user, uint16_t id, mdv_msg_topology const *msg)
{
    binn obj;

    if (!mdv_binn_topology(msg, &obj))
        return MDV_FAILED;

    mdv_msg message =
    {
        .hdr =
        {
            .id = mdv_msg_topology_id,
            .number = id,
            .size = binn_size(&obj)
        },
        .payload = binn_ptr(&obj)
    };

    mdv_errno err = mdv_user_reply(user, &message);

    binn_free(&obj);

    return err;
}


static mdv_errno mdv_user_wave_handler(mdv_msg const *msg, void *arg)
{
    MDV_LOGI("<<<<< '%s'", mdv_msg_name(msg->hdr.id));

    mdv_user *user = arg;

    binn binn_msg;

    if(!binn_load(msg->payload, &binn_msg))
    {
        MDV_LOGW("Message '%s' reading failed", mdv_msg_name(msg->hdr.id));
        return MDV_FAILED;
    }

    mdv_msg_hello client_hello = {};

    if (!mdv_unbinn_hello(&binn_msg, &client_hello))
    {
        MDV_LOGE("Invalid '%s' message", mdv_msg_name(msg->hdr.id));
        binn_free(&binn_msg);
        return MDV_FAILED;
    }

    binn_free(&binn_msg);

    if(client_hello.version != MDV_VERSION)
    {
        MDV_LOGE("Invalid client version");
        return MDV_FAILED;
    }

    mdv_msg_hello hello =
    {
        .version = MDV_VERSION,
        .uuid = {}
    };

    return mdv_user_wave_reply(user, msg->hdr.number, &hello);
}


static mdv_errno mdv_user_create_table_handler(mdv_msg const *msg, void *arg)
{
    MDV_LOGI("<<<<< '%s'", mdv_msg_name(msg->hdr.id));
/* TODO
    mdv_user    *user    = arg;
    mdv_core    *core    = user->core;
    mdv_tracker *tracker = core->tracker;

    binn binn_msg;

    if(!binn_load(msg->payload, &binn_msg))
    {
        MDV_LOGW("Message '%s' reading failed", mdv_msg_name(msg->hdr.id));
        return MDV_FAILED;
    }

    mdv_msg_create_table_base *create_table = mdv_unbinn_create_table(&binn_msg);

    binn_free(&binn_msg);

    if (!create_table)
    {
        MDV_LOGE("Invalid '%s' message", mdv_msg_name(mdv_msg_create_table_id));
        return MDV_FAILED;
    }

    mdv_objid const *objid = mdv_tablespace_create_table(&user->core->storage.tablespace, (mdv_table_base*)&create_table->table);

    mdv_errno err = MDV_FAILED;

    if (objid)
    {
        mdv_datasync_start(&core->datasync);
        mdv_committer_start(&core->committer);

        assert(objid->node == MDV_LOCAL_ID);

        mdv_msg_table_info const table_info =
        {
            .id =
            {
                .node = user->core->metainf.uuid.value,
                .id = objid->id
            }
        };

        err = mdv_user_table_info_reply(user, msg->hdr.number, &table_info);
    }
    else
    {
        mdv_msg_status const status =
        {
            .err = MDV_FAILED,
            .message = ""
        };

        err = mdv_user_status_reply(user, msg->hdr.number, &status);
    }

    mdv_free(create_table, "msg_create_table");

    return err;
*/
    return MDV_FAILED;
}


static mdv_errno mdv_user_get_topology_handler(mdv_msg const *msg, void *arg)
{
    MDV_LOGI("<<<<< '%s'", mdv_msg_name(msg->hdr.id));
/* TODO
    mdv_user    *user   = arg;
    mdv_core    *core   = user->core;
    mdv_tracker *tracker = core->tracker;

    mdv_topology *topology = mdv_tracker_topology(tracker);

    if (topology)
    {
        mdv_msg_topology const msg_topology =
        {
            .topology = topology
        };

        mdv_errno err = mdv_user_topology_reply(user, msg->hdr.number, &msg_topology);

        mdv_topology_release(topology);

        return err;
    }

    mdv_msg_status const status =
    {
        .err = MDV_FAILED,
        .message = "Topology getting failed"
    };

    return mdv_user_status_reply(user, msg->hdr.number, &status);
*/
    return MDV_FAILED;
}


static mdv_errno mdv_user_evt_topology(void *arg, mdv_event *event)
{
    mdv_user *user = arg;
    mdv_evt_topology *topo = (mdv_evt_topology *)event;

    // TODO

    return MDV_OK;
}


static const mdv_event_handler_type mdv_user_handlers[] =
{
    { MDV_EVT_TOPOLOGY,    mdv_user_evt_topology },
};


mdv_user * mdv_user_create(mdv_descriptor fd, mdv_ebus *ebus, mdv_topology *topology)
{
    mdv_rollbacker *rollbacker = mdv_rollbacker_create(4);

    mdv_user *user = mdv_alloc(sizeof(mdv_user), "userctx");

    if (!user)
    {
        MDV_LOGE("No memory for user connection context");
        mdv_rollback(rollbacker);
        return 0;
    }

    mdv_rollbacker_push(rollbacker, mdv_free, user, "userctx");

    MDV_LOGD("User %p initialization", user);

    static mdv_iconctx iconctx =
    {
        .retain = (mdv_conctx * (*)(mdv_conctx *)) mdv_user_retain,
        .release = (uint32_t (*)(mdv_conctx *))    mdv_user_release
    };

    atomic_init(&user->rc, 1);

    user->base.type = MDV_CTX_USER;
    user->base.vptr = &iconctx;

    user->topology = mdv_topology_retain(topology);
    mdv_rollbacker_push(rollbacker, mdv_topology_release, topology);

    user->ebus = mdv_ebus_retain(ebus);
    mdv_rollbacker_push(rollbacker, mdv_ebus_release, user->ebus);

    user->dispatcher = mdv_dispatcher_create(fd);

    if (!user->dispatcher)
    {
        MDV_LOGE("User connection context creation failed");
        mdv_rollback(rollbacker);
        return 0;
    }

    mdv_rollbacker_push(rollbacker, mdv_dispatcher_free, user->dispatcher);

    mdv_dispatcher_handler const handlers[] =
    {
        { mdv_message_id(hello),         &mdv_user_wave_handler,         user },
        { mdv_message_id(create_table),  &mdv_user_create_table_handler, user },
        { mdv_message_id(get_topology),  &mdv_user_get_topology_handler, user },
    };

    for(size_t i = 0; i < sizeof handlers / sizeof *handlers; ++i)
    {
        if (mdv_dispatcher_reg(user->dispatcher, handlers + i) != MDV_OK)
        {
            MDV_LOGE("Messages dispatcher handler not registered");
            mdv_rollback(rollbacker);
            return 0;
        }
    }

    if (mdv_ebus_subscribe_all(user->ebus,
                               user,
                               mdv_user_handlers,
                               sizeof mdv_user_handlers / sizeof *mdv_user_handlers) != MDV_OK)
    {
        MDV_LOGE("Ebus subscription failed");
        mdv_rollback(rollbacker);
        return 0;
    }

    MDV_LOGD("User %p initialized", user);

    mdv_rollbacker_free(rollbacker);

    return user;
}


static void mdv_user_free(mdv_user *user)
{
    if(user)
    {
        mdv_ebus_unsubscribe_all(user->ebus,
                                 user,
                                 mdv_user_handlers,
                                 sizeof mdv_user_handlers / sizeof *mdv_user_handlers);
        mdv_dispatcher_free(user->dispatcher);
        mdv_ebus_release(user->ebus);
        mdv_topology_release(user->topology);
        mdv_free(user, "userctx");
        MDV_LOGD("User %p freed", user);
    }
}


mdv_user * mdv_user_retain(mdv_user *user)
{
    if (user)
    {
        uint32_t rc = atomic_load_explicit(&user->rc, memory_order_relaxed);

        if (!rc)
            return 0;

        while(!atomic_compare_exchange_weak(&user->rc, &rc, rc + 1))
        {
            if (!rc)
                return 0;
        }
    }

    return user;

}


uint32_t mdv_user_release(mdv_user *user)
{
    if (user)
    {
        uint32_t rc = atomic_fetch_sub_explicit(&user->rc, 1, memory_order_relaxed) - 1;

        if (!rc)
            mdv_user_free(user);

        return rc;
    }

    return 0;
}


mdv_errno mdv_user_send(mdv_user *user, mdv_msg *req, mdv_msg *resp, size_t timeout)
{
    MDV_LOGI(">>>>> '%s'", mdv_msg_name(req->hdr.id));

    mdv_errno err = MDV_CLOSED;

    user = mdv_user_retain(user);

    if (user)
    {
        err = mdv_dispatcher_send(user->dispatcher, req, resp, timeout);
        mdv_user_release(user);
    }

    return err;
}


mdv_errno mdv_user_post(mdv_user *user, mdv_msg *msg)
{
    MDV_LOGI(">>>>> '%s'", mdv_msg_name(msg->hdr.id));

    mdv_errno err = MDV_CLOSED;

    user = mdv_user_retain(user);

    if (user)
    {
        err = mdv_dispatcher_post(user->dispatcher, msg);
        mdv_user_release(user);
    }

    return err;
}


static mdv_errno mdv_user_reply(mdv_user *user, mdv_msg const *msg)
{
    MDV_LOGI(">>>>> '%s'", mdv_msg_name(msg->hdr.id));
    return mdv_dispatcher_reply(user->dispatcher, msg);
}


mdv_errno mdv_user_recv(mdv_user *user)
{
    return mdv_dispatcher_read(user->dispatcher);
}


mdv_descriptor mdv_user_fd(mdv_user *user)
{
    return mdv_dispatcher_fd(user->dispatcher);
}

