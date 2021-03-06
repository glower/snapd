/*
 * Copyright (C) 2019 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "snap-confine-invocation.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libsnap-confine-private/cleanup-funcs.h"
#include "../libsnap-confine-private/snap.h"
#include "../libsnap-confine-private/string-utils.h"
#include "../libsnap-confine-private/utils.h"

void sc_init_invocation(sc_invocation *inv, const struct sc_args *args, const char *snap_instance) {
    /* Snap instance name is conveyed via untrusted environment. It may be
     * unset (typically when experimenting with snap-confine by hand). It
     * must also be a valid snap instance name. */
    if (snap_instance == NULL) {
        die("cannot use NULL snap instance name");
    }
    sc_instance_name_validate(snap_instance, NULL);

    /* The security tag is conveyed via untrusted command line. It must be
     * in agreement with snap instance name and must be a valid security
     * tag. */
    const char *security_tag = sc_args_security_tag(args);
    if (!verify_security_tag(security_tag, snap_instance)) {
        die("security tag %s not allowed", security_tag);
    }

    /* The base snap name is conveyed via untrusted, optional, command line
     * argument. It may be omitted where it implies the "core" snap is the
     * base. */
    const char *base_snap_name = sc_args_base_snap(args);
    if (base_snap_name == NULL) {
        base_snap_name = "core";
    }
    sc_snap_name_validate(base_snap_name, NULL);

    /* The executable is conveyed via untrusted command line. It must be set
     * but cannot be validated further than that at this time. It might be
     * arguable to validate it to be snap-exec in one of the well-known
     * locations or one of the special-cases like strace / gdb but this is
     * not done at this time. */
    const char *executable = sc_args_executable(args);
    if (executable == NULL) {
        die("cannot run with NULL executable");
    }

    /* Invocation helps to pass relevant data to various parts of snap-confine. */
    memset(inv, 0, sizeof *inv);
    inv->base_snap_name = sc_strdup(base_snap_name);
    inv->orig_base_snap_name = sc_strdup(base_snap_name);
    inv->executable = sc_strdup(executable);
    inv->security_tag = sc_strdup(security_tag);
    inv->snap_instance = sc_strdup(snap_instance);
    inv->classic_confinement = sc_args_is_classic_confinement(args);

    debug("security tag: %s", inv->security_tag);
    debug("executable:   %s", inv->executable);
    debug("confinement:  %s", inv->classic_confinement ? "classic" : "non-classic");
    debug("base snap:    %s", inv->base_snap_name);
}

void sc_cleanup_invocation(sc_invocation *inv) {
    if (inv != NULL) {
        sc_cleanup_string(&inv->snap_instance);
        sc_cleanup_string(&inv->base_snap_name);
        sc_cleanup_string(&inv->orig_base_snap_name);
        sc_cleanup_string(&inv->security_tag);
        sc_cleanup_string(&inv->executable);
    }
}

void sc_maybe_pick_alt_base_snap(sc_invocation *inv) {
    /* As a special fallback, allow the base snap to degrade from "core" to
     * "ubuntu-core". This is needed for the migration tests. */
    char mount_point[PATH_MAX] = {0};
    sc_must_snprintf(mount_point, sizeof mount_point, "%s/%s/current/", SNAP_MOUNT_DIR, inv->base_snap_name);

    if (access(mount_point, F_OK) == 0) {
        return;
    }
    /* The mount point is missing but for some snaps we may have an alternative. */
    if (sc_streq(inv->base_snap_name, "core")) {
        /* For "core" we can still use the ubuntu-core snap. This is helpful in
         * the migration path when new snap-confine runs before snapd has
         * finished obtaining the core snap. */
        sc_must_snprintf(mount_point, sizeof mount_point, "%s/%s/current/", SNAP_MOUNT_DIR, "ubuntu-core");
        if (access(mount_point, F_OK) == 0) {
            sc_cleanup_string(&inv->base_snap_name);
            inv->base_snap_name = sc_strdup("ubuntu-core");
            debug("falling back to ubuntu-core instead of unavailable core snap");
            return;
        }
    }
    die("cannot proceed without base snap %s", inv->base_snap_name);
}
