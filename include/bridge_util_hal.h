/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
*
* @file bridge_util_hal.h
* @brief The Bridge Util HAL, to interact with vendor software to control setting such as modes, connection enable/disable, QoS configuration, Ipv4 config. etc.
*
* This header is the complete Bridge Util HAL contract. It declares the bridge record
* a caller populates, the enumerations whose members are the legal ranges of four
* int-typed arguments, the fixed sizes that bound every buffer in the record, the
* caller-supplied globals that describe the platform, the logging macro, and the seven
* functions that make up the interface.
*
* Three properties of this interface decide how it is called, and each is stated here
* because a caller carrying assumptions over from another RDK-B HAL will get them
* wrong:
*
* - There is no lifecycle. No initialization, teardown, handle or session is declared,
*   so nothing is opened before a call and nothing is closed after one. What takes its
*   place is a vendor-hook arrangement: HandlePreConfigVendor() and
*   HandlePostConfigVendor() are implemented by the vendor and called by the client
*   either side of updateBridgeInfo(), which is the one ordering this interface fixes.
*   See `Initialization and Startup` and `Method Sequencing` in the repository
*   specification (docs/pages/halSpec.md).
* - The interface is not required to be thread safe, which is this repository's own
*   statement under `Threading Model`, and the calling module is the party obliged to
*   serialise its calls. Calls are expected from multiple processes, and the globals
*   below are ordinary process-scoped symbols, so this interface coordinates nothing
*   across processes; see `Process Model`.
* - Every outcome is delivered synchronously as the return value. There is no callback
*   typedef, no registration function and no event delivery, so nothing arrives at the
*   caller unsolicited; see `Asynchronous Notification Model` and `Internal Error
*   Handling`.
*
* The error vocabulary this interface defines today is exactly two values: 0 for
* success and -1 for failure, with INTERFACE_EXIST and INTERFACE_NOT_EXIST as the
* named aliases checkIfExists() reports them under. No error enumeration, no reason
* code and no out-parameter status is declared, so an implementation cannot report why
* a call failed and a caller cannot branch on a cause. Widening that would be a change
* to the interface rather than to this documentation.
*
* Behaviour stated in this header is derived from these declarations and from the
* repository specification. Where neither establishes a behaviour, the block says so
* explicitly rather than presenting a plausible value; nothing here is presented as
* observed runtime behaviour.
*
* @note This header does not include network_interface.h, and nothing declared here
*       requires it. The two headers are independent inputs to the documentation
*       generator; a caller that needs the gateway configuration record includes that
*       header explicitly.
*/

#ifndef  _BRIDGE_UTIL_OEM_H
#define  _BRIDGE_UTIL_OEM_H

/**
 * @defgroup BRIDGE_UTIL_OEM BRIDGE UTIL OEM
 *
 * The Bridge Util HAL is the interface through which RDK-B drives vendor bridge
 * configuration - bridge creation, update and deletion, and the OEM and SoC specific
 * configuration applied around each change - without depending on how a particular
 * platform implements it.
 *
 * @defgroup BRIDGE_UTIL_OEM_DATA_TYPES BRIDGE UTIL OEM Data Types
 * @ingroup  BRIDGE_UTIL_OEM
 *
 * The sizes, status aliases, caller-supplied globals, logging macro, enumerations and
 * the bridge record that a caller of this interface constructs or interprets.
 *
 * @defgroup BRIDGE_UTIL_OEM_APIS BRIDGE UTIL OEM APIs
 * @ingroup  BRIDGE_UTIL_OEM
 *
 * The seven declared functions: the two vendor hooks, the bridge mutation, the three
 * interface queries and the interface-list mutation.
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

/**
 * @addtogroup BRIDGE_UTIL_OEM_DATA_TYPES
 * @{
 */

#define BRIDGE_UTIL_LOG_FNAME "/rdklogs/logs/bridgeUtils.log" 				/**< Absolute path of the log file this interface writes diagnostic output to. It is the path the bridge_util_log() macro's stream is expected to be opened on; the repository specification records under `Logging and debugging requirements` that an implementation using this constant writes here rather than to the /var/tmp/ alternative that applies where a platform mounts its log partition elsewhere. This is diagnostic output, not persisted interface state; see `Persistence Model`. */
#define GRE_HANDLER_SCRIPT "/etc/utopia/service.d/service_multinet/handle_gre.sh"	/**< Absolute path of the platform-provided GRE handler script this interface names. It is an external executable supplied by the platform, not something this interface declares, invokes on the caller's behalf or persists; no declared function takes it as an argument. */
#define TOTAL_IFLIST_SIZE      1024		                                        /**< Size in bytes of a complete interface list, meaning all technologies together. It bounds a caller's own aggregate list buffer; it is not the size of any field of bridgeDetails, whose per-technology lists are each IFLIST_SIZE bytes. No declared function takes a buffer of this size. */
#define BRIDGE_NAME_SIZE       64			                                /**< Size in bytes of the bridgeDetails::bridgeName, bridgeDetails::vlan_name and bridgeDetails::VirtualParentIfname arrays. It is the array size and the whole of what this interface fixes about those fields, and no field carries a companion length. A caller that stores a name and a terminator inside one therefore has at most 63 characters of name available. */
#define IFACE_NAME_SIZE        64			                                /**< Size in bytes an individual interface name is bounded by. No field of bridgeDetails and no parameter of a declared function is declared with it, so it bounds a caller's own single-name buffer rather than anything this interface allocates. */
#define IFLIST_SIZE	       256			                                /**< Size in bytes of each per-technology list array of bridgeDetails - ethIfList, MoCAIfList, GreIfList and WiFiIfList. It is the array size, and each array holds a space-separated list of interface names, so it bounds the whole list rather than one name. No field carries a companion length, so a caller that stores a list and a terminator inside one has at most 255 characters of list available. */
#define MAX_LOG_BUFF_SIZE      1024		                                        /**< Size in bytes of the log_buff scratch buffer. The macro passes MAX_LOG_BUFF_SIZE-1 as the snprintf() size argument, which counts the terminator, so it writes at most MAX_LOG_BUFF_SIZE-2 characters plus the terminator and never uses the buffer's final byte. A longer message is truncated rather than overflowing. */
#define TIMESTAMP	       64			                                /**< Number of bytes reserved for the timestamp prefix a logged message carries, which is why log_msg_wtime is declared MAX_LOG_BUFF_SIZE+TIMESTAMP bytes long. It is a buffer allowance in bytes, not a time value. */
#define  INTERFACE_EXIST       0		                                        /**< The value checkIfExists() returns when the named interface is present. It is the same value as the success code every status-returning function in this interface uses, so a caller cannot distinguish "the query succeeded" from "the interface exists" - they are the same answer. */
#define  INTERFACE_NOT_EXIST  -1		                                        /**< The value checkIfExists() returns when the named interface is absent. It is the same value as the failure code every status-returning function in this interface uses. Absence is a legitimate answer to the question asked, so a caller must not treat this as a failed call; see `Internal Error Handling` in the repository specification. */

/*
 * Externally defined state this interface depends on.
 *
 * The fifteen symbols below are declared extern here and defined by the caller's
 * program, not by an implementation of this HAL: the interface reads and writes them
 * but supplies none of them. They are shared mutable state, and the repository
 * specification records the consequences under `Threading Model` - this interface is
 * not required to be thread safe, specifies no locking for these symbols, and places
 * the obligation to serialise access on the calling module - and under `Process
 * Model`, where each process that links the interface holds its own copies, so no
 * value below is coordinated across processes.
 *
 * Two constraints apply to all of them and are not repeated on each declaration.
 * They must exist before the first call that touches them, which for logFp means
 * before any call that logs. And this interface states no initial value, no valid
 * range beyond the values documented individually below, and no rule about which
 * declared function may modify which symbol; a caller must therefore not infer from
 * this interface when a value changes or what an implementation does differently for
 * a given value. See `Object Lifecycles` and `State-Dependent Behavior` in the
 * repository specification (docs/pages/halSpec.md).
 */

extern int DeviceMode;                                          /**< The device role the caller's platform is operating in: 0 is router operation and 2 is bridge operation. This interface declares the symbol and documents those two values, but does not state how an implementation's behaviour changes with it, so a caller must not infer a per-mode contract from this interface alone. */
extern int MocaIsolation_Val; 				        /**< The platform's MoCA isolation setting, which is platform-specific policy the caller supplies. This interface neither enumerates its values nor states which declared function consults it. */
extern int need_wifi_gw_refresh;				/**< Flag indicating that a Wi-Fi gateway refresh is needed. This interface declares the flag but specifies neither the values it takes nor which function sets, clears or acts on it. */
extern int need_switch_gw_refresh;				/**< Flag indicating that a switch gateway refresh is needed. As with need_wifi_gw_refresh, the values and the party that sets them are unspecified by this interface. */
extern int syncMembers;						/**< Marker used to coordinate bridge member updates during a sync. It is one of the two symbols that exist to mark work in progress, so concurrent bridge operations driven without external serialisation will interleave in it; this interface specifies no locking for that. */
extern int BridgeOprInPropgress;				/**< Marker that a bridge operation is in progress. The spelling is the interface's own and is preserved. This interface declares the marker but specifies neither the values it takes nor which function sets or clears it, so a caller cannot use it as a reliable interlock on the strength of this interface alone. */
extern FILE *logFp;						/**< The stream bridge_util_log() writes through. The caller's program opens it, owns it and closes it; nothing in this interface opens or closes it. It has the one ordering requirement among these symbols: while it is NULL the macro's file write is skipped, so it must be open before any call whose output must reach BRIDGE_UTIL_LOG_FNAME. */
extern char log_buff[MAX_LOG_BUFF_SIZE];			/**< Scratch buffer, MAX_LOG_BUFF_SIZE bytes, that bridge_util_log() formats the caller's message into. It is overwritten on every logged message, so its contents are meaningful only immediately after a log call and must not be treated as a record of anything. */
extern char log_msg_wtime[MAX_LOG_BUFF_SIZE+TIMESTAMP];		/**< Scratch buffer, MAX_LOG_BUFF_SIZE+TIMESTAMP bytes, that bridge_util_log() assembles the timestamped message into before writing it. Overwritten on every logged message, exactly as log_buff is. */
extern char primaryBridgeName[64];				/**< The name of the platform's primary bridge, supplied by the caller. The array is 64 bytes, the same width as bridgeDetails::bridgeName. No companion length symbol is declared for it, so a caller storing a name here keeps it inside the array and terminates it there; this interface states no other representation for the contents and names no declared function that reads them. */
extern int PORT2ENABLE;						/**< Platform-specific port enable state. This interface neither enumerates its values nor states which declared function consults it. */
extern int ethWanEnabled;				        /**< Whether an Ethernet WAN is enabled on the platform. This interface does not enumerate the values used for enabled and disabled. */
extern char ethWanIfaceName[64];				/**< The Ethernet WAN interface name, supplied by the caller. The array is 64 bytes. No companion length symbol is declared for it, so a caller storing a name here keeps it inside the array and terminates it there; this interface states no other representation for the contents and names no declared function that reads them. */
extern struct tm *timeinfo;					/**< Broken-down UTC time that bridge_util_log() uses to build a message's timestamp. The macro assigns it from gmtime(), which returns a pointer to storage owned by the C library, so the caller must not free it and must not rely on its contents across another gmtime() or localtime() call. */
extern time_t utc_time;						/**< The UTC time value bridge_util_log() reads from time() and converts through timeinfo when building a message's timestamp. */

/**
 * @brief Formats a message and writes it, timestamped, to the interface's log stream.
 *
 * A variadic macro taking a printf-style format string and its arguments. It formats
 * the message into log_buff, bounded at MAX_LOG_BUFF_SIZE-2 characters plus a
 * terminator, because the size passed to snprintf() counts the terminator; if logFp is
 * open it then timestamps the message in UTC into log_msg_wtime, bounded at
 * MAX_LOG_BUFF_SIZE+TIMESTAMP-1 characters, writes it to logFp and flushes it. The
 * emitted form is fixed:
 *
 *     YYYY-MM-DD HH:MM:SS ::: <message>
 *
 * The macro takes one named variadic parameter, `fmt ...`, which captures the format
 * string together with the arguments it consumes and is expanded into snprintf(). A
 * message longer than the bound above is truncated rather than overflowing the buffer.
 * The parameter is described here in prose rather than with a @c \@param tag, because
 * the tag cannot name a variadic macro parameter without the generator rejecting it.
 *
 * @pre logFp must be open for the message to be emitted at all. While it is NULL the
 *      message is formatted into log_buff and discarded.
 * @post log_buff has been overwritten, and when logFp is open so have log_msg_wtime,
 *       timeinfo and utc_time. The macro yields no value and reports no error: a
 *       failed write is not visible to the caller.
 *
 * @warning There is no fallback to standard output. The only output statements in this
 *          definition are inside the logFp != NULL branch, so when that stream is not
 *          open the message is silently dropped. Earlier documentation of this macro
 *          stated that the message is printed to standard output instead; the
 *          definition below does not do that, and a caller must not rely on it.
 *
 * @note The macro writes the shared globals log_buff, log_msg_wtime, utc_time and
 *       timeinfo with no locking, so two threads logging concurrently will interleave
 *       in them. This is the same obligation `Threading Model` places on the caller
 *       for the interface as a whole: serialise, or do not share.
 * @note This logging mechanism is intended to be made generic in a future revision of
 *       the interface, which the repository specification records under `Logging and
 *       debugging requirements`. A caller should therefore depend on the stream and
 *       the emitted form above, and not on the buffers or the internal structure.
 *
 * @see BRIDGE_UTIL_LOG_FNAME
 * @see MAX_LOG_BUFF_SIZE
 * @see TIMESTAMP
 */

#define bridge_util_log(fmt ...)    {\
				    		snprintf(log_buff, MAX_LOG_BUFF_SIZE-1,fmt);\
				    		if(logFp != NULL){ \
                                                snprintf(log_buff, MAX_LOG_BUFF_SIZE-1,fmt);\
                                                utc_time = time(NULL);\
                                                timeinfo = gmtime(&utc_time);\
                                                snprintf(log_msg_wtime, MAX_LOG_BUFF_SIZE+TIMESTAMP-1,"%04d-%02d-%02d %02d:%02d:%02d ::: %s",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec,log_buff);\
                                    		fprintf(logFp,"%s", log_msg_wtime);\
                                    		fflush(logFp);}\
                                 	}


/**********************************************************************
                ENUMERATION DEFINITIONS
**********************************************************************/

/**
 * @enum Config
 *
 * @brief Identifies which of the platform's configured bridges a vendor hook is being
 *        invoked for.
 *
 * Each member names one bridge the platform may configure - private LAN, home
 * security, the hotspots, the mesh and backhaul bridges, and the conditional managed
 * Wi-Fi and amenity bridges. A member of this enumeration is the range of the
 * InstanceNumber argument of HandlePreConfigVendor() and of the Config argument of
 * HandlePostConfigVendor(); both are declared int, so the enumerator is cast to int at
 * the call site. Passing an int that is not one of the members below compiles but is
 * outside the contract, and this interface does not state what an implementation does
 * with such a value. The convention is recorded under `State-Dependent Behavior` in
 * the repository specification (docs/pages/halSpec.md).
 *
 * @note The values are deliberately not contiguous: 5, 15, 16 and 17 are not assigned
 *       by the unconditional set - 17 belongs to the conditional MANAGE_WIFI_BRIDGE -
 *       and the conditional members are not contiguous with the unconditional set
 *       either. A caller must therefore select an enumerator by name and must never
 *       compute one by arithmetic, iterate a range, or use a member as an array index
 *       without mapping it first.
 * @note Four members are conditional on compile-time flags, so the declared set
 *       varies by build; each is marked below. Naming a member the build does not
 *       declare is a compile error, and passing its value in such a build is outside
 *       the contract. See `Variability Management` and `Optional Components`.
 *
 * @see HandlePreConfigVendor
 * @see HandlePostConfigVendor
 */
enum Config {
	PRIVATE_LAN = 1,					/**< The private LAN bridge, which carries the subscriber's own LAN traffic. */
	HOME_SECURITY = 2,					/**< The home security bridge, carrying traffic for home-monitoring devices kept off the private LAN. */
	HOTSPOT_2G = 3,						/**< The open 2.4 GHz community hotspot bridge. */
	HOTSPOT_5G = 4,						/**< The open 5 GHz community hotspot bridge. */
	LOST_N_FOUND = 6,					/**< The lost-and-found bridge, used for a device that has not completed onboarding. Note that 5 is not assigned, so this member does not follow HOTSPOT_5G numerically. */
	HOTSPOT_SECURE_2G = 7,				        /**< The secured 2.4 GHz community hotspot bridge. */
	HOTSPOT_SECURE_5G = 8,				        /**< The secured 5 GHz community hotspot bridge. */
	MOCA_ISOLATION = 9,					/**< The MoCA isolation bridge, which separates MoCA-attached devices from the private LAN. Related caller-supplied policy is carried in MocaIsolation_Val. */
	MESH_BACKHAUL = 10,					/**< The mesh backhaul bridge, carrying traffic between mesh nodes. */
	ETH_BACKHAUL = 11,					/**< The Ethernet backhaul bridge, carrying mesh backhaul traffic over a wired link. */
	MESH = 12,						/**< The mesh bridge itself, as distinct from the backhaul bridges that connect its nodes. */
    MESH_WIFI_BACKHAUL_2G = 13,     /**< The 2.4 GHz Wi-Fi mesh backhaul bridge. */
    MESH_WIFI_BACKHAUL_5G = 14,     /**< The 5 GHz Wi-Fi mesh backhaul bridge. */
    MESH_ONBOARD = 18,              /**< The mesh onboarding bridge, used while a node joins the mesh. Note that 15, 16 and 17 are not assigned unconditionally: 17 belongs to MANAGE_WIFI_BRIDGE below and exists only under WIFI_MANAGE_SUPPORTED. */
    MESH_WIFI_ONBOARD_2G = 19       /**< The 2.4 GHz Wi-Fi mesh onboarding bridge. */
#if defined  (WIFI_MANAGE_SUPPORTED)
        ,MANAGE_WIFI_BRIDGE = 17		                /**< The managed Wi-Fi bridge. Declared only when WIFI_MANAGE_SUPPORTED is defined; a build without that flag does not declare it, so its value 17 must not be passed as an instance number there. */
#endif /* WIFI_MANAGE_SUPPORTED*/
#if defined (AMENITIES_NETWORK_ENABLED) && !defined (_CBR2_PRODUCT_REQ_)
    ,AMENITY_BRIDGE_2G = 20,        /**< The 2.4 GHz amenity-network bridge. Declared only when AMENITIES_NETWORK_ENABLED is defined and _CBR2_PRODUCT_REQ_ is not; both conditions must hold. */
    AMENITY_BRIDGE_5G = 21,         /**< The 5 GHz amenity-network bridge, under the same pair of conditions as AMENITY_BRIDGE_2G. */
    AMENITY_BRIDGE_6G = 22          /**< The 6 GHz amenity-network bridge, under the same pair of conditions as AMENITY_BRIDGE_2G. */
#endif /*AMENITIES_NETWORK_ENABLED*/
};

/**
 * @enum INTERFACE_TYPE
 *
 * @brief Identifies the kind of network interface an updateBridgeInfo() call concerns.
 *
 * A member of this enumeration is the range of the type argument of
 * updateBridgeInfo(), which is declared int, so the enumerator is cast to int at the
 * call site. The member selected tells the implementation which class of interface the
 * operation applies to; IF_OTHER_BRIDGEUTIL additionally carries the specific meaning
 * described on it below.
 *
 * @note Passing an int that is not one of the members below compiles but is outside
 *       the contract, and this interface does not state what an implementation does
 *       with such a value. See `State-Dependent Behavior` in the repository
 *       specification (docs/pages/halSpec.md).
 * @note This enumeration is distinct from IF_TYPE in network_interface.h, which has
 *       different members and different values and belongs to that independent header.
 *       The two are not interchangeable.
 *
 * @see updateBridgeInfo
 */
enum INTERFACE_TYPE {
    IF_BRIDGE_BRIDGEUTIL = 1, 	        /**< A bridge interface, meaning the operation concerns the bridge itself rather than a member port. */
    IF_VLAN_BRIDGEUTIL = 2,		/**< A VLAN interface, identified in the bridge record by bridgeDetails::vlan_name and bridgeDetails::vlanID. */
    IF_GRE_BRIDGEUTIL = 3,		/**< A GRE (Generic Routing Encapsulation) tunnel interface, drawn from bridgeDetails::GreIfList. */
    IF_MOCA_BRIDGEUTIL = 4,		/**< A MoCA (Multimedia over Coax Alliance) interface, drawn from bridgeDetails::MoCAIfList. */
    IF_WIFI_BRIDGEUTIL = 5,		/**< A Wi-Fi interface, drawn from bridgeDetails::WiFiIfList. */
    IF_ETH_BRIDGEUTIL = 6,		/**< An Ethernet interface, drawn from bridgeDetails::ethIfList. */
    IF_OTHER_BRIDGEUTIL			/**< Any other or unspecified interface type. It carries no explicit initialiser, so it takes the value after IF_ETH_BRIDGEUTIL, which is 7. This is also the value an updateBridgeInfo() sync-delete passes for type, in place of the interface's own type. */
};

/**
 * @enum BridgeOpr
 *
 * @brief The operation an updateBridgeInfo() call performs on the bridge.
 *
 * A member of this enumeration is the range of the Opr argument of updateBridgeInfo(),
 * which is declared int, so the enumerator is cast to int at the call site. This is
 * the enumeration that argument's range actually comes from: the parameter
 * documentation on updateBridgeInfo() has historically named an operation enumeration
 * this interface does not declare, which the repository specification records as a
 * residue of the removed Open vSwitch dependency under `Variability Management`.
 *
 * @note These two members are inputs the caller selects, not states the interface
 *       reports, so no transition between them is specified or implied.
 *
 * @see updateBridgeInfo
 */
enum BridgeOpr {
	DELETE_BRIDGE = 0, 	/**< Delete the bridge the record identifies, or remove the named interface from it in the sync-delete case. */
	CREATE_BRIDGE = 1	/**< Create the bridge the record identifies, or update it where it already exists - updateBridgeInfo() covers creation and update through this one operation. */
};


/**********************************************************************
                STRUCTURE DEFINITIONS
**********************************************************************/

/**
 * @struct bridgeDetails
 *
 * @brief The bridge record: the complete description of one network bridge that a
 *        caller populates and passes to this interface.
 *
 * The record identifies the bridge and its VLAN, and associates the Ethernet, MoCA,
 * GRE and Wi-Fi interfaces that are to be members of it. It is the only structured
 * object this interface takes: updateBridgeInfo(), HandlePreConfigVendor() and
 * HandlePostConfigVendor() each take a pointer to one, and no declared function
 * returns one.
 *
 * Ownership is entirely the caller's, and the repository specification states this
 * under `Memory Model` and `Object Lifecycles`: the caller allocates the record,
 * populates it, and releases it. This interface declares no constructor, no
 * destructor, no copy and no validity check for it.
 *
 * The lifetime the implementation requires of that storage is a different question,
 * and this interface does not answer it. Nothing here obliges an implementation to
 * drop the pointer when the call returns, and nothing here obliges it to keep one
 * either; the implementation also has no means of learning when the caller's storage
 * is released, which is a reason for caution rather than a guarantee. `Memory Model`
 * records the same absence. So a caller keeps the record allocated and unmodified
 * after a call that took it, and settles with the vendor implementation whether the
 * storage may be released or reused before it does either.
 *
 * @note Every char field is a fixed array of the size named on it - BRIDGE_NAME_SIZE
 *       (64) or IFLIST_SIZE (256) - and that size is the whole of what this interface
 *       fixes about it. No field carries a companion length, and no declared function
 *       takes one, so the caller who populates the record is the only party that knows
 *       where a value ends: a caller storing a name or a list keeps it inside the array
 *       and terminates it there. This interface states nothing about what an
 *       implementation does with a value that is longer than its array or is not
 *       terminated, and declares no function that validates the record, so neither is
 *       reported to the caller.
 * @note This interface does not state which fields a given call reads. What each of
 *       the three functions taking the record requires of it is documented on that
 *       function, and where the interface does not establish it, the block says so.
 * @note The example values below are those this interface documents. They are examples
 *       of platform naming and not a fixed set; actual names are vendor-specific and
 *       may be discovered with getVendorIfaces().
 *
 * @see updateBridgeInfo
 * @see HandlePreConfigVendor
 * @see HandlePostConfigVendor
 */

typedef struct bridgeDetails {
	char bridgeName[BRIDGE_NAME_SIZE];				/**< Name of the bridge this record describes, in a fixed BRIDGE_NAME_SIZE (64) byte array. Example values are brlan0, brlan1, privbr and br-home. This is the field that identifies which bridge an operation applies to. */
	char vlan_name[BRIDGE_NAME_SIZE];				/**< Name of the VLAN interface associated with the bridge, in a fixed BRIDGE_NAME_SIZE (64) byte array. Example values are vlan1, vlan10 and guest_vlan. Meaningful for a VLAN operation; this interface does not state what an implementation does with it for other interface types. */
    	char VirtualParentIfname[BRIDGE_NAME_SIZE];	                /**< Name of the parent interface a virtual interface is created over, in a fixed BRIDGE_NAME_SIZE (64) byte array. Example values are eth0, eth1 and wan0. */
	int  vlanID;							/**< VLAN identifier for the VLAN named in vlan_name. Example values are 1, 2, 3, 10 and 100. This interface states no valid range for the field, so a caller must not assume that the 802.1Q range is validated by an implementation; see `Data Structures and Defines` in the repository specification. */
	char ethIfList[IFLIST_SIZE];					/**< Space-separated list of Ethernet interface names to associate with the bridge, in a fixed IFLIST_SIZE (256) byte array. Example values are eth0, eth1 and eth2. The bound applies to the whole list, not to one name. */
	char MoCAIfList[IFLIST_SIZE];					/**< Space-separated list of MoCA interface names to associate with the bridge, in a fixed IFLIST_SIZE (256) byte array. Example values are moca0 and moca1. */
	char GreIfList[IFLIST_SIZE];					/**< Space-separated list of GRE interface names to associate with the bridge, in a fixed IFLIST_SIZE (256) byte array. Example values are gre0 and gre1. */
	char WiFiIfList[IFLIST_SIZE];					/**< Space-separated list of Wi-Fi interface names to associate with the bridge, in a fixed IFLIST_SIZE (256) byte array. Example values are wlan0 and wlan1. */
}bridgeDetails;


/** @} */  //END OF GROUP BRIDGE_UTIL_OEM_DATA_TYPES

/**
 * @addtogroup BRIDGE_UTIL_OEM_APIS
 * @{
 */

/**
* @brief Creates, updates or deletes a network bridge according to the operation and
*        interface type supplied.
*
* This is the one function in this interface that changes bridge configuration. The
* bridge and the interfaces to associate with it are described by the caller's
* bridgeDetails record; Opr selects whether the bridge is created (which also covers
* update) or deleted; and type states which class of interface the operation concerns.
* The vendor implementation applies the change to the platform's bridge and network
* interface configuration.
*
* A caller does not invoke this function on its own. The repository specification fixes
* one ordering under `Method Sequencing` and it is the only ordering this interface
* establishes: HandlePreConfigVendor() is called before this function and
* HandlePostConfigVendor() after it, so that vendor-specific settings are applied
* either side of the change.
*
* @param[in] bridgeInfo        - Pointer to the caller-allocated bridgeDetails record
*                                describing the bridge to operate on. It must be
*                                non-NULL and must address storage of the declared
*                                type. The caller allocates, populates, owns and
*                                releases that storage, which is what `Memory Model` in
*                                the repository specification places on the caller, and
*                                no declaration here transfers that ownership. Whether
*                                the implementation retains the pointer beyond the call
*                                is not specified by this interface: neither header
*                                states a lifetime for it, and `Memory Model` records
*                                the same absence rather than resolving it. A caller
*                                must therefore not treat the return as the point at
*                                which the reference is dropped. The conservative
*                                reading, and the only one this interface supports, is
*                                to keep the record allocated and unmodified after the
*                                call and to establish with the vendor implementation
*                                whether the storage may be released or reused; a
*                                caller that frees or overwrites it on return relies on
*                                a guarantee this interface does not make.
*                                bridgeName identifies the bridge; vlan_name and
*                                vlanID describe its VLAN; and ethIfList, MoCAIfList,
*                                GreIfList and WiFiIfList carry the member interfaces.
*                                Each of those fields is a fixed array of
*                                BRIDGE_NAME_SIZE (64) or IFLIST_SIZE (256) bytes with
*                                no companion length, so the caller populating one
*                                keeps its value inside the array and terminates it
*                                there, as bridgeDetails records. This interface does
*                                not state which subset of the fields a given
*                                combination of Opr and type reads, nor whether the
*                                implementation writes any field back, so a caller
*                                must populate every field relevant to the operation
*                                it is requesting and must not expect the record to be
*                                updated by the call.
* @param[in] ifNameToBeUpdated - Name of the single interface to be deleted and
*                                updated, applicable only during sync. The possible
*                                values are "moca0", "wifi0" and "eth0". This name
*                                should be null-terminated. Ownership stays with the
*                                caller on the same terms as bridgeInfo, including the
*                                unspecified retention recorded there: this interface
*                                does not state whether the implementation keeps the
*                                pointer after returning. Outside the sync case this
*                                interface does not state what value to pass or how the
*                                argument is treated, so a caller must not infer that
*                                NULL or an empty string is accepted.
* @param[in] Opr  - Specifies the operation to be performed on a network interface or
*                   bridge. The range of acceptable values is `enum BridgeOpr` -
*                   DELETE_BRIDGE (0) or CREATE_BRIDGE (1) - and the enumerator is cast
*                   to `int` to use this param. Note that the historical documentation
*                   of this parameter named `enum OVS_CMD`: this interface does not
*                   declare that enumeration, and the repository specification records
*                   the name under `Variability Management` as a residue of the removed
*                   Open vSwitch dependency. A caller uses `enum BridgeOpr`. Passing an
*                   int outside that range compiles, and this interface does not state
*                   what an implementation does with it.
* @param[in] type - Types of interfaces and in case of sync delete the value is set to
*                   IF_OTHER_BRIDGEUTIL. The range of acceptable values is `enum
*                   INTERFACE_TYPE`, whose enumerator is cast to `int` to use this
*                   param. Passing an int outside that range compiles and is outside
*                   the contract.
*
* @pre A caller must have invoked HandlePreConfigVendor() for this bridge first, per
*      `Method Sequencing`. This interface declares no initialization call, so there is
*      nothing else to open beforehand; the caller-supplied globals must already exist,
*      and logFp must be open for anything this call logs to reach a file. Nothing in
*      this interface reports a violated ordering, so calling out of order is not
*      detectable from the return value.
* @post On success the requested bridge change has been applied by the vendor
*       implementation, and the caller proceeds to HandlePostConfigVendor(). On failure
*       this interface does not state whether the change was applied partially or not
*       at all, and no reason code is available, so a caller must re-read platform
*       state rather than assume the configuration is unchanged.
*
* @returns int - the result status of the operation, reported synchronously as the
*          return value. This interface defines no other outcome channel.
* @retval 0  - The operation succeeded.
* @retval -1 - The operation failed. This interface defines no reason code, so the
*              client logs the failure and either retries or reports it to the vendor
*              implementation's owner; it cannot branch on a cause.
*
* @note This call is synchronous and every result is delivered at the point of call,
*       which `Blocking calls` in the repository specification states for every
*       function in this interface. It is expected to complete within a period
*       commensurate with the complexity of the operation, and the same specification
*       records under `Initialization and Startup` that the vendor hooks bracketing
*       this call are expected not to return until the underlying hardware is ready -
*       so a caller should treat a bridge configuration sequence as blocking. No
*       numeric timeout is specified by this interface, and a caller that cannot
*       tolerate an unbounded wait must impose its own bound.
* @note This interface is not required to be thread safe, and `Threading Model` places
*       on the calling module the obligation to make its calls in a thread-safe manner,
*       which for a mutating call means serialising it against every other call in this
*       interface. The interface specifies no locking, and the caller-supplied globals
*       - BridgeOprInPropgress and syncMembers among them - are shared without any
*       either, so two concurrent bridge operations will interleave in them.
*
* @see HandlePreConfigVendor
* @see HandlePostConfigVendor
* @see bridgeDetails
* @see BridgeOpr
* @see INTERFACE_TYPE
*/
extern int updateBridgeInfo(bridgeDetails *bridgeInfo, char* ifNameToBeUpdated, int Opr , int type);

/**
* @brief Reports whether a named network interface exists on the platform.
*
* A query with no side effect on bridge configuration. The answer is the return value
* itself: this function reports presence through the two named constants rather than
* through an out-parameter, so a caller reads the interface's existence directly from
* the status it returns.
*
* @param[in] iface_name - Name of the interface to test. The string must be
*                         null-terminated and is read but not modified by the call, so
*                         the caller retains ownership. Whether the implementation
*                         retains the pointer after returning is not specified by this
*                         interface - being read-only says nothing about lifetime - so
*                         a caller keeps the storage allocated after the call unless it
*                         has established otherwise with the vendor implementation, as
*                         `Memory Model` in the repository specification records.
*                         Interface names are
*                         vendor-specific; getVendorIfaces() is the declared means of
*                         discovering which names a platform offers. This interface
*                         states no maximum length for the argument and no behaviour
*                         for NULL or an empty string, so a caller must pass a valid
*                         name rather than rely on either being rejected.
*
* @pre None. This interface declares no initialization call and this function has no
*      stated ordering relative to any other, which `Method Sequencing` in the
*      repository specification records explicitly for the three queries.
* @post Platform state is unchanged. The return value is the whole of the result; no
*       argument is written.
*
* @returns int - the presence of the interface, reported synchronously as the return
*          value using the two named constants this interface defines for it.
* @retval INTERFACE_EXIST     - Value 0. The named interface is present.
* @retval INTERFACE_NOT_EXIST - Value -1. The named interface is absent. This is a
*                               legitimate answer to the question asked and not an
*                               error in the call, which `Internal Error Handling` in
*                               the repository specification states explicitly; a
*                               client must not log it as a failure or retry on it. It
*                               is nonetheless the same value the rest of this
*                               interface uses for failure, so a client cannot
*                               distinguish "the interface is absent" from "the query
*                               could not be answered" through this return value, and
*                               this interface does not specify a way to.
*
* @note This call is synchronous and non-deferred: `Blocking calls` in the repository
*       specification states that every function in this interface delivers its result
*       at the point of call. No numeric timeout is specified for it, so a caller
*       needing a bound imposes its own.
* @note This interface is not required to be thread safe and specifies no locking, so
*       `Threading Model` places on the calling module the obligation to serialise this
*       call against the others. It reads platform state that a concurrent
*       updateBridgeInfo() may be changing, and this interface offers no way to obtain a
*       consistent view across the two.
*
* @see INTERFACE_EXIST
* @see INTERFACE_NOT_EXIST
* @see checkIfExistsInBridge
* @see getVendorIfaces
*/
extern int checkIfExists(char* iface_name);

/**
* @brief Removes a named interface from a caller-supplied interface list, in place.
*
* Operates on a list the caller already holds rather than on platform state: the list
* buffer is the object modified by the call. If the specified interface name is not
* found in the list, no action is taken. This function does not report an error in such
* cases, so the call is not a test of membership and a caller must not use it as one -
* checkIfExists() and checkIfExistsInBridge() are the declared queries.
*
* @param[in,out] str - The interface list, as a space-separated, null-terminated string
*                      of interface names; the possible value is
*                      "wl0 wl11 moca0 ath0 eth3". The parameter is declared non-const
*                      because the list is modified in place, so the caller must pass
*                      writable storage: passing a string literal or any read-only
*                      buffer is undefined behaviour, not a rejected argument. The
*                      caller allocates, owns and releases the storage. Whether the
*                      implementation retains the pointer beyond the call is not
*                      specified by this interface, so a caller keeps the list
*                      allocated after the call unless it has established otherwise
*                      with the vendor implementation; `Memory Model` in the repository
*                      specification records the same absence.
*                      Because the operation only removes, the result cannot be longer
*                      than the input, which is why no capacity argument is declared;
*                      the interface states no maximum length for the list itself and no
*                      behaviour for NULL, so a caller must pass a valid,
*                      null-terminated, writable list.
* @param[in]     sub - Name of the interface to remove from the list; the possible value
*                      is "moca0". The string must be null-terminated and is read but
*                      not modified, as its const declaration states, and ownership
*                      stays with the caller on the same unspecified-retention terms as
*                      str: this interface does not state whether the implementation
*                      keeps the pointer after returning. This
*                      interface does not state whether the match is exact, whole-token
*                      or a substring, so a caller must not pass a partial name and
*                      expect a defined result.
*
* @pre None. This interface declares no initialization call, and `Method Sequencing` in
*      the repository specification records that this function has no stated position
*      relative to any other.
* @post On return, str holds the list with the named interface removed if it was
*       present, and unchanged if it was not. Because nothing is reported either way, a
*       caller that must know whether the removal happened determines it by inspecting
*       the list, which `Internal Error Handling` in the repository specification states
*       explicitly. Platform state is not affected: removing a name from a caller's list
*       does not detach the interface from a bridge.
*
* @note This declaration is void. It yields no value and reports neither success nor
*       failure, so this block carries no return tag of any kind - adding one would
*       describe an outcome channel the declaration does not have.
* @note This call is synchronous and completes at the point of call, per `Blocking
*       calls` in the repository specification. It operates on caller memory rather than
*       on the platform, so no vendor or hardware wait arises from it, and no numeric
*       timeout is specified for it.
* @note This interface is not required to be thread safe and specifies no locking, so
*       `Threading Model` places on the calling module the obligation to serialise its
*       calls. Concretely for this function: the list buffer is mutated in place, so a
*       caller must not let another thread read or write that buffer for the duration of
*       the call.
*
* @see checkIfExists
* @see checkIfExistsInBridge
* @see bridgeDetails
*/
extern void removeIfaceFromList(char *str, const char *sub);

/**
* @brief Reports the outcome of testing whether a named interface is attached to a
*        named bridge.
*
* A query with no side effect on bridge configuration. It answers a narrower question
* than checkIfExists(): not whether the interface exists, but whether it is currently a
* member of the given bridge.
*
* @param[in] iface_name  - Name of the interface to test. Possible values are erouter0,
*                          eth0, eth1 and wlan0. The string must be null-terminated and
*                          is read but not modified, so the caller retains ownership.
*                          Whether the implementation retains the pointer after
*                          returning is not specified by this interface, so a caller
*                          keeps the storage allocated after the call unless it has
*                          established otherwise with the vendor implementation;
*                          `Memory Model` in the repository specification records the
*                          same absence.
*                          Interface names are vendor-specific; getVendorIfaces()
*                          discovers the names a platform offers. This interface states
*                          no maximum length and no behaviour for NULL or an empty
*                          string.
* @param[in] bridge_name - Name of the bridge to test membership of. Possible values are
*                          brlan0, brlan1 and br-home, which are the same names
*                          bridgeDetails::bridgeName carries. The string must be
*                          null-terminated and is read but not modified, on the same
*                          ownership terms as iface_name. The parameter is declared
*                          without const, but nothing in this interface states that the
*                          buffer is written, and a caller must not expect it to be.
*
* @pre None. This interface declares no initialization call and states no ordering for
*      this function relative to any other, per `Method Sequencing` in the repository
*      specification.
* @post Platform state is unchanged. The return value is the whole of the result; no
*       argument is written.
*
* @returns int - the result status of the operation, reported synchronously as the
*          return value.
* @retval 0  - The operation succeeded.
* @retval -1 - The operation failed. This interface defines no reason code, so a client
*              cannot branch on a cause; it logs the failure and either retries or
*              reports it to the vendor implementation's owner.
*
* @warning Unlike checkIfExists(), this declaration documents only success and failure
*          and does not say which of them corresponds to the interface being attached to
*          the bridge. It defines no equivalent of INTERFACE_EXIST and
*          INTERFACE_NOT_EXIST. **This interface therefore does not specify how a caller
*          reads an attachment answer out of the return value**, and the repository
*          specification records the same gap under `API Surface`. A caller must not
*          assume that -1 means "not attached": that mapping has to be established with
*          the implementation before the result can be interpreted as an attachment
*          test.
*
* @note This call is synchronous and delivers its result at the point of call, per
*       `Blocking calls` in the repository specification. No numeric timeout is
*       specified for it.
* @note This interface is not required to be thread safe and specifies no locking, so
*       `Threading Model` places on the calling module the obligation to serialise this
*       call against the others. It reads bridge membership that a concurrent
*       updateBridgeInfo() may be changing, and this interface offers no way to obtain a
*       consistent view across the two.
*
* @see checkIfExists
* @see updateBridgeInfo
* @see getVendorIfaces
*/
extern int checkIfExistsInBridge(char* iface_name, char *bridge_name);

/**
* @brief Applies OEM and SoC specific configuration before the client changes a bridge.
*
* This function will apply OEM/SOC-specific configurations and is called before
* updateBridgeInfo() by the client. A bridge is created, updated, or deleted by calling
* updateBridgeInfo(). This will ensure that any vendor-specific settings are correctly
* applied before any changes to the bridge.
*
* The direction of this call is the opposite of the rest of the interface and is worth
* being explicit about: it is implemented by the vendor and invoked by the client, at a
* point the client chooses in its own flow. It is not a callback registered with the
* HAL and later dispatched by it - this interface declares no registration function and
* no asynchronous delivery at all, which the repository specification states under
* `Asynchronous Notification Model`.
*
* @param[in] bridgeInfo     - Pointer to the caller-allocated bridgeDetails record for
*                             the bridge that is about to change. It must be non-NULL
*                             and must address storage of the declared type. The caller
*                             allocates, populates, owns and releases that storage, per
*                             `Memory Model` in the repository specification, and
*                             whether the implementation retains the pointer beyond the
*                             call is not specified by this interface - so the caller
*                             keeps the record allocated and unmodified after the call
*                             rather than treating the return as the point at which the
*                             reference is dropped, as bridgeDetails records. It is
*                             the same record the caller then passes to
*                             updateBridgeInfo(), so bridgeName identifies the bridge
*                             and the four per-technology lists carry the member
*                             interfaces, each bounded by IFLIST_SIZE (256) bytes and
*                             terminated inside its array by the caller that populates
*                             it, as bridgeDetails records. This interface does not
*                             state which fields a vendor implementation reads, nor
*                             whether it may modify the record before
*                             updateBridgeInfo() sees it, so a caller must populate
*                             the record fully beforehand and must not rely on it
*                             being either preserved or amended.
* @param[in] InstanceNumber - Identifies which configured bridge the vendor
*                             configuration is being applied for. It is declared `int`,
*                             and the range of acceptable values is `enum Config`, whose
*                             enumerator is cast to `int` to use this param - for
*                             example PRIVATE_LAN (1), HOTSPOT_2G (3) or MESH_BACKHAUL
*                             (10). The values are not contiguous and four of the
*                             members are conditional on compile-time flags, so a caller
*                             must select an enumerator by name and must never compute
*                             one arithmetically. Passing an int outside that range
*                             compiles, and this interface does not state what an
*                             implementation does with it.
*
* @pre This interface declares no initialization call, so nothing is opened beforehand,
*      and the caller-supplied globals must already exist. The ordering constraint runs
*      the other way: this function must be called before updateBridgeInfo() for the
*      same bridge, which `Method Sequencing` in the repository specification fixes.
*      Nothing here reports a violated ordering, so calling it after the change is not
*      detectable from the return value.
* @post On success the vendor's pre-change configuration has been applied and the caller
*       proceeds to updateBridgeInfo(). On failure this interface does not state whether
*       any part of the vendor configuration was applied, and defines no rollback, so a
*       caller must not assume the platform is untouched; nor does it state whether
*       updateBridgeInfo() may still be called, so a caller that continues does so
*       outside anything this interface establishes.
*
* @returns int - the result status of the operation, reported synchronously as the
*          return value.
* @retval 0  - The vendor pre-change configuration was applied.
* @retval -1 - It failed. This interface defines no reason code, so the client logs the
*              failure and reports it to the vendor implementation's owner rather than
*              branching on a cause.
*
* @note This call is synchronous, per `Blocking calls` in the repository specification,
*       and the vendor hooks are the one place that specification states a blocking
*       expectation: under `Initialization and Startup` they are expected not to return
*       until the underlying hardware is ready, so a caller should treat this call as
*       blocking for as long as that takes. No numeric timeout is specified by this
*       interface, so a caller that cannot tolerate an unbounded wait must impose its
*       own bound.
* @note This interface is not required to be thread safe and specifies no locking, so
*       `Threading Model` places on the calling module the obligation to serialise this
*       call together with the updateBridgeInfo() and HandlePostConfigVendor() calls
*       that bracket it - the three form one sequence, and interleaving another bridge
*       operation inside it is outside anything this interface specifies. A vendor
*       implementation may use internal threads provided it synchronises them and cleans
*       them up on closure.
*
* @see updateBridgeInfo()
* @see HandlePostConfigVendor
* @see Config
* @see bridgeDetails
*/

int HandlePreConfigVendor(bridgeDetails *bridgeInfo,int InstanceNumber);

/**
* @brief Applies OEM and SoC specific configuration after the client has changed a
*        bridge.
*
* This function will apply OEM/SOC-specific configurations and is called after
* updateBridgeInfo() by the client. A bridge is created, updated, or deleted by calling
* updateBridgeInfo(). This will ensure that any vendor-specific settings are correctly
* applied after any changes to the bridge.
*
* Like HandlePreConfigVendor(), it is implemented by the vendor and invoked by the
* client rather than dispatched by the HAL; it is a synchronous entry point in the
* caller's own flow, not a registered handler. See `Asynchronous Notification Model` in
* the repository specification.
*
* @param[in] bridgeInfo - Pointer to the caller-allocated bridgeDetails record for the
*                         bridge that has just changed. It must be non-NULL and must
*                         address storage of the declared type, and it is normally the
*                         same record passed to HandlePreConfigVendor() and
*                         updateBridgeInfo(). The caller allocates, populates, owns and
*                         releases that storage, per `Memory Model` in the repository
*                         specification, and whether the implementation retains the
*                         pointer beyond the call is not specified by this interface -
*                         so the caller keeps the record allocated and unmodified after
*                         the call rather than treating the return as the point at which
*                         the reference is dropped, as bridgeDetails records. Every
*                         string field is a fixed array with no companion length, so
*                         the caller that populates one terminates its value inside
*                         the array. This interface does not state which fields a
*                         vendor implementation reads, nor whether the record must
*                         still describe the bridge as it was before the change, so a
*                         caller passes the record it used for the change itself.
* @param[in] Config     - Identifies which configured bridge the vendor configuration is
*                         being applied for. Despite the parameter's name it is an
*                         instance identifier, not a structure: it is declared `int`, and
*                         the range of acceptable values is `enum Config`, whose
*                         enumerator is cast to `int` to use this param. The same value
*                         the caller passed as InstanceNumber to HandlePreConfigVendor()
*                         for this bridge is what belongs here. The values are not
*                         contiguous and four of the members are conditional on
*                         compile-time flags, so a caller must select an enumerator by
*                         name. Passing an int outside that range compiles, and this
*                         interface does not state what an implementation does with it.
*
* @pre updateBridgeInfo() must have been called for this bridge, which is the ordering
*      `Method Sequencing` in the repository specification fixes. This interface states
*      nothing about whether that call must have succeeded, so whether to invoke this
*      hook after a failed bridge change is not established here. No initialization call
*      exists to precede any of this, and nothing reports a violated ordering.
* @post On success the vendor's post-change configuration has been applied and the
*       bridge configuration sequence is complete. On failure this interface does not
*       state whether any part of the vendor configuration was applied, and defines no
*       rollback of the bridge change updateBridgeInfo() already made, so a caller must
*       re-read platform state rather than assume a consistent outcome.
*
* @returns int - the result status of the operation, reported synchronously as the
*          return value.
* @retval 0  - The vendor post-change configuration was applied.
* @retval -1 - It failed. This interface defines no reason code, so the client logs the
*              failure and reports it to the vendor implementation's owner rather than
*              branching on a cause.
*
* @note This call is synchronous, per `Blocking calls` in the repository specification,
*       and it carries the same blocking expectation as HandlePreConfigVendor(): under
*       `Initialization and Startup` the vendor hooks are expected not to return until
*       the underlying hardware is ready. No numeric timeout is specified by this
*       interface, so a caller needing a bound imposes its own.
* @note This interface is not required to be thread safe and specifies no locking, so
*       `Threading Model` places on the calling module the obligation to serialise this
*       call as the closing member of the three-call sequence it belongs to. A vendor
*       implementation may use internal threads provided it synchronises them and cleans
*       them up on closure.
*
* @see updateBridgeInfo()
* @see HandlePreConfigVendor
* @see Config
* @see bridgeDetails
*/

int HandlePostConfigVendor(bridgeDetails *bridgeInfo,int Config);

/**
* @brief Retrieves the vendor-specific interface names available for bridge management.
*
* Retrieves a list of vendor-specific interface names for bridge management. These names
* can be used for various network operations, such as creating, updating, or deleting
* network bridges - they are the names a caller places in the per-technology lists of a
* bridgeDetails record, and the names checkIfExists() and checkIfExistsInBridge() take.
* Because the naming is vendor-specific by definition, this is the declared means of
* discovering it rather than hard-coding platform names.
*
* This function returns a value, not a status code, so the three-way distinction matters
* when reading the result: there is no success or failure code here, and NULL is a
* documented answer rather than an error.
*
* @pre None. This interface declares no initialization call and states no ordering for
*      this function relative to any other, per `Method Sequencing` in the repository
*      specification. Note that the declaration is `char *getVendorIfaces()` with an
*      empty parameter list rather than `(void)`, so in C it is an unprototyped
*      declaration and the compiler will not check a call's arguments: a caller must
*      pass none.
* @post Platform state is unchanged. The only result is the returned pointer.
*
* @return char * - a zero-terminated string of vendor interface names on success, or
*         NULL when there are no interfaces. The names are vendor-specific; this
*         interface states neither a separator for multiple names nor a maximum length,
*         so a caller must read the string up to its terminator and must not assume the
*         same layout as the space-separated lists in bridgeDetails without establishing
*         it with the implementation. NULL means "no interfaces"; this interface defines
*         no distinct indication for a failure to determine the answer, so a caller
*         cannot tell "there are none" from "it could not be determined", and the
*         repository specification records that gap under `Internal Error Handling`.
*
* @warning **This interface does not specify the ownership or the lifetime of the
*          returned pointer.** Nothing in either header, and nothing elsewhere in this
*          repository, states whether the storage is allocated for the caller, whether
*          it is static or otherwise owned by the implementation, how long it stays
*          valid, or whether two calls return the same buffer. The repository
*          specification records exactly the same absence under `Memory Model` and
*          `Object Lifecycles` rather than resolving it by inference, and this block does
*          the same. What follows is therefore unsafe for a caller to assume, and each
*          assumption has a distinct failure mode:
*          - Do not call free() on it. If the storage is static or implementation-owned,
*            freeing it corrupts the allocator (CWE-590/CWE-762).
*          - Do not assume the caller must free it either. If the implementation does
*            allocate per call, never releasing it leaks on every call (CWE-401).
*          - Do not retain it across another call into this interface, or across any
*            point where the implementation may release or reuse the buffer; a retained
*            pointer may become dangling (CWE-416).
*          - Do not write through it, and do not assume two calls return distinct
*            buffers or stable contents.
*          The safe use this interface does support is to read the string immediately
*          and copy what is needed into caller-owned storage before doing anything else.
*          A caller that needs to free the buffer, retain it, or share it between threads
*          must establish the contract with the vendor implementation first; that
*          agreement is outside this interface, and resolving it here would mean stating
*          a guarantee the interface does not make.
*
* @note This call is synchronous and delivers its result at the point of call, per
*       `Blocking calls` in the repository specification. No numeric timeout is
*       specified for it.
* @note This interface is not required to be thread safe and specifies no locking, so
*       `Threading Model` places on the calling module the obligation to serialise this
*       call against the others. The unspecified lifetime above compounds that: if the
*       returned storage is shared between calls, two threads reading it concurrently are
*       sharing a buffer this interface gives them no way to coordinate.
*
* @see checkIfExists
* @see checkIfExistsInBridge
* @see bridgeDetails
*/
char *getVendorIfaces();
/** @} */  //END OF BRIDGE_UTIL_OEM_APIS
#endif
