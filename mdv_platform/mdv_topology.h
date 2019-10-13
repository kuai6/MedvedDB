/**
 * @file mdv_topology.h
 * @author Vladislav Volkov (wwwvladislav@gmail.com)
 * @brief Network topology description and functions for extraction and serialization.
 * @version 0.1
 * @date 2019-08-26
 * @copyright Copyright (c) 2019, Vladislav Volkov
 */
#pragma once
#include "mdv_uuid.h"
#include "mdv_vector.h"


/// Node description
typedef struct
{
    mdv_uuid    uuid;                   ///< Unique identifier
    char const *addr;                   ///< Address
} mdv_toponode;


/// Link description
typedef struct
{
    uint32_t    node[2];                ///< Linked nodes indices
    uint32_t    weight;                 ///< Link weight. Bigger is better.
} mdv_topolink;


/// Topology description
typedef struct mdv_topology mdv_topology;


/// Topologies difference
typedef struct
{
    mdv_topology *ab;                   ///< {a} - {b}
    mdv_topology *ba;                   ///< {b} - {a}
} mdv_topology_delta;


extern mdv_topology mdv_empty_topology;
extern mdv_topology_delta mdv_empty_topology_delta;


/**
 * @brief Two links comparision
 *
 * @param a [in]    first link
 * @param b [in]    second link
 *
 * @return an integer less than zero if a is less then b
 * @return zero if a is equal to b
 * @return an integer greater than zero if a is greater then b
 */
int mdv_link_cmp(mdv_topolink const *a, mdv_topolink const *b);


/**
 * @brief Creates new network topology
 *
 * @param nodes [in]        Nodes array (vector<mdv_toponode>)
 * @param links [in]        Links array (vector<mdv_topolink>)
 * @param extradata [in]    Additional data
 *
 * @return if there is no errors returns non-zero topology pointer
 */
mdv_topology * mdv_topology_create(mdv_vector *nodes, mdv_vector *links, mdv_vector *extradata);


/**
 * @brief Retains topology.
 * @details Reference counter is increased by one.
 */
mdv_topology * mdv_topology_retain(mdv_topology *topology);


/**
 * @brief Releases topology.
 * @details Reference counter is decreased by one.
 *          When the reference counter reaches zero, the topology's destructor is called.
 */
uint32_t mdv_topology_release(mdv_topology *topology);


/**
 * @brief Returns topology nodes
 */
mdv_vector * mdv_topology_nodes(mdv_topology *topology);


/**
 * @brief Returns topology links
 */
mdv_vector * mdv_topology_links(mdv_topology *topology);


/**
 * @brief Returns topology extradata vector
 */
mdv_vector * mdv_topology_extradata(mdv_topology *topology);


/**
 * @brief Topologies difference calculation (i.e. {a} - {b} and {b} - {a})
 * @details Links should be sorted in ascending order.
 *
 * @param a [in] first topology
 * @param b [in] second topology
 *
 * @return topologies difference
 */
mdv_topology_delta * mdv_topology_diff(mdv_topology *a, mdv_topology *b);


/**
 * @brief Free the network topologies difference
 *
 * @param delta [in] topologies difference
 */
void mdv_topology_delta_free(mdv_topology_delta *delta);
