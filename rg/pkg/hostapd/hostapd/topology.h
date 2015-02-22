#ifndef topology__h
#define topology__h 1

#include "hostapd.h"

/* hostapd_config_read_topology_file reads a topology file,
 * which defines radios and virtual aps, each of which have separate
 * config files.
 * Returns nonzero if error.
 */
int hostapd_config_read_topology_file(
	struct hapd_interfaces *interfaces,
        char *filepath
        );

/* hostapd_config_read_topology_files reads one or more topology files,
 * which define radios and virtual aps, each of which have separate
 * config files.
 * Returns nonzero if error.
 */
int hostapd_config_read_topology_files(
	struct hapd_interfaces *interfaces,
        char **filepaths
        );

#endif  /* topology__h */

