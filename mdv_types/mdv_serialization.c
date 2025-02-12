#include "mdv_serialization.h"
#include <mdv_log.h>
#include <mdv_alloc.h>
#include <mdv_rollbacker.h>


static bool binn_field(mdv_field const *field, binn *obj)
{
    if (!binn_create_object(obj))
    {
        MDV_LOGE("binn_field failed");
        return false;
    }

    if (0
        || !binn_object_set_uint32(obj, "T", (unsigned int)field->type)
        || !binn_object_set_uint32(obj, "L", (unsigned int)field->limit)
        || !binn_object_set_str(obj, "N", field->name.ptr))
    {
        MDV_LOGE("binn_field failed");
        binn_free(obj);
        return false;
    }

    return true;
}


static bool binn_add_to_list(binn *list, mdv_field_type type, void const *data)
{
    switch(type)
    {
        case MDV_FLD_TYPE_BOOL:     return binn_list_add_bool(list, *(bool const*)data);         break;
        case MDV_FLD_TYPE_CHAR:     return binn_list_add_int8(list, *(char const*)data);         break;
        case MDV_FLD_TYPE_BYTE:     return binn_list_add_int8(list, *(int8_t const*)data);       break;
        case MDV_FLD_TYPE_INT8:     return binn_list_add_int8(list, *(int8_t const*)data);       break;
        case MDV_FLD_TYPE_UINT8:    return binn_list_add_uint8(list, *(uint8_t const*)data);     break;
        case MDV_FLD_TYPE_INT16:    return binn_list_add_int16(list, *(int16_t const*)data);     break;
        case MDV_FLD_TYPE_UINT16:   return binn_list_add_uint16(list, *(uint16_t const*)data);   break;
        case MDV_FLD_TYPE_INT32:    return binn_list_add_int32(list, *(int32_t const*)data);     break;
        case MDV_FLD_TYPE_UINT32:   return binn_list_add_uint32(list, *(uint32_t const*)data);   break;
        case MDV_FLD_TYPE_INT64:    return binn_list_add_int64(list, *(int64_t const*)data);     break;
        case MDV_FLD_TYPE_UINT64:   return binn_list_add_uint64(list, *(uint64_t const*)data);   break;
        case MDV_FLD_TYPE_FLOAT:    return binn_list_add_float(list, *(float const*)data);       break;
        case MDV_FLD_TYPE_DOUBLE:   return binn_list_add_double(list, *(double const*)data);     break;
    }
    MDV_LOGE("Unknown field type: %u", type);
    return false;
}


static bool binn_get(binn *val, mdv_field_type type, void *data)
{
    bool ret = false;

    switch(type)
    {
        case MDV_FLD_TYPE_BOOL:
        {
            BOOL bool_val = 0;
            ret = binn_get_bool(val, &bool_val);
            *(bool*)data = bool_val != 0;
            break;
        }

        case MDV_FLD_TYPE_CHAR:
        {
            int int_val = 0;
            ret = binn_get_int32(val, &int_val);
            *(char*)data = (char)int_val;
            break;
        }

        case MDV_FLD_TYPE_BYTE:
        case MDV_FLD_TYPE_INT8:
        {
            int int_val = 0;
            ret = binn_get_int32(val, &int_val);
            *(int8_t*)data = (int8_t)int_val;
            break;
        }

        case MDV_FLD_TYPE_UINT8:
        {
            int int_val = 0;
            ret = binn_get_int32(val, &int_val);
            *(uint8_t*)data = (uint8_t)int_val;
            break;
        }

        case MDV_FLD_TYPE_INT16:
        {
            int int_val = 0;
            ret = binn_get_int32(val, &int_val);
            *(int16_t*)data = (int16_t)int_val;
            break;
        }

        case MDV_FLD_TYPE_UINT16:
        {
            int int_val = 0;
            ret = binn_get_int32(val, &int_val);
            *(uint16_t*)data = (uint16_t)int_val;
            break;
        }

        case MDV_FLD_TYPE_INT32:
        {
            int int_val = 0;
            ret = binn_get_int32(val, &int_val);
            *(int32_t*)data = (int32_t)int_val;
            break;
        }

        case MDV_FLD_TYPE_UINT32:
        {
            int64 int64_val = 0;
            ret = binn_get_int64(val, &int64_val);
            *(uint32_t*)data = (uint32_t)int64_val;
            break;
        }

        case MDV_FLD_TYPE_INT64:
        {
            int64 int64_val = 0;
            ret = binn_get_int64(val, &int64_val);
            *(int64_t*)data = (int64_t)int64_val;
            break;
        }

        case MDV_FLD_TYPE_UINT64:
        {
            int64 int64_val = 0;
            ret = binn_get_int64(val, &int64_val);
            *(uint64_t*)data = (uint64_t)int64_val;
            break;
        }

        case MDV_FLD_TYPE_FLOAT:
        {
            double double_val = 0;
            ret = binn_get_double(val, &double_val);
            *(float*)data = (float)double_val;
            break;
        }

        case MDV_FLD_TYPE_DOUBLE:
        {
            ret = binn_get_double(val, data);
            break;
        }
    }

    if (!ret)
        MDV_LOGE("Unknown field type: %u", type);

    return ret;
}


bool mdv_binn_table(mdv_table_base const *table, binn *obj)
{
    if (!binn_create_object(obj))
    {
        MDV_LOGE("binn_table failed");
        return false;
    }

    // Calculate size
    uint32_t size = offsetof(mdv_table_base, fields) + table->size * sizeof(mdv_field);
    size += table->name.size;
    for(uint32_t i = 0; i < table->size; ++i)
        size += table->fields[i].name.size;

    if (0
        || !binn_object_set_str(obj, "N", table->name.ptr)
        || !binn_object_set_uint64(obj, "U0", (uint64)table->id.u64[0])
        || !binn_object_set_uint64(obj, "U1", (uint64)table->id.u64[1])
        || !binn_object_set_uint32(obj, "S", table->size)
        || !binn_object_set_uint32(obj, "B", size))
    {
        MDV_LOGE("binn_table failed");
        binn_free(obj);
        return false;
    }

    binn fields;

    if (!binn_create_list(&fields))
    {
        MDV_LOGE("binn_table failed");
        binn_free(obj);
        return false;
    }

    for(uint32_t i = 0; i < table->size; ++i)
    {
        binn field;
        if(0
           || !binn_field(table->fields + i, &field)
           || !binn_list_add_object(&fields, &field))
        {
            binn_free(&field);
            binn_free(&fields);
            binn_free(obj);
            return false;
        }
        binn_free(&field);
    }

    if (!binn_object_set_list(obj, "F", &fields))
    {
        binn_free(&fields);
        binn_free(obj);
        return false;
    }

    binn_free(&fields);

    return true;
}


mdv_table_base * mdv_unbinn_table(binn const *obj)
{
    uint32_t size = 0;
    if (!binn_object_get_uint32((void*)obj, "B", &size))
    {
        MDV_LOGE("unbinn_table failed");
        return 0;
    }

    mdv_table_base *table = (mdv_table_base *)mdv_alloc(size, "table");

    if (!table)
    {
        MDV_LOGE("unbinn_table failed");
        return 0;
    }

    char *name = 0;

    if (0
        || !binn_object_get_str((void*)obj, "N", &name)
        || !binn_object_get_uint64((void*)obj, "U0", (uint64 *)&table->id.u64[0])
        || !binn_object_get_uint64((void*)obj, "U1", (uint64 *)&table->id.u64[1])
        || !binn_object_get_uint32((void*)obj, "S", &table->size))
    {
        MDV_LOGE("unbinn_table failed");
        mdv_free(table, "table");
        return 0;
    }

    char *buff = (char *)(table->fields + table->size);

    table->name.size = strlen(name) + 1;
    table->name.ptr = buff;
    buff += table->name.size;
    memcpy(table->name.ptr, name, table->name.size);

    binn *fields = 0;
    if (!binn_object_get_list((void*)obj, "F", (void**)&fields))
    {
        MDV_LOGE("unbinn_table failed");
        mdv_free(table, "table");
        return 0;
    }

    binn_iter iter = {};
    binn value = {};
    size_t i = 0;

    binn_list_foreach(fields, value)
    {
        if (i > table->size)
        {
            MDV_LOGE("unbinn_table failed");
            mdv_free(table, "table");
            return 0;
        }

        mdv_field *field = table->fields + i;
        char *field_name = 0;

        if (!binn_object_get_uint32(&value, "T", &field->type)
            || !binn_object_get_uint32(&value, "L", &field->limit)
            || !binn_object_get_str(&value, "N", &field_name))
        {
            MDV_LOGE("unbinn_table failed");
            mdv_free(table, "table");
            return 0;
        }

        field->name.size = strlen(field_name) + 1;
        field->name.ptr = buff;
        buff += field->name.size;
        memcpy(field->name.ptr, field_name, field->name.size);

        ++i;
    }

    if (buff - (char*)table != size)
        MDV_LOGE("memory is corrupted: %p, %zu != %u", table, buff - (char*)table, size);

    return table;
}


bool mdv_binn_row(mdv_field const *fields, mdv_row_base const *row, binn *list)
{
    if (!binn_create_list(list))
    {
        MDV_LOGE("binn_row failed");
        return false;
    }

    for(uint32_t i = 0; i < row->size; ++i)
    {
        uint32_t const field_type_size = mdv_field_type_size(fields[i].type);

        if(!field_type_size)
        {
            MDV_LOGE("binn_row failed. Invalid field type size.");
            binn_free(list);
            return false;
        }

        if(row->fields[i].size % field_type_size)
        {
            MDV_LOGE("binn_row failed. Invalid field size.");
            binn_free(list);
            return false;
        }

        uint32_t const arr_size = row->fields[i].size / field_type_size;

        if (fields[i].limit && fields[i].limit < arr_size)
        {
            MDV_LOGE("binn_row failed. Field is too long.");
            binn_free(list);
            return false;
        }

        BOOL res = true;

        if(fields[i].limit == 1)
            res = binn_add_to_list(list, fields[i].type, row->fields[i].ptr);
        else if (field_type_size == 1)
            res = binn_list_add_blob(list, row->fields[i].ptr, arr_size);
        else
        {
            binn *field_items = binn_list();
            if (!field_items)
            {
                MDV_LOGE("binn_row failed");
                binn_free(list);
                return false;
            }

            for(uint32_t j = 0; res && j < arr_size; ++j)
                res = binn_add_to_list(field_items, fields[i].type, (char const *)row->fields[i].ptr + j * field_type_size);

            if (res)
                res = binn_list_add_list(list, field_items);

            binn_free(field_items);
        }

        if(!res)
        {
            MDV_LOGE("binn_row failed.");
            binn_free(list);
            return false;
        }
    }

    return true;
}


static uint32_t binn_list_size(binn *obj)
{
    binn_iter iter;
    binn value;
    uint32_t n = 0;
    binn_list_foreach(obj, value)
        ++n;
    return n;
}


mdv_row_base * mdv_unbinn_row(binn const *obj, mdv_field const *fields)
{
    binn_iter iter = {};
    binn value = {};

    uint32_t size = offsetof(mdv_row_base, fields);
    uint32_t fields_count = 0;

    // Calculate necessary space for row
    binn_list_foreach((void*)obj, value)
    {
        uint32_t const field_type_size = mdv_field_type_size(fields[fields_count].type);

        if(fields[fields_count].limit == 1)
            size += sizeof(mdv_data) + field_type_size;
        else if (field_type_size == 1)
        {
            int const blob_size = binn_size(&value);
            if (blob_size < 0)
            {
                MDV_LOGE("blob size is negative: %d", blob_size);
                return 0;
            }
            size += sizeof(mdv_data) + blob_size;
        }
        else
            size += sizeof(mdv_data) + field_type_size * binn_list_size(&value);

        ++fields_count;
    }

    mdv_row_base *row = (mdv_row_base *)mdv_alloc(size, "row");
    if (!row)
    {
        MDV_LOGE("unbinn_table failed");
        return 0;
    }

    row->size = fields_count;

    fields_count = 0;

    char *buff = (char *)(row->fields + row->size);

    // Deserialize row
    binn_list_foreach((void*)obj, value)
    {
        uint32_t const field_type_size = mdv_field_type_size(fields[fields_count].type);

        if(fields[fields_count].limit == 1)
        {
            if (!binn_get(&value, fields[fields_count].type, buff))
            {
                MDV_LOGE("unbinn_table failed");
                mdv_free(row, "row");
                return 0;
            }
            row->fields[fields_count].size = field_type_size;
            row->fields[fields_count].ptr = buff;
            buff++;

        }
        else if (field_type_size == 1)
        {
            int const blob_size = binn_size(&value);
            memcpy(buff, binn_ptr(&value), blob_size);
            row->fields[fields_count].size = blob_size;
            row->fields[fields_count].ptr = buff;
            buff += blob_size;
        }
        else
        {
            binn_iter arr_iter = {};
            binn arr_value = {};
            uint32_t arr_len = 0;

            row->fields[fields_count].ptr = buff;

            binn_list_foreach(&value, arr_value)
            {
                if (!binn_get(&arr_value, fields[fields_count].type, buff))
                {
                    MDV_LOGE("unbinn_table failed");
                    mdv_free(row, "row");
                    return 0;
                }

                ++arr_len;
                buff += field_type_size;
            }

            row->fields[fields_count].size = field_type_size * arr_len;
        }

        ++fields_count;
    }

    if (buff - (char*)row != size)
        MDV_LOGE("memory is corrupted: %p, %zu != %u", row, buff - (char*)row, size);

    return row;
}


bool mdv_topology_serialize(mdv_topology *topology, binn *obj)
{
    mdv_rollbacker *rollbacker = mdv_rollbacker_create(6);

    binn nodes;
    binn links;

    if (!binn_create_object(obj))
    {
        MDV_LOGE("binn_topology failed");
        mdv_rollback(rollbacker);
        return false;
    }

    mdv_rollbacker_push(rollbacker, binn_free, obj);

    if (!binn_create_list(&nodes))
    {
        MDV_LOGE("binn_topology failed");
        mdv_rollback(rollbacker);
        return false;
    }

    mdv_rollbacker_push(rollbacker, binn_free, &nodes);

    if (!binn_create_list(&links))
    {
        MDV_LOGE("binn_topology failed");
        mdv_rollback(rollbacker);
        return false;
    }

    mdv_rollbacker_push(rollbacker, binn_free, &links);

    mdv_vector *toponodes = mdv_topology_nodes(topology);
    mdv_vector *topolinks = mdv_topology_links(topology);
    mdv_vector *topoextradata = mdv_topology_extradata(topology);

    mdv_rollbacker_push(rollbacker, mdv_vector_release, toponodes);
    mdv_rollbacker_push(rollbacker, mdv_vector_release, topolinks);
    mdv_rollbacker_push(rollbacker, mdv_vector_release, topoextradata);

    mdv_vector_foreach(toponodes, mdv_toponode, toponode)
    {
        binn node;

        if (!binn_create_object(&node))
        {
            MDV_LOGE("binn_topology failed");
            mdv_rollback(rollbacker);
            return false;
        }

        if (0
            || !binn_object_set_uint64(&node, "U1", toponode->uuid.u64[0])
            || !binn_object_set_uint64(&node, "U2", toponode->uuid.u64[1])
            || !binn_object_set_str(&node, "A", (char*)toponode->addr)
            || !binn_list_add_object(&nodes, &node))
        {
            MDV_LOGE("binn_topology failed");
            binn_free(&node);
            mdv_rollback(rollbacker);
            return false;
        }

        binn_free(&node);
    }

    mdv_vector_foreach(topolinks, mdv_topolink, topolink)
    {
        binn link;
        uint8_t tmp[64];

        if (!binn_create(&link, BINN_OBJECT, sizeof tmp, tmp))
        {
            MDV_LOGE("binn_topology failed");
            mdv_rollback(rollbacker);
            return false;
        }

        if (0
            || !binn_object_set_uint32(&link, "U1", topolink->node[0])
            || !binn_object_set_uint32(&link, "U2", topolink->node[1])
            || !binn_object_set_uint32(&link, "W", topolink->weight)
            || !binn_list_add_object(&links, &link))
        {
            MDV_LOGE("binn_topology failed");
            binn_free(&link);
            mdv_rollback(rollbacker);
            return false;
        }

        binn_free(&link);
    }

    if (0
        || !binn_object_set_uint64(obj, "NC", mdv_vector_size(toponodes))
        || !binn_object_set_uint64(obj, "LC", mdv_vector_size(topolinks))
        || !binn_object_set_uint64(obj, "ES", mdv_vector_size(topoextradata))
        || !binn_object_set_list(obj, "N", &nodes)
        || !binn_object_set_list(obj, "L", &links))
    {
        MDV_LOGE("binn_topology failed");
        mdv_rollback(rollbacker);
        return false;
    }

    mdv_vector_release(toponodes);
    mdv_vector_release(topolinks);
    mdv_vector_release(topoextradata);

    binn_free(&nodes);
    binn_free(&links);

    mdv_rollbacker_free(rollbacker);

    return true;
}


mdv_topology * mdv_topology_deserialize(binn const *obj)
{
    uint64   nodes_count = 0;
    uint64   links_count = 0;
    uint64   extradata_size = 0;
    binn    *nodes = 0;
    binn    *links = 0;

    if (0
        || !binn_object_get_uint64((void*)obj, "NC", &nodes_count)
        || !binn_object_get_uint64((void*)obj, "LC", &links_count)
        || !binn_object_get_uint64((void*)obj, "ES", &extradata_size)
        || !binn_object_get_list((void*)obj, "N", (void**)&nodes)
        || !binn_object_get_list((void*)obj, "L", (void**)&links))
    {
        MDV_LOGE("unbinn_topology failed");
        return 0;
    }

    mdv_rollbacker *rollbacker = mdv_rollbacker_create(3);

    mdv_vector *toponodes = mdv_vector_create(nodes_count,
                                              sizeof(mdv_toponode),
                                              &mdv_default_allocator);

    if(!toponodes)
    {
        MDV_LOGE("No memory for network topology");
        mdv_rollback(rollbacker);
        return 0;
    }

    mdv_rollbacker_push(rollbacker, mdv_vector_release, toponodes);

    mdv_vector *topolinks = mdv_vector_create(links_count,
                                              sizeof(mdv_topolink),
                                              &mdv_default_allocator);

    if(!topolinks)
    {
        MDV_LOGE("No memory for network topology");
        mdv_rollback(rollbacker);
        return 0;
    }

    mdv_rollbacker_push(rollbacker, mdv_vector_release, topolinks);

    mdv_vector *extradata = mdv_vector_create(extradata_size,
                                              sizeof(char),
                                              &mdv_default_allocator);

    if(!extradata)
    {
        MDV_LOGE("No memory for network topology");
        mdv_rollback(rollbacker);
        return 0;
    }

    mdv_rollbacker_push(rollbacker, mdv_vector_release, extradata);

    binn_iter iter = {};
    binn value = {};

    size_t i;

    // load nodes
    i = 0;
    binn_list_foreach(nodes, value)
    {
        if (i++ > nodes_count)
        {
            MDV_LOGE("unbinn_topology failed");
            mdv_rollback(rollbacker);
            return 0;
        }

        mdv_toponode node;

        char *addr = 0;

        if (0
            || !binn_object_get_uint64(&value, "U1", (uint64*)&node.uuid.u64[0])
            || !binn_object_get_uint64(&value, "U2", (uint64*)&node.uuid.u64[1])
            || !binn_object_get_str(&value, "A", &addr))
        {
            MDV_LOGE("unbinn_topology failed");
            mdv_rollback(rollbacker);
            return 0;
        }

        node.addr = mdv_vector_append(extradata, addr, strlen(addr) + 1);

        mdv_vector_push_back(toponodes, &node);
    }

    // load links
    i = 0;
    binn_list_foreach(links, value)
    {
        if (i++ > links_count)
        {
            MDV_LOGE("unbinn_topology failed");
            mdv_rollback(rollbacker);
            return 0;
        }

        mdv_topolink link;

        if (0
            || !binn_object_get_uint32(&value, "U1", link.node + 0)
            || !binn_object_get_uint32(&value, "U2", link.node + 1)
            || !binn_object_get_uint32(&value, "W", &link.weight)
            || link.node[0] >= nodes_count
            || link.node[1] >= nodes_count)
        {
            MDV_LOGE("unbinn_topology failed");
            mdv_rollback(rollbacker);
            return 0;
        }

        mdv_vector_push_back(topolinks, &link);
    }

    mdv_topology *topology = mdv_topology_create(toponodes, topolinks, extradata);

    mdv_rollback(rollbacker);

    return topology;
}
