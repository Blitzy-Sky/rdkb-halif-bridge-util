/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
/**
 * @file network_interface.h
 * @brief Gateway Config table types for the non-OVS bridge arrangement.
 *
 * This header is a type surface and nothing else: it declares no function, defines no
 * macro other than the three buffer sizes below, and includes no other header. It
 * declares the record that describes one row of the Gateway Config table -
 * Gateway_Config_Non_Ovs_Bridge - together with the two enumerations whose members are
 * two of that record's fields.
 *
 * It is an independent surface, not a part of bridge_util_hal.h. That header does not
 * include this one and nothing it declares requires this one, so a caller that needs
 * the gateway configuration record includes this header explicitly; the repository
 * specification states this under `Data Structures and Defines` and `Optional
 * Components` (docs/pages/halSpec.md).
 *
 * The type name records the header's purpose: this is the non-OVS arrangement. The
 * repository specification records under `Variability Management` that this header
 * exists to eliminate the build-time dependency on bridge utilities from OVS, and that
 * no header of this component carries an Open vSwitch dependency.
 *
 * @note No function in this interface consumes any type declared here. The record is
 *       filled in and interpreted by the caller and the platform code that shares the
 *       Gateway Config table; this header only fixes its layout, and it neither states
 *       nor implies which component populates a given field, when a field changes, or
 *       what an implementation does with a particular command value. Where those facts
 *       matter to a caller they must be established outside this interface.
 * @note Every char field is a fixed array of the size named on it, and none carries a
 *       companion length, so a caller that populates one keeps its value inside the
 *       array and terminates it there. This interface declares no validating function
 *       and no function that takes the record at all, so an over-long or unterminated
 *       value is not rejected anywhere and this header states no behaviour for one.
 */

#ifndef _NETWORK_INTERFACE_H_
#define _NETWORK_INTERFACE_H_

/** \def MAX_IF_NAME_SIZE
    \brief Size in bytes of a network interface name string.

    Bounds the Gateway_Config_Non_Ovs_Bridge::if_name and
    Gateway_Config_Non_Ovs_Bridge::parent_ifname arrays. It is the array size, so a
    caller that stores a name and a terminator inside one has at most fifteen characters
    of name available. Note that this is a narrower bound than the 64-byte
    BRIDGE_NAME_SIZE and IFACE_NAME_SIZE of bridge_util_hal.h: a name that fits a bridge
    record's field does not necessarily fit one of these.
*/
#define MAX_IF_NAME_SIZE     16
/** \def MAX_IP_ADDR_SIZE
    \brief Size in bytes of a network IP address string.

    Bounds the Gateway_Config_Non_Ovs_Bridge::inet_addr,
    Gateway_Config_Non_Ovs_Bridge::netmask,
    Gateway_Config_Non_Ovs_Bridge::gre_remote_inet_addr and
    Gateway_Config_Non_Ovs_Bridge::gre_local_inet_addr arrays. It is the array size, and
    sixteen bytes is exactly enough for an IPv4 address in dotted-decimal form at its
    longest, 255.255.255.255, together with a terminator, so these fields hold IPv4
    only: an IPv6 literal does not fit and this interface declares no field that would
    hold one.
*/
#define MAX_IP_ADDR_SIZE     16
/** \def MAX_BRIDGE_NAME_SIZE
    \brief Size in bytes of a network bridge name string.

    Bounds the Gateway_Config_Non_Ovs_Bridge::parent_bridge array. It is the array size,
    so a caller that stores a bridge name and a terminator inside it has at most fifteen
    characters of name available. It is a distinct constant from BRIDGE_NAME_SIZE in
    bridge_util_hal.h, which is 64 bytes and bounds a different record's fields.
*/
#define MAX_BRIDGE_NAME_SIZE 16
/**
 * @enum IF_TYPE
 *
 * @brief Interface Type.
 *
 * Enumeration that defines the different types of network interfaces supported by the
 * Gateway Config table. A member of this enumeration is the value of the
 * Gateway_Config_Non_Ovs_Bridge::if_type field, and it says which fields of the
 * surrounding record are meaningful: the GRE address pair applies to GRE_IF_TYPE, and
 * Gateway_Config_Non_Ovs_Bridge::vlan_id applies to VLAN_IF_TYPE.
 *
 * @note Only OTHER_IF_TYPE carries an explicit initialiser, so the remaining members
 *       take the successive values 1 to 4. A caller must nonetheless use the
 *       enumerators by name: this interface does not commit to those numbers as part of
 *       a wire or storage format, and nothing here maps them onto the different members
 *       and values of `enum INTERFACE_TYPE` in bridge_util_hal.h, which is a separate
 *       enumeration and not interchangeable with this one.
 * @note These are classifications, not states. This interface specifies no transition
 *       between them and no default, so a zero-initialised record reads as
 *       OTHER_IF_TYPE by virtue of C initialisation rules rather than by any statement
 *       made here.
 *
 * @see Gateway_Config_Non_Ovs_Bridge
 */
typedef enum IF_TYPE
{
    OTHER_IF_TYPE =  0, /*!< Some other network interface type: the record describes an interface that is none of the four classes below, so none of the class-specific fields is implied to be meaningful. */
    BRIDGE_IF_TYPE, /*!< Network bridge interface type. The record describes a bridge itself rather than a member interface of one. */
    ETH_IF_TYPE, /**< Network ethernet interface type. */
    GRE_IF_TYPE, /*!< Network GRE interface type. This is the class for which gre_remote_inet_addr and gre_local_inet_addr carry the tunnel endpoints; for any other class this interface does not state what those fields hold. */
    VLAN_IF_TYPE /*!< Network VLAN interface type. This is the class for which vlan_id carries the VLAN identifier; for any other class this interface does not state what that field holds. */
} IF_TYPE;
/**
 * @enum BR_CMD
 *
 * @brief Network Interface/Bridge Commands.
 *
 * Enumeration that defines the different network interface or bridge commands supported
 * by the Gateway Config table. A member of this enumeration is the value of the
 * Gateway_Config_Non_Ovs_Bridge::if_cmd field, and it states what is to be done with
 * the interface or bridge the rest of the record identifies.
 *
 * @note Only IF_UP_CMD carries an explicit initialiser, so the remaining members take
 *       the successive values 1 to 3. A caller must use the enumerators by name for the
 *       reason given on IF_TYPE.
 *       Because IF_UP_CMD is zero, a zero-initialised record requests an interface-up
 *       command rather than no command: this interface declares no "no command" member,
 *       so a caller must set if_cmd deliberately on every record it builds.
 * @note No function declared anywhere in this HAL consumes this enumeration, so the
 *       command is carried rather than executed by this interface. Which component acts
 *       on it, when, and what it does on failure are all outside what this interface
 *       establishes; the repository specification records the same absence under `State
 *       Diagram`, along with the fact that no transition between these four is
 *       specified.
 *
 * @see Gateway_Config_Non_Ovs_Bridge
 */
typedef enum BR_CMD
{
    IF_UP_CMD =  0, /*!< Network interface up command: bring the named interface up. This is also the value a zero-initialised record carries. */
    IF_DOWN_CMD, /*!< Network interface down command: take the named interface down, without deleting it. */
    IF_DELETE_CMD, /*!< Network interface delete command: delete the named interface. This interface does not state whether the interface is first taken down, nor what happens if it is a member of a bridge. */
    BR_REMOVE_CMD /*!< Network bridge removal command: remove the bridge named in parent_bridge. This interface does not state what becomes of the interfaces that were members of it. */
} BR_CMD;
/**
 * @struct Gateway_Config_Non_Ovs_Bridge
 *
 * @brief Gateway Config Table data.
 *
 * Structure that contains data related to the Gateway Config table: one row of it,
 * describing a single network interface or bridge, the addressing that applies to it,
 * its place in the interface hierarchy, and the command to be carried out on it.
 *
 * Two fields determine how the rest is read. if_type says which class of interface the
 * row describes, and therefore which of the class-specific fields - the GRE address pair
 * and vlan_id - are meaningful; if_cmd says what is to be done with it. Everything else
 * is identity and addressing.
 *
 * A caller allocates, populates and releases this record itself. No function in this HAL
 * takes it, returns it, allocates it or frees it, so it is not passed across the
 * interface boundary at all: this header fixes its layout for the caller and the
 * platform code that share the Gateway Config table. There is consequently no ownership
 * transfer, and no retention question of the kind that arises for bridgeDetails in
 * bridge_util_hal.h.
 *
 * @note This interface states no default for any field, no required subset for a given
 *       if_cmd, and no validation. A caller must therefore populate every field its
 *       command and interface type depend on, terminate every string inside its fixed
 *       array, and not rely on an unset field being rejected or defaulted.
 * @note Fields whose bound is MAX_IF_NAME_SIZE, MAX_IP_ADDR_SIZE or
 *       MAX_BRIDGE_NAME_SIZE are all 16-byte arrays, which is narrower than the
 *       equivalent fields of bridgeDetails in bridge_util_hal.h. A value copied from
 *       that record is not guaranteed to fit here and must be bounded on copy.
 *
 * @see IF_TYPE
 * @see BR_CMD
 */
typedef struct Gateway_Config_Non_Ovs_Bridge
{
    char if_name[MAX_IF_NAME_SIZE]; /*!< Network interface name: the interface this row describes, in a fixed MAX_IF_NAME_SIZE (16) byte array. This is the field that identifies the subject of if_cmd. */
    char inet_addr[MAX_IP_ADDR_SIZE]; /*!< Network IP Address of the interface, as an IPv4 address in dotted-decimal form, in a fixed MAX_IP_ADDR_SIZE (16) byte array. */
    char netmask[MAX_IP_ADDR_SIZE]; /*!< Network netmask for inet_addr, in the same dotted-decimal form and with the same bound. This interface expresses the mask as an address rather than as a prefix length, and declares no field for a prefix length. */
    char gre_remote_inet_addr[MAX_IP_ADDR_SIZE]; /*!< GRE remote IP Address: the far endpoint of the tunnel, in dotted-decimal form within MAX_IP_ADDR_SIZE (16) bytes. Meaningful for GRE_IF_TYPE; this interface does not state what it holds for any other interface type. */
    char gre_local_inet_addr[MAX_IP_ADDR_SIZE]; /*!< GRE local IP Address: the near endpoint of the tunnel, on the same terms as gre_remote_inet_addr. */
    char parent_ifname[MAX_IF_NAME_SIZE]; /*!< Parent network interface name: the interface this one is built over, such as the physical interface beneath a VLAN, in a fixed MAX_IF_NAME_SIZE (16) byte array. This interface does not state what an empty value means. */
    char parent_bridge[MAX_BRIDGE_NAME_SIZE]; /*!< Parent network bridge name: the bridge this interface belongs to, in a fixed MAX_BRIDGE_NAME_SIZE (16) byte array. It is also the bridge that BR_REMOVE_CMD names as its subject. */
    int mtu; /*!< MTU packet size in bytes for the interface. This interface states no valid range and no sentinel for "unset", so a caller must not assume that zero is treated as "leave unchanged" or that an out-of-range value is rejected. */
    int vlan_id; /*!< VLAN ID for the interface. Meaningful for VLAN_IF_TYPE; this interface states no valid range, so a caller must not assume the 802.1Q range is validated, and states nothing about the field's meaning for other interface types. */
    IF_TYPE if_type; /*!< Network interface type: which class of interface this row describes, and therefore which of the class-specific fields above are meaningful. */
    BR_CMD if_cmd; /*!< Network interface/bridge command: what is to be done with the interface or bridge identified above. Note that the zero value is IF_UP_CMD, not an absence of command. */
} Gateway_Config_Non_Ovs_Bridge;
#endif
